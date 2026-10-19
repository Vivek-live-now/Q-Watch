#include "sound_manager.h"
#include "hw_config.h"
#include "settings_data.h"
#include "file_manager.h"

#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
#include <driver/gpio.h>
#endif

SoundManager soundManager;

// Predefined effects sequences
const SoundNote seq_boot[] = { {1800, 80}, {2200, 80}, {2700, 150} };
const SoundNote seq_wake[] = { {2400, 60}, {2700, 100} };
const SoundNote seq_sleep[] = { {2700, 60}, {2000, 100} };
const SoundNote seq_notify[] = { {2700, 60}, {0, 30}, {2700, 60} };
const SoundNote seq_warning[] = { {1500, 120}, {0, 40}, {1500, 120} };
const SoundNote seq_alert[] = { {3000, 80}, {0, 30}, {3000, 80}, {0, 30}, {3000, 120} };
const SoundNote seq_qbranch[] = { {2200, 70}, {2700, 90}, {2400, 70}, {3100, 150} };

// UI Navigation Sequences
const SoundNote seq_mod_move[] = { {2700, 15} };
const SoundNote seq_mod_sel[] = { {2900, 30} };
const SoundNote seq_mod_back[] = { {2200, 35} };

const SoundNote seq_tac_move[] = { {1800, 10} };
const SoundNote seq_tac_sel[] = { {2400, 12} };
const SoundNote seq_tac_back[] = { {1500, 20} };

const SoundNote seq_ret_move[] = { {1000, 25} };
const SoundNote seq_ret_sel[] = { {1500, 30}, {2500, 30} };
const SoundNote seq_ret_back[] = { {1500, 30}, {800, 30} };

// Long beep for launching apps
const SoundNote seq_app_launch[] = { {2700, 220} };

// Iconic & Cute Jukebox Tunes
const SoundNote seq_mario[] = {
    {659, 100}, {0, 40}, {659, 100}, {0, 80}, {659, 100}, {0, 80}, {523, 100}, {0, 40},
    {659, 120}, {0, 80}, {784, 150}, {0, 150}, {392, 150}, {0, 150},
    {523, 120}, {0, 60}, {392, 120}, {0, 60}, {330, 120}, {0, 60},
    {440, 120}, {0, 40}, {494, 120}, {0, 40}, {466, 100}, {440, 120},
    {392, 100}, {659, 120}, {784, 120}, {880, 150}, {698, 100}, {784, 100},
    {659, 120}, {523, 100}, {587, 100}, {494, 180}
};
const uint8_t seq_mario_len = sizeof(seq_mario) / sizeof(seq_mario[0]);

const SoundNote seq_xmas[] = {
    {294, 200}, {392, 200}, {392, 100}, {440, 100}, {392, 100}, {370, 100}, {330, 200}, {330, 200},
    {330, 200}, {440, 200}, {440, 100}, {494, 100}, {440, 100}, {392, 100}, {370, 200}, {294, 200},
    {294, 200}, {494, 200}, {494, 100}, {523, 100}, {494, 100}, {440, 100}, {392, 200}, {330, 200},
    {294, 100}, {294, 100}, {330, 200}, {440, 200}, {370, 200}, {392, 400}
};
const uint8_t seq_xmas_len = sizeof(seq_xmas) / sizeof(seq_xmas[0]);

const SoundNote seq_birthday[] = {
    {262, 180}, {0, 20}, {262, 100}, {294, 250}, {262, 250}, {349, 250}, {330, 450},
    {262, 180}, {0, 20}, {262, 100}, {294, 250}, {262, 250}, {392, 250}, {349, 450},
    {262, 180}, {0, 20}, {262, 100}, {523, 250}, {440, 250}, {349, 250}, {330, 250}, {294, 250},
    {466, 180}, {0, 20}, {466, 100}, {440, 250}, {349, 250}, {392, 250}, {349, 500}
};
const uint8_t seq_birthday_len = sizeof(seq_birthday) / sizeof(seq_birthday[0]);

