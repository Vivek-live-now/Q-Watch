#include "mochi_pet.h"

MochiPet mochiPet;

static const char* EMOTE_NAMES[] = {
    "HAPPY", "LOVE", "LAUGH", "EXCITED", "RELAXED", "CONTENT",
    "PROUD", "ANGRY", "FRUSTRATED", "CONFUSED", "EMBARRASSED",
    "SLEEPY", "SLEEPING", "DIZZY", "DRIVING", "MUSIC", "GUNDAM"
};

static const char* HELMET_NAMES[] = {
    "CLASSIC", "GUNDAM", "CYBER", "NEKO", "TACTICAL"
};

static inline float clampf(float val, float min_val, float max_val) {
    if (val < min_val) return min_val;
    if (val > max_val) return max_val;
    return val;
}

MochiPet::MochiPet()
    : current_emote(MochiEmote::HAPPY),
      base_emote(MochiEmote::HAPPY),
      current_helmet(MochiHelmet::CLASSIC),
      submode(MochiSubmode::INTERACTIVE),
      last_sound(MochiSound::NONE),
      happiness(85),
      hunger(80),
      friendship_level(1),
      friendship_xp(0),
      is_sleeping(false),
      is_muted(false),
      pupil_dx(0.0f),
      pupil_dy(0.0f),
      target_pupil_dx(0.0f),
      target_pupil_dy(0.0f),
      head_lean(0.0f),
      is_dizzy(false),
      dizzy_timer(0.0f),
      idle_timer(0.0f),
      hunger_timer(0.0f),
      anim_timer(0.0f),
      blink_timer(0.0f),
      is_blinking(false),
      blink_phase(0.0f),
      breath_phase(0.0f),
      heart_pulse(1.0f),
      star_orbit_angle(0.0f),
      note_float_y(0.0f),
      zzz_offset(0.0f),
      feeding_active(false),
      snack_x(120.0f),
      snack_y(38.0f),
      chew_counter(0),
      feeding_timer(0.0f),
      petting_active(false),
      petting_timer(0.0f),
      emote_override_timer(0.0f),
      picker_selection(0),
      picker_offset(0)
{
    wind_line_x[0] = 128.0f;
    wind_line_x[1] = 96.0f;
    wind_line_x[2] = 64.0f;
    wind_line_x[3] = 32.0f;
}

void MochiPet::begin() {
    updateMoodLed();
    triggerSound(MochiSound::HAPPY_CHIRP);
}

const char* MochiPet::getEmoteName(MochiEmote emote) const {
    uint8_t idx = static_cast<uint8_t>(emote);
    if (idx < static_cast<uint8_t>(MochiEmote::COUNT)) {
        return EMOTE_NAMES[idx];
    }
    return "UNKNOWN";
}

const char* MochiPet::getHelmetName(MochiHelmet helmet) const {
    uint8_t idx = static_cast<uint8_t>(helmet);
    if (idx < static_cast<uint8_t>(MochiHelmet::COUNT)) {
        return HELMET_NAMES[idx];
    }
    return "UNKNOWN";
}

void MochiPet::setEmote(MochiEmote emote, uint32_t duration_ms) {
    current_emote = emote;
    if (duration_ms > 0) {
        emote_override_timer = duration_ms / 1000.0f;
    } else {
        base_emote = emote;
        emote_override_timer = 0.0f;
    }
    updateMoodLed();
}

void MochiPet::nextEmote() {
    uint8_t next = (static_cast<uint8_t>(current_emote) + 1) % static_cast<uint8_t>(MochiEmote::COUNT);
    setEmote(static_cast<MochiEmote>(next));
}

void MochiPet::prevEmote() {
    uint8_t total = static_cast<uint8_t>(MochiEmote::COUNT);
    uint8_t prev = (static_cast<uint8_t>(current_emote) + total - 1) % total;
    setEmote(static_cast<MochiEmote>(prev));
}

void MochiPet::setHelmet(MochiHelmet helmet) {
    if (static_cast<uint8_t>(helmet) < static_cast<uint8_t>(MochiHelmet::COUNT)) {
        current_helmet = helmet;
        if (current_helmet == MochiHelmet::GUNDAM) {
            triggerSound(MochiSound::GUNDAM_BEEP);
        } else {
            triggerSound(MochiSound::HAPPY_CHIRP);
        }
        updateMoodLed();
    }
}

void MochiPet::nextHelmet() {
    uint8_t next = (static_cast<uint8_t>(current_helmet) + 1) % static_cast<uint8_t>(MochiHelmet::COUNT);
    setHelmet(static_cast<MochiHelmet>(next));
}

