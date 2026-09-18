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
    int timezone_idx = 0; // e.g. 0: UTC, 1: IST (+5:30), 2: EST (-5), 3: PST (-8)
    bool format_24hr = true;

    // POWER
    int display_timeout_idx = 1; // 0: 15s, 1: 30s, 2: 60s, 3: 2m, 4: 5m, 5: NEVER
    int sleep_time_idx = 3;       // 0: 15s, 1: 30s, 2: 60s, 3: 2m, 4: 5m, 5: NEVER
    bool low_power = false;

    // DISPLAY
    int contrast = 100; // 0..100% or step
    bool invert_display = false;
    int ui_option_idx = 0;
};

// Option labels lists
static const char* const TIMEOUT_OPTIONS[] = {"15 sec", "30 sec", "60 sec", "2 min", "5 min", "NEVER"};
static const int TIMEOUT_OPTION_COUNT = 6;

static const char* const TIMEZONE_OPTIONS[] = {"UTC", "IST (+5:30)", "EST (-5)", "PST (-8)"};
static const int TIMEZONE_OPTION_COUNT = 4;

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
