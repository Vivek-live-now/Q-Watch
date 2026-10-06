#include "bounce.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const QWatchAPI* g_api = NULL;

#define BOUNCE_SAVE_PATH "/apps/bounce.dat"
#define MAX_PLATFORMS    16
#define MAX_SPIKES       12
#define MAX_RINGS        10
#define LEVEL_WIDTH      380.0f

typedef enum {
    BOUNCE_STATE_TITLE = 0,
    BOUNCE_STATE_PLAYING,
    BOUNCE_STATE_LEVEL_CLEAR,
    BOUNCE_STATE_GAME_OVER
} BounceState;

typedef struct {
    float x;
    float y;
    float w;
    float h;
} Platform;

typedef struct {
    float x;
    float y;
    bool  is_ceiling;
} Spike;

typedef struct {
    float x;
    float y;
    bool  collected;
} Ring;

// Game State
static BounceState s_state = BOUNCE_STATE_TITLE;
static float s_ball_x = 24.0f;
static float s_ball_y = 48.0f;
static float s_ball_vx = 0.0f;
static float s_ball_vy = 0.0f;
static float s_radius = 4.0f;
static bool  s_super_bouncy = false;
static int   s_lives = 3;
static int   s_score = 0;
static int   s_high_score = 0;
static int   s_level = 1;
static int   s_rings_left = 0;
static float s_cam_x = 0.0f;
static float s_state_timer = 0.0f;
static float s_respawn_timer = 0.0f;

// Checkpoint
static float s_chk_x = 24.0f;
static float s_chk_y = 48.0f;

// Exit Goal
static float s_goal_x = 350.0f;
static float s_goal_y = 44.0f;
static bool  s_goal_open = false;

// Controls & calibration
static float s_neutral_roll = 0.0f;
static bool  s_tare_done = false;
static float s_btn_override_timer = 0.0f;

// Level Geometry
static Platform s_platforms[MAX_PLATFORMS];
static int      s_num_platforms = 0;
static Spike    s_spikes[MAX_SPIKES];
static int      s_num_spikes = 0;
static Ring     s_rings[MAX_RINGS];
static int      s_num_rings = 0;

// Forward declarations
static void load_level(int lvl);
static void load_high_score(void);
static void save_high_score(void);
static void respawn_ball(void);

// QApp Header descriptor
static const QAppHeader s_bounce_header = {
    .magic = QAPP_MAGIC,
    .api_version = QAPP_API_VERSION,
    .required_caps = (QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_MPU | QAPP_CAP_AUDIO | QAPP_CAP_RGB_LED | QAPP_CAP_STORAGE),
    .name = "Nokia Bounce",
    .version = "1.0.0",
    .author = "MI6 Cyber",
    .required_psram = 2048,
    .init = bounce_init,
    .update = bounce_update,
    .render = bounce_render,
    .on_button = bounce_on_button,
    .teardown = bounce_teardown
};

const QAppHeader* get_bounce_header(void) {
    return &s_bounce_header;
}

static void load_high_score(void) {
    if (!g_api || !g_api->file_read) return;
    int saved = 0;
    int res = g_api->file_read(BOUNCE_SAVE_PATH, &saved, sizeof(saved));
    if (res == (int)sizeof(saved) && saved >= 0 && saved < 10000000) {
        s_high_score = saved;
    }
}

static void save_high_score(void) {
    if (!g_api || !g_api->file_write) return;
    if (s_score > s_high_score) s_high_score = s_score;
    g_api->file_write(BOUNCE_SAVE_PATH, &s_high_score, sizeof(s_high_score));
}

