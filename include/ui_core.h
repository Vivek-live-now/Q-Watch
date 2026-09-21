#ifndef UI_CORE_H
#define UI_CORE_H

#include <Arduino.h>
#include "file_manager.h"
#include "settings_data.h"
#include "keyboard.h"

enum class SettingsSubmenu {
    MAIN,
    CONNECTIVITY,
    WIFI_DETAILS,
    WIFI_SCAN,
    FILE_SERVER_DETAILS,
    TIME,
    POWER,
    SUB_DISPLAY,
    SENSORS,
    HEALTH_SETTINGS,
    SYSTEM,
    TIME_SYNC_STATUS,
    RESET_CONFIRM
};

enum class CompassState {
    PAGE_MAIN,
    PAGE_METRICS,
    PAGE_CAL_MENU,
    CAL_SWEEP,
    CAL_RESULT,
    CAL_TELEMETRY,
    CAL_DECLINATION
};

enum class MotionState {
    PAGE_LEVEL,
    PAGE_DATA,
    PAGE_SETTINGS
};

enum class UIState {
    APP_HOME,
    MAIN_MENU,
    APP_CLOCK,
    APP_WEATHER,
    APP_COMPASS,
    APP_HEALTH,
    APP_MOTION,
    APP_IR,
    APP_ALTIMETER,
    APP_SETTINGS,
    APP_BATTERY,
    APP_LED,
    APP_AUDIO,
    APP_ABOUT,
    APP_FILE_MANAGER,
    APP_STORAGE_INFO,
    APP_KEYBOARD,
    VALUE_EDIT,
    SLEEPING
};

class UICore {
public:
    UICore();
    void begin();
    void loop();

    UIState getState() const { return current_state; }
    int getWeatherPage() const { return weather_page; }
    void setWeatherPage(int p) { weather_page = p; }
    int getBmePage() const { return bme_page; }
    void setBmePage(int p) { bme_page = p; }
    int getHealthPage() const { return health_page; }
    void setHealthPage(int p) { health_page = p; }
    int getHealthHistoryGraphIdx() const { return health_history_graph_idx; }
    int getMenuSelection() const { return menu_selection; }
    int getMenuScrollOffset() const { return menu_scroll_offset; }
    int getEditValue() const { return edit_value; }

    SettingsSubmenu getSettingsSubmenu() const { return settings_submenu; }
    int getSettingsSelection() const { return settings_selection; }
    int getSettingsScrollOffset() const { return settings_scroll_offset; }
    const char* getToastMessage() const { return toast_msg; }
    uint32_t getToastEndTime() const { return toast_end_time; }
    void showToast(const char* msg, uint32_t duration_ms = 1500);

    void openKeyboard(const String& initial_text, const String& title, KeyboardMode mode = KeyboardMode::ALPHA, bool mask = false, int max_len = 32, void (*on_complete)(bool success, const String& result) = nullptr);

    CompassState getCompassState() const { return compass_state; }
    void setCompassState(CompassState s) { compass_state = s; needs_redraw = true; }
    int getCompassMenuSelection() const { return compass_menu_selection; }
    int getCompassMenuOffset() const { return compass_menu_offset; }

    static const int COMPASS_MENU_ITEM_COUNT = 6;
    const char* compass_menu_items[COMPASS_MENU_ITEM_COUNT] = {
        "3D Sweep Cal",
        "Mount Orient",
        "Invert Z-Axis",
        "Mag Declin.",
        "Telemetry",
        "Factory Reset"
    };

    MotionState getMotionState() const { return motion_state; }
    void setMotionState(MotionState s) { motion_state = s; needs_redraw = true; }
    void handleMotionInput();

    void handleLedInput();

    void handleAudioInput();
    static const int AUDIO_MENU_ITEM_COUNT = 2;
    const char* audio_menu_items[AUDIO_MENU_ITEM_COUNT] = {
        "Master Sw", "Theme Style"
    };
    int getAudioMenuSelection() const { return audio_menu_selection; }
    int getAudioMenuOffset() const { return audio_menu_offset; }

    int getLedMenuSelection() const { return led_menu_selection; }
    int getLedMenuOffset() const { return led_menu_offset; }

    static const int LED_MENU_ITEM_COUNT = 6;
    const char* led_menu_items[LED_MENU_ITEM_COUNT] = {
        "Master Sw", "Mode", "Brightness", "Presets", "Effects", "Factory Rst"
    };

    static const int MOTION_MENU_ITEM_COUNT = 6;
    const char* motion_menu_items[MOTION_MENU_ITEM_COUNT] = {
        "Swap X/Y", "Invert X", "Invert Y", "Invert Z", "Accel Cal", "Zero Level"
    };

    bool needsRedraw() const { return needs_redraw; }
    void clearRedrawFlag() { needs_redraw = false; }
    void forceRedraw() { needs_redraw = true; }

