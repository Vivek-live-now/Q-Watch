#include "dice.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const QWatchAPI* g_api = NULL;

#define HISTORY_LEN 4

static DiceMode s_mode = DICE_MODE_D6;
static bool     s_is_rolling = false;
static float    s_roll_timer = 0.0f;
static float    s_roll_duration = 1.0f;
static float    s_tick_timer = 0.0f;
static float    s_tick_interval = 0.03f;

// Dice values
static int  s_current_value = 1;
static int  s_die1 = 1;
static int  s_die2 = 1;
static int  s_die3 = 1;
static int  s_history[HISTORY_LEN] = { 0 };
static int  s_hist_head = 0;

// Shake detection
static float s_last_accel_mag = 1.0f;
static float s_shake_cooldown = 0.0f;

// Coin animation
static uint8_t s_coin_frame = 0;

static const char* s_mode_names[DICE_MODE_COUNT] = {
    "D6", "D20", "D100", "COIN", "2D6", "3D6", "D4", "D8", "D12"
};

// Forward declarations
static int generate_roll_for_mode(DiceMode mode);
static void apply_roll_result(int val);

// QApp Header descriptor
static const QAppHeader s_dice_header = {
    .magic = QAPP_MAGIC,
    .api_version = QAPP_API_VERSION,
    .required_caps = (QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_MPU | QAPP_CAP_AUDIO | QAPP_CAP_RGB_LED),
    .name = "Tactical Dice",
    .version = "1.0.0",
    .author = "MI6 Cyber",
    .required_psram = 2048,
    .init = dice_init,
    .update = dice_update,
    .render = dice_render,
    .on_button = dice_on_button,
    .teardown = dice_teardown
};

const QAppHeader* get_dice_header(void) {
    return &s_dice_header;
}

static uint32_t get_entropy_seed(void) {
    uint32_t seed = 0x5A17C3D5;
    if (g_api) {
        if (g_api->micros) seed ^= g_api->micros();
        if (g_api->random_range) seed ^= g_api->random_range(1, 0xFFFFFF);
    }
    return seed;
}

static int roll_range(int min_val, int max_val) {
    if (!g_api || !g_api->random_range) {
        uint32_t s = get_entropy_seed();
        return min_val + (int)(s % (uint32_t)(max_val - min_val + 1));
    }
    return (int)g_api->random_range((uint32_t)min_val, (uint32_t)max_val);
}

static int generate_roll_for_mode(DiceMode mode) {
    switch (mode) {
        case DICE_MODE_D6:   return roll_range(1, 6);
        case DICE_MODE_D20:  return roll_range(1, 20);
        case DICE_MODE_D100: return roll_range(1, 100);
        case DICE_MODE_COIN: return roll_range(0, 1); // 0 = Heads, 1 = Tails
        case DICE_MODE_2D6: {
            s_die1 = roll_range(1, 6);
            s_die2 = roll_range(1, 6);
            return s_die1 + s_die2;
        }
        case DICE_MODE_3D6: {
            s_die1 = roll_range(1, 6);
            s_die2 = roll_range(1, 6);
            s_die3 = roll_range(1, 6);
            return s_die1 + s_die2 + s_die3;
        }
        case DICE_MODE_D4:   return roll_range(1, 4);
        case DICE_MODE_D8:   return roll_range(1, 8);
        case DICE_MODE_D12:  return roll_range(1, 12);
        default:             return 1;
    }
}

int dice_init(const QWatchAPI* api) {
    if (!api) return QAPP_ERR_INVALID_PARAM;
    g_api = api;

    s_mode = DICE_MODE_D6;
    s_is_rolling = false;
    s_roll_timer = 0.0f;
    s_current_value = 6;
    s_die1 = 3;
    s_die2 = 3;
    s_die3 = 3;
    s_shake_cooldown = 0.5f;

    for (int i = 0; i < HISTORY_LEN; i++) s_history[i] = 0;
    s_hist_head = 0;

    if (g_api->set_led) {
        g_api->set_led(40, 30, 0); // Warm amber dice LED
    }
    if (g_api->play_tone) {
        g_api->play_tone(1200, 40);
    }
    return QAPP_OK;
}

void dice_trigger_roll(void) {
    if (s_is_rolling) return;
    s_is_rolling = true;
    s_roll_timer = 0.0f;
    s_roll_duration = 0.9f + (float)(g_api && g_api->random_range ? g_api->random_range(0, 40) : 20) * 0.01f;
    s_tick_timer = 0.0f;
    s_tick_interval = 0.03f;

    if (g_api && g_api->set_led) {
        g_api->set_led(20, 20, 0); // Dim rolling amber
    }
}

