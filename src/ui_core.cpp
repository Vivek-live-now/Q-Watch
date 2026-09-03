#include "ui_core.h"
#include "button_manager.h"
#include "hw_config.h"
#include "driver/rtc_io.h"

UICore ui;

UICore::UICore() :
    current_state(UIState::HOME),
    menu_selection(0),
    menu_scroll_offset(0),
    edit_value(5),
    needs_redraw(true) {}

void UICore::begin() {
    // Check if we woke from deep sleep
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
        Serial.println("Woke up from deep sleep via SELECT button (GPIO21)!");
        // The button manager will handle ignoring the initial hold of the button
    }
}

void UICore::loop() {
    if (current_state == UIState::SLEEPING) return;

    // Route inputs based on current state
    switch (current_state) {
        case UIState::HOME:
            handleHomeInput();
            break;
        case UIState::MAIN_MENU:
            handleMainMenuInput();
            break;
        case UIState::SETTINGS_MENU:
            handleSettingsMenuInput();
            break;
        case UIState::VALUE_EDIT:
            handleValueEditInput();
            break;
    }
}

void UICore::processNavUp() {
    menu_selection--;
    if (menu_selection < 0) menu_selection = 0;

    if (menu_selection < menu_scroll_offset) {
        menu_scroll_offset = menu_selection;
    }
    needs_redraw = true;
}

void UICore::processNavDown() {
    int max_items = (current_state == UIState::MAIN_MENU) ? MAIN_MENU_ITEM_COUNT : SETTINGS_MENU_ITEM_COUNT;

    menu_selection++;
    if (menu_selection >= max_items) menu_selection = max_items - 1;

    // Display shows ~4 items at a time
    if (menu_selection >= menu_scroll_offset + 4) {
        menu_scroll_offset = menu_selection - 3;
    }
    needs_redraw = true;
}

void UICore::handleHomeInput() {
    ButtonEvent sel_evt = btnManager.getEvent(BTN_ID_SEL);
    if (sel_evt == BTN_EVT_SHORT_PRESS) {
        current_state = UIState::MAIN_MENU;
        menu_selection = 0;
        menu_scroll_offset = 0;
        needs_redraw = true;
    }

    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
}

void UICore::handleMainMenuInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) processNavUp();

    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) processNavDown();

    ButtonEvent sel_evt = btnManager.getEvent(BTN_ID_SEL);
    if (sel_evt == BTN_EVT_SHORT_PRESS) {
        if (menu_selection == 5) { // "Settings"
            current_state = UIState::SETTINGS_MENU;
            menu_selection = 0;
            menu_scroll_offset = 0;
            needs_redraw = true;
        } else if (menu_selection == 6) { // "Sleep"
            enterDeepSleep();
        } else {
            // Placeholder: Normally open App. We'll just back out to Home for testing.
            current_state = UIState::HOME;
            needs_redraw = true;
        }
    } else if (sel_evt == BTN_EVT_LONG_PRESS) {
        // BACK
        current_state = UIState::HOME;
        needs_redraw = true;
    }
}

void UICore::handleSettingsMenuInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) processNavUp();

    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) processNavDown();

    ButtonEvent sel_evt = btnManager.getEvent(BTN_ID_SEL);
    if (sel_evt == BTN_EVT_SHORT_PRESS) {
        if (menu_selection == 0) { // "Brightness"
            current_state = UIState::VALUE_EDIT;
            needs_redraw = true;
        }
    } else if (sel_evt == BTN_EVT_LONG_PRESS) {
        // BACK to Main Menu
        current_state = UIState::MAIN_MENU;
        menu_selection = 5; // Put cursor back on "Settings"
        menu_scroll_offset = 2; // Approximate scroll to show it
        needs_redraw = true;
    }
}

void UICore::handleValueEditInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        if (edit_value > 0) { edit_value--; needs_redraw = true; }
    }

    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        if (edit_value < 10) { edit_value++; needs_redraw = true; }
    }

    ButtonEvent sel_evt = btnManager.getEvent(BTN_ID_SEL);
    if (sel_evt == BTN_EVT_SHORT_PRESS) {
        // Confirm & back
        current_state = UIState::SETTINGS_MENU;
        needs_redraw = true;
    } else if (sel_evt == BTN_EVT_LONG_PRESS) {
        // Cancel & back
        current_state = UIState::SETTINGS_MENU;
        needs_redraw = true;
    }
}

void UICore::enterDeepSleep() {
    current_state = UIState::SLEEPING;
    Serial.println("Going to sleep now...");

    // Allow OLED/UI to finish rendering any shutdown graphic if we had one
    delay(100);

    // Configure RTC pull-up for the unified SELECT button (GPIO21)
    rtc_gpio_pullup_en((gpio_num_t)BTN_SEL);
    rtc_gpio_pulldown_dis((gpio_num_t)BTN_SEL);

    // Wake on LOW from the SELECT button pin
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BTN_SEL, 0);

    esp_deep_sleep_start();
}
