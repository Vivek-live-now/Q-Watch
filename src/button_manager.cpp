#include "button_manager.h"
#include "hw_config.h"

ButtonManager btnManager;

ButtonManager::ButtonManager() {
    buttons[BTN_ID_UP].pin = BTN_UP;
    buttons[BTN_ID_SEL].pin = BTN_SEL;
    buttons[BTN_ID_DN].pin = BTN_DN;

    for (int i=0; i<BTN_COUNT; i++) {
        buttons[i].current_state = HIGH;
        buttons[i].last_state = HIGH;
        buttons[i].last_debounce_time = 0;
        buttons[i].pressed_time = 0;
        buttons[i].long_press_handled = false;
        buttons[i].last_repeat_time = 0;
        buttons[i].pending_event = BTN_EVT_NONE;
    }
}

void ButtonManager::begin() {
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_SEL, INPUT_PULLUP);
    pinMode(BTN_DN, INPUT_PULLUP);

    // Check if we woke from deep sleep via the SELECT button (GPIO21)
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
        // We woke up because SELECT is currently being held down to GND.
        // We must artificially set its state to "already handled" so it doesn't
        // emit a fake SHORT_PRESS or LONG_PRESS upon the user letting go of the WAKE action.

        // Pretend it has been held down for a long time and the long press was already handled.
        // Then, when they release it, it will just reset cleanly without emitting an event.
        buttons[BTN_ID_SEL].current_state = LOW;
        buttons[BTN_ID_SEL].last_state = LOW;
        buttons[BTN_ID_SEL].pressed_time = millis() - LONG_PRESS_MS - 1;
        buttons[BTN_ID_SEL].long_press_handled = true;
    }
}

void ButtonManager::loop() {
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
                    buttons[i].last_repeat_time = millis() + LONG_PRESS_MS;
                } else { // RELEASED
                    if (!buttons[i].long_press_handled) {
                        uint32_t duration = millis() - buttons[i].pressed_time;
                        if (duration > DEBOUNCE_DELAY_MS && duration < LONG_PRESS_MS) {
                            buttons[i].pending_event = BTN_EVT_SHORT_PRESS;
                        }
                    }
                }
            } else if (buttons[i].current_state == LOW) { // HOLDING
                uint32_t duration = millis() - buttons[i].pressed_time;

                if (!buttons[i].long_press_handled && duration >= LONG_PRESS_MS) {
                    buttons[i].pending_event = BTN_EVT_LONG_PRESS;
                    buttons[i].long_press_handled = true;
                }

                if (buttons[i].long_press_handled && (millis() - buttons[i].last_repeat_time >= REPEAT_DELAY_MS)) {
                    if(i == BTN_ID_UP || i == BTN_ID_DN) {
                        buttons[i].pending_event = BTN_EVT_REPEAT;
                    }
                    buttons[i].last_repeat_time = millis();
                }
            }
        }
        buttons[i].last_state = reading;
    }
}

ButtonEvent ButtonManager::getEvent(ButtonID id) {
    ButtonEvent evt = buttons[id].pending_event;
    buttons[id].pending_event = BTN_EVT_NONE;
    return evt;
}
