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
  // Using a slightly smaller font or splitting the text so it doesn't break out of the box
  oled.setFont(u8g2_font_ncenB08_tr);

  const char* text1 = "Q-WATCH OLED";
  const char* text2 = "TEST OK";

  int w1 = oled.getStrWidth(text1);
  int w2 = oled.getStrWidth(text2);

  // Draw the two lines centered
  oled.drawStr((128 - w1) / 2, 28, text1);
  oled.drawStr((128 - w2) / 2, 44, text2);

  // Send buffer to the display
  oled.sendBuffer();

  Serial.println("Display initialized and test screen rendered.");
}

void loop() {
  delay(1000);
}
