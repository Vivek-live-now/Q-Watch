#include "space_impact.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const QWatchAPI* g_api = NULL;

#define MAX_PLAYER_LASERS   6
#define MAX_PLAYER_MISSILES 2
#define MAX_ENEMIES         8
#define MAX_ENEMY_BULLETS   8
#define MAX_STARS           14
#define SPACE_IMPACT_SAVE   "/apps/space_impact.dat"

typedef enum {
    SI_STATE_TITLE = 0,
    SI_STATE_PLAYING,
    SI_STATE_STAGE_CLEAR,
    SI_STATE_GAME_OVER
} SpaceImpactState;

typedef enum {
    ENEMY_DRONE = 0,
    ENEMY_WAVE  = 1,
    ENEMY_ARMOR = 2,
    ENEMY_MINE  = 3,
    ENEMY_BOSS  = 4
} EnemyType;

typedef struct {
    float x;
    float y;
    bool  active;
} LaserBullet;

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    bool  active;
} Missile;

typedef struct {
    float x;
    float y;
    float base_y;
    float vx;
    float vy;
    float shoot_timer;
    float anim_timer;
    int   hp;
    int   max_hp;
    EnemyType type;
    bool  active;
} Enemy;

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    bool  active;
} EnemyBullet;

typedef struct {
    float x;
    float y;
    float speed;
} Star;

// Game State
static SpaceImpactState s_state = SI_STATE_TITLE;
static float s_player_x = 12.0f;
static float s_player_y = 32.0f;
static int   s_lives = 3;
static int   s_score = 0;
static int   s_high_score = 0;
static int   s_bombs = 3;
static int   s_stage = 1;
static float s_stage_progress = 0.0f;
static bool  s_boss_active = false;
static float s_state_timer = 0.0f;
static float s_fire_cooldown = 0.0f;
static float s_invincible_timer = 0.0f;

// Controls & calibration
static float s_neutral_pitch = 0.0f;
static float s_neutral_roll  = 0.0f;
static bool  s_tare_done = false;
static float s_btn_override_timer = 0.0f;

// Entities
static LaserBullet s_lasers[MAX_PLAYER_LASERS];
static Missile     s_missiles[MAX_PLAYER_MISSILES];
static Enemy       s_enemies[MAX_ENEMIES];
static EnemyBullet s_enemy_bullets[MAX_ENEMY_BULLETS];
static Star        s_stars[MAX_STARS];

// Forward declarations
static void spawn_enemy_wave(float dt);
static void fire_laser(void);
static void fire_missile(void);
static void load_high_score(void);
static void save_high_score(void);
static void init_stage(void);

// QApp Header descriptor
static const QAppHeader s_space_impact_header = {
    .magic = QAPP_MAGIC,
    .api_version = QAPP_API_VERSION,
    .required_caps = (QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_MPU | QAPP_CAP_AUDIO | QAPP_CAP_RGB_LED | QAPP_CAP_STORAGE),
    .name = "Space Impact 2",
    .version = "1.0.0",
    .author = "MI6 Cyber",
    .required_psram = 2048,
    .init = space_impact_init,
    .update = space_impact_update,
    .render = space_impact_render,
    .on_button = space_impact_on_button,
    .teardown = space_impact_teardown
};

const QAppHeader* get_space_impact_header(void) {
    return &s_space_impact_header;
}

static void load_high_score(void) {
    if (!g_api || !g_api->file_read) return;
    int saved = 0;
    int res = g_api->file_read(SPACE_IMPACT_SAVE, &saved, sizeof(saved));
    if (res == (int)sizeof(saved) && saved >= 0 && saved < 10000000) {
        s_high_score = saved;
    }
}

static void save_high_score(void) {
    if (!g_api || !g_api->file_write) return;
    if (s_score > s_high_score) s_high_score = s_score;
    g_api->file_write(SPACE_IMPACT_SAVE, &s_high_score, sizeof(s_high_score));
}

static void init_stage(void) {
    s_player_x = 12.0f;
    s_player_y = 32.0f;
    s_stage_progress = 0.0f;
    s_boss_active = false;
    s_invincible_timer = 1.5f;

    for (int i = 0; i < MAX_PLAYER_LASERS; i++) s_lasers[i].active = false;
    for (int i = 0; i < MAX_PLAYER_MISSILES; i++) s_missiles[i].active = false;
    for (int i = 0; i < MAX_ENEMIES; i++) s_enemies[i].active = false;
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) s_enemy_bullets[i].active = false;
}

