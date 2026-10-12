#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "qlink.h"

// Mock dependencies for host test
class MockButtonManager {
public:
    uint8_t last_injected_id;
    uint8_t last_injected_evt;
    void injectEvent(uint8_t id, uint8_t evt) {
        last_injected_id = id;
        last_injected_evt = evt;
    }
};

void test_compact_telemetry_layout() {
    printf("--- Test: Q-Link Compact Telemetry Layout ---\n");
    assert(sizeof(QLinkCompactTelemetry) == 32);
    printf("  [PASS] sizeof(QLinkCompactTelemetry) == 32 bytes exactly.\n");

    QLinkCompactTelemetry tel;
    memset(&tel, 0, sizeof(tel));
    qlink.fillCompactTelemetry(tel);

    assert(tel.magic == QLINK_MAGIC);
    assert(tel.magic == 0x514C);
    printf("  [PASS] Magic 0x514C verified.\n");

    assert(tel.battery_pct > 0 && tel.battery_pct <= 100);
    assert(tel.battery_mv >= 3000 && tel.battery_mv <= 4500);
    printf("  [PASS] Battery telemetry range verified: %d%% (%dmV)\n", tel.battery_pct, tel.battery_mv);

    assert(tel.temp_c_x10 != 0);
    assert(tel.pressure_hpa_x10 > 5000); // > 500 hPa
    assert(tel.humidity_pct_x10 <= 1000);
    printf("  [PASS] Environmental telemetry verified: Temp=%.1f C, Press=%.1f hPa, Hum=%.1f %%\n",
           tel.temp_c_x10 / 10.0f, tel.pressure_hpa_x10 / 10.0f, tel.humidity_pct_x10 / 10.0f);
}

void test_button_injection_parsing() {
    printf("\n--- Test: Q-Link Button Injection Parsing ---\n");
    assert(qlink.injectButton("UP", "SHORT_PRESS") == true);
    assert(qlink.injectButton("OK", "LONG_PRESS") == true);
    assert(qlink.injectButton("DOWN", "SHORT_PRESS") == true);
    assert(qlink.injectButton("CANCEL", "SHORT_PRESS") == true);
    assert(qlink.injectButton("INVALID_BTN", "SHORT_PRESS") == false);
    printf("  [PASS] Button injection parsing handles UP, OK, DOWN, CANCEL and rejects invalid inputs.\n");
}

void test_sync_handlers() {
    printf("\n--- Test: Q-Link Sync Handlers ---\n");
    uint32_t sample_epoch = 1790247600;
    int32_t sample_offset = 19800; // +5:30
    assert(qlink.syncTime(sample_epoch, sample_offset) == true);
    printf("  [PASS] syncTime accepted epoch=%u, offset=%d\n", sample_epoch, sample_offset);

    assert(qlink.syncWeather("Bengaluru", 26.5f, 55, 800, "Clear Sky", 29.0f, 19.5f) == true);
    printf("  [PASS] syncWeather accepted payload.\n");
}

void test_streaming_state() {
    printf("\n--- Test: Q-Link Display Streaming State ---\n");
    qlink.setStreamingDisplay(true, 30);
    assert(qlink.isStreamingDisplay() == true);
    assert(qlink.getTargetFps() == 30);

    qlink.setStreamingDisplay(false);
    assert(qlink.isStreamingDisplay() == false);
    printf("  [PASS] Display streaming state & FPS rate gating verified.\n");
}

