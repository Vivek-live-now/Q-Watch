#include "pacman.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const QWatchAPI* g_api = NULL;

#define MAZE_COLS 19
#define MAZE_ROWS 9
#define CELL_SZ   6
#define OFFSET_X  7
#define OFFSET_Y  8
#define PACMAN_SAVE_PATH "/apps/pacman.dat"

typedef enum {
    PAC_DIR_RIGHT = 0,
    PAC_DIR_DOWN  = 1,
    PAC_DIR_LEFT  = 2,
    PAC_DIR_UP    = 3,
    PAC_DIR_NONE  = 4
} PacDir;

typedef struct {
    float x;
    float y;
    PacDir dir;
    PacDir next_dir;
    int mouth_anim;
} PacmanEntity;

typedef struct {
    float x;
    float y;
    PacDir dir;
    bool frightened;
    bool eaten;
} GhostEntity;

typedef struct {
    uint16_t freq;
    uint16_t dur;
} PacTone;

// 100% symmetric 19x9 arcade Pac-Man maze template:
// 1 = Wall, 0 = Dot, 2 = Power Pellet, 3 = Empty / Ghost House / Tunnel
static const uint8_t s_maze_template[MAZE_ROWS][MAZE_COLS] = {
    { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 }, // Row 0: Top outer wall
    { 1,2,0,0,1,0,0,0,1,0,1,0,0,0,1,0,0,2,1 }, // Row 1: Corridors with Power Pellets (2) at corners
    { 1,0,1,0,1,0,1,0,0,0,0,0,1,0,1,0,1,0,1 }, // Row 2: Pillars
    { 0,0,1,0,0,0,1,1,3,3,3,1,1,0,0,0,1,0,0 }, // Row 3: Wrap tunnels (cols 0,1 & 17,18) + Ghost house gate (3)
    { 1,0,1,0,1,0,1,3,3,3,3,3,1,0,1,0,1,0,1 }, // Row 4: Ghost house interior + side halls
    { 1,0,0,0,1,0,1,1,1,1,1,1,1,0,1,0,0,0,1 }, // Row 5: Ghost house floor
    { 1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1 }, // Row 6: Wide open center corridor (Col 9 is 0!)
    { 1,2,0,0,1,1,1,0,0,0,0,0,1,1,1,0,0,2,1 }, // Row 7: Bottom corridor with Power Pellets (2) at corners
    { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 }  // Row 8: Bottom outer wall
};

static uint8_t s_maze[MAZE_ROWS][MAZE_COLS];
static int     s_dots_total = 0;
static int     s_dots_left = 0;

static PacmanEntity s_pacman;
static GhostEntity  s_blinky;
static GhostEntity  s_pinky;

static int   s_score = 0;
static int   s_high_score = 0;
static int   s_lives = 3;
static bool  s_game_over = false;
static bool  s_level_clear = false;
static float s_frightened_timer = 0.0f;
static bool  s_waka_phase = false;
static float s_flash_timer = 0.0f;

// Intro & Death Sequence State
static float s_intro_timer = 0.0f;
static float s_death_timer = 0.0f;

// Sound Sequencer State
static const PacTone* s_sound_seq = NULL;
static int            s_sound_len = 0;
static int            s_sound_idx = 0;
static float          s_sound_timer = 0.0f;
static float          s_siren_timer = 0.0f;

// Input handling & tilt Tare
static float s_neutral_roll = 0.0f;
static float s_neutral_pitch = 0.0f;
static bool  s_tare_done = false;
static float s_btn_override_timer = 0.0f;

// Original Pac-Man Theme Tunes
static const PacTone s_pac_intro[] = {
    { 494, 110 }, { 988, 110 }, { 740, 110 }, { 622, 110 },
    { 988, 55  }, { 740, 110 }, { 622, 160 },
    { 523, 110 }, { 1046, 110 }, { 784, 110 }, { 659, 110 },
    { 1046, 55 }, { 784, 110 }, { 659, 160 },
    { 494, 110 }, { 988, 110 }, { 740, 110 }, { 622, 110 },
    { 988, 55  }, { 740, 110 }, { 622, 140 },
    { 622, 55  }, { 659, 55  }, { 698, 55  }, { 740, 55 },
    { 784, 55  }, { 831, 55  }, { 880, 55  }, { 988, 220 }
};

