#include "sound_manager.h"
#include "hw_config.h"
#include <Preferences.h>

SoundManager soundManager;
Preferences soundPrefs;

// Predefined sequences
const SoundNote seq_boot[] = { {1000, 100}, {1500, 100}, {2000, 200} };

// Modern
const SoundNote seq_mod_move[] = { {800, 20} };
const SoundNote seq_mod_sel[] = { {1200, 30} };
const SoundNote seq_mod_back[] = { {600, 40} };

// Tactical
const SoundNote seq_tac_move[] = { {200, 10} };
const SoundNote seq_tac_sel[] = { {400, 10} };
const SoundNote seq_tac_back[] = { {150, 20} };

// Retro
const SoundNote seq_ret_move[] = { {440, 50} };
const SoundNote seq_ret_sel[] = { {880, 50}, {1760, 50} };
const SoundNote seq_ret_back[] = { {440, 50}, {220, 50} };

const SoundNote seq_err[] = { {200, 150}, {0, 50}, {200, 150} };
const SoundNote seq_succ[] = { {800, 100}, {1200, 100}, {1600, 150} };

SoundManager::SoundManager() :
    master_switch(true),
    current_style(SoundStyle::MODERN),
    current_sequence(nullptr),
    sequence_length(0),
    current_note_index(0),
    note_start_time(0),
    is_playing(false)
{}

void SoundManager::begin() {
    pinMode(BUZZER_PIN, OUTPUT);

    soundPrefs.begin("sound", false);
    master_switch = soundPrefs.getBool("on", true);
    current_style = (SoundStyle)soundPrefs.getInt("style", 1);
    soundPrefs.end();
}

void SoundManager::setMasterSwitch(bool state) {
    master_switch = state;
    soundPrefs.begin("sound", false);
    soundPrefs.putBool("on", master_switch);
    soundPrefs.end();
    if (!master_switch) {
        noTone(BUZZER_PIN);
        is_playing = false;
    }
}

void SoundManager::setStyle(SoundStyle style) {
    current_style = style;
    soundPrefs.begin("sound", false);
    soundPrefs.putInt("style", (int)current_style);
    soundPrefs.end();
}

void SoundManager::playSequence(const SoundNote* sequence, uint8_t length) {
    if (!master_switch || current_style == SoundStyle::SILENT) return;
    current_sequence = sequence;
    sequence_length = length;
    current_note_index = 0;
    note_start_time = millis();
    is_playing = true;

    if (current_sequence[0].freq > 0) {
        tone(BUZZER_PIN, current_sequence[0].freq);
    } else {
        noTone(BUZZER_PIN);
    }
}

void SoundManager::playBoot() { playSequence(seq_boot, 3); }
void SoundManager::playError() { playSequence(seq_err, 3); }
void SoundManager::playSuccess() { playSequence(seq_succ, 3); }

void SoundManager::playNavMove() {
    if (current_style == SoundStyle::MODERN) playSequence(seq_mod_move, 1);
    else if (current_style == SoundStyle::TACTICAL) playSequence(seq_tac_move, 1);
    else if (current_style == SoundStyle::RETRO) playSequence(seq_ret_move, 1);
}

void SoundManager::playNavSelect() {
    if (current_style == SoundStyle::MODERN) playSequence(seq_mod_sel, 1);
    else if (current_style == SoundStyle::TACTICAL) playSequence(seq_tac_sel, 1);
    else if (current_style == SoundStyle::RETRO) playSequence(seq_ret_sel, 2);
}

void SoundManager::playNavBack() {
    if (current_style == SoundStyle::MODERN) playSequence(seq_mod_back, 1);
    else if (current_style == SoundStyle::TACTICAL) playSequence(seq_tac_back, 1);
    else if (current_style == SoundStyle::RETRO) playSequence(seq_ret_back, 2);
}

void SoundManager::loop() {
    if (!is_playing) return;

    uint32_t now = millis();
    if (now - note_start_time >= current_sequence[current_note_index].duration) {
        current_note_index++;

        if (current_note_index >= sequence_length) {
            is_playing = false;
            noTone(BUZZER_PIN);
        } else {
            note_start_time = now;
            if (current_sequence[current_note_index].freq > 0) {
                tone(BUZZER_PIN, current_sequence[current_note_index].freq);
            } else {
                noTone(BUZZER_PIN);
            }
        }
    }
}
