#include <Arduino.h>
#include <Wire.h>
#include "hw_config.h"

// MPU-6500 Addresses & Registers
#define MPU6500_ADDR        0x68
#define MPU6500_WHO_AM_I    0x75
#define MPU6500_PWR_MGMT_1  0x6B
#define MPU6500_ACCEL_XOUT_H 0x3B

// Magnetometer 0x2C (Highly likely MMC5883MA) Addresses & Registers
#define MAG_ADDR            0x2C
#define MMC5883MA_PRODUCT_ID 0x2F
#define MMC5883MA_CTRL_0     0x08
#define MMC5883MA_STATUS     0x07
#define MMC5883MA_XOUT_L     0x00

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
    Serial.println("Q-WATCH RAW SENSOR DIAGNOSTIC");
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
    if (mpu_id == 0x70) {
        Serial.println("(Matches MPU-6500) -> OK");
        // Wake up MPU
        writeRegister(MPU6500_ADDR, MPU6500_PWR_MGMT_1, 0x00);
        delay(10);
        mpu_ok = true;
    } else if (mpu_id == 0x68 || mpu_id == 0x71) {
        Serial.println("(Matches MPU-6050/MPU-9250) -> OK, will proceed");
        writeRegister(MPU6500_ADDR, MPU6500_PWR_MGMT_1, 0x00);
        delay(10);
        mpu_ok = true;
    } else {
        Serial.println("(UNKNOWN DEVICE OR BUS ERROR)");
    }

    // ---------------------------------------------------------
    // 2. Magnetometer 0x2C Investigation
    // ---------------------------------------------------------
    Serial.println("\n--- Investigating Magnetometer at 0x2C ---");
    // Check if it responds
    Wire.beginTransmission(MAG_ADDR);
    if (Wire.endTransmission() == 0) {
        Serial.println("Device acknowledged at 0x2C.");

        // Attempt to read MMC5883MA Product ID register
        uint8_t mag_id = readRegister(MAG_ADDR, MMC5883MA_PRODUCT_ID);
        Serial.printf("Product ID Reg (0x2F): 0x%02X ", mag_id);

        if (mag_id == 0x0C) {
            Serial.println("(Matches MMC5883MA) -> OK");
            // Basic init for MMC5883MA
            // Write 0x08 (Set) to internal control 0 to charge capacitor
            writeRegister(MAG_ADDR, MMC5883MA_CTRL_0, 0x08);
            delay(10);
            mag_ok = true;
        } else {
            Serial.println("(UNKNOWN DEVICE)");
            Serial.println("Dumping first 10 registers for manual identification:");
            for(int i=0; i<10; i++) {
                Serial.printf("Reg 0x%02X: 0x%02X\n", i, readRegister(MAG_ADDR, i));
            }
            // We will still attempt to treat it as an MMC5883MA to see if it produces changing data
            mag_ok = true;
        }
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
            ax = (Wire.read() << 8) | Wire.read();
            ay = (Wire.read() << 8) | Wire.read();
            az = (Wire.read() << 8) | Wire.read();
            Wire.read(); Wire.read(); // Skip temperature
            gx = (Wire.read() << 8) | Wire.read();
            gy = (Wire.read() << 8) | Wire.read();
            gz = (Wire.read() << 8) | Wire.read();
        } else {
            Serial.print("[MPU ERROR: Bytes missing] ");
        }
    }

    // --- Read MMC5883MA (or unknown 0x2C mag) ---
    int16_t mx = 0, my = 0, mz = 0;

    if (mag_ok) {
        // Trigger a measurement (TM_M bit in CTRL_0)
        writeRegister(MAG_ADDR, MMC5883MA_CTRL_0, 0x01);

        // Wait for measurement to complete (Meas_M_Done bit in STATUS)
        bool ready = false;
        for (int i=0; i<10; i++) { // Max wait ~10ms
            if (readRegister(MAG_ADDR, MMC5883MA_STATUS) & 0x01) {
                ready = true;
                break;
            }
            delay(1);
        }

        if (ready) {
            Wire.beginTransmission(MAG_ADDR);
            Wire.write(MMC5883MA_XOUT_L);
            Wire.endTransmission(false);
            Wire.requestFrom(MAG_ADDR, (int)6);

            if (Wire.available() == 6) {
                // MMC5883MA registers are 16-bit, LSB first (unlike MPU which is MSB first)
                uint8_t xl = Wire.read();
                uint8_t xh = Wire.read();
                uint8_t yl = Wire.read();
                uint8_t yh = Wire.read();
                uint8_t zl = Wire.read();
                uint8_t zh = Wire.read();

                // Construct unsigned 16-bit, then offset by 32768 to make signed
                // The MMC5883MA returns 0 to 65535, where 32768 is roughly zero gauss.
                mx = (int16_t)(((uint16_t)xh << 8 | xl) - 32768);
                my = (int16_t)(((uint16_t)yh << 8 | yl) - 32768);
                mz = (int16_t)(((uint16_t)zh << 8 | zl) - 32768);
            }
        } else {
            Serial.print("[MAG ERROR: Not ready] ");
        }
    }

    // --- Output Formatting ---
    // Format: MPU_A( x, y, z ) MPU_G( x, y, z ) MAG( x, y, z )
    Serial.printf("MPU_A( %6d, %6d, %6d )  ", ax, ay, az);
    Serial.printf("MPU_G( %6d, %6d, %6d )  ", gx, gy, gz);
    Serial.printf("MAG( %6d, %6d, %6d )\n", mx, my, mz);

    // 10 Hz loop rate
    delay(100);
}
