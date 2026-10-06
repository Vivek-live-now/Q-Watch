#include "breakout.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const QWatchAPI* g_api = NULL;

#define BRICK_ROWS 4
#define BRICK_COLS 8
#define TOTAL_BRICKS (BRICK_ROWS * BRICK_COLS)
#define BREAKOUT_SAVE_PATH "/apps/breakout.dat"

typedef struct {
    bool alive;
    uint8_t points;
} Brick;

static Brick s_bricks[BRICK_ROWS][BRICK_COLS];
static int   s_bricks_left = TOTAL_BRICKS;

static float s_paddle_x = 55.0f;
static float s_paddle_w = 18.0f;
static float s_ball_x = 64.0f;
static float s_ball_y = 48.0f;
static float s_ball_vx = 35.0f;
static float s_ball_vy = -45.0f;
static bool  s_ball_launched = false;

static int   s_score = 0;
static int   s_high_score = 0;
static int   s_lives = 3;
static bool  s_game_over = false;
static bool  s_level_clear = false;

static float s_neutral_roll = 0.0f;
static bool  s_tare_done = false;
static float s_btn_override_timer = 0.0f;

static const QAppHeader s_breakout_header = {
    .magic = QAPP_MAGIC,
    .api_version = QAPP_API_VERSION,
    .required_caps = (QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_MPU | QAPP_CAP_AUDIO | QAPP_CAP_RGB_LED | QAPP_CAP_STORAGE),
    .name = "Breakout 007",
    .version = "1.0.0",
    .author = "Q-Watch Lab",
    .required_psram = 2048,
    .init = breakout_init,
    .update = breakout_update,
    .render = breakout_render,
    .on_button = breakout_on_button,
    .teardown = breakout_teardown
};

const QAppHeader* get_breakout_header(void) {
    return &s_breakout_header;
}

static void load_high_score(void) {
    if (!g_api || !g_api->file_read) return;
    int saved = 0;
    int res = g_api->file_read(BREAKOUT_SAVE_PATH, &saved, sizeof(saved));
    if (res == (int)sizeof(saved) && saved >= 0 && saved < 1000000) {
        s_high_score = saved;
    }
}

static void save_high_score(void) {
    if (!g_api || !g_api->file_write) return;
    if (s_score > s_high_score) s_high_score = s_score;
    g_api->file_write(BREAKOUT_SAVE_PATH, &s_high_score, sizeof(s_high_score));
}

static void init_bricks(void) {
    s_bricks_left = TOTAL_BRICKS;
    for (int r = 0; r < BRICK_ROWS; r++) {
        for (int c = 0; c < BRICK_COLS; c++) {
            s_bricks[r][c].alive = true;
            s_bricks[r][c].points = (uint8_t)((BRICK_ROWS - r) * 10);
        }
    }
}

static void reset_ball(void) {
    s_ball_launched = false;
    s_ball_x = s_paddle_x + s_paddle_w * 0.5f;
    s_ball_y = 54.0f;
    s_ball_vx = 35.0f;
    s_ball_vy = -45.0f;
}

int breakout_init(const QWatchAPI* api) {
    if (!api) return QAPP_ERR_INVALID_PARAM;
    g_api = api;

    s_paddle_x = 55.0f;
    s_paddle_w = 18.0f;
    s_score = 0;
    s_lives = 3;
    s_game_over = false;
    s_level_clear = false;
    s_tare_done = false;
    s_btn_override_timer = 0.0f;

    load_high_score();
    init_bricks();
    reset_ball();

    if (g_api->set_led)   g_api->set_led(0, 70, 70); // Cyan LED
    if (g_api->play_tone) g_api->play_tone(523, 60);

    return QAPP_OK;
}