void MochiPet::prevHelmet() {
    uint8_t total = static_cast<uint8_t>(MochiHelmet::COUNT);
    uint8_t prev = (static_cast<uint8_t>(current_helmet) + total - 1) % total;
    setHelmet(static_cast<MochiHelmet>(prev));
}

void MochiPet::toggleHud() {
    if (submode == MochiSubmode::INTERACTIVE) {
        submode = MochiSubmode::STATS_HUD;
    } else if (submode == MochiSubmode::STATS_HUD) {
        submode = MochiSubmode::INTERACTIVE;
    }
}

void MochiPet::pet() {
    if (is_sleeping) {
        wakeUp();
        return;
    }
    petting_active = true;
    petting_timer = 1.8f;
    happiness = (happiness + 15 > 100) ? 100 : (happiness + 15);
    friendship_xp += 10;
    if (friendship_xp >= static_cast<uint16_t>(friendship_level * 100)) {
        friendship_xp -= friendship_level * 100;
        if (friendship_level < 10) friendship_level++;
    }

    setEmote(MochiEmote::LOVE, 1800);
    triggerSound(MochiSound::LOVE_CHIME);
}

void MochiPet::feed() {
    if (is_sleeping) {
        wakeUp();
        return;
    }
    feeding_active = true;
    feeding_timer = 2.5f;
    snack_x = 118.0f;
    snack_y = 38.0f;
    chew_counter = 0;
    setEmote(MochiEmote::EXCITED, 2500);
    triggerSound(MochiSound::HAPPY_CHIRP);
}

void MochiPet::toggleSleep() {
    if (is_sleeping) {
        wakeUp();
    } else {
        is_sleeping = true;
        setEmote(MochiEmote::SLEEPING);
        triggerSound(MochiSound::SNORE);
    }
}

void MochiPet::wakeUp() {
    if (is_sleeping) {
        is_sleeping = false;
        idle_timer = 0.0f;
        setEmote(MochiEmote::HAPPY, 1500);
        triggerSound(MochiSound::WAKEUP);
    }
}

void MochiPet::updatePhysics(float pitch, float roll, float ax, float ay, float az, float gx, float gy, float gz) {
    // 1. Pupil Gaze Follows Watch Tilt
    target_pupil_dx = clampf(roll * 0.25f, -7.0f, 7.0f);
    target_pupil_dy = clampf(-pitch * 0.25f, -5.0f, 5.0f);

    // Smooth filter (exponential ease)
    pupil_dx += (target_pupil_dx - pupil_dx) * 0.25f;
    pupil_dy += (target_pupil_dy - pupil_dy) * 0.25f;

    // Head banking in cornering
    head_lean = clampf(roll * 0.12f, -8.0f, 8.0f);

    // 2. Gyro motion detection (wake Mochi if moved)
    float gyro_mag = fabsf(gx) + fabsf(gy) + fabsf(gz);
    if (gyro_mag > 20.0f) {
        if (is_sleeping) {
            wakeUp();
        }
        idle_timer = 0.0f;
    }

    // 3. Shake Detection (triggers Dizzy mode)
    float accel_mag = sqrtf(ax * ax + ay * ay + az * az);
    if (accel_mag > 2.2f && !is_dizzy && !is_sleeping) {
        is_dizzy = true;
        dizzy_timer = 3.5f;
        setEmote(MochiEmote::DIZZY, 3500);
        triggerSound(MochiSound::DIZZY_STUMBLE);
    }

    // 4. Freefall / Zero-G detection
    if (accel_mag < 0.25f && !is_sleeping && !is_dizzy && current_emote != MochiEmote::CONFUSED) {
        setEmote(MochiEmote::CONFUSED, 2000);
    }
}

