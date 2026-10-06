#include "power_manager.h"
#include "settings_data.h"
#include <stdio.h>
#include <math.h>

#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#endif

static const char* const PROFILE_NAMES[] = {
    "PERFORMANCE",
    "BALANCED",
    "ENDURANCE",
    "CUSTOM"
};

static const char* const SLEEP_ENGINE_NAMES[] = {
    "LIGHT SLEEP",
    "DEEP SLEEP",
    "DISPLAY OFF",
    "ULP SENTRY"
};

PowerManager powerManager;

PowerManager::PowerManager() :
    current_profile(PowerProfile::BALANCED),
    current_sleep_engine(SleepEngine::LIGHT_SLEEP),
    pedometer_247_enabled(true),
    eco_radio_cut(true),
    eco_led_block(false),
    eco_audio_mute(false),
    ulp_test_wake_count(0),
    ulp_sentry_active(false)
{}

void PowerManager::begin() {
    SettingsData& s = settingsManager.get();
    if (s.power_profile_idx >= 0 && s.power_profile_idx <= 3) {
        current_profile = static_cast<PowerProfile>(s.power_profile_idx);
    } else {
        current_profile = PowerProfile::BALANCED;
    }

    if (s.sleep_engine_idx >= 0 && s.sleep_engine_idx <= 3) {
        current_sleep_engine = static_cast<SleepEngine>(s.sleep_engine_idx);
    } else {
        current_sleep_engine = SleepEngine::LIGHT_SLEEP;
    }

    pedometer_247_enabled = s.pedometer_247;
    eco_radio_cut = s.eco_radio_cut;
    eco_led_block = s.eco_led_block;
    eco_audio_mute = s.eco_audio_mute;
    ulp_sentry_active = s.ulp_sentry_enabled;

    applyCpuFrequency();
}

void PowerManager::setProfile(PowerProfile p) {
    current_profile = p;
    SettingsData& s = settingsManager.get();
    s.power_profile_idx = static_cast<int>(p);

    switch (p) {
        case PowerProfile::PERFORMANCE:
            current_sleep_engine = SleepEngine::DISPLAY_OFF;
            pedometer_247_enabled = true;
            eco_radio_cut = false;
            eco_led_block = false;
            eco_audio_mute = false;
            break;

        case PowerProfile::BALANCED:
            current_sleep_engine = SleepEngine::LIGHT_SLEEP;
            pedometer_247_enabled = true;
            eco_radio_cut = true;
            eco_led_block = false;
            eco_audio_mute = false;
            break;

        case PowerProfile::ENDURANCE:
            current_sleep_engine = SleepEngine::DEEP_SLEEP;
            pedometer_247_enabled = false;
            eco_radio_cut = true;
            eco_led_block = true;
            eco_audio_mute = true;
            break;

        case PowerProfile::CUSTOM:
            // Keeps existing fine-grained settings
            break;
    }

    s.sleep_engine_idx = static_cast<int>(current_sleep_engine);
    s.pedometer_247 = pedometer_247_enabled;
    s.eco_radio_cut = eco_radio_cut;
    s.eco_led_block = eco_led_block;
    s.eco_audio_mute = eco_audio_mute;

    settingsManager.save();
    applyCpuFrequency();
}

const char* PowerManager::getProfileNameCStr(PowerProfile p) const {
    uint8_t idx = static_cast<uint8_t>(p);
    if (idx < 4) return PROFILE_NAMES[idx];
    return "UNKNOWN";
}

const char* PowerManager::getProfileNameCStr(int idx) const {
    if (idx >= 0 && idx < 4) return PROFILE_NAMES[idx];
    return "UNKNOWN";
}

void PowerManager::setSleepEngine(SleepEngine engine) {
    current_sleep_engine = engine;
    SettingsData& s = settingsManager.get();
    s.sleep_engine_idx = static_cast<int>(engine);
    if (current_profile != PowerProfile::CUSTOM) {
        current_profile = PowerProfile::CUSTOM;
        s.power_profile_idx = static_cast<int>(PowerProfile::CUSTOM);
    }
    settingsManager.save();
}

const char* PowerManager::getSleepEngineNameCStr(SleepEngine engine) const {
    uint8_t idx = static_cast<uint8_t>(engine);
    if (idx < 4) return SLEEP_ENGINE_NAMES[idx];
    return "UNKNOWN";
}

const char* PowerManager::getSleepEngineNameCStr(int idx) const {
    if (idx >= 0 && idx < 4) return SLEEP_ENGINE_NAMES[idx];
    return "UNKNOWN";
}

uint32_t PowerManager::getTargetCpuFreqMhz() const {
    switch (current_profile) {
        case PowerProfile::PERFORMANCE: return 240;
        case PowerProfile::BALANCED:    return 160;
        case PowerProfile::ENDURANCE:   return 80;
        case PowerProfile::CUSTOM:
            if (current_sleep_engine == SleepEngine::DEEP_SLEEP) return 80;
            if (current_sleep_engine == SleepEngine::DISPLAY_OFF) return 240;
            return 160;
    }
    return 160;
}

