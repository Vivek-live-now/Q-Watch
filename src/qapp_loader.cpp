#include "qapp_loader.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <LittleFS.h>
#include <esp_heap_caps.h>

#if __has_include(<esp_rom_spiflash.h>)
#include <esp_rom_spiflash.h>
#define QAPP_FLUSH_ICACHE() esp_rom_spiflash_cache_flush()
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ESP32S3)
#if __has_include("esp32s3/rom/cache.h")
#include "esp32s3/rom/cache.h"
#define QAPP_FLUSH_ICACHE() Cache_Invalidate_ICache_All()
#else
extern "C" void Cache_Invalidate_ICache_All(void);
#define QAPP_FLUSH_ICACHE() Cache_Invalidate_ICache_All()
#endif
#elif defined(CONFIG_IDF_TARGET_ESP32)
#if __has_include("esp32/rom/cache.h")
#include "esp32/rom/cache.h"
#define QAPP_FLUSH_ICACHE() Cache_Flush(0)
#else
extern "C" void Cache_Flush(int);
#define QAPP_FLUSH_ICACHE() Cache_Flush(0)
#endif
#else
#define QAPP_FLUSH_ICACHE() do {} while(0)
#endif

#include "display.h"
#include "sensors.h"
#include "max30102_manager.h"
#include "sound_manager.h"
#include "led_manager.h"
#include "ir_engine.h"
#include "file_manager.h"
#include "clock.h"
#include "battery.h"
#include "hw_config.h"
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

QAppLoader qappLoader;

// -------------------------------------------------------------
// Live Hardware Callback Implementations
// -------------------------------------------------------------
#ifdef ARDUINO

static uint8_t* live_get_framebuffer(void) {
    return oled.getBufferPtr();
}

static void live_draw_pixel(int16_t x, int16_t y, uint8_t color) {
    oled.setDrawColor(color ? 1 : 0);
    oled.drawPixel(x, y);
}

static void live_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color) {
    oled.setDrawColor(color ? 1 : 0);
    oled.drawLine(x0, y0, x1, y1);
}

static void live_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color, bool fill) {
    oled.setDrawColor(color ? 1 : 0);
    if (fill) {
        oled.drawBox(x, y, w, h);
    } else {
        oled.drawFrame(x, y, w, h);
    }
}

static void live_draw_circle(int16_t x, int16_t y, int16_t r, uint8_t color, bool fill) {
    oled.setDrawColor(color ? 1 : 0);
    if (fill) {
        oled.drawDisc(x, y, r);
    } else {
        oled.drawCircle(x, y, r);
    }
}

static void live_draw_string(int16_t x, int16_t y, const char* str, uint8_t font_size) {
    if (!str) return;
    oled.setDrawColor(1);
    if (font_size == 0) {
        oled.setFont(u8g2_font_4x6_tr);
    } else if (font_size == 1) {
        oled.setFont(u8g2_font_6x10_tr);
    } else {
        oled.setFont(u8g2_font_profont12_tr);
    }
    oled.drawStr(x, y + 8, str);
}

static void live_draw_bitmap(int16_t x, int16_t y, int16_t w, int16_t h, const uint8_t* bitmap) {
    if (!bitmap) return;
    oled.setDrawColor(1);
    oled.drawXBMP(x, y, w, h, bitmap);
}

static void live_clear_screen(void) {
    oled.clearBuffer();
}

static void live_flush_display(void) {
    oled.sendBuffer();
}

static uint8_t live_get_button_state(void) {
    uint8_t state = 0;
    if (digitalRead(BTN_UP) == LOW)     state |= QBTN_UP;
    if (digitalRead(BTN_OK) == LOW)     state |= QBTN_OK;
    if (digitalRead(BTN_DN) == LOW)     state |= QBTN_DOWN;
    if (digitalRead(BTN_CANCEL) == LOW) state |= QBTN_CANCEL;
    return state;
}

