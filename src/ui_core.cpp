#include "ui_core.h"
#include "button_manager.h"
#include "hw_config.h"
#include "led_controller.h"

#if defined(ARDUINO)
#include "driver/rtc_io.h"
#endif

UICore ui;

UICore::UICore() :
    current_state(UIState::APP_HOME),
    menu_selection(0),
    menu_scroll_offset(0),
    edit_value(5),
    needs_redraw(true) {}

void UICore::begin() {
#if defined(ARDUINO)
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
        Serial.println("Woke up from deep sleep via SELECT button (GPIO21)!");
    }
#endif
}

void UICore::loop() {
    if (current_state == UIState::SLEEPING) return;

    switch (current_state) {
        case UIState::APP_HOME:
            handleHomeInput();
            break;
        case UIState::MAIN_MENU:
            handleMainMenuInput();
            break;
        case UIState::APP_LED:
            handleLEDAppInput();
            break;
        case UIState::APP_SETTINGS:
            handleSettingsMenuInput();
            break;
        case UIState::VALUE_EDIT:
            handleValueEditInput();
            break;
        default:
            handleGenericAppInput();
            break;
    }
}

void UICore::processNavUp() {
    menu_selection--;
    if (menu_selection < 0) {
        menu_selection = 0;
    }

    if (menu_selection < menu_scroll_offset) {
        menu_scroll_offset = menu_selection;
    }
    needs_redraw = true;
}

void UICore::processNavDown() {
    int max_items = (current_state == UIState::MAIN_MENU) ? MAIN_MENU_ITEM_COUNT : SETTINGS_MENU_ITEM_COUNT;

    menu_selection++;
    if (menu_selection >= max_items) {
        menu_selection = max_items - 1;
    }

    if (menu_selection >= menu_scroll_offset + 3) {
        menu_scroll_offset = menu_selection - 2;
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
        switch(menu_selection) {
            case 0: current_state = UIState::APP_HOME; break;
            case 1: current_state = UIState::APP_CLOCK; break;
            case 2: current_state = UIState::APP_WEATHER; break;
            case 3: current_state = UIState::APP_COMPASS; break;
            case 4: current_state = UIState::APP_HEALTH; break;
            case 5: current_state = UIState::APP_MOTION; break;
            case 6: current_state = UIState::APP_IR; break;
            case 7: current_state = UIState::APP_GAMES; break;
            case 8: current_state = UIState::APP_BATTERY; break;
            case 9: current_state = UIState::APP_LED; break;
            case 10: current_state = UIState::APP_SETTINGS; menu_selection=0; menu_scroll_offset=0; break;
            case 11: current_state = UIState::APP_ABOUT; break;
        }
        needs_redraw = true;
    } else if (sel_evt == BTN_EVT_LONG_PRESS) {
        current_state = UIState::APP_HOME;
        needs_redraw = true;
    }
}

void UICore::handleLEDAppInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        uint8_t curBrt = ledController.getBrightness();
        if (curBrt <= 230) ledController.setBrightness(curBrt + 25);
        else ledController.setBrightness(255);
        needs_redraw = true;
    }

    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        uint8_t curBrt = ledController.getBrightness();
        if (curBrt >= 25) ledController.setBrightness(curBrt - 25);
        else ledController.setBrightness(0);
        needs_redraw = true;
    }

    ButtonEvent sel_evt = btnManager.getEvent(BTN_ID_SEL);
    if (sel_evt == BTN_EVT_SHORT_PRESS) {
        // Toggle preset or power state
        int p = (int)ledController.getPreset();
        p = (p + 1) % 8;
        ledController.setPreset((LEDPreset)p);
        needs_redraw = true;
    } else if (sel_evt == BTN_EVT_LONG_PRESS) {
        current_state = UIState::APP_HOME;
        needs_redraw = true;
    }
}

void UICore::handleGenericAppInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);

    ButtonEvent sel_evt = btnManager.getEvent(BTN_ID_SEL);
    if (sel_evt == BTN_EVT_LONG_PRESS) {
        current_state = UIState::APP_HOME;
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
        if (menu_selection == 3) { // "Sleep"
            enterDeepSleep();
        } else {
            current_state = UIState::VALUE_EDIT;
            needs_redraw = true;
        }
    } else if (sel_evt == BTN_EVT_LONG_PRESS) {
        current_state = UIState::MAIN_MENU;
        menu_selection = 10; // Reset cursor to Settings in main menu
        menu_scroll_offset = menu_selection - 2;
        if(menu_scroll_offset < 0) menu_scroll_offset = 0;
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
        current_state = UIState::APP_SETTINGS;
        needs_redraw = true;
    } else if (sel_evt == BTN_EVT_LONG_PRESS) {
        current_state = UIState::APP_SETTINGS;
        needs_redraw = true;
    }
}

void UICore::enterDeepSleep() {
    current_state = UIState::SLEEPING;
#if defined(ARDUINO)
    Serial.println("Going to sleep now...");
    delay(100);

    rtc_gpio_pullup_en((gpio_num_t)BTN_SEL);
    rtc_gpio_pulldown_dis((gpio_num_t)BTN_SEL);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BTN_SEL, 0);

    esp_deep_sleep_start();
#endif
}
