#include "invaders.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const QWatchAPI* g_api = NULL;

// Constants & Configuration
#define NUM_ALIEN_ROWS 3
#define NUM_ALIEN_COLS 6
#define TOTAL_ALIENS   (NUM_ALIEN_ROWS * NUM_ALIEN_COLS)
#define MAX_PLAYER_BULLETS 3
#define MAX_ALIEN_BULLETS  3
#define NUM_BUNKERS        3
#define BUNKERS_Y          44
#define HIGH_SCORE_PATH    "/apps/invaders.dat"

typedef enum {
    GAME_STATE_TITLE = 0,
    GAME_STATE_PLAYING,
    GAME_STATE_WAVE_CLEAR,
    GAME_STATE_GAME_OVER
} GameState;

typedef struct {
    float x;
    float y;
    bool  active;
} Bullet;

typedef struct {
    float x;
    float y;
    bool  alive;
    uint8_t type; // 0 = Spectre Boss (top), 1 = Drone (mid), 2 = Minion (bot)
} Alien;

typedef struct {
    int16_t x;
    int16_t y;
    int8_t  hp; // 4 to 0
} Bunker;

typedef struct {
    float x;
    float y;
    float speed;
    bool  active;
    int   points;
} UfoShip;

// Game State variables
static GameState s_state = GAME_STATE_TITLE;
static float     s_player_x = 64.0f;
static float     s_player_y = 57.0f;
static int       s_lives = 3;
static int       s_score = 0;
static int       s_high_score = 1000;
static int       s_wave = 1;
static int       s_aliens_alive = TOTAL_ALIENS;

// Unified sensor calibration tare and button override
static float     s_neutral_roll = 0.0f;
static bool      s_tare_initialized = false;
static float     s_btn_override_timer = 0.0f;

// Fleet movement & animation
static Alien   s_aliens[NUM_ALIEN_ROWS][NUM_ALIEN_COLS];
static float   s_fleet_dir = 1.0f; // +1 = right, -1 = left
static float   s_fleet_step_timer = 0.0f;
static float   s_fleet_step_interval = 0.45f;
static uint8_t s_anim_frame = 0;
static uint8_t s_step_tone_idx = 0;

// Bullets
static Bullet  s_player_bullets[MAX_PLAYER_BULLETS];
static Bullet  s_alien_bullets[MAX_ALIEN_BULLETS];
static float   s_alien_fire_timer = 0.0f;

// Bunkers
static Bunker  s_bunkers[NUM_BUNKERS];

// UFO
static UfoShip s_ufo;
static float   s_ufo_spawn_timer = 0.0f;

// Timers for states
static float   s_state_timer = 0.0f;

// Bitmaps for procedural rendering (8x6 pixels for aliens)
// Type 0 (Spectre Boss)
static const uint8_t s_alien_top_f0[6] = { 0x18, 0x3C, 0x7E, 0xDB, 0xFF, 0x24 };
static const uint8_t s_alien_top_f1[6] = { 0x18, 0x3C, 0x7E, 0xDB, 0xFF, 0x42 };

// Type 1 (Henchman Drone)
static const uint8_t s_alien_mid_f0[6] = { 0x42, 0x3C, 0x7E, 0xFF, 0xBD, 0xA5 };
static const uint8_t s_alien_mid_f1[6] = { 0x42, 0x3C, 0x7E, 0xFF, 0xBD, 0x5A };

// Type 2 (Foot Soldier)
static const uint8_t s_alien_bot_f0[6] = { 0x3C, 0x7E, 0xFF, 0x5A, 0xFF, 0x66 };
static const uint8_t s_alien_bot_f1[6] = { 0x3C, 0x7E, 0xFF, 0x5A, 0xFF, 0x99 };

// UFO bitmap (12x5)
static const uint8_t s_ufo_bmp[5] = { 0x0E, 0x1F, 0x3F, 0x1F, 0x0A };

// Forward declarations
static void init_wave(void);
static void load_high_score(void);
static void save_high_score(void);
static void draw_alien(int16_t x, int16_t y, uint8_t type, uint8_t frame);

