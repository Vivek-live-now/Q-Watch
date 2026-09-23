#ifndef UI_CORE_H
#define UI_CORE_H

#include <Arduino.h>
#include "file_manager.h"
#include "settings_data.h"
#include "keyboard.h"
#include "ir_engine.h"
#include "sound_manager.h"

enum class Imu6500SubApp {
    SUBAPP_MENU,
    SUBAPP_ALTIMETER,
    SUBAPP_AIRMOUSE
};

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

enum class IrSubmenu {
    MAIN,
    TV_B_GONE,
    CUSTOM_IR,
    REMOTE_VIEW,
    IR_READ,
    IR_READ_WAIT,
    IR_READ_RESULT,
    QUICK_REMOTE,
    QUICK_REMOTE_BUILD,
    QUICK_REMOTE_WAIT,
    UNIVERSAL,
    RECENT,
    FAVORITES,
    IR_FILES,
    IR_LAB
};

enum class SoundSubmenu {
    MAIN,
    SETTINGS,
    EFFECTS,
    CREATOR_LIST,
    CREATOR_EDIT,
    COMPOSER,
    LAB,
    METRONOME,
    MORSE
};

enum class ClockSubmenu {
    MAIN,
    FACE_SELECT,
    FACE_WIDGETS,
    STOPWATCH,
    TIMER,
    ALARMS,
    ALARM_EDIT,
    WORLD_CLOCK,
    PEDOMETER
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

    Imu6500SubApp getImuSubApp() const { return imu_subapp; }
    int getImuSubAppSelection() const { return imu_subapp_selection; }

    SettingsSubmenu getSettingsSubmenu() const { return settings_submenu; }
    int getSettingsSelection() const { return settings_selection; }
    int getSettingsScrollOffset() const { return settings_scroll_offset; }

    // Clock Submenu getters & state
    ClockSubmenu getClockSubmenu() const { return clock_submenu; }
    void setClockSubmenu(ClockSubmenu sub) { clock_submenu = sub; needs_redraw = true; }
    int getClockSelection() const { return clock_selection; }
    int getClockScrollOffset() const { return clock_scroll_offset; }
    int getAlarmEditIdx() const { return alarm_edit_idx; }
    int getAlarmEditField() const { return alarm_edit_field; }
    int getTimerPresetIdx() const { return timer_preset_idx; }
    int getWidgetsSelection() const { return widgets_selection; }
    int getWidgetsScrollOffset() const { return widgets_scroll_offset; }

    // IR Submenu getters & state
    IrSubmenu getIrSubmenu() const { return ir_submenu; }
    void setIrSubmenu(IrSubmenu sub) { ir_submenu = sub; needs_redraw = true; }
    int getIrSelection() const { return ir_selection; }
    int getIrScrollOffset() const { return ir_scroll_offset; }
    const IrRemoteFile& getIrActiveRemote() const { return ir_active_remote; }
    const IrButton& getIrCapturedButton() const { return ir_captured_btn; }
    void setIrActiveRemotePath(const String& path);
    void setIrQuickRemoteName(const String& name) { ir_quick_remote_name = name; }
    void setIrQuickButtonName(const String& name) { ir_quick_button_name = name; }

    // Sound Submenu getters & state
    SoundSubmenu getSoundSubmenu() const { return sound_submenu; }
    void setSoundSubmenu(SoundSubmenu sub) { sound_submenu = sub; needs_redraw = true; }
    int getSoundSelection() const { return sound_selection; }
    int getSoundScrollOffset() const { return sound_scroll_offset; }

    // Composer & Creator getters
    int getComposerNoteIdx() const { return composer_note_idx; }
    int getComposerOctave() const { return composer_octave; }
    int getComposerDurIdx() const { return composer_dur_idx; }
    int getComposerCount() const { return composer_count; }

    int getCreatorNoteCount() const { return creator_count; }
    int getCreatorCursor() const { return creator_cursor; }
    int getCreatorEditField() const { return creator_edit_field; }
    const SoundNote* getCreatorNotes() const { return creator_notes; }
    const SoundNote* getComposerNotes() const { return composer_notes; }
    String getActiveMelodyName() const { return active_melody_name; }

