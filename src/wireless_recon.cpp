#include "wireless_recon.h"
#include <math.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "sound_manager.h"
#include "led_manager.h"
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static inline uint32_t millis() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
#endif

WirelessRecon wirelessRecon;

#ifdef ARDUINO
static void wifi_promiscuous_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (!buf) return;
    wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    wirelessRecon.handlePromiscuousPacket(pkt->payload, pkt->rx_ctrl.sig_len, pkt->rx_ctrl.rssi);
}

class QWatchAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        String nameStr = advertisedDevice.haveName() ? advertisedDevice.getName().c_str() : "";
        String macStr = advertisedDevice.getAddress().toString().c_str();
        int8_t rssi = (int8_t)advertisedDevice.getRSSI();

        wirelessRecon.testInjectBleTarget(nameStr.c_str(), macStr.c_str(), rssi);
    }
};
#endif

WirelessRecon::WirelessRecon() :
    ble_scanning(false),
    ble_target_count(0),
    locked_target_idx(-1),
    last_ble_scan_time(0),
    channel_scanning(false),
    best_channel(1),
    total_aps_found(0),
    last_channel_scan_time(0),
    deauth_monitor_active(false),
    deauth_count(0),
    deauth_rate_per_sec(0),
    deauth_window_counter(0),
    last_deauth_rate_calc(0),
    attack_in_progress(false),
    attack_alert_timer(0),
    pkt_monitor_active(false),
    active_channel(1),
    total_packets(0),
    total_mgmt_pkts(0),
    total_ctrl_pkts(0),
    total_data_pkts(0),
    current_pkt_rate(0),
    pkt_window_counter(0),
    last_rate_update(0),
    pkt_history_head(0)
#ifdef ARDUINO
    , pBLEScan(nullptr)
#endif
{
    memset(ble_targets, 0, sizeof(ble_targets));
    memset(&last_attack_event, 0, sizeof(last_attack_event));
    memset(pkt_rate_history, 0, sizeof(pkt_rate_history));

    for (int i = 0; i < 14; i++) {
        channel_stats[i].channel = i + 1;
        channel_stats[i].ap_count = 0;
        channel_stats[i].max_rssi = -100;
    }
}

WirelessRecon::~WirelessRecon() {
    stopAllMonitors();
}

bool WirelessRecon::begin() {
    return true;
}

void WirelessRecon::stopAllMonitors() {
    stopBleScan();
    stopDeauthMonitor();
    stopPacketMonitor();
}