void breakout_update(float dt) {
    if (!g_api) return;
    if (dt <= 0.0f || dt > 0.2f) dt = 0.016f;

    if (!s_tare_done) {
        QTelemetry telem;
        g_api->get_telemetry(&telem);
        s_neutral_roll = telem.roll;
        s_tare_done = true;
    }

    if (s_game_over || s_level_clear) return;

    // Continuous button holding checks
    if (g_api->get_button_state) {
        uint8_t btns = g_api->get_button_state();
        if (btns & QBTN_UP) {
            s_paddle_x -= 60.0f * dt;
            s_btn_override_timer = 0.35f;
        }
        if (btns & QBTN_DOWN) {
            s_paddle_x += 60.0f * dt;
            s_btn_override_timer = 0.35f;
        }
    }

    if (s_btn_override_timer > 0.0f) {
        s_btn_override_timer -= dt;
    } else {
        // Unified Sensor Calibration tilt steering
        QTelemetry telem;
        g_api->get_telemetry(&telem);
        float d_roll = telem.roll - s_neutral_roll;
        if (fabsf(d_roll) > 12.0f) {
            float steer = (d_roll > 0.0f ? (d_roll - 12.0f) : (d_roll + 12.0f)) * 1.6f;
            s_paddle_x += steer * dt;
        }
    }

    // Clamp paddle to playfield boundaries
    if (s_paddle_x < 2.0f) s_paddle_x = 2.0f;
    if (s_paddle_x + s_paddle_w > 125.0f) s_paddle_x = 125.0f - s_paddle_w;

    if (!s_ball_launched) {
        s_ball_x = s_paddle_x + s_paddle_w * 0.5f;
        s_ball_y = 54.0f;
        return;
    }

    // Ball movement
    s_ball_x += s_ball_vx * dt;
    s_ball_y += s_ball_vy * dt;

    // Wall bounces
    if (s_ball_x <= 3.0f) {
        s_ball_x = 3.0f;
        s_ball_vx = -s_ball_vx;
        if (g_api->play_tone) g_api->play_tone(330, 15);
    }
    if (s_ball_x >= 124.0f) {
        s_ball_x = 124.0f;
        s_ball_vx = -s_ball_vx;
        if (g_api->play_tone) g_api->play_tone(330, 15);
    }
    if (s_ball_y <= 12.0f) {
        s_ball_y = 12.0f;
        s_ball_vy = -s_ball_vy;
        if (g_api->play_tone) g_api->play_tone(330, 15);
    }

    // Paddle collision
    if (s_ball_y >= 54.0f && s_ball_y <= 58.0f && s_ball_vy > 0.0f) {
        if (s_ball_x >= s_paddle_x - 1.0f && s_ball_x <= s_paddle_x + s_paddle_w + 1.0f) {
            s_ball_y = 54.0f;
            s_ball_vy = -s_ball_vy;

            // Angle deflection based on hit position relative to center of paddle
            float hit_offset = (s_ball_x - (s_paddle_x + s_paddle_w * 0.5f)) / (s_paddle_w * 0.5f);
            s_ball_vx = hit_offset * 50.0f;
            if (fabsf(s_ball_vx) < 15.0f) s_ball_vx = (s_ball_vx >= 0.0f) ? 15.0f : -15.0f;

            if (g_api->play_tone) g_api->play_tone(440, 20);
        }
    }

    // Bottom loss
    if (s_ball_y > 64.0f) {
        s_lives--;
        if (g_api->play_tone) g_api->play_tone(130, 150);
        if (g_api->set_led)   g_api->set_led(80, 0, 0); // Red
        if (s_lives <= 0) {
            s_game_over = true;
            save_high_score();
        } else {
            reset_ball();
        }
        return;
    }

    // Brick collisions
    // Bricks occupy x=6 to 122, y=14 to 34 (4 rows, 8 cols, each brick 14x4 px with 1px gap)
    for (int r = 0; r < BRICK_ROWS; r++) {
        for (int c = 0; c < BRICK_COLS; c++) {
            if (!s_bricks[r][c].alive) continue;

            int16_t bx = 6 + c * 15;
            int16_t by = 14 + r * 5;
            int16_t bw = 13;
            int16_t bh = 4;

            if (s_ball_x >= (float)bx && s_ball_x <= (float)(bx + bw) &&
                s_ball_y >= (float)by && s_ball_y <= (float)(by + bh)) {
                s_bricks[r][c].alive = false;
                s_bricks_left--;
                s_score += s_bricks[r][c].points;
                if (s_score > s_high_score) s_high_score = s_score;

                s_ball_vy = -s_ball_vy;

                uint16_t tone = 523 + (uint16_t)((BRICK_ROWS - r) * 150);
                if (g_api->play_tone) g_api->play_tone(tone, 25);

                if (s_bricks_left <= 0) {
                    s_level_clear = true;
                    if (g_api->play_tone) g_api->play_tone(1046, 200);
                    save_high_score();
                }
                return;
            }
        }
    }
}

