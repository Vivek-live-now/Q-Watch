# Q-Link Protocol Specification v1.0.0
**The Authoritative Hardware-Agnostic Interface for Q-Watch & Companion Systems**

---

## 1. Architectural Philosophy & Layering

The Q-Watch ecosystem follows a strict unidirectional tiered architecture:

```
+-----------------------------------------------------------+
|                      Q-Link:Android                       |
|        (Native Android Kotlin + Jetpack Compose)          |
+-----------------------------+-----------------------------+
                              |
                    Q-Link Protocol v1.0.0
         (Transport: Wi-Fi HTTP/Stream & BLE GATT)
                              |
+-----------------------------v-----------------------------+
|                      Q-Link Firmware                      |
|                  (include/qlink.h & src/qlink.cpp)        |
+-----------------------------+-----------------------------+
                              |
+-----------------------------v-----------------------------+
|                    Q-Watch API / Services                 |
|   (Display, Button, Sensors, Storage, Sound, Timekeeping)  |
+-----------------------------+-----------------------------+
                              |
+-----------------------------v-----------------------------+
|                       Hardware Layer                      |
|   (ESP32-S3, SSD1306/SH1106, BME280, MPU6500, MAX30102)   |
+-----------------------------------------------------------+
```

### Guiding Principles:
1. **Zero Undocumented Internals:** The Android app MUST NEVER rely on internal memory layouts, private firmware globals, or unversioned endpoints.
2. **Abstracted Hardware Access:** All sensor data, display buffers, file operations, and button inputs are routed through explicit, versioned Q-Link API contracts.
3. **Dual-Transport Parity:** Features are accessible over both **Wi-Fi** (high-speed data, live 30 FPS display streaming, large file transfers) and **Bluetooth Low Energy (BLE)** (low-power telemetry, button injection, notification forwarding, field sideloading).

---

## 2. Bluetooth Low Energy (BLE) GATT Profile

### 2.1 Service Definition
- **Service Name:** `Q-Watch Q-Link Service`
- **Primary Service UUID:** `0000FE50-0000-1000-8000-00805F9B34FB` (Base: `FE50`)

### 2.2 Characteristics Table
| Characteristic | UUID | Properties | Format | Description |
| :--- | :--- | :--- | :--- | :--- |
| **COMMAND / RX** | `0000FE51-0000-1000-8000-00805F9B34FB` | `WRITE`, `WRITE_NO_RESP` | Binary / JSON TLV | Phone writes commands (buttons, notifications, requests, time sync) |
| **TELEMETRY / TX** | `0000FE52-0000-1000-8000-00805F9B34FB` | `READ`, `NOTIFY` | Binary Compact (32B) | Watch broadcasts battery, BME280, IMU, PPG, step counters |
| **DISPLAY STREAM** | `0000FE53-0000-1000-8000-00805F9B34FB` | `NOTIFY` | Fragmented Chunks (128B) | Live OLED 128x64 display buffer chunks with frame sequence IDs |
| **FILE / APP SYNC** | `0000FE54-0000-1000-8000-00805F9B34FB` | `WRITE`, `NOTIFY` | Chunked Protocol | Sideloading `.qapp` binaries and file chunks over BLE |

### 2.3 BLE Compact Telemetry Packet Format (32 Bytes, Fixed Size)
Sent periodically (1–5 Hz) via `TELEMETRY / TX` notification:
```
Offset  Size  Type       Field             Description
0x00    2     uint16_t   magic             0x514C ("QL")
0x02    1     uint8_t    seq               Packet sequence number (0-255)
0x03    1     uint8_t    battery_pct       Battery percentage (0-100)
0x04    2     uint16_t   battery_mv        Battery voltage in millivolts (e.g. 4120)
0x06    2     int16_t    temp_c_x10        Temperature in °C * 10 (e.g. 245 = 24.5°C)
0x08    2     uint16_t   pressure_hpa_x10  Barometric pressure in hPa * 10
0x0A    2     uint16_t   humidity_pct_x10  Relative humidity % * 10
0x0C    2     int16_t    altitude_m        Calculated barometric altitude in meters
0x0E    2     int16_t    pitch_deg_x10     IMU Pitch in degrees * 10 (-900 to +900)
0x10    2     int16_t    roll_deg_x10      IMU Roll in degrees * 10 (-1800 to +1800)
0x12    2     uint16_t   heading_deg_x10   Compass heading in degrees * 10 (0-3599)
0x14    1     uint8_t    heart_rate_bpm    MAX30102 Heart Rate (0-250)
0x15    1     uint8_t    spo2_pct          MAX30102 SpO2 % (0-100)
0x16    4     uint32_t   step_count        Pedometer total steps
0x1A    2     uint16_t   status_flags      Bit 0: Charging, Bit 1: Wi-Fi on, Bit 2: Sleep active
0x1C    4     uint32_t   uptime_sec        Watch uptime in seconds
```

---

## 3. Wi-Fi REST & Live Streaming API (v1)

Base URL: `http://<watch-ip-or-q-watch.local>/api/v1`

### 3.1 Device & System Information
#### `GET /api/v1/info`
Returns device metadata, firmware versions, memory, and capabilities.
```json
{
  "device": "Q-Watch",
  "model": "ESP32-S3-SuperMini",
  "firmware_version": "1.4.0",
  "protocol_version": "1.0.0",
  "compile_date": "Sep 24 2026",
  "mac": "CC:7B:5C:80:12:34",
  "ip": "192.168.1.145",
  "rssi": -58,
  "battery": {
    "percent": 84,
    "voltage_mv": 3980,
    "charging": false
  },
  "memory": {
    "free_heap": 218400,
    "min_free_heap": 184500,
    "fs_total_bytes": 1441792,
    "fs_used_bytes": 412800
  },
  "uptime_sec": 3840
}
```

