#ifndef HW_CONFIG_H
#define HW_CONFIG_H

// ============================================================================
// Q-WATCH HARDWARE CONFIGURATION
// ============================================================================

// ----------------------------------------------------------------------------
// OLED Display (SPI) Pin Configuration [LOCKED]
// ----------------------------------------------------------------------------
#define OLED_MOSI  5
#define OLED_CLK   7
#define OLED_CS    4
#define OLED_DC    2
#define OLED_RST   8

// ----------------------------------------------------------------------------
// Shared I2C Bus Configuration [BME280, MPU-6500, HMC5883L, MAX30100]
// ----------------------------------------------------------------------------
#define I2C_SDA    15
#define I2C_SCL    16

// ----------------------------------------------------------------------------
// Navigation Buttons [Requires internal pull-ups]
// ----------------------------------------------------------------------------
#define BTN_UP     39
#define BTN_SEL    21  // Dual purpose: Normal SELECT input and Deep Sleep RTC Wake
#define BTN_DN     41

// ----------------------------------------------------------------------------
// Infrared (IR) Configuration
// ----------------------------------------------------------------------------
#define IR_RX      17
#define IR_TX      18

// ----------------------------------------------------------------------------
// Buzzer Configuration [Requires external MOSFET/BJT driver]
// ----------------------------------------------------------------------------
#define BUZZER_PIN 6

// ----------------------------------------------------------------------------
// Battery Voltage Monitor (ADC)
// ----------------------------------------------------------------------------
// Uses an external 1/2 voltage divider:
// R1 = 100k (Connected to Raw LiPo VBAT+)
// R2 = 100k (Connected to GND)
// A 100nF (104) ceramic capacitor is placed in parallel with R2 (ADC node to GND)
// to filter out high-frequency noise before the ESP32 ADC reads it.
// Note: GPIO 40 was originally evaluated but is digital-only on ESP32-S3.
// We use GPIO 1 (ADC1_CH0) instead, which is a clean, non-strapping ADC pin.
#define BATTERY_ADC 1

// ----------------------------------------------------------------------------
// Onboard RGB LED [ESP32-S3 SuperMini WS2812]
// ----------------------------------------------------------------------------
#define RGB_LED    48

// ----------------------------------------------------------------------------
// Reserve / Unused Pins
// ----------------------------------------------------------------------------
// GPIO 40, 42       (Clean Reserves)
// GPIO 43, 44       (UART0 / Serial Debugging)
// GPIO 0, 3, 45, 46 (Strapping / Boot pins - DO NOT USE)

#endif // HW_CONFIG_H
