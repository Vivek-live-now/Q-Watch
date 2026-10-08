#include "air_mouse.h"
#include "sensors.h"
#include "HIDTypes.h"
#include "qlink.h"
#include "settings_data.h"
#include <Preferences.h>
#ifdef ARDUINO
#include <esp_gap_ble_api.h>
#endif

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

AirMouseServerCallbacks::AirMouseServerCallbacks(volatile bool* connected_flag, volatile bool* was_connected_flag, BLECharacteristic* mouse_char)
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
    if (pServer) {
        BLEAdvertising* pAdv = pServer->getAdvertising();
        if (pAdv) {
            pAdv->start();
        }
    }
}

AirMouseManager::AirMouseManager() :
    enabled(false),
    is_connected(false),
    was_connected(false),
    movement_active(true),
    mode(AirMouseMode::POINTER),
    sensitivity(AirMouseSensitivity::SENS_MED),
    sensitivity_scale(1.0f),
    dead_zone(1.8f),
    anti_dead_zone(0.6f),
    combined_yaw_roll(true),
    precision_mode(false),
    is_stationary(false),
    prev_ax(0.0f),
    prev_ay(0.0f),
    prev_az(1.0f),
    stationary_samples(0),
    swap_xy(false),
    inv_x(false),
    inv_y(false),
    pServer(nullptr),
    hid(nullptr),
    inputMouse(nullptr),
    pCallbacks(nullptr),
    pSecurity(nullptr),
    buttons_state(0),
    offset_gx(0.0f),
    offset_gy(0.0f),
    offset_gz(0.0f),
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
    sensitivity_scale = p.getFloat("sens_scl", 1.0f);
    dead_zone = p.getFloat("dead_zone", 1.8f);
    anti_dead_zone = p.getFloat("anti_dz", 0.6f);
    combined_yaw_roll = p.getBool("comb_yr", true);
    precision_mode = p.getBool("prec_mode", false);
    p.end();
}

void AirMouseManager::savePreferences() {
    Preferences p;
    p.begin("airmouse", false);
    p.putBool("swap_xy", swap_xy);
    p.putBool("inv_x", inv_x);
    p.putBool("inv_y", inv_y);
    p.putInt("sens", (int)sensitivity);
    p.putFloat("sens_scl", sensitivity_scale);
    p.putFloat("dead_zone", dead_zone);
    p.putFloat("anti_dz", anti_dead_zone);
    p.putBool("comb_yr", combined_yaw_roll);
    p.putBool("prec_mode", precision_mode);
    p.end();
}

void AirMouseManager::setSwapXY(bool swap) { swap_xy = swap; savePreferences(); }
void AirMouseManager::setInvX(bool inv) { inv_x = inv; savePreferences(); }
void AirMouseManager::setInvY(bool inv) { inv_y = inv; savePreferences(); }
void AirMouseManager::toggleSwapXY() { swap_xy = !swap_xy; savePreferences(); }
void AirMouseManager::toggleInvX() { inv_x = !inv_x; savePreferences(); }
void AirMouseManager::toggleInvY() { inv_y = !inv_y; savePreferences(); }

void AirMouseManager::setCombinedYawRoll(bool enable) { combined_yaw_roll = enable; savePreferences(); }
void AirMouseManager::toggleCombinedYawRoll() { combined_yaw_roll = !combined_yaw_roll; savePreferences(); }

void AirMouseManager::setPrecisionMode(bool enable) { precision_mode = enable; savePreferences(); }
void AirMouseManager::togglePrecisionMode() { precision_mode = !precision_mode; savePreferences(); }

