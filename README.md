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
    *   `SENSORS` (Compass Cal, IMU Cal, Health, Sensor Status)
    *   `SYSTEM` (Storage Info, Reset Settings)

### Milestone 5: 4-Button Navigation & Gesture Framework
*   **On-Screen Input & Control:** Multi-button layout (K1 3-way directional + dedicated CANCEL button).
*   **Gestures & Semantics:**
    *   **HOME Screen:** Short UP / DOWN = Cycle Watch Face styles; Short CANCEL = Display ON/OFF toggle (10s auto-sleep); Long CANCEL = Enter Deep Sleep.
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

### Milestone 6(A): MAX30102 Health & Pulse Oximeter App
*   **Live Health Monitoring (Page 1):** Real-time PPG pulse waveform graph rendered chronologically from a 64-sample circular buffer, live Heart Rate (BPM), SpO2 percentage with a horizontal progress bar gauge, MAX30102 die/sensor temperature reading (°C), and dynamic finger contact detection.
*   **Whole-Day History & Trends (Page 2):** Historical trend graphs for Heart Rate, SpO2, and Sensor Temperature filtered strictly to today's local calendar day using `localtime_r` calendar comparisons (`tm_year`, `tm_yday`). Provides latest, minimum, maximum, and average summary statistics.
*   **Independent Background Scheduling:** Autonomous deep-sleep periodic wakeup logging for MAX30102 and BME280 sensors. Each sensor maintains an independent schedule (`last_bme` and `last_health` stored in persistent `Preferences`). Deep sleep calculates the earliest required wakeup time across all active background sensors.
*   **Strict Power Lifecycle:** MAX30102 LEDs and optical engine automatically power down when navigating away from Page 1, switching to Page 2, or performing background snapshots to maximize battery longevity.
*   **Health Settings:** Configurable under `SETTINGS -> SENSORS -> HEALTH` (Background Recording ON/OFF and Interval: 5m, 10m, 15m, 30m, 1h).

### Milestone 7: IMU6500 Sub-App Architecture & BLE Air Mouse Mode
*   **Sub-App Selector Shell:** IMU6500 operates as a parent menu containing two distinct sub-applications:
    *   **`ALTIMETER`:** Preserves the full artificial horizon / attitude indicator, altitude zero/reference, 3D compass integration, telemetry page, and IMU calibration/axis controls (including `Invert Z`).
    *   **`AIR MOUSE`:** Converts the watch into a wireless BLE HID air mouse remote for PCs, tablets, and mobile devices.
*   **Native BLE HID Mouse Lifecycle:** Utilizes native ESP32 BLE HID (`BLEHIDDevice`) for fast pairing and full teardown lifecycle management (`BLEDevice::deinit(true)`). BLE operates strictly when explicitly started inside Air Mouse mode and completely stops when exiting to conserve battery.
*   **Gyro Motion Pipeline:** Direct angular velocity control using calibrated MPU-6500 gyro data (Gyro Y → X movement, Gyro X → Y movement) with dead-zone noise suppression (~6 °/s threshold), exponential low-pass smoothing, sensitivity multipliers (`LOW`: 0.5x, `MED`: 1.0x, `HIGH`: 2.0x), and integer accumulation clamped to HID range (-127 to +127).
*   **Dual Mode & Controls:**
    *   **`POINTER` Mode:** Short UP = Left Click; Short DOWN = Right Click.
    *   **`SCROLL` Mode:** Short UP = Scroll Up; Short DOWN = Scroll Down.
    *   **Toggle Mode:** Short CANCEL toggles between `POINTER` and `SCROLL` modes.
    *   **Pause & Recenter:** Short OK toggles pointer/scroll activity ON/OFF; Long OK establishes an Air Mouse recenter offset without modifying global IMU calibration.
    *   **Sensitivity Control:** Long UP / Long DOWN adjusts sensitivity levels (`LOW`, `MED`, `HIGH`).
    *   **Reconnect Handling:** Displays explicit BLE status (`OFF`, `CONNECTING`, `CONNECTED`, `DISCONNECTED`). If a host disconnects, pressing short OK explicitly restarts advertising and reconnects.


