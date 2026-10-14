#ifndef QLINK_H
#define QLINK_H

#include <stdint.h>
#include <stddef.h>
#include "qwatch_api.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <WebServer.h>
#include <FS.h>
#include <LittleFS.h>
#else
#include <string>
#include <algorithm>
#include <cctype>
#include <cstring>
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
    void toLowerCase() {
        for (auto& c : *this) c = (char)tolower((unsigned char)c);
    }
    void replace(const std::string& from, const std::string& to) {
        if (from.empty()) return;
        size_t start_pos = 0;
        while((start_pos = find(from, start_pos)) != std::string::npos) {
            std::string::replace(start_pos, from.length(), to);
            start_pos += to.length();
        }
    }
    void trim() {
        while (!empty() && isspace((unsigned char)front())) erase(begin());
        while (!empty() && isspace((unsigned char)back())) pop_back();
    }
    bool startsWith(const char* prefix) const {
        return rfind(prefix, 0) == 0;
    }
    bool endsWith(const char* suffix) const {
        if (!suffix) return false;
        size_t s_len = strlen(suffix);
        if (size() < s_len) return false;
        return compare(size() - s_len, s_len, suffix) == 0;
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
    bool equalsIgnoreCase(const String& s) const {
        if (size() != s.size()) return false;
        for (size_t i = 0; i < size(); ++i) {
            if (tolower((unsigned char)(*this)[i]) != tolower((unsigned char)s[i])) return false;
        }
        return true;
    }
    bool equalsIgnoreCase(const char* s) const {
        if (!s) return false;
        return equalsIgnoreCase(String(s));
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
    void notifyFileReceived(const String& path);

    // Clock & Weather Sync
    bool syncTime(uint32_t epoch_sec, int32_t tz_offset_sec);
    bool syncWeather(const String& city, float temp_c, int humidity, int code, const String& desc, float high_c, float low_c);

    // Display Streaming Controls
    void setStreamingDisplay(bool enable, uint8_t target_fps = 20);
    bool isStreamingDisplay() const { return display_streaming; }
    uint8_t getTargetFps() const { return target_stream_fps; }

    // App Sideloading & Verification APIs
    bool validateQAppHeader(const uint8_t* data, size_t len, String& out_app_name, String& out_version, size_t& out_size, String* out_error = nullptr);
    bool validateAnimHeader(const uint8_t* data, size_t len, uint16_t& out_frames, uint16_t& out_delay_ms, String* out_error = nullptr);

    // Filesystem Storage & Safe Deletion APIs
    bool safeDeleteAnim(const String& path);
    String generateStorageJson();

    // Companion Advanced Subsystem Controls (Mochi, Power, Sigint)
    bool handleMochiAction(const String& action, int helmet_id = -1);
    bool setPowerProfileState(const String& profile_str, bool ulp, bool eco_led, bool eco_radio);
    String generatePowerProfileJson();
    String generateSigintScanJson();

#ifdef ARDUINO
    void registerHttpRoutes(WebServer& server);
#endif

    // BLE Subsystem Controls & Status
    bool isBleConnected() const { return ble_connected; }
    void setBleConnected(bool connected) { ble_connected = connected; }
    bool isBleEnabled() const { return ble_active; }
    bool isBleActive() const { return ble_active; }
    void setBleEnabled(bool enabled);
    void startBle();
    void stopBle();
    void pauseBleAdvertising();
    void resumeBleAdvertising();
    void clearBondedDevices();
    void handleBleCommand(const uint8_t* data, size_t len);
    void handleBleFilePacket(const uint8_t* data, size_t len);
    bool startFileUpload(const String& path, size_t total_size);
    bool processFileChunk(const uint8_t* chunk, size_t chunk_len);
    bool finishFileUpload(size_t expected_size);
    void cancelFileUpload();
    bool isUploadInProgress() const { return upload_in_progress; }
    size_t getUploadReceivedBytes() const { return upload_received_size; }
    const String& getUploadPath() const { return upload_file_path; }
    // IR Protocol & Raw Transmission APIs
    bool transmitParsedIr(const String& proto, uint32_t addr, uint32_t cmd, uint16_t nbits);
    bool startRawIr(size_t count, uint32_t frequency = 38000, float duty_cycle = 0.33f);
    bool processRawIrChunk(uint16_t seq, const uint8_t* data, size_t len);
    bool finishRawIr(size_t count = 0);
    void cancelRawIr();
    bool isRawIrInProgress() const { return raw_ir_in_progress; }
    size_t getRawIrReceivedCount() const { return raw_ir_received_count; }
    size_t getRawIrExpectedCount() const { return raw_ir_expected_count; }
    uint32_t getRawIrFrequency() const { return raw_ir_frequency; }

private:
    uint8_t packet_seq;
    bool display_streaming;
    uint8_t target_stream_fps;
    uint32_t last_stream_frame_time;

    // BLE Subsystem state
    bool ble_active;
    bool ble_connected;
    bool ble_advertising_paused;
    uint32_t last_telemetry_tx;
    bool upload_in_progress;
    String upload_file_path;
    size_t upload_expected_size;
    size_t upload_received_size;
    uint16_t upload_expected_seq;
    uint32_t upload_last_chunk_time;
    bool upload_use_ram;
    uint8_t* upload_ram_buf;
#ifdef ARDUINO
    File upload_file;
#else
    FILE* upload_file;
#endif

    // Raw IR session state
    bool raw_ir_in_progress;
    uint32_t raw_ir_frequency;
    float raw_ir_duty_cycle;
    size_t raw_ir_expected_count;
    size_t raw_ir_received_count;
    uint16_t raw_ir_expected_seq;
    uint32_t raw_ir_last_activity;
    uint16_t raw_ir_timings[1024];

#ifndef ARDUINO
public:
    String last_tx_proto;
    uint32_t last_tx_addr;
    uint32_t last_tx_cmd;
    uint16_t last_tx_nbits;
    size_t last_tx_raw_count;
    uint32_t last_tx_raw_freq;
private:
#endif
};

extern QLinkEngine qlink;

#endif // QLINK_H
