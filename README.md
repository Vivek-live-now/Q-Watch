# 007 Q-Watch

A James Bond "First Light" inspired smartwatch built on the ESP32-S3 SuperMini.

## Milestones & Features

### Milestone 1: Hardware Foundation
*   **Microcontroller:** ESP32-S3 SuperMini (4MB Flash, 2MB PSRAM)
*   **Display:** 1.3" 128x64 OLED Display (White Color)
*   **Controller:** SH1106 via 4-wire Hardware SPI.
*   **Configuration:** Custom `esp32s3_supermini` PlatformIO board definition to ensure correct memory and USB routing.

### Milestone 2 & 3: Connectivity, Time & Weather
*   **Captive Portal:** A mobile-friendly setup dashboard accessible at `192.168.4.1` (when Wi-Fi is unconfigured/disconnected) or `q-watch.local` via mDNS on your home network.
*   **Wi-Fi Management:** Asynchronous background connecting and a captive portal scanner to easily connect to local networks.
*   **NTP Clock:** Asynchronous time synchronization using standard ESP32 SNTP, allowing the clock to continue accurately without internet.
*   **OpenWeatherMap Integration:** Configurable weather fetching over **HTTPS** (Temperature, Feels Like, Humidity, Wind Speed, Condition).
*   **Energy Efficient Architecture:** Display only redraws when seconds change (1Hz). Weather API calls are heavily rate-limited and cached, executed via FreeRTOS tasks to prevent UI freezing.

## Hardware Pinout (SPI)

To avoid conflicts with the ESP32-S3's internal Flash/PSRAM lines and strapping pins, the following custom SPI pinout is used:

| ESP32-S3 Pin | OLED Pin | Description       |
| :---         | :---     | :---              |
| GPIO 5       | MOSI/DIN | Data In           |
| GPIO 7       | CLK/SCK  | Clock             |
| GPIO 4       | CS       | Chip Select       |
| GPIO 2       | DC       | Data/Command      |
| GPIO 8       | RST      | Reset             |

## Getting Started

1.  Open the project in PlatformIO.
2.  Connect the ESP32-S3 SuperMini to the OLED using the pinout above.
3.  Build and upload the code using the pre-configured `esp32s3_supermini` environment.
4.  On first boot, connect to the **Q-Watch-Setup** Wi-Fi network and navigate to `http://192.168.4.1`.
5.  Enter your Wi-Fi credentials, timezone, and OpenWeatherMap API key.
