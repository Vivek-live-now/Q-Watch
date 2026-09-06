#include <Arduino.h>
#include <Wire.h>
#include "hw_config.h"

#define MAG_ADDR 0x2C

uint8_t readRegister(uint8_t deviceAddr, uint8_t regAddr) {
    Wire.beginTransmission(deviceAddr);
    Wire.write(regAddr);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)deviceAddr, (uint8_t)1);
    if (Wire.available()) {
        return Wire.read();
    }
    return 0xFF; // Return FF if read fails
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n\n========================================");
    Serial.println("Q-WATCH SENSOR DIAGNOSTIC: PASSIVE DUMP");
    Serial.println("========================================");
    Serial.println("Scanning registers 0x00 to 0x0F on device 0x2C");

    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(100000);

    Wire.beginTransmission(MAG_ADDR);
    if (Wire.endTransmission() != 0) {
        Serial.println("WARNING: Device 0x2C NOT responding!");
    } else {
        Serial.println("Device 0x2C found. Starting dump loop...\n");
    }
}

void loop() {
    Serial.print("DUMP: ");

    // Perform a continuous block read if possible, or individual reads
    // Some chips require block reads to latch data, but we'll try individual first
    // to strictly get every register value independently.
    for (uint8_t i = 0; i <= 0x0F; i++) {
        uint8_t val = readRegister(MAG_ADDR, i);
        Serial.printf("%02X: %02X  ", i, val);
    }
    Serial.println();

    delay(2000); // 2 seconds delay for slow, readable output
}
