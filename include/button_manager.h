#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <Arduino.h>

enum ButtonID {
    BTN_ID_UP = 0,
    BTN_ID_SEL = 1,
    BTN_ID_DN = 2,
    BTN_COUNT = 3
};

enum ButtonEvent {
    BTN_EVT_NONE = 0,
    BTN_EVT_SHORT_PRESS,
    BTN_EVT_LONG_PRESS,
    BTN_EVT_REPEAT
};

class ButtonManager {
public:
    ButtonManager();
    void begin();
    void loop();

    ButtonEvent getEvent(ButtonID id);

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
    };

    ButtonState buttons[BTN_COUNT];
    static const uint32_t DEBOUNCE_DELAY_MS = 50;
    static const uint32_t LONG_PRESS_MS = 700;
    static const uint32_t REPEAT_DELAY_MS = 300; // Time between repeats when holding
};

extern ButtonManager btnManager;

#endif
