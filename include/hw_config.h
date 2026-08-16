#ifndef HW_CONFIG_H
#define HW_CONFIG_H

// OLED Display (SPI) Pin Configuration
// Using safer pins that don't conflict with ESP32-S3 Flash/PSRAM or strapping
#define OLED_MOSI  5
#define OLED_CLK   7
#define OLED_CS    4
#define OLED_DC    2
#define OLED_RST   8

#endif // HW_CONFIG_H
