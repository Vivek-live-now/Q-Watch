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

SensorManager::SensorManager() : mpu_ok(false), mag_ok(false), last_fusion_update(0), last_mag_update(0), cal_state(MagCalState::IDLE) {
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
    prefs.begin("sensors", false);



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



void SensorManager::setImuSwapXY(bool swap) { offsets.swap_xy = swap; prefs.begin("sensors", false); prefs.putBool("swap_xy", swap); prefs.end(); }
void SensorManager::setImuInvX(bool inv) { offsets.inv_x = inv; prefs.begin("sensors", false); prefs.putBool("inv_x", inv); prefs.end(); }
void SensorManager::setImuInvY(bool inv) { offsets.inv_y = inv; prefs.begin("sensors", false); prefs.putBool("inv_y", inv); prefs.end(); }
void SensorManager::setImuInvZ(bool inv) { offsets.inv_z = inv; prefs.begin("sensors", false); prefs.putBool("inv_z", inv); prefs.end(); }

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
    offsets.accel_bias_x = (float)ax_sum / CAL_SAMPLES;
    offsets.accel_bias_y = (float)ay_sum / CAL_SAMPLES;
    offsets.accel_bias_z = ((float)az_sum / CAL_SAMPLES) - 4096.0f;

    prefs.begin("sensors", false);
    prefs.putFloat("ab_x", offsets.accel_bias_x);
    prefs.putFloat("ab_y", offsets.accel_bias_y);
    prefs.putFloat("ab_z", offsets.accel_bias_z);
    prefs.end();
}

void SensorManager::zeroLevel() {
    // Current uncompensated euler angles (raw from madgwick)
    float raw_roll  = atan2(q0*q1 + q2*q3, 0.5f - q1*q1 - q2*q2) * 57.29578f;
    float raw_pitch = asin(-2.0f * (q1*q3 - q0*q2)) * 57.29578f;

    offsets.roll_offset = raw_roll;
    offsets.pitch_offset = raw_pitch;

    prefs.begin("sensors", false);
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

    saveMagCalibration(mag_cal);
}

void SensorManager::begin() {
    loadCalibration();
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000); // 400kHz Fast mode
    Wire.setTimeOut(10);   // Strict 10ms timeout to prevent watch freezing on I2C fault
    Serial.println("Initializing Sensors...");

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
    float rx = (float)raw_data.mx;
    float ry = (float)raw_data.my;
    float rz = (float)raw_data.mz;

    // Step A: Orientation Remap
    float mapped_x, mapped_y, mapped_z;
    if (mag_cal.orientation_mode == 0) { // Default Y-Fwd, X-Left
        mapped_x = -rx; mapped_y = ry; mapped_z = -rz;
    } else if (mag_cal.orientation_mode == 1) { // X-Fwd, Y-Right
        mapped_x = -ry; mapped_y = -rx; mapped_z = -rz;
    } else if (mag_cal.orientation_mode == 2) { // Y-Back, X-Right
        mapped_x = rx; mapped_y = -ry; mapped_z = -rz;
    } else if (mag_cal.orientation_mode == 3) { // X-Back, Y-Left
        mapped_x = ry; mapped_y = rx; mapped_z = -rz;
    } else {
        mapped_x = -rx; mapped_y = ry; mapped_z = -rz;
    }

    // Step B: Z-Invert
    if (mag_cal.invert_z) {
        mapped_z = -mapped_z;
        mapped_x = -mapped_x; // Maintain right-hand rule
    }

    // Step C: Hard-Iron (Offset) & Soft-Iron (Scale)
    cal_data.mx = (mapped_x - mag_cal.hard_iron_x) * mag_cal.soft_iron_x;
    cal_data.my = (mapped_y - mag_cal.hard_iron_y) * mag_cal.soft_iron_y;
    cal_data.mz = (mapped_z - mag_cal.hard_iron_z) * mag_cal.soft_iron_z;
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

        recipNorm = 1.0f / sqrt(ax * ax + ay * ay + az * az);
        ax *= recipNorm; ay *= recipNorm; az *= recipNorm;

        recipNorm = 1.0f / sqrt(mx * mx + my * my + mz * mz);
        mx *= recipNorm; my *= recipNorm; mz *= recipNorm;

        _2q0mx = 2.0f * q0 * mx; _2q0my = 2.0f * q0 * my; _2q0mz = 2.0f * q0 * mz; _2q1mx = 2.0f * q1 * mx;
        _2q0 = 2.0f * q0; _2q1 = 2.0f * q1; _2q2 = 2.0f * q2; _2q3 = 2.0f * q3;
        _2q0q2 = 2.0f * q0 * q2; _2q2q3 = 2.0f * q2 * q3;
        q0q0 = q0 * q0; q0q1 = q0 * q1; q0q2 = q0 * q2; q0q3 = q0 * q3;
        q1q1 = q1 * q1; q1q2 = q1 * q2; q1q3 = q1 * q3; q2q2 = q2 * q2; q2q3 = q2 * q3; q3q3 = q3 * q3;

        hx = mx * q0q0 - _2q0my * q3 + _2q0mz * q2 + mx * q1q1 + _2q1 * my * q2 + _2q1 * mz * q3 - mx * q2q2 - mx * q3q3;
        hy = _2q0mx * q3 + my * q0q0 - _2q0mz * q1 + _2q1mx * q2 - my * q1q1 + my * q2q2 + _2q2 * mz * q3 - my * q3q3;
        _2bx = sqrt(hx * hx + hy * hy);
        _2bz = -_2q0mx * q2 + _2q0my * q1 + mz * q0q0 + _2q1mx * q3 - mz * q1q1 + _2q2 * my * q3 - mz * q2q2 + mz * q3q3;
        _4bx = 2.0f * _2bx; _4bz = 2.0f * _2bz;

        s0 = -_2q2 * (2.0f * q1q3 - _2q0q2 - ax) + _2q1 * (2.0f * q0q1 + _2q2q3 - ay) - _2bz * q2 * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) + (-_2bx * q3 + _2bz * q1) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + _2bx * q2 * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);
        s1 = _2q3 * (2.0f * q1q3 - _2q0q2 - ax) + _2q0 * (2.0f * q0q1 + _2q2q3 - ay) - 4.0f * q1 * (1.0f - 2.0f * q1q1 - 2.0f * q2q2 - az) + _2bz * q3 * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) + (_2bx * q2 + _2bz * q0) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + (_2bx * q3 - _4bz * q1) * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);
        s2 = -_2q0 * (2.0f * q1q3 - _2q0q2 - ax) + _2q3 * (2.0f * q0q1 + _2q2q3 - ay) - 4.0f * q2 * (1.0f - 2.0f * q1q1 - 2.0f * q2q2 - az) + (-_4bx * q2 - _2bz * q0) * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) + (_2bx * q1 + _2bz * q3) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + (_2bx * q0 - _4bz * q2) * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);
        s3 = _2q1 * (2.0f * q1q3 - _2q0q2 - ax) + _2q2 * (2.0f * q0q1 + _2q2q3 - ay) + (-_4bx * q3 + _2bz * q1) * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) + (-_2bx * q0 + _2bz * q2) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + _2bx * q1 * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);

        recipNorm = 1.0f / sqrt(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3);
        s0 *= recipNorm; s1 *= recipNorm; s2 *= recipNorm; s3 *= recipNorm;

        qDot1 -= MADGWICK_BETA * s0;
        qDot2 -= MADGWICK_BETA * s1;
        qDot3 -= MADGWICK_BETA * s2;
        qDot4 -= MADGWICK_BETA * s3;
    }

    q0 += qDot1 * dt;
    q1 += qDot2 * dt;
    q2 += qDot3 * dt;
    q3 += qDot4 * dt;

    recipNorm = 1.0f / sqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    q0 *= recipNorm;
    q1 *= recipNorm;
    q2 *= recipNorm;
    q3 *= recipNorm;
}

