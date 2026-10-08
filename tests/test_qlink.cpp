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
    printf("\nALL Q-LINK PROTOCOL TESTS PASSED!\n");
    return 0;
}
