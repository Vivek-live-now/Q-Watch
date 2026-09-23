#include "compass_hud.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define DEG_TO_RAD (M_PI / 180.0f)

static const QWatchAPI* g_api = NULL;

static float   s_raw_heading = 0.0f;
static float   s_smooth_heading = 0.0f;
static float   s_mag_x = 0.0f;
static float   s_mag_y = 0.0f;
static float   s_mag_z = 0.0f;
static float   s_mag_field = 0.0f;
static bool    s_mag_calibrated = false;

static float   s_target_bearing = 0.0f;
static bool    s_target_locked = false;
static int16_t s_declination = 0; // magnetic declination offset (-180 to +180 deg)

static const char* get_cardinal(float deg) {
    while (deg < 0.0f) deg += 360.0f;
    while (deg >= 360.0f) deg -= 360.0f;
    if (deg >= 337.5f || deg < 22.5f)  return "N";
    if (deg < 67.5f)                   return "NE";
    if (deg < 112.5f)                  return "E";
    if (deg < 157.5f)                  return "SE";
    if (deg < 202.5f)                  return "S";
    if (deg < 247.5f)                  return "SW";
    if (deg < 292.5f)                  return "W";
    return "NW";
}

static const QAppHeader s_compass_hud_header = {
    .magic = QAPP_MAGIC,
    .api_version = QAPP_API_VERSION,
    .required_caps = (QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_MAG),
    .name = "Compass HUD",
    .version = "1.0.0",
    .author = "007 Agent",
    .required_psram = 1024,
    .init = compass_hud_init,
    .update = compass_hud_update,
    .render = compass_hud_render,
    .on_button = compass_hud_on_button,
    .teardown = compass_hud_teardown
};

const QAppHeader* get_compass_hud_header(void) {
    return &s_compass_hud_header;
}

int compass_hud_init(const QWatchAPI* api) {
    if (!api) return QAPP_ERR_INVALID_PARAM;
    g_api = api;

    s_raw_heading = 0.0f;
    s_smooth_heading = 0.0f;
    s_mag_x = 0.0f;
    s_mag_y = 0.0f;
    s_mag_z = 0.0f;
    s_mag_field = 0.0f;
    s_mag_calibrated = false;

    s_target_bearing = 0.0f;
    s_target_locked = false;
    s_declination = 0;

    if (g_api->set_led) {
        g_api->set_led(0, 20, 40); // Dim tactical cyan indicator
    }
    return QAPP_OK;
}

void compass_hud_update(float dt) {
    if (!g_api) return;

    QTelemetry telem;
    memset(&telem, 0, sizeof(telem));
    g_api->get_telemetry(&telem);

    s_raw_heading = telem.heading;
    s_mag_calibrated = telem.mag_calibrated;
    s_mag_x = telem.mag_x;
    s_mag_y = telem.mag_y;
    s_mag_z = telem.mag_z;
    s_mag_field = sqrtf(telem.mag_x * telem.mag_x +
                        telem.mag_y * telem.mag_y +
                        telem.mag_z * telem.mag_z);

    // Circular angle smoothing
    float diff = s_raw_heading - s_smooth_heading;
    while (diff < -180.0f) diff += 360.0f;
    while (diff > 180.0f)  diff -= 360.0f;

    float alpha = 0.30f;
    if (dt > 0.0f && dt < 1.0f) {
        alpha = 1.0f - expf(-10.0f * dt);
        if (alpha < 0.10f) alpha = 0.10f;
        if (alpha > 0.80f) alpha = 0.80f;
    }

    s_smooth_heading += diff * alpha;
    while (s_smooth_heading < 0.0f)   s_smooth_heading += 360.0f;
    while (s_smooth_heading >= 360.0f) s_smooth_heading -= 360.0f;
}