static void live_get_telemetry(QTelemetry* out) {
    if (!out) return;
    OrientationData ori = sensors.getOrientation();
    CalibratedSensorData cal = sensors.getCalData();
    EnvironmentData env = sensors.getEnvData();
    HealthMetrics hm = max30102Manager.getMetrics();

    out->pitch = ori.pitch;
    out->roll = ori.roll;
    out->yaw = ori.yaw;

    out->accel_x = cal.ax;
    out->accel_y = cal.ay;
    out->accel_z = cal.az;
    out->gyro_x = cal.gx;
    out->gyro_y = cal.gy;
    out->gyro_z = cal.gz;

    out->mag_x = cal.mx;
    out->mag_y = cal.my;
    out->mag_z = cal.mz;
    out->heading = ori.yaw;
    out->mag_calibrated = sensors.isMagOk();

    out->heart_rate_bpm = (uint16_t)hm.bpm;
    out->spo2_pct = (uint8_t)hm.spo2;
    out->finger_detected = hm.finger_detected;
    out->max30102_temp_c = hm.temperature;
    out->ppg_raw_red = hm.red_value;
    out->ppg_raw_ir = hm.ir_value;

    out->temperature_c = env.temperature;
    out->humidity_pct = env.humidity;
    out->pressure_hpa = env.pressure;
    out->altitude_m = env.altitude;

    out->battery_pct = (uint8_t)battery.readPercentage();
    out->battery_mv = (uint16_t)(battery.readVoltage() * 1000.0f);
    out->is_charging = false;

    out->hour = (uint8_t)qclock.getHour();
    out->minute = (uint8_t)qclock.getMinute();
    out->second = (uint8_t)qclock.getSecond();
    out->day = (uint8_t)qclock.getDay();
    out->month = (uint8_t)qclock.getMonth();
    out->year = (uint16_t)qclock.getYear();
}

static void live_enable_health_sensor(bool enable) {
    if (enable) {
        max30102Manager.enableSensor();
    } else {
        max30102Manager.disableSensor();
    }
}

static void live_get_ppg_buffer(uint32_t* red_buf, uint32_t* ir_buf, uint16_t count) {
    (void)red_buf; (void)ir_buf; (void)count;
}

static void live_ir_send_raw(const uint16_t* timings, uint16_t length, uint16_t khz) {
    irEngine.sendRaw(timings, length, khz * 1000);
}

static void live_ir_send_nec(uint32_t address, uint32_t command) {
    irEngine.sendParsed("NEC", address, command, 32);
}

static bool live_ir_has_received(void) {
    return false;
}

static bool live_ir_get_received(uint32_t* protocol, uint32_t* address, uint32_t* command) {
    (void)protocol; (void)address; (void)command;
    return false;
}

static void live_play_tone(uint16_t freq_hz, uint16_t duration_ms) {
    soundManager.playTone(freq_hz, duration_ms);
}

static void live_stop_tone(void) {
    soundManager.stop();
}

static void live_set_led(uint8_t r, uint8_t g, uint8_t b) {
    if (r == 0 && g == 0 && b == 0) {
        ledManager.setMode(LedMode::OFF);
    } else {
        ledManager.setMode(LedMode::SOLID);
        ledManager.setColor(CRGB(r, g, b));
    }
}

static uint32_t live_millis(void) {
    return millis();
}

static uint32_t live_micros(void) {
    return micros();
}

static void live_delay_ms(uint32_t ms) {
    delay(ms);
}

static uint32_t live_random_range(uint32_t min, uint32_t max) {
    if (min >= max) return min;
    return random(min, max + 1);
}

static int live_file_read(const char* path, void* buf, uint32_t max_len) {
    if (!path || !buf || max_len == 0) return -1;
    String full_path = String("/apps/") + path;
    if (!LittleFS.exists(full_path)) return -1;
    File f = LittleFS.open(full_path, "r");
    if (!f) return -1;
    size_t read_bytes = f.read((uint8_t*)buf, max_len);
    f.close();
    return (int)read_bytes;
}

static int live_file_write(const char* path, const void* buf, uint32_t len) {
    if (!path || !buf) return -1;
    String full_path = String("/apps/") + path;
    File f = LittleFS.open(full_path, "w");
    if (!f) return -1;
    size_t written = f.write((const uint8_t*)buf, len);
    f.close();
    return (int)written;
}