static const PacTone s_pac_death[] = {
    { 880, 45 }, { 831, 45 }, { 784, 45 }, { 740, 45 },
    { 698, 45 }, { 659, 45 }, { 622, 45 }, { 587, 45 },
    { 554, 45 }, { 523, 45 }, { 494, 45 }, { 440, 50 },
    { 370, 60 }, { 260, 80 }, { 160, 140 }
};

static const PacTone s_pac_eat_ghost[] = {
    { 523, 30 }, { 784, 30 }, { 1046, 35 }, { 1568, 60 }
};

static const PacTone s_pac_clear[] = {
    { 523, 90 }, { 659, 90 }, { 784, 90 }, { 1046, 110 }, { 1318, 110 }, { 1568, 220 }
};

static void play_sound_sequence(const PacTone* seq, int count) {
    s_sound_seq = seq;
    s_sound_len = count;
    s_sound_idx = 0;
    s_sound_timer = 0.0f;
}

static void update_sound_sequencer(float dt) {
    if (!g_api || !g_api->play_tone) return;
    if (s_sound_seq && s_sound_idx < s_sound_len) {
        s_sound_timer -= dt;
        if (s_sound_timer <= 0.0f) {
            uint16_t freq = s_sound_seq[s_sound_idx].freq;
            uint16_t dur = s_sound_seq[s_sound_idx].dur;
            if (freq > 0) {
                g_api->play_tone(freq, dur);
            }
            s_sound_idx++;
            if (s_sound_idx < s_sound_len) {
                s_sound_timer = (float)dur / 1000.0f;
            } else {
                s_sound_seq = NULL;
            }
        }
    }
}

static const QAppHeader s_pacman_header = {
    .magic = QAPP_MAGIC,
    .api_version = QAPP_API_VERSION,
    .required_caps = (QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_MPU | QAPP_CAP_AUDIO | QAPP_CAP_RGB_LED | QAPP_CAP_STORAGE),
    .name = "Pacman Arcade",
    .version = "1.0.0",
    .author = "Q-Watch Lab",
    .required_psram = 2048,
    .init = pacman_init,
    .update = pacman_update,
    .render = pacman_render,
    .on_button = pacman_on_button,
    .teardown = pacman_teardown
};

const QAppHeader* get_pacman_header(void) {
    return &s_pacman_header;
}

static void load_high_score(void) {
    if (!g_api || !g_api->file_read) return;
    int saved = 0;
    int res = g_api->file_read(PACMAN_SAVE_PATH, &saved, sizeof(saved));
    if (res == (int)sizeof(saved) && saved >= 0 && saved < 1000000) {
        s_high_score = saved;
    }
}

static void save_high_score(void) {
    if (!g_api || !g_api->file_write) return;
    if (s_score > s_high_score) s_high_score = s_score;
    g_api->file_write(PACMAN_SAVE_PATH, &s_high_score, sizeof(s_high_score));
}

static void reset_maze(void) {
    s_dots_total = 0;
    for (int r = 0; r < MAZE_ROWS; r++) {
        for (int c = 0; c < MAZE_COLS; c++) {
            s_maze[r][c] = s_maze_template[r][c];
            if (s_maze[r][c] == 0 || s_maze[r][c] == 2) {
                s_dots_total++;
            }
        }
    }
    s_dots_left = s_dots_total;

    // Pacman starts at center corridor (Column 9, Row 6) - completely open tile!
    s_pacman.x = 9.0f;
    s_pacman.y = 6.0f;
    s_pacman.dir = PAC_DIR_LEFT;
    s_pacman.next_dir = PAC_DIR_LEFT;
    s_pacman.mouth_anim = 0;

    // Blinky starts at gate exit (Row 3, Col 9)
    s_blinky.x = 9.0f;
    s_blinky.y = 3.0f;
    s_blinky.dir = PAC_DIR_UP;
    s_blinky.frightened = false;
    s_blinky.eaten = false;

    // Pinky starts inside ghost house (Row 4, Col 8)
    s_pinky.x = 8.0f;
    s_pinky.y = 4.0f;
    s_pinky.dir = PAC_DIR_UP;
    s_pinky.frightened = false;
    s_pinky.eaten = false;

    s_frightened_timer = 0.0f;
    s_death_timer = 0.0f;
}

