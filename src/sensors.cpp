#include "file_manager.h"
#include "settings_data.h"
#include "sensors.h"
#include "hw_config.h"
#include <math.h>

SensorManager sensors;

// MPU-6500 Addresses & Registers
#define MPU6500_ADDR        0x68
#define MPU6500_PWR_MGMT_1  0x6B
#define MPU6500_ACCEL_XOUT_H 0x3B
#define MPU6500_GYRO_CONFIG 0x1B
#define MPU6500_ACCEL_CONFIG 0x1C

// QMC5883P Addresses & Registers (0x2C)
#define QMC5883P_ADDR       0x2C
#define QMC5883P_DATA_START 0x01
#define QMC5883P_MODE       0x0A
#define QMC5883P_CONFIG     0x0B

// Madgwick Beta (Gain)
#define MADGWICK_BETA 0.1f

SensorManager::SensorManager() : mpu_ok(false), mag_ok(false), bme_ok(false), reference_pressure(1013.25f), last_bme_update(0), last_bme_log(0), height_state(BmeHeightState::OFF), history_count(0), last_fusion_update(0), last_mag_update(0), yaw_initialized(false), cal_state(MagCalState::IDLE), last_alt_zero_time(0) {
    q0 = 1.0f; q1 = 0.0f; q2 = 0.0f; q3 = 0.0f;
    orientation.roll = 0; orientation.pitch = 0; orientation.yaw = 0;
    offsets.gyro_bias_x = 0; offsets.gyro_bias_y = 0; offsets.gyro_bias_z = 0;
    offsets.accel_bias_x = 0; offsets.accel_bias_y = 0; offsets.accel_bias_z = 0;
    offsets.pitch_offset = 0; offsets.roll_offset = 0;
    offsets.swap_xy = false; offsets.inv_x = false; offsets.inv_y = false; offsets.inv_z = false;

}

uint8_t SensorManager::readRegister(uint8_t deviceAddr, uint8_t regAddr) {
    Wire.beginTransmission(deviceAddr);
    Wire.write(regAddr);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)deviceAddr, (uint8_t)1);
    if (Wire.available()) return Wire.read();
    return 0xFF;
}

void SensorManager::writeRegister(uint8_t deviceAddr, uint8_t regAddr, uint8_t data) {
    Wire.beginTransmission(deviceAddr);
    Wire.write(regAddr);
    Wire.write(data);
    Wire.endTransmission();
}


void SensorManager::loadCalibration() {
    temp_offset = prefs.getFloat("bme_toff", 0.0f);
    reference_pressure = prefs.getFloat("bme_refp", 1013.25f);

    prefs.begin("sensors", false);

    offsets.gyro_bias_x = prefs.getFloat("gb_x", 0.0f);
    offsets.gyro_bias_y = prefs.getFloat("gb_y", 0.0f);
    offsets.gyro_bias_z = prefs.getFloat("gb_z", 0.0f);
    offsets.pitch_offset = prefs.getFloat("p_off", 0.0f);
    offsets.roll_offset = prefs.getFloat("r_off", 0.0f);
    offsets.accel_bias_x = prefs.getFloat("ab_x", 0.0f);
    offsets.accel_bias_y = prefs.getFloat("ab_y", 0.0f);
    offsets.accel_bias_z = prefs.getFloat("ab_z", 0.0f);
    offsets.swap_xy = prefs.getBool("swap_xy", false);
    offsets.inv_x = prefs.getBool("inv_x", false);
    offsets.inv_y = prefs.getBool("inv_y", false);
    offsets.inv_z = prefs.getBool("inv_z", false);

    uint32_t ver = prefs.getUInt("mag_ver", 0);
    if (ver != 1) {
        factoryResetCalibration();
    } else {
        mag_cal.version = 1;
        mag_cal.is_valid = prefs.getBool("mag_valid", false);
        mag_cal.hard_iron_x = prefs.getFloat("hi_x", 0.0f);
        mag_cal.hard_iron_y = prefs.getFloat("hi_y", 0.0f);
        mag_cal.hard_iron_z = prefs.getFloat("hi_z", 0.0f);
        mag_cal.soft_iron_x = prefs.getFloat("si_x", 1.0f);
        mag_cal.soft_iron_y = prefs.getFloat("si_y", 1.0f);
        mag_cal.soft_iron_z = prefs.getFloat("si_z", 1.0f);
        mag_cal.orientation_mode = prefs.getInt("orient", 0);
        mag_cal.invert_z = prefs.getBool("inv_z", false);
        mag_cal.declination = prefs.getFloat("decl", 0.0f);
        mag_cal.auto_declination = prefs.getBool("auto_decl", false);
    }

    prefs.end();
}

