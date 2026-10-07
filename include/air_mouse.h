#ifndef AIR_MOUSE_H
#define AIR_MOUSE_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <BLEHIDDevice.h>

#define MOUSE_BUTTON_LEFT   0x01
#define MOUSE_BUTTON_RIGHT  0x02
#define MOUSE_BUTTON_MIDDLE 0x04
#define MOUSE_BUTTON_BACK   0x08

enum class AirMouseMode {
    POINTER,
    SCROLL
};

enum class AirMouseSensitivity {
    SENS_LOW,
    SENS_MED,
    SENS_HIGH
};

enum class AirMouseBleStatus {
    OFF,
    CONNECTING,
    CONNECTED,
    DISCONNECTED
};

class AirMouseServerCallbacks : public BLEServerCallbacks {
public:
    AirMouseServerCallbacks(bool* connected_flag, bool* was_connected_flag, BLECharacteristic* mouse_char = nullptr);
    void onConnect(BLEServer* pServer) override;
    void onDisconnect(BLEServer* pServer) override;

private:
    bool* connected;
    bool* was_connected;
    BLECharacteristic* inputMouse;
};

class AirMouseManager {
public:
    AirMouseManager();

    void begin();
    void loop();

    void start();
    void stop();

    bool isEnabled() const { return enabled; }
    bool isConnected() const { return is_connected; }
    bool isMovementActive() const { return movement_active; }
    bool isMovementPaused() const { return !movement_active; }
    AirMouseBleStatus getBleStatus() const;

    void toggleMovement();
    void toggleMovementPause();
    void setMovementActive(bool active) { movement_active = active; }
    void toggleMode();
    void cycleSensitivity();
    void cycleSensitivityUp();
    void cycleSensitivityDown();
    void setSensitivity(AirMouseSensitivity s) { sensitivity = s; }
    void recenter();

    // Slider-like Sensitivity Adjustment
    float getSensitivityScale() const { return sensitivity_scale; }
    void setSensitivityScale(float s);
    void cycleSensitivitySlider(bool up = true);
    int getSensitivityStep() const;
    int getSensitivityLevelsCount() const;

    // Dead Zone Adjustment
    float getDeadZone() const { return dead_zone; }
    void setDeadZone(float dz);
    void cycleDeadZone(bool up = true);
    int getDeadZoneStep() const;
    int getDeadZoneLevelsCount() const;

    // Anti Dead Zone Adjustment
    float getAntiDeadZone() const { return anti_dead_zone; }
    void setAntiDeadZone(float adz);
    void cycleAntiDeadZone(bool up = true);
    int getAntiDeadZoneStep() const;
    int getAntiDeadZoneLevelsCount() const;

    // Combined Yaw and Roll
    bool getCombinedYawRoll() const { return combined_yaw_roll; }
    void setCombinedYawRoll(bool enable);
    void toggleCombinedYawRoll();

    // Precision Mode (Micro-speed 1:1 linear tracking with 6-DOF tremor suppression)
    bool getPrecisionMode() const { return precision_mode; }
    void setPrecisionMode(bool enable);
    void togglePrecisionMode();

    // 6-DOF Stabilization State
    bool isStationary() const { return is_stationary; }

    // Axis swapping & inversion
    bool getSwapXY() const { return swap_xy; }
    bool getInvX() const { return inv_x; }
    bool getInvY() const { return inv_y; }
    void setSwapXY(bool swap);
    void setInvX(bool inv);
    void setInvY(bool inv);
    void toggleSwapXY();
    void toggleInvX();
    void toggleInvY();

    // Full Mouse Capabilities (Click and Hold / Drag & Drop / Back)
    void setButton(uint8_t button_mask, bool pressed);
    void clickLeft();
    void clickRight();
    void clickBack();
    void scrollUp();
    void scrollDown();
    uint8_t getButtons() const { return buttons_state; }

    AirMouseMode getMode() const { return mode; }
    AirMouseSensitivity getSensitivity() const { return sensitivity; }

private:
    bool enabled;
    bool is_connected;
    bool was_connected;
    bool movement_active;
    AirMouseMode mode;
    AirMouseSensitivity sensitivity;
    float sensitivity_scale;
    float dead_zone;
    float anti_dead_zone;
    bool combined_yaw_roll;
    bool precision_mode;
    bool is_stationary;

    // 6-DOF Tremor & Stability History
    float prev_ax;
    float prev_ay;
    float prev_az;
    uint32_t stationary_samples;

    // Axis Mapping
    bool swap_xy;
    bool inv_x;
    bool inv_y;

    // BLE HID Objects
    BLEServer* pServer;
    BLEHIDDevice* hid;
    BLECharacteristic* inputMouse;
    uint8_t buttons_state;

    // Recenter offsets (in gyro deg/s)
    float offset_gx;
    float offset_gy;
    float offset_gz;

    // Anti-jitter filter state (1-Euro dynamic velocity model)
    float smooth_dx;
    float smooth_dy;
    float prev_omega;

    // Accelerometer tremor baseline
    float resting_accel_norm;

    // Accumulators for sub-integer cursor movement
    float accum_x;
    float accum_y;

    uint32_t last_update_ms;

    float getSensitivityMultiplier() const;
    void sendReport(uint8_t buttons, signed char x, signed char y, signed char wheel, signed char hWheel);
    void loadPreferences();
    void savePreferences();
};

extern AirMouseManager airMouse;

#endif // AIR_MOUSE_H