static void load_level(int lvl) {
    s_num_platforms = 0;
    s_num_spikes = 0;
    s_num_rings = 0;
    s_rings_left = 0;
    s_goal_open = false;

    // Default continuous ground floor
    s_platforms[s_num_platforms++] = (Platform){ 0.0f, 56.0f, LEVEL_WIDTH, 8.0f };

    if (lvl == 1) {
        // --- LEVEL 1: Green Meadows ---
        s_chk_x = 24.0f; s_chk_y = 48.0f;
        s_goal_x = 350.0f; s_goal_y = 44.0f;

        // Steps & platforms
        s_platforms[s_num_platforms++] = (Platform){ 70.0f, 44.0f, 40.0f, 6.0f };
        s_platforms[s_num_platforms++] = (Platform){ 130.0f, 32.0f, 45.0f, 6.0f };
        s_platforms[s_num_platforms++] = (Platform){ 195.0f, 44.0f, 40.0f, 6.0f };
        s_platforms[s_num_platforms++] = (Platform){ 255.0f, 34.0f, 50.0f, 6.0f };

        // Ground Spikes
        s_spikes[s_num_spikes++] = (Spike){ 118.0f, 56.0f, false };
        s_spikes[s_num_spikes++] = (Spike){ 182.0f, 56.0f, false };
        s_spikes[s_num_spikes++] = (Spike){ 242.0f, 56.0f, false };

        // Gold Rings
        s_rings[s_num_rings++] = (Ring){ 48.0f, 42.0f, false };
        s_rings[s_num_rings++] = (Ring){ 90.0f, 34.0f, false };
        s_rings[s_num_rings++] = (Ring){ 150.0f, 22.0f, false };
        s_rings[s_num_rings++] = (Ring){ 215.0f, 34.0f, false };
        s_rings[s_num_rings++] = (Ring){ 280.0f, 24.0f, false };
    } else if (lvl == 2) {
        // --- LEVEL 2: Spike Cavern ---
        s_chk_x = 20.0f; s_chk_y = 48.0f;
        s_goal_x = 355.0f; s_goal_y = 44.0f;

        // Low ceiling
        s_platforms[s_num_platforms++] = (Platform){ 0.0f, 8.0f, LEVEL_WIDTH, 4.0f };

        // Elevated walkways
        s_platforms[s_num_platforms++] = (Platform){ 60.0f, 42.0f, 36.0f, 6.0f };
        s_platforms[s_num_platforms++] = (Platform){ 115.0f, 30.0f, 36.0f, 6.0f };
        s_platforms[s_num_platforms++] = (Platform){ 170.0f, 42.0f, 36.0f, 6.0f };
        s_platforms[s_num_platforms++] = (Platform){ 225.0f, 28.0f, 40.0f, 6.0f };
        s_platforms[s_num_platforms++] = (Platform){ 285.0f, 40.0f, 45.0f, 6.0f };

        // Ground & Ceiling Spikes
        s_spikes[s_num_spikes++] = (Spike){ 102.0f, 56.0f, false };
        s_spikes[s_num_spikes++] = (Spike){ 158.0f, 56.0f, false };
        s_spikes[s_num_spikes++] = (Spike){ 212.0f, 56.0f, false };
        s_spikes[s_num_spikes++] = (Spike){ 132.0f, 12.0f, true }; // Ceiling spike!
        s_spikes[s_num_spikes++] = (Spike){ 245.0f, 12.0f, true };

        // Gold Rings
        s_rings[s_num_rings++] = (Ring){ 40.0f, 44.0f, false };
        s_rings[s_num_rings++] = (Ring){ 78.0f, 32.0f, false };
        s_rings[s_num_rings++] = (Ring){ 133.0f, 22.0f, false };
        s_rings[s_num_rings++] = (Ring){ 188.0f, 32.0f, false };
        s_rings[s_num_rings++] = (Ring){ 245.0f, 20.0f, false };
        s_rings[s_num_rings++] = (Ring){ 305.0f, 30.0f, false };
    } else {
        // --- LEVEL 3: Sky Citadel ---
        s_chk_x = 20.0f; s_chk_y = 48.0f;
        s_goal_x = 360.0f; s_goal_y = 28.0f;

        // Floating islands
        s_platforms[s_num_platforms++] = (Platform){ 50.0f, 46.0f, 30.0f, 6.0f };
        s_platforms[s_num_platforms++] = (Platform){ 100.0f, 36.0f, 30.0f, 6.0f };
        s_platforms[s_num_platforms++] = (Platform){ 150.0f, 24.0f, 32.0f, 6.0f };
        s_platforms[s_num_platforms++] = (Platform){ 200.0f, 34.0f, 30.0f, 6.0f };
        s_platforms[s_num_platforms++] = (Platform){ 250.0f, 22.0f, 32.0f, 6.0f };
        s_platforms[s_num_platforms++] = (Platform){ 300.0f, 34.0f, 30.0f, 6.0f };
        s_platforms[s_num_platforms++] = (Platform){ 340.0f, 38.0f, 38.0f, 6.0f };

        // Floor Spikes field
        s_spikes[s_num_spikes++] = (Spike){ 85.0f, 56.0f, false };
        s_spikes[s_num_spikes++] = (Spike){ 135.0f, 56.0f, false };
        s_spikes[s_num_spikes++] = (Spike){ 185.0f, 56.0f, false };
        s_spikes[s_num_spikes++] = (Spike){ 235.0f, 56.0f, false };
        s_spikes[s_num_spikes++] = (Spike){ 285.0f, 56.0f, false };

        // Rings
        s_rings[s_num_rings++] = (Ring){ 35.0f, 42.0f, false };
        s_rings[s_num_rings++] = (Ring){ 65.0f, 36.0f, false };
        s_rings[s_num_rings++] = (Ring){ 115.0f, 26.0f, false };
        s_rings[s_num_rings++] = (Ring){ 166.0f, 15.0f, false };
        s_rings[s_num_rings++] = (Ring){ 215.0f, 24.0f, false };
        s_rings[s_num_rings++] = (Ring){ 266.0f, 14.0f, false };
        s_rings[s_num_rings++] = (Ring){ 315.0f, 24.0f, false };
    }

    s_rings_left = s_num_rings;
    respawn_ball();
}