void SensorManager::saveMagCalibration(const MagCalibration& cal) {
    mag_cal = cal;
    prefs.begin("sensors", false);
    prefs.putUInt("mag_ver", mag_cal.version);
    prefs.putBool("mag_valid", mag_cal.is_valid);
    prefs.putFloat("hi_x", mag_cal.hard_iron_x);
    prefs.putFloat("hi_y", mag_cal.hard_iron_y);
    prefs.putFloat("hi_z", mag_cal.hard_iron_z);
    prefs.putFloat("si_x", mag_cal.soft_iron_x);
    prefs.putFloat("si_y", mag_cal.soft_iron_y);
    prefs.putFloat("si_z", mag_cal.soft_iron_z);
    prefs.putInt("orient", mag_cal.orientation_mode);
    prefs.putBool("inv_z", mag_cal.invert_z);
    prefs.putFloat("decl", mag_cal.declination);
    prefs.putBool("auto_decl", mag_cal.auto_declination);
    prefs.end();
}

void SensorManager::setPreviewMagOrientation(int mode, bool inv_z) {
    mag_cal.orientation_mode = mode;
    mag_cal.invert_z = inv_z;
}

void SensorManager::saveOrientationMode(int mode, bool inv_z) {
    mag_cal.orientation_mode = mode;
    mag_cal.invert_z = inv_z;
    prefs.begin("sensors", false);
    prefs.putInt("orient", mag_cal.orientation_mode);
    prefs.putBool("inv_z", mag_cal.invert_z);
    prefs.end();
}

void SensorManager::revertMagOrientation() {
    prefs.begin("sensors", true);
    mag_cal.orientation_mode = prefs.getInt("orient", 0);
    mag_cal.invert_z = prefs.getBool("inv_z", false);
    prefs.end();
}



void SensorManager::setImuSwapXY(bool swap) { offsets.swap_xy = swap; prefs.begin("sensors", false); prefs.putBool("swap_xy", swap); prefs.end(); }
void SensorManager::setImuInvX(bool inv) { offsets.inv_x = inv; prefs.begin("sensors", false); prefs.putBool("inv_x", inv); prefs.end(); }
void SensorManager::setImuInvY(bool inv) { offsets.inv_y = inv; prefs.begin("sensors", false); prefs.putBool("inv_y", inv); prefs.end(); }
void SensorManager::setImuInvZ(bool inv) { offsets.inv_z = inv; prefs.begin("sensors", false); prefs.putBool("inv_z", inv); prefs.end(); }

void SensorManager::setPreviewImuOrientation(bool swap_xy, bool inv_x, bool inv_y, bool inv_z) {
    offsets.swap_xy = swap_xy;
    offsets.inv_x = inv_x;
    offsets.inv_y = inv_y;
    offsets.inv_z = inv_z;
}

void SensorManager::saveImuOrientation(bool swap_xy, bool inv_x, bool inv_y, bool inv_z) {
    offsets.swap_xy = swap_xy;
    offsets.inv_x = inv_x;
    offsets.inv_y = inv_y;
    offsets.inv_z = inv_z;
    prefs.begin("sensors", false);
    prefs.putBool("swap_xy", offsets.swap_xy);
    prefs.putBool("inv_x", offsets.inv_x);
    prefs.putBool("inv_y", offsets.inv_y);
    prefs.putBool("inv_z", offsets.inv_z);
    prefs.end();
}

void SensorManager::revertImuOrientation() {
    prefs.begin("sensors", true);
    offsets.swap_xy = prefs.getBool("swap_xy", false);
    offsets.inv_x = prefs.getBool("inv_x", false);
    offsets.inv_y = prefs.getBool("inv_y", false);
    offsets.inv_z = prefs.getBool("inv_z", false);
    prefs.end();
}

void SensorManager::calibrateAccel() {
    Serial.println("Calibrating Accel...");
    int32_t ax_sum = 0, ay_sum = 0, az_sum = 0;
    const int CAL_SAMPLES = 200;

    for (int i = 0; i < CAL_SAMPLES; i++) {
        readMpu();
        ax_sum += raw_data.ax;
        ay_sum += raw_data.ay;
        az_sum += raw_data.az;
        delay(5);
    }

    // Z is gravity (1G), which is 4096 in 8G range
    // When watch is held flat on table face-up, expected gravity along raw Z depends on whether Z is inverted:
    // If inv_z is true, raw Z points down (-4096 LSB). If inv_z is false, raw Z points up (+4096 LSB).
    float expected_gravity_z = offsets.inv_z ? -4096.0f : 4096.0f;
    offsets.accel_bias_x = (float)ax_sum / CAL_SAMPLES;
    offsets.accel_bias_y = (float)ay_sum / CAL_SAMPLES;
    offsets.accel_bias_z = ((float)az_sum / CAL_SAMPLES) - expected_gravity_z;

    prefs.begin("sensors", false);
    prefs.putFloat("ab_x", offsets.accel_bias_x);
    prefs.putFloat("ab_y", offsets.accel_bias_y);
    prefs.putFloat("ab_z", offsets.accel_bias_z);
    prefs.end();
}

