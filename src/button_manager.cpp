#include "button_manager.h"
#include "hw_config.h"

ButtonManager btnManager;

ButtonManager::ButtonManager() : pending_combo(COMBO_EVT_NONE), woken_from_sleep(false), cancel_first_tap_time(0), cancel_waiting_for_double_tap(false) {
    buttons[BTN_ID_UP].pin = BTN_UP;
    buttons[BTN_ID_OK].pin = BTN_OK;
    buttons[BTN_ID_DN].pin = BTN_DN;
    buttons[BTN_ID_CANCEL].pin = BTN_CANCEL;

    for (int i=0; i<BTN_COUNT; i++) {
        buttons[i].current_state = HIGH;
        buttons[i].last_state = HIGH;
        buttons[i].last_debounce_time = 0;
        buttons[i].pressed_time = 0;
        buttons[i].long_press_handled = false;
        buttons[i].last_repeat_time = 0;
        buttons[i].pending_event = BTN_EVT_NONE;
        buttons[i].combo_handled = false;
    }
}

void ButtonManager::begin() {
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_OK, INPUT_PULLUP);
    pinMode(BTN_DN, INPUT_PULLUP);
    pinMode(BTN_CANCEL, INPUT_PULLUP);

    // Check if we woke from deep sleep
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0 || wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
        woken_from_sleep = true;

        // If woke via CANCEL (GPIO21) or EXT1 wakeup mask containing GPIO21/GPIO8,
        // prevent CANCEL from generating a fake button event on release.
        buttons[BTN_ID_CANCEL].current_state = LOW;
        buttons[BTN_ID_CANCEL].last_state = LOW;
        buttons[BTN_ID_CANCEL].pressed_time = millis() - LONG_PRESS_MS - 1;
        buttons[BTN_ID_CANCEL].long_press_handled = true;
    }
}

void ButtonManager::loop() {
    // 1. Debounce and read raw button states
    for (int i=0; i<BTN_COUNT; i++) {
        bool reading = digitalRead(buttons[i].pin);

        if (reading != buttons[i].last_state) {
            buttons[i].last_debounce_time = millis();
        }

        if ((millis() - buttons[i].last_debounce_time) > DEBOUNCE_DELAY_MS) {
            if (reading != buttons[i].current_state) {
                buttons[i].current_state = reading;

                if (buttons[i].current_state == LOW) { // PRESSED
                    buttons[i].pressed_time = millis();
                    buttons[i].long_press_handled = false;
                    buttons[i].combo_handled = false;
                    buttons[i].last_repeat_time = millis() + LONG_PRESS_MS;
                } else { // RELEASED
                    if (!buttons[i].long_press_handled && !buttons[i].combo_handled) {
                        uint32_t duration = millis() - buttons[i].pressed_time;
                        if (duration > DEBOUNCE_DELAY_MS && duration < LONG_PRESS_MS) {
                            if (i == BTN_ID_CANCEL) {
                                // Double-tap window detection logic for CANCEL
                                uint32_t now = millis();
                                if (cancel_waiting_for_double_tap && (now - cancel_first_tap_time <= DOUBLE_TAP_WINDOW_MS)) {
                                    buttons[BTN_ID_CANCEL].pending_event = BTN_EVT_DOUBLE_TAP;
                                    cancel_waiting_for_double_tap = false;
                                } else {
                                    cancel_first_tap_time = now;
                                    cancel_waiting_for_double_tap = true;
                                }
                            } else {
                                if (buttons[i].pending_event == BTN_EVT_NONE) {
                                    buttons[i].pending_event = BTN_EVT_SHORT_PRESS;
                                }
                            }
                        }
                    }
                }
            } else if (buttons[i].current_state == LOW) { // HOLDING
                uint32_t duration = millis() - buttons[i].pressed_time;

                if (!buttons[i].long_press_handled && !buttons[i].combo_handled && duration >= LONG_PRESS_MS) {
                    if (buttons[i].pending_event == BTN_EVT_NONE) {
                        buttons[i].pending_event = BTN_EVT_LONG_PRESS;
                    }
                    buttons[i].long_press_handled = true;
                }

                if (buttons[i].long_press_handled && (millis() - buttons[i].last_repeat_time >= REPEAT_DELAY_MS)) {
                    if((i == BTN_ID_UP || i == BTN_ID_DN) && buttons[i].pending_event == BTN_EVT_NONE) {
                        buttons[i].pending_event = BTN_EVT_REPEAT;
                        buttons[i].last_repeat_time = millis();
                    }
                }
            }
        }
        buttons[i].last_state = reading;
    }

    // 2. Check for CANCEL combinations (CANCEL held down while UP, OK, or DN is pressed)
    if (buttons[BTN_ID_CANCEL].current_state == LOW && !buttons[BTN_ID_CANCEL].combo_handled) {
        if (buttons[BTN_ID_UP].current_state == LOW && !buttons[BTN_ID_UP].combo_handled) {
            pending_combo = COMBO_EVT_CANCEL_UP;
            buttons[BTN_ID_CANCEL].combo_handled = true;
            buttons[BTN_ID_UP].combo_handled = true;
        } else if (buttons[BTN_ID_OK].current_state == LOW && !buttons[BTN_ID_OK].combo_handled) {
            pending_combo = COMBO_EVT_CANCEL_OK;
            buttons[BTN_ID_CANCEL].combo_handled = true;
            buttons[BTN_ID_OK].combo_handled = true;
        } else if (buttons[BTN_ID_DN].current_state == LOW && !buttons[BTN_ID_DN].combo_handled) {
            pending_combo = COMBO_EVT_CANCEL_DN;
            buttons[BTN_ID_CANCEL].combo_handled = true;
            buttons[BTN_ID_DN].combo_handled = true;
        }
    }

    // 3. Resolve single tap for CANCEL if double-tap timeout expires
    if (cancel_waiting_for_double_tap && (millis() - cancel_first_tap_time > DOUBLE_TAP_WINDOW_MS)) {
        if (buttons[BTN_ID_CANCEL].pending_event == BTN_EVT_NONE) {
            buttons[BTN_ID_CANCEL].pending_event = BTN_EVT_SHORT_PRESS;
        }
        cancel_waiting_for_double_tap = false;
    }
}

ButtonEvent ButtonManager::getEvent(ButtonID id) {
    ButtonEvent evt = buttons[id].pending_event;
    buttons[id].pending_event = BTN_EVT_NONE;
    return evt;
}

ComboEvent ButtonManager::getComboEvent() {
    ComboEvent evt = pending_combo;
    pending_combo = COMBO_EVT_NONE;
    return evt;
}

void ButtonManager::injectEvent(ButtonID id, ButtonEvent evt) {
    if (id < BTN_COUNT) {
        buttons[id].pending_event = evt;
    }
}

