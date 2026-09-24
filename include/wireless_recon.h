#ifndef WIRELESS_RECON_H
#define WIRELESS_RECON_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#else
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#endif

// -------------------------------------------------------------
// 802.11 Frame Parsing Constants & Structures
// -------------------------------------------------------------
#define WIFI_FRAME_TYPE_MGMT    0x00
#define WIFI_FRAME_TYPE_CTRL    0x01
#define WIFI_FRAME_TYPE_DATA    0x02

#define WIFI_SUBTYPE_ASSOC_REQ  0x00
#define WIFI_SUBTYPE_ASSOC_RESP 0x01
#define WIFI_SUBTYPE_PROBE_REQ  0x04
#define WIFI_SUBTYPE_PROBE_RESP 0x05
#define WIFI_SUBTYPE_BEACON     0x08
#define WIFI_SUBTYPE_DISASSOC   0x0A
#define WIFI_SUBTYPE_AUTH       0x0B
#define WIFI_SUBTYPE_DEAUTH     0x0C

#pragma pack(push, 1)
struct MacAddress {
    uint8_t bytes[6];

    bool isBroadcast() const {
        return (bytes[0] == 0xFF && bytes[1] == 0xFF && bytes[2] == 0xFF &&
                bytes[3] == 0xFF && bytes[4] == 0xFF && bytes[5] == 0xFF);
    }

    bool isZero() const {
        return (bytes[0] == 0 && bytes[1] == 0 && bytes[2] == 0 &&
                bytes[3] == 0 && bytes[4] == 0 && bytes[5] == 0);
    }

    bool equals(const MacAddress& other) const {
        return memcmp(bytes, other.bytes, 6) == 0;
    }
};

struct WifiFrameHeader {
    uint16_t frame_control;
    uint16_t duration;
    MacAddress addr1; // Receiver / Destination
    MacAddress addr2; // Transmitter / Source
    MacAddress addr3; // BSSID
    uint16_t seq_ctrl;
};
#pragma pack(pop)

// Parse helpers for 802.11 Frame Control
static inline uint8_t get_wifi_frame_type(uint16_t fc) {
    return (uint8_t)((fc >> 2) & 0x03);
}

static inline uint8_t get_wifi_frame_subtype(uint16_t fc) {
    return (uint8_t)((fc >> 4) & 0x0F);
}

// -------------------------------------------------------------
// Data Structures for Subsystems
// -------------------------------------------------------------
// 1. BLE Target Entry
struct BleTarget {
    char name[20];
    char mac[18];        // "AA:BB:CC:DD:EE:FF"
    int8_t rssi;
    uint32_t last_seen;
    bool active;
};

// 2. Wi-Fi Channel Statistics (Channels 1 to 14)
struct WifiChannelStat {
    uint8_t channel;
    uint16_t ap_count;
    int8_t max_rssi;
};

// 3. Deauth Attack Event
struct DeauthAttackEvent {
    MacAddress source_mac;
    MacAddress target_mac;
    uint16_t reason_code;
    uint32_t timestamp;
    int8_t rssi;
};

// -------------------------------------------------------------
// Wireless Recon Manager Class
// -------------------------------------------------------------
class WirelessRecon {
public:
    WirelessRecon();
    ~WirelessRecon();

    bool begin();
    void loop();

    // --- BLE Radar & Proximity Subsystem ---
    void startBleScan();
    void stopBleScan();
    bool isBleScanning() const { return ble_scanning; }

    int getBleTargetCount() const { return ble_target_count; }
    const BleTarget* getBleTarget(int idx) const;
    int getLockedTargetIdx() const { return locked_target_idx; }
    void lockTarget(int idx);
    void unlockTarget();
    bool hasTargetLock() const { return locked_target_idx >= 0 && locked_target_idx < ble_target_count; }
    const BleTarget* getLockedTarget() const;

    float getTargetProximity(int8_t rssi) const;
    float getTargetEstimatedDistance(int8_t rssi) const;
    uint16_t getGeigerTickInterval(int8_t rssi) const;

    // --- Wi-Fi Channel Analyzer Subsystem ---
    void startChannelScan();
    bool isChannelScanning() const { return channel_scanning; }
    const WifiChannelStat* getChannelStats() const { return channel_stats; }
    uint8_t getBestChannel() const { return best_channel; }
    uint16_t getTotalApsFound() const { return total_aps_found; }

    // --- 802.11 Deauth Detector Subsystem ---
    void startDeauthMonitor(uint8_t channel = 1);
    void stopDeauthMonitor();
    bool isDeauthMonitorActive() const { return deauth_monitor_active; }
    uint32_t getDeauthCount() const { return deauth_count; }
    uint16_t getDeauthRatePerSec() const { return deauth_rate_per_sec; }
    bool isAttackDetected() const { return attack_in_progress; }
    const DeauthAttackEvent& getLastAttackEvent() const { return last_attack_event; }
    void resetDeauthStats();

    // --- Promiscuous Packet Monitor Subsystem ---
    void startPacketMonitor(uint8_t channel = 1);
    void stopPacketMonitor();
    bool isPacketMonitorActive() const { return pkt_monitor_active; }
    uint8_t getActiveChannel() const { return active_channel; }
    void setChannel(uint8_t ch);
    void cycleChannel();

    uint16_t getCurrentPacketRate() const { return current_pkt_rate; }
    uint32_t getTotalPackets() const { return total_packets; }
    void getPacketTypePercentages(uint8_t& mgmt_pct, uint8_t& ctrl_pct, uint8_t& data_pct) const;
    const uint8_t* getPacketRateHistory() const { return pkt_rate_history; }

    // Internal Promiscuous Packet Handler
    void handlePromiscuousPacket(const uint8_t* buf, uint16_t len, int8_t rssi);

    // Helpers
    static void formatMac(const uint8_t* mac, char* out_str, size_t max_len);

    // Test Injection Hooks
    void testInjectBleTarget(const char* name, const char* mac, int8_t rssi);
    void testInjectWifiScan(uint8_t channel, int8_t rssi);

private:
    void stopAllMonitors();

    // BLE state
    bool ble_scanning;
    static const int MAX_BLE_TARGETS = 16;
    BleTarget ble_targets[MAX_BLE_TARGETS];
    int ble_target_count;
    int locked_target_idx;
    uint32_t last_ble_scan_time;

    // Channel spectrum state
    bool channel_scanning;
    WifiChannelStat channel_stats[14]; // Index 0..13 for channels 1..14
    uint8_t best_channel;
    uint16_t total_aps_found;
    uint32_t last_channel_scan_time;

    // Deauth state
    bool deauth_monitor_active;
    uint32_t deauth_count;
    uint16_t deauth_rate_per_sec;
    uint16_t deauth_window_counter;
    uint32_t last_deauth_rate_calc;
    bool attack_in_progress;
    uint32_t attack_alert_timer;
    DeauthAttackEvent last_attack_event;

    // Packet monitor state
    bool pkt_monitor_active;
    uint8_t active_channel;
    uint32_t total_packets;
    uint32_t total_mgmt_pkts;
    uint32_t total_ctrl_pkts;
    uint32_t total_data_pkts;
    uint16_t current_pkt_rate;
    uint16_t pkt_window_counter;
    uint32_t last_rate_update;

    static const int PKT_RATE_HISTORY_SIZE = 64;
    uint8_t pkt_rate_history[PKT_RATE_HISTORY_SIZE];
    int pkt_history_head;

#ifdef ARDUINO
    BLEScan* pBLEScan;
#endif
};

extern WirelessRecon wirelessRecon;

#endif // WIRELESS_RECON_H