void SensorManager::zeroLevel() {
    // Re-calibrate gyro bias while watch is resting on level surface
    calibrateGyro();

    // Current uncompensated euler angles (raw from madgwick)
    // Performance Optimization: Use single-precision float math functions (atan2f/asinf) for hardware ESP32-S3 FPU acceleration
    float raw_roll  = atan2f(q0*q1 + q2*q3, 0.5f - q1*q1 - q2*q2) * 57.29578f;
    float sinp = -2.0f * (q1*q3 - q0*q2);
    if (sinp > 1.0f) sinp = 1.0f;
    else if (sinp < -1.0f) sinp = -1.0f;
    float raw_pitch = asinf(sinp) * 57.29578f;

    offsets.roll_offset = raw_roll;
    offsets.pitch_offset = raw_pitch;

    prefs.begin("sensors", false);
    prefs.putFloat("gb_x", offsets.gyro_bias_x);
    prefs.putFloat("gb_y", offsets.gyro_bias_y);
    prefs.putFloat("gb_z", offsets.gyro_bias_z);
    prefs.putFloat("p_off", offsets.pitch_offset);
    prefs.putFloat("r_off", offsets.roll_offset);
    prefs.end();
}

void SensorManager::factoryResetCalibration() {
    mag_cal.version = 1;
    mag_cal.is_valid = false;
    mag_cal.hard_iron_x = 0.0f;
    mag_cal.hard_iron_y = 0.0f;
    mag_cal.hard_iron_z = 0.0f;
    mag_cal.soft_iron_x = 1.0f;
    mag_cal.soft_iron_y = 1.0f;
    mag_cal.soft_iron_z = 1.0f;
    mag_cal.orientation_mode = 0; // 0 = default (Y-Fwd, X-Left)
    mag_cal.invert_z = false;
    mag_cal.declination = 0.0f;
    mag_cal.auto_declination = false;
    yaw_initialized = false;

    saveMagCalibration(mag_cal);
}

void SensorManager::begin() {
    loadCalibration();
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000); // 400kHz Fast mode
    Wire.setTimeOut(10);   // Strict 10ms timeout to prevent watch freezing on I2C fault
    Serial.println("Initializing Sensors...");
    // BME280 Init & Verification
    Wire.beginTransmission(0x76);
    if (Wire.endTransmission() == 0) {
        if (verifyBmeChip()) {
            if (bme.begin(0x76, &Wire)) {
                bme_ok = true;
                Serial.println("BME280 Ready at 0x76 (Chip ID: 0x60).");
                readBme();
            } else {
                Serial.println("BME280 begin failed.");
            }
        } else {
            Serial.println("BME280 Chip ID mismatch.");
        }
    } else {
        Serial.println("BME280 not detected at 0x76.");
    }

    // MPU-6500 Init
    Wire.beginTransmission(MPU6500_ADDR);
    if (Wire.endTransmission() == 0) {
        writeRegister(MPU6500_ADDR, MPU6500_PWR_MGMT_1, 0x00); // Wake
        delay(10);
        writeRegister(MPU6500_ADDR, MPU6500_GYRO_CONFIG, 0x18); // 2000dps
        writeRegister(MPU6500_ADDR, MPU6500_ACCEL_CONFIG, 0x10); // 8G
        mpu_ok = true;
        Serial.println("MPU-6500 Ready.");

        // Perform Gyro Zero-Rate Calibration
        calibrateGyro();
    }

    // QMC5883P Init
    Wire.beginTransmission(QMC5883P_ADDR);
    if (Wire.endTransmission() == 0) {
        writeRegister(QMC5883P_ADDR, QMC5883P_MODE, 0xCF);   // Continuous, 200Hz
        delay(10);
        writeRegister(QMC5883P_ADDR, QMC5883P_CONFIG, 0x08); // Set/Reset, 8G
        mag_ok = true;
        Serial.println("QMC5883P Ready.");
    }
}

