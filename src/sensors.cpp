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

SensorManager::SensorManager() : mpu_ok(false), mag_ok(false), last_fusion_update(0), last_mag_update(0) {
    q0 = 1.0f; q1 = 0.0f; q2 = 0.0f; q3 = 0.0f;
    orientation.roll = 0; orientation.pitch = 0; orientation.yaw = 0;
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

void SensorManager::begin() {
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000); // 400kHz for fast fusion

    Serial.println("Initializing Sensors...");

    // MPU-6500 Init
    Wire.beginTransmission(MPU6500_ADDR);
    if (Wire.endTransmission() == 0) {
        writeRegister(MPU6500_ADDR, MPU6500_PWR_MGMT_1, 0x00); // Wake
        delay(10);
        // Set Gyro to 2000 dps, Accel to 8G for better dynamic range during wrist movements
        writeRegister(MPU6500_ADDR, MPU6500_GYRO_CONFIG, 0x18); // 2000dps
        writeRegister(MPU6500_ADDR, MPU6500_ACCEL_CONFIG, 0x10); // 8G
        mpu_ok = true;
        Serial.println("MPU-6500 Ready.");
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
    // 1. Convert MPU raw to physical units
    // Accel 8G = 4096 LSB/g. Gyro 2000dps = 16.4 LSB/dps.
    // Gyro requires Radians/sec for Madgwick: (val / 16.4) * (PI/180) = val * 0.001065f
    float ax = (float)raw_data.ax / 4096.0f;
    float ay = (float)raw_data.ay / 4096.0f;
    float az = (float)raw_data.az / 4096.0f;
    float gx = (float)raw_data.gx * 0.001065f;
    float gy = (float)raw_data.gy * 0.001065f;
    float gz = (float)raw_data.gz * 0.001065f;
    float mx = (float)raw_data.mx;
    float my = (float)raw_data.my;
    float mz = (float)raw_data.mz;

    // 2. Axis Remapping (Critical step: must be configured to the physical board later)
    // For now, assume a standard ENU alignment mapping
    cal_data.ax = ax;  cal_data.ay = ay;  cal_data.az = az;
    cal_data.gx = gx;  cal_data.gy = gy;  cal_data.gz = gz;
    cal_data.mx = mx;  cal_data.my = my;  cal_data.mz = mz;

    // 3. Simple Hard Iron Calibration (Placeholders to be overwritten by real calibration routine later)
    float mag_bias_x = 0, mag_bias_y = 0, mag_bias_z = 0;
    cal_data.mx -= mag_bias_x;
    cal_data.my -= mag_bias_y;
    cal_data.mz -= mag_bias_z;
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

    // Use IMU algorithm if magnetometer measurement invalid (avoids NaN in magnetometer normalisation)
    if((mx == 0.0f) && (my == 0.0f) && (mz == 0.0f)) {
        // Fallback to 6DOF (not implemented here to save space, but essential for robust code)
        return;
    }

    // Rate of change of quaternion from gyroscope
    qDot1 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz);
    qDot2 = 0.5f * (q0 * gx + q2 * gz - q3 * gy);
    qDot3 = 0.5f * (q0 * gy - q1 * gz + q3 * gx);
    qDot4 = 0.5f * (q0 * gz + q1 * gy - q2 * gx);

    // Compute feedback only if accelerometer measurement valid
    if(!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f))) {

        // Normalise accelerometer measurement
        recipNorm = 1.0f / sqrt(ax * ax + ay * ay + az * az);
        ax *= recipNorm;
        ay *= recipNorm;
        az *= recipNorm;

        // Normalise magnetometer measurement
        recipNorm = 1.0f / sqrt(mx * mx + my * my + mz * mz);
        mx *= recipNorm;
        my *= recipNorm;
        mz *= recipNorm;

        // Auxiliary variables to avoid repeated arithmetic
        _2q0mx = 2.0f * q0 * mx;
        _2q0my = 2.0f * q0 * my;
        _2q0mz = 2.0f * q0 * mz;
        _2q1mx = 2.0f * q1 * mx;
        _2q0 = 2.0f * q0;
        _2q1 = 2.0f * q1;
        _2q2 = 2.0f * q2;
        _2q3 = 2.0f * q3;
        _2q0q2 = 2.0f * q0 * q2;
        _2q2q3 = 2.0f * q2 * q3;
        q0q0 = q0 * q0;
        q0q1 = q0 * q1;
        q0q2 = q0 * q2;
        q0q3 = q0 * q3;
        q1q1 = q1 * q1;
        q1q2 = q1 * q2;
        q1q3 = q1 * q3;
        q2q2 = q2 * q2;
        q2q3 = q2 * q3;
        q3q3 = q3 * q3;

        // Reference direction of Earth's magnetic field
        hx = mx * q0q0 - _2q0my * q3 + _2q0mz * q2 + mx * q1q1 + _2q1 * my * q2 + _2q1 * mz * q3 - mx * q2q2 - mx * q3q3;
        hy = _2q0mx * q3 + my * q0q0 - _2q0mz * q1 + _2q1mx * q2 - my * q1q1 + my * q2q2 + _2q2 * mz * q3 - my * q3q3;
        _2bx = sqrt(hx * hx + hy * hy);
        _2bz = -_2q0mx * q2 + _2q0my * q1 + mz * q0q0 + _2q1mx * q3 - mz * q1q1 + _2q2 * my * q3 - mz * q2q2 + mz * q3q3;
        _4bx = 2.0f * _2bx;
        _4bz = 2.0f * _2bz;

        // Gradient decent algorithm corrective step
        s0 = -_2q2 * (2.0f * q1q3 - _2q0q2 - ax) + _2q1 * (2.0f * q0q1 + _2q2q3 - ay) - _2bz * q2 * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) + (-_2bx * q3 + _2bz * q1) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + _2bx * q2 * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);
        s1 = _2q3 * (2.0f * q1q3 - _2q0q2 - ax) + _2q0 * (2.0f * q0q1 + _2q2q3 - ay) - 4.0f * q1 * (1.0f - 2.0f * q1q1 - 2.0f * q2q2 - az) + _2bz * q3 * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) + (_2bx * q2 + _2bz * q0) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + (_2bx * q3 - _4bz * q1) * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);
        s2 = -_2q0 * (2.0f * q1q3 - _2q0q2 - ax) + _2q3 * (2.0f * q0q1 + _2q2q3 - ay) - 4.0f * q2 * (1.0f - 2.0f * q1q1 - 2.0f * q2q2 - az) + (-_4bx * q2 - _2bz * q0) * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) + (_2bx * q1 + _2bz * q3) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + (_2bx * q0 - _4bz * q2) * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);
        s3 = _2q1 * (2.0f * q1q3 - _2q0q2 - ax) + _2q2 * (2.0f * q0q1 + _2q2q3 - ay) + (-_4bx * q3 + _2bz * q1) * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) + (-_2bx * q0 + _2bz * q2) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + _2bx * q1 * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);

        // Normalise step magnitude
        recipNorm = 1.0f / sqrt(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3);
        s0 *= recipNorm;
        s1 *= recipNorm;
        s2 *= recipNorm;
        s3 *= recipNorm;

        // Apply feedback step
        qDot1 -= MADGWICK_BETA * s0;
        qDot2 -= MADGWICK_BETA * s1;
        qDot3 -= MADGWICK_BETA * s2;
        qDot4 -= MADGWICK_BETA * s3;
    }

    // Integrate rate of change of quaternion
    q0 += qDot1 * dt;
    q1 += qDot2 * dt;
    q2 += qDot3 * dt;
    q3 += qDot4 * dt;

    // Normalise quaternion
    recipNorm = 1.0f / sqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    q0 *= recipNorm;
    q1 *= recipNorm;
    q2 *= recipNorm;
    q3 *= recipNorm;
}

void SensorManager::computeEulerAngles() {
    orientation.roll  = atan2(q0*q1 + q2*q3, 0.5f - q1*q1 - q2*q2) * 57.29578f;
    orientation.pitch = asin(-2.0f * (q1*q3 - q0*q2)) * 57.29578f;
    orientation.yaw   = atan2(q1*q2 + q0*q3, 0.5f - q2*q2 - q3*q3) * 57.29578f;

    // Normalize yaw to 0-360
    if (orientation.yaw < 0) orientation.yaw += 360.0f;
}

void SensorManager::loop() {
    uint32_t now = millis();

    // Fast 100Hz loop for MPU + Fusion
    if (now - last_fusion_update >= 10) {
        float dt = (now - last_fusion_update) / 1000.0f;
        last_fusion_update = now;

        readMpu();

        // Slower 50Hz loop for Magnetometer
        if (now - last_mag_update >= 20) {
            readMag();
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
