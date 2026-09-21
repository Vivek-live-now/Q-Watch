# 007 Q-Watch

A James Bond "First Light" tactical smartwatch built on the ESP32-S3 SuperMini.

## Milestones & Features

### Milestone 1: Hardware Foundation
*   **Microcontroller:** ESP32-S3 SuperMini (4MB Flash, 2MB PSRAM).
*   **Display:** 1.3" 128x64 OLED Display (White Color).
*   **Controller:** SH1106 via 4-wire Hardware SPI.
*   **Configuration:** Custom `esp32s3_supermini` PlatformIO board definition to ensure correct memory and USB routing.

### Milestone 2 & 3: Connectivity, Time & Weather
*   **Captive Portal:** A mobile-friendly setup dashboard accessible at `192.168.4.1` (when Wi-Fi is unconfigured/disconnected) or `q-watch.local` via mDNS on your home network.
*   **Wi-Fi Management & Settings:** Dedicated `SETTINGS -> CONNECTIVITY -> Wi-Fi` status screen displaying Power (ON/OFF), Status (`OFF`, `NO CREDS`, `CONNECTING`, `CONNECTED`, `FAILED`, `DISCONNECTED`), current SSID, and IP address. Features battery-saving radio shutdown (`WiFi.mode(WIFI_OFF)`) when disabled.
*   **Wi-Fi Scanner & Direct Connect:** Search local networks directly on the OLED (`SETTINGS -> CONNECTIVITY -> SCAN NETWORKS`), displaying RSSI and security locks. Select any open or encrypted network and input credentials right on the watch.
*   **NTP Time Synchronization:** Non-blocking SNTP time synchronization. Auto-syncs when Wi-Fi connects or on demand via `SETTINGS -> TIME -> SYNC NOW`. Features POSIX timezone support and 12-hour (AM/PM) / 24-hour time formatting toggles.
*   **OpenWeatherMap Integration:** Configurable weather fetching over **HTTPS** (Temperature, Feels Like, Humidity, Wind Speed, Condition).
*   **Energy Efficient Architecture:** Display only redraws when seconds change (1Hz). Weather API calls are heavily rate-limited and cached, executed via FreeRTOS tasks to prevent UI freezing.

### Milestone 4: Local Storage, File Manager & Opt-In Web File Server
*   **LittleFS Partition:** Configured a dedicated LittleFS filesystem partition for persistent local storage of configurations, calibration data, and settings (`/config/settings`).
*   **FileManager Abstraction:** A lightweight C++ wrapper class around LittleFS for robust read/write/append operations for both standard `String` text and raw binary (`uint8_t*`) data.
*   **File Browser UI:** Native 'FILE MANAGER' app in the Q-Watch menu. Features dynamic directory browsing, file size inspection, and a unified storage information panel (Total/Used/Free space on ESP32), operating within the 3-item OLED viewport.
*   **Opt-In Web File Server:** When enabled via `SETTINGS -> CONNECTIVITY -> FILE SERVER`, visiting `http://q-watch.local/fm` launches a full web-based LittleFS file manager interface:
    *   Dynamic directory and file browser
    *   File upload via HTTP multipart
    *   Direct file streaming and download
    *   File/directory deletion and folder creation
    *   Excludes destructive filesystem formatting for safety
*   **Settings Control Center:** Structured Settings hub with 6 top-level categories:
    *   `CONNECTIVITY` (Wi-Fi details, Wi-Fi Scanner, BLE, File Server status)
    *   `TIME` (Sync Now, Auto Sync, Timezone, 24 Hour)
    *   `POWER` (Display Timeout, Sleep Time, Low Power)
    *   `DISPLAY` (Contrast, Invert, UI Options)
    *   `SENSORS` (Compass Cal, IMU Cal, Sensor Status)
    *   `SYSTEM` (Storage Info, Reset Settings)

### Milestone 5: 4-Button Navigation & Gesture Framework
*   **On-Screen Input & Control:** Multi-button layout (K1 3-way directional + dedicated CANCEL button).
*   **Gestures & Semantics:**
    *   **HOME Screen:** Short CANCEL = Display ON/OFF toggle; Long CANCEL = Enter Deep Sleep.
    *   **Sub-Screens:** Short CANCEL = Back; Long OK = Back; Long CANCEL = Return to HOME.
    *   **Combinations & Double-Tap:** Generic framework support for CANCEL+UP, CANCEL+OK, CANCEL+DN, and Double-Tap CANCEL for future shortcut mapping.
*   **Dual Deep Sleep Wake:** Configured ESP32-S3 `EXT1` active-low wakeup on both **GPIO 21** (CANCEL button) and **GPIO 8** (MPU-6500 raise-to-wake motion interrupt). The initial wake event is automatically consumed so it does not trigger accidental in-app actions.


### Milestone 6: BME280 Environmental Sensor App & Weather App Architecture
*   **BME280 App (5 Pages):**
    *   **Page 1 (Pressure):** Real-time barometric pressure in hPa with min/max auto-scaled historical trend graph.
    *   **Page 2 (Humidity):** Real-time relative humidity (%RH) with min/max auto-scaled historical trend graph.
    *   **Page 3 (Temperature):** Real-time temperature (°C) incorporating active calibration offset with historical trend graph.
    *   **Page 4 (Relative Height / Altimeter):** Pressure-based relative altitude measurement state machine (OFF -> [OK] SET ZERO -> MEASURING -> [OK] PAUSE -> [OK] RESUME).
    *   **Page 5 (Diagnostic & Calibration):** Displays sensor status (Address 0x76, Chip ID 0x60), live readings, active temperature offset (°C), reference pressure (hPa), reading age, and interactive controls to adjust temperature offset or reset calibration.