void SensorManager::calibrateGyro() {
    Serial.println("Calibrating Gyro (Keep watch still)...");
    int32_t gx_sum = 0, gy_sum = 0, gz_sum = 0;
    const int CAL_SAMPLES = 200;

    for (int i = 0; i < CAL_SAMPLES; i++) {
        readMpu();
        gx_sum += raw_data.gx;
        gy_sum += raw_data.gy;
        gz_sum += raw_data.gz;
        delay(5); // 5ms * 200 = 1 second calibration routine during boot
    }

    // Store the raw bias average
    offsets.gyro_bias_x = (float)gx_sum / CAL_SAMPLES;
    offsets.gyro_bias_y = (float)gy_sum / CAL_SAMPLES;
    offsets.gyro_bias_z = (float)gz_sum / CAL_SAMPLES;

    Serial.printf("Gyro Bias -> X:%.2f Y:%.2f Z:%.2f\n", offsets.gyro_bias_x, offsets.gyro_bias_y, offsets.gyro_bias_z);
}



bool SensorManager::verifyBmeChip() {
    uint8_t chip_id = readRegister(0x76, 0xD0);
    return (chip_id == 0x60);
}

void SensorManager::readBme() {
    if (!bme_ok) return;

    env_data.temperature = bme.readTemperature() + temp_offset;
    env_data.humidity = bme.readHumidity();
    env_data.pressure = bme.readPressure() / 100.0F;

    if (height_state == BmeHeightState::MEASURING || height_state == BmeHeightState::PAUSED) {
        env_data.altitude = bme.readAltitude(reference_pressure);
    } else {
        env_data.altitude = 0.0f;
    }
}

void SensorManager::zeroAltitude() {
    if (!bme_ok) return;
    reference_pressure = bme.readPressure() / 100.0F;
    env_data.altitude = 0.0f;
    height_state = BmeHeightState::MEASURING;
    last_alt_zero_time = millis();

    prefs.begin("sensors", false);
    prefs.putFloat("bme_refp", reference_pressure);
    prefs.end();
}

void SensorManager::toggleHeightMeasurement() {
    if (height_state == BmeHeightState::OFF) {
        zeroAltitude();
    } else if (height_state == BmeHeightState::MEASURING) {
        height_state = BmeHeightState::PAUSED;
    } else if (height_state == BmeHeightState::PAUSED) {
        height_state = BmeHeightState::MEASURING;
    last_alt_zero_time = millis();
    }
}

void SensorManager::resetHeightMeasurement() {
    height_state = BmeHeightState::OFF;
    env_data.altitude = 0.0f;
}


void SensorManager::setTempOffset(float offset) {
    temp_offset = offset;
    prefs.begin("sensors", false);
    prefs.putFloat("bme_toff", temp_offset);
    prefs.end();
}

void SensorManager::resetBmeCalibration() {
    temp_offset = 0.0f;
    reference_pressure = 1013.25f;
    resetHeightMeasurement();

    prefs.begin("sensors", false);
    prefs.putFloat("bme_toff", 0.0f);
    prefs.putFloat("bme_refp", 1013.25f);
    prefs.end();
}

void SensorManager::resetReferencePressure() {
    reference_pressure = 1013.25f;
    resetHeightMeasurement();
}

void SensorManager::logBmeSample() {
    if (!bme_ok) return;

    BMEHistoryEntry entry;
    entry.timestamp = millis() / 1000;
    entry.temp_x10 = (int16_t)(env_data.temperature * 10.0f);
    entry.press_x10 = (uint16_t)(env_data.pressure * 10.0f);
    entry.hum_x10 = (uint16_t)(env_data.humidity * 10.0f);

    fileManager.append("/bme_history.bin", (const uint8_t*)&entry, sizeof(BMEHistoryEntry));

    // Limit binary history file to MAX_BME_HISTORY (288 entries * 10 bytes = 2880 bytes)
    size_t sz = fileManager.fileSize("/bme_history.bin");
    if (sz > (size_t)(MAX_BME_HISTORY * sizeof(BMEHistoryEntry))) {
        // Read buffer, trim oldest, write back
        int total = sz / sizeof(BMEHistoryEntry);
        BMEHistoryEntry* buf = new BMEHistoryEntry[total];
        fileManager.read("/bme_history.bin", (uint8_t*)buf, sz);

        int keep = MAX_BME_HISTORY;
        fileManager.write("/bme_history.bin", (const uint8_t*)&buf[total - keep], keep * sizeof(BMEHistoryEntry));
        delete[] buf;
    }

    last_bme_log = millis();
}

