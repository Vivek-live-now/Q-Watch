#ifndef UI_CORE_H
#define UI_CORE_H

#include <Arduino.h>

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
    bool needs_redraw;

    void handleHomeInput();
    void handleMainMenuInput();
    void handleSettingsMenuInput();
    void handleValueEditInput();
    void handleGenericAppInput(); // Shared handler for dummy apps

    void processNavUp();
    void processNavDown();
    void enterDeepSleep();
};

extern UICore ui;

#endif