static void respawn_ball(void) {
    s_ball_x = s_chk_x;
    s_ball_y = s_chk_y;
    s_ball_vx = 0.0f;
    s_ball_vy = 0.0f;
    s_cam_x = s_ball_x - 30.0f;
    if (s_cam_x < 0.0f) s_cam_x = 0.0f;
    s_respawn_timer = 0.8f;
}

int bounce_init(const QWatchAPI* api) {
    if (!api) return QAPP_ERR_INVALID_PARAM;
    g_api = api;

    s_state = BOUNCE_STATE_TITLE;
    s_lives = 3;
    s_score = 0;
    s_level = 1;
    s_radius = 4.0f;
    s_super_bouncy = false;
    s_state_timer = 0.0f;
    s_tare_done = false;
    s_btn_override_timer = 0.0f;

    load_high_score();
    load_level(1);

    if (g_api->set_led) {
        g_api->set_led(80, 15, 0); // Iconic Nokia Bounce orange-red
    }
    return QAPP_OK;
}

void bounce_update(float dt) {
    if (!g_api) return;
    if (dt <= 0.0f || dt > 0.2f) dt = 0.016f;

    if (s_state == BOUNCE_STATE_TITLE) {
        s_state_timer += dt;
        return;
    }

    if (s_state == BOUNCE_STATE_LEVEL_CLEAR) {
        s_state_timer += dt;
        if (s_state_timer > 2.5f) {
            s_level++;
            if (s_level > 3) s_level = 1;
            load_level(s_level);
            s_state = BOUNCE_STATE_PLAYING;
            s_state_timer = 0.0f;
        }
        return;
    }

    if (s_state == BOUNCE_STATE_GAME_OVER) {
        s_state_timer += dt;
        return;
    }

    if (s_respawn_timer > 0.0f) s_respawn_timer -= dt;
    if (s_btn_override_timer > 0.0f) s_btn_override_timer -= dt;

    // 1. Controls: Roll horizontal via Button hold (UP=Left, DOWN=Right)
    if (g_api->get_button_state) {
        uint8_t btn_mask = g_api->get_button_state();
        if (btn_mask & QBTN_UP) {
            s_ball_vx -= 110.0f * dt;
            s_btn_override_timer = 0.35f;
        }
        if (btn_mask & QBTN_DOWN) {
            s_ball_vx += 110.0f * dt;
            s_btn_override_timer = 0.35f;
        }
    }

    // 2. Analog Roll IMU Tilt Steering with tare & deadband
    QTelemetry telem;
    memset(&telem, 0, sizeof(telem));
    g_api->get_telemetry(&telem);

    if (!s_tare_done) {
        s_neutral_roll = telem.roll;
        s_tare_done = true;
    }

    if (s_btn_override_timer <= 0.0f) {
        float diff_roll = telem.roll - s_neutral_roll;
        const float DEADBAND = 6.0f;
        if (fabsf(diff_roll) > DEADBAND) {
            float roll_dir = (diff_roll > 0.0f) ? (diff_roll - DEADBAND) : (diff_roll + DEADBAND);
            s_ball_vx += roll_dir * 3.5f * dt;
        }
    }

    // Drag & max speed clamp
    s_ball_vx *= 0.94f;
    if (s_ball_vx > 65.0f)  s_ball_vx = 65.0f;
    if (s_ball_vx < -65.0f) s_ball_vx = -65.0f;

    // 3. Gravity & vertical physics
    const float GRAVITY = 145.0f;
    s_ball_vy += GRAVITY * dt;
    if (s_ball_vy > 120.0f) s_ball_vy = 120.0f;

    // Apply velocities
    s_ball_x += s_ball_vx * dt;
    s_ball_y += s_ball_vy * dt;

    // World horizontal clamps
    if (s_ball_x < s_radius) {
        s_ball_x = s_radius;
        s_ball_vx = 0.0f;
    }
    if (s_ball_x > LEVEL_WIDTH - s_radius) {
        s_ball_x = LEVEL_WIDTH - s_radius;
        s_ball_vx = 0.0f;
    }

    // 4. Platform Collisions (Natural Bouncing)
    for (int p = 0; p < s_num_platforms; p++) {
        const Platform* plat = &s_platforms[p];
        if (s_ball_x >= (plat->x - s_radius) && s_ball_x <= (plat->x + plat->w + s_radius)) {
            // Landing on top of platform
            if (s_ball_y + s_radius >= plat->y && (s_ball_y - s_radius) < (plat->y + 3.0f) && s_ball_vy >= 0.0f) {
                s_ball_y = plat->y - s_radius;
                // Bounce restitution
                float bounce_fac = s_super_bouncy ? 0.92f : 0.76f;
                s_ball_vy = -s_ball_vy * bounce_fac;

                if (fabsf(s_ball_vy) > 18.0f) {
                    if (g_api->play_tone) g_api->play_tone(380, 10);
                } else if (fabsf(s_ball_vy) < 6.0f) {
                    s_ball_vy = 0.0f;
                }
            }
            // Hitting bottom of ceiling platform
            else if (s_ball_y - s_radius <= (plat->y + plat->h) && (s_ball_y + s_radius) > (plat->y + plat->h) && s_ball_vy < 0.0f) {
                s_ball_y = plat->y + plat->h + s_radius;
                s_ball_vy = -s_ball_vy * 0.5f;
            }
        }
    }

    // 5. Check Spikes Collision
    if (s_respawn_timer <= 0.0f) {
        for (int s = 0; s < s_num_spikes; s++) {
            float sx = s_spikes[s].x;
            float sy = s_spikes[s].y;
            float dist = sqrtf((s_ball_x - sx)*(s_ball_x - sx) + (s_ball_y - sy)*(s_ball_y - sy));
            if (dist < (s_radius + 4.0f)) {
                // POP!
                s_lives--;
                if (g_api->play_tone) g_api->play_tone(180, 90);
                if (g_api->set_led)   g_api->set_led(90, 0, 0); // Red pop flash
                if (s_lives <= 0) {
                    s_state = BOUNCE_STATE_GAME_OVER;
                    save_high_score();
                } else {
                    respawn_ball();
                }
                break;
            }
        }
    }

    // 6. Ring Collection Check
    for (int r = 0; r < s_num_rings; r++) {
        if (!s_rings[r].collected) {
            float dist = sqrtf((s_ball_x - s_rings[r].x)*(s_ball_x - s_rings[r].x) +
                               (s_ball_y - s_rings[r].y)*(s_ball_y - s_rings[r].y));
            if (dist < (s_radius + 5.0f)) {
                s_rings[r].collected = true;
                s_rings_left--;
                s_score += 100;
                if (g_api->play_tone) g_api->play_tone(880, 35);
                if (g_api->set_led)   g_api->set_led(60, 60, 0); // Gold flash

                if (s_rings_left <= 0) {
                    s_goal_open = true;
                    if (g_api->play_tone) g_api->play_tone(1046, 75);
                }
            }
        }
    }

    // 7. Exit Goal Check
    if (s_goal_open) {
        float dist_goal = sqrtf((s_ball_x - s_goal_x)*(s_ball_x - s_goal_x) +
                                (s_ball_y - s_goal_y)*(s_ball_y - s_goal_y));
        if (dist_goal < 10.0f) {
            // Level Cleared!
            s_score += 500;
            s_state = BOUNCE_STATE_LEVEL_CLEAR;
            s_state_timer = 0.0f;
            save_high_score();
            if (g_api->play_tone) g_api->play_tone(1318, 150);
            if (g_api->set_led)   g_api->set_led(0, 90, 20); // Emerald win
        }
    }

    // 8. Smooth Camera Tracking
    float target_cam = s_ball_x - 56.0f;
    if (target_cam < 0.0f) target_cam = 0.0f;
    if (target_cam > LEVEL_WIDTH - 128.0f) target_cam = LEVEL_WIDTH - 128.0f;
    s_cam_x += (target_cam - s_cam_x) * 0.15f;
}