void test_qapp_sideload_validation() {
    printf("\n--- Test: Q-Link Q-App Sideload Validation ---\n");
    // 1. Buffer too small
    uint8_t tiny_buf[10] = { 0 };
    String name, ver, err;
    size_t sz = 0;
    assert(qlink.validateQAppHeader(tiny_buf, sizeof(tiny_buf), name, ver, sz, &err) == false);
    assert(err.indexOf("smaller than QAppFileHeader") != -1);

    // 2. Corrupt magic
    QAppFileHeader bad_hdr;
    memset(&bad_hdr, 0, sizeof(bad_hdr));
    bad_hdr.magic = 0xDEADBEEF;
    assert(qlink.validateQAppHeader((const uint8_t*)&bad_hdr, sizeof(bad_hdr), name, ver, sz, &err) == false);
    assert(err.indexOf("Invalid QAPP magic") != -1);

    // 3. Valid QApp file from disk (invaders.qapp)
    FILE* fp = fopen("apps/invaders.qapp", "rb");
    assert(fp != NULL);
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    assert(fsize > 0);
    uint8_t* inv_buf = (uint8_t*)malloc(fsize);
    assert(fread(inv_buf, 1, fsize, fp) == (size_t)fsize);
    fclose(fp);

    assert(qlink.validateQAppHeader(inv_buf, fsize, name, ver, sz, &err) == true);
    assert(name == "007 Invaders");
    assert(ver == "1.0.0");
    assert(sz == (size_t)fsize);
    printf("  [PASS] Verified invaders.qapp: name='%s', version='%s', size=%zu B\n", name.c_str(), ver.c_str(), sz);
    free(inv_buf);

    // 4. Valid QApp file from disk (dice.qapp)
    fp = fopen("apps/dice.qapp", "rb");
    assert(fp != NULL);
    fseek(fp, 0, SEEK_END);
    fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    assert(fsize > 0);
    uint8_t* dice_buf = (uint8_t*)malloc(fsize);
    assert(fread(dice_buf, 1, fsize, fp) == (size_t)fsize);
    fclose(fp);

    assert(qlink.validateQAppHeader(dice_buf, fsize, name, ver, sz, &err) == true);
    assert(name == "Tactical Dice");
    assert(ver == "1.0.0");
    assert(sz == (size_t)fsize);
    printf("  [PASS] Verified dice.qapp: name='%s', version='%s', size=%zu B\n", name.c_str(), ver.c_str(), sz);
    free(dice_buf);
}

void test_companion_subsystem_endpoints() {
    printf("\n--- Test: Q-Link Companion Subsystem Endpoints (Mochi, Power, Sigint) ---\n");
    // 1. Mochi Actions
    assert(qlink.handleMochiAction("pet") == true);
    assert(qlink.handleMochiAction("feed") == true);
    assert(qlink.handleMochiAction("wake") == true);
    assert(qlink.handleMochiAction("helmet", 0) == true);
    assert(qlink.handleMochiAction("helmet", 4) == true);
    assert(qlink.handleMochiAction("helmet", 5) == false);
    assert(qlink.handleMochiAction("invalid_action") == false);
    printf("  [PASS] handleMochiAction handles pet, feed, wake, helmet (0-4) and rejects invalid.\n");

    // 2. Power Governor State
    assert(qlink.setPowerProfileState("PERFORMANCE", false, false, false) == true);
    assert(qlink.setPowerProfileState("BALANCED", true, true, false) == true);
    assert(qlink.setPowerProfileState("ENDURANCE", true, true, true) == true);
    assert(qlink.setPowerProfileState("INVALID_PROFILE", false, false, false) == false);
    printf("  [PASS] setPowerProfileState switches governor profiles and guards invalid names.\n");

    // 3. Power Profile JSON Layout
    String power_json = qlink.generatePowerProfileJson();
    assert(power_json.indexOf("profile") != -1);
    assert(power_json.indexOf("cpu_mhz") != -1);
    assert(power_json.indexOf("battery_pct") != -1);
    assert(power_json.indexOf("voltage_v") != -1);
    printf("  [PASS] generatePowerProfileJson contains essential power metrics.\n");

    // 4. SIGINT Scan JSON Layout
    String sigint_json = qlink.generateSigintScanJson();
    assert(sigint_json.indexOf("active_channel") != -1);
    assert(sigint_json.indexOf("best_channel") != -1);
    assert(sigint_json.indexOf("channels") != -1);
    assert(sigint_json.indexOf("targets") != -1);
    printf("  [PASS] generateSigintScanJson contains RF spectrum and BLE radar payloads.\n");
}

