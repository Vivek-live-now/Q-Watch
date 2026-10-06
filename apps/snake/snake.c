#include "snake.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const QWatchAPI* g_api = NULL;

#define SNAKE_MAX_LEN 160
#define GRID_W 24
#define GRID_H 12
#define CELL_SIZE 4
#define OFFSET_X 4
#define OFFSET_Y 12
#define SNAKE_SAVE_PATH "/apps/snake.dat"

typedef enum {
    DIR_RIGHT = 0,
    DIR_DOWN  = 1,
    DIR_LEFT  = 2,
    DIR_UP    = 3
} SnakeDir;

typedef struct {
    int8_t x;
    int8_t y;
} SnakePoint;

static SnakePoint s_body[SNAKE_MAX_LEN];
static int        s_length = 3;
static SnakeDir   s_dir = DIR_RIGHT;
static SnakeDir   s_next_dir = DIR_RIGHT;

static SnakePoint s_food;
static SnakePoint s_bonus_food;
static bool       s_bonus_active = false;
static float      s_bonus_timer = 0.0f;

static int        s_score = 0;
static int        s_high_score = 0;
static bool       s_game_over = false;
static bool       s_paused = false;

static float      s_step_timer = 0.0f;
static float      s_step_interval = 0.16f;

static float      s_neutral_roll = 0.0f;
static float      s_neutral_pitch = 0.0f;
static bool       s_tare_done = false;
static float      s_btn_override_timer = 0.0f;

static const QAppHeader s_snake_header = {
    .magic = QAPP_MAGIC,
    .api_version = QAPP_API_VERSION,
    .required_caps = (QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_AUDIO | QAPP_CAP_RGB_LED | QAPP_CAP_STORAGE),
    .name = "Retro Snake",
    .version = "1.0.0",
    .author = "Q-Watch Lab",
    .required_psram = 2048,
    .init = snake_init,
    .update = snake_update,
    .render = snake_render,
    .on_button = snake_on_button,
    .teardown = snake_teardown
};

const QAppHeader* get_snake_header(void) {
    return &s_snake_header;
}

static void spawn_food(void) {
    bool collides;
    int attempts = 0;
    do {
        collides = false;
        if (g_api && g_api->random_range) {
            s_food.x = (int8_t)g_api->random_range(0, GRID_W);
            s_food.y = (int8_t)g_api->random_range(0, GRID_H);
        } else {
            s_food.x = rand() % GRID_W;
            s_food.y = rand() % GRID_H;
        }
        for (int i = 0; i < s_length; i++) {
            if (s_body[i].x == s_food.x && s_body[i].y == s_food.y) {
                collides = true;
                break;
            }
        }
        attempts++;
    } while (collides && attempts < 100);
}

static void spawn_bonus(void) {
    if (g_api && g_api->random_range) {
        s_bonus_food.x = (int8_t)g_api->random_range(0, GRID_W);
        s_bonus_food.y = (int8_t)g_api->random_range(0, GRID_H);
    } else {
        s_bonus_food.x = rand() % GRID_W;
        s_bonus_food.y = rand() % GRID_H;
    }
    s_bonus_active = true;
    s_bonus_timer = 6.0f; // 6 seconds before disappearing
}

static void load_high_score(void) {
    if (!g_api || !g_api->file_read) return;
    int saved = 0;
    int res = g_api->file_read(SNAKE_SAVE_PATH, &saved, sizeof(saved));
    if (res == (int)sizeof(saved) && saved >= 0 && saved < 100000) {
        s_high_score = saved;
    }
}

static void save_high_score(void) {
    if (!g_api || !g_api->file_write) return;
    if (s_score > s_high_score) s_high_score = s_score;
    g_api->file_write(SNAKE_SAVE_PATH, &s_high_score, sizeof(s_high_score));
}

int snake_init(const QWatchAPI* api) {
    if (!api) return QAPP_ERR_INVALID_PARAM;
    g_api = api;

    s_length = 3;
    s_dir = DIR_RIGHT;
    s_next_dir = DIR_RIGHT;
    s_score = 0;
    s_game_over = false;
    s_paused = false;
    s_step_timer = 0.0f;
    s_step_interval = 0.16f;
    s_bonus_active = false;
    s_tare_done = false;
    s_btn_override_timer = 0.0f;

    s_body[0].x = 6; s_body[0].y = 5;
    s_body[1].x = 5; s_body[1].y = 5;
    s_body[2].x = 4; s_body[2].y = 5;

    load_high_score();
    spawn_food();

    if (g_api->set_led)   g_api->set_led(0, 45, 0); // Green LED
    if (g_api->play_tone) g_api->play_tone(660, 60);

    return QAPP_OK;
}