void bounce_render(void) {
    if (!g_api) return;

    g_api->clear_screen();

    int16_t cam = (int16_t)roundf(s_cam_x);

    // 1. Top HUD (y=0 to 8)
    g_api->draw_line(0, 9, 127, 9, 1);
    char buf[20];
    snprintf(buf, sizeof(buf), "LV%d %d", s_level, s_score);
    g_api->draw_string(2, 1, buf, 0);

    // Lives icons
    for (int l = 0; l < s_lives; l++) {
        g_api->draw_circle(72 + l * 8, 5, 2, 1, true);
    }

    // Rings indicator
    snprintf(buf, sizeof(buf), "O:%d", s_rings_left);
    g_api->draw_string(100, 1, buf, 0);

    // 2. Draw Platforms
    for (int p = 0; p < s_num_platforms; p++) {
        int16_t px = (int16_t)s_platforms[p].x - cam;
        int16_t py = (int16_t)s_platforms[p].y;
        int16_t pw = (int16_t)s_platforms[p].w;
        int16_t ph = (int16_t)s_platforms[p].h;

        if (px + pw >= 0 && px < 128) {
            g_api->draw_rect(px, py, pw, ph, 1, true);
        }
    }

    // 3. Draw Spikes
    for (int s = 0; s < s_num_spikes; s++) {
        int16_t sx = (int16_t)s_spikes[s].x - cam;
        int16_t sy = (int16_t)s_spikes[s].y;

        if (sx >= -6 && sx < 134) {
            if (s_spikes[s].is_ceiling) {
                // Pointing down
                g_api->draw_line(sx - 3, sy, sx + 3, sy, 1);
                g_api->draw_line(sx - 3, sy, sx, sy + 5, 1);
                g_api->draw_line(sx + 3, sy, sx, sy + 5, 1);
            } else {
                // Pointing up
                g_api->draw_line(sx - 3, sy, sx + 3, sy, 1);
                g_api->draw_line(sx - 3, sy, sx, sy - 5, 1);
                g_api->draw_line(sx + 3, sy, sx, sy - 5, 1);
            }
        }
    }

    // 4. Draw Gold Rings
    for (int r = 0; r < s_num_rings; r++) {
        if (!s_rings[r].collected) {
            int16_t rx = (int16_t)s_rings[r].x - cam;
            int16_t ry = (int16_t)s_rings[r].y;
            if (rx >= -6 && rx < 134) {
                g_api->draw_circle(rx, ry, 4, 1, false);
                g_api->draw_circle(rx, ry, 3, 1, false);
            }
        }
    }

    // 5. Draw Exit Gate
    int16_t gx = (int16_t)s_goal_x - cam;
    int16_t gy = (int16_t)s_goal_y;
    if (gx >= -10 && gx < 138) {
        g_api->draw_rect(gx - 4, gy - 8, 8, 16, 1, false);
        if (s_goal_open) {
            // Open flag/star
            g_api->draw_line(gx, gy - 8, gx, gy + 8, 1);
            g_api->draw_line(gx, gy - 8, gx + 4, gy - 4, 1);
            g_api->draw_line(gx + 4, gy - 4, gx, gy, 1);
        } else {
            // Closed lock
            g_api->draw_rect(gx - 2, gy - 2, 4, 4, 1, true);
        }
    }

    // 6. Draw Bouncing Ball
    if (s_state != BOUNCE_STATE_GAME_OVER) {
        int16_t bx = (int16_t)roundf(s_ball_x) - cam;
        int16_t by = (int16_t)roundf(s_ball_y);
        int16_t r = (int16_t)s_radius;

        // Flicker if respawning
        bool draw_ball = true;
        if (s_respawn_timer > 0.0f) {
            draw_ball = (((int)(s_respawn_timer * 15.0f)) % 2 == 0);
        }

        if (draw_ball) {
            g_api->draw_circle(bx, by, r, 1, true);
            // Highlight pixel for 3D sphere look
            g_api->draw_pixel(bx - 1, by - 1, 0);
        }
    }

    // 7. State Overlays
    if (s_state == BOUNCE_STATE_TITLE) {
        g_api->draw_rect(16, 14, 96, 36, 0, true);
        g_api->draw_rect(16, 14, 96, 36, 1, false);
        g_api->draw_string(26, 18, "NOKIA BOUNCE", 0);
        g_api->draw_string(24, 28, "COLLECT RINGS", 0);
        g_api->draw_string(26, 38, "[OK] TO BOUNCE", 0);
    } else if (s_state == BOUNCE_STATE_LEVEL_CLEAR) {
        g_api->draw_rect(18, 20, 92, 24, 0, true);
        g_api->draw_rect(18, 20, 92, 24, 1, false);
        g_api->draw_string(28, 25, "LEVEL CLEAR!", 0);
        snprintf(buf, sizeof(buf), "PREPARING LEVEL %d", s_level + 1);
        g_api->draw_string(20, 34, buf, 0);
    } else if (s_state == BOUNCE_STATE_GAME_OVER) {
        g_api->draw_rect(20, 18, 88, 28, 0, true);
        g_api->draw_rect(20, 18, 88, 28, 1, false);
        g_api->draw_string(34, 23, "GAME OVER", 0);
        snprintf(buf, sizeof(buf), "BEST: %d", s_high_score);
        g_api->draw_string(30, 32, buf, 0);
        g_api->draw_string(24, 40, "[OK] TO RESTART", 0);
    }

    g_api->flush_display();
}