int space_impact_init(const QWatchAPI* api) {
    if (!api) return QAPP_ERR_INVALID_PARAM;
    g_api = api;

    s_state = SI_STATE_TITLE;
    s_lives = 3;
    s_score = 0;
    s_bombs = 3;
    s_stage = 1;
    s_state_timer = 0.0f;
    s_fire_cooldown = 0.0f;
    s_tare_done = false;
    s_btn_override_timer = 0.0f;

    // Initialize stars
    for (int i = 0; i < MAX_STARS; i++) {
        if (g_api->random_range) {
            s_stars[i].x = (float)g_api->random_range(0, 128);
            s_stars[i].y = (float)g_api->random_range(10, 62);
            s_stars[i].speed = 15.0f + (float)g_api->random_range(5, 35);
        } else {
            s_stars[i].x = (float)(rand() % 128);
            s_stars[i].y = (float)(10 + (rand() % 52));
            s_stars[i].speed = 25.0f;
        }
    }

    load_high_score();
    init_stage();

    if (g_api->set_led) {
        g_api->set_led(0, 30, 60); // Nokia tactical blue
    }
    return QAPP_OK;
}

static void fire_laser(void) {
    if (!g_api) return;
    for (int i = 0; i < MAX_PLAYER_LASERS; i++) {
        if (!s_lasers[i].active) {
            s_lasers[i].x = s_player_x + 9.0f;
            s_lasers[i].y = s_player_y;
            s_lasers[i].active = true;
            if (g_api->play_tone) g_api->play_tone(1750, 18);
            break;
        }
    }
}

static void fire_missile(void) {
    if (!g_api || s_bombs <= 0) return;
    s_bombs--;
    for (int i = 0; i < MAX_PLAYER_MISSILES; i++) {
        if (!s_missiles[i].active) {
            s_missiles[i].x = s_player_x + 6.0f;
            s_missiles[i].y = s_player_y;
            s_missiles[i].vx = 75.0f;
            s_missiles[i].vy = 0.0f;
            s_missiles[i].active = true;
            if (g_api->play_tone) g_api->play_tone(450, 60);
            if (g_api->set_led)   g_api->set_led(60, 40, 0); // Orange flash
            break;
        }
    }
}

static void spawn_enemy_wave(float dt) {
    if (s_boss_active) return;

    s_stage_progress += dt * 0.08f;
    if (s_stage_progress >= 1.0f) {
        // Spawn Boss!
        s_boss_active = true;
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (!s_enemies[i].active) {
                s_enemies[i].x = 120.0f;
                s_enemies[i].y = 32.0f;
                s_enemies[i].base_y = 32.0f;
                s_enemies[i].vx = -12.0f;
                s_enemies[i].vy = 20.0f;
                s_enemies[i].shoot_timer = 1.0f;
                s_enemies[i].anim_timer = 0.0f;
                s_enemies[i].hp = 30;
                s_enemies[i].max_hp = 30;
                s_enemies[i].type = ENEMY_BOSS;
                s_enemies[i].active = true;
                if (g_api->play_tone) g_api->play_tone(880, 150);
                if (g_api->set_led)   g_api->set_led(80, 0, 0); // Red alert
                break;
            }
        }
        return;
    }

    // Normal wave enemy spawner
    static float s_spawn_timer = 0.0f;
    s_spawn_timer += dt;
    if (s_spawn_timer > 1.25f) {
        s_spawn_timer = 0.0f;
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (!s_enemies[i].active) {
                int rnd_type = g_api->random_range ? g_api->random_range(0, 4) : (rand() % 4);
                float sy = 16.0f + (g_api->random_range ? (float)g_api->random_range(0, 40) : (float)(rand() % 40));

                s_enemies[i].x = 132.0f;
                s_enemies[i].y = sy;
                s_enemies[i].base_y = sy;
                s_enemies[i].anim_timer = 0.0f;
                s_enemies[i].shoot_timer = 1.0f + ((float)(rand() % 10) * 0.15f);
                s_enemies[i].type = (EnemyType)rnd_type;

                if (rnd_type == ENEMY_DRONE) {
                    s_enemies[i].vx = -38.0f;
                    s_enemies[i].vy = 0.0f;
                    s_enemies[i].hp = 1;
                    s_enemies[i].max_hp = 1;
                } else if (rnd_type == ENEMY_WAVE) {
                    s_enemies[i].vx = -30.0f;
                    s_enemies[i].vy = 0.0f;
                    s_enemies[i].hp = 2;
                    s_enemies[i].max_hp = 2;
                } else if (rnd_type == ENEMY_ARMOR) {
                    s_enemies[i].vx = -46.0f;
                    s_enemies[i].vy = 0.0f;
                    s_enemies[i].hp = 4;
                    s_enemies[i].max_hp = 4;
                } else { // ENEMY_MINE
                    s_enemies[i].vx = -22.0f;
                    s_enemies[i].vy = 0.0f;
                    s_enemies[i].hp = 1;
                    s_enemies[i].max_hp = 1;
                }
                s_enemies[i].active = true;
                break;
            }
        }
    }
}

