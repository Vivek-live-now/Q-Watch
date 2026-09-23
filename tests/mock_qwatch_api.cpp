#include "mock_qwatch_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define MOCK_FB_WIDTH 128
#define MOCK_FB_HEIGHT 64
#define MOCK_FB_SIZE (MOCK_FB_WIDTH * MOCK_FB_HEIGHT / 8) // 1024 bytes

static uint8_t s_mock_fb[MOCK_FB_SIZE];
static uint8_t s_mock_buttons = 0;
static QTelemetry s_mock_telemetry;
static uint16_t s_last_tone_freq = 0;
static uint16_t s_last_tone_dur = 0;
static uint8_t s_last_led_r = 0;
static uint8_t s_last_led_g = 0;
static uint8_t s_last_led_b = 0;
static bool s_exit_requested = false;
static bool s_health_enabled = false;
static int s_flush_count = 0;
static struct timespec s_start_time;
static bool s_time_initialized = false;

static void ensure_time_init() {
    if (!s_time_initialized) {
        clock_gettime(CLOCK_MONOTONIC, &s_start_time);
        s_time_initialized = true;
    }
}

// -------------------------------------------------------------
// Mock API Callbacks
// -------------------------------------------------------------
static uint8_t* cb_get_framebuffer(void) {
    return s_mock_fb;
}

static void cb_draw_pixel(int16_t x, int16_t y, uint8_t color) {
    if (x < 0 || x >= MOCK_FB_WIDTH || y < 0 || y >= MOCK_FB_HEIGHT) return;
    int index = x + (y / 8) * MOCK_FB_WIDTH;
    uint8_t bit = 1 << (y % 8);
    if (color) {
        s_mock_fb[index] |= bit;
    } else {
        s_mock_fb[index] &= ~bit;
    }
}

static void cb_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    while (1) {
        cb_draw_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void cb_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color, bool fill) {
    if (fill) {
        for (int16_t j = y; j < y + h; ++j) {
            for (int16_t i = x; i < x + w; ++i) {
                cb_draw_pixel(i, j, color);
            }
        }
    } else {
        cb_draw_line(x, y, x + w - 1, y, color);
        cb_draw_line(x, y + h - 1, x + w - 1, y + h - 1, color);
        cb_draw_line(x, y, x, y + h - 1, color);
        cb_draw_line(x + w - 1, y, x + w - 1, y + h - 1, color);
    }
}

static void cb_draw_circle(int16_t xc, int16_t yc, int16_t r, uint8_t color, bool fill) {
    int x = 0, y = r;
    int d = 3 - 2 * r;
    while (y >= x) {
        if (fill) {
            for (int i = xc - x; i <= xc + x; i++) {
                cb_draw_pixel(i, yc + y, color);
                cb_draw_pixel(i, yc - y, color);
            }
            for (int i = xc - y; i <= xc + y; i++) {
                cb_draw_pixel(i, yc + x, color);
                cb_draw_pixel(i, yc - x, color);
            }
        } else {
            cb_draw_pixel(xc + x, yc + y, color);
            cb_draw_pixel(xc - x, yc + y, color);
            cb_draw_pixel(xc + x, yc - y, color);
            cb_draw_pixel(xc - x, yc - y, color);
            cb_draw_pixel(xc + y, yc + x, color);
            cb_draw_pixel(xc - y, yc + x, color);
            cb_draw_pixel(xc + y, yc - x, color);
            cb_draw_pixel(xc - y, yc - x, color);
        }
        x++;
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
    }
}

static void cb_draw_string(int16_t x, int16_t y, const char* str, uint8_t font_size) {
    (void)x; (void)y; (void)str; (void)font_size;
    // In mock harness, string draws can be verified via logs or bounds check
}

static void cb_draw_bitmap(int16_t x, int16_t y, int16_t w, int16_t h, const uint8_t* bitmap) {
    if (!bitmap) return;
    int byte_width = (w + 7) / 8;
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            uint8_t byte = bitmap[j * byte_width + (i / 8)];
            if (byte & (1 << (7 - (i % 8)))) {
                cb_draw_pixel(x + i, y + j, 1);
            }
        }
    }
}

static void cb_clear_screen(void) {
    memset(s_mock_fb, 0, sizeof(s_mock_fb));
}

static void cb_flush_display(void) {
    s_flush_count++;
}

static uint8_t cb_get_button_state(void) {
    return s_mock_buttons;
}

static void cb_get_telemetry(QTelemetry* out) {
    if (out) {
        memcpy(out, &s_mock_telemetry, sizeof(QTelemetry));
    }
}

static void cb_enable_health_sensor(bool enable) {
    s_health_enabled = enable;
}

static void cb_get_ppg_buffer(uint32_t* red_buf, uint32_t* ir_buf, uint16_t count) {
    for (uint16_t i = 0; i < count; i++) {
        if (red_buf) red_buf[i] = s_mock_telemetry.ppg_raw_red;
        if (ir_buf) ir_buf[i] = s_mock_telemetry.ppg_raw_ir;
    }
}

static void cb_ir_send_raw(const uint16_t* timings, uint16_t length, uint16_t khz) {
    (void)timings; (void)length; (void)khz;
}

