# 007 Q-Watch ⌚🕶️

> **The Ultimate James Bond "First Light" Tactical Smartwatch & Cyber-Deck**  
> Engineered on the **ESP32-S3 SuperMini** with 2MB PSRAM, 4MB Flash, Xtensa LX7 SIMD Vector Acceleration, GDMA Asynchronous OLED Engine, Flipper Zero/Bruce IR Subsystem, Micro-ELF Relocatable App Store, Wireless Recon Suite, and Native Android Jetpack Compose Companion.

---

## 📑 Table of Contents

- [Overview](#-overview)
- [System Architecture & Hardware Specs](#-system-architecture--hardware-specs)
  - [Hardware Specifications](#hardware-specifications)
  - [Pinout & GPIO Allocation](#pinout--gpio-allocation)
  - [I2C Address Map](#i2c-address-map)
  - [Electrical & Circuit Design Notes](#electrical--circuit-design-notes)
- [Comprehensive Feature Breakdown](#-comprehensive-feature-breakdown)
  - [1. Watch Faces & Timekeeping Suite](#1-watch-faces--timekeeping-suite)
  - [2. Environmental & Atmospheric Weather Station](#2-environmental--atmospheric-weather-station)
  - [3. Cardiorespiratory Health & Biometrics](#3-cardiorespiratory-health--biometrics)
  - [4. Navigation, Motion & 3D Vector Graphics](#4-navigation-motion--3d-vector-graphics)
  - [5. BLE Air Mouse Remote](#5-ble-air-mouse-remote)
  - [6. On-Device TinyML Gesture AI](#6-on-device-tinyml-gesture-ai)
  - [7. Tactical Wireless Recon & SIGINT Suite](#7-tactical-wireless-recon--sigint-suite)
  - [8. Flipper Zero & Bruce-Compatible IR Remote Subsystem](#8-flipper-zero--bruce-compatible-ir-remote-subsystem)
  - [9. Dasai Mochi Procedural Emote Pet Companion](#9-dasai-mochi-procedural-emote-pet-companion)
  - [10. Polyphonic Chiptune Synthesizer & Beat Maker](#10-polyphonic-chiptune-synthesizer--beat-maker)
  - [11. Haptic Feedback & Vibration Subsystem](#11-haptic-feedback--vibration-subsystem)
  - [12. WS2812 Tactical RGB LED System](#12-ws2812-tactical-rgb-led-system)
  - [13. Micro-ELF Dynamic App Engine & Q-Apps](#13-micro-elf-dynamic-app-engine--q-apps)
  - [14. Tactical Animation Engine & Player](#14-tactical-animation-engine--player)
  - [15. LittleFS File Manager & Web File Server](#15-littlefs-file-manager--web-file-server)
  - [16. Multi-Tier Power Governor & Battery Cockpit](#16-multi-tier-power-governor--battery-cockpit)
  - [17. System Settings, Wi-Fi Scanner & Captive Portal](#17-system-settings-wi-fi-scanner--captive-portal)
  - [18. Hardware Acceleration: GDMA & Xtensa LX7 SIMD](#18-hardware-acceleration-gdma--xtensa-lx7-simd)
  - [19. Q-Link Android Companion Suite](#19-q-link-android-companion-suite)
- [Controls & Navigation Reference](#-controls--navigation-reference)
- [Project Directory Structure](#-project-directory-structure)
- [Installation & Quick Start](#-installation--quick-start)
  - [1. Installing Precompiled Firmware (Easiest)](#1-installing-precompiled-firmware-easiest)
    - [Method A: 1-Click Web Flasher (Browser-Based)](#method-a-1-click-web-flasher-browser-based)
    - [Method B: Command-Line Flashing via `esptool.py`](#method-b-command-line-flashing-via-esptoolpy)
  - [2. Building & Flashing Firmware from Source (PlatformIO)](#2-building--flashing-firmware-from-source-platformio)
  - [3. Flashing the LittleFS Asset Partition (Sounds, Remotes, Animations)](#3-flashing-the-littlefs-asset-partition-sounds-remotes-animations)
  - [4. Installing the Certified Android Companion App (`Q-Link`)](#4-installing-the-certified-android-companion-app-q-link)
    - [Direct APK Installation](#direct-apk-installation)
    - [Required Android App Permissions](#required-android-app-permissions)
    - [Building Android App from Source (Android Studio / Gradle)](#building-android-app-from-source-android-studio--gradle)
  - [5. First Boot & Network Pairing Walkthrough](#5-first-boot--network-pairing-walkthrough)
    - [On-Watch Direct Wi-Fi Setup](#on-watch-direct-wi-fi-setup)
    - [SoftAP Captive Portal Setup (`192.168.4.1`)](#softap-captive-portal-setup-19216841)
    - [Connecting to the Q-Link Android Companion](#connecting-to-the-q-link-android-companion)
- [Automated Testing & Verification](#-automated-testing--verification)
- [Contributing & License](#-contributing--license)

---

## 🌟 Overview

The **007 Q-Watch** is an open-source, James Bond-inspired wearable computer, tactical gadget, and development cyber-deck built from the ground up on the ultra-compact **ESP32-S3 SuperMini**. Combining high-performance dual-core 32-bit computing, hardware vector SIMD acceleration, high-speed asynchronous GDMA OLED graphics, and an array of tactical sensors and transceivers, Q-Watch bridges the gap between everyday smartwatches and advanced tactical hardware.

### Key Highlights
- **Micro-ELF Dynamic App Store**: Run relocatable C-ABI `.qapp` binaries compiled independently and loaded dynamically into internal SRAM and external PSRAM with zero-leak sandbox protection.
- **Wireless Recon & SIGINT**: On-device 2.4GHz Wi-Fi spectrum analyzer, 802.11 deauthentication/disassociation flood IDS sentry, promiscuous packet monitor, and BLE radar with acoustic Geiger counter proximity ticks.
- **Universal IR Transceiver**: Compatible with Flipper Zero and Bruce `.ir` files (both `raw` and `parsed`), featuring TV-B-Gone, on-device signal learning, multi-button remote cloning, and IR lab carrier diagnostics.
- **Dasai Mochi Pet Companion**: Vector-rendered procedural emote companion (<8KB flash, 140B RAM) with 17 emotional moods, 5 swappable helmet skins (including Gundam RX-78-2), 6-axis IMU reactive physics, and Tamagotchi petting/feeding progression.
- **Chiptune Audio Engine**: 4-channel polyphonic synthesizer with SIMD mixing, 8-step Beat Maker sequencer, built-in chiptune jukebox, metronome, frequency generator, and Morse code engine.
- **On-Device TinyML Gesture AI**: Real-time wrist gesture classification powered by INT8 SIMD-accelerated neural network inference on MPU-6500 6-DOF data.
- **Multi-Tier Power Governor**: 4 dynamic profiles (Performance 240MHz, Balanced 160MHz Light Sleep, Endurance 80MHz Deep Sleep, Custom), 5-page Battery Cockpit HUD, dual hardware EXT1 wakeup, raise-to-wake wrist tracking, and silent background sensor logging.
- **Native Android Companion App**: Cyberpunk OLED Jetpack Compose companion app supporting live display canvas mirroring with virtual D-Pad, sensor telemetry cockpit, OTA `.qapp` sideloading, notification forwarding (SMS/calls/WhatsApp), and atomic time/weather syncing.

---

## 🔧 System Architecture & Hardware Specs

### Hardware Specifications

| Component | Specification |
| :--- | :--- |
| **Microcontroller** | Espressif ESP32-S3FH4R2 (Dual-Core Xtensa LX7 @ up to 240 MHz) |
| **Memory** | 4MB Quad-SPI Flash, 2MB Quad-SPI PSRAM, 512KB SRAM |
| **Display** | 1.3-inch 128x64 Monochrome White OLED (SH1106 controller) |
| **Display Bus** | 4-Wire Hardware SPI + Dedicated ESP32-S3 GDMA Pipeline |
| **Atmospheric Sensor** | Bosch BME280 (Temperature, Relative Humidity, Barometric Pressure) |
| **Motion Tracking** | InvenSense MPU-6500 (6-DOF: 3-Axis Gyroscope + 3-Axis Accelerometer) |
| **Magnetometer** | QST QMC5883P (3-Axis Digital Magnetic Compass) |
| **Biometric Sensor** | SparkFun / Maxim MAX30102 (Photoplethysmogram Pulse & SpO2 Oximeter) |
| **Infrared Subsystem** | 38 kHz Demodulated IR Receiver + High-Power IR Blaster LED (LEDC Carrier) |
| **Audio Engine** | Piezoelectric Buzzer driven via N-MOSFET / BJT with 4-Channel Polyphonic Synth |
| **Haptic Feedback** | Eccentric Rotating Mass (ERM) Vibration Motor via LEDC Hardware PWM |
| **Visual Lighting** | Onboard WS2812 Addressable RGB LED (FastLED Engine) |
| **Input Interface** | K1 3-Way Rocker (UP, OK, DOWN) + Dedicated Tactile CANCEL / Wake Button |
| **Power Management** | Single-Cell LiPo / Li-ion Battery with 100k/100k ADC Voltage Divider |
| **Filesystem** | Partitioned LittleFS Flash Storage (Configs, History, Apps, Sounds, Remotes) |

---

### Pinout & GPIO Allocation

The ESP32-S3 SuperMini pinout has been engineered to prevent conflicts with internal Octal/Quad Flash and PSRAM lines while avoiding bootstrapping pins (GPIO 0, 3, 45, 46).

```
                 ESP32-S3 SuperMini
                   +-------------+
        BAT_ADC <--| GPIO 1   5V |-- Power In
        OLED_DC <--| GPIO 2  GND |-- Ground
        OLED_CS <--| GPIO 4  3V3 |-- 3.3V Rail
      OLED_MOSI <--| GPIO 5   10 |-- VIBE_PWM (Haptic Motor)
         BUZZER <--| GPIO 6    8 |-- MPU_INT (Motion Wake)
       OLED_SCK <--| GPIO 7    9 |-- [Reserved Outer]
       OLED_RST <--| GPIO 41  11 |-- [Reserved Outer]
        BTN_DWN <--| GPIO 42  12 |-- [Reserved Outer]
         BTN_OK <--| GPIO 40  13 |-- [Reserved Outer]
         BTN_UP <--| GPIO 39  15 |-- I2C_SDA (Shared Bus)
     BTN_CANCEL <--| GPIO 21  16 |-- I2C_SCL (Shared Bus)
          IR_TX <--| GPIO 18  17 |-- IR_RX (Demodulated 38kHz)
                   +-------------+
                    [Bottom Pad]
                      GPIO 38 (Clean Reserve)
                      GPIO 48 (Onboard WS2812 RGB LED)
```

#### Detailed Pin Mapping Table

| Subsystem | Signal Name | GPIO | Type / Mode | Description & Electrical Notes |
| :--- | :--- | :---: | :--- | :--- |
| **OLED Display** | `OLED_MOSI` | **5** | SPI Master Out | Hardware SPI Data line (GDMA driven) |
| | `OLED_SCK` | **7** | SPI Clock | Hardware SPI Clock line (GDMA driven) |
| | `OLED_CS` | **4** | Digital Output | Display SPI Chip Select (Active Low) |
| | `OLED_DC` | **2** | Digital Output | Display Data / Command Select |
| | `OLED_RST` | **41** | Digital Output | Display Hardware Reset (Active Low) |
| **I2C Sensor Bus** | `I2C_SDA` | **15** | Open-Drain / Pullup | Shared I2C Serial Data Bus |
| | `I2C_SCL` | **16** | Open-Drain / Pullup | Shared I2C Serial Clock Bus |
| | `MPU_INT` | **8** | Input / RTC Wake | MPU-6500 Wake-on-Motion interrupt (Active Low) |
| **Buttons & Input**| `BTN_UP` | **39** | `INPUT_PULLUP` | K1 Multi-directional UP button |
| | `BTN_OK` | **40** | `INPUT_PULLUP` | K1 Multi-directional OK / Select button |
| | `BTN_DOWN` | **42** | `INPUT_PULLUP` | K1 Multi-directional DOWN button |
| | `BTN_CANCEL` | **21** | `INPUT_PULLUP` | Dedicated CANCEL button & ESP32-S3 `EXT1` Deep Sleep Wake |
| **Infrared** | `IR_RX` | **17** | Digital Input | 38 kHz demodulated infrared receiver data |
| | `IR_TX` | **18** | LEDC PWM Output | High-power IR blaster LED driver |
| **Audio & Haptic** | `BUZZER` | **6** | LEDC PWM Output | Piezo buzzer driver (LEDC Timer 0 / Channel 0) |
| | `VIBRATION` | **10** | LEDC PWM Output | ERM vibration motor driver (LEDC Timer 1 / Channel 2, 200 Hz) |
| **RGB Lighting** | `WS2812_DATA` | **48** | FastLED Digital | Onboard addressable RGB LED data line |
| **Power Telemetry**| `BAT_ADC` | **1** | Analog Input (ADC1) | 100kΩ / 100kΩ voltage divider from raw battery rail |
| **UART / Debug** | `UART0_TX` | **43** | Serial Out | Hardware UART0 Transmit (Clean Reserve) |
| | `UART0_RX` | **44** | Serial In | Hardware UART0 Receive (Clean Reserve) |
| **Clean Reserves** | `RESERVE` | **9, 11, 12, 13, 38** | Digital I/O | Header & underside solder pads reserved for expansion |

---

### I2C Address Map

All four environmental, motion, and biometric sensors share the single high-speed I2C bus on **GPIO 15 (SDA)** and **GPIO 16 (SCL)**:

| Sensor Module | Model | 7-Bit I2C Address | Features Provided |
| :--- | :--- | :---: | :--- |
| **Atmospheric Sensor** | Bosch BME280 | `0x76` | Barometric pressure, humidity, ambient temperature, altimeter |
| **Motion Tracking** | InvenSense MPU-6500 | `0x68` | 3-axis accelerometer, 3-axis gyroscope, wake-on-motion interrupt |
| **3D Magnetometer** | QST QMC5883P | `0x0D` | 3-axis magnetic compass heading, azimuth calculation |
| **Pulse Oximeter** | Maxim MAX30102 | `0x57` | Photoplethysmogram (PPG) pulse wave, heart rate BPM, SpO2 % |

---

### Electrical & Circuit Design Notes

1. **I2C Pull-Up Resistor Optimization**:
   Connecting four independent sensor breakout boards in parallel results in multiple onboard pull-up resistors in parallel, dropping the bus pull-up resistance down to ~1 kΩ or less and causing bus contention. **Recommendation:** Physically desolder the SMD pull-up resistors from 2 or 3 of the breakout boards, maintaining a single clean 4.7 kΩ pull-up on the SDA and SCL lines.
2. **Decoupling Capacitors (High-Current Pulsed Loads)**:
   The **IR Transmitter LED** and **Piezo Buzzer** create heavy microsecond current spikes. Place a `100nF (104)` ceramic capacitor in parallel with a `10µF` tantalum or electrolytic capacitor directly across the driver rail to prevent microcontroller reset under pulse load.
3. **Vibration Motor Driver Circuit**:
   Never connect an ERM vibration motor directly to an ESP32-S3 GPIO pin. Drive GPIO 10 through a 330 Ω base resistor into an NPN transistor (e.g. 2N2222A) or N-channel MOSFET, with a 10 kΩ pulldown resistor on the gate/base, and place a `1N4148` or `1N5819` Schottky flyback diode in reverse parallel across the motor terminals to clamp inductive voltage spikes.
4. **Battery Voltage Divider**:
   Pin **GPIO 1** connects to the midpoint of two 100 kΩ 1% metal-film resistors bridging the raw LiPo battery rail (`VBAT`) to ground. A `100nF` ceramic capacitor placed between GPIO 1 and GND filters out high-frequency ADC switching noise.

---

## 🚀 Comprehensive Feature Breakdown

---

### 1. Watch Faces & Timekeeping Suite

Q-Watch features four distinct switchable watch faces and a full suite of precision horological tools.

#### Switchable Watch Faces
- **Digital Tactical Face**: Clean, high-legibility display featuring large 24-hour/12-hour time, running seconds, date, battery percentage, real-time weather temperature widget, pedometer step counter, and status indicator icons (Wi-Fi `W`, Alarms `A`, Hourly Chime `C`).
- **Analog Dial Face**: Classic 12-hour tick dial with accentuated quarter markers (12, 3, 6, 9), floating center hub, trigonometric hour/minute/second hands computed via the Xtensa hardware FPU (`cosf()`/`sinf()`), and flanking date/battery telemetry windows.
- **Retro Casio LCD Face**: Nostalgic double-frame vintage digital watch border featuring a top Day-of-Week selector pill matrix (`SU MO TU WE TH FR SA`) highlighting the active day with an inverted box, `CHI`/`ALM` status flags, bracketed seconds counter, and bottom telemetry bar.
- **Tactical / Mission Face**: Military HUD interface integrating live QMC5883P compass heading (`HDG 000° M`), BME280 barometric altitude (`ALT 000m`), 24-hour military timestamp, step progress bar against daily goal, battery gauge, and mission stopwatch indicator.

*Quick Switching*: Press `[UP]` or `[DOWN]` on the `HOME` screen to immediately cycle through all four watch faces. Preferences are automatically saved to LittleFS.

#### Clock Suite Applications (`MAIN MENU -> CLOCK`)
- **Tactical Stopwatch (Chronograph)**: High-precision stopwatch with millisecond readout (`MM:SS.hh`) and split lap recording for up to 8 laps. `[OK]` starts and records split laps, `[UP]` pauses and resumes, and `[DOWN]` resets.
- **Countdown Timer**: Configurable countdown timer with quick presets (1m, 3m, 5m, 10m, 15m, 30m, 60m), real-time graphical progress bar, and an audible alert sequence on expiration.
- **Multi-Alarms**: 3 independent daily alarms stored persistently in `/config/alarms`. Features an interactive hour/minute selector, audible ringing melody, 5-minute snooze, and one-touch dismiss.
- **Hourly Chime**: Configurable audio chime on every hour (`:00:00`) with a distinct dual high-frequency tone.
- **World Clock**: 12 world cities and military hubs (Kolkata, UTC, London, Berlin, New York, Chicago, Denver, Los Angeles, Dubai, Singapore, Tokyo, Sydney) with precise quarter-hour timezone calculations and relative day offsets (`+1 DAY`, `SAME DAY`, `-1 DAY`).
- **Pedometer & Step Goal**: Real-time MPU-6500 accelerometer magnitude step detector with threshold hysteresis and debouncing. Computes estimated distance (`km`), calories burned (`kcal`), step goal progress, and on-demand reset.
- **Manual Clock Setting**: Allows setting the time, date, and year manually without an active Wi-Fi or NTP connection.

---

### 2. Environmental & Atmospheric Weather Station

Powered by the onboard **Bosch BME280** sensor and the **OpenWeatherMap HTTPS API**, Q-Watch provides comprehensive local and remote weather tracking.

#### Applications & Views (`APP_WEATHER` & `BME280 App`)
- **Live Environmental Readings**: Real-time barometric pressure in hectopascals (hPa), relative humidity percentage (%RH), and ambient temperature in Celsius (°C).
- **Barometric Altimeter**: Pressure-based relative altitude measurement state machine. Allows users to zero their current altitude at ground level and track relative climbs or descents in real time.
- **OpenWeatherMap HTTPS Integration**: Fetches encrypted remote weather data over TLS when Wi-Fi is connected, displaying temperature, "feels like" temperature, humidity, wind speed, and meteorological condition strings.
- **Cute Animated Weather Graphics**: Procedural 1-bit animated weather icons for Clear/Sunny, Clouds, Rain, Thunderstorm, and Mist/Fog, plus an offline animation placeholder.
- **24-Hour Historical Trend Graphs**: 288-record binary ring-buffer (`/bme_history.bin`) logging environmental metrics at user-defined intervals (5m, 10m, 15m, 30m, 1h). The watch renders auto-scaled rolling 24-hour graphical trend charts for temperature, pressure, and humidity.
- **Interactive Calibration**: Real-time temperature offset adjustment and reference ground sea-level pressure calibration saved in persistent NVS Preferences.

---

### 3. Cardiorespiratory Health & Biometrics

Equipped with the **Maxim MAX30102** photoplethysmogram (PPG) optical sensor, Q-Watch serves as an on-wrist biometric monitoring station.

#### Applications & Views (`APP_HEALTH`)
- **Real-Time Pulse Oximeter**: Live heart rate detection (BPM) and blood oxygen saturation (SpO2 %) rendered alongside an interactive SpO2 progress bar gauge.
- **Live PPG Pulse Waveform Visualizer**: Dynamic 64-sample circular waveform visualizer rendering raw optical absorption data filtered in real time by an Xtensa LX7 SIMD 32-tap digital FIR bandpass filter.
- **Sensor Die Temperature**: Measures MAX30102 internal die temperature for calibration and thermal compensation.
- **Calendar-Day Trend History**: Historical trend graphs for Heart Rate, SpO2, and sensor temperature, automatically filtered to the current local calendar day (`localtime_r`). Displays latest, minimum, maximum, and average statistics.
- **Dynamic Optical Power Lifecycle**: Automatically powers down the MAX30102 red and infrared LEDs when navigating away from the health screen to eliminate unneeded power draw.
- **Finger Contact Detection**: Smart optical threshold detection that pauses calculation and alerts the user when no finger or wrist contact is present.

---

### 4. Navigation, Motion & 3D Vector Graphics

Integrating the **QMC5883P 3-Axis Magnetometer** and **MPU-6500 6-Axis IMU**, Q-Watch delivers tactical situational awareness and 3D graphics rendering.

#### Subsystems & Views (`APP_COMPASS`, `APP_MOTION`)
- **Tactical Compass HUD (`APP_COMPASS`)**:
  - Full-screen rotating North-seeking compass rose and needle.
  - Real-time magnetic heading in degrees (`000°`) and 8-point cardinal direction (`N`, `NE`, `E`, `SE`, `S`, `SW`, `W`, `NW`).
  - Waypoint bearing lock with course deviation indicator (CDI) showing degrees off-course.
  - Interactive magnetic declination offset adjustment for true geographic North alignment.
- **Artificial Horizon & Attitude Indicator (`SUBAPP_ALTIMETER`)**:
  - Aircraft-style tactical artificial horizon displaying real-time pitch and roll angles.
  - Digital bubble level indicator with numerical degree readouts for precision leveling.
- **Interactive 3D Mount Orientation Engine**:
  - Solves sensor placement discrepancies by projecting 3D wireframe axes directly on the 128x64 OLED display.
  - Provides 8 selectable orthogonal mounting presets (including inverted-Z, upside-down, and 90° rotations) with persistent saving across all apps and watch faces.
- **Real-Time 3D Wireframe Graphics Engine (`SUBAPP_WIREFRAME_3D`)**:
  - Hardware-accelerated 3D vector pipeline with real-time perspective division and 3D polygon clipping.
  - Renders rotating 3D wireframe geometric models (Cube, Pyramid, Torus, Spacecraft).
  - Integrates with the MPU-6500 IMU, allowing users to rotate 3D objects in physical space by tilting the watch.

---

### 5. BLE Air Mouse Remote

Turn the Q-Watch into a wireless Bluetooth Low Energy (BLE) HID air mouse remote for laptops, smart TVs, tablets, and phones.

#### Features & Controls (`MAIN MENU -> MOTION -> AIR MOUSE`)
- **Native BLE HID Stack**: Uses ESP32-S3 `BLEHIDDevice` to present as a standard Bluetooth mouse. Operates strictly when the app is active and completely de-initializes (`BLEDevice::deinit(false)`) upon exit to maximize battery life.
- **Calibrated Gyroscope Control Pipeline**: Maps angular velocity from the MPU-6500 directly to cursor coordinates, featuring:
  - Adjustable dead-zone suppression (0.5°/s to 10.0°/s) to eliminate resting hand tremors.
  - Ramped anti-dead-zone compensation (0.0°/s to 3.4°/s) for smooth micro-movements.
  - 6-DOF true zero-drift auto-tracking and adaptive velocity smoothing.
  - Non-linear power-curve acceleration.
  - Remainder-preserving integer accumulation clamped to HID bounds (-127 to +127).
- **Dual Operational Modes**:
  - **`POINTER` Mode**: Wrist motion moves the pointer. `[UP]` = Left Click & Hold (drag and drop / text selection); `[DOWN]` = Right Click & Hold; `[OK]` = Back Click.
  - **`SCROLL` Mode**: `[UP]` = Scroll Up; `[DOWN]` = Scroll Down; `[OK]` = Back Click.
  - `[CANCEL]` toggles instantly between `POINTER` and `SCROLL` modes.
- **In-Situ Settings & Precision Mode**: Long-pressing `[OK]` pauses cursor tracking, opening an in-situ tuning menu to adjust sensitivity, dead zones, axis swapping, gyro recentering, or toggle **Precision Mode** (1:1 linear tracking, 0.35x micro-speed, heavy tremor filtering for CAD and pixel art).

---

### 6. On-Device TinyML Gesture AI

Run real-time machine learning inference directly on the watch to classify dynamic wrist gestures.

#### Capabilities (`MAIN MENU -> MOTION -> GESTURE AI`)
- **Quantized INT8 Neural Network**: Compact multi-layer perceptron (MLP) trained on 6-axis accelerometer and gyroscope time-series data.
- **SIMD-Accelerated Inference**: Leverages the ESP32-S3 Xtensa LX7 PIE vector instructions to compute dot products and activations with near-zero latency.
- **Real-Time Classification**: Identifies distinct wrist gestures:
  - *Flick Up*
  - *Flick Down*
  - *Twist Left*
  - *Twist Right*
  - *Shake / Tremor*
- **Live Telemetry HUD**: Displays real-time confidence bars, detected gesture class, and inference duration in milliseconds.

---

### 7. Tactical Wireless Recon & SIGINT Suite

Transform the Q-Watch into a wrist-mounted signals intelligence (SIGINT) unit for network analysis and attack detection.

#### Applications (`SYS MENU -> WIRELESS`)
- **BLE Radar (`BLE RADAR`)**:
  - Rotating 52px tactical reticle with a continuous sweep line.
  - Computes real-time target distance using log-distance path loss estimation:
    $$d \approx 10^{\frac{A - \text{RSSI}}{10n}} \quad (A = -59\text{ dBm}, n = 2.4)$$
  - Directional target lock brackets and live RSSI signal strength gauge.
  - **Acoustic Geiger Counter**: Dynamic audio ticks from the piezo buzzer that shorten in interval as you get closer to the target (from 1200ms at range down to 60ms at point-blank proximity).
- **Wi-Fi Channel Spectrum Analyzer (`CH SPECTRUM`)**:
  - Sweeps all 13 channels across the 2.4GHz ISM band.
  - Renders a live access point density histogram on the OLED.
  - Highlights non-overlapping channels (1, 6, 11) and automatically calculates the cleanest channel (`REC: CH X`).
- **802.11 Deauth / Disassociation Attack Detector (`DEAUTH DETECT`)**:
  - Promiscuous 802.11 IDS sentry listening for deauthentication (`0x00C0`) and disassociation (`0x00A0`) management frame floods.
  - Calculates attack flood rates (deauths/sec), identifies the attacking transmitter MAC and victim station MAC, and extracts 802.11 reason codes.
  - Triggers flashing visual alerts and audible sirens upon detecting an active attack.
- **Promiscuous Packet Monitor (`PKT MONITOR`)**:
  - Non-blocking 802.11 air traffic monitor displaying live packets/second rate.
  - 64-sample scrolling histogram rate graph and cumulative packet counters.
  - Real-time percentage breakdown and multi-segment bar displaying Management, Control, and Data frame ratios.

---

### 8. Flipper Zero & Bruce-Compatible IR Remote Subsystem

A full-featured infrared transceiver compatible with community remote databases.

#### Capabilities (`APP_IR`)
- **Flipper Zero & Bruce `.ir` File Compatibility**: Complete parser and serializer for standard `.ir` library files stored in `/ir/` on LittleFS. Supports both `type: parsed` (protocol, address, command, bit length) and `type: raw` (carrier frequency, duty cycle, microsecond pulse sequences).
- **Multi-Protocol Engine**: 100% round-trip fidelity for `NEC`, `NECext`, `NEC42`, `Samsung32`, `RC5`, `RC5X`, `RC6`, and Sony `SIRC` (12-bit, 15-bit, 20-bit).
- **Signal Learning & Capture**: Demodulated 38 kHz receiver on **GPIO 17** captures incoming signals in real time, automatically decoding protocol, address, and command, or recording raw microsecond pulse timings up to 1024 edges.
- **Quick Remote Cloning**: Interactive on-watch remote builder: Name Remote → Name Button → Learn Signal → Test Transmission → Save Button → Add Buttons → Save to `.ir` file.
- **TV-B-Gone Power Blaster**: Transmits universal power codes for hundreds of television models in rapid succession with an animated status display and instant cancellation via `[CANCEL]`.
- **IR Signal Lab**: Generates 36 kHz, 38 kHz, and 40 kHz PWM carrier waves to test transmitters and analyze raw-to-parsed protocol decoding.

---

### 9. Dasai Mochi Procedural Emote Pet Companion

An ultra-lightweight procedural vector animation pet inspired by Dasai Mochi.

#### Features (`APP_MOCHI`)
- **Ultra-Compact Procedural Vector Engine**: Consumes **< 8 KB of flash** and **only 140 bytes of RAM**, avoiding bloated GIF frame arrays.
- **17 Expressive Moods & Emotes**:
  - *Positive*: `HAPPY` (crescent eyes, blushing cheeks), `LOVE` (pulsing heart eyes, love chime), `LAUGH` (squinting `><` eyes), `EXCITED` (energetic hop), `CONTENT`, `RELAXED`, `PROUD`.
  - *Reactive*: `ANGRY` (slanted brows, red LED, grumble), `FRUSTRATED` (sweat drop), `CONFUSED` (`?` marker), `EMBARRASSED` (hatching blush `///`), `DIZZY` (spinning Archimedean spiral eyes).
  - *Ambient*: `SLEEPY` (drooping eyelids), `SLEEPING` (floating `Zzz`, slow breathing), `DRIVING` (racing visor, wind streaks), `MUSIC` (head bobbing, eighth-notes), `GUNDAM` (combat visor optics).
- **5 Swappable Modular Digital Helmets**:
  - `CLASSIC`: Smooth rounded helmet with aerodynamic intake.
  - `GUNDAM`: RX-78-2 Mobile Suit helmet with V-fin crest, forehead sensor gem, cheek vents, and red chin tab.
  - `CYBER`: Cyberpunk edition with corner brackets and telemetry lines.
  - `NEKO`: Cat ears with inner notch geometry.
  - `TACTICAL`: 007 Agent night-vision reticle bezel.
- **6-Axis IMU Reactive Physics**:
  - *Gaze Tracking*: Pupils smoothly follow wrist tilt ($dx \in [-7, 7]$, $dy \in [-5, 5]$).
  - *Cornering Lean*: Body tilts into turns during movement.
  - *High-G Shake*: Shaking the watch (>2.2G) triggers dizzy eyes and stumble tones.
  - *Freefall Detection*: Zero-G drops (<0.25G) trigger panic expressions.
  - *Idle Timer*: Stationary inactivity (>25s) transitions Mochi into a yawn and slumber (`Zzz`).
- **Tamagotchi Petting & Feeding Mechanics**:
  - Short `[OK]`: Pet Mochi (heart burst, happiness boost, XP gain, arpeggio chime).
  - Long `[OK]`: Feed Mochi (animated onigiri snack, chewing animation, restores hunger).
  - Friendship Level progression (Level 1–10) with persistent saving.
  - Short `[CANCEL]`: Toggles between full-screen face and the Stats Cockpit (Happiness, Hunger, Friendship, Battery, Steps).

---

### 10. Polyphonic Chiptune Synthesizer & Beat Maker

A modular chiptune music workstation and tactical audio engine.

#### Features (`APP_AUDIO`)
- **4-Channel Polyphonic Synthesizer (`poly_synth`)**:
  - 4 independent voices mixed in real time using SIMD saturated arithmetic.
  - Waveform generators: Square (variable duty cycle), Sawtooth, Triangle, Sine, and Noise.
  - Configurable Attack-Decay-Sustain-Release (ADSR) volume envelopes.
- **8-Step Beat Maker Sequencer**:
  - Visual 8-step rhythm grid editor on the 128x64 display.
  - Programmable kick, snare, hi-hat, and synth notes.
  - Real-time tempo adjustment (BPM) and continuous looping playback.
- **Jukebox & Chiptune Player**:
  - Preloaded with 8-bit chiptune arrangements, including the iconic James Bond 007 theme and retro arcade fanfares.
- **Tactical Sound Lab & Utilities**:
  - Frequency tone generator (1 Hz to 20 kHz).
  - Visual pendulum metronome.
  - Morse code transmitter (`.--.`, `--.-`, etc.) with synchronized audio and screen flash.

---

### 11. Haptic Feedback & Vibration Subsystem

Tactile haptic feedback powered by an ERM vibration motor on **GPIO 10**.

#### Subsystems (`APP_VIBRATION`)
- **Hardware PWM Driver**: Uses ESP32-S3 LEDC Timer 1 / Channel 2 running at 200 Hz with 8-bit duty cycle resolution, preventing timer collisions with buzzer audio.
- **Asynchronous Pattern Sequencer**:
  - `CLICK / TICK`: Crisp 60ms tactile click for UI button navigation.
  - `DOUBLE PULSE`: Two 50ms pulses with 70ms pause for smartphone notifications.
  - `TACTICAL ALERT`: 220ms continuous pulse for alarms and countdown timers.
  - `HEARTBEAT`: Biometric lub-dub cardiac rhythm simulation.
  - `SOS MORSE`: Standard `··· ——— ···` emergency sequence.
  - `RAMP INTENSITY`: Dynamic acceleration ramp from 20% to 100%.
- **Interactive Haptic Lab App**:
  - Live oscillating waveform visualizer on the OLED with `[VIB]` / `[IDLE]` state indicators.
  - Master vibration toggle and button-click haptic toggle.
  - Variable intensity adjustment (25%, 50%, 75%, 100%).

---

### 12. WS2812 Tactical RGB LED System

Visual status indicators driven by the onboard addressable RGB LED on **GPIO 48**.

#### Modes (`APP_LED`)
- **Lighting Effects**: Solid Color, Gentle Pulse, Rainbow Cycle, High-Visibility Strobe, and Tactical Beacon.
- **Color Presets**: Red, Green, Blue, Amber, Cyan, Purple, and White.
- **360° Compass Hue Blending**: Dynamically blends LED color continuously across the color wheel based on QMC5883P compass heading.
- **Brightness Scaling**: Variable duty cycle scaling from 10% to 100% to conserve battery.

---

### 13. Micro-ELF Dynamic App Engine & Q-Apps

Execute relocatable binary applications from LittleFS storage without reflashing firmware.

#### Engine Architecture (`qapp_loader`)
- **Dual-Segment Relocatable Binary Format (`.qapp`)**: Separates executable code loaded into internal SRAM (`MALLOC_CAP_EXEC | MALLOC_CAP_INTERNAL`) from initialized data, BSS, and heap memory allocated in external 2MB PSRAM (`MALLOC_CAP_SPIRAM`), resolved via relocation tables (`QAppReloc`).
- **Sandboxed C-ABI Export (`qwatch_api.h`)**: Provides applications with hardware-abstracted access to OLED drawing routines, 4-button input events, 6-axis IMU, 3D magnetometer, MAX30102 biometrics, IR transceivers, audio synthesis, RGB LED, and LittleFS file I/O.

#### Pre-Loaded Applications & Games (`apps/`)

| Application | Binary File | Description |
| :--- | :--- | :--- |
| **007 Space Invaders** | `invaders.qapp` | Classic arcade shooter with button or wrist-tilt steering, 3 alien rows, destructible bunkers, mystery SPECTRE UFO, and high-score saving. |
| **Tactical Dice & TRNG** | `dice.qapp` | High-entropy random number generator combining hardware TRNG, timers, and IMU jitter. Supports D6, D20 (with Nat 20/Nat 1 banners), D100, Coin flip, and shake-to-roll physics. |
| **Tilt Ball** | `tilt_ball.qapp` | 6-axis IMU roll/pitch physics simulation with bounce damping, target collision, and audio chimes. |
| **Compass HUD** | `compass_hud.qapp` | Tactical navigation HUD with rotating needle, waypoint bearing lock, and course deviation indicator. |
| **Retro Snake** | `snake.qapp` | Nokia-style snake game with 4-way steering, food spawning, growth mechanics, and high-score persistence. |
| **F1 Grand Prix** | `f1_race.qapp` | Top-down high-speed racer with multi-lane traffic avoidance, boost mechanics, and engine audio. |
| **Pacman Arcade** | `pacman.qapp` | Authentic arcade maze chase with power pellets, ghost AI pathfinding, and fruit scoring. |
| **Breakout 007** | `breakout.qapp` | Brick-busting platformer with paddle bounce physics, laser power-ups, and multi-ball action. |
| **Space Impact 2** | `space_impact.qapp` | Side-scrolling space shooter with delta fighter, laser blasts, screen-clearing bombs, and boss battles. |
| **Nokia Bounce** | `bounce.qapp` | Platformer with bouncing ball physics, gravity restitution, obstacle avoidance, and rotating ring collection across 3 levels. |

---

### 14. Tactical Animation Engine & Player

A lightweight streaming 1-bit animation player for boot splashes, UI effects, and visual clips.

#### Capabilities (`APP_ANIM_PLAYER`)
- **Proprietary Streaming Binary Format (`.anim`)**: 16-byte packed header with sequential 1024-byte uncompressed 1-bit XBM frames streamed straight from LittleFS without RAM buffering.
- **Windows Monochrome BMP Compatibility**: Decodes standard 1-bit monochrome `.bmp` files on the fly with hardware bit-reversal.
- **Interactive Player Controls**:
  - `[OK]` Short Press: Play / Pause.
  - `[OK]` Long Press: Set current animation as the system boot splash (`/boot/boot.anim`).
  - `[UP]` Short Press: Cycle speed (`0.5x` -> `1.0x` -> `1.5x` -> `2.0x`) when playing, or step forward 1 frame when paused.
  - `[DOWN]` Short Press: Toggle looping (`LOOP: ON/OFF`) when playing, or step backward 1 frame when paused.
  - `[CANCEL]`: Exit player.
- **Custom Boot Splash**: Cold boot plays `/boot/boot.anim` or `/boot/boot.bmp`, or falls back to the MI6 tactical radar sweep. Pressing any button instantly skips the boot animation.
- **CLI Packing Tool (`tools/anim_packer.py`)**: Python tool to convert GIF, MP4, and image sequences into `.anim` files with Bayer 4x4 spatial dithering.

---

### 15. LittleFS File Manager & Web File Server

Manage configurations, logs, animations, and applications directly on the watch or over Wi-Fi.

#### Features (`APP_FILE_MANAGER`)
- **On-Watch OLED Browser**: Native 3-item scrolling viewport displaying filenames, file sizes, and total/free storage. Includes a context menu (`[OK]` long press) to open, delete, or inspect file properties.
- **Direct Viewer Associations**: Selecting `.ir` files launches the IR Remote viewer; selecting `.anim` or `.bmp` launches the Animation Player; selecting `.qapp` executes the dynamic binary loader.
- **Opt-In Web File Server**: Enable via `SETTINGS -> CONNECTIVITY -> FILE SERVER` and visit `http://q-watch.local/fm` on any browser:
  - Dynamic folder navigation and file downloading.
  - Multipart HTTP file uploading.
  - File deletion and folder creation.
  - Protected with strict path traversal sandboxing (`FileManager::isPathSafe`) to prevent directory traversal attacks (`../`, `//`).

---

### 16. Multi-Tier Power Governor & Battery Cockpit

Sophisticated power lifecycle management designed to maximize battery life across operating conditions.

#### Dynamic Power Profiles (`APP_BATTERY`)
- **`PERFORMANCE` ("Tactical Max")**: 240 MHz dual-core CPU clock, display-off idling with FreeRTOS `waiti` yielding, zero sleep latency, high-performance radios and sensors (~12–16 hrs runtime).
- **`BALANCED` ("Everyday Smart")**: Dynamic 160 MHz CPU clock, ESP32-S3 Light Sleep with 300µs wake latency, 24/7 background pedometer tracking, automatic radio sleep (~3–5 days runtime).
- **`ENDURANCE` ("Ultra Saver")**: 80 MHz eco CPU clock, Deep Sleep down to 15µA quiescent draw, automatic radio kill, RGB LED and buzzer muting (~15–20 days runtime).
- **`CUSTOM` ("Power Lab")**: Granular user configuration of CPU clocks, sleep engines, radio power-down policies, and low-voltage cutoff guards.

#### 5-Page Battery Cockpit HUD
- **Page 1 (HUD)**: Battery level gauge, real-time voltage (`3.92V`), percentage (`84%`), dynamic runtime estimate (`EST: 4.2d`), active profile badge, and manual ADC refresh via `[OK]`.
- **Page 2 (Profiles)**: 1-click profile switcher.
- **Page 3 (Sleep Engine)**: Configures sleep engines (`LIGHT SLEEP`, `DEEP SLEEP`, `DISPLAY OFF`, `ULP SENTRY`), pedometer toggle, and raise-to-wake gesture sensitivity.
- **Page 4 (Load Shedding)**: Subsystem power cuts (Radio Cut on lock, RGB LED mute, Audio mute, low-battery cutoff at `3.20V`).
- **Page 5 (ULP Sentinel Lab)**: Telemetry HUD displaying ULP RISC-V coprocessor execution status, RTC SLOW RAM allocation (8192 bytes), and wake trigger tallies.

#### Wakeup & Background Logging
- **Dual Hardware Wakeup**: ESP32-S3 `EXT1` active-low wakeup on **GPIO 21** (`BTN_CANCEL`) and **GPIO 8** (`MPU_INT` motion interrupt).
- **Raise-to-Wake Gesture**: Evaluates wrist angle before sleep (`pitch 15°..65°`, `|roll| <= 35°`).
- **Silent Background Sleep Recording**: When waking from deep sleep on an RTC timer event, the watch samples the BME280 or MAX30102, appends data to LittleFS, and returns immediately to deep sleep without powering the OLED display or playing sound.

---

### 17. System Settings, Wi-Fi Scanner & Captive Portal

Centralized configuration hub for device setup, network credentials, and system maintenance.

#### Settings Control Center (`APP_SETTINGS`)
- **`CONNECTIVITY`**: Wi-Fi status, Wi-Fi Scanner, Web File Server toggle, BLE status.
- **`TIME`**: NTP Sync Now, Auto Sync toggle, POSIX Timezone configuration, 12h/24h toggle, Manual Time Set.
- **`POWER`**: Display timeout, sleep profiles, raise-to-wake sensitivity, load shedding.
- **`DISPLAY`**: Contrast adjustment, screen inversion, UI transition effects.
- **`SENSORS`**: Auto Data Recording master switch, Compass 3D Mount Orientation, IMU Calibration, Health Recording Interval.
- **`SYSTEM`**: Storage Info, Firmware Version, Factory Reset (with confirmation dialog).

#### Wi-Fi Scanner & Captive Portal
- **On-Watch Wi-Fi Scanner**: Scan local 2.4GHz networks directly on the watch, select an SSID, and input passwords using the on-screen keyboard.
- **SoftAP Captive Portal**: If Wi-Fi is unconfigured, the watch broadcasts a `Q-Watch-Setup` network accessible at `http://192.168.4.1` (or `http://q-watch.local` via mDNS). Static assets (HTML, CSS, JS, SVG) are stored in `PROGMEM`, saving over 14 KB of heap RAM for networking and app execution.

---

### 18. Hardware Acceleration: GDMA & Xtensa LX7 SIMD

Q-Watch takes full advantage of the ESP32-S3 hardware architecture for fluid performance.

- **ESP32-S3 GDMA Asynchronous Display Pipeline**:
  Transfers 128x64 display frames from memory to the SH1106 controller over SPI using General Direct Memory Access (GDMA). The CPU initiates the transfer and immediately yields, eliminating blocking waits during frame rendering.
- **Xtensa LX7 128-Bit SIMD Framebuffer Operations**:
  Uses Xtensa LX7 PIE vector opcodes (`ee.notq`, `ee.xorq`, `ee.andq`, `ee.orq`) to process 16 bytes (128 pixels) per instruction for instant screen inversion, XOR mask blitting, and UI transitions.
- **Hardware-Vectorized 32-Tap FIR Filter**:
  A dual-accumulator SIMD FIR bandpass filter accelerates biometric pulse oximetry and PPG wave filtering for the MAX30102 sensor.
- **6-DOF IMU Parallel Vector Filter**:
  Smooths accelerometer and gyroscope channels simultaneously using SIMD vector instructions.

---

### 19. Q-Link Android Companion Suite

A native Android companion application written in **Kotlin** and **Jetpack Compose**, communicating over BLE GATT (`0xFE50`) and Wi-Fi REST.

#### Companion Features (`android/`)
- **Live 128x64 OLED Canvas Mirroring**: Real-time rendering of the watch's display buffer on the phone with selectable phosphor glow colors (Cyan, Amber, Green, White), screenshot export, and an interactive virtual D-Pad (`UP`, `OK`, `DOWN`, `BACK`) for full remote control.
- **Sensor Telemetry Cockpit**: Live readouts for the BME280 weather station, MPU-6500 artificial horizon (with live pitch/roll angles), QMC5883P rotating compass rose, and MAX30102 cardiorespiratory biometrics.
- **Micro-ELF App Store**: Browse and install `.qapp` binaries directly to the watch over Wi-Fi (`POST /api/v1/app/install`) with a single tap.
- **Online IRDB & Adaptive Remote**: Access a cloud database of Bruce and Flipper Zero `.ir` remotes and transmit codes through the watch.
- **Animation Market & Studio**: Preview, convert, and upload animations (`.anim`, `.bmp`, GIFs) directly to `/anim` or `/mochi` on the watch.
- **Phone Notification Forwarding**: Android `NotificationListenerService` forwards incoming SMS, WhatsApp messages, and incoming calls to the watch, triggering custom vibration pulses and RGB LED flashes.
- **Atomic Time & GPS Weather Relay**: Sync exact phone RTC time and hyper-local GPS weather data to the watch with one tap (no OpenWeatherMap API key required on the watch).
- **Remote File Manager**: Visual explorer for LittleFS partitions supporting directory browsing, file uploads, and file deletion.

---

## 🎮 Controls & Navigation Reference

The watch uses a 4-button input scheme: the **K1 3-way directional rocker** (`UP`, `OK`, `DOWN`) and a dedicated tactile **`CANCEL`** button.

### Global Button Action Matrix

| Location / Mode | Button Action | Behavior / Result |
| :--- | :--- | :--- |
| **HOME Screen** | `[UP]` / `[DOWN]` Short | Cycle through watch faces (Digital, Analog, Casio, Tactical) |
| | `[OK]` Short | Open `MAIN MENU` |
| | `[CANCEL]` Short | Toggle Display Power Save (Screen Off / On) |
| | `[CANCEL]` Long (1.5s) | Enter ESP32-S3 Deep Sleep immediately |
| | **Wrist Raise** | Wake display from sleep (Raise-to-Wake gesture) |
| **Menus & Lists** | `[UP]` / `[DOWN]` Short | Scroll items up / down (with cyclic wrap-around) |
| | `[OK]` Short | Select item / Enter sub-menu / Toggle option |
| | `[CANCEL]` Short | Return to previous screen / Back |
| | `[CANCEL]` Long (1.5s) | Immediately return to `HOME` screen |
| **Sub-Apps & Tools**| `[CANCEL]` Short | Exit sub-app to main menu |
| | `[OK]` Long | Context menu / Secondary action (e.g. Set as Boot Splash in Anim Player) |
| **Air Mouse Mode** | `[CANCEL]` Short | Toggle between `POINTER` mode and `SCROLL` mode |
| | `[OK]` Long | Pause / Resume cursor movement & open in-situ settings |
| | `[UP]` (Pointer) | Left Click & Hold (drag / select) |
| | `[DOWN]` (Pointer) | Right Click & Hold |
| | `[UP]` / `[DOWN]` (Scroll) | Scroll page Up / Down |
| **Mochi Pet** | `[OK]` Short | Pet Mochi (heart burst, happiness boost, XP gain) |
| | `[OK]` Long | Feed Mochi (animated onigiri snack, chewing animation) |
| | `[CANCEL]` Short | Toggle between Full-Screen Face and Stats Cockpit |
| **Stopwatch** | `[OK]` Short | Start / Split Lap |
| | `[UP]` Short | Pause / Resume |
| | `[DOWN]` Short | Reset |

---

## 📂 Project Directory Structure

```
Q-Watch/
├── android/                   # Native Android Jetpack Compose companion app
│   ├── app/src/main/java/     # Kotlin source (Q-Link BLE/Wi-Fi, screens, IRDB, anims)
│   └── build.gradle.kts       # Android Gradle build configuration
├── apps/                      # Micro-ELF relocatable C-ABI applications (.qapp)
│   ├── bounce/                # Nokia Bounce platformer
│   ├── breakout/              # Breakout 007 arcade game
│   ├── compass_hud/           # Tactical Compass HUD app
│   ├── dice/                  # Tactical 3D TRNG dice roller
│   ├── f1_race/               # F1 Grand Prix top-down racer
│   ├── invaders/              # 007 Space Invaders arcade shooter
│   ├── pacman/                # Pacman arcade maze game
│   ├── snake/                 # Retro Nokia snake game
│   ├── space_impact/          # Space Impact 2 side-scrolling shooter
│   └── tilt_game/             # 6-axis IMU physics tilt ball
├── boards/                    # Custom PlatformIO board definitions (esp32s3_supermini)
├── data/                      # LittleFS filesystem assets (configs, animations, remotes)
├── docs/                      # Architectural specifications (qlink_spec.md, anim_market_spec.md)
├── include/                   # Firmware C++ header files
│   ├── air_mouse.h            # BLE HID air mouse engine
│   ├── anim_engine.h          # Streaming 1-bit animation player
│   ├── clock.h                # Watch faces, stopwatch, timer, alarms, world clock
│   ├── display.h              # SH1106 OLED display driver & UI graphics
│   ├── gdma_manager.h         # ESP32-S3 GDMA display transfer pipeline
│   ├── gesture_ai.h           # TinyML on-device INT8 gesture neural network
│   ├── hw_config.h            # Hardware pin mapping and bus definitions
│   ├── ir_engine.h            # Flipper Zero / Bruce IR transceiver subsystem
│   ├── mochi_pet.h            # Dasai Mochi procedural emote engine
│   ├── poly_synth.h           # 4-channel polyphonic chiptune synthesizer
│   ├── power_manager.h        # Multi-tier power governor & battery cockpit
│   ├── qapp_loader.h          # Micro-ELF binary loader & symbol resolver
│   ├── qlink.h                # Q-Link BLE/Wi-Fi communication protocol
│   ├── qwatch_api.h           # Versioned C-ABI export table for Q-Apps
│   ├── sensors.h              # BME280, MPU-6500, QMC5883P sensor managers
│   ├── simd_accel.h           # Xtensa LX7 SIMD/PIE vector acceleration
│   └── ui_core.h              # UI state machine and event router
├── src/                       # Firmware C++ implementation files
│   ├── main.cpp               # System entry point, setup, and loop dispatch
│   ├── ui_core.cpp            # UI state dispatch and navigation handling
│   ├── ui_core_sound.cpp      # Beat maker, jukebox, and sound menu UI
│   └── ...                    # Subsystem implementations
├── tests/                     # Automated host verification test suite
│   ├── test_suite.py          # 35+ automated pytest verification test suites
│   ├── test_qapp_system.cpp   # Micro-ELF binary loader host test harness
│   └── ...                    # Subsystem host unit tests
├── tools/                     # Developer utilities
│   └── anim_packer.py         # Animation conversion and packing CLI
└── platformio.ini             # PlatformIO build configuration
```

---

## 🛠️ Installation & Quick Start

Whether you prefer a 1-click browser flashing process, command-line flashing, or building directly from source, follow the instructions below.

---

### 1. Installing Precompiled Firmware (Easiest)

Precompiled release binaries are available under [**GitHub Releases**](https://github.com/Vivek-live-now/Q-Watch/releases/latest).

#### Entering ESP32-S3 Bootloader Mode
Before flashing, connect your ESP32-S3 SuperMini to your computer using a high-quality USB-C data cable. If your operating system fails to detect the serial port, put the board into ROM Bootloader mode:
1. Hold down the **BOOT** button (labeled `B` or `BOOT` on the SuperMini board).
2. Connect the USB-C cable (or press and release the **RESET** button while holding **BOOT**).
3. Release the **BOOT** button. The chip is now in flashing mode.

---

#### Method A: 1-Click Web Flasher (Browser-Based)
*No command-line tools or drivers needed! Works on Chrome and Edge.*

1. Download **`qwatch-esp32s3-factory-merged-v1.7.0.bin`** from [GitHub Releases](https://github.com/Vivek-live-now/Q-Watch/releases/latest).
2. Navigate to [ESP Web Flasher](https://esp.adrian-alvarez.dev/) or [Espressif Web Tool](https://espressif.github.io/esptool-js/).
3. Click **Connect** and select your ESP32-S3 USB Serial port.
4. Set the flash address to **`0x0`** (or `0x0000`).
5. Choose the downloaded `qwatch-esp32s3-factory-merged-v1.7.0.bin` file.
6. Click **Program** / **Flash**. Once completed, press the **RESET** button to boot into Q-Watch!

---

#### Method B: Command-Line Flashing via `esptool.py`

Install `esptool` via Python:
```bash
pip install --upgrade esptool
```

**Flashing the Single Factory-Merged Binary (Recommended):**
```bash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 write_flash -z 0x0 qwatch-esp32s3-factory-merged-v1.7.0.bin
```
*(On Windows, replace `/dev/ttyUSB0` with your COM port, e.g., `COM3`)*

**Alternatively, Flashing Discrete Partitions:**
If flashing individual binaries from `qwatch-firmware-esp32s3-v1.7.0.zip`:
```bash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 write_flash -z \
  0x0000 bootloader.bin \
  0x8000 partitions.bin \
  0x10000 firmware.bin
```

---

### 2. Building & Flashing Firmware from Source (PlatformIO)

For developers who want to modify firmware, tune features, or compile custom apps:

#### Prerequisites
- [VS Code](https://code.visualstudio.com/) with the [PlatformIO IDE extension](https://platformio.org/) installed, or PlatformIO Core CLI (`pip install platformio`).
- ESP-IDF / Arduino framework via `espressif32@7.1.3` (automatically managed by PlatformIO).

#### Build & Flash Steps
1. **Clone the Repository**:
   ```bash
   git clone https://github.com/Vivek-live-now/Q-Watch.git
   cd Q-Watch
   ```
2. **Compile the Firmware**:
   ```bash
   pio run -e esp32s3_supermini
   ```
3. **Upload to Board**:
   Connect the ESP32-S3 SuperMini via USB-C and flash:
   ```bash
   pio run -e esp32s3_supermini -t upload
   ```
4. **Serial Monitor (Optional)**:
   ```bash
   pio device monitor -b 115200
   ```

---

### 3. Flashing the LittleFS Asset Partition (Sounds, Remotes, Animations)

Q-Watch utilizes an onboard **LittleFS** partition to store boot animations (`/boot`), IR remote databases (`/ir`), procedural pet assets (`/mochi`), and preloaded Micro-ELF Q-Apps (`/apps`).

To populate the flash filesystem with default repository assets:
```bash
pio run -e esp32s3_supermini -t uploadfs
```
*Note: Run `uploadfs` at least once upon initial hardware setup so all default apps, boot animations, and infrared remotes are present on the watch.*

---

### 4. Installing the Certified Android Companion App (`Q-Link`)

The **Q-Link Companion App** gives you real-time OLED screen mirroring, remote D-pad controls, telemetry cockpits, wireless `.qapp` app sideloading, cloud IRDB syncing, and smartphone notification forwarding.

#### Direct APK Installation
1. On your Android smartphone (Android 8.0 / API 26+), download the latest **`qlink-companion-v1.7.0.apk`** from [GitHub Releases](https://github.com/Vivek-live-now/Q-Watch/releases/latest).
2. Open the downloaded APK. If prompted, enable **"Install unknown apps"** in your Android device settings.
3. Tap **Install** to finish setup.

> [!IMPORTANT]
> **Troubleshooting "App not installed":**
> If you previously installed any test, debug, or older version of the Q-Link app on your device, Android will block the installation due to conflicting signature keys. You must **uninstall the existing Q-Link app first**, then install the release APK. The APK is fully signed and certified with official release signature schemes (v1, v2, and v3) for clean installation.

#### Required Android App Permissions
To enable all tactical features, grant the following permissions in Android Settings:
- **Nearby Devices / Bluetooth**: Required for BLE GATT screen mirroring, sensor telemetry, and Air Mouse remote.
- **Location**: Required by the Android OS framework to scan for nearby Bluetooth Low Energy peripherals.
- **Notification Access**: Go to **Settings -> Apps -> Special app access -> Notification access -> Enable "Q-Link"**. This allows the watch to receive real-time SMS, phone calls, and WhatsApp notifications with custom haptic vibration pulses and RGB LED flashes.

#### Building Android App from Source (Android Studio / Gradle)
If you wish to compile or modify the Android companion app:
1. Open the `android/` directory in **Android Studio Hedgehog** (2023.1.1) or newer.
2. Ensure **JDK 17** is selected in *Settings -> Build, Execution, Deployment -> Build Tools -> Gradle*.
3. Connect your Android phone with USB Debugging enabled (or start an emulator).
4. Run or build the app:
   ```bash
   cd android
   ./gradlew assembleDebug
   adb install -r app/build/outputs/apk/debug/app-debug.apk
   ```

---

### 5. First Boot & Network Pairing Walkthrough

#### Power On
Upon cold boot, Q-Watch plays the tactical boot animation (`/boot/boot.anim` or the default MI6 radar sweep) and enters the `HOME` watch face. Press any button to immediately skip the boot splash.

#### On-Watch Direct Wi-Fi Setup
1. From the `HOME` screen, press `[OK]` to enter `MAIN MENU`.
2. Scroll to `SETTINGS -> CONNECTIVITY -> SCAN NETWORKS`.
3. Select your local 2.4GHz Wi-Fi network from the list.
4. Enter your network password using the on-screen keyboard and press `[OK]`.

#### SoftAP Captive Portal Setup (`192.168.4.1`)
1. If Wi-Fi is unconfigured or out of range, the watch automatically broadcasts a soft-AP network named `Q-Watch-Setup`.
2. Connect your phone or laptop to `Q-Watch-Setup`. The setup portal will launch automatically, or navigate to `http://192.168.4.1` in your browser.
3. Fill in your home Wi-Fi SSID, password, POSIX timezone string, and optional OpenWeatherMap API key.
4. Click **Save & Connect**. Once connected, access the watch dashboard anytime at `http://q-watch.local` or the Web File Manager at `http://q-watch.local/fm`.

#### Connecting to the Q-Link Android Companion
1. Launch the **Q-Link** app on your Android device with Bluetooth enabled.
2. In the Q-Link dashboard, tap **Scan & Connect**. Select your Q-Watch.
3. Navigate to **Mirror** for live 128x64 display simulation and remote D-pad control, or **Sensors** for live telemetry dials!

---

## 🧪 Automated Testing & Verification

The Q-Watch repository includes a comprehensive host test suite verifying hardware algorithms, communication protocols, memory bounds, and SIMD instructions without requiring physical hardware:

```bash
# Run the complete Python-based test harness (35+ test suites)
pytest tests/test_suite.py -v

# Run the Micro-ELF binary loader and sandbox host test
g++ -std=c++17 tests/test_qapp_system.cpp tests/mock_qwatch_api.cpp -o test_qapp && ./test_qapp

# Run the Dasai Mochi procedural vector engine test
g++ -std=c++17 tests/test_mochi_system.cpp src/mochi_pet.cpp -o test_mochi && ./test_mochi
```

---

## 🤝 Contributing & License

Contributions, bug reports, and pull requests are welcome! Please ensure that code changes maintain documentation integrity and pass the host automated verification test suite before submitting pull requests.

This project is licensed under the **ISC / MIT License**. See [package.json](package.json) for details.

---

*Designed for secret agents, embedded engineers, and hardware enthusiasts.* 🍸🔫⌚
