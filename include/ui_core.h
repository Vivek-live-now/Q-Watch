#ifndef UI_CORE_H
#define UI_CORE_H

#include <Arduino.h>
#include "file_manager.h"
#include "settings_data.h"

enum class SettingsSubmenu {
    MAIN,
    CONNECTIVITY,
    TIME,
    POWER,
    SUB_DISPLAY,
    SENSORS,
    SYSTEM,
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
    VALUE_EDIT,
    SLEEPING
};

class UICore {
public:
    UICore();
    void begin();
    void loop();

    UIState getState() const { return current_state; }
    int getMenuSelection() const { return menu_selection; }
    int getMenuScrollOffset() const { return menu_scroll_offset; }
    int getEditValue() const { return edit_value; }

    // Settings Navigation & Submenus
    SettingsSubmenu getSettingsSubmenu() const { return settings_submenu; }
    int getSettingsSelection() const { return settings_selection; }
    int getSettingsScrollOffset() const { return settings_scroll_offset; }
    const char* getToastMessage() const { return toast_msg; }
    uint32_t getToastEndTime() const { return toast_end_time; }
    void showToast(const char* msg, uint32_t duration_ms = 1500);

    CompassState getCompassState() const { return compass_state; }
    void setCompassState(CompassState s) { compass_state = s; needs_redraw = true; }
    int getCompassMenuSelection() const { return compass_menu_selection; }
    int getCompassMenuOffset() const { return compass_menu_offset; }

    // Config items
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
        "MOTION", "IR REMOTE", "ALTIMETER", "BATTERY", "LED RGB", "FILE MANAGER", "SETTINGS", "ABOUT"
    };

    // Top-Level Settings Menu Items (6 Categories)
    static const int SETTINGS_MAIN_ITEM_COUNT = 6;
    const char* settings_main_items[SETTINGS_MAIN_ITEM_COUNT] = {
        "CONNECTIVITY", "TIME", "POWER", "DISPLAY", "SENSORS", "SYSTEM"
    };

    // Submenu Item Counts & Labels
    static const int CONNECTIVITY_ITEM_COUNT = 3;
    const char* connectivity_items[CONNECTIVITY_ITEM_COUNT] = {
        "Wi-Fi", "BLE", "FILE SERVER"
    };

    static const int TIME_ITEM_COUNT = 4;
    const char* time_items[TIME_ITEM_COUNT] = {
        "SYNC NOW", "AUTO SYNC", "TIMEZONE", "24 HOUR"
    };

    static const int POWER_ITEM_COUNT = 3;
    const char* power_items[POWER_ITEM_COUNT] = {
        "DISPLAY TIMEOUT", "SLEEP TIME", "LOW POWER"
    };

    static const int DISPLAY_ITEM_COUNT = 3;
    const char* display_items[DISPLAY_ITEM_COUNT] = {
        "CONTRAST", "INVERT", "UI OPTIONS"
    };

    static const int SENSORS_ITEM_COUNT = 3;
    const char* sensors_items[SENSORS_ITEM_COUNT] = {
        "COMPASS CAL", "IMU CAL", "SENSOR STATUS"
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

private:
    UIState current_state;
    int menu_selection;
    int menu_scroll_offset;
    int edit_value;
    int led_menu_selection;
    int led_menu_offset;
    int audio_menu_selection;
    int audio_menu_offset;

    // Settings Navigation State
    SettingsSubmenu settings_submenu;
    int settings_selection;
    int settings_scroll_offset;
    char toast_msg[32];
    uint32_t toast_end_time;

    CompassState compass_state;
    int compass_menu_selection;
    int compass_menu_offset;
    MotionState motion_state;

    bool needs_redraw;

    void handleHomeInput();
    void handleMainMenuInput();
    void handleSettingsMenuInput();
    void handleValueEditInput();
    void handleGenericAppInput();
    void handleCompassInput();

    // Settings Submenu Specific Input Handlers
    void handleSettingsMainInput();
    void handleConnectivityInput();
    void handleTimeInput();
    void handlePowerInput();
    void handleDisplayInput();
    void handleSensorsInput();
    void handleSystemInput();
    void handleResetConfirmInput();

    // File Manager State
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
};

extern UICore ui;

#endif