bool SensorManager::getBmeHistory(BMEHistoryEntry* buffer, int max_entries) const {
    if (!fileManager.exists("/bme_history.bin")) return false;
    size_t sz = fileManager.fileSize("/bme_history.bin");
    int total = sz / sizeof(BMEHistoryEntry);
    if (total <= 0) return false;

    int to_read = (total < max_entries) ? total : max_entries;
    size_t offset_bytes = (total - to_read) * sizeof(BMEHistoryEntry);

    // Read full or tail
    BMEHistoryEntry* full_buf = new BMEHistoryEntry[total];
    fileManager.read("/bme_history.bin", (uint8_t*)full_buf, sz);
    memcpy(buffer, &full_buf[total - to_read], to_read * sizeof(BMEHistoryEntry));
    delete[] full_buf;
    return true;
}

void SensorManager::updateBmeHistory() {
    if (!bme_ok) return;

    SettingsData& s = settingsManager.get();
    uint32_t intervals_ms[] = {300000, 600000, 900000, 1800000, 3600000}; // 5m, 10m, 15m, 30m, 1h
    uint32_t interval = intervals_ms[s.bme_interval_idx];

    if (last_bme_log == 0 || (millis() - last_bme_log >= interval)) {
        logBmeSample();
    }
}


void SensorManager::readMpu() {
    if (!mpu_ok) return;
    Wire.beginTransmission(MPU6500_ADDR);
    Wire.write(MPU6500_ACCEL_XOUT_H);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)MPU6500_ADDR, (uint8_t)14);

    if (Wire.available() == 14) {
        raw_data.ax = (Wire.read() << 8) | Wire.read();
        raw_data.ay = (Wire.read() << 8) | Wire.read();
        raw_data.az = (Wire.read() << 8) | Wire.read();
        Wire.read(); Wire.read(); // skip temp
        raw_data.gx = (Wire.read() << 8) | Wire.read();
        raw_data.gy = (Wire.read() << 8) | Wire.read();
        raw_data.gz = (Wire.read() << 8) | Wire.read();
    }
}

void SensorManager::readMag() {
    if (!mag_ok) return;
    Wire.beginTransmission(QMC5883P_ADDR);
    Wire.write(QMC5883P_DATA_START);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)QMC5883P_ADDR, (uint8_t)6);

    if (Wire.available() == 6) {
        uint8_t xl = Wire.read(); uint8_t xh = Wire.read();
        uint8_t yl = Wire.read(); uint8_t yh = Wire.read();
        uint8_t zl = Wire.read(); uint8_t zh = Wire.read();

        raw_data.mx = (int16_t)((uint16_t)xh << 8 | xl);
        raw_data.my = (int16_t)((uint16_t)yh << 8 | yl);
        raw_data.mz = (int16_t)((uint16_t)zh << 8 | zl);
    }
}