void compass_hud_render(void) {
    if (!g_api) return;

    g_api->clear_screen();

    // Compute effective heading adjusted for user-set declination
    float eff_heading = s_smooth_heading + (float)s_declination;
    while (eff_heading < 0.0f)   eff_heading += 360.0f;
    while (eff_heading >= 360.0f) eff_heading -= 360.0f;

    // Dial geometry (centered on left side of 128x64 display)
    const int16_t cx = 34;
    const int16_t cy = 32;
    const int16_t r = 26;

    // Outer compass bezel ring
    g_api->draw_circle(cx, cy, r, 1, false);

    // Fixed cardinal tick marks on bezel
    // Top (Lubber line / straight ahead mark)
    g_api->draw_line(cx, cy - r, cx, cy - r + 4, 1);
    g_api->draw_line(cx - 1, cy - r, cx + 1, cy - r, 1);
    // Bottom
    g_api->draw_line(cx, cy + r - 3, cx, cy + r, 1);
    // Left
    g_api->draw_line(cx - r, cy, cx - r + 3, cy, 1);
    // Right
    g_api->draw_line(cx + r - 3, cy, cx + r, cy, 1);

    // Rotating needle pointing to North:
    // When device faces heading H, North lies at angle -(H + 90 deg) relative to screen center
    float rad_n = -(eff_heading + 90.0f) * DEG_TO_RAD;
    float cos_n = cosf(rad_n);
    float sin_n = sinf(rad_n);

    // North arrow tip (reach radius 21)
    int16_t nx = cx + (int16_t)roundf(cos_n * 21.0f);
    int16_t ny = cy + (int16_t)roundf(sin_n * 21.0f);

    // Perpendicular vector for needle arrowhead width
    float cos_p = -sin_n;
    float sin_p = cos_n;

    // North arrow wing vertices
    int16_t bx = cx + (int16_t)roundf(cos_n * 7.0f);
    int16_t by = cy + (int16_t)roundf(sin_n * 7.0f);
    int16_t w1x = bx + (int16_t)roundf(cos_p * 4.0f);
    int16_t w1y = by + (int16_t)roundf(sin_p * 4.0f);
    int16_t w2x = bx - (int16_t)roundf(cos_p * 4.0f);
    int16_t w2y = by - (int16_t)roundf(sin_p * 4.0f);

    // Draw tactical North arrow
    g_api->draw_line(nx, ny, w1x, w1y, 1);
    g_api->draw_line(nx, ny, w2x, w2y, 1);
    g_api->draw_line(w1x, w1y, cx, cy, 1);
    g_api->draw_line(w2x, w2y, cx, cy, 1);
    g_api->draw_line(nx, ny, cx, cy, 1);

    // South needle tail (radius 15 in opposite direction)
    int16_t sx = cx - (int16_t)roundf(cos_n * 15.0f);
    int16_t sy = cy - (int16_t)roundf(sin_n * 15.0f);
    g_api->draw_line(cx, cy, sx, sy, 1);

    // South crossbar
    int16_t sb1x = sx + (int16_t)roundf(cos_p * 3.0f);
    int16_t sb1y = sy + (int16_t)roundf(sin_p * 3.0f);
    int16_t sb2x = sx - (int16_t)roundf(cos_p * 3.0f);
    int16_t sb2y = sy - (int16_t)roundf(sin_p * 3.0f);
    g_api->draw_line(sb1x, sb1y, sb2x, sb2y, 1);

    // Center pivot point
    g_api->draw_circle(cx, cy, 2, 1, true);
    g_api->draw_pixel(cx, cy, 0);

    // Target bearing indicator on rim if waypoint lock is active
    if (s_target_locked) {
        float rel_target = -(s_target_bearing - eff_heading + 90.0f) * DEG_TO_RAD;
        int16_t tx = cx + (int16_t)roundf(cosf(rel_target) * (r - 2));
        int16_t ty = cy + (int16_t)roundf(sinf(rel_target) * (r - 2));
        g_api->draw_circle(tx, ty, 2, 1, true);
    }

    // Vertical divider line between dial and telemetry HUD
    g_api->draw_line(67, 0, 67, 63, 1);

    // Right-side HUD telemetry readout
    const char* card = get_cardinal(eff_heading);
    char buf[20];

    // Row 1: Heading degrees and cardinal direction
    snprintf(buf, sizeof(buf), "%03.0f %s", eff_heading, card);
    g_api->draw_string(71, 3, buf, 0);

    // Row 2: Course deviation or magnetic field strength
    if (s_target_locked) {
        float dev = eff_heading - s_target_bearing;
        while (dev < -180.0f) dev += 360.0f;
        while (dev > 180.0f)  dev -= 360.0f;
        if (fabsf(dev) < 1.0f) {
            snprintf(buf, sizeof(buf), "ON CRS");
        } else if (dev > 0.0f) {
            snprintf(buf, sizeof(buf), "R %2.0f", dev);
        } else {
            snprintf(buf, sizeof(buf), "L %2.0f", -dev);
        }
    } else {
        snprintf(buf, sizeof(buf), "%.0fuT", s_mag_field);
    }
    g_api->draw_string(71, 16, buf, 0);

    // Row 3: Target lock status
    if (s_target_locked) {
        snprintf(buf, sizeof(buf), "TGT:%03.0f", s_target_bearing);
    } else {
        snprintf(buf, sizeof(buf), "HOLD:OFF");
    }
    g_api->draw_string(71, 29, buf, 0);

    // Row 4: Calibration status
    if (s_mag_calibrated) {
        g_api->draw_string(71, 42, "CAL:OK", 0);
    } else {
        g_api->draw_string(71, 42, "CAL:NO", 0);
    }

    // Row 5: Declination offset
    if (s_declination != 0) {
        snprintf(buf, sizeof(buf), "DEC:%+d", s_declination);
    } else {
        snprintf(buf, sizeof(buf), "DEC: 0");
    }
    g_api->draw_string(71, 54, buf, 0);

    g_api->flush_display();
}

