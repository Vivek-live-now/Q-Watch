#include "air_mouse.h"
#include "sensors.h"
#include "HIDTypes.h"
#include <Preferences.h>

AirMouseManager airMouse;

static const uint8_t _mouseReportDescriptor[] = {
  USAGE_PAGE(1),       0x01, // USAGE_PAGE (Generic Desktop)
  USAGE(1),            0x02, // USAGE (Mouse)
  COLLECTION(1),       0x01, // COLLECTION (Application)
  USAGE(1),            0x01, //   USAGE (Pointer)
  COLLECTION(1),       0x00, //   COLLECTION (Physical)
  // Buttons (Left, Right, Middle, Back, Forward)
  USAGE_PAGE(1),       0x09, //     USAGE_PAGE (Button)
  USAGE_MINIMUM(1),    0x01, //     USAGE_MINIMUM (Button 1)
  USAGE_MAXIMUM(1),    0x05, //     USAGE_MAXIMUM (Button 5)
  LOGICAL_MINIMUM(1),  0x00, //     LOGICAL_MINIMUM (0)
  LOGICAL_MAXIMUM(1),  0x01, //     LOGICAL_MAXIMUM (1)
  REPORT_SIZE(1),      0x01, //     REPORT_SIZE (1)
  REPORT_COUNT(1),     0x05, //     REPORT_COUNT (5)
  HIDINPUT(1),         0x02, //     INPUT (Data, Variable, Absolute)
  // Padding
  REPORT_SIZE(1),      0x03, //     REPORT_SIZE (3)
  REPORT_COUNT(1),     0x01, //     REPORT_COUNT (1)
  HIDINPUT(1),         0x03, //     INPUT (Constant, Variable, Absolute)
  // X/Y position, Wheel
  USAGE_PAGE(1),       0x01, //     USAGE_PAGE (Generic Desktop)
  USAGE(1),            0x30, //     USAGE (X)
  USAGE(1),            0x31, //     USAGE (Y)
  USAGE(1),            0x38, //     USAGE (Wheel)
  LOGICAL_MINIMUM(1),  0x81, //     LOGICAL_MINIMUM (-127)
  LOGICAL_MAXIMUM(1),  0x7f, //     LOGICAL_MAXIMUM (127)
  REPORT_SIZE(1),      0x08, //     REPORT_SIZE (8)
  REPORT_COUNT(1),     0x03, //     REPORT_COUNT (3)
  HIDINPUT(1),         0x06, //     INPUT (Data, Variable, Relative)
  // Horizontal wheel
  USAGE_PAGE(1),       0x0c, //     USAGE PAGE (Consumer Devices)
  USAGE(2),      0x38, 0x02, //     USAGE (AC Pan)
  LOGICAL_MINIMUM(1),  0x81, //     LOGICAL_MINIMUM (-127)
  LOGICAL_MAXIMUM(1),  0x7f, //     LOGICAL_MAXIMUM (127)
  REPORT_SIZE(1),      0x08, //     REPORT_SIZE (8)
  REPORT_COUNT(1),     0x01, //     REPORT_COUNT (1)
  HIDINPUT(1),         0x06, //     INPUT (Data, Var, Rel)
  END_COLLECTION(0),
  END_COLLECTION(0)
};

AirMouseServerCallbacks::AirMouseServerCallbacks(bool* connected_flag, bool* was_connected_flag, BLECharacteristic* mouse_char)
    : connected(connected_flag), was_connected(was_connected_flag), inputMouse(mouse_char) {}

void AirMouseServerCallbacks::onConnect(BLEServer* pServer) {
    if (connected) *connected = true;
    if (was_connected) *was_connected = true;
    if (inputMouse) {
        BLE2902* desc = (BLE2902*)inputMouse->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
        if (desc) {
            desc->setNotifications(true);
        }
    }
}

void AirMouseServerCallbacks::onDisconnect(BLEServer* pServer) {
    if (connected) *connected = false;
}

AirMouseManager::AirMouseManager() :
    enabled(false),
    is_connected(false),
    was_connected(false),
    movement_active(true),
    mode(AirMouseMode::POINTER),
    sensitivity(AirMouseSensitivity::SENS_MED),
    swap_xy(false),
    inv_x(false),
    inv_y(false),
    pServer(nullptr),
    hid(nullptr),
    inputMouse(nullptr),
    buttons_state(0),
    offset_gx(0.0f),
    offset_gy(0.0f),
    smooth_dx(0.0f),
    smooth_dy(0.0f),
    prev_omega(0.0f),
    resting_accel_norm(1.0f),
    accum_x(0.0f),
    accum_y(0.0f),
    last_update_ms(0) {
}

void AirMouseManager::begin() {
    loadPreferences();
}