void SensorManager::applyCalibrationAndMapping() {
    // 1. Subtract Bias
    float raw_gx_cal = (float)raw_data.gx - offsets.gyro_bias_x;
    float raw_gy_cal = (float)raw_data.gy - offsets.gyro_bias_y;
    float raw_gz_cal = (float)raw_data.gz - offsets.gyro_bias_z;

    float raw_ax_cal = (float)raw_data.ax - offsets.accel_bias_x;
    float raw_ay_cal = (float)raw_data.ay - offsets.accel_bias_y;
    float raw_az_cal = (float)raw_data.az - offsets.accel_bias_z;

    // 2. Convert MPU raw to physical units
    float ax = raw_ax_cal / 4096.0f;
    float ay = raw_ay_cal / 4096.0f;
    float az = raw_az_cal / 4096.0f;
    float gx = raw_gx_cal * 0.001065f;
    float gy = raw_gy_cal * 0.001065f;
    float gz = raw_gz_cal * 0.001065f;

    // 3. MPU Mapping
    if (offsets.swap_xy) {
        cal_data.ax = ay; cal_data.ay = ax;
        cal_data.gx = gy; cal_data.gy = gx;
    } else {
        cal_data.ax = ax; cal_data.ay = ay;
        cal_data.gx = gx; cal_data.gy = gy;
    }
    cal_data.az = az; cal_data.gz = gz;

    if (offsets.inv_x) { cal_data.ax = -cal_data.ax; cal_data.gx = -cal_data.gx; }
    if (offsets.inv_y) { cal_data.ay = -cal_data.ay; cal_data.gy = -cal_data.gy; }
    if (offsets.inv_z) { cal_data.az = -cal_data.az; cal_data.gz = -cal_data.gz; }

    // MAG Pipeline
    // Step A: Hard-Iron (Offset) & Soft-Iron (Scale) in raw chip coordinate frame
    float cx = ((float)raw_data.mx - mag_cal.hard_iron_x) * mag_cal.soft_iron_x;
    float cy = ((float)raw_data.my - mag_cal.hard_iron_y) * mag_cal.soft_iron_y;
    float cz = ((float)raw_data.mz - mag_cal.hard_iron_z) * mag_cal.soft_iron_z;

    // Step B: Orientation Remap into watch body frame
    float mapped_x, mapped_y, mapped_z;
    if (mag_cal.orientation_mode == 0) { // Default Y-Fwd, X-Left
        mapped_x = -cx; mapped_y = cy; mapped_z = -cz;
    } else if (mag_cal.orientation_mode == 1) { // X-Fwd, Y-Right
        mapped_x = -cy; mapped_y = -cx; mapped_z = -cz;
    } else if (mag_cal.orientation_mode == 2) { // Y-Back, X-Right
        mapped_x = cx; mapped_y = -cy; mapped_z = -cz;
    } else if (mag_cal.orientation_mode == 3) { // X-Back, Y-Left
        mapped_x = cy; mapped_y = cx; mapped_z = -cz;
    } else {
        mapped_x = -cx; mapped_y = cy; mapped_z = -cz;
    }

    // Step C: Z-Invert
    if (mag_cal.invert_z) {
        mapped_z = -mapped_z;
        mapped_x = -mapped_x; // Maintain right-hand rule
    }

    cal_data.mx = mapped_x;
    cal_data.my = mapped_y;
    cal_data.mz = mapped_z;
}
void SensorManager::updateMadgwick(float dt) {
    float recipNorm;
    float s0, s1, s2, s3;
    float qDot1, qDot2, qDot3, qDot4;
    float hx, hy;
    float _2q0mx, _2q0my, _2q0mz, _2q1mx, _2bx, _2bz, _4bx, _4bz, _2q0, _2q1, _2q2, _2q3, _2q0q2, _2q2q3, q0q0, q0q1, q0q2, q0q3, q1q1, q1q2, q1q3, q2q2, q2q3, q3q3;

    float ax = cal_data.ax, ay = cal_data.ay, az = cal_data.az;
    float gx = cal_data.gx, gy = cal_data.gy, gz = cal_data.gz;
    float mx = cal_data.mx, my = cal_data.my, mz = cal_data.mz;

    if((mx == 0.0f) && (my == 0.0f) && (mz == 0.0f)) { return; }

    qDot1 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz);
    qDot2 = 0.5f * (q0 * gx + q2 * gz - q3 * gy);
    qDot3 = 0.5f * (q0 * gy - q1 * gz + q3 * gx);
    qDot4 = 0.5f * (q0 * gz + q1 * gy - q2 * gx);

    if(!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f))) {

        // Performance Optimization: Use sqrtf to execute directly on Xtensa LX7 FPU avoiding double precision emulation
        float norm_a = sqrtf(ax * ax + ay * ay + az * az);
        if (norm_a > 1e-4f) {
            recipNorm = 1.0f / norm_a;
            ax *= recipNorm; ay *= recipNorm; az *= recipNorm;
        }

        float norm_m = sqrtf(mx * mx + my * my + mz * mz);
        if (norm_m > 1e-4f) {
            recipNorm = 1.0f / norm_m;
            mx *= recipNorm; my *= recipNorm; mz *= recipNorm;
        }

        _2q0mx = 2.0f * q0 * mx; _2q0my = 2.0f * q0 * my; _2q0mz = 2.0f * q0 * mz; _2q1mx = 2.0f * q1 * mx;
        _2q0 = 2.0f * q0; _2q1 = 2.0f * q1; _2q2 = 2.0f * q2; _2q3 = 2.0f * q3;
        _2q0q2 = 2.0f * q0 * q2; _2q2q3 = 2.0f * q2 * q3;
        q0q0 = q0 * q0; q0q1 = q0 * q1; q0q2 = q0 * q2; q0q3 = q0 * q3;
        q1q1 = q1 * q1; q1q2 = q1 * q2; q1q3 = q1 * q3; q2q2 = q2 * q2; q2q3 = q2 * q3; q3q3 = q3 * q3;

        hx = mx * q0q0 - _2q0my * q3 + _2q0mz * q2 + mx * q1q1 + _2q1 * my * q2 + _2q1 * mz * q3 - mx * q2q2 - mx * q3q3;
        hy = _2q0mx * q3 + my * q0q0 - _2q0mz * q1 + _2q1mx * q2 - my * q1q1 + my * q2q2 + _2q2 * mz * q3 - my * q3q3;
        _2bx = sqrtf(hx * hx + hy * hy);
        _2bz = -_2q0mx * q2 + _2q0my * q1 + mz * q0q0 + _2q1mx * q3 - mz * q1q1 + _2q2 * my * q3 - mz * q2q2 + mz * q3q3;
        _4bx = 2.0f * _2bx; _4bz = 2.0f * _2bz;

        s0 = -_2q2 * (2.0f * q1q3 - _2q0q2 - ax) + _2q1 * (2.0f * q0q1 + _2q2q3 - ay) - _2bz * q2 * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) + (-_2bx * q3 + _2bz * q1) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + _2bx * q2 * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);
        s1 = _2q3 * (2.0f * q1q3 - _2q0q2 - ax) + _2q0 * (2.0f * q0q1 + _2q2q3 - ay) - 4.0f * q1 * (1.0f - 2.0f * q1q1 - 2.0f * q2q2 - az) + _2bz * q3 * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) + (_2bx * q2 + _2bz * q0) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + (_2bx * q3 - _4bz * q1) * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);
        s2 = -_2q0 * (2.0f * q1q3 - _2q0q2 - ax) + _2q3 * (2.0f * q0q1 + _2q2q3 - ay) - 4.0f * q2 * (1.0f - 2.0f * q1q1 - 2.0f * q2q2 - az) + (-_4bx * q2 - _2bz * q0) * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) + (_2bx * q1 + _2bz * q3) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + (_2bx * q0 - _4bz * q2) * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);
        s3 = _2q1 * (2.0f * q1q3 - _2q0q2 - ax) + _2q2 * (2.0f * q0q1 + _2q2q3 - ay) + (-_4bx * q3 + _2bz * q1) * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) + (-_2bx * q0 + _2bz * q2) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + _2bx * q1 * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);

        float norm_s = sqrtf(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3);
        if (norm_s > 1e-4f) {
            recipNorm = 1.0f / norm_s;
            s0 *= recipNorm; s1 *= recipNorm; s2 *= recipNorm; s3 *= recipNorm;

            qDot1 -= MADGWICK_BETA * s0;
            qDot2 -= MADGWICK_BETA * s1;
            qDot3 -= MADGWICK_BETA * s2;
            qDot4 -= MADGWICK_BETA * s3;
        }
    }

    q0 += qDot1 * dt;
    q1 += qDot2 * dt;
    q2 += qDot3 * dt;
    q3 += qDot4 * dt;

    float norm_q = sqrtf(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    if (norm_q > 1e-4f) {
        recipNorm = 1.0f / norm_q;
        q0 *= recipNorm;
        q1 *= recipNorm;
        q2 *= recipNorm;
        q3 *= recipNorm;
    }
}

