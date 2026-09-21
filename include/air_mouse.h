#ifndef AIR_MOUSE_H
#define AIR_MOUSE_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <BLEHIDDevice.h>

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
    AirMouseServerCallbacks(bool* connected_flag, bool* was_connected_flag);
    void onConnect(BLEServer* pServer) override;
    void onDisconnect(BLEServer* pServer) override;

private:
    bool* connected;
    bool* was_connected;
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
    AirMouseBleStatus getBleStatus() const;

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
    bool enabled;
    bool is_connected;
    bool was_connected;
    bool movement_active;
    AirMouseMode mode;
    AirMouseSensitivity sensitivity;

    // BLE HID Objects
    BLEServer* pServer;
    BLEHIDDevice* hid;
    BLECharacteristic* inputMouse;
    uint8_t buttons_state;

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
    void sendReport(uint8_t buttons, signed char x, signed char y, signed char wheel, signed char hWheel);
};

extern AirMouseManager airMouse;

#endif // AIR_MOUSE_H