    int getMetronomeBpm() const { return metronome_bpm; }
    bool isMetronomeActive() const { return metronome_active; }
    int getMetronomeBeat() const { return metronome_beat; }

    uint16_t getLabFreq() const { return lab_freq; }
    uint8_t getLabDuty() const { return lab_duty; }
    bool isLabSweepActive() const { return lab_sweep_active; }
    uint16_t getLabSweepFreq() const { return lab_sweep_freq; }

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
    void handleSoundInput();

    int getLedMenuSelection() const { return led_menu_selection; }
    int getLedMenuOffset() const { return led_menu_offset; }

    static const int LED_MENU_ITEM_COUNT = 6;
    const char* led_menu_items[LED_MENU_ITEM_COUNT] = {
        "Master Sw", "Mode", "Brightness", "Presets", "Effects", "Factory Rst"
    };

    static const int MOTION_MENU_ITEM_COUNT = 7;
    const char* motion_menu_items[MOTION_MENU_ITEM_COUNT] = {
        "Zero Altitude", "Zero Level IMU", "Calibrate Accel", "Swap X/Y", "Invert X", "Invert Y", "Invert Z"
    };

    static const int IMU_SUBAPP_COUNT = 2;
    const char* imu_subapp_items[IMU_SUBAPP_COUNT] = {
        "ALTIMETER", "AIR MOUSE"
    };

    static const int CLOCK_MENU_ITEM_COUNT = 8;
    const char* clock_menu_items[CLOCK_MENU_ITEM_COUNT] = {
        "WATCH FACE", "FACE WIDGETS", "STOPWATCH", "TIMER", "ALARMS", "HOURLY CHIME", "WORLD CLOCK", "PEDOMETER"
    };

    static const int WIDGETS_ITEM_COUNT = 5;
    const char* widgets_items[WIDGETS_ITEM_COUNT] = {
        "SHOW DATE", "SHOW BATTERY", "WEATHER WIDGET", "STEPS WIDGET", "STATUS ICONS"
    };

    bool needsRedraw() const { return needs_redraw; }
    void clearRedrawFlag() { needs_redraw = false; }
    void forceRedraw() { needs_redraw = true; }

    static const int MAIN_MENU_ITEM_COUNT = 14;
    const char* main_menu_items[MAIN_MENU_ITEM_COUNT] = {
        "HOME", "CLOCK", "WEATHER", "COMPASS", "HEALTH",
        "IMU6500", "IR REMOTE", "SOUND", "ALTIMETER", "BATTERY", "LED RGB", "FILE MANAGER", "SETTINGS", "ABOUT"
    };

    static const int SOUND_MAIN_ITEM_COUNT = 7;
    const char* sound_main_items[SOUND_MAIN_ITEM_COUNT] = {
        "SETTINGS", "SOUND EFFECTS", "MELODY CREATOR", "COMPOSER", "SOUND LAB", "METRONOME", "MORSE / Q-CODE"
    };

    static const int SOUND_SETTINGS_ITEM_COUNT = 5;
    const char* sound_settings_items[SOUND_SETTINGS_ITEM_COUNT] = {
        "Master Switch", "Volume %", "Button Sounds", "Notifications", "Sound Style"
    };

    static const int SOUND_EFFECTS_ITEM_COUNT = 7;
    const char* sound_effects_items[SOUND_EFFECTS_ITEM_COUNT] = {
        "Boot", "Wake", "Sleep", "Notification", "Warning", "Alert", "Q-Branch"
    };

    static const int SOUND_LAB_ITEM_COUNT = 4;
    const char* sound_lab_items[SOUND_LAB_ITEM_COUNT] = {
        "2700 Hz Resonance", "Frequency Sweep", "Duty Cycle Test", "Buzzer Diagnostic"
    };

    static const int IR_MAIN_ITEM_COUNT = 9;
    const char* ir_main_items[IR_MAIN_ITEM_COUNT] = {
        "TV-B-GONE", "CUSTOM IR", "IR READ", "QUICK REMOTE",
        "UNIVERSAL", "RECENT", "FAVORITES", "IR FILES", "IR LAB"
    };