int pacman_init(const QWatchAPI* api) {
    if (!api) return QAPP_ERR_INVALID_PARAM;
    g_api = api;

    s_score = 0;
    s_lives = 3;
    s_game_over = false;
    s_level_clear = false;
    s_tare_done = false;
    s_btn_override_timer = 0.0f;
    s_intro_timer = 2.8f; // ~2.8 seconds of opening tune and "READY!" display

    load_high_score();
    reset_maze();

    if (g_api->set_led) g_api->set_led(80, 80, 0); // Yellow Pacman LED
    play_sound_sequence(s_pac_intro, (int)(sizeof(s_pac_intro) / sizeof(s_pac_intro[0])));

    return QAPP_OK;
}

static bool is_tile_walkable(int c, int r) {
    if (r == 3 && (c < 0 || c >= MAZE_COLS)) return true; // Horizontal warp tunnel
    if (c < 0 || c >= MAZE_COLS || r < 0 || r >= MAZE_ROWS) return false;
    return (s_maze[r][c] != 1);
}

static bool is_opposite_dir(PacDir a, PacDir b) {
    if (a == PAC_DIR_RIGHT && b == PAC_DIR_LEFT) return true;
    if (a == PAC_DIR_LEFT && b == PAC_DIR_RIGHT) return true;
    if (a == PAC_DIR_UP && b == PAC_DIR_DOWN) return true;
    if (a == PAC_DIR_DOWN && b == PAC_DIR_UP) return true;
    return false;
}

// Robust grid-based entity movement with zero-sticking guarantee
static void move_entity(float* x, float* y, PacDir dir, float speed, float dt) {
    float step = speed * dt;
    if (step > 0.35f) step = 0.35f;

    // Wrap around horizontal tunnel at row 3
    if (fabsf(*y - 3.0f) < 0.3f) {
        if (*x < -0.5f) { *x = (float)MAZE_COLS - 0.5f; return; }
        if (*x > (float)MAZE_COLS - 0.5f) { *x = -0.5f; return; }
    }

    if (dir == PAC_DIR_RIGHT) {
        *y = roundf(*y);
        int cur_c = (int)floorf(*x);
        int next_c = cur_c + 1;
        if (is_tile_walkable(next_c, (int)*y)) {
            *x += step;
        } else {
            float stop_x = (float)cur_c;
            if (*x < stop_x) {
                *x += step;
                if (*x > stop_x) *x = stop_x;
            } else {
                *x = stop_x;
            }
        }
    } else if (dir == PAC_DIR_LEFT) {
        *y = roundf(*y);
        int cur_c = (int)ceilf(*x);
        int next_c = cur_c - 1;
        if (is_tile_walkable(next_c, (int)*y)) {
            *x -= step;
        } else {
            float stop_x = (float)cur_c;
            if (*x > stop_x) {
                *x -= step;
                if (*x < stop_x) *x = stop_x;
            } else {
                *x = stop_x;
            }
        }
    } else if (dir == PAC_DIR_DOWN) {
        *x = roundf(*x);
        int cur_r = (int)floorf(*y);
        int next_r = cur_r + 1;
        if (is_tile_walkable((int)*x, next_r)) {
            *y += step;
        } else {
            float stop_y = (float)cur_r;
            if (*y < stop_y) {
                *y += step;
                if (*y > stop_y) *y = stop_y;
            } else {
                *y = stop_y;
            }
        }
    } else if (dir == PAC_DIR_UP) {
        *x = roundf(*x);
        int cur_r = (int)ceilf(*y);
        int next_r = cur_r - 1;
        if (is_tile_walkable((int)*x, next_r)) {
            *y -= step;
        } else {
            float stop_y = (float)cur_r;
            if (*y > stop_y) {
                *y -= step;
                if (*y < stop_y) *y = stop_y;
            } else {
                *y = stop_y;
            }
        }
    }
}

