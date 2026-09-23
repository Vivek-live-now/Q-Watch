#ifndef SETTINGS_DATA_H
#define SETTINGS_DATA_H

#include <Arduino.h>

struct SettingsData {
    // CONNECTIVITY
    bool wifi_enabled = true;
    bool ble_enabled = false;
    bool fileserver_enabled = false;

    // TIME
    bool auto_sync = true;
    int timezone_idx = 0;
    bool format_24hr = true;

    // POWER
    int display_timeout_idx = 1; // 0: 10s, 1: 30s, 2: 1m, 3: 5m, 4: NEVER
    int sleep_time_idx = 0;       // 0: OFF, 1: 1m, 2: 5m, 3: 15m, 4: 30m
    bool raise_to_wake = true;    // Hand raise to wake gesture (IMU motion interrupt)
    int wifi_auto_off_idx = 0;    // 0: OFF, 1: After Sync, 2: When Idle
    bool low_power = false;

    // DISPLAY
    int contrast_idx = 3;         // 0: 25%, 1: 50%, 2: 75%, 3: 100%
    bool invert_display = false;
    int ui_option_idx = 0;

    // BME280 / WEATHER SENSORS
    int bme_interval_idx = 0;     // 0: 5m, 1: 10m, 2: 15m, 3: 30m, 4: 1h

    // MAX30102 / HEALTH SENSOR
    bool health_bg_enabled = false;
    int health_interval_idx = 0;  // 0: 5m, 1: 10m, 2: 15m, 3: 30m, 4: 1h

    // SOUND SYSTEM
    bool sound_master_on = true;
    int volume_pct = 70;          // 0 - 100%
    bool button_sounds_on = true;
    bool notifications_on = true;
    int sound_style_idx = 1;      // 0: SILENT, 1: MODERN, 2: TACTICAL, 3: RETRO

    // TIMEKEEPING & WATCH SUITE
    int watch_face_style = 0;     // 0: DIGITAL, 1: ANALOG, 2: RETRO, 3: MISSION
    bool show_date = true;
    bool show_battery = true;
    bool show_weather_widget = true;
    bool show_steps_widget = true;
    bool show_status_icons = true;
    bool hourly_chime_enabled = false;
    int world_clock_tz_idx = 1;   // Default: UTC (1)
    uint32_t step_goal = 10000;
};

// Option labels lists
static const char* const WATCH_FACE_OPTIONS[] = {"DIGITAL", "ANALOG", "RETRO LCD", "MISSION"};
static const int WATCH_FACE_COUNT = 4;
static const char* const DISPLAY_TIMEOUT_OPTIONS[] = {"10 sec", "30 sec", "1 min", "5 min", "NEVER"};
static const int DISPLAY_TIMEOUT_COUNT = 5;

static const char* const SLEEP_TIMEOUT_OPTIONS[] = {"OFF", "1 min", "5 min", "15 min", "30 min"};
static const int SLEEP_TIMEOUT_COUNT = 5;

static const char* const WIFI_AUTO_OFF_OPTIONS[] = {"OFF", "After Sync", "When Idle"};
static const int WIFI_AUTO_OFF_COUNT = 3;

static const char* const BME_INTERVAL_OPTIONS[] = {"5 min", "10 min", "15 min", "30 min", "1 hour"};
static const int BME_INTERVAL_COUNT = 5;

static const char* const HEALTH_INTERVAL_OPTIONS[] = {"5 min", "10 min", "15 min", "30 min", "1 hour"};
static const int HEALTH_INTERVAL_COUNT = 5;

static const char* const CONTRAST_OPTIONS[] = {"25%", "50%", "75%", "100%"};
static const int CONTRAST_OPTION_COUNT = 4;

static const char* const TIMEZONE_OPTIONS[] = {
    "Asia/Kolkata",
    "UTC",
    "Europe/London",
    "Europe/Berlin",
    "America/New_York",
    "America/Chicago",
    "America/Denver",
    "America/Los_Angeles",
    "Asia/Dubai",
    "Asia/Singapore",
    "Asia/Tokyo",
    "Australia/Sydney"
};
static const int TIMEZONE_OPTION_COUNT = 12;

static const char* const UI_OPTIONS_LIST[] = {"Default", "Compact", "High Contrast"};
static const int UI_OPTIONS_COUNT = 3;

static const char* const SOUND_STYLE_OPTIONS[] = {"SILENT", "MODERN", "TACTICAL", "RETRO"};
static const int SOUND_STYLE_COUNT = 4;

class SettingsManager {
public:
    SettingsManager();
    void begin();
    void load();
    void save();

    SettingsData& get() { return settings; }

private:
    SettingsData settings;
};

extern SettingsManager settingsManager;

#endif