void MochiPet::update(float dt) {
    if (dt <= 0.0f) dt = 0.033f;
    anim_timer += dt;

    // 1. Breathing Cycle (~0.8 Hz)
    breath_phase = sinf(anim_timer * 3.5f) * 1.5f;

    // 2. Heart Pulse for LOVE emote
    heart_pulse = 1.0f + 0.25f * sinf(anim_timer * 8.0f);

    // 3. Star orbit for DIZZY emote
    star_orbit_angle += dt * 4.0f;
    if (star_orbit_angle > 6.28318f) star_orbit_angle -= 6.28318f;

    // 4. Floating Music Notes
    note_float_y += dt * 14.0f;
    if (note_float_y > 32.0f) note_float_y = 0.0f;

    // 5. Sleeping Zzz Drift
    if (current_emote == MochiEmote::SLEEPING) {
        zzz_offset += dt * 12.0f;
        if (zzz_offset > 24.0f) zzz_offset = 0.0f;
    }

    // 6. Driving Wind Streaks
    for (int i = 0; i < 4; i++) {
        wind_line_x[i] -= dt * (80.0f + i * 20.0f);
        if (wind_line_x[i] < -20.0f) {
            wind_line_x[i] = 130.0f + (i * 15.0f);
        }
    }

    // 7. Natural Eye Blinking State Machine
    if (!is_sleeping && current_emote != MochiEmote::LAUGH && current_emote != MochiEmote::SLEEPY) {
        blink_timer += dt;
        if (!is_blinking && blink_timer > 3.8f) {
            is_blinking = true;
            blink_phase = 0.0f;
            blink_timer = 0.0f;
        } else if (is_blinking) {
            blink_phase += dt * 7.0f; // blink lasts ~0.14 sec
            if (blink_phase >= 1.0f) {
                is_blinking = false;
                blink_phase = 0.0f;
            }
        }
    }

    // 8. Dizzy Countdown
    if (is_dizzy) {
        dizzy_timer -= dt;
        if (dizzy_timer <= 0.0f) {
            is_dizzy = false;
            setEmote(base_emote);
        }
    }

    // 9. Emote Override Timer
    if (emote_override_timer > 0.0f) {
        emote_override_timer -= dt;
        if (emote_override_timer <= 0.0f) {
            current_emote = base_emote;
            updateMoodLed();
        }
    }

    // 10. Feeding State Machine
    if (feeding_active) {
        feeding_timer -= dt;
        if (snack_x > 68.0f) {
            snack_x -= dt * 45.0f; // Snack glides towards mouth
        } else {
            // Snack reached mouth, chew!
            chew_counter++;
            if (chew_counter % 8 == 0) {
                triggerSound(MochiSound::EATING_MUNCH);
            }
        }

        if (feeding_timer <= 0.0f) {
            feeding_active = false;
            hunger = (hunger + 35 > 100) ? 100 : (hunger + 35);
            happiness = (happiness + 10 > 100) ? 100 : (happiness + 10);
            friendship_xp += 15;
            if (friendship_xp >= static_cast<uint16_t>(friendship_level * 100)) {
                friendship_xp -= friendship_level * 100;
                if (friendship_level < 10) friendship_level++;
            }
            setEmote(MochiEmote::CONTENT, 1500);
            triggerSound(MochiSound::HAPPY_CHIRP);
        }
    }

    // 11. Petting Animation
    if (petting_active) {
        petting_timer -= dt;
        if (petting_timer <= 0.0f) {
            petting_active = false;
        }
    }

    // 12. Idle Timer -> Auto Sleep
    if (!is_sleeping && !feeding_active && !petting_active) {
        idle_timer += dt;
        if (idle_timer > 22.0f && current_emote != MochiEmote::SLEEPY) {
            setEmote(MochiEmote::SLEEPY);
        }
        if (idle_timer > 30.0f) {
            is_sleeping = true;
            setEmote(MochiEmote::SLEEPING);
            triggerSound(MochiSound::SNORE);
        }
    }

    // 13. Hunger & Happiness Natural Decay (every 45 seconds)
    hunger_timer += dt;
    if (hunger_timer > 45.0f) {
        hunger_timer = 0.0f;
        if (hunger > 5) hunger -= 2;
        if (happiness > 10) happiness -= 1;
        if (hunger < 25 && !is_sleeping) {
            setEmote(MochiEmote::FRUSTRATED);
        }
    }
}

void MochiPet::pickerUp() {
    if (submode == MochiSubmode::EMOTE_PICKER) {
        if (picker_selection > 0) picker_selection--;
        if (picker_selection < picker_offset) picker_offset = picker_selection;
    } else if (submode == MochiSubmode::HELMET_PICKER) {
        if (picker_selection > 0) picker_selection--;
        if (picker_selection < picker_offset) picker_offset = picker_selection;
    }
}

void MochiPet::pickerDown() {
    if (submode == MochiSubmode::EMOTE_PICKER) {
        int max_items = static_cast<int>(MochiEmote::COUNT);
        if (picker_selection < max_items - 1) picker_selection++;
        if (picker_selection >= picker_offset + 3) picker_offset = picker_selection - 2;
    } else if (submode == MochiSubmode::HELMET_PICKER) {
        int max_items = static_cast<int>(MochiHelmet::COUNT);
        if (picker_selection < max_items - 1) picker_selection++;
        if (picker_selection >= picker_offset + 3) picker_offset = picker_selection - 2;
    }
}

