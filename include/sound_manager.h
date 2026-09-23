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

    // Direct Tone Control & SW Volume / Duty Control
    void playTone(uint16_t freq, uint16_t duration_ms, uint8_t duty_pct = 0);
    void setToneDuty(uint16_t freq, uint8_t duty_pct);
    void stop();

    // Sound Effects
    void playBoot();
    void playWake();
    void playSleep();
    void playNotification();
    void playWarning();
    void playAlert();
    void playQBranch();

    // UI Navigation Feedback
    void playNavMove();
    void playNavSelect();
    void playNavBack();

    // Dynamic Melody Sequence Playback
    void playSequence(const SoundNote* sequence, uint8_t length);

    // Morse Code Playback
    void playMorse(const String& text);

    // Settings Sync & Controls
    void setMasterSwitch(bool state);
    bool isMasterSwitchOn() const;

    void setVolumePct(int pct);
    int getVolumePct() const;

    void setButtonSoundsEnabled(bool enabled);
    bool isButtonSoundsEnabled() const;

    void setNotificationsEnabled(bool enabled);
    bool isNotificationsEnabled() const;

    void setStyle(SoundStyle style);
    SoundStyle getStyle() const;

    bool isPlaying() const { return is_playing; }

private:
    void startCurrentNote();
    void applyPwmTone(uint16_t freq, uint8_t duty_pct);

    SoundNote dynamic_sequence[64];
    const SoundNote* current_sequence;
    uint8_t sequence_length;
    uint8_t current_note_index;
    uint32_t note_start_time;
    uint8_t active_duty_pct;
    bool is_playing;
    bool is_tone_active;
};

extern SoundManager soundManager;

#endif
