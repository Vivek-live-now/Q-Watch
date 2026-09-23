#include "tilt_game.h"
#include <stdio.h>
#include <math.h>

static const QWatchAPI* g_api = NULL;

static float s_ball_x = 64.0f;
static float s_ball_y = 32.0f;
static float s_vel_x = 0.0f;
static float s_vel_y = 0.0f;
static int   s_score = 0;

static int16_t s_target_x = 30;
static int16_t s_target_y = 20;

static const QAppHeader s_tilt_game_header = {
    .magic = QAPP_MAGIC,
    .api_version = QAPP_API_VERSION,
    .required_caps = (QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_MPU | QAPP_CAP_AUDIO | QAPP_CAP_RGB_LED),
    .name = "Tilt Ball",
    .version = "1.0.0",
    .author = "007 Agent",
    .required_psram = 2048,
    .init = tilt_game_init,
    .update = tilt_game_update,
    .render = tilt_game_render,
    .on_button = tilt_game_on_button,
    .teardown = tilt_game_teardown
};

const QAppHeader* get_tilt_game_header(void) {
    return &s_tilt_game_header;
}

int tilt_game_init(const QWatchAPI* api) {
    if (!api) return QAPP_ERR_INVALID_PARAM;
    g_api = api;

    s_ball_x = 64.0f;
    s_ball_y = 32.0f;
    s_vel_x = 0.0f;
    s_vel_y = 0.0f;
    s_score = 0;
    s_target_x = 30;
    s_target_y = 20;

    if (g_api->set_led) {
        g_api->set_led(0, 40, 0); // Green startup LED
    }
    return QAPP_OK;
}

void tilt_game_update(float dt) {
    if (!g_api) return;

    QTelemetry telem;
    g_api->get_telemetry(&telem);

    // MPU-6500 Tilt physics
    // Roll moves X (+roll tilts right), Pitch moves Y (+pitch tilts down)
    float ax = telem.roll * 0.15f;
    float ay = telem.pitch * 0.15f;

    s_vel_x += ax * dt;
    s_vel_y += ay * dt;

    // Friction damping
    s_vel_x *= 0.95f;
    s_vel_y *= 0.95f;

    s_ball_x += s_vel_x;
    s_ball_y += s_vel_y;

    // Boundaries on 128x64 display
    if (s_ball_x < 4.0f) {
        s_ball_x = 4.0f;
        s_vel_x = -s_vel_x * 0.6f;
        if (g_api->play_tone) g_api->play_tone(400, 20);
    }
    if (s_ball_x > 123.0f) {
        s_ball_x = 123.0f;
        s_vel_x = -s_vel_x * 0.6f;
        if (g_api->play_tone) g_api->play_tone(400, 20);
    }
    if (s_ball_y < 4.0f) {
        s_ball_y = 4.0f;
        s_vel_y = -s_vel_y * 0.6f;
        if (g_api->play_tone) g_api->play_tone(400, 20);
    }
    if (s_ball_y > 59.0f) {
        s_ball_y = 59.0f;
        s_vel_y = -s_vel_y * 0.6f;
        if (g_api->play_tone) g_api->play_tone(400, 20);
    }

    // Target collision check (radius 4)
    float dx = s_ball_x - (float)s_target_x;
    float dy = s_ball_y - (float)s_target_y;
    if ((dx * dx + dy * dy) < 25.0f) {
        s_score++;
        if (g_api->play_tone) g_api->play_tone(1200, 60);
        if (g_api->set_led)   g_api->set_led(0, 0, 80); // Blue flash on score

        if (g_api->random_range) {
            s_target_x = (int16_t)g_api->random_range(12, 115);
            s_target_y = (int16_t)g_api->random_range(12, 52);
        } else {
            s_target_x = 64;
            s_target_y = 32;
        }
    }
}

void tilt_game_render(void) {
    if (!g_api) return;

    g_api->clear_screen();

    // Boundary frame
    g_api->draw_rect(0, 0, 128, 64, 1, false);

    // Target square
    g_api->draw_rect(s_target_x - 2, s_target_y - 2, 5, 5, 1, true);

    // Player ball circle
    g_api->draw_circle((int16_t)s_ball_x, (int16_t)s_ball_y, 3, 1, true);

    // HUD String
    char hud[16];
    snprintf(hud, sizeof(hud), "PTS:%d", s_score);
    g_api->draw_string(4, 2, hud, 0);

    g_api->flush_display();
}

void tilt_game_on_button(uint8_t btn, QButtonEvent evt) {
    if (btn == QBTN_OK && (evt == QEVT_BTN_DOWN || evt == QEVT_BTN_SHORT_CLICK)) {
        // Recenter ball
        s_ball_x = 64.0f;
        s_ball_y = 32.0f;
        s_vel_x = 0.0f;
        s_vel_y = 0.0f;
    }
}

void tilt_game_teardown(void) {
    if (g_api) {
        if (g_api->set_led)   g_api->set_led(0, 0, 0);
        if (g_api->stop_tone) g_api->stop_tone();
    }
    g_api = NULL;
}

float tilt_game_get_ball_x(void) { return s_ball_x; }
float tilt_game_get_ball_y(void) { return s_ball_y; }
int   tilt_game_get_score(void)  { return s_score; }
