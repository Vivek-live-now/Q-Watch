#ifndef MOCHI_PET_H
#define MOCHI_PET_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <U8g2lib.h>
#include "sensors.h"
#include "sound_manager.h"
#include "led_manager.h"
#include "file_manager.h"
#include "anim_engine.h"
#else
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#endif

// -------------------------------------------------------------
// 1. MOCHI EMOTIONS (Full Dasai Mochi Expressions)
// -------------------------------------------------------------
enum class MochiEmote : uint8_t {
    HAPPY = 0,       // Cute curved eyes ^ ^, blushing cheeks, gentle cheerful bounce
    LOVE,            // Pulsing beating heart eyes <3 <3, arpeggio love chirp, pink/red LED
    LAUGH,           // Squinting >< eyes, laughing mouth, giggle bounce
    EXCITED,         // Winking eye > ^, energetic hop
    RELAXED,         // Gentle wavy eyes ~ ~, calm aura
    CONTENT,         // Soft eyes, pleasant smile
    PROUD,           // Raised eyebrows, smug grin
    ANGRY,           // Slanted sharp brows \ /, red LED, low grumble
    FRUSTRATED,      // Annoyed frown, dripping sweat drop on temple
    CONFUSED,        // One big eye, one small eye, floating ?
    EMBARRASSED,     // Bashful downward glance, hatching blush ///
    SLEEPY,          // Drooping heavy eyelids, yawning
    SLEEPING,        // Closed eyes - -, floating Zzz drifting up, slow breathing
    DIZZY,           // Spinning spiral eyes @ @, orbiting stars (triggered by shake)
    DRIVING,         // Narrow racing visor eyes, wind speed streaks, engine rev
    MUSIC,           // Swaying head bob, floating musical notes
    GUNDAM,          // Awakened combat visor, lock-on reticle, robot chime
    COUNT
};

// -------------------------------------------------------------
// 2. MODULAR DIGITAL HELMETS (Dasai Swappable Helmet Collection)
// -------------------------------------------------------------
enum class MochiHelmet : uint8_t {
    CLASSIC = 0,     // Minimalist smooth round helmet visor
    GUNDAM,          // RX-78-2 V-fin crest, forehead sensor, cheek vents, red chin
    CYBER,           // Cyberpunk carbon edition, corner HUD brackets, grid lines
    NEKO,            // Cute cat ears with inner ear detail
    TACTICAL,        // 007 night-vision stealth visor
    COUNT
};

// -------------------------------------------------------------
// 3. MOCHI APP SUBMODES
// -------------------------------------------------------------
enum class MochiSubmode : uint8_t {
    INTERACTIVE = 0, // Full-screen face with live physics, gaze tracking, animations
    STATS_HUD,       // Stats overlay (Pet Name "MOCHI", Friendship, Hunger, Energy, Mood)
    EMOTE_PICKER,    // Interactive on-screen list to manually pick/preview any emote
    HELMET_PICKER,   // Visual preview and selector for helmets
    COUNT
};

// -------------------------------------------------------------
// 4. MOCHI SOUND MELODIES & SFX IDS
// -------------------------------------------------------------
enum class MochiSound : uint8_t {
    NONE = 0,
    HAPPY_CHIRP,
    LOVE_CHIME,
    GIGGLE,
    EATING_MUNCH,
    PURR,
    SNORE,
    WAKEUP,
    ANGRY_GROWL,
    DIZZY_STUMBLE,
    DRIVING_REV,
    GUNDAM_BEEP
};

// -------------------------------------------------------------
// 5. MASTER MOCHI PET CONTROLLER CLASS
// -------------------------------------------------------------
class MochiPet {
public:
    MochiPet();

    void begin();
    void update(float dt = 0.033f);

    // Core Petting & Feeding Interactions
    void pet();
    void feed();
    void toggleSleep();
    void wakeUp();

    // Emotion & Mood Controls
    void setEmote(MochiEmote emote, uint32_t duration_ms = 0);
    MochiEmote getEmote() const { return current_emote; }
    const char* getEmoteName(MochiEmote emote) const;
    void nextEmote();
    void prevEmote();

    // Helmet Controls
    void setHelmet(MochiHelmet helmet);
    MochiHelmet getHelmet() const { return current_helmet; }
    const char* getHelmetName(MochiHelmet helmet) const;
    void nextHelmet();
    void prevHelmet();

    // Submode Navigation
    MochiSubmode getSubmode() const { return submode; }
    void setSubmode(MochiSubmode m) { submode = m; }
    void toggleHud();

    // Tamagotchi Stats
    uint8_t getHappiness() const { return happiness; }
    uint8_t getHunger() const { return hunger; }
    uint8_t getFriendshipLevel() const { return friendship_level; }
    uint16_t getFriendshipXP() const { return friendship_xp; }
    bool isSleeping() const { return is_sleeping; }

    // IMU Reactivity
    void updatePhysics(float pitch, float roll, float ax, float ay, float az, float gx, float gy, float gz);
    float getPupilDx() const { return pupil_dx; }
    float getPupilDy() const { return pupil_dy; }
    float getHeadLean() const { return head_lean; }
    bool isDizzy() const { return is_dizzy; }

    // Audio & Sound
    void toggleMute() { is_muted = !is_muted; }
    bool isMuted() const { return is_muted; }
    MochiSound getLastPlayedSound() const { return last_sound; }

    // Picker Navigation
    int getPickerSelection() const { return picker_selection; }
    int getPickerOffset() const { return picker_offset; }
    void pickerUp();
    void pickerDown();
    void pickerSelect();

    // Animation & State checks
    bool isFeeding() const { return feeding_active; }
    bool isPetting() const { return petting_active; }
    float getSnackX() const { return snack_x; }
    float getSnackY() const { return snack_y; }
    float getHeartPulse() const { return heart_pulse; }
    float getStarAngle() const { return star_orbit_angle; }

    // Rendering Interface
#ifdef ARDUINO
    void render(U8G2& display);
#endif
    // Virtual rendering buffer for host test harness / headless verification (128x64, 1024 bytes)
    void renderToBuffer(uint8_t* buffer, int width = 128, int height = 64);

private:
    MochiEmote current_emote;
    MochiEmote base_emote;
    MochiHelmet current_helmet;
    MochiSubmode submode;
    MochiSound last_sound;

    // Tamagotchi stats
    uint8_t happiness;
    uint8_t hunger;
    uint8_t friendship_level;
    uint16_t friendship_xp;
    bool is_sleeping;
    bool is_muted;

    // IMU Physics and Gaze
    float pupil_dx;
    float pupil_dy;
    float target_pupil_dx;
    float target_pupil_dy;
    float head_lean;
    bool is_dizzy;
    float dizzy_timer;
    float idle_timer;
    float hunger_timer;

    // Animation Timers & Phases
    float anim_timer;
    float blink_timer;
    bool is_blinking;
    float blink_phase;
    float breath_phase;
    float heart_pulse;
    float star_orbit_angle;
    float note_float_y;
    float wind_line_x[4];
    float zzz_offset;

    // Feeding & Petting Animation State
    bool feeding_active;
    float snack_x;
    float snack_y;
    int chew_counter;
    float feeding_timer;

    bool petting_active;
    float petting_timer;

    // Emote override timer
    float emote_override_timer;

    // Pickers
    int picker_selection;
    int picker_offset;

    // Helpers
    void triggerSound(MochiSound sound);
    void updateMoodLed();
};

extern MochiPet mochiPet;

#endif // MOCHI_PET_H
