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
// I2C Note 1 (Pull-ups): Breakout boards often include 4.7k or 10k pull-ups.
// Placing 4 modules in parallel yields an effective resistance of ~1.1k to 2.5k.
// For standard 100kHz or 400kHz operation on short internal watch wiring (low
// capacitance), 2.2k - 4.7k is ideal. If effective resistance drops below 1.5k,
// the ESP32-S3 might struggle to pull the line low enough to meet the logic-low
// threshold (V_IL). Recommendation: Physically remove the pull-up SMD resistors
// from 2 or 3 of the 4 breakout boards to maintain an effective ~4.7k resistance.
//
// I2C Note 2 (MAX30100): The user has a specific hardware modification to allow
// their MAX30100 breakout to operate safely on the 3.3V bus. Details of this
// physical trace cut/jumper modification are maintained by the user.
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
// Onboard RGB LED [ESP32-S3 SuperMini WS2812]
// ----------------------------------------------------------------------------
#define RGB_LED    48

// ----------------------------------------------------------------------------
// Reserve / Unused Pins
// ----------------------------------------------------------------------------
// GPIO 1, 40, 42    (Clean Reserves)
// GPIO 43, 44       (UART0 / Serial Debugging)
// GPIO 0, 3, 45, 46 (Strapping / Boot pins - DO NOT USE)

// ============================================================================
// HARDWARE DECOUPLING CAPACITOR STRATEGY
// ============================================================================
// 1. ESP32-S3 SuperMini: The main board already contains internal 100nF and 10uF
//    bulk decoupling for the ESP32-S3 chip. No additional capacitance is strictly
//    needed for the MCU itself.
// 2. I2C Sensors (BME280, MPU6500, HMC5883L, MAX30100): Commercial breakout boards
//    almost universally include local 100nF (104) decoupling caps across VDD/GND
//    near the sensor IC. Adding more local 100nF caps to these boards is redundant.
// 3. OLED: The 1.3" SPI OLED module includes its own charge pump capacitors and
//    local logic decoupling. No external cap required.
// 4. IR TX / Buzzer: These are high-current, pulsed loads. When the buzzer sounds
//    or the IR LED pulses (up to 100mA+), it causes sudden voltage droop on the
//    3.3V or 5V rail (depending on where they are sourced).
//    -> RECOMMENDATION: A single 100nF (104) ceramic capacitor placed in parallel
//       with a 10uF bulk capacitor across the power rails of the IR TX LED and
//       Buzzer driver is highly beneficial. It suppresses high-frequency switching
//       noise and prevents voltage droops that could reset the ESP32 or disrupt
//       I2C readings. Place these physically close to the MOSFET/Driver stage.
// ============================================================================

#endif // HW_CONFIG_H