    static const int IR_CUSTOM_ITEM_COUNT = 4;
    const char* ir_custom_items[IR_CUSTOM_ITEM_COUNT] = {
        "Browse /ir", "Recent", "Favorites", "Search"
    };

    static const int IR_LAB_ITEM_COUNT = 5;
    const char* ir_lab_items[IR_LAB_ITEM_COUNT] = {
        "Carrier 38 kHz", "Carrier 36 kHz", "Carrier 40 kHz", "Raw -> Parsed", "IR Config"
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

    static const int POWER_ITEM_COUNT = 5;
    const char* power_items[POWER_ITEM_COUNT] = {
        "DISPLAY TIMEOUT", "SLEEP TIME", "RAISE TO WAKE", "WIFI AUTO-OFF", "LOW POWER"
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
    Imu6500SubApp imu_subapp;
    int imu_subapp_selection;

    uint32_t last_activity_time;
    bool display_off;
    uint32_t display_off_time;
    bool just_woke_display;
    UIState return_state;
    int bme_page;
    int weather_page;
    int health_page;
    int health_history_graph_idx;
    int menu_selection;
    int menu_scroll_offset;
    int edit_value;
    int led_menu_selection;
    int led_menu_offset;

    // Clock sub-app state
    ClockSubmenu clock_submenu;
    int clock_selection;
    int clock_scroll_offset;
    int alarm_edit_idx;
    int alarm_edit_field;
    int timer_preset_idx;
    int widgets_selection;
    int widgets_scroll_offset;

    // Sound sub-app state
    SoundSubmenu sound_submenu;
    int sound_selection;
    int sound_scroll_offset;

    // Composer & Melody Creator Sequences
    int composer_note_idx;
    int composer_octave;
    int composer_dur_idx;
    int composer_count;
    SoundNote composer_notes[32];

    SoundNote creator_notes[64];
    int creator_count;
    int creator_cursor;
    int creator_edit_field;
    String active_melody_name;
public:
    void setActiveMelodyName(const String& n) { active_melody_name = n; }
    void setCreatorCount(int c) { creator_count = c; }
    void loadCreatorNotes(const SoundNote* notes, int count) { creator_count = min(count, 64); for (int i = 0; i < creator_count; i++) creator_notes[i] = notes[i]; creator_cursor = 0; creator_edit_field = 0; }

    int metronome_bpm;
    bool metronome_active;
    int metronome_beat;
    uint32_t last_metronome_tick;

    uint16_t lab_freq;
    uint8_t lab_duty;
    bool lab_sweep_active;
    uint16_t lab_sweep_freq;
    uint32_t last_sweep_time;

    std::vector<String> melody_file_list;

    // IR state variables
    IrSubmenu ir_submenu;
    int ir_selection;
    int ir_scroll_offset;
    IrRemoteFile ir_active_remote;
    IrButton ir_captured_btn;
    String ir_quick_remote_name;
    String ir_quick_button_name;
    std::vector<String> ir_file_list;

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
    void handleClockInput();
    void handleClockMenuInput();
    void handleFaceSelectInput();
    void handleFaceWidgetsInput();
    void handleStopwatchInput();
    void handleTimerInput();
    void handleAlarmsInput();
    void handleAlarmEditInput();
    void handleWorldClockInput();
    void handlePedometerInput();
    void handleIRInput();
    void handleIrMainInput();
    void handleTvBGoneInput();
    void handleIrCustomInput();
    void handleIrRemoteViewInput();
    void handleIrReadInput();
    void handleQuickRemoteInput();
    void handleIrUniversalInput();
    void handleIrRecentInput();
    void handleIrFavoritesInput();
    void handleIrFilesInput();
    void handleIrLabInput();

    void handleSoundMainInput();
    void handleSoundSettingsInput();
    void handleSoundEffectsInput();
    void handleSoundCreatorListInput();
    void handleSoundCreatorEditInput();
    void handleSoundComposerInput();
    void handleSoundLabInput();
    void handleMetronomeInput();
    void handleMorseInput();

    void handleSettingsMenuInput();
    void handleValueEditInput();
    void handleKeyboardInput();
    void handleGenericAppInput();
    void handleCompassInput();
    void handleAirMouseInput();

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