### Milestone 8: Full IR Remote Subsystem & Bruce / Flipper Zero Compatibility
* **Pure Menu Architecture:** Structured as a 9-item scrolling menu shell (`TV-B-GONE`, `CUSTOM IR`, `IR READ`, `QUICK REMOTE`, `UNIVERSAL`, `RECENT`, `FAVORITES`, `IR FILES`, `IR LAB`) on the 128x64 OLED display.
* **Flipper Zero & Bruce `.ir` File Compatibility:** Dedicated parser and serializer for standard `.ir` library files, supporting both `type: parsed` (protocol, address, command, nbits) and `type: raw` (frequency, duty_cycle, microsecond pulse data). Files stored under `/ir/` on LittleFS can be transferred seamlessly between Bruce, Flipper Zero, and Q-Watch.
* **Exact Protocol Variant Preservation:** Full support and 100% round-trip fidelity for `NEC`, `NECext`, `NEC42`, `Samsung32`, `RC5`, `RC5X`, `RC6`, `SIRC` (Sony 12-bit), `SIRC15`, and `SIRC20`. Preserves exact multi-byte hex addresses/commands, bit counts, and raw carrier frequency/duty cycle attributes without inventing unmeasured default values.
* **Non-Blocking IR Read & RX Lifecycle:** Asynchronous signal capture pipeline (`IRrecv` on GPIO 17) with explicit receiver ownership. RX automatically pauses during transmissions (`IRsend` on GPIO 18) and when navigating away from capture screens.
* **Interactive Quick Remote Cloning:** Multi-button remote builder workflow: Name Remote → Name Button → Learn Signal → Test Transmission → Save Button → Add Additional Buttons → Save as standard `.ir` file.
* **TV-B-GONE Power Blaster:** Non-blocking power code transmitter featuring animated progress status (`CODE X/Y`), immediate cancellation on Short CANCEL, and data-driven code expansion support via `/ir/tvbgone.ir` on LittleFS.
* **IR Signal Lab Diagnostics:** Engineering laboratory tool featuring 36 kHz, 38 kHz, and 40 kHz PWM carrier output validation (LEDC channel) and real-time raw-to-parsed protocol decoding analysis.
* **File Manager Integration:** Selecting any `.ir` file directly from the main Q-Watch File Manager (`APP_FILE_MANAGER`) launches the IR Remote viewer to inspect and transmit buttons immediately.
* **Signal Limits & Storage:** Strict `MAX_IR_RAW_TIMINGS = 1024` buffer enforcement across parsing, capturing, and serialization. Persistent Recent signals log (`/ir/recent.txt`) and Favorite button shortcuts (`/ir/favorites.txt`).

### Milestone 9: Altimeter Control, RGB LED System, Weather Binding & Factory Reset
* **BME Altimeter Input Handling:** Interactive page navigation and calibration controls on the BME280 Altimeter app (`[UP]`/`[DN]` to cycle pages, `[OK]` to zero reference altitude, tune temperature offset, and reset baseline).
* **WS2812 RGB LED Control Center:** Full settings menu (`LED CONTROL`) with toggle, color presets (Red, Green, Blue, Amber, Cyan, Purple, White), brightness levels (10% to 100%), and animation effects (Solid, Pulse, Rainbow, Strobe, Tactical Beacon).
* **Live Weather Data Binding:** Real-time HTTPS OpenWeatherMap fetching seamlessly integrated with local BME280 telemetry fallback when offline.
* **Factory Reset Protection:** `SETTINGS -> SYSTEM -> RESET SETTINGS` provides a dedicated confirmation dialog with `[OK] CONFIRM` and `[CANCEL] ABORT` to safely reset configuration to defaults without data corruption.

