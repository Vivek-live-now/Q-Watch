#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include <Wire.h>

struct RawSensorData {
    int16_t ax, ay, az;
    int16_t gx, gy, gz;
    int16_t mx, my, mz;
};

struct CalibratedSensorData {
    float ax, ay, az; // in g
    float gx, gy, gz; // in deg/s
    float mx, my, mz; // in uT (or arbitrary normalized units)
};

struct OrientationData {
    float roll;
    float pitch;
    float yaw; // Heading
};

class SensorManager {
public:
    SensorManager();
    void begin();
    void loop();

    OrientationData getOrientation() const;
    bool isMpuOk() const { return mpu_ok; }
    bool isMagOk() const { return mag_ok; }

private:
    bool mpu_ok;
    bool mag_ok;
    uint32_t last_fusion_update;
    uint32_t last_mag_update;

    RawSensorData raw_data;
    CalibratedSensorData cal_data;
    OrientationData orientation;

    // Madgwick filter state
    float q0, q1, q2, q3;

    uint8_t readRegister(uint8_t deviceAddr, uint8_t regAddr);
    void writeRegister(uint8_t deviceAddr, uint8_t regAddr, uint8_t data);

    void readMpu();
    void readMag();
    void applyCalibrationAndMapping();
    void updateMadgwick(float dt);
    void computeEulerAngles();
};

extern SensorManager sensors;

#endif
