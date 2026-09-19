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
    int wifi_auto_off_idx = 0;    // 0: OFF, 1: After Sync, 2: When Idle
    bool low_power = false;

    // DISPLAY
    int contrast_idx = 3;         // 0: 25%, 1: 50%, 2: 75%, 3: 100%
    bool invert_display = false;
    int ui_option_idx = 0;
};

// Option labels lists
static const char* const DISPLAY_TIMEOUT_OPTIONS[] = {"10 sec", "30 sec", "1 min", "5 min", "NEVER"};
static const int DISPLAY_TIMEOUT_COUNT = 5;

static const char* const SLEEP_TIMEOUT_OPTIONS[] = {"OFF", "1 min", "5 min", "15 min", "30 min"};
static const int SLEEP_TIMEOUT_COUNT = 5;

static const char* const WIFI_AUTO_OFF_OPTIONS[] = {"OFF", "After Sync", "When Idle"};
static const int WIFI_AUTO_OFF_COUNT = 3;

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