static bool live_file_exists(const char* path) {
    if (!path) return false;
    String full_path = String("/apps/") + path;
    return LittleFS.exists(full_path);
}

static void live_log_print(const char* msg) {
    if (msg) Serial.println(msg);
}

static void live_exit_app(void) {
    qappLoader.unloadApp();
}

#endif // ARDUINO

#ifndef ARDUINO
extern "C" const QWatchAPI* get_mock_qwatch_api(void);
#endif

// Master live dispatch table
static QWatchAPI s_live_qwatch_api;
static bool s_api_initialized = false;

void QAppLoader::initLiveApi() {
    if (s_api_initialized) return;

    memset(&s_live_qwatch_api, 0, sizeof(QWatchAPI));
    s_live_qwatch_api.api_version = QAPP_API_VERSION;
    s_live_qwatch_api.supported_caps = QAPP_CAP_ALL;

#ifdef ARDUINO
    s_live_qwatch_api.get_framebuffer = live_get_framebuffer;
    s_live_qwatch_api.draw_pixel = live_draw_pixel;
    s_live_qwatch_api.draw_line = live_draw_line;
    s_live_qwatch_api.draw_rect = live_draw_rect;
    s_live_qwatch_api.draw_circle = live_draw_circle;
    s_live_qwatch_api.draw_string = live_draw_string;
    s_live_qwatch_api.draw_bitmap = live_draw_bitmap;
    s_live_qwatch_api.clear_screen = live_clear_screen;
    s_live_qwatch_api.flush_display = live_flush_display;

    s_live_qwatch_api.get_button_state = live_get_button_state;

    s_live_qwatch_api.get_telemetry = live_get_telemetry;
    s_live_qwatch_api.enable_health_sensor = live_enable_health_sensor;
    s_live_qwatch_api.get_ppg_buffer = live_get_ppg_buffer;

    s_live_qwatch_api.ir_send_raw = live_ir_send_raw;
    s_live_qwatch_api.ir_send_nec = live_ir_send_nec;
    s_live_qwatch_api.ir_has_received = live_ir_has_received;
    s_live_qwatch_api.ir_get_received = live_ir_get_received;

    s_live_qwatch_api.play_tone = live_play_tone;
    s_live_qwatch_api.stop_tone = live_stop_tone;

    s_live_qwatch_api.set_led = live_set_led;

    s_live_qwatch_api.millis = live_millis;
    s_live_qwatch_api.micros = live_micros;
    s_live_qwatch_api.delay_ms = live_delay_ms;
    s_live_qwatch_api.random_range = live_random_range;

    s_live_qwatch_api.file_read = live_file_read;
    s_live_qwatch_api.file_write = live_file_write;
    s_live_qwatch_api.file_exists = live_file_exists;

    s_live_qwatch_api.log_print = live_log_print;
    s_live_qwatch_api.exit_app = live_exit_app;
#else
    const QWatchAPI* mock = get_mock_qwatch_api();
    if (mock) {
        memcpy(&s_live_qwatch_api, mock, sizeof(QWatchAPI));
    }
#endif

    s_api_initialized = true;
}

const QWatchAPI* QAppLoader::getLiveApi() {
    if (!s_api_initialized) {
        initLiveApi();
    }
    return &s_live_qwatch_api;
}

// -------------------------------------------------------------
// QAppLoader Implementation
// -------------------------------------------------------------
QAppLoader::QAppLoader()
    : is_running(false),
      code_memory(nullptr),
      code_allocated_size(0),
      data_memory(nullptr),
      data_allocated_size(0),
      using_psram(false) {
    memset(&active_file_header, 0, sizeof(QAppFileHeader));
    memset(&active_header, 0, sizeof(QAppHeader));
}

QAppLoader::~QAppLoader() {
    unloadApp();
}

