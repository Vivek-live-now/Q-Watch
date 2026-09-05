#include <Arduino.h>
#include <Wire.h>
#include "hw_config.h"

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("\n\n========================================");
  Serial.println("Q-WATCH I2C HARDWARE SCANNER");
  Serial.println("========================================");
  Serial.printf("SDA Pin: %d\n", I2C_SDA);
  Serial.printf("SCL Pin: %d\n", I2C_SCL);
  Serial.println("Initializing Wire...");

  Wire.begin(I2C_SDA, I2C_SCL);
  Serial.println("Wire initialized. Starting scan...");
}

void loop() {
  byte error, address;
  int nDevices;

  Serial.println("Scanning...");

  nDevices = 0;
  for(address = 1; address < 127; address++ ) {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.print(address, HEX);

      // Attempt to identify common Q-Watch sensors
      if (address == 0x68 || address == 0x69) Serial.print("  <-- Likely MPU-6500");
      else if (address == 0x76 || address == 0x77) Serial.print("  <-- Likely BME280");
      else if (address == 0x1E || address == 0x0D) Serial.print("  <-- Likely QMC5883L / HMC5883L");
      else if (address == 0x57) Serial.print("  <-- Likely MAX30100");

      Serial.println();
      nDevices++;
    }
    else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0) {
    Serial.println("No I2C devices found\n");
  } else {
    Serial.println("done\n");
  }

  delay(5000);           // wait 5 seconds for next scan
}
