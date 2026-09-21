#ifndef AIR_MOUSE_H
#define AIR_MOUSE_H

#include <Arduino.h>

#ifdef LOW
#undef LOW
#endif

#ifdef HIGH
#undef HIGH
#endif

#include <BleMouse.h>

enum class AirMouseMode {
    POINTER,
    SCROLL
};

enum class AirMouseSensitivity {
    SENS_LOW,
    SENS_MED,
    SENS_HIGH
};

class AirMouseManager {
public:
    AirMouseManager();

    void begin();
    void loop();

    void start();
    void stop();

    bool isEnabled() const { return enabled; }
    bool isConnected() const;
    bool isMovementActive() const { return movement_active; }

    void toggleMovement();
    void toggleMode();
    void cycleSensitivityUp();
    void cycleSensitivityDown();
    void recenter();

    void clickLeft();
    void clickRight();
    void scrollUp();
    void scrollDown();

    AirMouseMode getMode() const { return mode; }
    AirMouseSensitivity getSensitivity() const { return sensitivity; }

private:
    BleMouse* ble_mouse;
    bool enabled;
    bool movement_active;
    AirMouseMode mode;
    AirMouseSensitivity sensitivity;

    // Recenter offsets (in gyro deg/s)
    float offset_gx;
    float offset_gy;

    // Smoothing filter memory
    float smooth_dx;
    float smooth_dy;

    // Accumulators for sub-integer cursor movement
    float accum_x;
    float accum_y;

    uint32_t last_update_ms;

    float getSensitivityMultiplier() const;
};

extern AirMouseManager airMouse;

#endif // AIR_MOUSE_H
