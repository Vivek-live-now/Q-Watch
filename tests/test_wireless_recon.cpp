#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "../include/wireless_recon.h"

int main() {
    printf("=== Running Wireless Recon Suite Unit Tests ===\n");

    // 1. 802.11 Frame Control and Header Parsing
    printf("[1/5] Verifying 802.11 Frame Control and Subtype decoding...\n");
    // Beacon frame: Type 0 (Mgmt), Subtype 8 (Beacon) -> FC = (8 << 4) | (0 << 2) = 0x0080
    uint16_t fc_beacon = 0x0080;
    assert(get_wifi_frame_type(fc_beacon) == WIFI_FRAME_TYPE_MGMT);
    assert(get_wifi_frame_subtype(fc_beacon) == WIFI_SUBTYPE_BEACON);

    // Deauth frame: Type 0 (Mgmt), Subtype 12 (0x0C) -> FC = (12 << 4) | (0 << 2) = 0x00C0
    uint16_t fc_deauth = 0x00C0;
    assert(get_wifi_frame_type(fc_deauth) == WIFI_FRAME_TYPE_MGMT);
    assert(get_wifi_frame_subtype(fc_deauth) == WIFI_SUBTYPE_DEAUTH);

    // Disassoc frame: Type 0 (Mgmt), Subtype 10 (0x0A) -> FC = (10 << 4) | (0 << 2) = 0x00A0
    uint16_t fc_disassoc = 0x00A0;
    assert(get_wifi_frame_type(fc_disassoc) == WIFI_FRAME_TYPE_MGMT);
    assert(get_wifi_frame_subtype(fc_disassoc) == WIFI_SUBTYPE_DISASSOC);

    // Data frame: Type 2 (Data), Subtype 0 -> FC = 0x0008
    uint16_t fc_data = 0x0008;
    assert(get_wifi_frame_type(fc_data) == WIFI_FRAME_TYPE_DATA);

    // Control ACK: Type 1 (Ctrl), Subtype 13 (0x0D) -> FC = (13 << 4) | (1 << 2) = 0x00D4
    uint16_t fc_ack = 0x00D4;
    assert(get_wifi_frame_type(fc_ack) == WIFI_FRAME_TYPE_CTRL);

    printf("  PASS: 802.11 Frame Control type and subtype masks verified.\n");

    // 2. Deauth Flood Attack Detection & Promiscuous Packet Parsing
    printf("[2/5] Testing Deauth/Disassociation Attack Detection...\n");
    wirelessRecon.startDeauthMonitor(6);
    assert(wirelessRecon.isDeauthMonitorActive() == true);
    assert(wirelessRecon.getDeauthCount() == 0);

    // Craft simulated deauth frame packet
    uint8_t deauth_pkt[32] = {0};
    WifiFrameHeader* deauth_hdr = (WifiFrameHeader*)deauth_pkt;
    deauth_hdr->frame_control = fc_deauth;
    deauth_hdr->duration = 314;
    // Broadcast target: FF:FF:FF:FF:FF:FF
    memset(deauth_hdr->addr1.bytes, 0xFF, 6);
    // Attacker source: 00:11:22:33:44:55
    uint8_t attacker_mac[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    memcpy(deauth_hdr->addr2.bytes, attacker_mac, 6);
    // BSSID: 66:77:88:99:AA:BB
    uint8_t bssid[6] = {0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB};
    memcpy(deauth_hdr->addr3.bytes, bssid, 6);
    // Reason code: 7 (Class 3 frame from nonassociated station)
    deauth_pkt[sizeof(WifiFrameHeader)] = 0x07;
    deauth_pkt[sizeof(WifiFrameHeader) + 1] = 0x00;

    // Inject 5 deauth packets
    for (int i = 0; i < 5; i++) {
        wirelessRecon.handlePromiscuousPacket(deauth_pkt, sizeof(deauth_pkt), -55);
    }
    assert(wirelessRecon.getDeauthCount() == 5);

    const DeauthAttackEvent& ev = wirelessRecon.getLastAttackEvent();
    assert(ev.reason_code == 7);
    assert(ev.rssi == -55);
    assert(ev.target_mac.isBroadcast() == true);
    assert(memcmp(ev.source_mac.bytes, attacker_mac, 6) == 0);

    char mac_str[24];
    WirelessRecon::formatMac(ev.source_mac.bytes, mac_str, sizeof(mac_str));
    assert(strcmp(mac_str, "00:11:22:33:44:55") == 0);

    wirelessRecon.stopDeauthMonitor();
    assert(wirelessRecon.isDeauthMonitorActive() == false);
    printf("  PASS: Promiscuous deauth packet parsing, MAC extraction, and attack stats verified.\n");

    // 3. BLE Radar & Proximity Tracking Math
    printf("[3/5] Testing BLE Radar proximity & distance calculations...\n");
    // Proximity at point blank (-35 dBm) -> 1.0
    float prox_close = wirelessRecon.getTargetProximity(-35);
    assert(fabsf(prox_close - 1.0f) < 0.01f);

    // Proximity at lost signal (-95 dBm) -> 0.0
    float prox_far = wirelessRecon.getTargetProximity(-95);
    assert(fabsf(prox_far - 0.0f) < 0.01f);

    // Proximity at midpoint (-65 dBm) -> 0.5
    float prox_mid = wirelessRecon.getTargetProximity(-65);
    assert(fabsf(prox_mid - 0.5f) < 0.01f);

    // Distance at reference RSSI (-59 dBm) -> 1.0 meter
    float dist_1m = wirelessRecon.getTargetEstimatedDistance(-59);
    assert(fabsf(dist_1m - 1.0f) < 0.1f);

    // Geiger counter tick interval: close (-35) -> 60ms, far (-95) -> 1000ms
    uint16_t tick_close = wirelessRecon.getGeigerTickInterval(-35);
    uint16_t tick_far = wirelessRecon.getGeigerTickInterval(-95);
    assert(tick_close == 60);
    assert(tick_far == 1000);

    // Target injection and lock
    wirelessRecon.testInjectBleTarget("MI6_BEACON", "11:22:33:44:55:66", -50);
    wirelessRecon.testInjectBleTarget("UNKNOWN_TAG", "AA:BB:CC:DD:EE:FF", -80);
    assert(wirelessRecon.getBleTargetCount() == 2);

    wirelessRecon.lockTarget(0);
    assert(wirelessRecon.hasTargetLock() == true);
    assert(strcmp(wirelessRecon.getLockedTarget()->name, "MI6_BEACON") == 0);
    assert(wirelessRecon.getLockedTarget()->rssi == -50);

    wirelessRecon.unlockTarget();
    assert(wirelessRecon.hasTargetLock() == false);
    printf("  PASS: BLE logarithmic distance model, proximity mapping, and Geiger intervals verified.\n");

    // 4. Wi-Fi Channel Spectrum Analyzer
    printf("[4/5] Testing Wi-Fi 2.4GHz Channel Spectrum Analyzer...\n");
    wirelessRecon.startChannelScan();
    // Simulate scan results:
    // Channel 1: 5 APs (congested)
    // Channel 6: 1 AP (cleanest)
    // Channel 11: 3 APs
    for (int i = 0; i < 5; i++) wirelessRecon.testInjectWifiScan(1, -70);
    wirelessRecon.testInjectWifiScan(6, -85);
    for (int i = 0; i < 3; i++) wirelessRecon.testInjectWifiScan(11, -75);

    const WifiChannelStat* ch_stats = wirelessRecon.getChannelStats();
    assert(ch_stats[0].ap_count == 5); // Channel 1
    assert(ch_stats[5].ap_count == 1); // Channel 6
    assert(ch_stats[10].ap_count == 3); // Channel 11
    assert(wirelessRecon.getTotalApsFound() == 9);
    assert(wirelessRecon.getBestChannel() == 6); // Channel 6 has least APs
    printf("  PASS: 2.4GHz spectrum AP tally and cleanest channel recommendation verified.\n");

    // 5. Promiscuous Packet Rate & Type Percentages
    printf("[5/5] Testing Promiscuous Packet Monitor & Type Ratios...\n");
    wirelessRecon.startPacketMonitor(1);
    assert(wirelessRecon.isPacketMonitorActive() == true);
    assert(wirelessRecon.getActiveChannel() == 1);

    wirelessRecon.cycleChannel();
    assert(wirelessRecon.getActiveChannel() == 2);

    // Inject mixed traffic: 20 mgmt, 50 ctrl, 30 data -> 100 total
    uint8_t dummy_pkt[16] = {0};
    // Mgmt
    dummy_pkt[0] = 0x00; dummy_pkt[1] = 0x00;
    for (int i = 0; i < 20; i++) wirelessRecon.handlePromiscuousPacket(dummy_pkt, 16, -60);
    // Ctrl
    dummy_pkt[0] = 0x04; dummy_pkt[1] = 0x00;
    for (int i = 0; i < 50; i++) wirelessRecon.handlePromiscuousPacket(dummy_pkt, 16, -60);
    // Data
    dummy_pkt[0] = 0x08; dummy_pkt[1] = 0x00;
    for (int i = 0; i < 30; i++) wirelessRecon.handlePromiscuousPacket(dummy_pkt, 16, -60);

    assert(wirelessRecon.getTotalPackets() == 100);
    uint8_t m_pct = 0, c_pct = 0, d_pct = 0;
    wirelessRecon.getPacketTypePercentages(m_pct, c_pct, d_pct);
    assert(m_pct == 20);
    assert(c_pct == 50);
    assert(d_pct == 30);

    wirelessRecon.stopPacketMonitor();
    assert(wirelessRecon.isPacketMonitorActive() == false);
    printf("  PASS: Promiscuous packet counting, channel hopping, and traffic breakdown verified.\n");

    printf("\nALL WIRELESS RECON TESTS PASSED!\n");
    return 0;
}
