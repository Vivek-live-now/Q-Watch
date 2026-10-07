#include "f1_race.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const QWatchAPI* g_api = NULL;

#define MAX_ENEMY_CARS 4
#define F1_SAVE_PATH "/apps/f1_race.dat"

typedef struct {
    float x;
    float y;
    float speed;
    bool  active;
} EnemyCar;

static float    s_player_x = 64.0f;
static float    s_player_y = 52.0f;
static float    s_speed = 60.0f;       // km/h (virtual)
static float    s_max_speed = 180.0f;
static bool     s_boost = false;
static float    s_boost_timer = 0.0f;
static float    s_road_scroll = 0.0f;
static float    s_curve = 0.0f;
static float    s_target_curve = 0.0f;
static float    s_curve_timer = 0.0f;

static EnemyCar s_enemies[MAX_ENEMY_CARS];
static float    s_enemy_spawn_timer = 0.0f;

static int      s_score = 0;
static int      s_high_score = 0;
static int      s_overtakes = 0;
static bool     s_game_over = false;
static float    s_sound_timer = 0.0f;

static float    s_neutral_roll = 0.0f;
static bool     s_tare_done = false;
static float    s_btn_override_timer = 0.0f;

static const QAppHeader s_f1_race_header = {
    .magic = QAPP_MAGIC,
    .api_version = QAPP_API_VERSION,
    .required_caps = (QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_MPU | QAPP_CAP_AUDIO | QAPP_CAP_RGB_LED | QAPP_CAP_STORAGE),
    .name = "F1 Grand Prix",
    .version = "1.0.0",
    .author = "Q-Watch Lab",
    .required_psram = 2048,
    .init = f1_race_init,
    .update = f1_race_update,
    .render = f1_race_render,
    .on_button = f1_race_on_button,
    .teardown = f1_race_teardown
};

const QAppHeader* get_f1_race_header(void) {
    return &s_f1_race_header;
}

static void load_high_score(void) {
    if (!g_api || !g_api->file_read) return;
    int saved = 0;
    int res = g_api->file_read(F1_SAVE_PATH, &saved, sizeof(saved));
    if (res == (int)sizeof(saved) && saved >= 0 && saved < 1000000) {
        s_high_score = saved;
    }
}

static void save_high_score(void) {
    if (!g_api || !g_api->file_write) return;
    if (s_score > s_high_score) s_high_score = s_score;
    g_api->file_write(F1_SAVE_PATH, &s_high_score, sizeof(s_high_score));
}

int f1_race_init(const QWatchAPI* api) {
    if (!api) return QAPP_ERR_INVALID_PARAM;
    g_api = api;

    s_player_x = 64.0f;
    s_player_y = 52.0f;
    s_speed = 70.0f;
    s_boost = false;
    s_boost_timer = 0.0f;
    s_road_scroll = 0.0f;
    s_curve = 0.0f;
    s_target_curve = 0.0f;
    s_curve_timer = 0.0f;
    s_score = 0;
    s_overtakes = 0;
    s_game_over = false;
    s_sound_timer = 0.0f;
    s_tare_done = false;
    s_btn_override_timer = 0.0f;

    for (int i = 0; i < MAX_ENEMY_CARS; i++) {
        s_enemies[i].active = false;
    }
    s_enemy_spawn_timer = 1.0f;

    load_high_score();

    if (g_api->set_led)   g_api->set_led(0, 0, 80); // Blue LED
    if (g_api->play_tone) g_api->play_tone(440, 80);

    return QAPP_OK;
}

static void spawn_enemy(void) {
    for (int i = 0; i < MAX_ENEMY_CARS; i++) {
        if (!s_enemies[i].active) {
            // Pick a lane: 0 = Left (42), 1 = Mid (64), 2 = Right (86)
            int lane = (g_api && g_api->random_range) ? (int)g_api->random_range(0, 3) : (rand() % 3);
            s_enemies[i].x = 42.0f + (float)lane * 22.0f;
            s_enemies[i].y = -10.0f;
            // Enemy travels forward slower than player (between 30 and 45 km/h)
            s_enemies[i].speed = 35.0f + (float)(lane * 4);
            s_enemies[i].active = true;
            break;
        }
    }
}

