#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include "hw_config.h"

// Initialize U8g2 for an SH1106 display over 4-wire hardware SPI
U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI oled(U8G2_R0, OLED_CS, OLED_DC, OLED_RST);

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Q Watch Initialization...");

  // Override standard SPI pins before oled begins
  SPI.begin(OLED_CLK, -1, OLED_MOSI, OLED_CS);

  // Initialize the display
  oled.begin();

  // Clear display buffer
  oled.clearBuffer();

  // Draw a border around the entire 128x64 display
  oled.drawFrame(0, 0, 128, 64);

  // Set font and write text
  oled.setFont(u8g2_font_ncenB08_tr);

  // Center text roughly
  const char* text = "Q-WATCH OLED TEST OK";

  int w1 = oled.getStrWidth(text);

  oled.drawStr((128 - w1) / 2, 32, text);

  // Send buffer to the display
  oled.sendBuffer();

  Serial.println("Display initialized and test screen rendered.");
}

void loop() {
  delay(1000);
}