void snake_update(float dt) {
    if (!g_api) return;
    if (dt <= 0.0f || dt > 0.2f) dt = 0.016f;

    // Unified Sensor Calibration tare on first frame
    if (!s_tare_done) {
        QTelemetry telem;
        g_api->get_telemetry(&telem);
        s_neutral_roll = telem.roll;
        s_neutral_pitch = telem.pitch;
        s_tare_done = true;
    }

    if (s_game_over || s_paused) return;

    // Bonus fruit timer
    if (s_bonus_active) {
        s_bonus_timer -= dt;
        if (s_bonus_timer <= 0.0f) {
            s_bonus_active = false;
            if (g_api->set_led) g_api->set_led(0, 45, 0);
        }
    }

    // Button override timer
    if (s_btn_override_timer > 0.0f) {
        s_btn_override_timer -= dt;
    } else {
        // Tilt steering as natural tilt navigation
        QTelemetry telem;
        g_api->get_telemetry(&telem);
        float d_roll = telem.roll - s_neutral_roll;
        float d_pitch = telem.pitch - s_neutral_pitch;

        if (fabsf(d_roll) > 18.0f || fabsf(d_pitch) > 18.0f) {
            if (fabsf(d_roll) > fabsf(d_pitch)) {
                if (d_roll > 18.0f && s_dir != DIR_LEFT)  s_next_dir = DIR_RIGHT;
                else if (d_roll < -18.0f && s_dir != DIR_RIGHT) s_next_dir = DIR_LEFT;
            } else {
                if (d_pitch > 18.0f && s_dir != DIR_UP)   s_next_dir = DIR_DOWN;
                else if (d_pitch < -18.0f && s_dir != DIR_DOWN) s_next_dir = DIR_UP;
            }
        }
    }

    s_step_timer += dt;
    if (s_step_timer >= s_step_interval) {
        s_step_timer = 0.0f;
        s_dir = s_next_dir;

        // Calculate next head position
        SnakePoint new_head = s_body[0];
        if (s_dir == DIR_RIGHT) new_head.x++;
        else if (s_dir == DIR_LEFT)  new_head.x--;
        else if (s_dir == DIR_DOWN)  new_head.y++;
        else if (s_dir == DIR_UP)    new_head.y--;

        // Wall collisions
        if (new_head.x < 0 || new_head.x >= GRID_W ||
            new_head.y < 0 || new_head.y >= GRID_H) {
            s_game_over = true;
            if (g_api->play_tone) g_api->play_tone(150, 200);
            if (g_api->set_led)   g_api->set_led(80, 0, 0); // Red
            save_high_score();
            return;
        }

        // Self collisions
        for (int i = 0; i < s_length; i++) {
            if (s_body[i].x == new_head.x && s_body[i].y == new_head.y) {
                s_game_over = true;
                if (g_api->play_tone) g_api->play_tone(150, 200);
                if (g_api->set_led)   g_api->set_led(80, 0, 0);
                save_high_score();
                return;
            }
        }

        // Check food collision
        bool ate_food = (new_head.x == s_food.x && new_head.y == s_food.y);
        bool ate_bonus = (s_bonus_active && new_head.x == s_bonus_food.x && new_head.y == s_bonus_food.y);

        // Move body
        if (ate_food || ate_bonus) {
            if (s_length < SNAKE_MAX_LEN - 1) {
                s_length++;
            }
            if (ate_food) {
                s_score += 10;
                if (g_api->play_tone) g_api->play_tone(988, 30);
                spawn_food();
                // Randomly spawn bonus every 50 points
                if (s_score % 50 == 0 && !s_bonus_active) {
                    spawn_bonus();
                    if (g_api->set_led) g_api->set_led(70, 50, 0); // Yellow
                }
                // Gradually speed up
                if (s_step_interval > 0.08f) s_step_interval -= 0.003f;
            }
            if (ate_bonus) {
                s_score += 50;
                s_bonus_active = false;
                if (g_api->play_tone) g_api->play_tone(1568, 60);
                if (g_api->set_led)   g_api->set_led(0, 60, 0);
            }
            if (s_score > s_high_score) s_high_score = s_score;
        }

        for (int i = s_length - 1; i > 0; i--) {
            s_body[i] = s_body[i - 1];
        }
        s_body[0] = new_head;
    }
}