void PowerManager::applyCpuFrequency() {
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
    uint32_t target_mhz = getTargetCpuFreqMhz();
    if (getCpuFrequencyMhz() != target_mhz) {
        setCpuFrequencyMhz(target_mhz);
    }
#endif
}

float PowerManager::getEstimatedCurrentMa() const {
    // Current consumption model for ESP32-S3 + OLED + Sensors:
    // Nominal battery capacity: 300 mAh
    switch (current_profile) {
        case PowerProfile::PERFORMANCE:
            // 240 MHz active, display sleep only, radios ready: ~25.0 mA blended avg
            return 25.0f;

        case PowerProfile::BALANCED:
            // 160 MHz active (45mA) on glance, Light Sleep (~1.2mA) during screen off: ~3.0 mA blended avg
            return 3.0f;

        case PowerProfile::ENDURANCE:
            // 80 MHz eco, Deep Sleep (15µA) during screen off, muted: ~0.65 mA blended avg
            return 0.65f;

        case PowerProfile::CUSTOM:
            if (current_sleep_engine == SleepEngine::DEEP_SLEEP) return 0.70f;
            if (current_sleep_engine == SleepEngine::DISPLAY_OFF) return 22.0f;
            return 3.5f;
    }
    return 3.0f;
}

float PowerManager::calculateEstimatedRuntimeHours(float voltage, int percentage) const {
    if (percentage <= 0 || voltage < 3.20f) return 0.0f;
    if (percentage > 100) percentage = 100;

    const float NOMINAL_BATTERY_MAH = 300.0f;
    float remaining_mah = (percentage / 100.0f) * NOMINAL_BATTERY_MAH;
    float current_ma = getEstimatedCurrentMa();

    if (current_ma <= 0.01f) current_ma = 0.01f;
    return remaining_mah / current_ma;
}

void PowerManager::formatRuntimeEstimate(char* buf, size_t max_len, float voltage, int percentage) const {
    if (!buf || max_len == 0) return;

    if (percentage <= 5 || voltage < 3.30f) {
        snprintf(buf, max_len, "LOW BAT");
        return;
    }

    float hours = calculateEstimatedRuntimeHours(voltage, percentage);

    if (hours >= 48.0f) {
        float days = hours / 24.0f;
        snprintf(buf, max_len, "%.1fd", days);
    } else {
        snprintf(buf, max_len, "%.1fh", hours);
    }
}

UlpTelemetry PowerManager::getUlpTelemetry() const {
    UlpTelemetry t;
    t.active = ulp_sentry_active;
    t.rtc_mem_used_bytes = ulp_sentry_active ? 384 : 0;
    t.wake_count = ulp_test_wake_count;
    t.last_adc_raw = 0;
    t.status_str = ulp_sentry_active ? "RUNNING [ON]" : "STANDBY [OFF]";
    return t;
}

void PowerManager::setUlpEnabled(bool en) {
    ulp_sentry_active = en;
    SettingsData& s = settingsManager.get();
    s.ulp_sentry_enabled = en;
    if (en) {
        ulp_test_wake_count++;
    }
}

void PowerManager::triggerUlpSentryTest() {
    setUlpEnabled(!ulp_sentry_active);
}

void PowerManager::executeSleep(uint32_t sleep_sec, bool raise_to_wake, uint64_t wake_mask) {
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
    switch (current_sleep_engine) {
        case SleepEngine::DEEP_SLEEP: {
            if (sleep_sec > 0) {
                esp_sleep_enable_timer_wakeup((uint64_t)sleep_sec * 1000000ULL);
            } else {
                esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
            }
            esp_sleep_enable_ext1_wakeup(wake_mask, ESP_EXT1_WAKEUP_ANY_LOW);
            esp_deep_sleep_start();
            break;
        }

        case SleepEngine::LIGHT_SLEEP: {
            // Light Sleep: maintains RAM and fast 300µs wake
            if (sleep_sec > 0) {
                esp_sleep_enable_timer_wakeup((uint64_t)sleep_sec * 1000000ULL);
            } else {
                esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
            }
            esp_sleep_enable_ext1_wakeup(wake_mask, ESP_EXT1_WAKEUP_ANY_LOW);
            esp_light_sleep_start();
            // On wake, execution continues immediately here!
            break;
        }

        case SleepEngine::DISPLAY_OFF: {
            // Display off only: no deep/light sleep, CPU yields
            delay(10);
            break;
        }

        case SleepEngine::ULP_SENTINEL: {
            // Experimental: deep sleep with ULP sentry
            if (sleep_sec > 0) {
                esp_sleep_enable_timer_wakeup((uint64_t)sleep_sec * 1000000ULL);
            } else {
                esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
            }
            esp_sleep_enable_ext1_wakeup(wake_mask, ESP_EXT1_WAKEUP_ANY_LOW);
            esp_deep_sleep_start();
            break;
        }
    }
#else
    // Host mock testing
    (void)sleep_sec;
    (void)raise_to_wake;
    (void)wake_mask;
#endif
}
