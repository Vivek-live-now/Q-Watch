#include "sound_manager.h"
#include "hw_config.h"
#include "settings_data.h"
#include "file_manager.h"

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

const SoundNote seq_err[] = { {1200, 120}, {0, 30}, {1000, 140} };
const SoundNote seq_succ[] = { {2200, 80}, {2700, 80}, {3200, 120} };

SoundManager::SoundManager() :
    current_sequence(nullptr),
    sequence_length(0),
    current_note_index(0),
    note_start_time(0),
    active_duration(0),
    is_playing(false),
    is_tone_active(false)
{}

void SoundManager::begin() {
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
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
    noTone(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, LOW);
    is_playing = false;
    is_tone_active = false;
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

void SoundManager::playTone(uint16_t freq, uint16_t duration_ms) {
    if (!isMasterSwitchOn() || getVolumePct() == 0) return;
    dynamic_sequence[0] = { freq, duration_ms };
    playSequence(dynamic_sequence, 1);
}

void SoundManager::startCurrentNote() {
    if (current_note_index >= sequence_length) {
        stop();
        return;
    }

    uint16_t raw_freq = current_sequence[current_note_index].freq;
    uint16_t dur = current_sequence[current_note_index].duration;
    int vol = getVolumePct();

    if (raw_freq > 0 && vol > 0) {
        active_duration = (uint16_t)((uint32_t)dur * vol / 100);
        if (active_duration == 0) active_duration = 1;
        tone(BUZZER_PIN, raw_freq);
        is_tone_active = true;
    } else {
        active_duration = dur;
        noTone(BUZZER_PIN);
        digitalWrite(BUZZER_PIN, LOW);
        is_tone_active = false;
    }
    note_start_time = millis();
}

void SoundManager::loop() {
    if (!is_playing) return;

    uint32_t elapsed = millis() - note_start_time;

    if (is_tone_active && elapsed >= active_duration) {
        noTone(BUZZER_PIN);
        digitalWrite(BUZZER_PIN, LOW);
        is_tone_active = false;
    }

    if (elapsed >= current_sequence[current_note_index].duration) {
        current_note_index++;
        if (current_note_index >= sequence_length) {
            stop();
        } else {
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

void SoundManager::playError() { playSequence(seq_err, 3); }
void SoundManager::playSuccess() { playSequence(seq_succ, 3); }

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

bool SoundManager::playMelodyFile(const String& path) {
    if (!fileManager.exists(path)) return false;
    String content = fileManager.read(path);
    if (content.length() == 0) return false;

    uint8_t idx = 0;
    int pos = 0;
    while (pos < content.length() && idx < 64) {
        int next_nl = content.indexOf('\n', pos);
        if (next_nl == -1) next_nl = content.length();
        String line = content.substring(pos, next_nl);
        line.trim();
        pos = next_nl + 1;

        if (line.length() == 0 || line.startsWith("#")) continue;

        int comma = line.indexOf(',');
        if (comma != -1) {
            uint16_t freq = line.substring(0, comma).toInt();
            uint16_t dur = line.substring(comma + 1).toInt();
            dynamic_sequence[idx++] = { freq, dur };
        }
    }

    if (idx > 0) {
        playSequence(dynamic_sequence, idx);
        return true;
    }
    return false;
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
