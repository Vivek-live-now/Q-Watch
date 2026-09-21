#include "air_mouse.h"
#include "sensors.h"

AirMouseManager airMouse;

AirMouseManager::AirMouseManager() :
    ble_mouse(nullptr),
    enabled(false),
    movement_active(true),
    mode(AirMouseMode::POINTER),
    sensitivity(AirMouseSensitivity::SENS_MED),
    offset_gx(0.0f),
    offset_gy(0.0f),
    smooth_dx(0.0f),
    smooth_dy(0.0f),
    accum_x(0.0f),
    accum_y(0.0f),
    last_update_ms(0) {
}

void AirMouseManager::begin() {
    // Lazy init or allocated on demand
}

void AirMouseManager::start() {
    if (!ble_mouse) {
        ble_mouse = new BleMouse("Q-Watch Air Mouse", "Q-Branch", 100);
    }
    if (ble_mouse && !enabled) {
        ble_mouse->begin();
        enabled = true;
        movement_active = true;
        offset_gx = 0.0f;
        offset_gy = 0.0f;
        smooth_dx = 0.0f;
        smooth_dy = 0.0f;
        accum_x = 0.0f;
        accum_y = 0.0f;
        last_update_ms = millis();
    }
}

void AirMouseManager::stop() {
    if (enabled && ble_mouse) {
        ble_mouse->end();
        delete ble_mouse;
        ble_mouse = nullptr;
        enabled = false;
    }
}

bool AirMouseManager::isConnected() const {
    return (enabled && ble_mouse && ble_mouse->isConnected());
}

void AirMouseManager::toggleMovement() {
    movement_active = !movement_active;
}

void AirMouseManager::toggleMode() {
    if (mode == AirMouseMode::POINTER) {
        mode = AirMouseMode::SCROLL;
    } else {
        mode = AirMouseMode::POINTER;
    }
}

void AirMouseManager::cycleSensitivityUp() {
    if (sensitivity == AirMouseSensitivity::SENS_LOW) sensitivity = AirMouseSensitivity::SENS_MED;
    else if (sensitivity == AirMouseSensitivity::SENS_MED) sensitivity = AirMouseSensitivity::SENS_HIGH;
}

void AirMouseManager::cycleSensitivityDown() {
    if (sensitivity == AirMouseSensitivity::SENS_HIGH) sensitivity = AirMouseSensitivity::SENS_MED;
    else if (sensitivity == AirMouseSensitivity::SENS_MED) sensitivity = AirMouseSensitivity::SENS_LOW;
}

void AirMouseManager::recenter() {
    CalibratedSensorData cal = sensors.getCalData();
    offset_gx = cal.gx;
    offset_gy = cal.gy;
    smooth_dx = 0.0f;
    smooth_dy = 0.0f;
    accum_x = 0.0f;
    accum_y = 0.0f;
}

void AirMouseManager::clickLeft() {
    if (isConnected()) {
        ble_mouse->click(MOUSE_LEFT);
    }
}

void AirMouseManager::clickRight() {
    if (isConnected()) {
        ble_mouse->click(MOUSE_RIGHT);
    }
}

void AirMouseManager::scrollUp() {
    if (isConnected()) {
        ble_mouse->move(0, 0, 1);
    }
}

void AirMouseManager::scrollDown() {
    if (isConnected()) {
        ble_mouse->move(0, 0, -1);
    }
}

float AirMouseManager::getSensitivityMultiplier() const {
    switch (sensitivity) {
        case AirMouseSensitivity::SENS_LOW:  return 0.5f;
        case AirMouseSensitivity::SENS_MED:  return 1.0f;
        case AirMouseSensitivity::SENS_HIGH: return 2.0f;
        default: return 1.0f;
    }
}

void AirMouseManager::loop() {
    if (!enabled || !isConnected() || !movement_active) {
        last_update_ms = millis();
        return;
    }

    uint32_t now = millis();
    float dt = (now - last_update_ms) / 1000.0f;
    last_update_ms = now;

    if (dt <= 0.001f || dt > 0.2f) {
        dt = 0.01f;
    }

    CalibratedSensorData cal = sensors.getCalData();

    // Raw calibrated gyro values minus recenter offset
    // Mapping: Gyro Y -> cursor X, Gyro X -> cursor Y
    float gx = cal.gy - offset_gy;
    float gy = cal.gx - offset_gx;

    // Dead zone check (~5-10 deg/s threshold)
    const float DEAD_ZONE = 6.0f; // deg/s
    if (fabsf(gx) < DEAD_ZONE) gx = 0.0f;
    else if (gx > 0) gx -= DEAD_ZONE;
    else gx += DEAD_ZONE;

    if (fabsf(gy) < DEAD_ZONE) gy = 0.0f;
    else if (gy > 0) gy -= DEAD_ZONE;
    else gy += DEAD_ZONE;

    // Low-pass exponential smoothing
    const float ALPHA = 0.35f;
    smooth_dx = smooth_dx + ALPHA * (gx - smooth_dx);
    smooth_dy = smooth_dy + ALPHA * (gy - smooth_dy);

    // Apply sensitivity multiplier
    float mult = getSensitivityMultiplier();
    float raw_move_x = smooth_dx * mult * dt * 8.0f;
    float raw_move_y = smooth_dy * mult * dt * 8.0f;

    accum_x += raw_move_x;
    accum_y += raw_move_y;

    int move_x = (int)accum_x;
    int move_y = (int)accum_y;

    if (move_x != 0 || move_y != 0) {
        accum_x -= move_x;
        accum_y -= move_y;

        // Clamp HID int8_t range (-127 to +127)
        if (move_x > 127) move_x = 127;
        if (move_x < -127) move_x = -127;
        if (move_y > 127) move_y = 127;
        if (move_y < -127) move_y = -127;

        ble_mouse->move((signed char)move_x, (signed char)move_y);
    }
}
