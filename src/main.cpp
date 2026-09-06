#include <Arduino.h>
#include <Wire.h>
#include "hw_config.h"

// MPU-6500 Addresses & Registers
#define MPU6500_ADDR        0x68
#define MPU6500_WHO_AM_I    0x75
#define MPU6500_PWR_MGMT_1  0x6B
#define MPU6500_ACCEL_XOUT_H 0x3B

// QMC5883P Addresses & Registers (0x2C)
#define QMC5883P_ADDR       0x2C
#define QMC5883P_DATA_START 0x01
#define QMC5883P_STATUS     0x09
#define QMC5883P_MODE       0x0A
#define QMC5883P_CONFIG     0x0B

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
    Serial.println("Q-WATCH RAW SENSOR DIAGNOSTIC (QMC5883P)");
    Serial.println("========================================");

    // Initialize I2C at 100kHz
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
    // 2. Magnetometer (QMC5883P) at 0x2C
    // ---------------------------------------------------------
    Serial.println("\n--- Checking QMC5883P at 0x2C ---");
    Wire.beginTransmission(QMC5883P_ADDR);
    if (Wire.endTransmission() == 0) {
        Serial.println("Device acknowledged at 0x2C.");

        // Setup based on QMC5883P specific datasheet
        // 0xCF to Mode Reg 0x0A: Continuous mode, 200Hz data output rate
        writeRegister(QMC5883P_ADDR, QMC5883P_MODE, 0xCF);
        delay(10);

        // 0x08 to Config Reg 0x0B: Set/Reset mode ON, Range +/- 8G
        writeRegister(QMC5883P_ADDR, QMC5883P_CONFIG, 0x08);
        delay(10);

        mag_ok = true;
        Serial.println("QMC5883P Initialized.");
    } else {
        Serial.println("Device NOT responding at 0x2C!");
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
            // MPU is MSB first
            ax = (Wire.read() << 8) | Wire.read();
            ay = (Wire.read() << 8) | Wire.read();
            az = (Wire.read() << 8) | Wire.read();
            Wire.read(); Wire.read(); // Skip temp
            gx = (Wire.read() << 8) | Wire.read();
            gy = (Wire.read() << 8) | Wire.read();
            gz = (Wire.read() << 8) | Wire.read();
        }
    }

    // --- Read QMC5883P ---
    int16_t mx = 0, my = 0, mz = 0;
    bool mag_read_success = false;

    if (mag_ok) {
        // Read 6 bytes starting from register 0x01
        Wire.beginTransmission(QMC5883P_ADDR);
        Wire.write(QMC5883P_DATA_START);
        Wire.endTransmission(false);
        Wire.requestFrom((uint8_t)QMC5883P_ADDR, (uint8_t)6);

        if (Wire.available() == 6) {
            // QMC5883P is LSB first
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
