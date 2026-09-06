#include <Arduino.h>
#include <Wire.h>
#include "hw_config.h"

// MPU-6500 Addresses & Registers
#define MPU6500_ADDR        0x68
#define MPU6500_WHO_AM_I    0x75
#define MPU6500_PWR_MGMT_1  0x6B
#define MPU6500_ACCEL_XOUT_H 0x3B

// HP5883 / QMC5883L Clone at 0x2C
#define MAG_ADDR            0x2C
#define MAG_DATA_START      0x00
#define MAG_CTRL_1          0x09
#define MAG_SET_RESET       0x0B

bool mpu_ok = false;
bool mag_ok = false;

// Helper to read a single register
uint8_t readRegister(uint8_t deviceAddr, uint8_t regAddr) {
    Wire.beginTransmission(deviceAddr);
    Wire.write(regAddr);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)deviceAddr, (uint8_t)1);
    if (Wire.available()) {
        return Wire.read();
    }
    return 0xFF;
}

// Helper to write a single register
void writeRegister(uint8_t deviceAddr, uint8_t regAddr, uint8_t data) {
    Wire.beginTransmission(deviceAddr);
    Wire.write(regAddr);
    Wire.write(data);
    Wire.endTransmission();
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n\n========================================");
    Serial.println("Q-WATCH RAW SENSOR DIAGNOSTIC V3 (HP5883)");
    Serial.println("========================================");

    // Initialize I2C at 100kHz
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(100000);

    // ---------------------------------------------------------
    // 1. MPU-6500 Initialization
    // ---------------------------------------------------------
    uint8_t mpu_id = readRegister(MPU6500_ADDR, MPU6500_WHO_AM_I);
    if (mpu_id == 0x70 || mpu_id == 0x68 || mpu_id == 0x71) {
        writeRegister(MPU6500_ADDR, MPU6500_PWR_MGMT_1, 0x00); // Wake up
        delay(10);
        mpu_ok = true;
    }

    // ---------------------------------------------------------
    // 2. Magnetometer (HP5883) at 0x2C
    // ---------------------------------------------------------
    Wire.beginTransmission(MAG_ADDR);
    if (Wire.endTransmission() == 0) {
        // Initialization sequence for HP5883 / QMC clones
        // 1. Write 0x01 to SET/RESET Period register (0x0B)
        writeRegister(MAG_ADDR, MAG_SET_RESET, 0x01);
        delay(10);

        // 2. Write 0x1D to Control 1 (0x09) -> Continuous, 200Hz, 8G
        writeRegister(MAG_ADDR, MAG_CTRL_1, 0x1D);
        delay(10);

        mag_ok = true;
    }

    Serial.println("\nStarting 10Hz raw data stream...\n");
}

void loop() {
    // --- Read MPU-6500 ---
    int16_t ax = 0, ay = 0, az = 0;
    int16_t gx = 0, gy = 0, gz = 0;

    if (mpu_ok) {
        Wire.beginTransmission(MPU6500_ADDR);
        Wire.write(MPU6500_ACCEL_XOUT_H);
        Wire.endTransmission(false);
        Wire.requestFrom((uint8_t)MPU6500_ADDR, (uint8_t)14);

        if (Wire.available() == 14) {
            ax = (Wire.read() << 8) | Wire.read();
            ay = (Wire.read() << 8) | Wire.read();
            az = (Wire.read() << 8) | Wire.read();
            Wire.read(); Wire.read(); // Skip temp
            gx = (Wire.read() << 8) | Wire.read();
            gy = (Wire.read() << 8) | Wire.read();
            gz = (Wire.read() << 8) | Wire.read();
        }
    }

    // --- Read HP5883 / 0x2C ---
    int16_t mx = 0, my = 0, mz = 0;
    bool mag_read_success = false;

    if (mag_ok) {
        // For the HP5883 clone, we are bypassing the STATUS register check
        // entirely because it often behaves unreliably. We will just "blind read"
        // the 6 data bytes directly. Since we set it to Continuous 200Hz mode,
        // the data registers should constantly be updating in the background.

        Wire.beginTransmission(MAG_ADDR);
        Wire.write(MAG_DATA_START);
        Wire.endTransmission(false);
        Wire.requestFrom((uint8_t)MAG_ADDR, (uint8_t)6);

        if (Wire.available() == 6) {
            uint8_t xl = Wire.read();
            uint8_t xh = Wire.read();
            uint8_t yl = Wire.read();
            uint8_t yh = Wire.read();
            uint8_t zl = Wire.read();
            uint8_t zh = Wire.read();

            mx = (int16_t)((uint16_t)xh << 8 | xl);
            my = (int16_t)((uint16_t)yh << 8 | yl);
            mz = (int16_t)((uint16_t)zh << 8 | zl);

            mag_read_success = true;
        }
    }

    // --- Output Formatting ---
    Serial.printf("MPU_A( %6d, %6d, %6d )  ", ax, ay, az);
    Serial.printf("MPU_G( %6d, %6d, %6d )  ", gx, gy, gz);

    if (mag_read_success) {
        Serial.printf("MAG( %6d, %6d, %6d )\n", mx, my, mz);
    } else {
        Serial.println("MAG( ERROR )");
    }

    delay(100);
}