void pacman_update(float dt) {
    if (!g_api) return;
    if (dt <= 0.0f || dt > 0.2f) dt = 0.016f;

    // Tare neutral wrist posture on first frame
    if (!s_tare_done) {
        QTelemetry telem;
        g_api->get_telemetry(&telem);
        s_neutral_roll = telem.roll;
        s_neutral_pitch = telem.pitch;
        s_tare_done = true;
    }

    update_sound_sequencer(dt);

    if (s_game_over || s_level_clear) return;

    // Handle opening intro countdown
    if (s_intro_timer > 0.0f) {
        s_intro_timer -= dt;
        return;
    }

    // Handle death animation pause
    if (s_death_timer > 0.0f) {
        s_death_timer -= dt;
        if (s_death_timer <= 0.0f) {
            s_pacman.x = 9.0f; s_pacman.y = 6.0f;
            s_pacman.dir = PAC_DIR_LEFT;
            s_pacman.next_dir = PAC_DIR_LEFT;
            s_blinky.x = 9.0f; s_blinky.y = 3.0f; s_blinky.dir = PAC_DIR_UP;
            s_pinky.x = 8.0f; s_pinky.y = 4.0f; s_pinky.dir = PAC_DIR_UP;
            if (g_api->set_led) g_api->set_led(80, 80, 0);
        }
        return;
    }

    // Flash timer for power pellets & HUD
    s_flash_timer += dt;

    // Button override timer
    if (s_btn_override_timer > 0.0f) {
        s_btn_override_timer -= dt;
    } else {
        // Unified Sensor Calibration tilt buffer
        QTelemetry telem;
        g_api->get_telemetry(&telem);
        float d_roll = telem.roll - s_neutral_roll;
        float d_pitch = telem.pitch - s_neutral_pitch;

        // Fused tilt and gyro flick rate-of-change steering
        float roll_signal = d_roll + (telem.gyro_x * 0.25f);
        float pitch_signal = d_pitch + (telem.gyro_y * 0.25f);

        if (fabsf(roll_signal) > 12.0f || fabsf(pitch_signal) > 12.0f) {
            if (fabsf(roll_signal) > fabsf(pitch_signal)) {
                if (roll_signal > 12.0f)       s_pacman.next_dir = PAC_DIR_RIGHT;
                else if (roll_signal < -12.0f) s_pacman.next_dir = PAC_DIR_LEFT;
            } else {
                if (pitch_signal > 12.0f)      s_pacman.next_dir = PAC_DIR_DOWN;
                else if (pitch_signal < -12.0f) s_pacman.next_dir = PAC_DIR_UP;
            }
        }
    }

    // Frightened ghost timer & siren
    if (s_frightened_timer > 0.0f) {
        s_frightened_timer -= dt;
        s_siren_timer -= dt;
        if (s_siren_timer <= 0.0f && !s_sound_seq) {
            s_waka_phase = !s_waka_phase;
            if (g_api->play_tone) g_api->play_tone(s_waka_phase ? 640 : 760, 40);
            s_siren_timer = 0.18f;
        }
        if (s_frightened_timer <= 0.0f) {
            s_blinky.frightened = false;
            s_pinky.frightened = false;
            if (g_api->set_led) g_api->set_led(80, 80, 0); // Back to yellow LED
        }
    }

    // Pacman Direction Execution:
    // 1. Immediate 180-degree turnaround allowed anywhere in a corridor
    if (is_opposite_dir(s_pacman.dir, s_pacman.next_dir)) {
        s_pacman.dir = s_pacman.next_dir;
    }
    // 2. 90-degree cornering when near tile center
    else if (s_pacman.next_dir != s_pacman.dir) {
        int cur_c = (int)roundf(s_pacman.x);
        int cur_r = (int)roundf(s_pacman.y);
        float dist_to_center = hypotf(s_pacman.x - (float)cur_c, s_pacman.y - (float)cur_r);
        if (dist_to_center < 0.35f) {
            int tc = cur_c; int tr = cur_r;
            if (s_pacman.next_dir == PAC_DIR_RIGHT) tc++;
            else if (s_pacman.next_dir == PAC_DIR_LEFT) tc--;
            else if (s_pacman.next_dir == PAC_DIR_DOWN) tr++;
            else if (s_pacman.next_dir == PAC_DIR_UP) tr--;

            if (is_tile_walkable(tc, tr)) {
                s_pacman.x = (float)cur_c;
                s_pacman.y = (float)cur_r;
                s_pacman.dir = s_pacman.next_dir;
            }
        }
    }

    // Move Pacman
    float pac_speed = 3.8f; // cells per second
    move_entity(&s_pacman.x, &s_pacman.y, s_pacman.dir, pac_speed, dt);
    s_pacman.mouth_anim = ((int)(s_pacman.x * 5.0f + s_pacman.y * 5.0f)) % 4;

    // Eat dots & power pellets
    int pc = (int)roundf(s_pacman.x);
    int pr = (int)roundf(s_pacman.y);
    if (pc >= 0 && pc < MAZE_COLS && pr >= 0 && pr < MAZE_ROWS) {
        if (s_maze[pr][pc] == 0) {
            // Normal dot: alternating classic Waka-Waka munch
            s_maze[pr][pc] = 3;
            s_score += 10;
            s_dots_left--;
            s_waka_phase = !s_waka_phase;
            if (!s_sound_seq && s_frightened_timer <= 0.0f && g_api->play_tone) {
                g_api->play_tone(s_waka_phase ? 494 : 330, 25);
            }
            if (s_score > s_high_score) s_high_score = s_score;
        } else if (s_maze[pr][pc] == 2) {
            // Power Pellet!
            s_maze[pr][pc] = 3;
            s_score += 50;
            s_dots_left--;
            s_frightened_timer = 7.5f;
            s_blinky.frightened = true;
            s_pinky.frightened = true;
            s_blinky.eaten = false;
            s_pinky.eaten = false;
            s_siren_timer = 0.0f;
            if (g_api->set_led) g_api->set_led(0, 0, 90); // Blue frightened LED
            if (s_score > s_high_score) s_high_score = s_score;
        }
    }

    // Level Clear Check
    if (s_dots_left <= 0) {
        s_level_clear = true;
        play_sound_sequence(s_pac_clear, (int)(sizeof(s_pac_clear) / sizeof(s_pac_clear[0])));
        save_high_score();
        return;
    }

    // Ghost AI Navigation
    float ghost_speed = (s_frightened_timer > 0.0f) ? 2.1f : 3.0f;
    GhostEntity* ghosts[2] = { &s_blinky, &s_pinky };

    for (int g = 0; g < 2; g++) {
        move_entity(&ghosts[g]->x, &ghosts[g]->y, ghosts[g]->dir, ghost_speed, dt);
        int gc = (int)roundf(ghosts[g]->x);
        int gr = (int)roundf(ghosts[g]->y);
        float dist_to_center = hypotf(ghosts[g]->x - (float)gc, ghosts[g]->y - (float)gr);

        if (dist_to_center < 0.15f) {
            PacDir candidates[4] = { PAC_DIR_UP, PAC_DIR_LEFT, PAC_DIR_DOWN, PAC_DIR_RIGHT };
            PacDir opposite = PAC_DIR_NONE;
            if (ghosts[g]->dir == PAC_DIR_RIGHT) opposite = PAC_DIR_LEFT;
            else if (ghosts[g]->dir == PAC_DIR_LEFT) opposite = PAC_DIR_RIGHT;
            else if (ghosts[g]->dir == PAC_DIR_UP) opposite = PAC_DIR_DOWN;
            else if (ghosts[g]->dir == PAC_DIR_DOWN) opposite = PAC_DIR_UP;

            PacDir valid_dirs[4];
            int valid_count = 0;

            for (int d = 0; d < 4; d++) {
                if (candidates[d] == opposite) continue;
                int tc = gc; int tr = gr;
                if (candidates[d] == PAC_DIR_RIGHT) tc++;
                else if (candidates[d] == PAC_DIR_LEFT) tc--;
                else if (candidates[d] == PAC_DIR_DOWN) tr++;
                else if (candidates[d] == PAC_DIR_UP) tr--;

                if (is_tile_walkable(tc, tr)) {
                    valid_dirs[valid_count++] = candidates[d];
                }
            }

            // Dead-end fallback: allow opposite so ghost NEVER freezes
            if (valid_count == 0 && opposite != PAC_DIR_NONE) {
                valid_dirs[valid_count++] = opposite;
            }

            if (valid_count > 0) {
                float target_x = s_pacman.x;
                float target_y = s_pacman.y;
                if (g == 1 && !ghosts[g]->frightened) {
                    // Pinky targets 2 tiles ahead of Pacman
                    if (s_pacman.dir == PAC_DIR_RIGHT) target_x += 2.0f;
                    else if (s_pacman.dir == PAC_DIR_LEFT) target_x -= 2.0f;
                    else if (s_pacman.dir == PAC_DIR_UP) target_y -= 2.0f;
                    else if (s_pacman.dir == PAC_DIR_DOWN) target_y += 2.0f;
                }

                PacDir best_dir = valid_dirs[0];
                float best_metric = ghosts[g]->frightened ? -9999.0f : 9999.0f;

                for (int i = 0; i < valid_count; i++) {
                    int tc = gc; int tr = gr;
                    if (valid_dirs[i] == PAC_DIR_RIGHT) tc++;
                    else if (valid_dirs[i] == PAC_DIR_LEFT) tc--;
                    else if (valid_dirs[i] == PAC_DIR_DOWN) tr++;
                    else if (valid_dirs[i] == PAC_DIR_UP) tr--;

                    float dist_sq = ((float)tc - target_x) * ((float)tc - target_x) +
                                    ((float)tr - target_y) * ((float)tr - target_y);

                    if (ghosts[g]->frightened) {
                        if (dist_sq > best_metric) {
                            best_metric = dist_sq;
                            best_dir = valid_dirs[i];
                        }
                    } else {
                        if (dist_sq < best_metric) {
                            best_metric = dist_sq;
                            best_dir = valid_dirs[i];
                        }
                    }
                }
                ghosts[g]->dir = best_dir;
            }
        }

        // Collision with Pacman
        float dist_to_pac = hypotf(ghosts[g]->x - s_pacman.x, ghosts[g]->y - s_pacman.y);
        if (dist_to_pac < 0.65f) {
            if (ghosts[g]->frightened && !ghosts[g]->eaten) {
                // Eat ghost!
                ghosts[g]->eaten = true;
                ghosts[g]->frightened = false;
                ghosts[g]->x = 9.0f; ghosts[g]->y = 4.0f; // Return to house
                ghosts[g]->dir = PAC_DIR_UP;
                s_score += 200;
                play_sound_sequence(s_pac_eat_ghost, (int)(sizeof(s_pac_eat_ghost) / sizeof(s_pac_eat_ghost[0])));
                if (g_api->set_led) g_api->set_led(0, 90, 0); // Green flash
            } else if (!ghosts[g]->eaten) {
                // Pacman dies!
                s_lives--;
                play_sound_sequence(s_pac_death, (int)(sizeof(s_pac_death) / sizeof(s_pac_death[0])));
                if (g_api->set_led) g_api->set_led(90, 0, 0); // Red LED
                if (s_lives <= 0) {
                    s_game_over = true;
                    save_high_score();
                } else {
                    s_death_timer = 1.6f; // Pause for death sound/dissolve
                }
                return;
            }
        }
    }
}