void compass_hud_on_button(uint8_t btn, QButtonEvent evt) {
    if (!g_api) return;

    if (btn == QBTN_OK && (evt == QEVT_BTN_DOWN || evt == QEVT_BTN_SHORT_CLICK)) {
        // Toggle Target Heading Lock / Waypoint Bearing
        s_target_locked = !s_target_locked;
        if (s_target_locked) {
            float eff_heading = s_smooth_heading + (float)s_declination;
            while (eff_heading < 0.0f)   eff_heading += 360.0f;
            while (eff_heading >= 360.0f) eff_heading -= 360.0f;
            s_target_bearing = eff_heading;
            if (g_api->play_tone) g_api->play_tone(1200, 30);
            if (g_api->set_led)   g_api->set_led(0, 50, 0); // Green lock flash
        } else {
            if (g_api->play_tone) g_api->play_tone(600, 30);
            if (g_api->set_led)   g_api->set_led(0, 20, 40); // Restore cyan
        }
    } else if (btn == QBTN_UP && (evt == QEVT_BTN_DOWN || evt == QEVT_BTN_SHORT_CLICK)) {
        // Increment magnetic declination
        s_declination++;
        if (s_declination > 180) s_declination = -180;
        if (g_api->play_tone) g_api->play_tone(1000, 15);
    } else if (btn == QBTN_DOWN && (evt == QEVT_BTN_DOWN || evt == QEVT_BTN_SHORT_CLICK)) {
        // Decrement magnetic declination
        s_declination--;
        if (s_declination < -180) s_declination = 180;
        if (g_api->play_tone) g_api->play_tone(800, 15);
    } else if (btn == QBTN_CANCEL && (evt == QEVT_BTN_DOWN || evt == QEVT_BTN_SHORT_CLICK)) {
        // Request app exit via QWatchAPI
        if (g_api->exit_app) g_api->exit_app();
    }
}

void compass_hud_teardown(void) {
    if (g_api) {
        if (g_api->set_led)   g_api->set_led(0, 0, 0);
        if (g_api->stop_tone) g_api->stop_tone();
    }
    g_api = NULL;
}

float compass_hud_get_heading(void) {
    float eff_heading = s_smooth_heading + (float)s_declination;
    while (eff_heading < 0.0f)   eff_heading += 360.0f;
    while (eff_heading >= 360.0f) eff_heading -= 360.0f;
    return eff_heading;
}

float compass_hud_get_target_bearing(void) {
    return s_target_bearing;
}

bool compass_hud_is_target_locked(void) {
    return s_target_locked;
}

int16_t compass_hud_get_declination(void) {
    return s_declination;
}

bool compass_hud_is_calibrated(void) {
    return s_mag_calibrated;
}

float compass_hud_get_field_strength(void) {
    return s_mag_field;
}
