#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>

// ============================================================================
// Q-WATCH POWER MANAGEMENT SUITE & POWER PROFILES
// ============================================================================

enum class PowerProfile : uint8_t {
    PERFORMANCE = 0, // 240 MHz, Display Off only on timeout, radios active
    BALANCED = 1,    // 160 MHz dynamic, Light Sleep (300µs wake), 24/7 steps
    ENDURANCE = 2,   // 80 MHz eco, Deep Sleep (15µA), radios killed, muted
    CUSTOM = 3       // User-defined parameters
};

enum class SleepEngine : uint8_t {
    LIGHT_SLEEP = 0, // esp_light_sleep_start(): instant 300µs wake, RAM retained
    DEEP_SLEEP = 1,  // esp_deep_sleep_start(): 15µA draw, cold boot on wake
    DISPLAY_OFF = 2, // oled.setPowerSave(1), CPU yields with waiti idling
    ULP_SENTINEL = 3 // Experimental ULP RISC-V sentry
};

struct UlpTelemetry {
    bool active;
    uint16_t rtc_mem_used_bytes;
    uint32_t wake_count;
    uint16_t last_adc_raw;
    const char* status_str;
};

class PowerManager {
public:
    PowerManager();

    void begin();

    // Profile Management
    PowerProfile getProfile() const { return current_profile; }
    void setProfile(PowerProfile p);
    const char* getProfileNameCStr(PowerProfile p) const;
    const char* getProfileNameCStr(int idx) const;

    // Sleep Engine Configuration
    SleepEngine getSleepEngine() const { return current_sleep_engine; }
    void setSleepEngine(SleepEngine engine);
    const char* getSleepEngineNameCStr(SleepEngine engine) const;
    const char* getSleepEngineNameCStr(int idx) const;

    // CPU Frequency Scaling
    uint32_t getTargetCpuFreqMhz() const;
    void applyCpuFrequency();

    // Runtime Estimation & Consumption Model (Nominal 300mAh LiPo)
    float calculateEstimatedRuntimeHours(float voltage, int percentage) const;
    void formatRuntimeEstimate(char* buf, size_t max_len, float voltage, int percentage) const;
    float getEstimatedCurrentMa() const;

    // Peripheral Eco Controls
    bool isPedometer247Enabled() const { return pedometer_247_enabled; }
    void setPedometer247Enabled(bool en) { pedometer_247_enabled = en; }

    bool isEcoRadioCutEnabled() const { return eco_radio_cut; }
    void setEcoRadioCutEnabled(bool en) { eco_radio_cut = en; }

    bool isEcoLedBlockEnabled() const { return eco_led_block; }
    void setEcoLedBlockEnabled(bool en) { eco_led_block = en; }

    bool isEcoAudioMuteEnabled() const { return eco_audio_mute; }
    void setEcoAudioMuteEnabled(bool en) { eco_audio_mute = en; }

    // ULP Lab / Diagnostic Interface
    UlpTelemetry getUlpTelemetry() const;
    void triggerUlpSentryTest();
    bool isUlpEnabled() const { return ulp_sentry_active; }
    void setUlpEnabled(bool en);

    // Sleep Execution Dispatcher
    void executeSleep(uint32_t sleep_sec, bool raise_to_wake, uint64_t wake_mask);

private:
    PowerProfile current_profile;
    SleepEngine current_sleep_engine;
    bool pedometer_247_enabled;
    bool eco_radio_cut;
    bool eco_led_block;
    bool eco_audio_mute;

    // ULP Diagnostic State
    uint32_t ulp_test_wake_count;
    bool ulp_sentry_active;
};

extern PowerManager powerManager;

#endif // POWER_MANAGER_H
