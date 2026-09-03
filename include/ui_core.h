#ifndef UI_CORE_H
#define UI_CORE_H

#include <Arduino.h>

enum class UIState {
    HOME,
    MAIN_MENU,
    SETTINGS_MENU,
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

    static const int MAIN_MENU_ITEM_COUNT = 7;
    const char* main_menu_items[MAIN_MENU_ITEM_COUNT] = {"Clock", "Weather", "Sensors", "Games", "Themes", "Settings", "Sleep"};

    static const int SETTINGS_MENU_ITEM_COUNT = 3;
    const char* settings_menu_items[SETTINGS_MENU_ITEM_COUNT] = {"Brightness", "Wi-Fi", "About"};

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

    void processNavUp();
    void processNavDown();
    void enterDeepSleep();
};

extern UICore ui;

#endif