void AirMouseManager::loadPreferences() {
    Preferences p;
    p.begin("airmouse", true);
    swap_xy = p.getBool("swap_xy", false);
    inv_x = p.getBool("inv_x", false);
    inv_y = p.getBool("inv_y", false);
    int sens = p.getInt("sens", (int)AirMouseSensitivity::SENS_MED);
    sensitivity = (AirMouseSensitivity)sens;
    p.end();
}

void AirMouseManager::savePreferences() {
    Preferences p;
    p.begin("airmouse", false);
    p.putBool("swap_xy", swap_xy);
    p.putBool("inv_x", inv_x);
    p.putBool("inv_y", inv_y);
    p.putInt("sens", (int)sensitivity);
    p.end();
}

void AirMouseManager::setSwapXY(bool swap) { swap_xy = swap; savePreferences(); }
void AirMouseManager::setInvX(bool inv) { inv_x = inv; savePreferences(); }
void AirMouseManager::setInvY(bool inv) { inv_y = inv; savePreferences(); }
void AirMouseManager::toggleSwapXY() { swap_xy = !swap_xy; savePreferences(); }
void AirMouseManager::toggleInvX() { inv_x = !inv_x; savePreferences(); }
void AirMouseManager::toggleInvY() { inv_y = !inv_y; savePreferences(); }

void AirMouseManager::start() {
    if (enabled) return;

    if (!BLEDevice::getInitialized()) {
        BLEDevice::init("Q-Watch Air Mouse");
    }

    if (!pServer) {
        pServer = BLEDevice::createServer();
        hid = new BLEHIDDevice(pServer);
        inputMouse = hid->inputReport(0);
        inputMouse->addDescriptor(new BLE2902());
        pServer->setCallbacks(new AirMouseServerCallbacks(&is_connected, &was_connected, inputMouse));

        hid->manufacturer()->setValue("Q-Branch");
        hid->pnp(0x02, 0xe502, 0xa111, 0x0210);
        hid->hidInfo(0x00, 0x02);

        BLESecurity* pSecurity = new BLESecurity();
        pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);
        pSecurity->setCapability(ESP_IO_CAP_NONE);
        pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

        hid->reportMap((uint8_t*)_mouseReportDescriptor, sizeof(_mouseReportDescriptor));
        hid->startServices();

        BLEAdvertising* pAdvertising = pServer->getAdvertising();
        pAdvertising->setAppearance(HID_MOUSE);
        pAdvertising->addServiceUUID(hid->hidService()->getUUID());
        hid->setBatteryLevel(100);
    }

    BLEAdvertising* pAdvertising = pServer->getAdvertising();
    if (pAdvertising) {
        pAdvertising->start();
    }

    enabled = true;
    is_connected = false;
    was_connected = false;
    movement_active = true;
    offset_gx = 0.0f;
    offset_gy = 0.0f;
    smooth_dx = 0.0f;
    smooth_dy = 0.0f;
    accum_x = 0.0f;
    accum_y = 0.0f;
    buttons_state = 0;
    last_update_ms = millis();
}

void AirMouseManager::stop() {
    if (!enabled) return;

    if (pServer) {
        BLEAdvertising* pAdvertising = pServer->getAdvertising();
        if (pAdvertising) {
            pAdvertising->stop();
        }
    }

    BLEDevice::deinit(false);

    pServer = nullptr;
    hid = nullptr;
    inputMouse = nullptr;
    enabled = false;
    is_connected = false;
    was_connected = false;
    buttons_state = 0;
}

AirMouseBleStatus AirMouseManager::getBleStatus() const {
    if (!enabled) return AirMouseBleStatus::OFF;
    if (is_connected) return AirMouseBleStatus::CONNECTED;
    if (was_connected) return AirMouseBleStatus::DISCONNECTED;
    return AirMouseBleStatus::CONNECTING;
}

void AirMouseManager::toggleMovement() {
    movement_active = !movement_active;
}

void AirMouseManager::toggleMovementPause() {
    toggleMovement();
}

void AirMouseManager::toggleMode() {
    if (mode == AirMouseMode::POINTER) {
        mode = AirMouseMode::SCROLL;
    } else {
        mode = AirMouseMode::POINTER;
    }
}