static void apply_roll_result(int val) {
    s_current_value = val;
    s_history[s_hist_head] = val;
    s_hist_head = (s_hist_head + 1) % HISTORY_LEN;

    if (!g_api) return;

    if (s_mode == DICE_MODE_D20) {
        if (val == 20) {
            // Natural 20 Crit
            if (g_api->set_led)   g_api->set_led(0, 120, 50); // Emerald Green
            if (g_api->play_tone) g_api->play_tone(1800, 100);
        } else if (val == 1) {
            // Natural 1 Fumble
            if (g_api->set_led)   g_api->set_led(120, 0, 0); // Crimson Red
            if (g_api->play_tone) g_api->play_tone(150, 120);
        } else {
            if (g_api->set_led)   g_api->set_led(0, 40, 80); // Cyan
            if (g_api->play_tone) g_api->play_tone(1000, 50);
        }
    } else if (s_mode == DICE_MODE_COIN) {
        if (val == 0) {
            if (g_api->set_led)   g_api->set_led(0, 60, 100); // Heads Cyan
        } else {
            if (g_api->set_led)   g_api->set_led(100, 60, 0); // Tails Gold
        }
        if (g_api->play_tone) g_api->play_tone(1200, 60);
    } else {
        if (g_api->set_led)   g_api->set_led(0, 50, 80);
        if (g_api->play_tone) g_api->play_tone(1100, 50);
    }
}

void dice_update(float dt) {
    if (!g_api) return;
    if (dt <= 0.0f || dt > 0.2f) dt = 0.016f;

    // 1. Shake detection using IMU magnitude
    QTelemetry telem;
    memset(&telem, 0, sizeof(telem));
    g_api->get_telemetry(&telem);

    float mag = sqrtf(telem.accel_x * telem.accel_x +
                      telem.accel_y * telem.accel_y +
                      telem.accel_z * telem.accel_z);

    if (s_shake_cooldown > 0.0f) {
        s_shake_cooldown -= dt;
    } else {
        if (fabsf(mag - s_last_accel_mag) > 0.9f || mag > 1.85f) {
            if (!s_is_rolling) {
                dice_trigger_roll();
                s_shake_cooldown = 1.2f;
            }
        }
    }
    s_last_accel_mag = mag;

    // 2. Rolling physics and tumble animation
    if (s_is_rolling) {
        s_roll_timer += dt;
        s_tick_timer += dt;

        // Decelerating tick interval
        float progress = s_roll_timer / s_roll_duration;
        s_tick_interval = 0.03f + progress * 0.18f;

        if (s_tick_timer >= s_tick_interval) {
            s_tick_timer = 0.0f;
            s_current_value = generate_roll_for_mode(s_mode);
            s_coin_frame = (s_coin_frame + 1) % 4;

            // Clicking sound
            if (g_api->play_tone) {
                uint16_t freq = 1400 - (uint16_t)(progress * 600.0f);
                g_api->play_tone(freq, 12);
            }
        }

        if (s_roll_timer >= s_roll_duration) {
            s_is_rolling = false;
            apply_roll_result(s_current_value);
        }
    }
}