void AirMouseManager::start() {
    if (enabled) return;

    loadPreferences();

    if (!BLEDevice::getInitialized()) {
        BLEDevice::init("Q-Watch Air Mouse");
    }

#ifdef ARDUINO
    // Set BLE RF TX power level to -3 dBm to eliminate excessive heating on ESP32-S3 SuperMini
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_N3);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_N3);
#endif

    if (!pServer) {
        pServer = BLEDevice::createServer();
        hid = new BLEHIDDevice(pServer);
        inputMouse = hid->inputReport(0);
        inputMouse->addDescriptor(new BLE2902());
        pCallbacks = new AirMouseServerCallbacks(&is_connected, &was_connected, inputMouse);
        pServer->setCallbacks(pCallbacks);

        hid->manufacturer()->setValue("Q-Branch");
        hid->pnp(0x02, 0xe502, 0xa111, 0x0210);
        hid->hidInfo(0x00, 0x02);

        pSecurity = new BLESecurity();
        pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);
        pSecurity->setCapability(ESP_IO_CAP_NONE);
        pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

        hid->reportMap((uint8_t*)_mouseReportDescriptor, sizeof(_mouseReportDescriptor));
        hid->startServices();

        BLEAdvertising* pAdvertising = pServer->getAdvertising();
        pAdvertising->setAppearance(HID_MOUSE);
        pAdvertising->addServiceUUID(hid->hidService()->getUUID());
        pAdvertising->setMinInterval(0x100); // 160ms
        pAdvertising->setMaxInterval(0x200); // 320ms
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
    CalibratedSensorData cal = sensors.getCalData();
    offset_gx = cal.gx;
    offset_gy = cal.gy;
    offset_gz = cal.gz;
    smooth_dx = 0.0f;
    smooth_dy = 0.0f;
    accum_x = 0.0f;
    accum_y = 0.0f;
    buttons_state = 0;
    is_stationary = false;
    stationary_samples = 0;
    prev_ax = cal.ax;
    prev_ay = cal.ay;
    prev_az = cal.az;
    last_update_ms = millis();
}

void AirMouseManager::restartAdvertising() {
    if (enabled && pServer) {
        BLEAdvertising* pAdvertising = pServer->getAdvertising();
        if (pAdvertising) {
            pAdvertising->start();
        }
    }
}