void f1_race_update(float dt) {
    if (!g_api) return;
    if (dt <= 0.0f || dt > 0.2f) dt = 0.016f;

    // Tare on first valid telemetry frame
    if (!s_tare_done) {
        QTelemetry telem;
        g_api->get_telemetry(&telem);
        s_neutral_roll = telem.roll;
        s_tare_done = true;
    }

    if (s_game_over) return;

    // Continuous button holding checks
    if (g_api->get_button_state) {
        uint8_t btns = g_api->get_button_state();
        if (btns & QBTN_UP) {
            s_player_x -= 55.0f * dt;
            s_btn_override_timer = 0.35f;
        }
        if (btns & QBTN_DOWN) {
            s_player_x += 55.0f * dt;
            s_btn_override_timer = 0.35f;
        }
    }

    // Decrement button override timer
    if (s_btn_override_timer > 0.0f) {
        s_btn_override_timer -= dt;
    } else {
        // Rate-of-change (dy/dx via gyro rate) + proportional tilt fusion steering
        QTelemetry telem;
        g_api->get_telemetry(&telem);
        float d_roll = telem.roll - s_neutral_roll;
        const float DEADBAND = 2.5f;
        float prop_roll = 0.0f;
        if (fabsf(d_roll) > DEADBAND) {
            prop_roll = (d_roll > 0.0f ? (d_roll - DEADBAND) : (d_roll + DEADBAND));
        }
        float steer = (prop_roll * 1.5f) + (telem.gyro_x * 0.35f);
        if (fabsf(steer) > 1.0f) {
            s_player_x += steer * dt;
        }
    }

    // Clamp player car to road boundaries (between 30 and 98)
    int16_t road_left = (int16_t)(32 + s_curve);
    int16_t road_right = (int16_t)(96 + s_curve);
    if (s_player_x < (float)(road_left + 4)) {
        s_player_x = (float)(road_left + 4);
        s_speed -= 30.0f * dt; // Grass friction penalty
    }
    if (s_player_x > (float)(road_right - 4)) {
        s_player_x = (float)(road_right - 4);
        s_speed -= 30.0f * dt;
    }

    // Boost timer
    if (s_boost) {
        s_boost_timer -= dt;
        if (s_boost_timer <= 0.0f) {
            s_boost = false;
            if (g_api->set_led) g_api->set_led(0, 0, 80);
        }
    }

    // Dynamic speed regulation
    float target_speed = s_boost ? s_max_speed : (80.0f + (float)(s_overtakes * 3));
    if (target_speed > s_max_speed) target_speed = s_max_speed;
    if (s_speed < target_speed) s_speed += 25.0f * dt;
    if (s_speed > target_speed) s_speed -= 15.0f * dt;
    if (s_speed < 40.0f) s_speed = 40.0f;

    // Road scroll & curving
    s_road_scroll += s_speed * 1.2f * dt;
    while (s_road_scroll >= 16.0f) s_road_scroll -= 16.0f;

    s_curve_timer += dt;
    if (s_curve_timer > 3.0f) {
        s_curve_timer = 0.0f;
        s_target_curve = (g_api && g_api->random_range) ? ((float)g_api->random_range(0, 25) - 12.0f) : ((float)(rand() % 25) - 12.0f);
    }
    s_curve += (s_target_curve - s_curve) * 0.8f * dt;

    // Score accumulation
    s_score += (int)(s_speed * 0.1f * dt);
    if (s_score > s_high_score) s_high_score = s_score;

    // Enemy car spawning
    s_enemy_spawn_timer -= dt;
    if (s_enemy_spawn_timer <= 0.0f) {
        spawn_enemy();
        s_enemy_spawn_timer = 1.4f - (s_speed / s_max_speed) * 0.6f;
        if (s_enemy_spawn_timer < 0.6f) s_enemy_spawn_timer = 0.6f;
    }

    // Update enemy cars
    for (int i = 0; i < MAX_ENEMY_CARS; i++) {
        if (!s_enemies[i].active) continue;

        // Relative downward speed
        float rel_speed = (s_speed - s_enemies[i].speed);
        s_enemies[i].y += rel_speed * dt;

        // Overtake check
        if (s_enemies[i].y > 64.0f) {
            s_enemies[i].active = false;
            s_overtakes++;
            s_score += 50;
            if (g_api->play_tone) g_api->play_tone(880, 20);
        }

        // Collision check with player (player is ~10x8 pixels)
        float dx = fabsf(s_enemies[i].x - s_player_x);
        float dy = fabsf(s_enemies[i].y - s_player_y);
        if (dx < 7.0f && dy < 7.0f) {
            // Crash!
            s_game_over = true;
            if (g_api->play_tone) g_api->play_tone(90, 250);
            if (g_api->set_led)   g_api->set_led(100, 0, 0); // Red
            save_high_score();
            return;
        }
    }

    // Engine sound feedback (dynamic tone frequency based on speed)
    s_sound_timer += dt;
    if (s_sound_timer > 0.12f) {
        s_sound_timer = 0.0f;
        if (g_api->play_tone) {
            uint16_t engine_tone = (uint16_t)(100.0f + (s_speed / s_max_speed) * 180.0f);
            g_api->play_tone(engine_tone, 20);
        }
    }
}

