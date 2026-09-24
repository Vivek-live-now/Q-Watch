#ifndef QLINK_H
#define QLINK_H

#include <stdint.h>
#include <stddef.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <WebServer.h>
#else
#include <string>
#include <algorithm>
#include <cctype>
class String : public std::string {
public:
    String() : std::string() {}
    String(const char* s) : std::string(s ? s : "") {}
    String(const std::string& s) : std::string(s) {}
    String(int v) : std::string(std::to_string(v)) {}
    String(unsigned int v) : std::string(std::to_string(v)) {}
    String(long v) : std::string(std::to_string(v)) {}
    String(unsigned long v) : std::string(std::to_string(v)) {}
    String(float v, int dec = 2) : std::string() {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.*f", dec, v);
        *this = buf;
    }
    void toUpperCase() {
        for (auto& c : *this) c = (char)toupper((unsigned char)c);
    }
    void trim() {
        while (!empty() && isspace((unsigned char)front())) erase(begin());
        while (!empty() && isspace((unsigned char)back())) pop_back();
    }
    bool startsWith(const char* prefix) const {
        return rfind(prefix, 0) == 0;
    }
    int indexOf(const char* str) const {
        auto pos = find(str);
        return (pos == std::string::npos) ? -1 : (int)pos;
    }
    String substring(size_t from, size_t to = std::string::npos) const {
        if (from >= size()) return String("");
        if (to == std::string::npos || to > size()) to = size();
        return String(substr(from, to - from));
    }
    void reserve(size_t) {}
};
#endif

// ============================================================================
// Q-LINK PROTOCOL DEFINITIONS & GATT CONSTANTS (v1.0.0)
// ============================================================================
#define QLINK_PROTOCOL_VERSION "1.0.0"
#define QLINK_MAGIC 0x514C // "QL"

// BLE GATT Service & Characteristics UUIDs (128-bit)
#define QLINK_SERVICE_UUID     "0000FE50-0000-1000-8000-00805F9B34FB"
#define QLINK_CHAR_COMMAND     "0000FE51-0000-1000-8000-00805F9B34FB"
#define QLINK_CHAR_TELEMETRY   "0000FE52-0000-1000-8000-00805F9B34FB"
#define QLINK_CHAR_DISPLAY     "0000FE53-0000-1000-8000-00805F9B34FB"
#define QLINK_CHAR_FILE        "0000FE54-0000-1000-8000-00805F9B34FB"

// Compact Binary Telemetry Packet (32 Bytes Fixed Layout)
#pragma pack(push, 1)
struct QLinkCompactTelemetry {
    uint16_t magic;            // 0x00: 0x514C ("QL")
    uint8_t  seq;              // 0x02: Sequence Counter (0-255)
    uint8_t  battery_pct;      // 0x03: Battery % (0-100)
    uint16_t battery_mv;       // 0x04: Battery Millivolts (e.g. 4120)
    int16_t  temp_c_x10;       // 0x06: Temperature °C * 10 (e.g. 245 = 24.5°C)
    uint16_t pressure_hpa_x10; // 0x08: Pressure hPa * 10 (e.g. 10132 = 1013.2 hPa)
    uint16_t humidity_pct_x10; // 0x0A: Relative Humidity % * 10 (e.g. 550 = 55.0%)
    int16_t  altitude_m;       // 0x0C: Altitude in Meters
    int16_t  pitch_deg_x10;    // 0x0E: IMU Pitch in Degrees * 10 (-900 to +900)
    int16_t  roll_deg_x10;     // 0x10: IMU Roll in Degrees * 10 (-1800 to +1800)
    uint16_t heading_deg_x10;  // 0x12: Compass Heading Degrees * 10 (0-3599)
    uint8_t  heart_rate_bpm;   // 0x14: MAX30102 Heart Rate (0-250)
    uint8_t  spo2_pct;         // 0x15: MAX30102 SpO2 % (0-100)
    uint32_t step_count;       // 0x16: Pedometer Steps
    uint16_t status_flags;     // 0x1A: Bit 0: Charging, Bit 1: Wi-Fi on, Bit 2: Sleep
    uint32_t uptime_sec;       // 0x1C: Watch Uptime in Seconds
};
#pragma pack(pop)

class QLinkEngine {
public:
    QLinkEngine();
    void begin();
    void loop();

    // Telemetry & Device Info Serialization
    void fillCompactTelemetry(QLinkCompactTelemetry& out);
    String generateTelemetryJson();
    String generateDeviceInfoJson();

    // Button Event Injection
    bool injectButton(const String& btn_str, const String& evt_str);

    // Notification Handler
    void handleNotification(const String& app_name, const String& title, const String& body,
                            const String& alert_style, const String& led_hex);

    // Clock & Weather Sync
    bool syncTime(uint32_t epoch_sec, int32_t tz_offset_sec);
    bool syncWeather(const String& city, float temp_c, int humidity, int code, const String& desc, float high_c, float low_c);

    // Display Streaming Controls
    void setStreamingDisplay(bool enable, uint8_t target_fps = 20);
    bool isStreamingDisplay() const { return display_streaming; }
    uint8_t getTargetFps() const { return target_stream_fps; }

#ifdef ARDUINO
    void registerHttpRoutes(WebServer& server);
#endif

private:
    uint8_t packet_seq;
    bool display_streaming;
    uint8_t target_stream_fps;
    uint32_t last_stream_frame_time;
};

extern QLinkEngine qlink;

#endif // QLINK_H