void test_mochi_anim_validation() {
    printf("\n--- Test: Q-Link Animation Market Asset Validation ---\n");
    // 1. Short buffer
    uint8_t tiny[12] = { 0 };
    uint16_t frames = 0, delay = 0;
    String err;
    assert(qlink.validateAnimHeader(tiny, sizeof(tiny), frames, delay, &err) == false);
    assert(err.indexOf("smaller than AnimHeader") != -1);

    // 2. Corrupt magic
    uint8_t bad_magic[16] = { 'B', 'A', 'D', '!', 1, 0, 128, 64, 1, 0, 50, 0, 1, 0, 0, 0 };
    assert(qlink.validateAnimHeader(bad_magic, sizeof(bad_magic), frames, delay, &err) == false);
    assert(err.indexOf("Invalid ANIM magic") != -1);

    // 3. Wrong dimensions
    uint8_t bad_dim[16] = { 'Q', 'A', 'N', 'M', 1, 0, 64, 32, 1, 0, 50, 0, 1, 0, 0, 0 };
    assert(qlink.validateAnimHeader(bad_dim, sizeof(bad_dim), frames, delay, &err) == false);
    assert(err.indexOf("Invalid dimensions") != -1);

    // 4. Size mismatch
    uint8_t size_mismatch[16] = { 'Q', 'A', 'N', 'M', 1, 0, 128, 64, 2, 0, 50, 0, 1, 0, 0, 0 };
    assert(qlink.validateAnimHeader(size_mismatch, sizeof(size_mismatch), frames, delay, &err) == false);
    assert(err.indexOf("Size mismatch") != -1);

    // 5. Valid file from market assets (extras/mochi_market/adore.anim)
    FILE* fp = fopen("extras/mochi_market/adore.anim", "rb");
    if (!fp) fp = fopen("data/mochi/dancing.anim", "rb");
    assert(fp != NULL);
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    assert(fsize > 16);
    uint8_t* anim_buf = (uint8_t*)malloc(fsize);
    assert(fread(anim_buf, 1, fsize, fp) == (size_t)fsize);
    fclose(fp);

    assert(qlink.validateAnimHeader(anim_buf, fsize, frames, delay, &err) == true);
    assert(frames > 0);
    assert(delay > 0);
    assert(fsize == (long)(16 + (size_t)frames * 1024));
    printf("  [PASS] Verified Mochi anim: frames=%u, delay=%u ms, total_size=%ld B\n", frames, delay, fsize);
    free(anim_buf);
}

void test_safe_delete_and_storage() {
    printf("\n--- Test: Q-Link Safe Deletion & Storage Telemetry ---\n");
    // 1. Safe deletion path checks
    assert(qlink.safeDeleteAnim("/mochi/dancing.anim") == true);
    assert(qlink.safeDeleteAnim("../../../etc/passwd") == false); // Path traversal rejected
    printf("  [PASS] safeDeleteAnim validates safe paths and unlinks cleanly.\n");

    // 2. Storage JSON payload
    String storage_json = qlink.generateStorageJson();
    assert(storage_json.indexOf("fs_total_bytes") != -1);
    assert(storage_json.indexOf("fs_used_bytes") != -1);
    assert(storage_json.indexOf("fs_free_bytes") != -1);
    assert(storage_json.indexOf("free_anim_slots") != -1);
    printf("  [PASS] generateStorageJson contains total, used, free, and slot metrics.\n");
}

void test_ble_file_upload_protocol() {
    printf("\n--- Test: Q-Link BLE File Upload Protocol ---\n");
    // 1. Path safety and initialization
    assert(qlink.startFileUpload("../evil.ir", 500) == false);
    assert(qlink.startFileUpload("", 500) == false);
    assert(qlink.startFileUpload("/ir/Samsung_BN59.ir", 1000) == true);
    assert(qlink.isUploadInProgress() == true);
    assert(qlink.getUploadPath() == "/ir/Samsung_BN59.ir");
    assert(qlink.getUploadReceivedBytes() == 0);

    // 2. Direct chunk processing
    uint8_t sample_chunk[100];
    memset(sample_chunk, 0x55, sizeof(sample_chunk));
    assert(qlink.processFileChunk(sample_chunk, sizeof(sample_chunk)) == true);
    assert(qlink.getUploadReceivedBytes() == 100);

    // 3. Binary framed packet [0xFE, 0x01, seq(2B), len(2B), data...]
    uint8_t framed_packet[106];
    framed_packet[0] = 0xFE;
    framed_packet[1] = 0x01;
    framed_packet[2] = 0x00; // seq hi
    framed_packet[3] = 0x01; // seq lo
    framed_packet[4] = 0x00; // len hi
    framed_packet[5] = 100;  // len lo
    memset(framed_packet + 6, 0xAA, 100);
    qlink.handleBleFilePacket(framed_packet, sizeof(framed_packet));
    assert(qlink.getUploadReceivedBytes() == 200);

    // 4. Finish upload
    assert(qlink.finishFileUpload(200) == true);
    assert(qlink.isUploadInProgress() == false);

    // 5. Cancel upload
    assert(qlink.startFileUpload("/ir/temp.ir", 50) == true);
    qlink.cancelFileUpload();
    assert(qlink.isUploadInProgress() == false);
    printf("  [PASS] BLE chunked file transfer START, framed packets, received size, and FINISH verified.\n");
}