QAppErrorCode QAppLoader::validateFileHeader(const QAppFileHeader& hdr, uint32_t fileSize) {
    if (fileSize < sizeof(QAppFileHeader)) {
        return QAPP_ERR_CORRUPT_HEADER;
    }
    if (hdr.magic != QAPP_MAGIC) {
        return QAPP_ERR_MAGIC;
    }
    if (hdr.api_version < QAPP_MIN_COMPAT_VERSION || hdr.api_version > QAPP_API_VERSION) {
        return QAPP_ERR_VERSION;
    }
    if ((hdr.required_caps & ~QAPP_CAP_ALL) != 0) {
        return QAPP_ERR_CAPS_UNSUPPORTED;
    }
    if (hdr.code_size == 0) {
        return QAPP_ERR_CORRUPT_HEADER;
    }
    if (hdr.code_offset + hdr.code_size > fileSize) {
        return QAPP_ERR_CORRUPT_HEADER;
    }
    if (hdr.data_offset + hdr.data_size > fileSize) {
        return QAPP_ERR_CORRUPT_HEADER;
    }
    uint32_t reloc_end = hdr.reloc_offset + (hdr.reloc_count * sizeof(QAppReloc));
    if (reloc_end > fileSize) {
        return QAPP_ERR_CORRUPT_HEADER;
    }
    if (hdr.entry_offset >= hdr.code_size) {
        return QAPP_ERR_NULL_ENTRY;
    }
    return QAPP_OK;
}