void MochiPet::pickerSelect() {
    if (submode == MochiSubmode::EMOTE_PICKER) {
        setEmote(static_cast<MochiEmote>(picker_selection));
        submode = MochiSubmode::INTERACTIVE;
        triggerSound(MochiSound::HAPPY_CHIRP);
    } else if (submode == MochiSubmode::HELMET_PICKER) {
        setHelmet(static_cast<MochiHelmet>(picker_selection));
        submode = MochiSubmode::INTERACTIVE;
    }
}

void MochiPet::triggerSound(MochiSound sound) {
    last_sound = sound;
    if (is_muted) return;

#ifdef ARDUINO
    switch(sound) {
        case MochiSound::HAPPY_CHIRP: {
            soundManager.playTone(523, 60);
            delay(20);
            soundManager.playTone(659, 70);
            delay(20);
            soundManager.playTone(784, 90);
            break;
        }
        case MochiSound::LOVE_CHIME: {
            soundManager.playTone(659, 80);
            delay(15);
            soundManager.playTone(880, 120);
            break;
        }
        case MochiSound::GIGGLE: {
            soundManager.playTone(880, 40);
            delay(15);
            soundManager.playTone(988, 40);
            delay(15);
            soundManager.playTone(1046, 50);
            break;
        }
        case MochiSound::EATING_MUNCH: {
            soundManager.playTone(380, 35);
            break;
        }
        case MochiSound::PURR: {
            soundManager.playTone(220, 120);
            break;
        }
        case MochiSound::SNORE: {
            soundManager.playTone(165, 180);
            break;
        }
        case MochiSound::WAKEUP: {
            soundManager.playTone(440, 80);
            delay(20);
            soundManager.playTone(880, 100);
            break;
        }
        case MochiSound::ANGRY_GROWL: {
            soundManager.playTone(130, 200);
            break;
        }
        case MochiSound::DIZZY_STUMBLE: {
            soundManager.playTone(784, 50);
            delay(15);
            soundManager.playTone(659, 50);
            delay(15);
            soundManager.playTone(523, 70);
            break;
        }
        case MochiSound::DRIVING_REV: {
            soundManager.playTone(294, 90);
            delay(15);
            soundManager.playTone(392, 110);
            break;
        }
        case MochiSound::GUNDAM_BEEP: {
            soundManager.playTone(1046, 60);
            delay(15);
            soundManager.playTone(1318, 90);
            break;
        }
        default:
            break;
    }
#endif
}

void MochiPet::updateMoodLed() {
#ifdef ARDUINO
    CRGB color;
    switch(current_emote) {
        case MochiEmote::HAPPY:
        case MochiEmote::CONTENT:
            color = CRGB(255, 160, 20); // Warm Golden Amber
            break;
        case MochiEmote::LOVE:
            color = CRGB(255, 20, 80);  // Vivid Pink/Red
            break;
        case MochiEmote::LAUGH:
        case MochiEmote::EXCITED:
            color = CRGB(255, 220, 0);  // Cheerful Bright Yellow
            break;
        case MochiEmote::ANGRY:
            color = CRGB(255, 0, 0);    // Crimson
            break;
        case MochiEmote::SLEEPING:
            color = CRGB(0, 40, 80);    // Soft Dim Moon Blue
            break;
        case MochiEmote::GUNDAM:
            color = CRGB(0, 120, 255);  // Mobile Suit Blue
            break;
        case MochiEmote::MUSIC:
            color = CRGB(180, 0, 255);  // Purple
            break;
        case MochiEmote::DRIVING:
            color = CRGB(255, 80, 0);   // Turbo Orange
            break;
        default:
            color = CRGB(0, 180, 100);  // Emerald Mint
            break;
    }
    ledManager.setColor(color);
#endif
}

