#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <Arduino.h>

enum ButtonID {
    BTN_ID_UP = 0,
    BTN_ID_OK = 1,
    BTN_ID_SEL = 1, // Alias for BTN_ID_OK
    BTN_ID_DN = 2,
    BTN_ID_CANCEL = 3,
    BTN_COUNT = 4
};

enum ButtonEvent {
    BTN_EVT_NONE = 0,
    BTN_EVT_SHORT_PRESS,
    BTN_EVT_LONG_PRESS,
    BTN_EVT_REPEAT,
    BTN_EVT_DOUBLE_TAP
};

enum ComboEvent {
    COMBO_EVT_NONE = 0,
    COMBO_EVT_CANCEL_UP,
    COMBO_EVT_CANCEL_OK,
    COMBO_EVT_CANCEL_DN
};

class ButtonManager {
public:
    ButtonManager();
    void begin();
    void loop();

    ButtonEvent getEvent(ButtonID id);
    ComboEvent getComboEvent();
    bool isWokenFromSleep() const { return woken_from_sleep; }

private:
    struct ButtonState {
        uint8_t pin;
        bool current_state;
        bool last_state;
        uint32_t last_debounce_time;
        uint32_t pressed_time;
        bool long_press_handled;
        uint32_t last_repeat_time;
        ButtonEvent pending_event;
        bool combo_handled;
    };

    ButtonState buttons[BTN_COUNT];
    ComboEvent pending_combo;
    bool woken_from_sleep;

    // Double tap detection for CANCEL
    uint32_t cancel_first_tap_time;
    bool cancel_waiting_for_double_tap;

    static const uint32_t DEBOUNCE_DELAY_MS = 80;
    static const uint32_t LONG_PRESS_MS = 700;
    static const uint32_t REPEAT_DELAY_MS = 300; // Time between repeats when holding
    static const uint32_t DOUBLE_TAP_WINDOW_MS = 250;
};

extern ButtonManager btnManager;

#endif
