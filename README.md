# 007 Q Watch

A James Bond "First Light" inspired watch built on the ESP32-S3 SuperMini.

## Milestone 1: Hardware Foundation

This milestone establishes the initial hardware setup, focusing strictly on getting the ESP32-S3 communicating with the 1.3-inch OLED display over SPI.

### Hardware

*   **Microcontroller:** ESP32-S3 SuperMini (4MB Flash, 2MB PSRAM)
*   **Display:** 1.3" 128x64 OLED Display (White Color)

#### A note on the Display Controller
The physical OLED module is labeled/sold as an "SSD1106". However, in the embedded display ecosystem, 1.3-inch OLEDs almost universally use the **SH1106** controller. This project uses the `U8g2` library configured with the `SH1106` driver to communicate with the display over SPI.

### Pinout (Hardware SPI)

To avoid conflicts with the ESP32-S3's internal Flash/PSRAM lines (often GPIO 9-14 depending on the specific variant) and strapping pins, the following custom SPI pinout is used:

| ESP32-S3 Pin | OLED Pin | Description       |
| :---         | :---     | :---              |
| GPIO 5       | MOSI/DIN | Data In           |
| GPIO 7       | CLK/SCK  | Clock             |
| GPIO 4       | CS       | Chip Select       |
| GPIO 2       | DC       | Data/Command      |
| GPIO 8       | RST      | Reset             |

*Note: GPIO 3 is deliberately avoided for DC as it is a strapping pin.*

### Getting Started

1.  Open the project in PlatformIO.
2.  Connect the ESP32-S3 SuperMini to the OLED using the pinout above.
3.  Build and upload the code.
4.  The display should show a border around the edge and the text "007 SYSTEM INITIALIZED".
