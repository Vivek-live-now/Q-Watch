#include <Arduino.h>
#include <Wire.h>
#include "hw_config.h"

// MPU-6500 Addresses & Registers
#define MPU6500_ADDR        0x68
#define MPU6500_WHO_AM_I    0x75
#define MPU6500_PWR_MGMT_1  0x6B
#define MPU6500_ACCEL_XOUT_H 0x3B

// QMC5883L (Clone variant at 0x2C) Addresses & Registers
#define QMC5883L_ADDR       0x2C
#define QMC5883L_DATA_START 0x00
#define QMC5883L_STATUS     0x06
#define QMC5883L_CTRL_1     0x09
#define QMC5883L_SET_RESET  0x0B

bool mpu_ok = false;
bool mag_ok = false;

// Helper to read a single register
uint8_t readRegister(uint8_t deviceAddr, uint8_t regAddr) {
    Wire.beginTransmission(deviceAddr);
    Wire.write(regAddr);
    Wire.endTransmission(false);
    Wire.requestFrom(deviceAddr, (uint8_t)1);
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
    Serial.println("Q-WATCH RAW SENSOR DIAGNOSTIC V2");
    Serial.println("========================================");

    // Initialize I2C at 100kHz as requested
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(100000);

    // ---------------------------------------------------------
    // 1. MPU-6500 Initialization
    // ---------------------------------------------------------
    Serial.println("\n--- Checking MPU-6500 at 0x68 ---");
    uint8_t mpu_id = readRegister(MPU6500_ADDR, MPU6500_WHO_AM_I);
    Serial.printf("WHO_AM_I: 0x%02X ", mpu_id);
    if (mpu_id == 0x70 || mpu_id == 0x68 || mpu_id == 0x71) {
        Serial.println(" -> OK");
        writeRegister(MPU6500_ADDR, MPU6500_PWR_MGMT_1, 0x00); // Wake up
        delay(10);
        mpu_ok = true;
    } else {
        Serial.println("(UNKNOWN DEVICE OR BUS ERROR)");
    }

    // ---------------------------------------------------------
    // 2. QMC5883L Magnetometer at 0x2C Investigation
    // ---------------------------------------------------------
    Serial.println("\n--- Checking QMC5883L at 0x2C ---");
    Wire.beginTransmission(QMC5883L_ADDR);
    if (Wire.endTransmission() == 0) {
        Serial.println("Device acknowledged at 0x2C.");

        // QMC5883L Initialization
        // 1. Write 0x01 to SET/RESET Period register (0x0B)
        writeRegister(QMC5883L_ADDR, QMC5883L_SET_RESET, 0x01);
        delay(10);

        // 2. Write to Control Register 1 (0x09)
        // 0x1D = Continuous Mode, ODR 200Hz, RNG 8G, OSR 512
        writeRegister(QMC5883L_ADDR, QMC5883L_CTRL_1, 0x1D);
        delay(10);

        mag_ok = true;
        Serial.println("QMC5883L Initialized.");
    } else {
        Serial.println("Device NOT responding at 0x2C!");
    }

    Serial.println("\nStarting 10Hz raw data stream...\n");
}

void loop() {
    if (!mpu_ok && !mag_ok) {
        Serial.println("Both sensors failed to initialize. Check wiring.");
        delay(1000);
        return;
    }

    // --- Read MPU-6500 ---
    int16_t ax = 0, ay = 0, az = 0;
    int16_t gx = 0, gy = 0, gz = 0;

    if (mpu_ok) {
        Wire.beginTransmission(MPU6500_ADDR);
        Wire.write(MPU6500_ACCEL_XOUT_H);
        Wire.endTransmission(false);
        Wire.requestFrom(MPU6500_ADDR, (int)14);

        if (Wire.available() == 14) {
            // MPU is MSB first
            ax = (Wire.read() << 8) | Wire.read();
            ay = (Wire.read() << 8) | Wire.read();
            az = (Wire.read() << 8) | Wire.read();
            Wire.read(); Wire.read(); // Skip temp
            gx = (Wire.read() << 8) | Wire.read();
            gy = (Wire.read() << 8) | Wire.read();
            gz = (Wire.read() << 8) | Wire.read();
        } else {
            Serial.print("[MPU ERROR] ");
        }
    }

    // --- Read QMC5883L ---
    int16_t mx = 0, my = 0, mz = 0;

    if (mag_ok) {
        // Read Status Register
        uint8_t status = readRegister(QMC5883L_ADDR, QMC5883L_STATUS);

        if (status & 0x01) { // Data Ready bit
            Wire.beginTransmission(QMC5883L_ADDR);
            Wire.write(QMC5883L_DATA_START);
            Wire.endTransmission(false);
            Wire.requestFrom(QMC5883L_ADDR, (int)6);

            if (Wire.available() == 6) {
                // QMC5883L is LSB first
                uint8_t xl = Wire.read();
                uint8_t xh = Wire.read();
                uint8_t yl = Wire.read();
                uint8_t yh = Wire.read();
                uint8_t zl = Wire.read();
                uint8_t zh = Wire.read();

                mx = (int16_t)((uint16_t)xh << 8 | xl);
                my = (int16_t)((uint16_t)yh << 8 | yl);
                mz = (int16_t)((uint16_t)zh << 8 | zl);
            }
        } else {
            Serial.print("[MAG: Waiting for DRDY] ");
        }
    }

    // --- Output Formatting ---
    Serial.printf("MPU_A( %6d, %6d, %6d )  ", ax, ay, az);
    Serial.printf("MPU_G( %6d, %6d, %6d )  ", gx, gy, gz);
    Serial.printf("MAG( %6d, %6d, %6d )\n", mx, my, mz);

    delay(100);
}