void test_ir_parsed_protocol_transmission() {
    printf("\n--- Test: Q-Link IR Parsed Protocol Transmission & Validation ---\n");
    // 1. Valid NEC 32-bit
    assert(qlink.transmitParsedIr("NEC", 0x04, 0x08, 32) == true);
    assert(qlink.last_tx_proto == "NEC");
    assert(qlink.last_tx_addr == 0x04);
    assert(qlink.last_tx_cmd == 0x08);
    assert(qlink.last_tx_nbits == 32);

    // 2. Default bit length resolution when nbits = 0
    assert(qlink.transmitParsedIr("SONY", 0x01, 0x12, 0) == true);
    assert(qlink.last_tx_nbits == 12); // Default Sony is 12

    assert(qlink.transmitParsedIr("PANASONIC", 0x10, 0x20, 0) == true);
    assert(qlink.last_tx_nbits == 48); // Default Panasonic is 48

    // 3. Strict 24-bit enforcement for RCA and Nikai
    assert(qlink.transmitParsedIr("NIKAI", 0x00, 0x807F, 0) == true);
    assert(qlink.last_tx_nbits == 24);

    assert(qlink.transmitParsedIr("NIKAI", 0x00, 0x807F, 24) == true);
    assert(qlink.last_tx_nbits == 24);

    assert(qlink.transmitParsedIr("NIKAI", 0x00, 0x807F, 32) == false); // Non-24b Nikai rejected!
    assert(qlink.transmitParsedIr("NIKAI", 0x00, 0x807F, 16) == false);

    assert(qlink.transmitParsedIr("RCA", 0x04, 0x0C, 0) == true);
    assert(qlink.last_tx_nbits == 24);

    assert(qlink.transmitParsedIr("RCA", 0x04, 0x0C, 24) == true);
    assert(qlink.last_tx_nbits == 24);

    assert(qlink.transmitParsedIr("RCA", 0x04, 0x0C, 32) == false); // Non-24b RCA rejected!

    // 4. Invalid protocol / out-of-range bit length rejection
    assert(qlink.transmitParsedIr("", 0x00, 0x00, 32) == false); // Empty protocol rejected
    assert(qlink.transmitParsedIr("NEC", 0x00, 0x00, 256) == false); // > 128 bits rejected
    assert(qlink.transmitParsedIr("NEC", 0x00, 0x00, 150) == false);

    printf("  [PASS] Parsed IR transmission validated across NEC, Sony, Panasonic, strict 24b RCA/Nikai, and range checks.\n");
}