---

### 3.2 Real-Time Sensor Telemetry
#### `GET /api/v1/telemetry`
Returns full diagnostic sensor readings.
```json
{
  "bme280": {
    "temperature_c": 24.6,
    "pressure_hpa": 1013.25,
    "humidity_pct": 52.8,
    "altitude_m": 84.2
  },
  "imu": {
    "pitch_deg": 4.2,
    "roll_deg": -1.8,
    "accel_x": 0.05,
    "accel_y": 0.02,
    "accel_z": 0.98
  },
  "compass": {
    "heading_deg": 142.5,
    "field_ut": 46.2
  },
  "health": {
    "heart_rate_bpm": 72,
    "spo2_pct": 98,
    "finger_detected": true
  },
  "pedometer": {
    "steps": 6420,
    "distance_km": 4.81,
    "calories_kcal": 256
  }
}
```

---

### 3.3 Live Display Simulation & Virtual Controls

#### `GET /api/v1/display/frame`
- **Output:** Raw 1024-byte binary stream (`Content-Type: application/octet-stream`).
- **Format:** 128×64 1-bit monochrome SSD1306/SH1106 column-major/page layout (1024 bytes = 128 horizontal columns × 8 vertical pages of 8 bits each).

#### `GET /api/v1/display/stream`
- **Output:** Continuous multipart binary HTTP stream or WebSocket stream at 10–30 FPS with header:
  `[Magic: 0x51 0x44 ("QD")][FrameNum: uint16][Payload: 1024 bytes]`
- High efficiency: 30 FPS consumes only ~30 KB/sec!

#### `POST /api/v1/display/control`
Turns streaming generation on or off on the watch to save battery when the user exits the Live Mirror screen.
```json
{
  "streaming": true,
  "fps": 20
}
```

#### `POST /api/v1/button`
Injects virtual button events into the watch’s event loop.
```json
{
  "button": "OK",        // "UP", "DOWN", "OK", "CANCEL"
  "event": "SHORT_PRESS" // "SHORT_PRESS", "LONG_PRESS"
}
```
*Response: `{"status": "ok"}`*

---

### 3.4 Phone Notification Ingestion
#### `POST /api/v1/notification`
Forwards notifications captured by Android's `NotificationListenerService`.
```json
{
  "app_name": "WhatsApp",
  "package": "com.whatsapp",
  "title": "Jane Doe",
  "body": "Mission rendezvous confirmed at 14:00.",
  "category": "MESSAGE",  // "MESSAGE", "CALL", "EMAIL", "SYSTEM"
  "alert_style": "CHIME", // "CHIME", "BUZZ", "SILENT"
  "led_color": "#00E5FF"  // Hex color for WS2812 alert pulse
}
```
*Watch Action:*
1. Pops up on-screen notification HUD (overlay toast or modal).
2. Plays audible notification chime via `soundManager.playNotification()`.
3. Pulses WS2812 RGB LED with specified color.

---

### 3.5 File Operations & Q-App Sideloading
#### `GET /api/v1/fs/list?path=/`
Lists directory contents with sizes and file types.
```json
{
  "path": "/apps",
  "files": [
    {"name": "tilt_ball.qapp", "size": 3480, "is_dir": false},
    {"name": "compass_hud.qapp", "size": 4120, "is_dir": false}
  ]
}
```

#### `GET /api/v1/fs/download?path=/sounds/theme.mel`
Downloads requested file content.

#### `POST /api/v1/fs/upload`
Standard multipart form-data upload to specified destination path.

#### `DELETE /api/v1/fs/delete?path=/apps/old.qapp`
Deletes specified file from LittleFS.

#### `POST /api/v1/app/install`
Specialized `.qapp` packager endpoint:
1. Validates Micro-ELF header magic (`\x7fELF`) and Q-App ABI compatibility (`QAPP_MAGIC 0x51415050`).
2. Atomically writes to `/apps/<name>.qapp`.
3. Refreshes the watch's internal App Store manifest.
```json
{
  "status": "success",
  "app_name": "Space Invaders",
  "version": "1.0",
  "size_bytes": 5240
}
```

---

### 3.6 Clock & Weather Synchronization
#### `POST /api/v1/sync/time`
Synchronizes watch RTC with atomic phone time.
```json
{
  "epoch_sec": 1790247600,
  "timezone_offset_sec": 19800,
  "timezone_name": "Asia/Kolkata"
}
```

#### `POST /api/v1/sync/weather`
Phone fetches GPS-accurate weather and pushes directly to watch (no OpenWeatherMap API key required on watch).
```json
{
  "city": "Bengaluru",
  "temp_c": 26.4,
  "humidity_pct": 60,
  "condition_code": 800,
  "condition_str": "Clear Sky",
  "high_c": 29.0,
  "low_c": 19.5
}
```

---

## 4. Error Code Standard
All API errors return consistent JSON:
```json
{
  "error": {
    "code": 404,
    "name": "FILE_NOT_FOUND",
    "message": "Path /apps/missing.qapp does not exist on LittleFS"
  }
}
```
Common codes:
- `400 BAD_REQUEST`: Malformed JSON or invalid parameter
- `404 NOT_FOUND`: Missing file or endpoint
- `415 INVALID_ELF`: Invalid .qapp ELF binary header
- `500 INTERNAL_ERROR`: Hardware or LittleFS write failure
- `503 BUSY`: Radar sweep or OTA flashing currently locks bus