const SoundNote seq_tetris[] = {
    {659, 250}, {494, 125}, {523, 125}, {587, 250}, {523, 125}, {494, 125},
    {440, 250}, {440, 125}, {523, 125}, {659, 250}, {587, 125}, {523, 125},
    {494, 375}, {523, 125}, {587, 250}, {659, 250},
    {523, 250}, {440, 250}, {440, 350}
};
const uint8_t seq_tetris_len = sizeof(seq_tetris) / sizeof(seq_tetris[0]);

const SoundNote seq_starwars[] = {
    {440, 300}, {440, 300}, {440, 300}, {349, 200}, {523, 100},
    {440, 300}, {349, 200}, {523, 100}, {440, 500},
    {659, 300}, {659, 300}, {659, 300}, {698, 200}, {523, 100},
    {415, 300}, {349, 200}, {523, 100}, {440, 500}
};
const uint8_t seq_starwars_len = sizeof(seq_starwars) / sizeof(seq_starwars[0]);

static const char* const TUNE_NAMES[5] = {
    "SUPER MARIO",
    "MERRY XMAS",
    "HAPPY BIRTHDAY",
    "TETRIS THEME",
    "STAR WARS"
};

#define BUZZER_LEDC_CHANNEL 0
#define BUZZER_LEDC_RES_BITS 8

SoundManager::SoundManager() :
    current_sequence(nullptr),
    sequence_length(0),
    current_note_index(0),
    note_start_time(0),
    active_duty_pct(0),
    is_playing(false),
    is_tone_active(false)
{}

void SoundManager::reinitHardware() {
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
    gpio_deep_sleep_hold_dis();
    gpio_hold_dis((gpio_num_t)BUZZER_PIN);
    pinMode(BUZZER_PIN, OUTPUT);
    ledcSetup(BUZZER_LEDC_CHANNEL, 2700, BUZZER_LEDC_RES_BITS);
    ledcAttachPin(BUZZER_PIN, BUZZER_LEDC_CHANNEL);
    ledcWrite(BUZZER_LEDC_CHANNEL, 0);
#endif
    is_playing = false;
    is_tone_active = false;
    active_duty_pct = 0;
}

void SoundManager::begin() {
    reinitHardware();
}

bool SoundManager::isMasterSwitchOn() const {
    return settingsManager.get().sound_master_on;
}

void SoundManager::setMasterSwitch(bool state) {
    settingsManager.get().sound_master_on = state;
    settingsManager.save();
    if (!state) {
        stop();
    }
}

int SoundManager::getVolumePct() const {
    return settingsManager.get().volume_pct;
}

void SoundManager::setVolumePct(int pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    settingsManager.get().volume_pct = pct;
    settingsManager.save();
}

bool SoundManager::isButtonSoundsEnabled() const {
    return settingsManager.get().button_sounds_on;
}

void SoundManager::setButtonSoundsEnabled(bool enabled) {
    settingsManager.get().button_sounds_on = enabled;
    settingsManager.save();
}

bool SoundManager::isNotificationsEnabled() const {
    return settingsManager.get().notifications_on;
}

void SoundManager::setNotificationsEnabled(bool enabled) {
    settingsManager.get().notifications_on = enabled;
    settingsManager.save();
}

SoundStyle SoundManager::getStyle() const {
    return (SoundStyle)settingsManager.get().sound_style_idx;
}

void SoundManager::setStyle(SoundStyle style) {
    settingsManager.get().sound_style_idx = (int)style;
    settingsManager.save();
}

void SoundManager::stop() {
    ledcWrite(BUZZER_LEDC_CHANNEL, 0);
    is_playing = false;
    is_tone_active = false;
    active_duty_pct = 0;
}

void SoundManager::applyPwmTone(uint16_t freq, uint8_t duty_pct) {
    if (freq == 0 || duty_pct == 0) {
        ledcWrite(BUZZER_LEDC_CHANNEL, 0);
        return;
    }
    ledcWriteTone(BUZZER_LEDC_CHANNEL, freq);
    uint32_t duty_val = (uint32_t)duty_pct * 255 / 100;
    ledcWrite(BUZZER_LEDC_CHANNEL, duty_val);
}

