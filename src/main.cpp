#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include "hw_config.h"

// Initialize U8g2 for an SH1106 display over 4-wire hardware SPI
// The 'F' means full framebuffer mode (1024 bytes of RAM for 128x64)
// We pass the explicit SPI pins to a custom SPIClass instance, but U8g2 allows using
// standard hardware SPI if we just setup the SPI bus correctly before begin.
// However, to ensure 100% reliability on ESP32-S3 with custom pins,
// using the explicit pin constructor that includes Clock and Data is safer if the standard HW SPI fails.
// Since we are requested to use HW SPI, we must ensure U8g2 binds to the correct SPI interface.
// For ESP32, U8g2 uses VSPI or HSPI. We must call SPI.begin with our custom pins.

U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI u8g2(U8G2_R0, OLED_CS, OLED_DC, OLED_RST);

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Q Watch Initialization...");

  // Override standard SPI pins before u8g2 begins
  // The ESP32 core allows re-routing the default SPI pins.
  SPI.begin(OLED_CLK, -1, OLED_MOSI, OLED_CS); // sck, miso, mosi, ss

  // A short delay to allow the OLED power supply to stabilize
  delay(100);

  // Initialize the display. Since we called SPI.begin with custom pins,
  // the U8G2 HW SPI constructor will use these pins via the standard SPI object.
  u8g2.begin();

  // Some clone displays need explicit contrast setting to be visible
  u8g2.setContrast(255);

  // Clear display buffer
  u8g2.clearBuffer();

  // Draw a border around the entire 128x64 display
  u8g2.drawFrame(0, 0, 128, 64);

  // Set font and write text
  // Using a small font so the long text fits
  u8g2.setFont(u8g2_font_ncenB08_tr);

  // Center text roughly
  const char* text = "007 SYSTEM";
  const char* text2 = "INITIALIZED";

  int w1 = u8g2.getStrWidth(text);
  int w2 = u8g2.getStrWidth(text2);

  u8g2.drawStr((128 - w1) / 2, 28, text);
  u8g2.drawStr((128 - w2) / 2, 44, text2);

  // Send buffer to the display
  u8g2.sendBuffer();

  Serial.println("Display initialized and test screen rendered.");
}

void loop() {
  // Nothing to do for Milestone 1
  delay(1000);
}