// QApp Header descriptor
static const QAppHeader s_invaders_header = {
    .magic = QAPP_MAGIC,
    .api_version = QAPP_API_VERSION,
    .required_caps = (QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_MPU | QAPP_CAP_AUDIO | QAPP_CAP_RGB_LED | QAPP_CAP_STORAGE),
    .name = "007 Invaders",
    .version = "1.0.0",
    .author = "MI6 Cyber",
    .required_psram = 4096,
    .init = invaders_init,
    .update = invaders_update,
    .render = invaders_render,
    .on_button = invaders_on_button,
    .teardown = invaders_teardown
};

const QAppHeader* get_invaders_header(void) {
    return &s_invaders_header;
}

static void load_high_score(void) {
    if (!g_api || !g_api->file_read) return;
    int saved = 0;
    int res = g_api->file_read(HIGH_SCORE_PATH, &saved, sizeof(saved));
    if (res == (int)sizeof(saved) && saved > 0 && saved < 1000000) {
        s_high_score = saved;
    }
}

static void save_high_score(void) {
    if (!g_api || !g_api->file_write) return;
    g_api->file_write(HIGH_SCORE_PATH, &s_high_score, sizeof(s_high_score));
}

static void init_bunkers(void) {
    int16_t x_coords[NUM_BUNKERS] = { 24, 60, 96 };
    for (int i = 0; i < NUM_BUNKERS; i++) {
        s_bunkers[i].x = x_coords[i];
        s_bunkers[i].y = BUNKERS_Y;
        s_bunkers[i].hp = 4;
    }
}

static void init_wave(void) {
    s_aliens_alive = TOTAL_ALIENS;
    s_fleet_dir = 1.0f;
    s_fleet_step_timer = 0.0f;
    s_anim_frame = 0;
    s_step_tone_idx = 0;

    // Interval speeds up as wave number increases
    s_fleet_step_interval = 0.45f - (float)(s_wave - 1) * 0.05f;
    if (s_fleet_step_interval < 0.18f) s_fleet_step_interval = 0.18f;

    float start_y = 12.0f + (float)((s_wave - 1) % 4) * 2.0f;
    for (int r = 0; r < NUM_ALIEN_ROWS; r++) {
        for (int c = 0; c < NUM_ALIEN_COLS; c++) {
            s_aliens[r][c].x = 16.0f + (float)c * 16.0f;
            s_aliens[r][c].y = start_y + (float)r * 10.0f;
            s_aliens[r][c].alive = true;
            s_aliens[r][c].type = (uint8_t)r;
        }
    }

    // Clear bullets
    for (int i = 0; i < MAX_PLAYER_BULLETS; i++) s_player_bullets[i].active = false;
    for (int i = 0; i < MAX_ALIEN_BULLETS; i++)  s_alien_bullets[i].active = false;

    // Reset UFO
    s_ufo.active = false;
    s_ufo_spawn_timer = 12.0f;

    init_bunkers();
}

int invaders_init(const QWatchAPI* api) {
    if (!api) return QAPP_ERR_INVALID_PARAM;
    g_api = api;

    s_state = GAME_STATE_PLAYING;
    s_player_x = 64.0f;
    s_player_y = 57.0f;
    s_lives = 3;
    s_score = 0;
    s_wave = 1;
    s_alien_fire_timer = 1.5f;
    s_neutral_roll = 0.0f;
    s_tare_initialized = false;
    s_btn_override_timer = 0.0f;

    load_high_score();
    init_wave();

    if (g_api->set_led) {
        g_api->set_led(0, 40, 20); // Tactical cyan startup LED
    }
    if (g_api->play_tone) {
        g_api->play_tone(880, 80);
    }
    return QAPP_OK;
}