void AirMouseManager::cycleSensitivity() {
    if (sensitivity == AirMouseSensitivity::SENS_LOW) sensitivity = AirMouseSensitivity::SENS_MED;
    else if (sensitivity == AirMouseSensitivity::SENS_MED) sensitivity = AirMouseSensitivity::SENS_HIGH;
    else sensitivity = AirMouseSensitivity::SENS_LOW;
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

void AirMouseManager::sendReport(uint8_t buttons, signed char x, signed char y, signed char wheel, signed char hWheel) {
    if (is_connected && inputMouse) {
        uint8_t m[5];
        m[0] = buttons;
        m[1] = x;
        m[2] = y;
        m[3] = wheel;
        m[4] = hWheel;
        inputMouse->setValue(m, 5);
        inputMouse->notify();
    }
}

void AirMouseManager::setButton(uint8_t button_mask, bool pressed) {
    uint8_t prev = buttons_state;
    if (pressed) {
        buttons_state |= button_mask;
    } else {
        buttons_state &= ~button_mask;
    }
    if (prev != buttons_state && is_connected && movement_active) {
        sendReport(buttons_state, 0, 0, 0, 0);
    }
}

void AirMouseManager::clickLeft() {
    if (is_connected && movement_active) {
        setButton(0x01, true);
        delay(12);
        setButton(0x01, false);
    }
}

void AirMouseManager::clickRight() {
    if (is_connected && movement_active) {
        setButton(0x02, true);
        delay(12);
        setButton(0x02, false);
    }
}

void AirMouseManager::clickBack() {
    if (is_connected && movement_active) {
        setButton(0x08, true); // Button 4: Back
        delay(12);
        setButton(0x08, false);
    }
}

void AirMouseManager::scrollUp() {
    if (is_connected && movement_active) {
        sendReport(buttons_state, 0, 0, 1, 0);
    }
}

void AirMouseManager::scrollDown() {
    if (is_connected && movement_active) {
        sendReport(buttons_state, 0, 0, -1, 0);
    }
}

float AirMouseManager::getSensitivityMultiplier() const {
    switch (sensitivity) {
        case AirMouseSensitivity::SENS_LOW:  return 0.6f;
        case AirMouseSensitivity::SENS_MED:  return 1.0f;
        case AirMouseSensitivity::SENS_HIGH: return 1.8f;
        default: return 1.0f;
    }
}

void AirMouseManager::loop() {
    if (!enabled || !is_connected || !movement_active) {
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
    // Watch frame: Gyro Y -> horizontal roll/pan, Gyro X -> vertical pitch/tilt
    float raw_gx = cal.gy - offset_gy;
    float raw_gy = cal.gx - offset_gx;

    // Accelerometer Stability & Tremor Fusion:
    // When the watch is resting or user is aiming at a small target, ||accel|| is steady 1.0g
    float ax = cal.ax, ay = cal.ay, az = cal.az;
    float accel_norm = sqrtf(ax * ax + ay * ay + az * az);
    float accel_jitter = fabsf(accel_norm - resting_accel_norm);
    resting_accel_norm = resting_accel_norm * 0.95f + accel_norm * 0.05f;

    // Angular velocity magnitude (deg/s)
    float omega = sqrtf(raw_gx * raw_gx + raw_gy * raw_gy);

    // Adaptive deadband:
    // If hand is resting / aiming steadily, suppress noise completely
    const float DEAD_ZONE = 1.8f; // deg/s
    float dynamic_deadband = DEAD_ZONE;
    if (accel_jitter < 0.035f && omega < 4.0f) {
        dynamic_deadband = DEAD_ZONE + 0.3f; // Extra steady deadband during resting hand
    }

    float gx = 0.0f;
    float gy = 0.0f;
    if (omega > dynamic_deadband) {
        float scale = (omega - dynamic_deadband) / omega;
        gx = raw_gx * scale;
        gy = raw_gy * scale;
    }

    // 1-Euro / Dynamic Velocity Filter:
    // Low velocity (omega < 6 deg/s): heavy smoothing (alpha = 0.12) to eradicate physiological hand tremor
    // High velocity (omega > 25 deg/s): scales up to alpha = 0.65 for instant flick responsiveness
    float alpha = 0.12f;
    if (omega > 4.0f) {
        float factor = (omega - 4.0f) / 28.0f;
        if (factor > 1.0f) factor = 1.0f;
        alpha = 0.12f + 0.53f * factor;
    }

    smooth_dx = smooth_dx + alpha * (gx - smooth_dx);
    smooth_dy = smooth_dy + alpha * (gy - smooth_dy);

    // Non-linear power-law acceleration curve:
    // Fine movements give 1-2 pixel accuracy; swift flicks traverse desktop monitors effortlessly
    float speed = sqrtf(smooth_dx * smooth_dx + smooth_dy * smooth_dy);
    float accel_mult = 1.0f + 0.06f * powf(speed, 0.45f);

    float mult = getSensitivityMultiplier();
    float delta_x = smooth_dx * mult * accel_mult * dt * 28.0f;
    float delta_y = smooth_dy * mult * accel_mult * dt * 28.0f;

    // Axis swapping & inversion
    float final_move_x = swap_xy ? delta_y : delta_x;
    float final_move_y = swap_xy ? delta_x : delta_y;
    if (inv_x) final_move_x = -final_move_x;
    if (inv_y) final_move_y = -final_move_y;

    accum_x += final_move_x;
    accum_y += final_move_y;

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

        // CRITICAL: Always send current buttons_state so click-and-drag / text-selection works!
        sendReport(buttons_state, (signed char)move_x, (signed char)move_y, 0, 0);
    }
}