void SoundManager::playSequence(const SoundNote* sequence, uint8_t length) {
    if (!isMasterSwitchOn() || getStyle() == SoundStyle::SILENT || getVolumePct() == 0) return;
    current_sequence = sequence;
    sequence_length = length;
    current_note_index = 0;
    note_start_time = millis();
    is_playing = true;
    startCurrentNote();
}

void SoundManager::playTone(uint16_t freq, uint16_t duration_ms, uint8_t duty_pct) {
    if (!isMasterSwitchOn() || getVolumePct() == 0) return;
    active_duty_pct = duty_pct;
    dynamic_sequence[0] = { freq, duration_ms };
    playSequence(dynamic_sequence, 1);
}

void SoundManager::setToneDuty(uint16_t freq, uint8_t duty_pct) {
    applyPwmTone(freq, duty_pct);
}

void SoundManager::startCurrentNote() {
    if (current_note_index >= sequence_length) {
        stop();
        return;
    }

    uint16_t raw_freq = current_sequence[current_note_index].freq;
    int vol = getVolumePct();

    if (raw_freq > 0 && vol > 0) {
        uint8_t duty = 0;
        if (active_duty_pct > 0) {
            duty = active_duty_pct;
        } else {
            duty = (uint8_t)(vol * 50 / 100);
            if (duty == 0) duty = 1;
        }
        applyPwmTone(raw_freq, duty);
        is_tone_active = true;
    } else {
        applyPwmTone(0, 0);
        is_tone_active = false;
    }
    note_start_time = millis();
}

void SoundManager::loop() {
    if (!is_playing) return;

    uint32_t elapsed = millis() - note_start_time;

    if (elapsed >= current_sequence[current_note_index].duration) {
        current_note_index++;
        if (current_note_index >= sequence_length) {
            stop();
        } else {
            active_duty_pct = 0; // Reset explicit duty for subsequent notes in sequence
            startCurrentNote();
        }
    }
}

void SoundManager::playBoot() { playSequence(seq_boot, 3); }
void SoundManager::playWake() { playSequence(seq_wake, 2); }
void SoundManager::playSleep() { playSequence(seq_sleep, 2); }
void SoundManager::playNotification() { if (isNotificationsEnabled()) playSequence(seq_notify, 3); }
void SoundManager::playWarning() { playSequence(seq_warning, 3); }
void SoundManager::playAlert() { playSequence(seq_alert, 5); }
void SoundManager::playQBranch() { playSequence(seq_qbranch, 4); }

void SoundManager::playMario() { playSequence(seq_mario, seq_mario_len); }
void SoundManager::playChristmas() { playSequence(seq_xmas, seq_xmas_len); }
void SoundManager::playBirthday() { playSequence(seq_birthday, seq_birthday_len); }
void SoundManager::playTetris() { playSequence(seq_tetris, seq_tetris_len); }
void SoundManager::playStarWars() { playSequence(seq_starwars, seq_starwars_len); }

void SoundManager::playTuneByIdx(uint8_t idx) {
    switch (idx) {
        case 0: playMario(); break;
        case 1: playChristmas(); break;
        case 2: playBirthday(); break;
        case 3: playTetris(); break;
        case 4: playStarWars(); break;
    }
}

const char* SoundManager::getTuneName(uint8_t idx) const {
    if (idx < TUNE_COUNT) return TUNE_NAMES[idx];
    return "UNKNOWN";
}

void SoundManager::playKick() {
    if (!isMasterSwitchOn() || getVolumePct() == 0) return;
    static const SoundNote seq_kick[] = { {160, 15}, {110, 15}, {60, 20} };
    playSequence(seq_kick, 3);
}

void SoundManager::playSnare() {
    if (!isMasterSwitchOn() || getVolumePct() == 0) return;
    static const SoundNote seq_snare[] = { {1800, 12}, {900, 12}, {450, 15} };
    playSequence(seq_snare, 3);
}

void SoundManager::playHiHat() {
    if (!isMasterSwitchOn() || getVolumePct() == 0) return;
    static const SoundNote seq_hat[] = { {6200, 8} };
    playSequence(seq_hat, 1);
}