void AirMouseManager::stop() {
    if (!enabled) return;

    if (is_connected && inputMouse) {
        sendReport(0, 0, 0, 0, 0);
    }

    if (pServer) {
        BLEAdvertising* pAdvertising = pServer->getAdvertising();
        if (pAdvertising) {
            pAdvertising->stop();
        }
    }

    if (!settingsManager.get().ble_enabled && !qlink.isBleConnected()) {
        BLEDevice::deinit(false);
    } else {
        BLEAdvertising* pAdv = BLEDevice::getAdvertising();
        if (pAdv) {
            pAdv->stop();
            pAdv->addServiceUUID(QLINK_SERVICE_UUID);
            pAdv->start();
        }
    }

    if (pCallbacks) {
        delete pCallbacks;
        pCallbacks = nullptr;
    }
    if (pSecurity) {
        delete pSecurity;
        pSecurity = nullptr;
    }

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
    if (!movement_active) {
        if (buttons_state != 0 && is_connected) {
            buttons_state = 0;
            sendReport(0, 0, 0, 0, 0);
        } else {
            buttons_state = 0;
        }
    }
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

static const float kSensLevels[] = {0.15f, 0.3f, 0.5f, 0.8f, 1.0f, 1.3f, 1.8f, 2.4f, 3.2f, 4.2f};
static const int kNumSensLevels = 10;

static const float kDeadZoneLevels[] = {0.5f, 1.0f, 1.8f, 2.5f, 3.5f, 5.0f, 7.0f, 10.0f};
static const int kNumDeadZoneLevels = 8;

static const float kAntiDeadLevels[] = {0.0f, 0.3f, 0.6f, 1.0f, 1.5f, 2.0f, 2.6f, 3.4f};
static const int kNumAntiDeadLevels = 8;

int AirMouseManager::getSensitivityStep() const {
    int closest = 0;
    float min_diff = 999.0f;
    for (int i = 0; i < kNumSensLevels; i++) {
        float diff = fabsf(sensitivity_scale - kSensLevels[i]);
        if (diff < min_diff) {
            min_diff = diff;
            closest = i;
        }
    }
    return closest;
}

int AirMouseManager::getSensitivityLevelsCount() const {
    return kNumSensLevels;
}

void AirMouseManager::setSensitivityScale(float s) {
    sensitivity_scale = s;
    if (sensitivity_scale <= 0.8f) sensitivity = AirMouseSensitivity::SENS_LOW;
    else if (sensitivity_scale >= 2.0f) sensitivity = AirMouseSensitivity::SENS_HIGH;
    else sensitivity = AirMouseSensitivity::SENS_MED;
    savePreferences();
}

void AirMouseManager::cycleSensitivitySlider(bool up) {
    int cur = getSensitivityStep();
    if (up) {
        cur = (cur + 1) % kNumSensLevels;
    } else {
        cur = (cur + kNumSensLevels - 1) % kNumSensLevels;
    }
    setSensitivityScale(kSensLevels[cur]);
}

void AirMouseManager::cycleSensitivity() {
    cycleSensitivitySlider(true);
}

void AirMouseManager::cycleSensitivityUp() {
    cycleSensitivitySlider(true);
}

void AirMouseManager::cycleSensitivityDown() {
    cycleSensitivitySlider(false);
}

int AirMouseManager::getDeadZoneStep() const {
    int closest = 0;
    float min_diff = 999.0f;
    for (int i = 0; i < kNumDeadZoneLevels; i++) {
        float diff = fabsf(dead_zone - kDeadZoneLevels[i]);
        if (diff < min_diff) {
            min_diff = diff;
            closest = i;
        }
    }
    return closest;
}

int AirMouseManager::getDeadZoneLevelsCount() const {
    return kNumDeadZoneLevels;
}

void AirMouseManager::setDeadZone(float dz) {
    dead_zone = dz;
    savePreferences();
}

void AirMouseManager::cycleDeadZone(bool up) {
    int cur = getDeadZoneStep();
    if (up) {
        cur = (cur + 1) % kNumDeadZoneLevels;
    } else {
        cur = (cur + kNumDeadZoneLevels - 1) % kNumDeadZoneLevels;
    }
    setDeadZone(kDeadZoneLevels[cur]);
}

int AirMouseManager::getAntiDeadZoneStep() const {
    int closest = 0;
    float min_diff = 999.0f;
    for (int i = 0; i < kNumAntiDeadLevels; i++) {
        float diff = fabsf(anti_dead_zone - kAntiDeadLevels[i]);
        if (diff < min_diff) {
            min_diff = diff;
            closest = i;
        }
    }
    return closest;
}

int AirMouseManager::getAntiDeadZoneLevelsCount() const {
    return kNumAntiDeadLevels;
}

void AirMouseManager::setAntiDeadZone(float adz) {
    anti_dead_zone = adz;
    savePreferences();
}

void AirMouseManager::cycleAntiDeadZone(bool up) {
    int cur = getAntiDeadZoneStep();
    if (up) {
        cur = (cur + 1) % kNumAntiDeadLevels;
    } else {
        cur = (cur + kNumAntiDeadLevels - 1) % kNumAntiDeadLevels;
    }
    setAntiDeadZone(kAntiDeadLevels[cur]);
}

void AirMouseManager::recenter() {
    CalibratedSensorData cal = sensors.getCalData();
    offset_gx = cal.gx;
    offset_gy = cal.gy;
    offset_gz = cal.gz;
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
    if (sensitivity_scale > 0.01f) {
        return sensitivity_scale;
    }
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

    if (dt <= 0.001f) {
        return;
    }
    last_update_ms = now;

    if (dt > 0.2f) {
        dt = 0.01f;
    }

    CalibratedSensorData cal = sensors.getCalData();

    // Raw calibrated gyro values minus recenter offset
    // Watch frame: Gyro Y -> roll, Gyro X -> pitch/tilt, Gyro Z -> yaw
    float roll = cal.gy - offset_gy;
    float pitch = cal.gx - offset_gx;
    float yaw = -(cal.gz - offset_gz);

    // Combined yaw and roll for intuitive horizontal pointing
    float raw_gx = combined_yaw_roll ? (roll + yaw) : roll;
    float raw_gy = pitch;

    // 6-DOF Accelerometer & Gyroscope Fusion:
    // 1) Accelerometer magnitude & resting deviation:
    float ax = cal.ax, ay = cal.ay, az = cal.az;
    float accel_norm = sqrtf(ax * ax + ay * ay + az * az);
    float accel_jitter = fabsf(accel_norm - resting_accel_norm);
    resting_accel_norm = resting_accel_norm * 0.95f + accel_norm * 0.05f;

    // 2) 3-Axis Accelerometer delta (translational motion / vibration):
    float d_ax = ax - prev_ax;
    float d_ay = ay - prev_ay;
    float d_az = az - prev_az;
    float accel_delta = sqrtf(d_ax * d_ax + d_ay * d_ay + d_az * d_az);
    prev_ax = ax; prev_ay = ay; prev_az = az;

    // 3) Angular velocity magnitude (deg/s)
    float omega = sqrtf(raw_gx * raw_gx + raw_gy * raw_gy);

    // Adaptive deadband with user dead zone and anti dead zone:
    // If hand is resting / aiming steadily, suppress noise completely
    const float DEAD_ZONE = 1.8f; // base reference
    float active_dead_zone = (dead_zone > 0.01f) ? dead_zone : DEAD_ZONE;
    float dynamic_deadband = active_dead_zone;
    if (accel_jitter < 0.035f && omega < 4.0f) {
        dynamic_deadband = active_dead_zone + 0.3f; // Extra steady deadband during resting hand
    }

    // 6-DOF True Stationary Check:
    // Uses true physical 3-axis gyro angular velocity magnitude (independent of mapping and user dead-zone)
    float true_gyro_omega = sqrtf(cal.gx * cal.gx + cal.gy * cal.gy + cal.gz * cal.gz);
    bool current_still = (accel_jitter < 0.03f && accel_delta < 0.04f && true_gyro_omega < 1.2f);
    if (current_still) {
        stationary_samples++;
    } else {
        stationary_samples = 0;
    }
    is_stationary = (stationary_samples >= 3);

    if (is_stationary) {
        // Continuous Zero-Drift Tracking:
        // Automatically adapt gyro bias offsets while watch is stationary!
        offset_gx += 0.03f * (cal.gx - offset_gx);
        offset_gy += 0.03f * (cal.gy - offset_gy);
        offset_gz += 0.03f * (cal.gz - offset_gz);

        // Immediate hard clamp to zero: ABSOLUTELY ZERO DRIFT!
        smooth_dx = 0.0f;
        smooth_dy = 0.0f;
        accum_x = 0.0f;
        accum_y = 0.0f;
        return;
    }

    float gx = 0.0f;
    float gy = 0.0f;
    if (omega > dynamic_deadband) {
        float excess = omega - dynamic_deadband;
        float ramp = (excess < 1.0f) ? (excess / 1.0f) : 1.0f;
        float effective_omega = excess + anti_dead_zone * ramp;
        float scale = effective_omega / omega;
        gx = raw_gx * scale;
        gy = raw_gy * scale;
    } else {
        accum_x *= 0.5f;
        accum_y *= 0.5f;
        if (fabsf(accum_x) < 0.05f) accum_x = 0.0f;
        if (fabsf(accum_y) < 0.05f) accum_y = 0.0f;
    }

    // Dynamic Velocity Filter:
    float alpha = 0.12f;
    if (precision_mode) {
        alpha = 0.07f;
        if (omega > 5.0f) {
            float factor = (omega - 5.0f) / 18.0f;
            if (factor > 1.0f) factor = 1.0f;
            alpha = 0.07f + 0.35f * factor;
        }
    } else {
        if (omega > 4.0f) {
            float factor = (omega - 4.0f) / 28.0f;
            if (factor > 1.0f) factor = 1.0f;
            alpha = 0.12f + 0.53f * factor;
        }
    }

    smooth_dx = smooth_dx + alpha * (gx - smooth_dx);
    smooth_dy = smooth_dy + alpha * (gy - smooth_dy);

    // Non-linear power-law acceleration curve:
    float speed = sqrtf(smooth_dx * smooth_dx + smooth_dy * smooth_dy);
    float accel_mult = 1.0f;
    if (!precision_mode) {
        accel_mult = 1.0f + 0.06f * powf(speed, 0.45f);
    }

    float mult = getSensitivityMultiplier();
    if (precision_mode) {
        mult *= 0.35f; // Precision Mode micro-speed scaling
    }
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
        int clamped_x = move_x;
        int clamped_y = move_y;

        // Clamp HID int8_t range (-127 to +127)
        if (clamped_x > 127) clamped_x = 127;
        if (clamped_x < -127) clamped_x = -127;
        if (clamped_y > 127) clamped_y = 127;
        if (clamped_y < -127) clamped_y = -127;

        accum_x -= clamped_x;
        accum_y -= clamped_y;

        // CRITICAL: Always send current buttons_state so click-and-drag / text-selection works!
        sendReport(buttons_state, (signed char)clamped_x, (signed char)clamped_y, 0, 0);
    }
}
