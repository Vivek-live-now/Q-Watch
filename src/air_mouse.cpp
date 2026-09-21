#include "air_mouse.h"
#include "sensors.h"
#include "HIDTypes.h"

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

AirMouseServerCallbacks::AirMouseServerCallbacks(bool* connected_flag, bool* was_connected_flag)
    : connected(connected_flag), was_connected(was_connected_flag) {}

void AirMouseServerCallbacks::onConnect(BLEServer* pServer) {
    if (connected) *connected = true;
    if (was_connected) *was_connected = true;
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
    pServer(nullptr),
    hid(nullptr),
    inputMouse(nullptr),
    buttons_state(0),
    offset_gx(0.0f),
    offset_gy(0.0f),
    smooth_dx(0.0f),
    smooth_dy(0.0f),
    accum_x(0.0f),
    accum_y(0.0f),
    last_update_ms(0) {
}

void AirMouseManager::begin() {
}

void AirMouseManager::start() {
    if (enabled) return;

    BLEDevice::init("Q-Watch Air Mouse");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new AirMouseServerCallbacks(&is_connected, &was_connected));

    hid = new BLEHIDDevice(pServer);
    inputMouse = hid->inputReport(0);

    hid->manufacturer()->setValue("Q-Branch");
    hid->pnp(0x02, 0xe502, 0xa111, 0x0210);
    hid->hidInfo(0x00, 0x02);

    BLESecurity* pSecurity = new BLESecurity();
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);

    hid->reportMap((uint8_t*)_mouseReportDescriptor, sizeof(_mouseReportDescriptor));
    hid->startServices();

    BLEAdvertising* pAdvertising = pServer->getAdvertising();
    pAdvertising->setAppearance(HID_MOUSE);
    pAdvertising->addServiceUUID(hid->hidService()->getUUID());
    pAdvertising->start();
    hid->setBatteryLevel(100);

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

    BLEDevice::deinit(true);

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

void AirMouseManager::clickLeft() {
    if (is_connected && movement_active) {
        sendReport(0x01, 0, 0, 0, 0);
        sendReport(0x00, 0, 0, 0, 0);
    }
}

void AirMouseManager::clickRight() {
    if (is_connected && movement_active) {
        sendReport(0x02, 0, 0, 0, 0);
        sendReport(0x00, 0, 0, 0, 0);
    }
}

void AirMouseManager::scrollUp() {
    if (is_connected && movement_active) {
        sendReport(0, 0, 0, 1, 0);
    }
}

void AirMouseManager::scrollDown() {
    if (is_connected && movement_active) {
        sendReport(0, 0, 0, -1, 0);
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

        sendReport(0, (signed char)move_x, (signed char)move_y, 0, 0);
    }
}
