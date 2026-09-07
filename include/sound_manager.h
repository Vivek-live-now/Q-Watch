#ifndef SOUND_MANAGER_H
#define SOUND_MANAGER_H

#include <Arduino.h>

struct SoundNote {
    uint16_t freq;
    uint16_t duration;
};

enum class SoundStyle {
    SILENT = 0,
    MODERN = 1,
    TACTICAL = 2,
    RETRO = 3
};

class SoundManager {
public:
    SoundManager();
    void begin();
    void loop();

    void playBoot();
    void playNavMove();
    void playNavSelect();
    void playNavBack();
    void playError();
    void playSuccess();

    void setMasterSwitch(bool state);
    bool isMasterSwitchOn() const { return master_switch; }

    void setStyle(SoundStyle style);
    SoundStyle getStyle() const { return current_style; }

private:
    void playSequence(const SoundNote* sequence, uint8_t length);

    bool master_switch;
    SoundStyle current_style;

    const SoundNote* current_sequence;
    uint8_t sequence_length;
    uint8_t current_note_index;
    uint32_t note_start_time;
    bool is_playing;
};

extern SoundManager soundManager;

#endif