static void cb_ir_send_nec(uint32_t address, uint32_t command) {
    (void)address; (void)command;
}

static bool cb_ir_has_received(void) {
    return false;
}

static bool cb_ir_get_received(uint32_t* protocol, uint32_t* address, uint32_t* command) {
    (void)protocol; (void)address; (void)command;
    return false;
}

static void cb_play_tone(uint16_t freq_hz, uint16_t duration_ms) {
    s_last_tone_freq = freq_hz;
    s_last_tone_dur = duration_ms;
}

static void cb_stop_tone(void) {
    s_last_tone_freq = 0;
    s_last_tone_dur = 0;
}

static void cb_set_led(uint8_t r, uint8_t g, uint8_t b) {
    s_last_led_r = r;
    s_last_led_g = g;
    s_last_led_b = b;
}

static uint32_t cb_millis(void) {
    ensure_time_init();
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint32_t)((now.tv_sec - s_start_time.tv_sec) * 1000 +
                     (now.tv_nsec - s_start_time.tv_nsec) / 1000000);
}

static uint32_t cb_micros(void) {
    ensure_time_init();
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint32_t)((now.tv_sec - s_start_time.tv_sec) * 1000000 +
                     (now.tv_nsec - s_start_time.tv_nsec) / 1000);
}

static void cb_delay_ms(uint32_t ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

static uint32_t cb_random_range(uint32_t min, uint32_t max) {
    if (min >= max) return min;
    return min + (uint32_t)(rand() % (max - min + 1));
}

static int cb_file_read(const char* path, void* buf, uint32_t max_len) {
    (void)path; (void)buf; (void)max_len;
    return 0;
}

static int cb_file_write(const char* path, const void* buf, uint32_t len) {
    (void)path; (void)buf; (void)len;
    return (int)len;
}

static bool cb_file_exists(const char* path) {
    (void)path;
    return false;
}

static void cb_log_print(const char* msg) {
    if (msg) printf("[QAPP_LOG] %s\n", msg);
}

static void cb_exit_app(void) {
    s_exit_requested = true;
}

static const QWatchAPI s_mock_api = {
    .api_version = QAPP_API_VERSION,
    .supported_caps = QAPP_CAP_ALL,

    .get_framebuffer = cb_get_framebuffer,
    .draw_pixel = cb_draw_pixel,
    .draw_line = cb_draw_line,
    .draw_rect = cb_draw_rect,
    .draw_circle = cb_draw_circle,
    .draw_string = cb_draw_string,
    .draw_bitmap = cb_draw_bitmap,
    .clear_screen = cb_clear_screen,
    .flush_display = cb_flush_display,

    .get_button_state = cb_get_button_state,

    .get_telemetry = cb_get_telemetry,
    .enable_health_sensor = cb_enable_health_sensor,
    .get_ppg_buffer = cb_get_ppg_buffer,

    .ir_send_raw = cb_ir_send_raw,
    .ir_send_nec = cb_ir_send_nec,
    .ir_has_received = cb_ir_has_received,
    .ir_get_received = cb_ir_get_received,

    .play_tone = cb_play_tone,
    .stop_tone = cb_stop_tone,

    .set_led = cb_set_led,

    .millis = cb_millis,
    .micros = cb_micros,
    .delay_ms = cb_delay_ms,
    .random_range = cb_random_range,

    .file_read = cb_file_read,
    .file_write = cb_file_write,
    .file_exists = cb_file_exists,

    .log_print = cb_log_print,
    .exit_app = cb_exit_app
};

const QWatchAPI* get_mock_qwatch_api(void) {
    return &s_mock_api;
}

void mock_reset_state(void) {
    memset(s_mock_fb, 0, sizeof(s_mock_fb));
    s_mock_buttons = 0;
    memset(&s_mock_telemetry, 0, sizeof(s_mock_telemetry));
    s_last_tone_freq = 0;
    s_last_tone_dur = 0;
    s_last_led_r = 0;
    s_last_led_g = 0;
    s_last_led_b = 0;
    s_exit_requested = false;
    s_health_enabled = false;
    s_flush_count = 0;
}

void mock_set_telemetry(const QTelemetry* telem) {
    if (telem) {
        memcpy(&s_mock_telemetry, telem, sizeof(QTelemetry));
    }
}

void mock_set_buttons(uint8_t btn_mask) {
    s_mock_buttons = btn_mask;
}

uint8_t* mock_get_display_buffer(void) {
    return s_mock_fb;
}

uint16_t mock_get_last_tone_freq(void) {
    return s_last_tone_freq;
}

uint16_t mock_get_last_tone_duration(void) {
    return s_last_tone_dur;
}

void mock_get_last_led(uint8_t* r, uint8_t* g, uint8_t* b) {
    if (r) *r = s_last_led_r;
    if (g) *g = s_last_led_g;
    if (b) *b = s_last_led_b;
}

bool mock_is_exit_requested(void) {
    return s_exit_requested;
}

bool mock_is_health_sensor_enabled(void) {
    return s_health_enabled;
}

int mock_get_flush_count(void) {
    return s_flush_count;
}