void invaders_fire_player_laser(void) {
    if (!g_api) return;
    for (int i = 0; i < MAX_PLAYER_BULLETS; i++) {
        if (!s_player_bullets[i].active) {
            s_player_bullets[i].x = s_player_x + 4.0f;
            s_player_bullets[i].y = s_player_y - 2.0f;
            s_player_bullets[i].active = true;

            if (g_api->play_tone) g_api->play_tone(1760, 25);
            if (g_api->set_led)   g_api->set_led(0, 80, 0); // Green laser muzzle flash
            break;
        }
    }
}

void invaders_update(float dt) {
    if (!g_api) return;
    if (dt <= 0.0f || dt > 0.2f) dt = 0.016f;

    // Decrement button override timer
    if (s_btn_override_timer > 0.0f) {
        s_btn_override_timer -= dt;
    }

    // Continuous button hold steering via live button state
    if (g_api && g_api->get_button_state && s_state == GAME_STATE_PLAYING) {
        uint8_t btn_mask = g_api->get_button_state();
        if (btn_mask & QBTN_UP) {
            s_player_x -= 55.0f * dt;
            if (s_player_x < 4.0f) s_player_x = 4.0f;
            s_btn_override_timer = 0.35f;
        }
        if (btn_mask & QBTN_DOWN) {
            s_player_x += 55.0f * dt;
            if (s_player_x > 116.0f) s_player_x = 116.0f;
            s_btn_override_timer = 0.35f;
        }
    }

    // Poll Gyro / IMU tilt for smooth analog steering (uses unified sensor calibration)
    QTelemetry telem;
    memset(&telem, 0, sizeof(telem));
    g_api->get_telemetry(&telem);

    if (!s_tare_initialized) {
        s_neutral_roll = telem.roll;
        s_tare_initialized = true;
    }

    // Only apply tilt steering if buttons aren't actively steering
    if (s_btn_override_timer <= 0.0f && s_state == GAME_STATE_PLAYING) {
        float effective_roll = telem.roll - s_neutral_roll;
        const float DEADBAND = 14.0f;
        if (fabsf(effective_roll) > DEADBAND) {
            float tilt_delta = (effective_roll > 0.0f) ? (effective_roll - DEADBAND) : (effective_roll + DEADBAND);
            float tilt_speed = tilt_delta * 1.5f;
            if (tilt_speed > 65.0f)  tilt_speed = 65.0f;
            if (tilt_speed < -65.0f) tilt_speed = -65.0f;
            s_player_x += tilt_speed * dt;
            if (s_player_x < 4.0f)   s_player_x = 4.0f;
            if (s_player_x > 116.0f) s_player_x = 116.0f;
        }
    }

    if (s_state == GAME_STATE_WAVE_CLEAR) {
        s_state_timer += dt;
        if (s_state_timer > 2.0f) {
            s_wave++;
            init_wave();
            s_state = GAME_STATE_PLAYING;
            s_state_timer = 0.0f;
        }
        return;
    }

    if (s_state == GAME_STATE_GAME_OVER) {
        s_state_timer += dt;
        return;
    }

    if (s_state != GAME_STATE_PLAYING) return;

    // 1. Move Player Lasers
    for (int i = 0; i < MAX_PLAYER_BULLETS; i++) {
        if (s_player_bullets[i].active) {
            s_player_bullets[i].y -= 95.0f * dt;
            if (s_player_bullets[i].y < 2.0f) {
                s_player_bullets[i].active = false;
            }
        }
    }

    // 2. Move Alien Bombs
    for (int i = 0; i < MAX_ALIEN_BULLETS; i++) {
        if (s_alien_bullets[i].active) {
            s_alien_bullets[i].y += 50.0f * dt;
            if (s_alien_bullets[i].y > 62.0f) {
                s_alien_bullets[i].active = false;
            }
        }
    }

    // 3. Alien Fleet Stepping
    s_fleet_step_timer += dt;
    // Step interval accelerates proportionally as alien count diminishes
    float dynamic_interval = s_fleet_step_interval * ((float)s_aliens_alive / (float)TOTAL_ALIENS * 0.75f + 0.25f);
    if (dynamic_interval < 0.06f) dynamic_interval = 0.06f;

    if (s_fleet_step_timer >= dynamic_interval) {
        s_fleet_step_timer = 0.0f;
        s_anim_frame ^= 1;

        // Determine if fleet hits left or right screen boundary
        bool hit_wall = false;
        for (int r = 0; r < NUM_ALIEN_ROWS; r++) {
            for (int c = 0; c < NUM_ALIEN_COLS; c++) {
                if (s_aliens[r][c].alive) {
                    float next_x = s_aliens[r][c].x + (s_fleet_dir * 3.0f);
                    if (next_x <= 4.0f || next_x >= 116.0f) {
                        hit_wall = true;
                        break;
                    }
                }
            }
            if (hit_wall) break;
        }

        // Step cadence tones: classic 4-note heartbeat
        static const uint16_t cadence_tones[4] = { 160, 140, 125, 110 };
        if (g_api->play_tone) {
            g_api->play_tone(cadence_tones[s_step_tone_idx % 4], 20);
            s_step_tone_idx++;
        }

        if (hit_wall) {
            s_fleet_dir = -s_fleet_dir;
            for (int r = 0; r < NUM_ALIEN_ROWS; r++) {
                for (int c = 0; c < NUM_ALIEN_COLS; c++) {
                    s_aliens[r][c].y += 3.5f;
                    // Check invasion breach (aliens landed on player row)
                    if (s_aliens[r][c].alive && s_aliens[r][c].y >= s_player_y - 4.0f) {
                        s_lives = 0;
                        s_state = GAME_STATE_GAME_OVER;
                        s_state_timer = 0.0f;
                        if (g_api->play_tone) g_api->play_tone(90, 400);
                        if (g_api->set_led)   g_api->set_led(120, 0, 0);
                        save_high_score();
                        return;
                    }
                }
            }
        } else {
            for (int r = 0; r < NUM_ALIEN_ROWS; r++) {
                for (int c = 0; c < NUM_ALIEN_COLS; c++) {
                    s_aliens[r][c].x += (s_fleet_dir * 3.0f);
                }
            }
        }
    }

    // 4. Alien Bomb Drops
    s_alien_fire_timer -= dt;
    if (s_alien_fire_timer <= 0.0f) {
        s_alien_fire_timer = 0.8f + (float)(g_api->random_range ? g_api->random_range(0, 100) : 50) * 0.012f;

        // Pick an alive alien in lowest row
        int candidates[NUM_ALIEN_COLS];
        int count = 0;
        for (int c = 0; c < NUM_ALIEN_COLS; c++) {
            for (int r = NUM_ALIEN_ROWS - 1; r >= 0; r--) {
                if (s_aliens[r][c].alive) {
                    candidates[count++] = (r << 8) | c;
                    break;
                }
            }
        }

        if (count > 0) {
            uint32_t choice = g_api->random_range ? g_api->random_range(0, count - 1) : 0;
            int r = (candidates[choice] >> 8) & 0xFF;
            int c = candidates[choice] & 0xFF;

            for (int i = 0; i < MAX_ALIEN_BULLETS; i++) {
                if (!s_alien_bullets[i].active) {
                    s_alien_bullets[i].x = s_aliens[r][c].x + 4.0f;
                    s_alien_bullets[i].y = s_aliens[r][c].y + 7.0f;
                    s_alien_bullets[i].active = true;
                    break;
                }
            }
        }
    }

    // 5. UFO Mystery Ship Logic
    if (!s_ufo.active) {
        s_ufo_spawn_timer -= dt;
        if (s_ufo_spawn_timer <= 0.0f) {
            s_ufo.active = true;
            s_ufo.x = -14.0f;
            s_ufo.y = 8.0f;
            s_ufo.speed = 35.0f;
            s_ufo.points = (g_api->random_range ? (int)g_api->random_range(1, 3) : 2) * 50;
            s_ufo_spawn_timer = 20.0f + (float)(g_api->random_range ? g_api->random_range(0, 15) : 8);
            if (g_api->set_led) g_api->set_led(80, 50, 0); // Amber warning
        }
    } else {
        s_ufo.x += s_ufo.speed * dt;
        if (s_ufo.x > 132.0f) {
            s_ufo.active = false;
        }
    }

    // 6. Collision: Player Laser vs Aliens
    for (int i = 0; i < MAX_PLAYER_BULLETS; i++) {
        if (!s_player_bullets[i].active) continue;

        float bx = s_player_bullets[i].x;
        float by = s_player_bullets[i].y;

        // Check UFO
        if (s_ufo.active && bx >= s_ufo.x && bx <= s_ufo.x + 12.0f && by >= s_ufo.y && by <= s_ufo.y + 6.0f) {
            s_player_bullets[i].active = false;
            s_ufo.active = false;
            s_score += s_ufo.points;
            if (s_score > s_high_score) {
                s_high_score = s_score;
                save_high_score();
            }
            if (g_api->play_tone) g_api->play_tone(1500, 120);
            if (g_api->set_led)   g_api->set_led(100, 0, 100); // Purple UFO flash
            continue;
        }

        // Check Aliens
        bool hit = false;
        for (int r = 0; r < NUM_ALIEN_ROWS; r++) {
            for (int c = 0; c < NUM_ALIEN_COLS; c++) {
                if (!s_aliens[r][c].alive) continue;

                float ax = s_aliens[r][c].x;
                float ay = s_aliens[r][c].y;

                if (bx >= ax && bx <= ax + 8.0f && by >= ay && by <= ay + 7.0f) {
                    s_aliens[r][c].alive = false;
                    s_player_bullets[i].active = false;
                    hit = true;
                    s_aliens_alive--;

                    int pts = (r == 0) ? 30 : ((r == 1) ? 20 : 10);
                    s_score += pts;
                    if (s_score > s_high_score) {
                        s_high_score = s_score;
                        save_high_score();
                    }

                    if (g_api->play_tone) g_api->play_tone(320, 35);
                    if (g_api->set_led)   g_api->set_led(0, 0, 80); // Blue alien explosion

                    // Check wave clear
                    if (s_aliens_alive <= 0) {
                        s_state = GAME_STATE_WAVE_CLEAR;
                        s_state_timer = 0.0f;
                        if (g_api->play_tone) g_api->play_tone(1046, 180);
                        if (g_api->set_led)   g_api->set_led(0, 100, 50); // Victory emerald
                    }
                    break;
                }
            }
            if (hit) break;
        }

        // Check Bunker Hit by Player Laser
        if (!hit && s_player_bullets[i].active) {
            for (int b = 0; b < NUM_BUNKERS; b++) {
                if (s_bunkers[b].hp <= 0) continue;
                if (bx >= (float)s_bunkers[b].x && bx <= (float)(s_bunkers[b].x + 12) &&
                    by >= (float)s_bunkers[b].y && by <= (float)(s_bunkers[b].y + 6)) {
                    s_player_bullets[i].active = false;
                    s_bunkers[b].hp--;
                    if (g_api->play_tone) g_api->play_tone(220, 20);
                    break;
                }
            }
        }
    }

    // 7. Collision: Alien Bomb vs Bunkers & Player
    for (int i = 0; i < MAX_ALIEN_BULLETS; i++) {
        if (!s_alien_bullets[i].active) continue;

        float bx = s_alien_bullets[i].x;
        float by = s_alien_bullets[i].y;

        // Bunkers
        bool bunker_hit = false;
        for (int b = 0; b < NUM_BUNKERS; b++) {
            if (s_bunkers[b].hp <= 0) continue;
            if (bx >= (float)s_bunkers[b].x && bx <= (float)(s_bunkers[b].x + 12) &&
                by >= (float)s_bunkers[b].y && by <= (float)(s_bunkers[b].y + 6)) {
                s_alien_bullets[i].active = false;
                s_bunkers[b].hp--;
                bunker_hit = true;
                if (g_api->play_tone) g_api->play_tone(200, 20);
                break;
            }
        }
        if (bunker_hit) continue;

        // Player Hit
        if (bx >= s_player_x && bx <= s_player_x + 9.0f &&
            by >= s_player_y && by <= s_player_y + 5.0f) {
            s_alien_bullets[i].active = false;
            s_lives--;
            if (g_api->play_tone) g_api->play_tone(110, 150);
            if (g_api->set_led)   g_api->set_led(120, 0, 0); // Red damage flash

            if (s_lives <= 0) {
                s_state = GAME_STATE_GAME_OVER;
                s_state_timer = 0.0f;
                save_high_score();
            }
        }
    }
}