void snake_render(void) {
    if (!g_api) return;
    g_api->clear_screen();

    // Top HUD Bar
    char hud[32];
    snprintf(hud, sizeof(hud), "PTS:%04d HI:%04d", s_score, s_high_score);
    g_api->draw_string(4, 0, hud, 0);

    // Playfield border: width = GRID_W * CELL_SIZE + 2, height = GRID_H * CELL_SIZE + 2
    int16_t bx = OFFSET_X - 1;
    int16_t by = OFFSET_Y - 1;
    int16_t bw = GRID_W * CELL_SIZE + 2;
    int16_t bh = GRID_H * CELL_SIZE + 2;
    g_api->draw_rect(bx, by, bw, bh, 1, false);

    // Draw food
    int16_t fx = OFFSET_X + s_food.x * CELL_SIZE;
    int16_t fy = OFFSET_Y + s_food.y * CELL_SIZE;
    g_api->draw_rect(fx, fy, CELL_SIZE - 1, CELL_SIZE - 1, 1, true);

    // Draw bonus food (blinking circle)
    if (s_bonus_active) {
        int16_t bfx = OFFSET_X + s_bonus_food.x * CELL_SIZE + 1;
        int16_t bfy = OFFSET_Y + s_bonus_food.y * CELL_SIZE + 1;
        if (((int)(s_bonus_timer * 6.0f) % 2) == 0) {
            g_api->draw_circle(bfx, bfy, 2, 1, true);
        }
    }

    // Draw snake body
    for (int i = 0; i < s_length; i++) {
        int16_t sx = OFFSET_X + s_body[i].x * CELL_SIZE;
        int16_t sy = OFFSET_Y + s_body[i].y * CELL_SIZE;
        if (i == 0) {
            // Head: filled with center eye
            g_api->draw_rect(sx, sy, CELL_SIZE, CELL_SIZE, 1, true);
            g_api->draw_pixel(sx + 1, sy + 1, 0);
        } else {
            // Body segments
            g_api->draw_rect(sx, sy, CELL_SIZE - 1, CELL_SIZE - 1, 1, true);
        }
    }

    // Overlays
    if (s_game_over) {
        g_api->draw_rect(24, 20, 80, 24, 0, true);
        g_api->draw_rect(24, 20, 80, 24, 1, false);
        g_api->draw_string(34, 24, "GAME OVER", 0);
        g_api->draw_string(28, 33, "[OK] RESTART", 0);
    } else if (s_paused) {
        g_api->draw_rect(32, 22, 64, 18, 0, true);
        g_api->draw_rect(32, 22, 64, 18, 1, false);
        g_api->draw_string(42, 27, "PAUSED", 0);
    }

    g_api->flush_display();
}

void snake_on_button(uint8_t btn, QButtonEvent evt) {
    if (evt != QEVT_BTN_DOWN && evt != QEVT_BTN_SHORT_CLICK) return;

    if (btn == QBTN_CANCEL) {
        if (g_api && g_api->exit_app) g_api->exit_app();
        return;
    }

    if (s_game_over) {
        if (btn == QBTN_OK) {
            snake_init(g_api);
        }
        return;
    }

    if (btn == QBTN_OK) {
        s_paused = !s_paused;
        if (g_api->play_tone) g_api->play_tone(523, 20);
        return;
    }

    // UP button turns counter-clockwise:
    // RIGHT -> UP, UP -> LEFT, LEFT -> DOWN, DOWN -> RIGHT
    if (btn == QBTN_UP) {
        if (s_dir == DIR_RIGHT)      s_next_dir = DIR_UP;
        else if (s_dir == DIR_UP)    s_next_dir = DIR_LEFT;
        else if (s_dir == DIR_LEFT)  s_next_dir = DIR_DOWN;
        else if (s_dir == DIR_DOWN)  s_next_dir = DIR_RIGHT;
        s_btn_override_timer = 0.6f;
        if (g_api->play_tone) g_api->play_tone(440, 10);
    }
    // DOWN button turns clockwise:
    // RIGHT -> DOWN, DOWN -> LEFT, LEFT -> UP, UP -> RIGHT
    else if (btn == QBTN_DOWN) {
        if (s_dir == DIR_RIGHT)      s_next_dir = DIR_DOWN;
        else if (s_dir == DIR_DOWN)  s_next_dir = DIR_LEFT;
        else if (s_dir == DIR_LEFT)  s_next_dir = DIR_UP;
        else if (s_dir == DIR_UP)    s_next_dir = DIR_RIGHT;
        s_btn_override_timer = 0.6f;
        if (g_api->play_tone) g_api->play_tone(440, 10);
    }
}

void snake_teardown(void) {
    if (g_api) {
        if (g_api->set_led)   g_api->set_led(0, 0, 0);
        if (g_api->stop_tone) g_api->stop_tone();
    }
    save_high_score();
    g_api = NULL;
}

int snake_get_score(void)        { return s_score; }
int snake_get_high_score(void)   { return s_high_score; }
int snake_get_length(void)       { return s_length; }
bool snake_is_game_over(void)    { return s_game_over; }