    static const int MAIN_MENU_ITEM_COUNT = 13;
    const char* main_menu_items[MAIN_MENU_ITEM_COUNT] = {
        "HOME", "CLOCK", "WEATHER", "COMPASS", "HEALTH",
        "MOTION", "IR REMOTE", "BME280", "BATTERY", "LED RGB", "FILE MANAGER", "SETTINGS", "ABOUT"
    };

    static const int SETTINGS_MAIN_ITEM_COUNT = 6;
    const char* settings_main_items[SETTINGS_MAIN_ITEM_COUNT] = {
        "CONNECTIVITY", "TIME", "POWER", "DISPLAY", "SENSORS", "SYSTEM"
    };

    static const int CONNECTIVITY_ITEM_COUNT = 4;
    const char* connectivity_items[CONNECTIVITY_ITEM_COUNT] = {
        "Wi-Fi", "SCAN NETWORKS", "BLE", "FILE SERVER"
    };

    static const int TIME_ITEM_COUNT = 5;
    const char* time_items[TIME_ITEM_COUNT] = {
        "SYNC NOW", "SYNC STATUS", "AUTO SYNC", "TIMEZONE", "24 HOUR"
    };

    static const int POWER_ITEM_COUNT = 4;
    const char* power_items[POWER_ITEM_COUNT] = {
        "DISPLAY TIMEOUT", "SLEEP TIME", "WIFI AUTO-OFF", "LOW POWER"
    };

    static const int DISPLAY_ITEM_COUNT = 3;
    const char* display_items[DISPLAY_ITEM_COUNT] = {
        "CONTRAST", "INVERT", "UI OPTIONS"
    };

    static const int SENSORS_ITEM_COUNT = 4;
    const char* sensors_items[SENSORS_ITEM_COUNT] = {
        "COMPASS CAL", "IMU CAL", "HEALTH", "SENSOR STATUS"
    };

    static const int HEALTH_SETTINGS_ITEM_COUNT = 2;
    const char* health_settings_items[HEALTH_SETTINGS_ITEM_COUNT] = {
        "BG RECORDING", "REC INTERVAL"
    };

    static const int SYSTEM_ITEM_COUNT = 2;
    const char* system_items[SYSTEM_ITEM_COUNT] = {
        "STORAGE INFO", "RESET SETTINGS"
    };

public:
    String getFmCurrentPath() const { return fm_current_path; }
    int getFmSelection() const { return fm_selection; }
    int getFmScrollOffset() const { return fm_scroll_offset; }
    int getFmEntryCount() const { return fm_entry_count; }
    const struct FileInfo* getFmEntries() const { return fm_entries; }

    static String pending_selected_ssid;

public:
    uint32_t getLastActivityTime() const { return last_activity_time; }
private:
    UIState current_state;
    uint32_t last_activity_time;
    bool display_off;
    bool just_woke_display;
    UIState return_state;
    int bme_page;
    int weather_page;
    int health_page; // 0: Live Page 1, 1: History Page 2
    int health_history_graph_idx; // 0: HR, 1: SpO2, 2: Temp
    int menu_selection;
    int menu_scroll_offset;
    int edit_value;
    int led_menu_selection;
    int led_menu_offset;
    int audio_menu_selection;
    int audio_menu_offset;

    SettingsSubmenu settings_submenu;
    int settings_selection;
    int settings_scroll_offset;
    char toast_msg[32];
    uint32_t toast_end_time;

    void (*kb_callback)(bool success, const String& result);

    CompassState compass_state;
    int compass_menu_selection;
    int compass_menu_offset;
    MotionState motion_state;

    bool needs_redraw;

    void handleBmeInput();
    void handleHealthInput();
    void handleWeatherInput();
    void handleHomeInput();
    void handleMainMenuInput();
    void handleSettingsMenuInput();
    void handleValueEditInput();
    void handleKeyboardInput();
    void handleGenericAppInput();
    void handleCompassInput();

    void handleSettingsMainInput();
    void handleConnectivityInput();
    void handleWifiDetailsInput();
    void handleWifiScanInput();
    void handleFileServerDetailsInput();
    void handleTimeInput();
    void handlePowerInput();
    void handleDisplayInput();
    void handleSensorsInput();
    void handleHealthSettingsInput();
    void handleSystemInput();
    void handleResetConfirmInput();
    void handleTimeSyncStatusInput();

    String fm_current_path;
    int fm_selection;
    int fm_scroll_offset;
    int fm_entry_count;
    struct FileInfo* fm_entries;
    void handleFileManagerInput();
    void handleStorageInfoInput();
    void loadDirectory(const String& path);
    void freeFileManager();

    void processNavUp();
    void processNavDown();
    void enterDeepSleep();
    bool isDisplayOff() const { return display_off; }
    void registerActivity();
};

extern UICore ui;

#endif