static void draw_alien(int16_t x, int16_t y, uint8_t type, uint8_t frame) {
    if (!g_api) return;
    const uint8_t* rows = NULL;
    if (type == 0)      rows = (frame == 0) ? s_alien_top_f0 : s_alien_top_f1;
    else if (type == 1) rows = (frame == 0) ? s_alien_mid_f0 : s_alien_mid_f1;
    else                rows = (frame == 0) ? s_alien_bot_f0 : s_alien_bot_f1;

    for (int r = 0; r < 6; r++) {
        uint8_t byte = rows[r];
        for (int c = 0; c < 8; c++) {
            if (byte & (0x80 >> c)) {
                g_api->draw_pixel(x + c, y + r, 1);
            }
        }
    }
}

void invaders_render(void) {
    if (!g_api) return;
    g_api->clear_screen();

    // 1. Top HUD Bar (Score, High Score, Wave)
    char hud_buf[32];
    snprintf(hud_buf, sizeof(hud_buf), "%04d HI:%04d W%d", s_score, s_high_score, s_wave);
    g_api->draw_string(2, 0, hud_buf, 0);

    // 2. Draw UFO if active
    if (s_ufo.active) {
        int16_t ux = (int16_t)s_ufo.x;
        int16_t uy = (int16_t)s_ufo.y;
        for (int r = 0; r < 5; r++) {
            uint8_t bits = s_ufo_bmp[r];
            for (int c = 0; c < 8; c++) {
                if (bits & (0x80 >> c)) g_api->draw_pixel(ux + c, uy + r, 1);
            }
        }
    }

    // 3. Draw Aliens
    for (int r = 0; r < NUM_ALIEN_ROWS; r++) {
        for (int c = 0; c < NUM_ALIEN_COLS; c++) {
            if (s_aliens[r][c].alive) {
                draw_alien((int16_t)s_aliens[r][c].x, (int16_t)s_aliens[r][c].y, s_aliens[r][c].type, s_anim_frame);
            }
        }
    }

    // 4. Draw Destructible Bunkers
    for (int b = 0; b < NUM_BUNKERS; b++) {
        if (s_bunkers[b].hp <= 0) continue;
        int16_t bx = s_bunkers[b].x;
        int16_t by = s_bunkers[b].y;
        int8_t  hp = s_bunkers[b].hp;

        // Draw bunker shape based on remaining HP
        if (hp >= 4) {
            g_api->draw_rect(bx, by, 12, 5, 1, true);
            g_api->draw_rect(bx + 4, by + 3, 4, 3, 0, true); // Inner archway notch
        } else if (hp == 3) {
            g_api->draw_rect(bx + 1, by, 10, 4, 1, true);
            g_api->draw_rect(bx + 4, by + 2, 4, 3, 0, true);
        } else if (hp == 2) {
            g_api->draw_rect(bx + 2, by + 1, 8, 3, 1, false);
        } else {
            g_api->draw_pixel(bx + 3, by + 2, 1);
            g_api->draw_pixel(bx + 5, by + 1, 1);
            g_api->draw_pixel(bx + 8, by + 2, 1);
        }
    }

    // 5. Draw Bullets
    for (int i = 0; i < MAX_PLAYER_BULLETS; i++) {
        if (s_player_bullets[i].active) {
            int16_t px = (int16_t)s_player_bullets[i].x;
            int16_t py = (int16_t)s_player_bullets[i].y;
            g_api->draw_line(px, py, px, py + 2, 1);
        }
    }
    for (int i = 0; i < MAX_ALIEN_BULLETS; i++) {
        if (s_alien_bullets[i].active) {
            int16_t ax = (int16_t)s_alien_bullets[i].x;
            int16_t ay = (int16_t)s_alien_bullets[i].y;
            g_api->draw_pixel(ax, ay, 1);
            g_api->draw_pixel(ax + ((ay & 2) ? 1 : -1), ay + 1, 1); // Zigzag bomb
            g_api->draw_pixel(ax, ay + 2, 1);
        }
    }

    // 6. Draw Player Cannon Ship (9x5)
    int16_t sx = (int16_t)s_player_x;
    int16_t sy = (int16_t)s_player_y;
    // Base
    g_api->draw_rect(sx, sy + 2, 9, 3, 1, true);
    // Turret
    g_api->draw_rect(sx + 3, sy + 1, 3, 2, 1, true);
    // Muzzle tip
    g_api->draw_pixel(sx + 4, sy, 1);

    // 7. Bottom Shield / Lives Telemetry
    for (int l = 0; l < s_lives; l++) {
        int16_t lx = 100 + l * 9;
        g_api->draw_rect(lx, 60, 6, 3, 1, true);
        g_api->draw_pixel(lx + 2, 59, 1);
    }

    // 8. State Overlays
    if (s_state == GAME_STATE_WAVE_CLEAR) {
        g_api->draw_rect(24, 24, 80, 16, 0, true);
        g_api->draw_rect(24, 24, 80, 16, 1, false);
        g_api->draw_string(28, 28, "WAVE CLEARED!", 0);
    } else if (s_state == GAME_STATE_GAME_OVER) {
        g_api->draw_rect(24, 20, 80, 24, 0, true);
        g_api->draw_rect(24, 20, 80, 24, 1, false);
        g_api->draw_string(36, 24, "GAME OVER", 0);
        g_api->draw_string(28, 33, "[OK] TO RESTART", 0);
    }

    g_api->flush_display();
}