void SensorManager::computeEulerAngles() {
    float raw_roll  = atan2(q0*q1 + q2*q3, 0.5f - q1*q1 - q2*q2) * 57.29578f;
    float raw_pitch = asin(-2.0f * (q1*q3 - q0*q2)) * 57.29578f;

    orientation.roll = raw_roll - offsets.roll_offset;
    orientation.pitch = raw_pitch - offsets.pitch_offset;

    float yaw_math = atan2(q1*q2 + q0*q3, 0.5f - q2*q2 - q3*q3) * 57.29578f;

    orientation.yaw = 360.0f - yaw_math - 90.0f;

    // Apply Declination
    orientation.yaw += mag_cal.declination;

    // Normalize
    while (orientation.yaw < 0.0f) orientation.yaw += 360.0f;
    while (orientation.yaw >= 360.0f) orientation.yaw -= 360.0f;
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

    // Read raw to find min/max boundaries
    // Important: we map axes first so offsets align with the user's chosen coordinate frame.
    float rx = (float)raw_data.mx;
    float ry = (float)raw_data.my;
    float rz = (float)raw_data.mz;

    float mx, my, mz;
    if (mag_cal.orientation_mode == 0) { mx = -rx; my = ry; mz = -rz; }
    else if (mag_cal.orientation_mode == 1) { mx = -ry; my = -rx; mz = -rz; }
    else if (mag_cal.orientation_mode == 2) { mx = rx; my = -ry; mz = -rz; }
    else if (mag_cal.orientation_mode == 3) { mx = ry; my = rx; mz = -rz; }
    else { mx = -rx; my = ry; mz = -rz; }

    if (mag_cal.invert_z) { mz = -mz; mx = -mx; }

    if (mx < min_x) min_x = mx; if (mx > max_x) max_x = mx;
    if (my < min_y) min_y = my; if (my > max_y) max_y = my;
    if (mz < min_z) min_z = mz; if (mz > max_z) max_z = mz;

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

        applyCalibrationAndMapping();
        updateMadgwick(dt);
        computeEulerAngles();
    }
}

OrientationData SensorManager::getOrientation() const {
    return orientation;
}