void WirelessRecon::formatMac(const uint8_t* mac, char* out_str, size_t max_len) {
    if (!mac || !out_str || max_len < 18) return;
    snprintf(out_str, max_len, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// -------------------------------------------------------------
// BLE Radar & Proximity Tracking
// -------------------------------------------------------------
void WirelessRecon::startBleScan() {
    stopAllMonitors();
    ble_scanning = true;
    last_ble_scan_time = millis();

#ifdef ARDUINO
    if (!BLEDevice::getInitialized()) {
        BLEDevice::init("007_QWatch_Recon");
    }
    pBLEScan = BLEDevice::getScan();
    if (pBLEScan) {
        pBLEScan->setAdvertisedDeviceCallbacks(new QWatchAdvertisedDeviceCallbacks(), true);
        pBLEScan->setActiveScan(true);
        pBLEScan->setInterval(100);
        pBLEScan->setWindow(99);
        pBLEScan->start(0, nullptr, false); // continuous background scan
    }
#endif
}

void WirelessRecon::stopBleScan() {
    if (!ble_scanning) return;
    ble_scanning = false;

#ifdef ARDUINO
    if (pBLEScan) {
        pBLEScan->stop();
        pBLEScan->clearResults();
        pBLEScan = nullptr;
    }
#endif
}

void WirelessRecon::testInjectBleTarget(const char* name, const char* mac, int8_t rssi) {
    if (!mac || strlen(mac) == 0) return;

    // Check if target already exists
    for (int i = 0; i < ble_target_count; i++) {
        if (strcasecmp(ble_targets[i].mac, mac) == 0) {
            ble_targets[i].rssi = rssi;
            ble_targets[i].last_seen = millis();
            ble_targets[i].active = true;
            if (name && strlen(name) > 0 && strcmp(ble_targets[i].name, "UNKNOWN") == 0) {
                strncpy(ble_targets[i].name, name, sizeof(ble_targets[i].name) - 1);
                ble_targets[i].name[sizeof(ble_targets[i].name) - 1] = '\0';
            }
            return;
        }
    }

    // Add new target
    if (ble_target_count < MAX_BLE_TARGETS) {
        BleTarget& t = ble_targets[ble_target_count];
        if (name && strlen(name) > 0) {
            strncpy(t.name, name, sizeof(t.name) - 1);
        } else {
            strncpy(t.name, "UNKNOWN", sizeof(t.name) - 1);
        }
        t.name[sizeof(t.name) - 1] = '\0';
        strncpy(t.mac, mac, sizeof(t.mac) - 1);
        t.mac[sizeof(t.mac) - 1] = '\0';
        t.rssi = rssi;
        t.last_seen = millis();
        t.active = true;
        ble_target_count++;
    } else {
        // Replace oldest inactive target
        int oldest_idx = 0;
        uint32_t oldest_time = 0xFFFFFFFF;
        for (int i = 0; i < MAX_BLE_TARGETS; i++) {
            if (i == locked_target_idx) continue;
            if (ble_targets[i].last_seen < oldest_time) {
                oldest_time = ble_targets[i].last_seen;
                oldest_idx = i;
            }
        }
        BleTarget& t = ble_targets[oldest_idx];
        if (name && strlen(name) > 0) {
            strncpy(t.name, name, sizeof(t.name) - 1);
        } else {
            strncpy(t.name, "UNKNOWN", sizeof(t.name) - 1);
        }
        t.name[sizeof(t.name) - 1] = '\0';
        strncpy(t.mac, mac, sizeof(t.mac) - 1);
        t.mac[sizeof(t.mac) - 1] = '\0';
        t.rssi = rssi;
        t.last_seen = millis();
        t.active = true;
    }
}

const BleTarget* WirelessRecon::getBleTarget(int idx) const {
    if (idx >= 0 && idx < ble_target_count) {
        return &ble_targets[idx];
    }
    return nullptr;
}

void WirelessRecon::lockTarget(int idx) {
    if (idx >= 0 && idx < ble_target_count) {
        locked_target_idx = idx;
    }
}

void WirelessRecon::unlockTarget() {
    locked_target_idx = -1;
}

const BleTarget* WirelessRecon::getLockedTarget() const {
    if (hasTargetLock()) {
        return &ble_targets[locked_target_idx];
    }
    return nullptr;
}

float WirelessRecon::getTargetProximity(int8_t rssi) const {
    // Clamp RSSI to -95 (lost) .. -35 (point-blank)
    float r = (float)rssi;
    if (r < -95.0f) r = -95.0f;
    if (r > -35.0f) r = -35.0f;
    return (r - (-95.0f)) / (-35.0f - (-95.0f)); // [0.0 .. 1.0]
}

float WirelessRecon::getTargetEstimatedDistance(int8_t rssi) const {
    // Log-distance path loss: d = 10 ^ ((A - RSSI) / (10 * n))
    // Reference RSSI at 1 meter: A = -59 dBm, Path loss exponent n = 2.0
    float exponent = (-59.0f - (float)rssi) / 20.0f;
    float dist = powf(10.0f, exponent);
    if (dist < 0.1f) dist = 0.1f;
    if (dist > 99.0f) dist = 99.0f;
    return dist;
}

uint16_t WirelessRecon::getGeigerTickInterval(int8_t rssi) const {
    float prox = getTargetProximity(rssi);
    // Faster ticks when close (60ms), slower when far (1000ms)
    float interval = 60.0f + (1.0f - prox) * 940.0f;
    return (uint16_t)interval;
}

// -------------------------------------------------------------
// Wi-Fi Channel Analyzer Subsystem
// -------------------------------------------------------------
void WirelessRecon::startChannelScan() {
    stopAllMonitors();
    channel_scanning = true;
    last_channel_scan_time = millis();
    total_aps_found = 0;

    for (int i = 0; i < 14; i++) {
        channel_stats[i].ap_count = 0;
        channel_stats[i].max_rssi = -100;
    }

#ifdef ARDUINO
    WiFi.disconnect(false);
    WiFi.mode(WIFI_STA);
    WiFi.scanDelete();
    WiFi.scanNetworks(true, false, false, 250); // fast asynchronous scan
#endif
}

void WirelessRecon::testInjectWifiScan(uint8_t channel, int8_t rssi) {
    if (channel >= 1 && channel <= 14) {
        int idx = channel - 1;
        channel_stats[idx].ap_count++;
        if (rssi > channel_stats[idx].max_rssi) {
            channel_stats[idx].max_rssi = rssi;
        }
        total_aps_found++;

        // Recalculate best non-overlapping channel (1, 6, or 11)
        uint8_t cand[3] = {1, 6, 11};
        uint8_t best_ch = 1;
        int min_aps = 99999;
        int min_rssi = 100;

        for (int i = 0; i < 3; i++) {
            uint8_t ch = cand[i];
            int count = channel_stats[ch - 1].ap_count;
            int max_r = channel_stats[ch - 1].max_rssi;
            if (count < min_aps || (count == min_aps && max_r < min_rssi)) {
                min_aps = count;
                min_rssi = max_r;
                best_ch = ch;
            }
        }
        best_channel = best_ch;
    }
}

// -------------------------------------------------------------
// 802.11 Deauth Detector Subsystem
// -------------------------------------------------------------
void WirelessRecon::startDeauthMonitor(uint8_t channel) {
    stopAllMonitors();
    deauth_monitor_active = true;
    active_channel = (channel >= 1 && channel <= 14) ? channel : 1;
    resetDeauthStats();

#ifdef ARDUINO
    WiFi.disconnect(false);
    WiFi.mode(WIFI_STA);
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(&wifi_promiscuous_cb);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(active_channel, WIFI_SECOND_CHAN_NONE);
#endif
}

void WirelessRecon::stopDeauthMonitor() {
    if (!deauth_monitor_active) return;
    deauth_monitor_active = false;
    attack_in_progress = false;

#ifdef ARDUINO
    esp_wifi_set_promiscuous(false);
#endif
}

void WirelessRecon::resetDeauthStats() {
    deauth_count = 0;
    deauth_rate_per_sec = 0;
    deauth_window_counter = 0;
    last_deauth_rate_calc = millis();
    attack_in_progress = false;
    memset(&last_attack_event, 0, sizeof(last_attack_event));
}

// -------------------------------------------------------------
// Promiscuous Packet Monitor Subsystem
// -------------------------------------------------------------
void WirelessRecon::startPacketMonitor(uint8_t channel) {
    stopAllMonitors();
    pkt_monitor_active = true;
    active_channel = (channel >= 1 && channel <= 14) ? channel : 1;
    total_packets = 0;
    total_mgmt_pkts = 0;
    total_ctrl_pkts = 0;
    total_data_pkts = 0;
    current_pkt_rate = 0;
    pkt_window_counter = 0;
    last_rate_update = millis();
    pkt_history_head = 0;
    memset(pkt_rate_history, 0, sizeof(pkt_rate_history));

#ifdef ARDUINO
    WiFi.disconnect(false);
    WiFi.mode(WIFI_STA);
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(&wifi_promiscuous_cb);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(active_channel, WIFI_SECOND_CHAN_NONE);
#endif
}

void WirelessRecon::stopPacketMonitor() {
    if (!pkt_monitor_active) return;
    pkt_monitor_active = false;

#ifdef ARDUINO
    esp_wifi_set_promiscuous(false);
#endif
}

void WirelessRecon::setChannel(uint8_t ch) {
    if (ch >= 1 && ch <= 14) {
        active_channel = ch;
#ifdef ARDUINO
        if (deauth_monitor_active || pkt_monitor_active) {
            esp_wifi_set_channel(active_channel, WIFI_SECOND_CHAN_NONE);
        }
#endif
    }
}

void WirelessRecon::cycleChannel() {
    uint8_t next = active_channel + 1;
    if (next > 13) next = 1;
    setChannel(next);
}

void WirelessRecon::getPacketTypePercentages(uint8_t& mgmt_pct, uint8_t& ctrl_pct, uint8_t& data_pct) const {
    uint32_t tot = total_mgmt_pkts + total_ctrl_pkts + total_data_pkts;
    if (tot == 0) {
        mgmt_pct = 0;
        ctrl_pct = 0;
        data_pct = 0;
        return;
    }
    mgmt_pct = (uint8_t)((total_mgmt_pkts * 100) / tot);
    ctrl_pct = (uint8_t)((total_ctrl_pkts * 100) / tot);
    data_pct = (uint8_t)((total_data_pkts * 100) / tot);
}

// -------------------------------------------------------------
// Promiscuous Packet Parsing Logic
// -------------------------------------------------------------
void WirelessRecon::handlePromiscuousPacket(const uint8_t* buf, uint16_t len, int8_t rssi) {
    if (!buf || len < 10) return;

    total_packets++;
    pkt_window_counter++;

    uint16_t fc = (uint16_t)(buf[0] | (buf[1] << 8));
    uint8_t ftype = get_wifi_frame_type(fc);
    uint8_t fsubtype = get_wifi_frame_subtype(fc);

    if (ftype == WIFI_FRAME_TYPE_MGMT) total_mgmt_pkts++;
    else if (ftype == WIFI_FRAME_TYPE_CTRL) total_ctrl_pkts++;
    else if (ftype == WIFI_FRAME_TYPE_DATA) total_data_pkts++;

    // Deauth / Disassociation Detection
    if (ftype == WIFI_FRAME_TYPE_MGMT && (fsubtype == WIFI_SUBTYPE_DEAUTH || fsubtype == WIFI_SUBTYPE_DISASSOC)) {
        deauth_count++;
        deauth_window_counter++;

        if (len >= sizeof(WifiFrameHeader)) {
            const WifiFrameHeader* hdr = (const WifiFrameHeader*)buf;
            last_attack_event.source_mac = hdr->addr2;
            last_attack_event.target_mac = hdr->addr1;
            last_attack_event.rssi = rssi;
            last_attack_event.timestamp = millis();

            if (len >= sizeof(WifiFrameHeader) + 2) {
                last_attack_event.reason_code = (uint16_t)(buf[sizeof(WifiFrameHeader)] | (buf[sizeof(WifiFrameHeader) + 1] << 8));
            } else {
                last_attack_event.reason_code = 0;
            }
        }
    }
}

// -------------------------------------------------------------
// Background Loop
// -------------------------------------------------------------
void WirelessRecon::loop() {
    uint32_t now = millis();

    // 1. Wi-Fi Channel Scan Completion Check
#ifdef ARDUINO
    if (channel_scanning) {
        int n = WiFi.scanComplete();
        if (n >= 0) {
            total_aps_found = 0;
            for (int i = 0; i < 14; i++) {
                channel_stats[i].ap_count = 0;
                channel_stats[i].max_rssi = -100;
            }

            for (int i = 0; i < n; i++) {
                uint8_t ch = WiFi.channel(i);
                int8_t rssi = (int8_t)WiFi.RSSI(i);
                if (ch >= 1 && ch <= 14) {
                    channel_stats[ch - 1].ap_count++;
                    if (rssi > channel_stats[ch - 1].max_rssi) {
                        channel_stats[ch - 1].max_rssi = rssi;
                    }
                    total_aps_found++;
                }
            }
            WiFi.scanDelete();

            // Calculate cleanest channel across 1, 6, 11
            uint8_t cand[3] = {1, 6, 11};
            uint8_t best_ch = 1;
            int min_aps = 99999;
            int min_rssi = 100;
            for (int i = 0; i < 3; i++) {
                uint8_t ch = cand[i];
                int count = channel_stats[ch - 1].ap_count;
                int max_r = channel_stats[ch - 1].max_rssi;
                if (count < min_aps || (count == min_aps && max_r < min_rssi)) {
                    min_aps = count;
                    min_rssi = max_r;
                    best_ch = ch;
                }
            }
            best_channel = best_ch;
            channel_scanning = false;
        }
    }
#endif

    // 2. Deauth Rate & Attack Verification (every 1s)
    if (deauth_monitor_active && now - last_deauth_rate_calc >= 1000) {
        deauth_rate_per_sec = deauth_window_counter;
        deauth_window_counter = 0;
        last_deauth_rate_calc = now;

        if (deauth_rate_per_sec >= 3) {
            attack_in_progress = true;
            attack_alert_timer = now;
#ifdef ARDUINO
            soundManager.playAlert();
            ledManager.setMode(LEDMode::STROBE);
            ledManager.setColor(255, 0, 0); // Red alert
#endif
        } else if (attack_in_progress && now - attack_alert_timer > 3000) {
            attack_in_progress = false;
        }
    }

    // 3. Packet Rate Histogram Calculation (every 250ms)
    if (pkt_monitor_active && now - last_rate_update >= 250) {
        uint32_t dt = now - last_rate_update;
        current_pkt_rate = (uint16_t)(((uint32_t)pkt_window_counter * 1000) / (dt ? dt : 1));
        pkt_window_counter = 0;
        last_rate_update = now;

        // Shift into 64-sample circular buffer (scaled for 24-pixel OLED height)
        uint8_t val = (uint8_t)(current_pkt_rate / 10);
        if (val > 24) val = 24;
        pkt_rate_history[pkt_history_head] = val;
        pkt_history_head = (pkt_history_head + 1) % PKT_RATE_HISTORY_SIZE;
    }
}