void pacman_render(void) {
    if (!g_api) return;
    g_api->clear_screen();

    // Top Status HUD
    char hud[32];
    snprintf(hud, sizeof(hud), "P1:%04d  HI:%04d", s_score, s_high_score);
    g_api->draw_string(2, 0, hud, 0);

    // Draw Maze Walls & Dots
    bool pellet_flash = ((int)(s_flash_timer * 4.0f)) % 2 == 0;
    for (int r = 0; r < MAZE_ROWS; r++) {
        for (int c = 0; c < MAZE_COLS; c++) {
            int16_t px = OFFSET_X + c * CELL_SZ;
            int16_t py = OFFSET_Y + r * CELL_SZ;
            uint8_t tile = s_maze[r][c];

            if (tile == 1) {
                // Distinct double-lined wall brick
                g_api->draw_rect(px, py, CELL_SZ, CELL_SZ, 1, false);
            } else if (tile == 0) {
                // Normal Dot: sharp center pixel
                g_api->draw_pixel(px + CELL_SZ / 2, py + CELL_SZ / 2, 1);
            } else if (tile == 2) {
                // Power Pellet: pulsing circle
                if (pellet_flash) {
                    g_api->draw_circle(px + CELL_SZ / 2, py + CELL_SZ / 2, 2, 1, true);
                } else {
                    g_api->draw_circle(px + CELL_SZ / 2, py + CELL_SZ / 2, 1, 1, true);
                }
            }
        }
    }

    // Draw Distinct Animated Pac-Man Character
    int16_t pac_cx = OFFSET_X + (int16_t)(s_pacman.x * (float)CELL_SZ) + CELL_SZ / 2;
    int16_t pac_cy = OFFSET_Y + (int16_t)(s_pacman.y * (float)CELL_SZ) + CELL_SZ / 2;

    if (s_death_timer > 0.0f) {
        // Dissolve shrinking animation during death
        int r = (int)(s_death_timer * 1.8f);
        if (r > 2) r = 2;
        if (r > 0) g_api->draw_circle(pac_cx, pac_cy, r, 1, true);
    } else {
        // Solid circular body (radius 2)
        g_api->draw_circle(pac_cx, pac_cy, 2, 1, true);

        // Directional Animated Mouth
        if (s_pacman.mouth_anim == 1 || s_pacman.mouth_anim == 2) {
            // Cut out wedge facing moving direction
            if (s_pacman.dir == PAC_DIR_RIGHT) {
                g_api->draw_pixel(pac_cx + 2, pac_cy, 0);
                if (s_pacman.mouth_anim == 2) {
                    g_api->draw_pixel(pac_cx + 1, pac_cy, 0);
                    g_api->draw_pixel(pac_cx + 2, pac_cy - 1, 0);
                    g_api->draw_pixel(pac_cx + 2, pac_cy + 1, 0);
                }
            } else if (s_pacman.dir == PAC_DIR_LEFT) {
                g_api->draw_pixel(pac_cx - 2, pac_cy, 0);
                if (s_pacman.mouth_anim == 2) {
                    g_api->draw_pixel(pac_cx - 1, pac_cy, 0);
                    g_api->draw_pixel(pac_cx - 2, pac_cy - 1, 0);
                    g_api->draw_pixel(pac_cx - 2, pac_cy + 1, 0);
                }
            } else if (s_pacman.dir == PAC_DIR_UP) {
                g_api->draw_pixel(pac_cx, pac_cy - 2, 0);
                if (s_pacman.mouth_anim == 2) {
                    g_api->draw_pixel(pac_cx, pac_cy - 1, 0);
                    g_api->draw_pixel(pac_cx - 1, pac_cy - 2, 0);
                    g_api->draw_pixel(pac_cx + 1, pac_cy - 2, 0);
                }
            } else if (s_pacman.dir == PAC_DIR_DOWN) {
                g_api->draw_pixel(pac_cx, pac_cy + 2, 0);
                if (s_pacman.mouth_anim == 2) {
                    g_api->draw_pixel(pac_cx, pac_cy + 1, 0);
                    g_api->draw_pixel(pac_cx - 1, pac_cy + 2, 0);
                    g_api->draw_pixel(pac_cx + 1, pac_cy + 2, 0);
                }
            }
        }
    }

    // Draw Ghosts (Blinky & Pinky)
    GhostEntity* ghosts[2] = { &s_blinky, &s_pinky };
    for (int g = 0; g < 2; g++) {
        int16_t gx = OFFSET_X + (int16_t)(ghosts[g]->x * (float)CELL_SZ) + 1;
        int16_t gy = OFFSET_Y + (int16_t)(ghosts[g]->y * (float)CELL_SZ) + 1;

        if (ghosts[g]->frightened) {
            // Frightened wavy ghost (flashing when timer is low)
            bool flash = (s_frightened_timer < 2.0f) && (((int)(s_flash_timer * 6.0f)) % 2 == 0);
            g_api->draw_circle(gx + 2, gy + 1, 2, 1, flash);
            g_api->draw_line(gx, gy + 4, gx + 4, gy + 4, 1);
            // Squiggly mouth
            g_api->draw_pixel(gx + 1, gy + 2, flash ? 0 : 1);
            g_api->draw_pixel(gx + 2, gy + 3, flash ? 0 : 1);
            g_api->draw_pixel(gx + 3, gy + 2, flash ? 0 : 1);
        } else {
            // Classic Ghost dome & eyes
            g_api->draw_circle(gx + 2, gy + 1, 2, 1, true);
            // Skirt feet
            g_api->draw_line(gx, gy + 4, gx + 4, gy + 4, 1);
            g_api->draw_pixel(gx + 1, gy + 4, 0);
            g_api->draw_pixel(gx + 3, gy + 4, 0);

            // Pupil gaze tracking ghost direction
            int16_t ex1 = gx + 1, ey1 = gy + 1;
            int16_t ex2 = gx + 3, ey2 = gy + 1;
            if (ghosts[g]->dir == PAC_DIR_LEFT) { ex1--; ex2--; }
            else if (ghosts[g]->dir == PAC_DIR_RIGHT) { ex1++; ex2++; }
            else if (ghosts[g]->dir == PAC_DIR_UP) { ey1--; ey2--; }
            else if (ghosts[g]->dir == PAC_DIR_DOWN) { ey1++; ey2++; }
            g_api->draw_pixel(ex1, ey1, 0);
            g_api->draw_pixel(ex2, ey2, 0);
        }
    }

    // Top-right Lives indicators (Mini Pac-Man icons)
    for (int l = 0; l < s_lives; l++) {
        g_api->draw_circle(106 + l * 7, 3, 2, 1, true);
        g_api->draw_pixel(106 + l * 7 - 2, 3, 0); // Open mouth left
    }

    // Overlays & Game States
    if (s_intro_timer > 0.0f) {
        g_api->draw_rect(34, 24, 60, 16, 0, true);
        g_api->draw_rect(34, 24, 60, 16, 1, false);
        g_api->draw_string(46, 28, "READY!", 0);
    } else if (s_game_over) {
        g_api->draw_rect(24, 20, 80, 24, 0, true);
        g_api->draw_rect(24, 20, 80, 24, 1, false);
        g_api->draw_string(36, 24, "GAME OVER", 0);
        g_api->draw_string(28, 33, "[OK] RESTART", 0);
    } else if (s_level_clear) {
        g_api->draw_rect(24, 20, 80, 24, 0, true);
        g_api->draw_rect(24, 20, 80, 24, 1, false);
        g_api->draw_string(30, 24, "MAZE CLEARED!", 0);
        g_api->draw_string(28, 33, "[OK] NEXT LEVEL", 0);
    }

    g_api->flush_display();
}