#ifdef ARDUINO
// -------------------------------------------------------------
// ARDUINO OLED PROCEDURAL VECTOR RENDERING ENGINE
// -------------------------------------------------------------
void MochiPet::render(U8G2& display) {
    if (submode == MochiSubmode::STATS_HUD) {
        // === STATS & TAMAGOTCHI COCKPIT ===
        display.setFont(u8g2_font_6x10_tf);
        display.setDrawColor(1);
        display.drawStr(4, 10, "DASAI MOCHI");
        
        char buf[24];
        snprintf(buf, sizeof(buf), "LV.%d", friendship_level);
        display.drawStr(98, 10, buf);
        display.drawHLine(0, 13, 128);

        // Happiness gauge
        display.drawStr(4, 25, "HAP");
        display.drawFrame(30, 17, 65, 8);
        int hap_w = (happiness * 61) / 100;
        if (hap_w > 0) display.drawBox(32, 19, hap_w, 4);
        snprintf(buf, sizeof(buf), "%d%%", happiness);
        display.drawStr(98, 25, buf);

        // Hunger gauge
        display.drawStr(4, 37, "HNG");
        display.drawFrame(30, 29, 65, 8);
        int hng_w = (hunger * 61) / 100;
        if (hng_w > 0) display.drawBox(32, 31, hng_w, 4);
        snprintf(buf, sizeof(buf), "%d%%", hunger);
        display.drawStr(98, 37, buf);

        // Mood & Helmet
        snprintf(buf, sizeof(buf), "MOOD: %s", getEmoteName(current_emote));
        display.drawStr(4, 49, buf);
        snprintf(buf, sizeof(buf), "HELM: %s", getHelmetName(current_helmet));
        display.drawStr(4, 60, buf);
        return;
    }

    if (submode == MochiSubmode::EMOTE_PICKER || submode == MochiSubmode::HELMET_PICKER) {
        display.setFont(u8g2_font_6x10_tf);
        display.setDrawColor(1);
        const char* title = (submode == MochiSubmode::EMOTE_PICKER) ? "SELECT EMOTE" : "SELECT HELMET";
        display.drawStr(4, 10, title);
        display.drawHLine(0, 13, 128);

        int total = (submode == MochiSubmode::EMOTE_PICKER) ? static_cast<int>(MochiEmote::COUNT) : static_cast<int>(MochiHelmet::COUNT);
        for (int i = 0; i < 3; i++) {
            int idx = picker_offset + i;
            if (idx >= total) break;
            int y = 26 + (i * 13);
            const char* name = (submode == MochiSubmode::EMOTE_PICKER) 
                ? getEmoteName(static_cast<MochiEmote>(idx)) 
                : getHelmetName(static_cast<MochiHelmet>(idx));

            if (idx == picker_selection) {
                display.drawBox(2, y - 9, 86, 11);
                display.setDrawColor(0);
                display.drawStr(4, y, name);
                display.setDrawColor(1);
            } else {
                display.drawStr(4, y, name);
            }
        }

        // Mini preview box on right
        display.drawFrame(92, 16, 34, 45);
        display.drawStr(95, 26, "PREV");
        display.drawCircle(109, 44, 10);
        return;
    }

    // === FULL-SCREEN INTERACTIVE PET MODE ===
    int16_t cx = 64 + static_cast<int16_t>(head_lean);
    int16_t cy = 32 + static_cast<int16_t>(breath_phase);

    // 1. Digital Helmet / Outer Visor
    switch (current_helmet) {
        case MochiHelmet::CLASSIC:
            // Sleek aerodynamic helmet shell
            display.drawRFrame(cx - 44, cy - 22, 88, 44, 8);
            display.drawBox(cx - 6, cy - 24, 12, 3); // top air vent
            break;

        case MochiHelmet::GUNDAM:
            // RX-78 Gundam Faceplate & V-Fin
            display.drawRFrame(cx - 44, cy - 22, 88, 44, 6);
            // V-Fin Crest
            display.drawLine(cx - 4, cy - 22, cx - 24, cy - 31);
            display.drawLine(cx + 4, cy - 22, cx + 24, cy - 31);
            display.drawBox(cx - 4, cy - 26, 8, 5); // Forehead camera
            // Cheek vents
            display.drawLine(cx - 38, cy + 8, cx - 30, cy + 14);
            display.drawLine(cx - 38, cy + 12, cx - 30, cy + 18);
            display.drawLine(cx + 38, cy + 8, cx + 30, cy + 14);
            display.drawLine(cx + 38, cy + 12, cx + 30, cy + 18);
            // Red Chin
            display.drawBox(cx - 4, cy + 22, 8, 4);
            break;

        case MochiHelmet::CYBER:
            // Cyberpunk HUD brackets
            display.drawFrame(cx - 44, cy - 22, 88, 44);
            display.drawBox(cx - 44, cy - 22, 6, 2);
            display.drawBox(cx - 44, cy - 22, 2, 6);
            display.drawBox(cx + 38, cy - 22, 6, 2);
            display.drawBox(cx + 42, cy - 22, 2, 6);
            display.drawBox(cx - 44, cy + 20, 6, 2);
            display.drawBox(cx - 44, cy + 16, 2, 6);
            display.drawBox(cx + 38, cy + 20, 6, 2);
            display.drawBox(cx + 42, cy + 16, 2, 6);
            break;

        case MochiHelmet::NEKO:
            // Cute Cat Ears
            display.drawRFrame(cx - 44, cy - 22, 88, 44, 8);
            display.drawTriangle(cx - 36, cy - 22, cx - 28, cy - 31, cx - 18, cy - 22);
            display.drawTriangle(cx + 18, cy - 22, cx + 28, cy - 31, cx + 36, cy - 22);
            break;

        case MochiHelmet::TACTICAL:
            // 007 Night-Vision Reticle Bezel
            display.drawRFrame(cx - 44, cy - 22, 88, 44, 4);
            display.drawLine(cx - 46, cy, cx - 40, cy);
            display.drawLine(cx + 40, cy, cx + 46, cy);
            display.drawLine(cx, cy - 24, cx, cy - 18);
            display.drawLine(cx, cy + 18, cx, cy + 24);
            break;

        default:
            display.drawRFrame(cx - 44, cy - 22, 88, 44, 8);
            break;
    }

    // 2. Eyes & Pupils
    int16_t lx = cx - 18 + static_cast<int16_t>(pupil_dx);
    int16_t rx = cx + 18 + static_cast<int16_t>(pupil_dx);
    int16_t ey = cy - 2 + static_cast<int16_t>(pupil_dy);

    if (is_blinking) {
        // Natural eye blink slit
        display.drawLine(lx - 8, ey, lx + 8, ey);
        display.drawLine(rx - 8, ey, rx + 8, ey);
    } else {
        switch (current_emote) {
            case MochiEmote::HAPPY:
                // Smiling curved arcs ^ ^
                display.drawLine(lx - 8, ey + 2, lx, ey - 5);
                display.drawLine(lx, ey - 5, lx + 8, ey + 2);
                display.drawLine(rx - 8, ey + 2, rx, ey - 5);
                display.drawLine(rx, ey - 5, rx + 8, ey + 2);
                break;

            case MochiEmote::LOVE: {
                // Pulsing Heart Eyes
                int r = static_cast<int>(3.5f * heart_pulse);
                display.drawDisc(lx - r, ey - 2, r);
                display.drawDisc(lx + r, ey - 2, r);
                display.drawTriangle(lx - 2 * r, ey - 1, lx + 2 * r, ey - 1, lx, ey + 2 * r + 2);

                display.drawDisc(rx - r, ey - 2, r);
                display.drawDisc(rx + r, ey - 2, r);
                display.drawTriangle(rx - 2 * r, ey - 1, rx + 2 * r, ey - 1, rx, ey + 2 * r + 2);
                break;
            }

            case MochiEmote::LAUGH:
                // Squinting chevrons ><
                display.drawLine(lx - 7, ey - 5, lx + 3, ey);
                display.drawLine(lx + 3, ey, lx - 7, ey + 5);
                display.drawLine(rx + 7, ey - 5, rx - 3, ey);
                display.drawLine(rx - 3, ey, rx + 7, ey + 5);
                break;

            case MochiEmote::EXCITED:
                // Winking Left Chevron, Wide Right Eye
                display.drawLine(lx - 6, ey - 4, lx + 2, ey);
                display.drawLine(lx + 2, ey, lx - 6, ey + 4);
                display.drawDisc(rx, ey, 6);
                display.setDrawColor(0);
                display.drawDisc(rx - 1, ey - 1, 2); // highlight
                display.setDrawColor(1);
                break;

            case MochiEmote::RELAXED:
                // Gentle wavy eyes ~ ~
                display.drawLine(lx - 7, ey, lx - 3, ey - 2);
                display.drawLine(lx - 3, ey - 2, lx + 2, ey + 2);
                display.drawLine(lx + 2, ey + 2, lx + 7, ey);
                display.drawLine(rx - 7, ey, rx - 3, ey - 2);
                display.drawLine(rx - 3, ey - 2, rx + 2, ey + 2);
                display.drawLine(rx + 2, ey + 2, rx + 7, ey);
                break;

            case MochiEmote::CONTENT:
                display.drawDisc(lx, ey, 5);
                display.drawDisc(rx, ey, 5);
                display.setDrawColor(0);
                display.drawPixel(lx + 1, ey - 1);
                display.drawPixel(rx + 1, ey - 1);
                display.setDrawColor(1);
                break;

            case MochiEmote::PROUD:
                // Raised tilted eyebrows
                display.drawLine(lx - 8, ey - 6, lx + 6, ey - 8);
                display.drawLine(rx - 6, ey - 8, rx + 8, ey - 6);
                display.drawDisc(lx, ey, 4);
                display.drawDisc(rx, ey, 4);
                break;

            case MochiEmote::ANGRY:
                // Slanted sharp eyebrows \ /
                display.drawLine(lx - 9, ey - 8, lx + 7, ey - 2);
                display.drawLine(rx - 7, ey - 2, rx + 9, ey - 8);
                display.drawBox(lx - 5, ey - 2, 10, 4);
                display.drawBox(rx - 5, ey - 2, 10, 4);
                break;

            case MochiEmote::FRUSTRATED:
                display.drawHLine(lx - 7, ey - 4, 14);
                display.drawHLine(rx - 7, ey - 4, 14);
                display.drawDisc(lx, ey + 1, 3);
                display.drawDisc(rx, ey + 1, 3);
                // Dripping sweat bead
                display.drawDisc(cx - 30, cy - 8, 2);
                display.drawLine(cx - 30, cy - 11, cx - 30, cy - 8);
                break;

            case MochiEmote::CONFUSED:
                // Big left eye, tiny right eye
                display.drawCircle(lx, ey, 7);
                display.drawDisc(lx, ey, 3);
                display.drawDisc(rx, ey + 2, 2);
                // Question mark
                display.drawStr(rx + 8, cy - 6, "?");
                break;

            case MochiEmote::EMBARRASSED:
                display.drawDisc(lx - 2, ey + 2, 4);
                display.drawDisc(rx - 2, ey + 2, 4);
                // Heavy blushing hatching ///
                display.drawLine(cx - 32, cy + 4, cx - 28, cy + 9);
                display.drawLine(cx - 28, cy + 4, cx - 24, cy + 9);
                display.drawLine(cx + 24, cy + 4, cx + 28, cy + 9);
                display.drawLine(cx + 28, cy + 4, cx + 32, cy + 9);
                break;

            case MochiEmote::SLEEPY:
                // Drooping 70% closed eyelids
                display.drawDisc(lx, ey, 5);
                display.drawDisc(rx, ey, 5);
                display.setDrawColor(0);
                display.drawBox(lx - 6, ey - 6, 12, 6);
                display.drawBox(rx - 6, ey - 6, 12, 6);
                display.setDrawColor(1);
                display.drawHLine(lx - 6, ey, 12);
                display.drawHLine(rx - 6, ey, 12);
                break;

            case MochiEmote::SLEEPING:
                // Closed curved eye lines - -
                display.drawLine(lx - 7, ey, lx + 7, ey);
                display.drawLine(rx - 7, ey, rx + 7, ey);
                // Floating Zzz
                display.drawStr(cx + 18, cy - 10 - static_cast<int>(zzz_offset), "z");
                display.drawStr(cx + 25, cy - 16 - static_cast<int>(zzz_offset), "Z");
                break;

            case MochiEmote::DIZZY: {
                // Spinning Spiral Eyes & Orbiting Stars
                display.drawCircle(lx, ey, 5);
                display.drawCircle(lx, ey, 2);
                display.drawCircle(rx, ey, 5);
                display.drawCircle(rx, ey, 2);

                // Orbiting Stars
                int16_t star1_x = cx + static_cast<int16_t>(cosf(star_orbit_angle) * 22.0f);
                int16_t star1_y = cy - 14 + static_cast<int16_t>(sinf(star_orbit_angle) * 6.0f);
                display.drawPixel(star1_x, star1_y);
                display.drawPixel(star1_x - 1, star1_y);
                display.drawPixel(star1_x + 1, star1_y);
                display.drawPixel(star1_x, star1_y - 1);
                display.drawPixel(star1_x, star1_y + 1);
                break;
            }

            case MochiEmote::DRIVING:
                // Narrow racing eyes with visor sheen
                display.drawBox(lx - 7, ey - 2, 14, 5);
                display.drawBox(rx - 7, ey - 2, 14, 5);
                // Wind streaks
                for (int i = 0; i < 4; i++) {
                    int16_t wx = static_cast<int16_t>(wind_line_x[i]);
                    display.drawHLine(wx, 8 + i * 14, 18);
                }
                break;

            case MochiEmote::MUSIC:
                display.drawDisc(lx, ey, 5);
                display.drawDisc(rx, ey, 5);
                // Floating Eighth Note
                display.drawDisc(cx + 26, cy - static_cast<int16_t>(note_float_y), 2);
                display.drawVLine(cx + 28, cy - static_cast<int16_t>(note_float_y) - 6, 6);
                display.drawHLine(cx + 28, cy - static_cast<int16_t>(note_float_y) - 6, 4);
                break;

            case MochiEmote::GUNDAM:
                // Mech Visor Optics
                display.drawBox(lx - 6, ey - 3, 12, 6);
                display.drawBox(rx - 6, ey - 3, 12, 6);
                display.drawLine(cx - 30, cy, cx + 30, cy);
                break;

            default:
                display.drawDisc(lx, ey, 5);
                display.drawDisc(rx, ey, 5);
                break;
        }
    }

    // 3. Cute Cheeks (Blush)
    if (current_emote == MochiEmote::HAPPY || current_emote == MochiEmote::LOVE || current_emote == MochiEmote::LAUGH) {
        display.drawLine(cx - 26, cy + 8, cx - 22, cy + 12);
        display.drawLine(cx - 22, cy + 8, cx - 18, cy + 12);
        display.drawLine(cx + 18, cy + 8, cx + 22, cy + 12);
        display.drawLine(cx + 22, cy + 8, cx + 26, cy + 12);
    }

    // 4. Mouth
    int16_t my = cy + 10;
    if (feeding_active) {
        // Munching animation
        if ((chew_counter / 4) % 2 == 0) {
            display.drawDisc(cx, my, 4); // open mouth
        } else {
            display.drawLine(cx - 4, my, cx + 4, my); // closed mouth
        }
    } else {
        switch (current_emote) {
            case MochiEmote::HAPPY:
            case MochiEmote::LOVE:
            case MochiEmote::CONTENT:
                display.drawLine(cx - 3, my, cx, my + 2);
                display.drawLine(cx, my + 2, cx + 3, my);
                break;
            case MochiEmote::LAUGH:
            case MochiEmote::EXCITED:
                display.drawTriangle(cx - 4, my, cx + 4, my, cx, my + 4);
                break;
            case MochiEmote::ANGRY:
                display.drawLine(cx - 4, my + 2, cx, my);
                display.drawLine(cx, my, cx + 4, my + 2);
                break;
            case MochiEmote::SLEEPY:
                display.drawCircle(cx, my + 1, 3); // yawn
                break;
            default:
                display.drawHLine(cx - 3, my, 6);
                break;
        }
    }

    // 5. Feeding Snack Sprite
    if (feeding_active) {
        int16_t sx = static_cast<int16_t>(snack_x);
        int16_t sy = static_cast<int16_t>(snack_y);
        // Onigiri triangle with nori
        display.drawTriangle(sx, sy - 4, sx - 4, sy + 3, sx + 4, sy + 3);
        display.drawBox(sx - 1, sy + 1, 3, 2);
    }

    // 6. Petting Floating Hearts
    if (petting_active) {
        int16_t hx = cx - 20 + static_cast<int16_t>(sinf(anim_timer * 10.0f) * 6.0f);
        int16_t hy = cy - 22;
        display.drawDisc(hx - 2, hy, 2);
        display.drawDisc(hx + 2, hy, 2);
        display.drawTriangle(hx - 4, hy + 1, hx + 4, hy + 1, hx, hy + 5);
    }
}
#endif