static void draw_d6_face(int16_t cx, int16_t cy, int val) {
    if (!g_api) return;
    // Draw 32x32 rounded dice box
    int16_t x0 = cx - 16;
    int16_t y0 = cy - 16;
    g_api->draw_rect(x0, y0, 32, 32, 1, false);
    g_api->draw_rect(x0 + 1, y0 + 1, 30, 30, 0, true);

    // Subtle 3D shadow line
    g_api->draw_line(x0 + 2, y0 + 31, x0 + 31, y0 + 31, 1);
    g_api->draw_line(x0 + 31, y0 + 2, x0 + 31, y0 + 31, 1);

    // Draw pips (radius 2)
    int16_t px_l = cx - 8;
    int16_t px_c = cx;
    int16_t px_r = cx + 8;
    int16_t py_t = cy - 8;
    int16_t py_c = cy;
    int16_t py_b = cy + 8;

    switch (val) {
        case 1:
            g_api->draw_circle(px_c, py_c, 3, 1, true);
            break;
        case 2:
            g_api->draw_circle(px_l, py_t, 2, 1, true);
            g_api->draw_circle(px_r, py_b, 2, 1, true);
            break;
        case 3:
            g_api->draw_circle(px_l, py_t, 2, 1, true);
            g_api->draw_circle(px_c, py_c, 2, 1, true);
            g_api->draw_circle(px_r, py_b, 2, 1, true);
            break;
        case 4:
            g_api->draw_circle(px_l, py_t, 2, 1, true);
            g_api->draw_circle(px_r, py_t, 2, 1, true);
            g_api->draw_circle(px_l, py_b, 2, 1, true);
            g_api->draw_circle(px_r, py_b, 2, 1, true);
            break;
        case 5:
            g_api->draw_circle(px_l, py_t, 2, 1, true);
            g_api->draw_circle(px_r, py_t, 2, 1, true);
            g_api->draw_circle(px_c, py_c, 2, 1, true);
            g_api->draw_circle(px_l, py_b, 2, 1, true);
            g_api->draw_circle(px_r, py_b, 2, 1, true);
            break;
        case 6:
            g_api->draw_circle(px_l, py_t, 2, 1, true);
            g_api->draw_circle(px_r, py_t, 2, 1, true);
            g_api->draw_circle(px_l, py_c, 2, 1, true);
            g_api->draw_circle(px_r, py_c, 2, 1, true);
            g_api->draw_circle(px_l, py_b, 2, 1, true);
            g_api->draw_circle(px_r, py_b, 2, 1, true);
            break;
        default:
            break;
    }
}

static void draw_coin_face(int16_t cx, int16_t cy, int val, bool rolling, uint8_t frame) {
    if (!g_api) return;
    if (rolling) {
        // Tumbling coin animation
        switch (frame) {
            case 0: // Full circle
                g_api->draw_circle(cx, cy, 14, 1, false);
                g_api->draw_line(cx - 6, cy, cx + 6, cy, 1);
                break;
            case 1: // Narrow oval
                g_api->draw_circle(cx, cy, 8, 1, false);
                break;
            case 2: // Vertical line
                g_api->draw_line(cx, cy - 14, cx, cy + 14, 1);
                g_api->draw_line(cx - 1, cy - 12, cx - 1, cy + 12, 1);
                break;
            case 3: // Narrow oval
                g_api->draw_circle(cx, cy, 8, 1, false);
                break;
        }
    } else {
        // Settled Coin
        g_api->draw_circle(cx, cy, 15, 1, false);
        g_api->draw_circle(cx, cy, 13, 1, false);
        if (val == 0) {
            // HEADS (MI6 "007" / "H")
            g_api->draw_string(cx - 7, cy - 3, "H", 1);
            g_api->draw_string(cx - 15, cy + 17, "HEADS", 0);
        } else {
            // TAILS ("T")
            g_api->draw_string(cx - 7, cy - 3, "T", 1);
            g_api->draw_string(cx - 15, cy + 17, "TAILS", 0);
        }
    }
}