void space_impact_update(float dt) {
    if (!g_api) return;
    if (dt <= 0.0f || dt > 0.2f) dt = 0.016f;

    // Starfield scroll
    for (int i = 0; i < MAX_STARS; i++) {
        s_stars[i].x -= s_stars[i].speed * dt;
        if (s_stars[i].x < 0.0f) {
            s_stars[i].x = 127.0f;
            s_stars[i].y = (float)(10 + (rand() % 52));
        }
    }

    if (s_state == SI_STATE_TITLE) {
        s_state_timer += dt;
        return;
    }

    if (s_state == SI_STATE_STAGE_CLEAR) {
        s_state_timer += dt;
        if (s_state_timer > 2.5f) {
            s_stage++;
            s_bombs += 2;
            if (s_bombs > 5) s_bombs = 5;
            init_stage();
            s_state = SI_STATE_PLAYING;
            s_state_timer = 0.0f;
        }
        return;
    }

    if (s_state == SI_STATE_GAME_OVER) {
        s_state_timer += dt;
        return;
    }

    // --- Active Playing State ---
    if (s_invincible_timer > 0.0f) s_invincible_timer -= dt;
    if (s_fire_cooldown > 0.0f)    s_fire_cooldown -= dt;
    if (s_btn_override_timer > 0.0f) s_btn_override_timer -= dt;

    // 1. Controls: Button hold steering & continuous fire
    if (g_api->get_button_state) {
        uint8_t btn_mask = g_api->get_button_state();
        if (btn_mask & QBTN_UP) {
            s_player_y -= 48.0f * dt;
            s_btn_override_timer = 0.30f;
        }
        if (btn_mask & QBTN_DOWN) {
            s_player_y += 48.0f * dt;
            s_btn_override_timer = 0.30f;
        }
        // Continuous auto-fire when holding OK
        if ((btn_mask & QBTN_OK) && s_fire_cooldown <= 0.0f) {
            fire_laser();
            s_fire_cooldown = 0.18f;
        }
    }

    // 2. Analog Pitch / Roll IMU Steering with tare & deadband
    QTelemetry telem;
    memset(&telem, 0, sizeof(telem));
    g_api->get_telemetry(&telem);

    if (!s_tare_done) {
        s_neutral_pitch = telem.pitch;
        s_neutral_roll  = telem.roll;
        s_tare_done = true;
    }

    if (s_btn_override_timer <= 0.0f) {
        float diff_pitch = telem.pitch - s_neutral_pitch;
        float diff_roll  = telem.roll - s_neutral_roll;
        const float DEADBAND = 2.5f;

        float prop_pitch = 0.0f;
        if (fabsf(diff_pitch) > DEADBAND) {
            prop_pitch = (diff_pitch > 0.0f) ? (diff_pitch - DEADBAND) : (diff_pitch + DEADBAND);
        }
        float vy = (prop_pitch * 0.85f) + (telem.gyro_y * 0.25f);
        if (fabsf(vy) > 0.8f) {
            s_player_y += vy * dt;
        }

        float prop_roll = 0.0f;
        if (fabsf(diff_roll) > DEADBAND) {
            prop_roll = (diff_roll > 0.0f) ? (diff_roll - DEADBAND) : (diff_roll + DEADBAND);
        }
        float vx = (prop_roll * 0.65f) + (telem.gyro_x * 0.20f);
        if (fabsf(vx) > 0.8f) {
            s_player_x += vx * dt;
        }
    }

    // Boundary constraints
    if (s_player_x < 4.0f)  s_player_x = 4.0f;
    if (s_player_x > 38.0f) s_player_x = 38.0f;
    if (s_player_y < 12.0f) s_player_y = 12.0f;
    if (s_player_y > 58.0f) s_player_y = 58.0f;

    // 3. Move Player Lasers
    for (int i = 0; i < MAX_PLAYER_LASERS; i++) {
        if (s_lasers[i].active) {
            s_lasers[i].x += 125.0f * dt;
            if (s_lasers[i].x > 128.0f) s_lasers[i].active = false;
        }
    }

    // 4. Move Player Missiles
    for (int i = 0; i < MAX_PLAYER_MISSILES; i++) {
        if (s_missiles[i].active) {
            s_missiles[i].x += s_missiles[i].vx * dt;
            if (s_missiles[i].x > 128.0f) s_missiles[i].active = false;
        }
    }

    // 5. Spawn Enemies
    spawn_enemy_wave(dt);

    // 6. Update Enemies & Alien AI
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!s_enemies[i].active) continue;

        s_enemies[i].anim_timer += dt;
        if (s_enemies[i].type == ENEMY_WAVE) {
            s_enemies[i].x += s_enemies[i].vx * dt;
            s_enemies[i].y = s_enemies[i].base_y + sinf(s_enemies[i].anim_timer * 4.0f) * 12.0f;
        } else if (s_enemies[i].type == ENEMY_BOSS) {
            // Boss enters to x=102, then oscillates vertically
            if (s_enemies[i].x > 102.0f) {
                s_enemies[i].x += s_enemies[i].vx * dt;
            }
            s_enemies[i].y += s_enemies[i].vy * dt;
            if (s_enemies[i].y < 16.0f) {
                s_enemies[i].y = 16.0f;
                s_enemies[i].vy = -s_enemies[i].vy;
            } else if (s_enemies[i].y > 48.0f) {
                s_enemies[i].y = 48.0f;
                s_enemies[i].vy = -s_enemies[i].vy;
            }
        } else {
            s_enemies[i].x += s_enemies[i].vx * dt;
        }

        // Alien shooting logic
        s_enemies[i].shoot_timer -= dt;
        if (s_enemies[i].shoot_timer <= 0.0f) {
            s_enemies[i].shoot_timer = (s_enemies[i].type == ENEMY_BOSS) ? 0.9f : 2.2f;
            // Spawn enemy bullet
            for (int b = 0; b < MAX_ENEMY_BULLETS; b++) {
                if (!s_enemy_bullets[b].active) {
                    s_enemy_bullets[b].x = s_enemies[i].x - 4.0f;
                    s_enemy_bullets[b].y = s_enemies[i].y;
                    s_enemy_bullets[b].vx = -55.0f;
                    s_enemy_bullets[b].vy = (s_enemies[i].type == ENEMY_BOSS) ? ((s_player_y - s_enemies[i].y) * 0.8f) : 0.0f;
                    s_enemy_bullets[b].active = true;
                    if (g_api->play_tone) g_api->play_tone(320, 15);
                    break;
                }
            }
        }

        // Offscreen check
        if (s_enemies[i].x < -12.0f) {
            s_enemies[i].active = false;
        }
    }

    // 7. Update Enemy Bullets
    for (int b = 0; b < MAX_ENEMY_BULLETS; b++) {
        if (!s_enemy_bullets[b].active) continue;
        s_enemy_bullets[b].x += s_enemy_bullets[b].vx * dt;
        s_enemy_bullets[b].y += s_enemy_bullets[b].vy * dt;

        if (s_enemy_bullets[b].x < -4.0f || s_enemy_bullets[b].y < 8.0f || s_enemy_bullets[b].y > 64.0f) {
            s_enemy_bullets[b].active = false;
            continue;
        }

        // Hit player check
        if (s_invincible_timer <= 0.0f) {
            float dx = fabsf(s_enemy_bullets[b].x - s_player_x);
            float dy = fabsf(s_enemy_bullets[b].y - s_player_y);
            if (dx < 6.0f && dy < 5.0f) {
                s_enemy_bullets[b].active = false;
                s_lives--;
                s_invincible_timer = 2.0f;
                if (g_api->play_tone) g_api->play_tone(150, 75);
                if (g_api->set_led)   g_api->set_led(80, 0, 0); // Red damage
                if (s_lives <= 0) {
                    s_state = SI_STATE_GAME_OVER;
                    save_high_score();
                }
            }
        }
    }

    // 8. Player Bullets vs Enemies Collision
    for (int l = 0; l < MAX_PLAYER_LASERS; l++) {
        if (!s_lasers[l].active) continue;
        for (int e = 0; e < MAX_ENEMIES; e++) {
            if (!s_enemies[e].active) continue;
            float ew = (s_enemies[e].type == ENEMY_BOSS) ? 14.0f : 7.0f;
            float eh = (s_enemies[e].type == ENEMY_BOSS) ? 16.0f : 6.0f;

            if (s_lasers[l].x >= (s_enemies[e].x - ew) && s_lasers[l].x <= (s_enemies[e].x + ew) &&
                fabsf(s_lasers[l].y - s_enemies[e].y) < eh) {
                s_lasers[l].active = false;
                s_enemies[e].hp--;
                if (g_api->play_tone) g_api->play_tone(600, 15);

                if (s_enemies[e].hp <= 0) {
                    s_enemies[e].active = false;
                    s_score += (s_enemies[e].type == ENEMY_BOSS) ? 1500 : 100;
                    if (g_api->play_tone) g_api->play_tone(180, 45);
                    if (g_api->set_led)   g_api->set_led(0, 80, 0);

                    if (s_enemies[e].type == ENEMY_BOSS) {
                        s_boss_active = false;
                        s_state = SI_STATE_STAGE_CLEAR;
                        s_state_timer = 0.0f;
                        save_high_score();
                        if (g_api->play_tone) g_api->play_tone(1046, 180);
                    }
                }
                break;
            }
        }
    }

    // 9. Player Missiles vs Enemies Collision (Massive Area Damage)
    for (int m = 0; m < MAX_PLAYER_MISSILES; m++) {
        if (!s_missiles[m].active) continue;
        for (int e = 0; e < MAX_ENEMIES; e++) {
            if (!s_enemies[e].active) continue;
            float dx = fabsf(s_missiles[m].x - s_enemies[e].x);
            float dy = fabsf(s_missiles[m].y - s_enemies[e].y);
            if (dx < 12.0f && dy < 12.0f) {
                s_missiles[m].active = false;
                s_enemies[e].hp -= 8;
                if (g_api->play_tone) g_api->play_tone(140, 80);
                if (g_api->set_led)   g_api->set_led(100, 50, 0);

                if (s_enemies[e].hp <= 0) {
                    s_enemies[e].active = false;
                    s_score += (s_enemies[e].type == ENEMY_BOSS) ? 1500 : 150;
                    if (s_enemies[e].type == ENEMY_BOSS) {
                        s_boss_active = false;
                        s_state = SI_STATE_STAGE_CLEAR;
                        s_state_timer = 0.0f;
                        save_high_score();
                    }
                }
                break;
            }
        }
    }

    // 10. Player Ship vs Enemy Collision
    if (s_invincible_timer <= 0.0f) {
        for (int e = 0; e < MAX_ENEMIES; e++) {
            if (!s_enemies[e].active) continue;
            float dx = fabsf(s_enemies[e].x - s_player_x);
            float dy = fabsf(s_enemies[e].y - s_player_y);
            if (dx < 8.0f && dy < 6.0f) {
                if (s_enemies[e].type != ENEMY_BOSS) {
                    s_enemies[e].active = false;
                }
                s_lives--;
                s_invincible_timer = 2.0f;
                if (g_api->play_tone) g_api->play_tone(120, 80);
                if (g_api->set_led)   g_api->set_led(100, 0, 0);
                if (s_lives <= 0) {
                    s_state = SI_STATE_GAME_OVER;
                    save_high_score();
                }
                break;
            }
        }
    }
}