void SensorManager::computeEulerAngles() {
    // Performance Optimization: Use single-precision atan2f/asinf for direct hardware FPU execution
    float raw_roll  = atan2f(q0*q1 + q2*q3, 0.5f - q1*q1 - q2*q2) * 57.29578f;
    float sinp = -2.0f * (q1*q3 - q0*q2);
    if (sinp > 1.0f) sinp = 1.0f;
    else if (sinp < -1.0f) sinp = -1.0f;
    float raw_pitch = asinf(sinp) * 57.29578f;

    orientation.roll = raw_roll - offsets.roll_offset;
    orientation.pitch = raw_pitch - offsets.pitch_offset;

    float yaw_math = atan2f(q1*q2 + q0*q3, 0.5f - q2*q2 - q3*q3) * 57.29578f;

    float target_yaw = 360.0f - yaw_math - 90.0f;

    // Apply Declination
    target_yaw += mag_cal.declination;

    // Normalize target to [0, 360)
    while (target_yaw < 0.0f) target_yaw += 360.0f;
    while (target_yaw >= 360.0f) target_yaw -= 360.0f;

    // Circular exponential smoothing (alpha = 0.25) across 360-degree wrap-around
    if (!yaw_initialized) {
        orientation.yaw = target_yaw;
        yaw_initialized = true;
    } else {
        float diff = target_yaw - orientation.yaw;
        while (diff < -180.0f) diff += 360.0f;
        while (diff > 180.0f) diff -= 360.0f;
        orientation.yaw += 0.25f * diff;
        while (orientation.yaw < 0.0f) orientation.yaw += 360.0f;
        while (orientation.yaw >= 360.0f) orientation.yaw -= 360.0f;
    }
}

void SensorManager::startMagCalibration() {
    cal_state = MagCalState::SWEEPING;
    cal_start_time = millis();
    min_x = 32767; max_x = -32768;
    min_y = 32767; max_y = -32768;
    min_z = 32767; max_z = -32768;
}

void SensorManager::cancelMagCalibration() {
    cal_state = MagCalState::IDLE;
}