### Milestone 10: Power Optimization, Instant Screen-Off & Raise-to-Wake Gesture
* **Instant Screen-Off:** Short press of the dedicated `CANCEL` button on the `HOME` screen immediately turns off display power (`displayManager.setPowerSave(true)`).
* **10-Second Deep Sleep Auto-Timeout:** If the screen is toggled off on the `HOME` screen, the watch automatically transitions into ESP32 deep sleep after 10 seconds of inactivity to maximize battery life.
* **Dual Deep Sleep Wakeup:** Configured ESP32-S3 `EXT1` active-low wakeup on both **GPIO 21** (`BTN_CANCEL`) and **GPIO 8** (`MPU_INT` motion interrupt).
* **Raise-to-Wake Gesture Detection:** Pre-sleep wrist orientation tracking via calibrated MPU-6500 (`pitch 15°..65°`, `|roll| <= 35°`) with configurable setting toggle under `SETTINGS -> POWER -> RAISE TO WAKE`.
* **Wake Audio Chime:** Pleasant dual-tone wake sound upon resuming from sleep.

### Milestone 11: Comprehensive Watch & Timekeeping Suite
* **4 Switchable Watch Faces:**
    * **Digital Face:** Clean modern layout with large digital time, seconds, date, battery percentage, weather widget (temperature), pedometer steps widget, and status icons (Wi-Fi `W`, Alarms `A`, Hourly Chime `C`).
    * **Analog Face:** 12-hour tick dial with accentuated quarter markers (12, 3, 6, 9), hardware FPU trigonometric hands (`cosf()`/`sinf()`), center disc hub, and flanking telemetry widgets (battery %, date window, weather, steps).
    * **Retro Casio LCD Face:** Vintage double-frame border with top Day-of-Week selector pills (`SU MO TU WE TH FR SA`) highlighting the active day with an inverted box, `CHI`/`ALM` status tags, bracketed seconds box, and bottom telemetry.
    * **Tactical / Mission Face:** Military tactical HUD with live compass heading (`HDG 000M`), barometric altitude (`ALT 000m`), 24-hour military time, step progress bar against target goal, battery telemetry, and mission/stopwatch active status.
    * **On-the-Fly Face Cycling:** Quick `[UP]` / `[DN]` buttons on the `HOME` screen cycle watch faces instantly with persistent LittleFS saving.
* **Unified Clock Suite App (`MAIN_MENU -> CLOCK`):**
    * **Watch Face Selector:** Interactive visual picker across all 4 watch face styles.
    * **Face Widgets Options:** Toggle individual face widgets (Date, Battery, Weather, Steps, Status Icons).
    * **Tactical Stopwatch (Chronograph):** Millisecond precision (`MM:SS.hh`), split lap recording up to 8 laps, `[OK]` for Start/Lap, `[UP]` for Pause/Resume, `[DN]` for Reset.
    * **Countdown Timer:** Duration presets (1m, 3m, 5m, 10m, 15m, 30m, 60m), countdown tick loop, audible alert on expiration via `soundManager.playAlert()`.
    * **Multi-Alarms:** 3 independent configurable daily alarms stored in `/config/alarms`, audible ringing sequence, snooze (+5 min), dismiss, and interactive Hour/Minute editor.
    * **Hourly Chime:** Configurable audio chime on each hour (`:00:00`) with dual high-frequency tone.
    * **World Clock:** 12 major world cities (Kolkata, UTC, London, Berlin, New York, Chicago, Denver, Los Angeles, Dubai, Singapore, Tokyo, Sydney) with precise quarter-hour timezone offset calculations and relative day indicator (`+1 DAY`, `SAME DAY`, `-1 DAY`).
    * **Pedometer & Step Goal:** Real-time MPU-6500 accelerometer magnitude step detector with threshold hysteresis and debouncing, estimated distance (`km`), calories (`kcal`), step goal adjustment (+/- 1,000 steps), and step counter reset.