void pacman_on_button(uint8_t btn, QButtonEvent evt) {
    if (evt != QEVT_BTN_DOWN && evt != QEVT_BTN_SHORT_CLICK) return;

    if (btn == QBTN_CANCEL) {
        if (g_api && g_api->exit_app) g_api->exit_app();
        return;
    }

    if (s_intro_timer > 0.0f) {
        if (btn == QBTN_OK) {
            s_intro_timer = 0.0f; // Skip intro directly to gameplay
            s_sound_seq = NULL;
            if (g_api->stop_tone) g_api->stop_tone();
        }
        return;
    }

    if (s_game_over || s_level_clear) {
        if (btn == QBTN_OK) {
            if (s_level_clear) {
                reset_maze();
                s_level_clear = false;
                s_intro_timer = 1.5f;
            } else {
                pacman_init(g_api);
            }
        }
        return;
    }

    // Re-tare neutral wrist tilt reference on OK press during game
    if (btn == QBTN_OK) {
        s_tare_done = false;
        if (g_api->play_tone) g_api->play_tone(880, 20);
        return;
    }

    // UP button turns counter-clockwise:
    // RIGHT -> UP, UP -> LEFT, LEFT -> DOWN, DOWN -> RIGHT
    if (btn == QBTN_UP) {
        if (s_pacman.dir == PAC_DIR_RIGHT)      s_pacman.next_dir = PAC_DIR_UP;
        else if (s_pacman.dir == PAC_DIR_UP)    s_pacman.next_dir = PAC_DIR_LEFT;
        else if (s_pacman.dir == PAC_DIR_LEFT)  s_pacman.next_dir = PAC_DIR_DOWN;
        else if (s_pacman.dir == PAC_DIR_DOWN)  s_pacman.next_dir = PAC_DIR_RIGHT;
        s_btn_override_timer = 0.6f;
        if (g_api->play_tone) g_api->play_tone(440, 10);
    }
    // DOWN button turns clockwise:
    // RIGHT -> DOWN, DOWN -> LEFT, LEFT -> UP, UP -> RIGHT
    else if (btn == QBTN_DOWN) {
        if (s_pacman.dir == PAC_DIR_RIGHT)      s_pacman.next_dir = PAC_DIR_DOWN;
        else if (s_pacman.dir == PAC_DIR_DOWN)  s_pacman.next_dir = PAC_DIR_LEFT;
        else if (s_pacman.dir == PAC_DIR_LEFT)  s_pacman.next_dir = PAC_DIR_UP;
        else if (s_pacman.dir == PAC_DIR_UP)    s_pacman.next_dir = PAC_DIR_RIGHT;
        s_btn_override_timer = 0.6f;
        if (g_api->play_tone) g_api->play_tone(440, 10);
    }
}

void pacman_teardown(void) {
    if (g_api) {
        if (g_api->set_led)   g_api->set_led(0, 0, 0);
        if (g_api->stop_tone) g_api->stop_tone();
    }
    save_high_score();
    g_api = NULL;
}

int  pacman_get_score(void)      { return s_score; }
int  pacman_get_lives(void)      { return s_lives; }
int  pacman_get_dots_left(void)  { return s_dots_left; }
bool pacman_is_game_over(void)   { return s_game_over; }