int SensorManager::getCalProgress() const {
    if (cal_state != MagCalState::SWEEPING) return 0;
    uint32_t elapsed = millis() - cal_start_time;
    int pct = (elapsed * 100) / 15000;
    return (pct > 100) ? 100 : pct;
}

void SensorManager::updateMagCalibration() {
    if (cal_state != MagCalState::SWEEPING) return;

    // Track min/max boundaries directly in raw sensor coordinate frame
    // Decoupled from 3D orientation presets so changing presets never corrupts hard-iron calibration
    float rx = (float)raw_data.mx;
    float ry = (float)raw_data.my;
    float rz = (float)raw_data.mz;

    if (rx < min_x) min_x = rx; if (rx > max_x) max_x = rx;
    if (ry < min_y) min_y = ry; if (ry > max_y) max_y = ry;
    if (rz < min_z) min_z = rz; if (rz > max_z) max_z = rz;

    if (millis() - cal_start_time >= 15000) {
        completeMagCalibration();
    }
}

void SensorManager::completeMagCalibration() {
    cal_state = MagCalState::RESULT;

    float hx = (max_x + min_x) / 2.0f;
    float hy = (max_y + min_y) / 2.0f;
    float hz = (max_z + min_z) / 2.0f;

    float diff_x = max_x - min_x;
    float diff_y = max_y - min_y;
    float diff_z = max_z - min_z;

    float avg_delta = (diff_x + diff_y + diff_z) / 3.0f;

    float sx = diff_x > 0 ? (avg_delta / diff_x) : 1.0f;
    float sy = diff_y > 0 ? (avg_delta / diff_y) : 1.0f;
    float sz = diff_z > 0 ? (avg_delta / diff_z) : 1.0f;

    pending_cal = mag_cal;
    pending_cal.hard_iron_x = hx;
    pending_cal.hard_iron_y = hy;
    pending_cal.hard_iron_z = hz;
    pending_cal.soft_iron_x = sx;
    pending_cal.soft_iron_y = sy;
    pending_cal.soft_iron_z = sz;
    pending_cal.is_valid = true;

    // Quality Checks
    // Coverage: We expect the user to rotate it well, so min-max difference should be > 500 units on all axes
    cal_result.coverage_ok = (diff_x > 500 && diff_y > 500 && diff_z > 500);

    // Field: We don't expect crazy spikes (e.g., > 10000 which indicates magnets too close)
    cal_result.field_ok = (diff_x < 10000 && diff_y < 10000 && diff_z < 10000);

    cal_result.is_good = cal_result.coverage_ok && cal_result.field_ok;
}

void SensorManager::saveCurrentCalibration() {
    if (cal_state == MagCalState::RESULT) {
        saveMagCalibration(pending_cal);
        cal_state = MagCalState::IDLE;
    }
}

void SensorManager::loop() {
    uint32_t now = millis();

    if (now - last_fusion_update >= 10) {
        float dt = (now - last_fusion_update) / 1000.0f;
        last_fusion_update = now;

        readMpu();

        if (now - last_mag_update >= 20) {
            readMag();
            updateMagCalibration();
            last_mag_update = now;
        }

        if (now - last_bme_update >= 1000) {
            readBme();
            updateBmeHistory();
            last_bme_update = now;
        }

        applyCalibrationAndMapping();
        updateMadgwick(dt);
        computeEulerAngles();
    }
}

OrientationData SensorManager::getOrientation() const {
    return orientation;
}

void SensorManager::setupMpuInterrupt() {
    if (!mpu_ok) return;
    // Configure INT pin: Active Low (bit 7 = 1), Open Drain (bit 6 = 1), Latch until cleared (bit 5 = 1), Clear on any read (bit 4 = 1)
    writeRegister(MPU6500_ADDR, 0x37, 0xF0);
}

void SensorManager::enableMotionInterruptForSleep() {
    if (!mpu_ok) return;
    // Set INT pin active low open-drain latched
    writeRegister(MPU6500_ADDR, 0x37, 0xF0);

    // Set motion threshold (WOM_THR 0x1F)
    writeRegister(MPU6500_ADDR, 0x1F, 0x20); // ~62.5mg threshold

    // Enable Accel hardware intelligence control (ACCEL_INTEL_CTRL 0x69)
    writeRegister(MPU6500_ADDR, 0x69, 0xC0); // Enable WOM logic and compare with previous sample

    // Enable WOM interrupt in INT_ENABLE (0x38)
    writeRegister(MPU6500_ADDR, 0x38, 0x40); // Bit 6 = WOM_EN
}

void SensorManager::clearMpuInterrupt() {
    if (!mpu_ok) return;
    // Read INT_STATUS register (0x3A) to clear interrupt
    readRegister(MPU6500_ADDR, 0x3A);
}
