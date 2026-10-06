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
#define OFFSET_Y  9
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

static const uint8_t s_maze_template[MAZE_ROWS][MAZE_COLS] = {
    { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
    { 1,2,0,0,1,0,0,0,1,1,1,0,0,0,1,0,0,2,1 },
    { 1,0,1,0,1,0,1,0,0,0,0,0,1,0,1,0,1,0,1 },
    { 0,0,1,0,0,0,1,1,3,3,1,1,0,0,0,1,0,0,0 }, // Side tunnel wrap at row 3!
    { 1,0,1,0,1,0,1,3,3,3,3,1,0,1,0,1,0,1,1 },
    { 1,0,0,0,1,0,1,1,1,1,1,1,0,1,0,0,0,0,1 },
    { 1,0,1,0,0,0,0,0,0,1,0,0,0,0,0,1,0,1,1 },
    { 1,2,0,0,1,1,1,0,0,0,0,1,1,1,0,0,0,2,1 },
    { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 }
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

static float s_neutral_roll = 0.0f;
static float s_neutral_pitch = 0.0f;
static bool  s_tare_done = false;
static float s_btn_override_timer = 0.0f;

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

    // Pacman start at center bottom
    s_pacman.x = 9.0f;
    s_pacman.y = 6.0f;
    s_pacman.dir = PAC_DIR_LEFT;
    s_pacman.next_dir = PAC_DIR_LEFT;
    s_pacman.mouth_anim = 0;

    // Ghosts start in center room
    s_blinky.x = 9.0f;
    s_blinky.y = 3.0f;
    s_blinky.dir = PAC_DIR_UP;
    s_blinky.frightened = false;
    s_blinky.eaten = false;

    s_pinky.x = 8.0f;
    s_pinky.y = 4.0f;
    s_pinky.dir = PAC_DIR_RIGHT;
    s_pinky.frightened = false;
    s_pinky.eaten = false;

    s_frightened_timer = 0.0f;
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

    load_high_score();
    reset_maze();

    if (g_api->set_led)   g_api->set_led(80, 80, 0); // Yellow Pacman LED
    if (g_api->play_tone) g_api->play_tone(587, 80);

    return QAPP_OK;
}

static bool is_tile_walkable(int c, int r) {
    if (r == 3 && (c < 0 || c >= MAZE_COLS)) return true; // Wrap-around tunnel
    if (c < 0 || c >= MAZE_COLS || r < 0 || r >= MAZE_ROWS) return false;
    return (s_maze[r][c] != 1);
}

static void move_entity(float* x, float* y, PacDir dir, float speed, float dt) {
    float nx = *x;
    float ny = *y;
    if (dir == PAC_DIR_RIGHT) nx += speed * dt;
    else if (dir == PAC_DIR_LEFT)  nx -= speed * dt;
    else if (dir == PAC_DIR_DOWN)  ny += speed * dt;
    else if (dir == PAC_DIR_UP)    ny -= speed * dt;

    // Wrap around horizontal tunnel
    if (ny >= 2.8f && ny <= 3.2f) {
        if (nx < -0.5f) nx = (float)MAZE_COLS - 0.5f;
        else if (nx > (float)MAZE_COLS - 0.5f) nx = -0.5f;
    }

    // Check collision with tile center
    int next_c = (int)roundf(nx);
    int next_r = (int)roundf(ny);
    if (is_tile_walkable(next_c, next_r)) {
        *x = nx;
        *y = ny;
    }
}

void pacman_update(float dt) {
    if (!g_api) return;
    if (dt <= 0.0f || dt > 0.2f) dt = 0.016f;

    // Tare neutral wrist angle on first frame
    if (!s_tare_done) {
        QTelemetry telem;
        g_api->get_telemetry(&telem);
        s_neutral_roll = telem.roll;
        s_neutral_pitch = telem.pitch;
        s_tare_done = true;
    }

    if (s_game_over || s_level_clear) return;

    // Button override timer
    if (s_btn_override_timer > 0.0f) {
        s_btn_override_timer -= dt;
    } else {
        // Unified Sensor Calibration tilt buffer
        QTelemetry telem;
        g_api->get_telemetry(&telem);
        float d_roll = telem.roll - s_neutral_roll;
        float d_pitch = telem.pitch - s_neutral_pitch;

        if (fabsf(d_roll) > 16.0f || fabsf(d_pitch) > 16.0f) {
            if (fabsf(d_roll) > fabsf(d_pitch)) {
                if (d_roll > 16.0f)  s_pacman.next_dir = PAC_DIR_RIGHT;
                else if (d_roll < -16.0f) s_pacman.next_dir = PAC_DIR_LEFT;
            } else {
                if (d_pitch > 16.0f) s_pacman.next_dir = PAC_DIR_DOWN;
                else if (d_pitch < -16.0f) s_pacman.next_dir = PAC_DIR_UP;
            }
        }
    }

    // Frightened ghost timer
    if (s_frightened_timer > 0.0f) {
        s_frightened_timer -= dt;
        if (s_frightened_timer <= 0.0f) {
            s_blinky.frightened = false;
            s_pinky.frightened = false;
            if (g_api->set_led) g_api->set_led(80, 80, 0); // Back to yellow
        }
    }

    // Test if pacman can turn into next_dir
    int cur_c = (int)roundf(s_pacman.x);
    int cur_r = (int)roundf(s_pacman.y);
    int target_c = cur_c;
    int target_r = cur_r;
    if (s_pacman.next_dir == PAC_DIR_RIGHT) target_c++;
    else if (s_pacman.next_dir == PAC_DIR_LEFT)  target_c--;
    else if (s_pacman.next_dir == PAC_DIR_DOWN)  target_r++;
    else if (s_pacman.next_dir == PAC_DIR_UP)    target_r--;

    if (is_tile_walkable(target_c, target_r)) {
        s_pacman.dir = s_pacman.next_dir;
    }

    // Move Pacman
    float pac_speed = 3.6f; // cells per second
    move_entity(&s_pacman.x, &s_pacman.y, s_pacman.dir, pac_speed, dt);
    s_pacman.mouth_anim = ((int)(s_pacman.x * 4.0f + s_pacman.y * 4.0f)) % 4;

    // Eat dots & power pellets
    int pc = (int)roundf(s_pacman.x);
    int pr = (int)roundf(s_pacman.y);
    if (pc >= 0 && pc < MAZE_COLS && pr >= 0 && pr < MAZE_ROWS) {
        if (s_maze[pr][pc] == 0) {
            // Normal dot
            s_maze[pr][pc] = 3;
            s_score += 10;
            s_dots_left--;
            s_waka_phase = !s_waka_phase;
            if (g_api->play_tone) g_api->play_tone(s_waka_phase ? 494 : 330, 20);
            if (s_score > s_high_score) s_high_score = s_score;
        } else if (s_maze[pr][pc] == 2) {
            // Power Pellet!
            s_maze[pr][pc] = 3;
            s_score += 50;
            s_dots_left--;
            s_frightened_timer = 7.0f;
            s_blinky.frightened = true;
            s_pinky.frightened = true;
            s_blinky.eaten = false;
            s_pinky.eaten = false;
            if (g_api->play_tone) g_api->play_tone(880, 80);
            if (g_api->set_led)   g_api->set_led(0, 0, 90); // Blue frightened LED
            if (s_score > s_high_score) s_high_score = s_score;
        }
    }

    // Level Clear Check
    if (s_dots_left <= 0) {
        s_level_clear = true;
        if (g_api->play_tone) g_api->play_tone(1318, 150);
        save_high_score();
        return;
    }

    // Move Ghosts
    float ghost_speed = (s_frightened_timer > 0.0f) ? 2.0f : 3.0f;
    // Simple ghost AI navigation
    GhostEntity* ghosts[2] = { &s_blinky, &s_pinky };
    for (int g = 0; g < 2; g++) {
        move_entity(&ghosts[g]->x, &ghosts[g]->y, ghosts[g]->dir, ghost_speed, dt);
        int gc = (int)roundf(ghosts[g]->x);
        int gr = (int)roundf(ghosts[g]->y);

        // If ghost reached center of tile, decide next direction
        if (fabsf(ghosts[g]->x - (float)gc) < 0.1f && fabsf(ghosts[g]->y - (float)gr) < 0.1f) {
            PacDir candidates[4] = { PAC_DIR_UP, PAC_DIR_DOWN, PAC_DIR_LEFT, PAC_DIR_RIGHT };
            PacDir opposite = PAC_DIR_NONE;
            if (ghosts[g]->dir == PAC_DIR_RIGHT) opposite = PAC_DIR_LEFT;
            else if (ghosts[g]->dir == PAC_DIR_LEFT) opposite = PAC_DIR_RIGHT;
            else if (ghosts[g]->dir == PAC_DIR_UP) opposite = PAC_DIR_DOWN;
            else if (ghosts[g]->dir == PAC_DIR_DOWN) opposite = PAC_DIR_UP;

            PacDir best_dir = ghosts[g]->dir;
            float min_dist = 9999.0f;
            for (int d = 0; d < 4; d++) {
                if (candidates[d] == opposite) continue;
                int tc = gc; int tr = gr;
                if (candidates[d] == PAC_DIR_RIGHT) tc++;
                else if (candidates[d] == PAC_DIR_LEFT) tc--;
                else if (candidates[d] == PAC_DIR_DOWN) tr++;
                else if (candidates[d] == PAC_DIR_UP) tr--;

                if (is_tile_walkable(tc, tr)) {
                    float dist = 0.0f;
                    if (ghosts[g]->frightened) {
                        // Move away from pacman
                        dist = -hypotf((float)tc - s_pacman.x, (float)tr - s_pacman.y);
                    } else {
                        // Move towards pacman
                        dist = hypotf((float)tc - s_pacman.x, (float)tr - s_pacman.y);
                    }
                    if (dist < min_dist) {
                        min_dist = dist;
                        best_dir = candidates[d];
                    }
                }
            }
            ghosts[g]->dir = best_dir;
        }

        // Ghost collision with Pacman
        float dist_to_pac = hypotf(ghosts[g]->x - s_pacman.x, ghosts[g]->y - s_pacman.y);
        if (dist_to_pac < 0.75f) {
            if (ghosts[g]->frightened && !ghosts[g]->eaten) {
                // Eat ghost!
                ghosts[g]->eaten = true;
                ghosts[g]->frightened = false;
                ghosts[g]->x = 9.0f; ghosts[g]->y = 4.0f; // Return to cage
                s_score += 200;
                if (g_api->play_tone) g_api->play_tone(1568, 80);
                if (g_api->set_led)   g_api->set_led(0, 90, 0); // Green flash
            } else if (!ghosts[g]->eaten) {
                // Pacman dies!
                s_lives--;
                if (g_api->play_tone) g_api->play_tone(110, 200);
                if (g_api->set_led)   g_api->set_led(90, 0, 0); // Red
                if (s_lives <= 0) {
                    s_game_over = true;
                    save_high_score();
                } else {
                    // Reset positions
                    s_pacman.x = 9.0f; s_pacman.y = 6.0f;
                    s_blinky.x = 9.0f; s_blinky.y = 3.0f;
                    s_pinky.x = 8.0f; s_pinky.y = 4.0f;
                }
                return;
            }
        }
    }
}

void pacman_render(void) {
    if (!g_api) return;
    g_api->clear_screen();

    // Top HUD
    char hud[32];
    snprintf(hud, sizeof(hud), "P1:%04d  HI:%04d", s_score, s_high_score);
    g_api->draw_string(2, 0, hud, 0);

    // Draw Maze Walls & Dots
    for (int r = 0; r < MAZE_ROWS; r++) {
        for (int c = 0; c < MAZE_COLS; c++) {
            int16_t px = OFFSET_X + c * CELL_SZ;
            int16_t py = OFFSET_Y + r * CELL_SZ;
            uint8_t tile = s_maze[r][c];

            if (tile == 1) {
                // Wall brick
                g_api->draw_rect(px, py, CELL_SZ, CELL_SZ, 1, false);
            } else if (tile == 0) {
                // Normal Dot (1x1 pixel)
                g_api->draw_pixel(px + 2, py + 2, 1);
            } else if (tile == 2) {
                // Power Pellet (3x3 circle)
                g_api->draw_circle(px + 2, py + 2, 2, 1, true);
            }
        }
    }

    // Draw Pacman (5x5 circular character with directional mouth)
    int16_t pac_px = OFFSET_X + (int16_t)(s_pacman.x * (float)CELL_SZ) + 2;
    int16_t pac_py = OFFSET_Y + (int16_t)(s_pacman.y * (float)CELL_SZ) + 2;
    g_api->draw_circle(pac_px, pac_py, 2, 1, true);
    if (s_pacman.mouth_anim == 1 || s_pacman.mouth_anim == 2) {
        if (s_pacman.dir == PAC_DIR_RIGHT) g_api->draw_pixel(pac_px + 2, pac_py, 0);
        else if (s_pacman.dir == PAC_DIR_LEFT)  g_api->draw_pixel(pac_px - 2, pac_py, 0);
        else if (s_pacman.dir == PAC_DIR_UP)    g_api->draw_pixel(pac_px, pac_py - 2, 0);
        else if (s_pacman.dir == PAC_DIR_DOWN)  g_api->draw_pixel(pac_px, pac_py + 2, 0);
    }

    // Draw Ghosts
    GhostEntity* ghosts[2] = { &s_blinky, &s_pinky };
    for (int g = 0; g < 2; g++) {
        int16_t gx = OFFSET_X + (int16_t)(ghosts[g]->x * (float)CELL_SZ);
        int16_t gy = OFFSET_Y + (int16_t)(ghosts[g]->y * (float)CELL_SZ);
        // Ghost dome top (4x4)
        g_api->draw_circle(gx + 2, gy + 1, 2, 1, !ghosts[g]->frightened);
        // Skirt feet
        g_api->draw_line(gx, gy + 4, gx + 4, gy + 4, 1);
        // Eyes
        g_api->draw_pixel(gx + 1, gy + 1, ghosts[g]->frightened ? 1 : 0);
        g_api->draw_pixel(gx + 3, gy + 1, ghosts[g]->frightened ? 1 : 0);
    }

    // Bottom Lives
    for (int l = 0; l < s_lives; l++) {
        g_api->draw_circle(100 + l * 8, 3, 2, 1, true);
    }

    // Overlays
    if (s_game_over) {
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

    if (s_game_over || s_level_clear) {
        if (btn == QBTN_OK) {
            if (s_level_clear) {
                reset_maze();
                s_level_clear = false;
            } else {
                pacman_init(g_api);
            }
        }
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