void breakout_render(void) {
    if (!g_api) return;
    g_api->clear_screen();

    // Top HUD
    char hud[32];
    snprintf(hud, sizeof(hud), "%04d HI:%04d  L:%d", s_score, s_high_score, s_lives);
    g_api->draw_string(2, 0, hud, 0);

    // Border frame
    g_api->draw_rect(0, 10, 128, 54, 1, false);

    // Draw Bricks
    for (int r = 0; r < BRICK_ROWS; r++) {
        for (int c = 0; c < BRICK_COLS; c++) {
            if (!s_bricks[r][c].alive) continue;
            int16_t bx = 6 + c * 15;
            int16_t by = 14 + r * 5;
            g_api->draw_rect(bx, by, 13, 4, 1, (r % 2 == 0));
        }
    }

    // Draw Paddle
    int16_t px = (int16_t)s_paddle_x;
    g_api->draw_rect(px, 57, (int16_t)s_paddle_w, 3, 1, true);

    // Draw Ball (2x2)
    int16_t bx = (int16_t)s_ball_x;
    int16_t by = (int16_t)s_ball_y;
    g_api->draw_rect(bx - 1, by - 1, 2, 2, 1, true);

    // Overlays
    if (!s_ball_launched && !s_game_over && !s_level_clear) {
        g_api->draw_string(32, 44, "[OK] TO SERVE", 0);
    } else if (s_game_over) {
        g_api->draw_rect(24, 20, 80, 24, 0, true);
        g_api->draw_rect(24, 20, 80, 24, 1, false);
        g_api->draw_string(36, 24, "GAME OVER", 0);
        g_api->draw_string(28, 33, "[OK] RESTART", 0);
    } else if (s_level_clear) {
        g_api->draw_rect(24, 20, 80, 24, 0, true);
        g_api->draw_rect(24, 20, 80, 24, 1, false);
        g_api->draw_string(30, 24, "LEVEL CLEARED!", 0);
        g_api->draw_string(28, 33, "[OK] NEXT ROUND", 0);
    }

    g_api->flush_display();
}

void breakout_on_button(uint8_t btn, QButtonEvent evt) {
    if (evt != QEVT_BTN_DOWN && evt != QEVT_BTN_SHORT_CLICK) return;

    if (btn == QBTN_CANCEL) {
        if (g_api && g_api->exit_app) g_api->exit_app();
        return;
    }

    if (s_game_over) {
        if (btn == QBTN_OK) breakout_init(g_api);
        return;
    }

    if (s_level_clear) {
        if (btn == QBTN_OK) {
            init_bricks();
            reset_ball();
            s_level_clear = false;
        }
        return;
    }

    if (btn == QBTN_OK) {
        if (!s_ball_launched) {
            s_ball_launched = true;
            if (g_api->play_tone) g_api->play_tone(660, 40);
        }
        return;
    }

    if (btn == QBTN_UP) {
        s_paddle_x -= 10.0f;
        s_btn_override_timer = 0.35f;
        if (g_api->play_tone) g_api->play_tone(350, 10);
    } else if (btn == QBTN_DOWN) {
        s_paddle_x += 10.0f;
        s_btn_override_timer = 0.35f;
        if (g_api->play_tone) g_api->play_tone(350, 10);
    }
}

void breakout_teardown(void) {
    if (g_api) {
        if (g_api->set_led)   g_api->set_led(0, 0, 0);
        if (g_api->stop_tone) g_api->stop_tone();
    }
    save_high_score();
    g_api = NULL;
}

int  breakout_get_score(void)       { return s_score; }
int  breakout_get_lives(void)       { return s_lives; }
int  breakout_get_bricks_left(void) { return s_bricks_left; }
bool breakout_is_game_over(void)    { return s_game_over; }