void dice_render(void) {
    if (!g_api) return;
    g_api->clear_screen();

    // 1. Top HUD Mode Bar
    g_api->draw_rect(0, 0, 128, 11, 1, true);
    char mode_str[32];
    snprintf(mode_str, sizeof(mode_str), "DICE: %s", s_mode_names[s_mode]);
    g_api->draw_string(4, 2, mode_str, 0);

    const char* hint = s_is_rolling ? "ROLLING..." : "[OK] ROLL";
    g_api->draw_string(72, 2, hint, 0);

    // 2. Render Main Dice Body based on mode
    int16_t center_x = 64;
    int16_t center_y = 33;

    if (s_mode == DICE_MODE_D6) {
        draw_d6_face(center_x, center_y, s_current_value);
    } else if (s_mode == DICE_MODE_COIN) {
        draw_coin_face(center_x, center_y, s_current_value, s_is_rolling, s_coin_frame);
    } else if (s_mode == DICE_MODE_2D6) {
        draw_d6_face(center_x - 22, center_y, s_die1);
        draw_d6_face(center_x + 22, center_y, s_die2);
        char sum_str[16];
        snprintf(sum_str, sizeof(sum_str), "=%d", s_current_value);
        g_api->draw_string(center_x - 6, center_y + 16, sum_str, 0);
    } else if (s_mode == DICE_MODE_3D6) {
        // Draw 3 smaller dice
        g_api->draw_rect(center_x - 38, center_y - 10, 20, 20, 1, false);
        char d1[8]; snprintf(d1, sizeof(d1), "%d", s_die1);
        g_api->draw_string(center_x - 32, center_y - 4, d1, 0);

        g_api->draw_rect(center_x - 10, center_y - 10, 20, 20, 1, false);
        char d2[8]; snprintf(d2, sizeof(d2), "%d", s_die2);
        g_api->draw_string(center_x - 4, center_y - 4, d2, 0);

        g_api->draw_rect(center_x + 18, center_y - 10, 20, 20, 1, false);
        char d3[8]; snprintf(d3, sizeof(d3), "%d", s_die3);
        g_api->draw_string(center_x + 24, center_y - 4, d3, 0);

        char sum_str[16];
        snprintf(sum_str, sizeof(sum_str), "SUM:%d", s_current_value);
        g_api->draw_string(center_x - 14, center_y + 14, sum_str, 0);
    } else {
        // D20, D100, D4, D8, D12
        // Tactical Diamond / Polyhedral HUD Frame
        g_api->draw_line(center_x, center_y - 17, center_x + 24, center_y, 1);
        g_api->draw_line(center_x + 24, center_y, center_x, center_y + 17, 1);
        g_api->draw_line(center_x, center_y + 17, center_x - 24, center_y, 1);
        g_api->draw_line(center_x - 24, center_y, center_x, center_y - 17, 1);

        char val_buf[16];
        snprintf(val_buf, sizeof(val_buf), "%d", s_current_value);
        int16_t offset_x = (s_current_value >= 100) ? 9 : ((s_current_value >= 10) ? 6 : 3);
        g_api->draw_string(center_x - offset_x, center_y - 4, val_buf, 1);

        // Natural 20 / Natural 1 highlight banner
        if (s_mode == DICE_MODE_D20 && !s_is_rolling) {
            if (s_current_value == 20) {
                g_api->draw_string(center_x - 20, center_y + 18, "* CRIT! *", 0);
            } else if (s_current_value == 1) {
                g_api->draw_string(center_x - 22, center_y + 18, "! FUMBLE !", 0);
            }
        }
    }

    // 3. Bottom History Bar
    char hist_str[36] = "HIST:";
    for (int i = 0; i < HISTORY_LEN; i++) {
        int idx = (s_hist_head - 1 - i + HISTORY_LEN) % HISTORY_LEN;
        if (s_history[idx] > 0) {
            char item[10];
            snprintf(item, sizeof(item), " %d", s_history[idx]);
            strncat(hist_str, item, sizeof(hist_str) - strlen(hist_str) - 1);
        }
    }
    g_api->draw_string(2, 56, hist_str, 0);

    g_api->flush_display();
}

void dice_on_button(uint8_t btn, QButtonEvent evt) {
    if (evt == QEVT_BTN_DOWN || evt == QEVT_BTN_SHORT_CLICK) {
        if (btn == QBTN_UP) {
            // Cycle Mode Backward
            s_mode = (DiceMode)((s_mode + DICE_MODE_COUNT - 1) % DICE_MODE_COUNT);
            s_current_value = generate_roll_for_mode(s_mode);
            if (g_api && g_api->play_tone) g_api->play_tone(800, 20);
        } else if (btn == QBTN_DOWN) {
            // Cycle Mode Forward
            s_mode = (DiceMode)((s_mode + 1) % DICE_MODE_COUNT);
            s_current_value = generate_roll_for_mode(s_mode);
            if (g_api && g_api->play_tone) g_api->play_tone(900, 20);
        } else if (btn == QBTN_OK) {
            // Roll
            dice_trigger_roll();
        } else if (btn == QBTN_CANCEL) {
            if (g_api && g_api->exit_app) g_api->exit_app();
        }
    }
}

void dice_teardown(void) {
    if (g_api) {
        if (g_api->set_led)   g_api->set_led(0, 0, 0);
        if (g_api->stop_tone) g_api->stop_tone();
    }
    g_api = NULL;
}

// Unit test inspection helpers
DiceMode dice_get_mode(void)          { return s_mode; }
void     dice_set_mode(DiceMode mode) { s_mode = mode; }
int      dice_get_last_result(void)   { return s_current_value; }
bool     dice_is_rolling(void)        { return s_is_rolling; }
int      dice_get_history(int index)  {
    if (index < 0 || index >= HISTORY_LEN) return 0;
    return s_history[index];
}