void space_impact_render(void) {
    if (!g_api) return;

    g_api->clear_screen();

    // 1. Draw Starfield
    for (int i = 0; i < MAX_STARS; i++) {
        g_api->draw_pixel((int16_t)s_stars[i].x, (int16_t)s_stars[i].y, 1);
    }

    // 2. Top HUD Bar (y=0 to 8)
    g_api->draw_line(0, 9, 127, 9, 1);
    char buf[20];
    snprintf(buf, sizeof(buf), "SC:%d", s_score);
    g_api->draw_string(2, 1, buf, 0);

    // Lives icons
    for (int l = 0; l < s_lives; l++) {
        int16_t lx = 60 + l * 7;
        g_api->draw_line(lx, 3, lx + 4, 5, 1);
        g_api->draw_line(lx, 7, lx + 4, 5, 1);
        g_api->draw_line(lx, 3, lx, 7, 1);
    }

    // Bombs count
    snprintf(buf, sizeof(buf), "B:%d", s_bombs);
    g_api->draw_string(88, 1, buf, 0);

    // 3. Draw Player Lasers & Missiles
    for (int i = 0; i < MAX_PLAYER_LASERS; i++) {
        if (s_lasers[i].active) {
            int16_t lx = (int16_t)s_lasers[i].x;
            int16_t ly = (int16_t)s_lasers[i].y;
            g_api->draw_line(lx, ly, lx + 3, ly, 1);
        }
    }
    for (int i = 0; i < MAX_PLAYER_MISSILES; i++) {
        if (s_missiles[i].active) {
            int16_t mx = (int16_t)s_missiles[i].x;
            int16_t my = (int16_t)s_missiles[i].y;
            g_api->draw_rect(mx, my - 1, 5, 3, 1, true);
        }
    }

    // 4. Draw Player Ship (Nokia delta-wing fighter)
    // Flicker if invincible
    bool draw_ship = true;
    if (s_invincible_timer > 0.0f) {
        draw_ship = (((int)(s_invincible_timer * 15.0f)) % 2 == 0);
    }
    if (draw_ship && s_state != SI_STATE_GAME_OVER) {
        int16_t px = (int16_t)s_player_x;
        int16_t py = (int16_t)s_player_y;
        // Fighter fuselage & wings
        g_api->draw_line(px - 5, py - 4, px + 4, py, 1);
        g_api->draw_line(px - 5, py + 4, px + 4, py, 1);
        g_api->draw_line(px - 5, py - 4, px - 5, py + 4, 1);
        g_api->draw_line(px - 2, py - 2, px + 2, py, 1);
        g_api->draw_line(px - 2, py + 2, px + 2, py, 1);
        g_api->draw_pixel(px + 5, py, 1); // Cockpit needle
    }

    // 5. Draw Enemies
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!s_enemies[i].active) continue;
        int16_t ex = (int16_t)s_enemies[i].x;
        int16_t ey = (int16_t)s_enemies[i].y;

        if (s_enemies[i].type == ENEMY_DRONE) {
            // Bio-drone (chevron)
            g_api->draw_line(ex + 3, ey - 3, ex - 3, ey, 1);
            g_api->draw_line(ex + 3, ey + 3, ex - 3, ey, 1);
            g_api->draw_line(ex + 3, ey - 3, ex + 3, ey + 3, 1);
            g_api->draw_pixel(ex - 1, ey, 1);
        } else if (s_enemies[i].type == ENEMY_WAVE) {
            // Wave flyer (saucer)
            g_api->draw_rect(ex - 3, ey - 2, 7, 5, 1, false);
            g_api->draw_line(ex - 4, ey, ex + 4, ey, 1);
        } else if (s_enemies[i].type == ENEMY_ARMOR) {
            // Heavy cruiser
            g_api->draw_rect(ex - 4, ey - 4, 9, 9, 1, false);
            g_api->draw_rect(ex - 2, ey - 2, 5, 5, 1, true);
        } else if (s_enemies[i].type == ENEMY_MINE) {
            // Explosive mine
            g_api->draw_circle(ex, ey, 3, 1, false);
            g_api->draw_pixel(ex, ey, 1);
        } else if (s_enemies[i].type == ENEMY_BOSS) {
            // Giant Alien Boss Cruiser (16x22)
            g_api->draw_rect(ex - 8, ey - 10, 16, 20, 1, false);
            g_api->draw_line(ex - 8, ey, ex - 12, ey, 1);
            g_api->draw_line(ex - 8, ey - 6, ex - 14, ey - 3, 1);
            g_api->draw_line(ex - 8, ey + 6, ex - 14, ey + 3, 1);
            g_api->draw_rect(ex - 2, ey - 4, 6, 8, 1, true); // Core

            // Boss health bar at top right
            int bar_w = (s_enemies[i].hp * 26) / s_enemies[i].max_hp;
            if (bar_w < 0) bar_w = 0;
            g_api->draw_rect(100, 2, 26, 5, 1, false);
            g_api->draw_rect(100, 2, bar_w, 5, 1, true);
        }
    }

    // 6. Draw Enemy Bullets
    for (int b = 0; b < MAX_ENEMY_BULLETS; b++) {
        if (s_enemy_bullets[b].active) {
            int16_t bx = (int16_t)s_enemy_bullets[b].x;
            int16_t by = (int16_t)s_enemy_bullets[b].y;
            g_api->draw_rect(bx - 1, by - 1, 3, 3, 1, true);
        }
    }

    // 7. State Overlays
    if (s_state == SI_STATE_TITLE) {
        g_api->draw_rect(14, 14, 100, 36, 0, true);
        g_api->draw_rect(14, 14, 100, 36, 1, false);
        g_api->draw_string(22, 18, "SPACE IMPACT 2", 0);
        g_api->draw_string(28, 28, "NOKIA CLASSIC", 0);
        g_api->draw_string(26, 38, "[OK] TO LAUNCH", 0);
    } else if (s_state == SI_STATE_STAGE_CLEAR) {
        g_api->draw_rect(18, 20, 92, 24, 0, true);
        g_api->draw_rect(18, 20, 92, 24, 1, false);
        g_api->draw_string(24, 25, "SECTOR SECURED!", 0);
        snprintf(buf, sizeof(buf), "PREPARING STAGE %d", s_stage + 1);
        g_api->draw_string(20, 34, buf, 0);
    } else if (s_state == SI_STATE_GAME_OVER) {
        g_api->draw_rect(20, 18, 88, 28, 0, true);
        g_api->draw_rect(20, 18, 88, 28, 1, false);
        g_api->draw_string(34, 23, "GAME OVER", 0);
        snprintf(buf, sizeof(buf), "BEST: %d", s_high_score);
        g_api->draw_string(30, 32, buf, 0);
        g_api->draw_string(24, 40, "[OK] TO RESTART", 0);
    }

    g_api->flush_display();
}