QAppErrorCode QAppLoader::inspectFile(const char* path, QAppFileHeader* out_hdr) {
    if (!path || !out_hdr) return QAPP_ERR_INVALID_PARAM;

#ifdef ARDUINO
    if (!LittleFS.exists(path)) return QAPP_ERR_FILE_NOT_FOUND;
    File f = LittleFS.open(path, "r");
    if (!f) return QAPP_ERR_FILE_NOT_FOUND;
    uint32_t fsize = f.size();
    if (fsize < sizeof(QAppFileHeader)) {
        f.close();
        return QAPP_ERR_CORRUPT_HEADER;
    }
    size_t read_bytes = f.read((uint8_t*)out_hdr, sizeof(QAppFileHeader));
    f.close();
    if (read_bytes != sizeof(QAppFileHeader)) return QAPP_ERR_CORRUPT_HEADER;
    return validateFileHeader(*out_hdr, fsize);
#else
    FILE* fp = fopen(path, "rb");
    if (!fp) return QAPP_ERR_FILE_NOT_FOUND;
    fseek(fp, 0, SEEK_END);
    uint32_t fsize = (uint32_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (fsize < sizeof(QAppFileHeader)) {
        fclose(fp);
        return QAPP_ERR_CORRUPT_HEADER;
    }
    size_t read_bytes = fread(out_hdr, 1, sizeof(QAppFileHeader), fp);
    fclose(fp);
    if (read_bytes != sizeof(QAppFileHeader)) return QAPP_ERR_CORRUPT_HEADER;
    return validateFileHeader(*out_hdr, fsize);
#endif
}

QAppErrorCode QAppLoader::inspectFile(const char* path, QAppHeader* out_hdr) {
    if (!path || !out_hdr) return QAPP_ERR_INVALID_PARAM;
    QAppFileHeader fhdr;
    QAppErrorCode err = inspectFile(path, &fhdr);
    if (err != QAPP_OK) return err;

    memset(out_hdr, 0, sizeof(QAppHeader));
    out_hdr->magic = fhdr.magic;
    out_hdr->api_version = fhdr.api_version;
    out_hdr->required_caps = fhdr.required_caps;
    strncpy(out_hdr->name, fhdr.name, sizeof(out_hdr->name) - 1);
    strncpy(out_hdr->version, fhdr.version, sizeof(out_hdr->version) - 1);
    strncpy(out_hdr->author, fhdr.author, sizeof(out_hdr->author) - 1);
    out_hdr->required_psram = fhdr.required_psram;
    return QAPP_OK;
}

QAppErrorCode QAppLoader::loadApp(const char* path) {
    if (is_running) return QAPP_ERR_ALREADY_RUNNING;
    if (!path) return QAPP_ERR_INVALID_PARAM;

#ifdef ARDUINO
    if (!LittleFS.exists(path)) return QAPP_ERR_FILE_NOT_FOUND;
    File f = LittleFS.open(path, "r");
    if (!f) return QAPP_ERR_FILE_NOT_FOUND;
    uint32_t fsize = f.size();

    QAppFileHeader fhdr;
    if (f.read((uint8_t*)&fhdr, sizeof(QAppFileHeader)) != sizeof(QAppFileHeader)) {
        f.close();
        return QAPP_ERR_CORRUPT_HEADER;
    }
    QAppErrorCode err = validateFileHeader(fhdr, fsize);
    if (err != QAPP_OK) {
        f.close();
        return err;
    }

    // 1. Allocate Executable Code Memory in internal IRAM
    uint8_t* code_buf = (uint8_t*)heap_caps_malloc(
        fhdr.code_size,
        MALLOC_CAP_EXEC | MALLOC_CAP_32BIT | MALLOC_CAP_INTERNAL
    );
    if (!code_buf) {
        f.close();
        return QAPP_ERR_OOM;
    }

    // 2. Allocate Data & BSS in external 2MB PSRAM
    uint32_t total_data = fhdr.data_size + fhdr.bss_size + fhdr.required_psram;
    uint8_t* data_buf = nullptr;
    bool in_psram = false;
    if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) >= total_data) {
        data_buf = (uint8_t*)heap_caps_malloc(total_data, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (data_buf) in_psram = true;
    }
    if (!data_buf) {
        data_buf = (uint8_t*)malloc(total_data);
    }
    if (!data_buf) {
        heap_caps_free(code_buf);
        f.close();
        return QAPP_ERR_OOM;
    }

    // 3. Read Code Section into IRAM
    f.seek(fhdr.code_offset);
    if (f.read(code_buf, fhdr.code_size) != fhdr.code_size) {
        heap_caps_free(code_buf);
        free(data_buf);
        f.close();
        return QAPP_ERR_CORRUPT_HEADER;
    }

    // 4. Read Initialized Data into PSRAM
    if (fhdr.data_size > 0) {
        f.seek(fhdr.data_offset);
        if (f.read(data_buf, fhdr.data_size) != fhdr.data_size) {
            heap_caps_free(code_buf);
            free(data_buf);
            f.close();
            return QAPP_ERR_CORRUPT_HEADER;
        }
    }
    // Zero out BSS
    if (fhdr.bss_size > 0) {
        memset(data_buf + fhdr.data_size, 0, fhdr.bss_size);
    }

    // 5. Read and Apply Relocations
    initLiveApi();
    if (fhdr.reloc_count > 0) {
        f.seek(fhdr.reloc_offset);
        for (uint32_t i = 0; i < fhdr.reloc_count; i++) {
            QAppReloc reloc;
            if (f.read((uint8_t*)&reloc, sizeof(QAppReloc)) != sizeof(QAppReloc)) {
                heap_caps_free(code_buf);
                free(data_buf);
                f.close();
                return QAPP_ERR_CORRUPT_HEADER;
            }

            uint8_t* target_base = (reloc.section == 0) ? code_buf : data_buf;
            uint32_t* target_word = (uint32_t*)(target_base + reloc.offset);

            if (reloc.type == QRELOC_CODE_ADDR) {
                *target_word += (uint32_t)(uintptr_t)code_buf;
            } else if (reloc.type == QRELOC_DATA_ADDR) {
                *target_word += (uint32_t)(uintptr_t)data_buf;
            } else if (reloc.type == QRELOC_API_TABLE) {
                *target_word = (uint32_t)(uintptr_t)&s_live_qwatch_api;
            }
        }
    }
    f.close();

    // 6. Invalidate CPU Instruction Cache so IRAM is coherent
    QAPP_FLUSH_ICACHE();
#if defined(__XTENSA__)
    asm volatile("isync\n\tmemw\n\t");
#endif

    // 7. Invoke Relocated Entry Point
    QAppEntryFunc entry_fn = (QAppEntryFunc)((uintptr_t)code_buf + fhdr.entry_offset);
    const QAppHeader* hdr = entry_fn(&s_live_qwatch_api);
    if (!hdr || !hdr->init || !hdr->update || !hdr->render || !hdr->teardown) {
        heap_caps_free(code_buf);
        free(data_buf);
        return QAPP_ERR_INIT_FAILED;
    }

    code_memory = code_buf;
    code_allocated_size = fhdr.code_size;
    data_memory = data_buf;
    data_allocated_size = total_data;
    using_psram = in_psram;
    active_file_header = fhdr;
    active_header = *hdr;

    int init_res = active_header.init(&s_live_qwatch_api);
    if (init_res != QAPP_OK) {
        unloadApp();
        return QAPP_ERR_INIT_FAILED;
    }

    is_running = true;
    return QAPP_OK;

#else
    FILE* fp = fopen(path, "rb");
    if (!fp) return QAPP_ERR_FILE_NOT_FOUND;
    fseek(fp, 0, SEEK_END);
    uint32_t fsize = (uint32_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);

    QAppFileHeader fhdr;
    if (fread(&fhdr, 1, sizeof(QAppFileHeader), fp) != sizeof(QAppFileHeader)) {
        fclose(fp);
        return QAPP_ERR_CORRUPT_HEADER;
    }
    QAppErrorCode err = validateFileHeader(fhdr, fsize);
    if (err != QAPP_OK) {
        fclose(fp);
        return err;
    }

    // 1. Allocate Executable Memory via mmap on Host
    size_t page_size = sysconf(_SC_PAGESIZE);
    size_t alloc_code_size = ((fhdr.code_size + page_size - 1) / page_size) * page_size;
    uint8_t* code_buf = (uint8_t*)mmap(
        NULL, alloc_code_size,
        PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_ANONYMOUS | MAP_PRIVATE, -1, 0
    );
    if (code_buf == MAP_FAILED) {
        fclose(fp);
        return QAPP_ERR_OOM;
    }

    // 2. Allocate Data Memory
    uint32_t total_data = fhdr.data_size + fhdr.bss_size + fhdr.required_psram;
    uint8_t* data_buf = (uint8_t*)malloc(total_data ? total_data : 16);
    if (!data_buf) {
        munmap(code_buf, alloc_code_size);
        fclose(fp);
        return QAPP_ERR_OOM;
    }

    // 3. Read Code Section
    fseek(fp, fhdr.code_offset, SEEK_SET);
    if (fread(code_buf, 1, fhdr.code_size, fp) != fhdr.code_size) {
        munmap(code_buf, alloc_code_size);
        free(data_buf);
        fclose(fp);
        return QAPP_ERR_CORRUPT_HEADER;
    }

    // 4. Read Data Section
    if (fhdr.data_size > 0) {
        fseek(fp, fhdr.data_offset, SEEK_SET);
        if (fread(data_buf, 1, fhdr.data_size, fp) != fhdr.data_size) {
            munmap(code_buf, alloc_code_size);
            free(data_buf);
            fclose(fp);
            return QAPP_ERR_CORRUPT_HEADER;
        }
    }
    if (fhdr.bss_size > 0) {
        memset(data_buf + fhdr.data_size, 0, fhdr.bss_size);
    }

    // 5. Read and Apply Relocations
    initLiveApi();
    if (fhdr.reloc_count > 0) {
        fseek(fp, fhdr.reloc_offset, SEEK_SET);
        for (uint32_t i = 0; i < fhdr.reloc_count; i++) {
            QAppReloc reloc;
            if (fread(&reloc, 1, sizeof(QAppReloc), fp) != sizeof(QAppReloc)) {
                munmap(code_buf, alloc_code_size);
                free(data_buf);
                fclose(fp);
                return QAPP_ERR_CORRUPT_HEADER;
            }

            uint8_t* target_base = (reloc.section == 0) ? code_buf : data_buf;
            uintptr_t* target_word = (uintptr_t*)(target_base + reloc.offset);

            if (reloc.type == QRELOC_CODE_ADDR) {
                *target_word += (uintptr_t)code_buf;
            } else if (reloc.type == QRELOC_DATA_ADDR) {
                *target_word += (uintptr_t)data_buf;
            } else if (reloc.type == QRELOC_API_TABLE) {
                *target_word = (uintptr_t)&s_live_qwatch_api;
            }
        }
    }
    fclose(fp);

    __builtin___clear_cache((char*)code_buf, (char*)code_buf + alloc_code_size);

    // 6. Invoke Relocated Entry Point
    QAppEntryFunc entry_fn = (QAppEntryFunc)((uintptr_t)code_buf + fhdr.entry_offset);
    const QAppHeader* hdr = entry_fn(&s_live_qwatch_api);
    if (!hdr || !hdr->init || !hdr->update || !hdr->render || !hdr->teardown) {
        munmap(code_buf, alloc_code_size);
        free(data_buf);
        return QAPP_ERR_INIT_FAILED;
    }

    code_memory = code_buf;
    code_allocated_size = (uint32_t)alloc_code_size;
    data_memory = data_buf;
    data_allocated_size = total_data;
    using_psram = false;
    active_file_header = fhdr;
    active_header = *hdr;

    int init_res = active_header.init(&s_live_qwatch_api);
    if (init_res != QAPP_OK) {
        unloadApp();
        return QAPP_ERR_INIT_FAILED;
    }

    is_running = true;
    return QAPP_OK;
#endif
}