void SoundManager::playSynthNote(uint8_t note_idx) {
    if (!isMasterSwitchOn() || getVolumePct() == 0) return;
    static const uint16_t SYNTH_PENTATONIC[8] = { 523, 587, 659, 784, 880, 1046, 1174, 1318 };
    playTone(SYNTH_PENTATONIC[note_idx % 8], 60);
}

void SoundManager::playNavMove() {
    if (!isButtonSoundsEnabled()) return;
    SoundStyle st = getStyle();
    if (st == SoundStyle::MODERN) playSequence(seq_mod_move, 1);
    else if (st == SoundStyle::TACTICAL) playSequence(seq_tac_move, 1);
    else if (st == SoundStyle::RETRO) playSequence(seq_ret_move, 1);
}

void SoundManager::playNavSelect() {
    if (!isButtonSoundsEnabled()) return;
    SoundStyle st = getStyle();
    if (st == SoundStyle::MODERN) playSequence(seq_mod_sel, 1);
    else if (st == SoundStyle::TACTICAL) playSequence(seq_tac_sel, 1);
    else if (st == SoundStyle::RETRO) playSequence(seq_ret_sel, 2);
}

void SoundManager::playNavBack() {
    if (!isButtonSoundsEnabled()) return;
    SoundStyle st = getStyle();
    if (st == SoundStyle::MODERN) playSequence(seq_mod_back, 1);
    else if (st == SoundStyle::TACTICAL) playSequence(seq_tac_back, 1);
    else if (st == SoundStyle::RETRO) playSequence(seq_ret_back, 2);
}

void SoundManager::playAppLaunch() {
    if (!isButtonSoundsEnabled()) return;
    playSequence(seq_app_launch, 1);
}

void SoundManager::playMorse(const String& text) {
    uint8_t idx = 0;
    uint16_t dot = 70;
    uint16_t dash = 210;
    uint16_t element_gap = 70;
    uint16_t letter_gap = 210;

    String upper = text;
    upper.toUpperCase();

    for (size_t i = 0; i < upper.length() && idx < 60; i++) {
        char c = upper.charAt(i);
        if (c == ' ') {
            dynamic_sequence[idx++] = { 0, (uint16_t)(letter_gap * 2) };
            continue;
        }

        const char* code = "";
        switch (c) {
            case 'A': code = ".-"; break;
            case 'B': code = "-..."; break;
            case 'C': code = "-.-."; break;
            case 'D': code = "-.."; break;
            case 'E': code = "."; break;
            case 'F': code = "..-."; break;
            case 'G': code = "--."; break;
            case 'H': code = "...."; break;
            case 'I': code = ".."; break;
            case 'J': code = ".---"; break;
            case 'K': code = "-.-"; break;
            case 'L': code = ".-.."; break;
            case 'M': code = "--"; break;
            case 'N': code = "-."; break;
            case 'O': code = "---"; break;
            case 'P': code = ".--."; break;
            case 'Q': code = "--.-"; break;
            case 'R': code = ".-."; break;
            case 'S': code = "..."; break;
            case 'T': code = "-"; break;
            case 'U': code = "..-"; break;
            case 'V': code = "...-"; break;
            case 'W': code = ".--"; break;
            case 'X': code = "-..-"; break;
            case 'Y': code = "-.--"; break;
            case 'Z': code = "--.."; break;
            case '1': code = ".----"; break;
            case '2': code = "..---"; break;
            case '3': code = "...--"; break;
            case '4': code = "....-"; break;
            case '5': code = "....."; break;
            case '6': code = "-...."; break;
            case '7': code = "--..."; break;
            case '8': code = "---.."; break;
            case '9': code = "----."; break;
            case '0': code = "-----"; break;
            default: continue;
        }

        for (size_t j = 0; code[j] != '\0' && idx < 60; j++) {
            uint16_t dur = (code[j] == '.') ? dot : dash;
            dynamic_sequence[idx++] = { 2700, dur };
            dynamic_sequence[idx++] = { 0, element_gap };
        }
        dynamic_sequence[idx++] = { 0, letter_gap };
    }

    if (idx > 0) {
        playSequence(dynamic_sequence, idx);
    }
}