### Milestone 12: Micro-ELF Relocatable Q-App Store Architecture, Loader & Target Ecosystem
* **Q-App Binary Interface (`qwatch_api.h`):** Versioned C-ABI export table (`QWatchAPI`) providing modular, hardware-abstracted access to OLED display graphics, 4-button input events, MPU-6500 6-axis motion fusion, QMC5883P 3D magnetometer, MAX30102 PPG/health, IR transceiver, audio tones, WS2812 RGB LED, LittleFS sandboxed file I/O, system time, and lifecycle hooks.
* **Micro-ELF Relocatable Binary Format (`.qapp`):** Dual-segment architecture cleanly separating executable code placed in executable internal SRAM (`MALLOC_CAP_EXEC | MALLOC_CAP_INTERNAL`) from initialized data/BSS/heap allocated in external 2MB PSRAM (`MALLOC_CAP_SPIRAM`), resolved via relocation table records (`QAppReloc`).
* **Dynamic In-Firmware App Launcher:** Dynamic scanning of LittleFS `/apps/*.qapp`, binary header validation, dynamic symbol resolution, per-frame `update(dt)` / `render()`, button routing, and clean unmapping on exit.
* **Reference Q-Apps:**
    * **Tilt Ball (`apps/tilt_ball.qapp`):** 6-axis MPU-6500 roll/pitch physics simulation, bounce damping, target collision, audio score chimes, and LED flash.
    * **Compass HUD (`apps/compass_hud.qapp`):** QMC5883P 3D magnetometer tactical HUD, rotating North-seeking needle, 8-point cardinal telemetry, waypoint bearing lock, course deviation indicator, and declination adjustment.
* **Automated Verification:** Comprehensive test harness (`tests/test_qapp_system.cpp`) with ESP32-S3 IRAM/PSRAM POC execution, fault injection (corrupt magic, ABI mismatch, OOB entry, invalid relocations), and 100-cycle zero-leak stress tests.

### Milestone 13: Tactical Animation Engine, Custom Boot Splash & Player
* **Compact Binary Animation Format (`.anim`):** 16-byte packed header (`magic: 0x4D4E4151` / `"QANM"`, `version: 1`, `width: 128`, `height: 64`, `frame_count`, `frame_delay_ms`, `flags`) with direct sequential 1024-byte 1-bit XBM frames for fast streaming off LittleFS without RAM buffering.
* **Native 1-Bit BMP Compatibility:** Auto-detects and decodes standard 1-bit Windows monochrome BMP files (`.bmp`), converting bottom-up MSB-first scanlines to display-native LSB-first XBM bitmaps on-the-fly with hardware bit-reversal.
* **On-Device Animation Browser (`SYS MENU -> ANIMATIONS`):** Lists installed `.anim` and `.bmp` files in `/anim` and `/boot`, displaying filename, frame count (`XF`), or `BMP` badge with 3-item scrolling viewport and scrollbar.
* **Tactical Animation Player (`APP_ANIM_PLAYER`):**
    * Full 128x64 display rendering with tactical auto-hiding HUD overlay (playback status `PLAY`/`PAUSE`, frame position `X/Y`, speed multiplier, timeline progress bar).
    * `[OK]` Short Press: Toggle Play / Pause.
    * `[OK]` Long Press: Set current animation as system boot animation (`/boot/boot.anim` or `/boot/boot.bmp`).
    * `[UP]` Short Press: Speed cycle (`1.0x` -> `1.5x` -> `2.0x` -> `0.5x` -> `1.0x`) when playing, or Step Forward 1 frame when paused.
    * `[DN]` Short Press: Toggle Loop (`LOOP: ON` / `LOOP: OFF`) when playing, or Step Backward 1 frame when paused.
    * `[CANCEL]` Short / Long Press: Exit player and return to browser or file manager.
* **Custom Boot Animation & Instant Skip:** On cold boot or reset, plays `/boot/boot.anim` or `/boot/boot.bmp` if installed, or falls back to the MI6 tactical radar sweep sequence. Any button press immediately skips boot animation to launch straight into the watch face.
* **Animation Tooling (`tools/anim_packer.py`):** Standalone CLI tool to pack frame sequences, inspect metadata, and generate procedural animations (`radar.anim`, `gunbarrel.anim`).

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
| BME280 / MPU-6500 / QMC5883P / MAX30102 | SCL | 16 | Address 0x57 for MAX30102 |
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
