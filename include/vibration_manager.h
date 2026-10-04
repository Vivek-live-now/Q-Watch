#ifndef VIBRATION_MANAGER_H
#define VIBRATION_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef ARDUINO
#include <Arduino.h>
#endif

enum class VibePattern {
    CLICK = 0,       // 35ms crisp haptic tap (ideal for button feedback)
    DOUBLE_PULSE,    // Two 50ms pulses with 70ms pause (notification)
    ALERT,           // 220ms strong tactile buzz (alarm / warning)
    HEARTBEAT,       // Lub-dub cardiac rhythm (60ms on, 80ms pause, 80ms on)
    SOS_MORSE,       // ··· ——— ··· Morse code sequence
    RAMP_UP,         // PWM acceleration ramp (20% to 100% duty)
    CONTINUOUS,      // Sustained vibration for circuit validation
    COUNT
};

struct VibeStep {
    uint8_t duty_pct;      // 0 to 100% duty cycle
    uint16_t duration_ms;  // Step duration in milliseconds
};

class VibrationManager {
public:
    VibrationManager();
    void begin();
    void loop();

    // Trigger predefined patterns
    void triggerPattern(VibePattern pattern);
    void triggerPulse(uint16_t duration_ms, uint8_t duty_pct = 100);
    void triggerHapticClick();
    void stop();

    // Configuration & State
    bool isVibrating() const { return is_active; }
    VibePattern getCurrentPattern() const { return current_pattern; }
    const char* getPatternName(VibePattern pattern) const;

    uint8_t getIntensity() const { return intensity_pct; }
    void setIntensity(uint8_t pct);

    bool isMasterSwitchOn() const { return master_switch; }
    void setMasterSwitch(bool en);

    bool isButtonHapticsEnabled() const { return button_haptics; }
    void setButtonHaptics(bool en);

    // Live wave amplitude for OLED oscilloscope visualization (0 to 100)
    uint8_t getLiveAmplitude() const;

private:
    void applyPwmDuty(uint8_t duty_pct);

    bool master_switch;
    bool button_haptics;
    uint8_t intensity_pct;

    bool is_active;
    VibePattern current_pattern;
    uint8_t active_step_duty;

    // Pattern step sequence buffer (up to 24 steps for SOS)
    static const size_t MAX_STEPS = 24;
    VibeStep step_sequence[MAX_STEPS];
    size_t sequence_len;
    size_t current_step_idx;
    uint32_t step_start_time;

    void loadPatternSteps(VibePattern pattern);
};

extern VibrationManager vibrationManager;

#endif // VIBRATION_MANAGER_H