QAppErrorCode QAppLoader::loadAppFromMemory(const uint8_t* buffer, uint32_t size) {
    if (is_running) return QAPP_ERR_ALREADY_RUNNING;
    if (!buffer || size < sizeof(QAppFileHeader)) return QAPP_ERR_INVALID_PARAM;

    const QAppFileHeader* fhdr = (const QAppFileHeader*)buffer;
    QAppErrorCode err = validateFileHeader(*fhdr, size);
    if (err != QAPP_OK) return err;

#ifdef ARDUINO
    uint8_t* code_buf = (uint8_t*)heap_caps_malloc(
        fhdr->code_size,
        MALLOC_CAP_EXEC | MALLOC_CAP_32BIT | MALLOC_CAP_INTERNAL
    );
    if (!code_buf) return QAPP_ERR_OOM;

    uint32_t total_data = fhdr->data_size + fhdr->bss_size + fhdr->required_psram;
    uint8_t* data_buf = (uint8_t*)malloc(total_data ? total_data : 16);
    if (!data_buf) {
        heap_caps_free(code_buf);
        return QAPP_ERR_OOM;
    }

    memcpy(code_buf, buffer + fhdr->code_offset, fhdr->code_size);
    if (fhdr->data_size > 0) {
        memcpy(data_buf, buffer + fhdr->data_offset, fhdr->data_size);
    }
    if (fhdr->bss_size > 0) {
        memset(data_buf + fhdr->data_size, 0, fhdr->bss_size);
    }

    initLiveApi();
    const QAppReloc* relocs = (const QAppReloc*)(buffer + fhdr->reloc_offset);
    for (uint32_t i = 0; i < fhdr->reloc_count; i++) {
        uint8_t* target_base = (relocs[i].section == 0) ? code_buf : data_buf;
        uint32_t* target_word = (uint32_t*)(target_base + relocs[i].offset);

        if (relocs[i].type == QRELOC_CODE_ADDR) {
            *target_word += (uint32_t)(uintptr_t)code_buf;
        } else if (relocs[i].type == QRELOC_DATA_ADDR) {
            *target_word += (uint32_t)(uintptr_t)data_buf;
        } else if (relocs[i].type == QRELOC_API_TABLE) {
            *target_word = (uint32_t)(uintptr_t)&s_live_qwatch_api;
        }
    }

    QAPP_FLUSH_ICACHE();
#if defined(__XTENSA__)
    asm volatile("isync\n\tmemw\n\t");
#endif

    QAppEntryFunc entry_fn = (QAppEntryFunc)((uintptr_t)code_buf + fhdr->entry_offset);
    const QAppHeader* hdr = entry_fn(&s_live_qwatch_api);
    if (!hdr || !hdr->init || !hdr->update || !hdr->render || !hdr->teardown) {
        heap_caps_free(code_buf);
        free(data_buf);
        return QAPP_ERR_INIT_FAILED;
    }

    code_memory = code_buf;
    code_allocated_size = fhdr->code_size;
    data_memory = data_buf;
    data_allocated_size = total_data;
    active_file_header = *fhdr;
    active_header = *hdr;

    int init_res = active_header.init(&s_live_qwatch_api);
    if (init_res != QAPP_OK) {
        unloadApp();
        return QAPP_ERR_INIT_FAILED;
    }

    is_running = true;
    return QAPP_OK;

#else
    size_t page_size = sysconf(_SC_PAGESIZE);
    size_t alloc_code_size = ((fhdr->code_size + page_size - 1) / page_size) * page_size;
    uint8_t* code_buf = (uint8_t*)mmap(
        NULL, alloc_code_size,
        PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_ANONYMOUS | MAP_PRIVATE, -1, 0
    );
    if (code_buf == MAP_FAILED) return QAPP_ERR_OOM;

    uint32_t total_data = fhdr->data_size + fhdr->bss_size + fhdr->required_psram;
    uint8_t* data_buf = (uint8_t*)malloc(total_data ? total_data : 16);
    if (!data_buf) {
        munmap(code_buf, alloc_code_size);
        return QAPP_ERR_OOM;
    }

    memcpy(code_buf, buffer + fhdr->code_offset, fhdr->code_size);
    if (fhdr->data_size > 0) {
        memcpy(data_buf, buffer + fhdr->data_offset, fhdr->data_size);
    }
    if (fhdr->bss_size > 0) {
        memset(data_buf + fhdr->data_size, 0, fhdr->bss_size);
    }

    initLiveApi();
    const QAppReloc* relocs = (const QAppReloc*)(buffer + fhdr->reloc_offset);
    for (uint32_t i = 0; i < fhdr->reloc_count; i++) {
        uint8_t* target_base = (relocs[i].section == 0) ? code_buf : data_buf;
        uintptr_t* target_word = (uintptr_t*)(target_base + relocs[i].offset);

        if (relocs[i].type == QRELOC_CODE_ADDR) {
            *target_word += (uintptr_t)code_buf;
        } else if (relocs[i].type == QRELOC_DATA_ADDR) {
            *target_word += (uintptr_t)data_buf;
        } else if (relocs[i].type == QRELOC_API_TABLE) {
            *target_word = (uintptr_t)&s_live_qwatch_api;
        }
    }

    __builtin___clear_cache((char*)code_buf, (char*)code_buf + alloc_code_size);

    QAppEntryFunc entry_fn = (QAppEntryFunc)((uintptr_t)code_buf + fhdr->entry_offset);
    const QAppHeader* hdr = entry_fn(&s_live_qwatch_api);
    if (!hdr || !hdr->init || !hdr->update || !hdr->render || !hdr->teardown) {
        munmap(code_buf, alloc_code_size);
        free(data_buf);
        return QAPP_ERR_INIT_FAILED;
    }

    code_memory = code_buf;
    code_allocated_size = (uint32_t)alloc_code_size;
    data_memory = data_buf;
    data_allocated_size = total_data;
    active_file_header = *fhdr;
    active_header = *hdr;

    int init_res = active_header.init(&s_live_qwatch_api);
    if (init_res != QAPP_OK) {
        unloadApp();
        return QAPP_ERR_INIT_FAILED;
    }

    is_running = true;
    return QAPP_OK;
#endif
}

void QAppLoader::update(float dt) {
    if (is_running && active_header.update) {
        active_header.update(dt);
    }
}

void QAppLoader::render() {
    if (is_running && active_header.render) {
        active_header.render();
    }
}

void QAppLoader::handleButton(uint8_t btn, QButtonEvent evt) {
    if (is_running && active_header.on_button) {
        active_header.on_button(btn, evt);
    }
}

void QAppLoader::unloadApp() {
    if (is_running) {
        if (active_header.teardown) {
            active_header.teardown();
        }
#ifdef ARDUINO
        soundManager.stop();
        ledManager.setMode(LedMode::OFF);
        max30102Manager.disableSensor();
#endif
        is_running = false;
    }

    if (code_memory) {
#ifdef ARDUINO
        heap_caps_free(code_memory);
#else
        munmap(code_memory, code_allocated_size);
#endif
        code_memory = nullptr;
    }
    code_allocated_size = 0;

    if (data_memory) {
        free(data_memory);
        data_memory = nullptr;
    }
    data_allocated_size = 0;
    using_psram = false;
    memset(&active_file_header, 0, sizeof(QAppFileHeader));
    memset(&active_header, 0, sizeof(QAppHeader));
}