void test_ir_raw_timing_transfer_and_chunking() {
    printf("\n--- Test: Q-Link Raw IR Timing Transfer, Chunking & Integrity ---\n");

    // 1. Rejection of invalid session start
    assert(qlink.startRawIr(0, 38000) == false); // Empty count rejected
    assert(qlink.startRawIr(1025, 38000) == false); // Oversized (>1024) rejected
    assert(qlink.startRawIr(2000, 38000) == false);
    assert(qlink.startRawIr(100, 150000) == false); // Freq > 100kHz rejected

    // 2. Successful multi-chunk transfer session (100 timings = 200 bytes)
    assert(qlink.startRawIr(100, 38000) == true);
    assert(qlink.isRawIrInProgress() == true);
    assert(qlink.getRawIrExpectedCount() == 100);
    assert(qlink.getRawIrReceivedCount() == 0);
    assert(qlink.getRawIrFrequency() == 38000);

    // Chunk 0: 60 timings = 120 bytes (seq 0)
    uint8_t chunk0[6 + 120];
    chunk0[0] = 0xFE;
    chunk0[1] = 0x03; // Magic for raw IR chunk
    chunk0[2] = 0x00; // seq hi
    chunk0[3] = 0x00; // seq lo (0)
    chunk0[4] = 0x00; // len hi
    chunk0[5] = 120;  // len lo
    for (int i = 0; i < 60; i++) {
        uint16_t t = 500 + i * 10;
        chunk0[6 + i * 2] = (uint8_t)(t & 0xFF);
        chunk0[6 + i * 2 + 1] = (uint8_t)((t >> 8) & 0xFF);
    }
    qlink.handleBleFilePacket(chunk0, sizeof(chunk0));
    assert(qlink.getRawIrReceivedCount() == 60);
    assert(qlink.isRawIrInProgress() == true);

    // Chunk 1: 40 timings = 80 bytes (seq 1) -> Reaches total 100
    uint8_t chunk1[6 + 80];
    chunk1[0] = 0xFE;
    chunk1[1] = 0x03;
    chunk1[2] = 0x00;
    chunk1[3] = 0x01; // seq 1
    chunk1[4] = 0x00;
    chunk1[5] = 80;
    for (int i = 0; i < 40; i++) {
        uint16_t t = 1000 + i * 10;
        chunk1[6 + i * 2] = (uint8_t)(t & 0xFF);
        chunk1[6 + i * 2 + 1] = (uint8_t)((t >> 8) & 0xFF);
    }
    qlink.handleBleFilePacket(chunk1, sizeof(chunk1));
    assert(qlink.isRawIrInProgress() == false); // Auto-completed & transmitted on final chunk!
    assert(qlink.last_tx_raw_count == 100);
    assert(qlink.last_tx_raw_freq == 38000);

    // 3. Out-of-order sequence packet rejection
    assert(qlink.startRawIr(50, 38000) == true);
    uint8_t bad_seq[6 + 20];
    bad_seq[0] = 0xFE;
    bad_seq[1] = 0x03;
    bad_seq[2] = 0x00;
    bad_seq[3] = 0x05; // Expected seq 0, but got 5!
    bad_seq[4] = 0x00;
    bad_seq[5] = 20;
    for (int i = 0; i < 10; i++) {
        bad_seq[6 + i * 2] = 0x50;
        bad_seq[6 + i * 2 + 1] = 0x00;
    }
    qlink.handleBleFilePacket(bad_seq, sizeof(bad_seq));
    assert(qlink.isRawIrInProgress() == false); // Aborted due to sequence mismatch!

    // 4. Zero timing duration rejection
    assert(qlink.startRawIr(10, 38000) == true);
    uint8_t zero_t[6 + 20];
    zero_t[0] = 0xFE;
    zero_t[1] = 0x03;
    zero_t[2] = 0x00;
    zero_t[3] = 0x00;
    zero_t[4] = 0x00;
    zero_t[5] = 20;
    memset(zero_t + 6, 0, 20); // All 0 timings!
    qlink.handleBleFilePacket(zero_t, sizeof(zero_t));
    assert(qlink.isRawIrInProgress() == false); // Aborted due to zero duration!

    // 5. Incomplete transfer cannot be transmitted (never transmit partial data)
    assert(qlink.startRawIr(50, 38000) == true);
    uint8_t part_chunk[6 + 20];
    part_chunk[0] = 0xFE;
    part_chunk[1] = 0x03;
    part_chunk[2] = 0x00;
    part_chunk[3] = 0x00;
    part_chunk[4] = 0x00;
    part_chunk[5] = 20;
    for (int i = 0; i < 10; i++) {
        part_chunk[6 + i * 2] = 0x64;
        part_chunk[6 + i * 2 + 1] = 0x00;
    }
    qlink.handleBleFilePacket(part_chunk, sizeof(part_chunk));
    assert(qlink.getRawIrReceivedCount() == 10);
    // Attempt to finish early
    assert(qlink.finishRawIr(50) == false); // Rejected: received 10 != expected 50!
    assert(qlink.isRawIrInProgress() == false); // Aborted without transmitting!

    // 6. Explicit cancel
    assert(qlink.startRawIr(20, 38000) == true);
    qlink.cancelRawIr();
    assert(qlink.isRawIrInProgress() == false);

    printf("  [PASS] Raw IR multi-chunk streaming, framing (0xFE, 0x03), sequence check, zero-guard, and partial abort verified.\n");
}

int main() {
    printf("==========================================\n");
    printf(" RUNNING Q-LINK PROTOCOL VERIFICATION\n");
    printf("==========================================\n");
    test_compact_telemetry_layout();
    test_button_injection_parsing();
    test_sync_handlers();
    test_streaming_state();
    test_qapp_sideload_validation();
    test_companion_subsystem_endpoints();
    test_mochi_anim_validation();
    test_safe_delete_and_storage();
    test_ble_file_upload_protocol();
    test_ir_parsed_protocol_transmission();
    test_ir_raw_timing_transfer_and_chunking();
    printf("\nALL Q-LINK PROTOCOL TESTS PASSED!\n");
    return 0;
}
