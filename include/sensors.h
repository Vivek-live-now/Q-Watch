#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <Adafruit_BME280.h>


struct BMEHistoryEntry {
    uint32_t timestamp; // Unix timestamp or uptime in sec
    int16_t temp_x10;   // Temp * 10
    uint16_t press_x10; // Pressure * 10
    uint16_t hum_x10;   // Humidity * 10
};

enum class BmeHeightState {
    OFF,
    MEASURING,
    PAUSED
};

struct EnvironmentData {
    float temperature;
    float humidity;
    float pressure;
    float altitude; // Relative
};

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
    float accel_bias_x;
    float accel_bias_y;
    float accel_bias_z;
    float pitch_offset;
    float roll_offset;
    bool swap_xy;
    bool inv_x;
    bool inv_y;
    bool inv_z;
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
    void setPreviewMagOrientation(int mode, bool inv_z);
    void saveOrientationMode(int mode, bool inv_z);
    void revertMagOrientation();
    void factoryResetCalibration();

    void setImuSwapXY(bool swap);
    void setImuInvX(bool inv);
    void setImuInvY(bool inv);
    void setImuInvZ(bool inv);
    void setPreviewImuOrientation(bool swap_xy, bool inv_x, bool inv_y, bool inv_z);
    void saveImuOrientation(bool swap_xy, bool inv_x, bool inv_y, bool inv_z);
    void revertImuOrientation();
    bool getImuSwapXY() const { return offsets.swap_xy; }
    bool getImuInvX() const { return offsets.inv_x; }
    bool getImuInvY() const { return offsets.inv_y; }
    bool getImuInvZ() const { return offsets.inv_z; }
    void calibrateAccel();
    void zeroLevel();

    void startMagCalibration();
    void cancelMagCalibration();
    void updateMagCalibration();
    void completeMagCalibration();
    void saveCurrentCalibration(); // Saves the pending result

    MagCalState getCalState() const { return cal_state; }
    MagCalResult getCalResult() const { return cal_result; }
    int getCalProgress() const;

    // Interrupt & Deep Sleep Methods for MPU-6500
    void setupMpuInterrupt();
    void enableMotionInterruptForSleep();
    void clearMpuInterrupt();

    // For Telemetry
    RawSensorData getRawData() const { return raw_data; }
    CalibratedSensorData getCalData() const { return cal_data; }
    bool isMpuOk() const { return mpu_ok; }
    bool isMagOk() const { return mag_ok; }

    EnvironmentData getEnvData() const { return env_data; }
    void zeroAltitude();
    bool isBmeOk() const { return bme_ok; }

    // BME280 Extensions
    bool verifyBmeChip();
    void updateBmeHistory();
    void logBmeSample();
    int getBmeHistoryCount() const { return history_count; }
    bool getBmeHistory(BMEHistoryEntry* buffer, int max_entries) const;

    BmeHeightState getHeightState() const { return height_state; }
    void toggleHeightMeasurement();
    void resetHeightMeasurement();

    uint32_t getLastAltZeroTime() const { return last_alt_zero_time; }
    uint32_t getLastBmeReadingTime() const { return last_bme_update; }
    float getReferencePressure() const { return reference_pressure; }
    void resetReferencePressure();
    float getTempOffset() const { return temp_offset; }
    void setTempOffset(float offset);
    void resetBmeCalibration();


private:
    bool mpu_ok;
    bool mag_ok;
    bool bme_ok;

    Adafruit_BME280 bme;
    EnvironmentData env_data;
    float reference_pressure;
    float temp_offset;
    uint32_t last_alt_zero_time;
    uint32_t last_bme_update;
    uint32_t last_bme_log;
    BmeHeightState height_state;
    int history_count;
    static const int MAX_BME_HISTORY = 288;
    void readBme();


    uint32_t last_fusion_update;
    uint32_t last_mag_update;

    RawSensorData raw_data;
    CalibratedSensorData cal_data;
    CalibrationOffsets offsets;
    OrientationData orientation;
    bool yaw_initialized;

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