void invaders_on_button(uint8_t btn, QButtonEvent evt) {
    if (evt == QEVT_BTN_DOWN || evt == QEVT_BTN_SHORT_CLICK) {
        if (s_state == GAME_STATE_PLAYING) {
            if (btn == QBTN_UP) {
                // Move Left
                s_player_x -= 8.0f;
                if (s_player_x < 4.0f) s_player_x = 4.0f;
                s_btn_override_timer = 0.35f;
            } else if (btn == QBTN_DOWN) {
                // Move Right
                s_player_x += 8.0f;
                if (s_player_x > 116.0f) s_player_x = 116.0f;
                s_btn_override_timer = 0.35f;
            } else if (btn == QBTN_OK) {
                // Fire
                invaders_fire_player_laser();
            } else if (btn == QBTN_CANCEL) {
                if (g_api && g_api->exit_app) g_api->exit_app();
            }
        } else if (s_state == GAME_STATE_GAME_OVER) {
            if (btn == QBTN_OK) {
                invaders_init(g_api);
            } else if (btn == QBTN_CANCEL) {
                if (g_api && g_api->exit_app) g_api->exit_app();
            }
        }
    }
}

void invaders_teardown(void) {
    if (g_api) {
        if (g_api->set_led)   g_api->set_led(0, 0, 0);
        if (g_api->stop_tone) g_api->stop_tone();
    }
    save_high_score();
    g_api = NULL;
}

// Unit test inspection helpers
int   invaders_get_score(void)        { return s_score; }
int   invaders_get_high_score(void)   { return s_high_score; }
int   invaders_get_lives(void)        { return s_lives; }
int   invaders_get_wave(void)         { return s_wave; }
int   invaders_get_aliens_alive(void) { return s_aliens_alive; }
float invaders_get_player_x(void)     { return s_player_x; }
void  invaders_reset_game(void)       { if (g_api) invaders_init(g_api); }