static void draw_f1_car(int16_t x, int16_t y, bool is_player) {
    if (!g_api) return;
    // Front wing (nose): 8 wide
    g_api->draw_line(x - 3, y - 4, x + 3, y - 4, 1);
    // Nose cone / body: 2 wide
    g_api->draw_line(x, y - 3, x, y + 2, 1);
    // Front wheels: 2 wide each
    g_api->draw_rect(x - 4, y - 4, 2, 3, 1, true);
    g_api->draw_rect(x + 3, y - 4, 2, 3, 1, true);
    // Cockpit: helmet dot
    g_api->draw_pixel(x, y - 1, is_player ? 0 : 1);
    // Sidepods: body width 6
    g_api->draw_rect(x - 2, y, 5, 2, 1, true);
    // Rear wheels: 2 wide each
    g_api->draw_rect(x - 4, y + 1, 2, 3, 1, true);
    g_api->draw_rect(x + 3, y + 1, 2, 3, 1, true);
    // Rear wing spoiler: 8 wide
    g_api->draw_line(x - 3, y + 4, x + 3, y + 4, 1);
}

void f1_race_render(void) {
    if (!g_api) return;
    g_api->clear_screen();

    // Top HUD Bar: Score, Speed, HI
    char hud[32];
    snprintf(hud, sizeof(hud), "%03d KM/H  %04d HI:%04d", (int)s_speed, s_score, s_high_score);
    g_api->draw_string(2, 0, hud, 0);

    // Draw Road borders
    int16_t left_curb = (int16_t)(32 + s_curve);
    int16_t right_curb = (int16_t)(96 + s_curve);

    // Left curb
    g_api->draw_line(left_curb, 10, left_curb, 63, 1);
    g_api->draw_line(left_curb - 1, 10, left_curb - 1, 63, 1);
    // Right curb
    g_api->draw_line(right_curb, 10, right_curb, 63, 1);
    g_api->draw_line(right_curb + 1, 10, right_curb + 1, 63, 1);

    // Scrolling lane markers (dashed lines)
    int16_t lane1 = left_curb + 21;
    int16_t lane2 = left_curb + 43;
    for (int y = 10; y < 64; y += 8) {
        int16_t draw_y = y + (int16_t)s_road_scroll;
        if (draw_y >= 10 && draw_y < 62) {
            g_api->draw_line(lane1, draw_y, lane1, draw_y + 3, 1);
            g_api->draw_line(lane2, draw_y, lane2, draw_y + 3, 1);
        }
    }

    // Draw enemy cars
    for (int i = 0; i < MAX_ENEMY_CARS; i++) {
        if (s_enemies[i].active && s_enemies[i].y >= 8.0f && s_enemies[i].y <= 62.0f) {
            draw_f1_car((int16_t)s_enemies[i].x, (int16_t)s_enemies[i].y, false);
        }
    }

    // Draw player car
    draw_f1_car((int16_t)s_player_x, (int16_t)s_player_y, true);

    // Overlays
    if (s_game_over) {
        g_api->draw_rect(24, 20, 80, 24, 0, true);
        g_api->draw_rect(24, 20, 80, 24, 1, false);
        g_api->draw_string(36, 24, "CRASHED!", 0);
        g_api->draw_string(28, 33, "[OK] RESTART", 0);
    }

    g_api->flush_display();
}

void f1_race_on_button(uint8_t btn, QButtonEvent evt) {
    if (evt != QEVT_BTN_DOWN && evt != QEVT_BTN_SHORT_CLICK) return;

    if (btn == QBTN_CANCEL) {
        if (g_api && g_api->exit_app) g_api->exit_app();
        return;
    }

    if (s_game_over) {
        if (btn == QBTN_OK) f1_race_init(g_api);
        return;
    }

    if (btn == QBTN_UP) {
        s_player_x -= 8.0f;
        s_btn_override_timer = 0.35f;
        if (g_api->play_tone) g_api->play_tone(400, 10);
    } else if (btn == QBTN_DOWN) {
        s_player_x += 8.0f;
        s_btn_override_timer = 0.35f;
        if (g_api->play_tone) g_api->play_tone(400, 10);
    } else if (btn == QBTN_OK) {
        // Turbo Boost
        s_boost = true;
        s_boost_timer = 2.5f;
        if (g_api->play_tone) g_api->play_tone(650, 80);
        if (g_api->set_led)   g_api->set_led(0, 80, 80); // Cyan boost
    }
}

void f1_race_teardown(void) {
    if (g_api) {
        if (g_api->set_led)   g_api->set_led(0, 0, 0);
        if (g_api->stop_tone) g_api->stop_tone();
    }
    save_high_score();
    g_api = NULL;
}

int   f1_race_get_score(void)     { return s_score; }
float f1_race_get_speed(void)     { return s_speed; }
bool  f1_race_is_game_over(void)  { return s_game_over; }
