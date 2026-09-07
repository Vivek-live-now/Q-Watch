#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>

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



enum class MagCalState {
    IDLE,
    SWEEPING,
    RESULT
};

struct MagCalResult {
    bool is_good;
    bool field_ok;
    bool coverage_ok;
};

struct MagCalibration {
    uint32_t version;
    bool is_valid;
    float hard_iron_x, hard_iron_y, hard_iron_z;
    float soft_iron_x, soft_iron_y, soft_iron_z;
    int orientation_mode;
    bool invert_z;
    float declination;
    bool auto_declination;
};

struct CalibrationOffsets {
    float gyro_bias_x;
    float gyro_bias_y;
    float gyro_bias_z;
    float pitch_offset;
    float roll_offset;
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

    MagCalibration getMagCalibration() const { return mag_cal; }
    void saveMagCalibration(const MagCalibration& cal);
    void factoryResetCalibration();
    void zeroLevel();

    void startMagCalibration();
    void cancelMagCalibration();
    void updateMagCalibration();
    void completeMagCalibration();
    void saveCurrentCalibration(); // Saves the pending result

    MagCalState getCalState() const { return cal_state; }
    MagCalResult getCalResult() const { return cal_result; }
    int getCalProgress() const;


    // For Telemetry
    RawSensorData getRawData() const { return raw_data; }

    CalibratedSensorData getCalData() const { return cal_data; }
    bool isMpuOk() const { return mpu_ok; }
    bool isMagOk() const { return mag_ok; }

private:
    bool mpu_ok;
    bool mag_ok;
    uint32_t last_fusion_update;
    uint32_t last_mag_update;

    RawSensorData raw_data;
    CalibratedSensorData cal_data;
    CalibrationOffsets offsets;
    OrientationData orientation;

    MagCalibration mag_cal;

    MagCalState cal_state;
    MagCalResult cal_result;
    uint32_t cal_start_time;
    int16_t min_x, max_x, min_y, max_y, min_z, max_z;
    MagCalibration pending_cal;

    Preferences prefs;
    void loadCalibration();


    // Madgwick filter state
    float q0, q1, q2, q3;

    uint8_t readRegister(uint8_t deviceAddr, uint8_t regAddr);
    void writeRegister(uint8_t deviceAddr, uint8_t regAddr, uint8_t data);

    void readMpu();
    void readMag();
    void calibrateGyro();
    void applyCalibrationAndMapping();
    void updateMadgwick(float dt);
    void computeEulerAngles();
};

extern SensorManager sensors;

#endif