*   **Weather App (6 Pages):**
    *   **Page 1 (Local Readings):** Live local BME280 temperature, humidity, and barometric pressure.
    *   **Page 2 (OpenWeatherMap + Cute Animated Graphics):** HTTPS OpenWeatherMap report with animated weather icons (Clear, Clouds, Rain, Thunderstorm, Mist/Fog, plus offline animated cute placeholder).
    *   **Pages 3–5 (24h Trend Graphs):** Whole-day temperature, pressure, and humidity graphs drawn from historical buffer.
    *   **Page 6 (Logging & Refresh Settings):** Configurable sensor logging / refresh interval (5 min, 10 min, 15 min, 30 min, 1 hour).
*   **24-Hour Historical Data Logging in LittleFS:**
    *   Compact binary ring-buffer logger (`/bme_history.bin`) storing up to 288 records (10 bytes/entry) for 24-hour history at 5-minute intervals.
    *   **Persistent Calibration:** Saves temperature offset (`temp_offset`) and reference ground pressure (`reference_pressure`) in NVS Preferences, surviving deep sleep and complete restarts.
    *   **Deep Sleep Periodic Logging:** When waking from deep sleep via the RTC timer, the watch silently reads the BME280, logs calibrated values to `/bme_history.bin`, re-arms the timer, and returns to deep sleep immediately without activating the OLED display or Wi-Fi.

## Hardware Architecture & Pinout

To avoid conflicts with the ESP32-S3's internal Flash/PSRAM lines and strapping pins, the following optimized GPIO map is used.

### 1. OLED Display
| Peripheral | Function | GPIO | Notes |
| :--- | :--- | :--- | :--- |
| OLED | MOSI/DIN | 5 | Hardware SPI |
| OLED | CLK/SCK | 7 | Hardware SPI |
| OLED | CS | 4 | |
| OLED | DC | 2 | |
| OLED | RST | 41 | Reassigned to GPIO 41 (Standard digital output) |

### 2. I2C Sensors (Shared Bus)
| Peripheral | Function | GPIO | Notes |
| :--- | :--- | :--- | :--- |
| BME280 / MPU-6500 / QMC5883P / MAX30102 | SDA | 15 | |
| BME280 / MPU-6500 / QMC5883P / MAX30102 | SCL | 16 | |
| MPU-6500 Interrupt | INT | 8 | RTC Wake Capable (Active-Low WOM Interrupt) |

*Note on I2C Pull-ups:* When placing 4 breakout boards in parallel, the effective pull-up resistance drops significantly. To maintain an ideal ~4.7kΩ resistance, it is recommended to physically desolder the SMD pull-up resistors from 2 or 3 of the breakout boards.

### 3. Inputs & Audio/Visual
| Peripheral | Function | GPIO | Notes |
| :--- | :--- | :--- | :--- |
| Button Up | INPUT_PULLUP | 39 | K1 Multi-directional UP |
| Button OK / Select | INPUT_PULLUP | 40 | K1 Multi-directional OK / SELECT |
| Button Down | INPUT_PULLUP | 42 | K1 Multi-directional DOWN |
| Button Cancel / Back | INPUT_PULLUP / RTC WAKE | 21 | Tactile CANCEL button & Deep Sleep RTC Wake source |
| Battery Monitor | ADC1_CH0 | 1 | Requires 100k/100k external divider from raw VBAT + 104 filter cap |
| IR Receiver | RX DATA | 17 | |
| IR Transmitter| TX DATA | 18 | High current pulse load |
| Buzzer | CONTROL | 6 | Requires N-channel MOSFET/BJT driver |
| RGB LED | WS2812 DATA | 48 | Onboard RGB LED |

### 4. Available / Reserved Pins
The following GPIOs on the ESP32-S3 SuperMini have been intentionally left unassigned to preserve them for future features, sensors, or debugging:
*   **GPIO 38:** Clean reserve pin.
*   **GPIO 43:** Reserved (Hardware UART0 TX / Serial Debugging if USB CDC fails).
*   **GPIO 44:** Reserved (Hardware UART0 RX / Serial Debugging if USB CDC fails).
*   *Note: GPIOs 0, 3, 45, and 46 are strictly avoided as they are boot/strapping pins.*

### 5. Decoupling Capacitor Strategy (104 Ceramic)
Because the **IR Transmitter** and **Buzzer** are high-current pulsed loads, it is recommended to place a single `100nF (104)` ceramic capacitor in parallel with a `10uF` bulk capacitor directly across the power rails of their respective driver circuits to prevent voltage droops.

## Getting Started

1.  Open the project in PlatformIO.
2.  Connect the hardware according to the pinout above.
3.  Build and upload the code using the pre-configured `esp32s3_supermini` environment.
4.  On first boot, configure Wi-Fi directly on the watch via `SETTINGS -> CONNECTIVITY -> SCAN NETWORKS` or connect to the **Q-Watch-Setup** Wi-Fi captive portal network at `http://192.168.4.1`.
5.  Enter your Wi-Fi credentials, timezone, and OpenWeatherMap API key.
6.  Once connected to your home network, access the watch dashboard anytime at `http://q-watch.local` or the Web File Manager at `http://q-watch.local/fm`.