// -------------------------------------------------------------
// HEADLESS BUFFER RENDERING (For Host Automated Test Harness)
// -------------------------------------------------------------
void MochiPet::renderToBuffer(uint8_t* buffer, int width, int height) {
    if (!buffer || width <= 0 || height <= 0) return;
    memset(buffer, 0, (width * height) / 8);

    auto set_pixel = [buffer, width, height](int x, int y) {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            int byte_idx = (y * (width / 8)) + (x / 8);
            buffer[byte_idx] |= (1 << (x % 8));
        }
    };

    int16_t cx = 64 + static_cast<int16_t>(head_lean);
    int16_t cy = 32 + static_cast<int16_t>(breath_phase);

    // Draw helmet box outline
    for (int x = cx - 40; x <= cx + 40; x++) {
        set_pixel(x, cy - 20);
        set_pixel(x, cy + 20);
    }
    for (int y = cy - 20; y <= cy + 20; y++) {
        set_pixel(cx - 40, y);
        set_pixel(cx + 40, y);
    }

    // Draw eyes
    int16_t lx = cx - 18 + static_cast<int16_t>(pupil_dx);
    int16_t rx = cx + 18 + static_cast<int16_t>(pupil_dx);
    int16_t ey = cy - 2 + static_cast<int16_t>(pupil_dy);

    for (int dx = -3; dx <= 3; dx++) {
        for (int dy = -3; dy <= 3; dy++) {
            set_pixel(lx + dx, ey + dy);
            set_pixel(rx + dx, ey + dy);
        }
    }
}
