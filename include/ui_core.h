#ifndef UI_CORE_H
#define UI_CORE_H

#include <Arduino.h>


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
    PAGE_DATA
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
    APP_GAMES,
    APP_SETTINGS,
    APP_ABOUT,
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

    bool needsRedraw() const { return needs_redraw; }
    void clearRedrawFlag() { needs_redraw = false; }
    void forceRedraw() { needs_redraw = true; }

    static const int MAIN_MENU_ITEM_COUNT = 10;
    const char* main_menu_items[MAIN_MENU_ITEM_COUNT] = {
        "HOME", "CLOCK", "WEATHER", "COMPASS", "HEALTH",
        "MOTION", "IR REMOTE", "GAMES", "SETTINGS", "ABOUT"
    };

    static const int SETTINGS_MENU_ITEM_COUNT = 4;
    const char* settings_menu_items[SETTINGS_MENU_ITEM_COUNT] = {
        "Display", "Sound", "Theme", "Sleep"
    };

private:
    UIState current_state;
    int menu_selection;
    int menu_scroll_offset;
    int edit_value;

    CompassState compass_state;
    int compass_menu_selection;
    int compass_menu_offset;
    MotionState motion_state;

    bool needs_redraw;

    void handleHomeInput();
    void handleMainMenuInput();
    void handleSettingsMenuInput();
    void handleValueEditInput();
    void handleGenericAppInput(); // Shared handler for dummy apps
    void handleCompassInput();






    void processNavUp();
    void processNavDown();
    void enterDeepSleep();
};

extern UICore ui;

#endif