void bounce_on_button(uint8_t btn, QButtonEvent evt) {
    if (evt != QEVT_BTN_DOWN && evt != QEVT_BTN_SHORT_CLICK) return;

    if (btn == QBTN_CANCEL) {
        if (g_api && g_api->exit_app) g_api->exit_app();
        return;
    }

    if (s_state == BOUNCE_STATE_TITLE) {
        if (btn == QBTN_OK) {
            s_state = BOUNCE_STATE_PLAYING;
            load_level(1);
            if (g_api->play_tone) g_api->play_tone(523, 40);
        }
        return;
    }

    if (s_state == BOUNCE_STATE_GAME_OVER) {
        if (btn == QBTN_OK) {
            bounce_init(g_api);
            s_state = BOUNCE_STATE_PLAYING;
        }
        return;
    }

    if (s_state == BOUNCE_STATE_PLAYING) {
        // [OK] Button = Super Jump / High Bounce
        if (btn == QBTN_OK) {
            // Boost bounce if near ground
            if (s_ball_vy >= -15.0f && s_ball_vy <= 25.0f) {
                s_ball_vy = -85.0f;
                if (g_api->play_tone) g_api->play_tone(520, 25);
            }
        } else if (btn == QBTN_UP) {
            // Roll Left
            s_ball_vx -= 20.0f;
            s_btn_override_timer = 0.35f;
        } else if (btn == QBTN_DOWN) {
            // Roll Right
            s_ball_vx += 20.0f;
            s_btn_override_timer = 0.35f;
        }
    }
}

void bounce_teardown(void) {
    if (g_api) {
        if (g_api->set_led)   g_api->set_led(0, 0, 0);
        if (g_api->stop_tone) g_api->stop_tone();
    }
    save_high_score();
    g_api = NULL;
}

// Unit test query helpers
int   bounce_get_score(void)        { return s_score; }
int   bounce_get_high_score(void)   { return s_high_score; }
int   bounce_get_lives(void)        { return s_lives; }
int   bounce_get_rings_left(void)   { return s_rings_left; }
int   bounce_get_level(void)        { return s_level; }
float bounce_get_ball_x(void)       { return s_ball_x; }
float bounce_get_ball_y(void)       { return s_ball_y; }
bool  bounce_is_game_over(void)     { return s_state == BOUNCE_STATE_GAME_OVER; }
