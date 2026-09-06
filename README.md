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

## Hardware Architecture & Pinout

To avoid conflicts with the ESP32-S3's internal Flash/PSRAM lines and strapping pins, the following optimized GPIO map is used.

### 1. OLED Display (LOCKED)
| Peripheral | Function | GPIO |
| :--- | :--- | :--- |
| OLED | MOSI/DIN | 5 |
| OLED | CLK/SCK | 7 |
| OLED | CS | 4 |
| OLED | DC | 2 |
| OLED | RST | 8 |

### 2. I2C Sensors (Shared Bus)
| Peripheral | Function | GPIO |
| :--- | :--- | :--- |
| BME280 / MPU-6500 / HMC5883L / MAX30100 | SDA | 15 |
| BME280 / MPU-6500 / HMC5883L / MAX30100 | SCL | 16 |

*Note on I2C Pull-ups:* When placing 4 breakout boards in parallel, the effective pull-up resistance drops significantly. To maintain an ideal ~4.7kΩ resistance, it is recommended to physically desolder the SMD pull-up resistors from 2 or 3 of the breakout boards.
*Note on MAX30100:* This project utilizes a specific physical modification to allow the 1.8V MAX30100 breakout to safely interface with the 3.3V logic of the ESP32.

### 3. Inputs & Audio/Visual
| Peripheral | Function | GPIO | Notes |
| :--- | :--- | :--- | :--- |
| Button Up | INPUT_PULLUP | 39 | Reclaims JTAG MTCK |
| Button Select / Wake | INPUT_PULLUP / RTC WAKE | 21 | Dual purpose: Normal SELECT input and Deep Sleep RTC Wake |
| Button Down | INPUT_PULLUP | 41 | Reclaims JTAG MTDI |
| Battery Monitor | ADC1_CH0 | 1 | Requires 100k/100k external divider from raw VBAT + 104 filter cap |
| IR Receiver | RX DATA | 17 | |
| IR Transmitter| TX DATA | 18 | High current pulse load |
| Buzzer | CONTROL | 6 | Requires N-channel MOSFET/BJT driver |
| RGB LED | WS2812 DATA | 48 | Preserved for onboard LED |

### 4. Available / Reserved Pins
The following GPIOs on the ESP32-S3 SuperMini have been intentionally left unassigned to preserve them for future features, sensors, or debugging.
*   **GPIO 40:** Clean reserve pin (Reclaims JTAG MTDO).
*   **GPIO 42:** Clean reserve pin.
*   **GPIO 43:** Reserved (Used for hardware UART0 TX / Serial Debugging if USB CDC fails).
*   **GPIO 44:** Reserved (Used for hardware UART0 RX / Serial Debugging if USB CDC fails).
*   *Note: GPIOs 0, 3, 45, and 46 are strictly avoided as they are boot/strapping pins.*

### 5. Decoupling Capacitor Strategy (104 Ceramic)
The ESP32-S3, OLED, and individual sensor breakouts already contain adequate local decoupling. However, because the **IR Transmitter** and **Buzzer** are high-current pulsed loads, it is highly recommended to place a single `100nF (104)` ceramic capacitor in parallel with a `10uF` bulk capacitor directly across the power rails of their respective driver circuits to prevent voltage droops.

## Getting Started

1.  Open the project in PlatformIO.
2.  Connect the hardware according to the pinout above.
3.  Build and upload the code using the pre-configured `esp32s3_supermini` environment.
4.  On first boot, connect to the **Q-Watch-Setup** Wi-Fi network and navigate to `http://192.168.4.1`.
5.  Enter your Wi-Fi credentials, timezone, and OpenWeatherMap API key.
6.  Once connected to your home network, access the watch dashboard anytime at `http://q-watch.local`.
