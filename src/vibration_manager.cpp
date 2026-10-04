#include "vibration_manager.h"
#include "hw_config.h"

#ifdef ARDUINO
#include "settings_data.h"
#endif

VibrationManager vibrationManager;

#define VIBE_LEDC_CHANNEL   1
#define VIBE_LEDC_FREQ      1000
#define VIBE_LEDC_RES_BITS  8

VibrationManager::VibrationManager() :
    master_switch(true),
    button_haptics(true),
    intensity_pct(100),
    is_active(false),
    current_pattern(VibePattern::CLICK),
    active_step_duty(0),
    sequence_len(0),
    current_step_idx(0),
    step_start_time(0)
{
}

void VibrationManager::begin() {
#ifdef ARDUINO
    pinMode(VIBRATOR_PIN, OUTPUT);
    digitalWrite(VIBRATOR_PIN, LOW);
    ledcSetup(VIBE_LEDC_CHANNEL, VIBE_LEDC_FREQ, VIBE_LEDC_RES_BITS);
    ledcAttachPin(VIBRATOR_PIN, VIBE_LEDC_CHANNEL);
    ledcWrite(VIBE_LEDC_CHANNEL, 0);

    master_switch = settingsManager.get().vibe_master_on;
    intensity_pct = settingsManager.get().vibe_intensity;
    button_haptics = settingsManager.get().vibe_button_clicks;
#endif
    is_active = false;
    active_step_duty = 0;
}

void VibrationManager::applyPwmDuty(uint8_t duty_pct) {
    active_step_duty = duty_pct;
#ifdef ARDUINO
    if (!master_switch || duty_pct == 0) {
        ledcWrite(VIBE_LEDC_CHANNEL, 0);
        return;
    }
    // Scale duty by user intensity preference (0-100%)
    uint32_t effective_pct = (static_cast<uint32_t>(duty_pct) * intensity_pct) / 100;
    uint32_t val = (effective_pct * 255) / 100;
    ledcWrite(VIBE_LEDC_CHANNEL, val);
#endif
}

void VibrationManager::stop() {
    applyPwmDuty(0);
    is_active = false;
    sequence_len = 0;
    current_step_idx = 0;
}

void VibrationManager::setIntensity(uint8_t pct) {
    if (pct > 100) pct = 100;
    intensity_pct = pct;
#ifdef ARDUINO
    settingsManager.get().vibe_intensity = pct;
    settingsManager.save();
#endif
    if (is_active && active_step_duty > 0) {
        applyPwmDuty(active_step_duty);
    }
}

void VibrationManager::setMasterSwitch(bool en) {
    master_switch = en;
#ifdef ARDUINO
    settingsManager.get().vibe_master_on = en;
    settingsManager.save();
#endif
    if (!en) {
        stop();
    }
}

void VibrationManager::setButtonHaptics(bool en) {
    button_haptics = en;
#ifdef ARDUINO
    settingsManager.get().vibe_button_clicks = en;
    settingsManager.save();
#endif
}

void VibrationManager::triggerHapticClick() {
    if (master_switch && button_haptics) {
        triggerPattern(VibePattern::CLICK);
    }
}

void VibrationManager::triggerPulse(uint16_t duration_ms, uint8_t duty_pct) {
    if (!master_switch) return;
    sequence_len = 1;
    current_step_idx = 0;
    step_sequence[0].duty_pct = duty_pct;
    step_sequence[0].duration_ms = duration_ms;
    current_pattern = VibePattern::CLICK;
    is_active = true;
#ifdef ARDUINO
    step_start_time = millis();
#else
    step_start_time = 0;
#endif
    applyPwmDuty(duty_pct);
}

void VibrationManager::loadPatternSteps(VibePattern pattern) {
    current_pattern = pattern;
    sequence_len = 0;
    current_step_idx = 0;

    switch (pattern) {
        case VibePattern::CLICK:
            step_sequence[0] = {100, 35};
            sequence_len = 1;
            break;

        case VibePattern::DOUBLE_PULSE:
            step_sequence[0] = {100, 50};
            step_sequence[1] = {0,   70};
            step_sequence[2] = {100, 50};
            sequence_len = 3;
            break;

        case VibePattern::ALERT:
            step_sequence[0] = {100, 220};
            sequence_len = 1;
            break;

        case VibePattern::HEARTBEAT:
            step_sequence[0] = {70,  60};
            step_sequence[1] = {0,   80};
            step_sequence[2] = {100, 80};
            sequence_len = 3;
            break;

        case VibePattern::SOS_MORSE:
            // 3 dots (60ms on, 50ms off)
            step_sequence[0]  = {100, 60}; step_sequence[1]  = {0, 50};
            step_sequence[2]  = {100, 60}; step_sequence[3]  = {0, 50};
            step_sequence[4]  = {100, 60}; step_sequence[5]  = {0, 140}; // letter gap
            // 3 dashes (160ms on, 50ms off)
            step_sequence[6]  = {100, 160}; step_sequence[7]  = {0, 50};
            step_sequence[8]  = {100, 160}; step_sequence[9]  = {0, 50};
            step_sequence[10] = {100, 160}; step_sequence[11] = {0, 140}; // letter gap
            // 3 dots (60ms on, 50ms off)
            step_sequence[12] = {100, 60}; step_sequence[13] = {0, 50};
            step_sequence[14] = {100, 60}; step_sequence[15] = {0, 50};
            step_sequence[16] = {100, 60};
            sequence_len = 17;
            break;

        case VibePattern::RAMP_UP:
            step_sequence[0] = {20,  60};
            step_sequence[1] = {40,  60};
            step_sequence[2] = {60,  60};
            step_sequence[3] = {80,  60};
            step_sequence[4] = {100, 120};
            sequence_len = 5;
            break;

        case VibePattern::CONTINUOUS:
            step_sequence[0] = {100, 10000}; // 10 seconds continuous test limit
            sequence_len = 1;
            break;

        default:
            step_sequence[0] = {100, 50};
            sequence_len = 1;
            break;
    }
}

void VibrationManager::triggerPattern(VibePattern pattern) {
    if (!master_switch) return;
    loadPatternSteps(pattern);
    if (sequence_len > 0) {
        is_active = true;
#ifdef ARDUINO
        step_start_time = millis();
#else
        step_start_time = 0;
#endif
        applyPwmDuty(step_sequence[0].duty_pct);
    }
}

void VibrationManager::loop() {
    if (!is_active || sequence_len == 0) return;

#ifdef ARDUINO
    uint32_t now = millis();
    if (now - step_start_time >= step_sequence[current_step_idx].duration_ms) {
        current_step_idx++;
        if (current_step_idx < sequence_len) {
            step_start_time = now;
            applyPwmDuty(step_sequence[current_step_idx].duty_pct);
        } else {
            stop();
        }
    }
#endif
}

const char* VibrationManager::getPatternName(VibePattern pattern) const {
    switch (pattern) {
        case VibePattern::CLICK:        return "CLICK / TICK";
        case VibePattern::DOUBLE_PULSE: return "DOUBLE PULSE";
        case VibePattern::ALERT:        return "TACTICAL ALERT";
        case VibePattern::HEARTBEAT:    return "HEARTBEAT";
        case VibePattern::SOS_MORSE:    return "SOS MORSE";
        case VibePattern::RAMP_UP:      return "RAMP INTENSITY";
        case VibePattern::CONTINUOUS:   return "CONTINUOUS RUN";
        default:                        return "UNKNOWN";
    }
}

uint8_t VibrationManager::getLiveAmplitude() const {
    if (!is_active) return 0;
    return active_step_duty;
}