void space_impact_on_button(uint8_t btn, QButtonEvent evt) {
    if (evt != QEVT_BTN_DOWN && evt != QEVT_BTN_SHORT_CLICK) return;

    if (btn == QBTN_CANCEL) {
        if (g_api && g_api->exit_app) g_api->exit_app();
        return;
    }

    if (s_state == SI_STATE_TITLE) {
        if (btn == QBTN_OK) {
            s_state = SI_STATE_PLAYING;
            init_stage();
            if (g_api->play_tone) g_api->play_tone(1000, 40);
        }
        return;
    }

    if (s_state == SI_STATE_GAME_OVER) {
        if (btn == QBTN_OK) {
            space_impact_init(g_api);
            s_state = SI_STATE_PLAYING;
        }
        return;
    }

    if (s_state == SI_STATE_PLAYING) {
        if (btn == QBTN_UP) {
            s_player_y -= 8.0f;
            s_btn_override_timer = 0.35f;
        } else if (btn == QBTN_DOWN) {
            s_player_y += 8.0f;
            s_btn_override_timer = 0.35f;
        } else if (btn == QBTN_OK) {
            // Tap OK fires laser; double-tap or tap with missile available launches missile if cooldown
            if (s_fire_cooldown <= 0.0f) {
                fire_laser();
                s_fire_cooldown = 0.15f;
            } else if (s_bombs > 0) {
                fire_missile();
            }
        }
    }
}

void space_impact_teardown(void) {
    if (g_api) {
        if (g_api->set_led)   g_api->set_led(0, 0, 0);
        if (g_api->stop_tone) g_api->stop_tone();
    }
    save_high_score();
    g_api = NULL;
}

// Unit test query helpers
int  space_impact_get_score(void)        { return s_score; }
int  space_impact_get_high_score(void)   { return s_high_score; }
int  space_impact_get_lives(void)        { return s_lives; }
int  space_impact_get_bombs(void)        { return s_bombs; }
bool space_impact_is_boss_active(void)   { return s_boss_active; }
bool space_impact_is_game_over(void)     { return s_state == SI_STATE_GAME_OVER; }
