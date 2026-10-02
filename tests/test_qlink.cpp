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

int main() {
    printf("==========================================\n");
    printf(" RUNNING Q-LINK PROTOCOL VERIFICATION\n");
    printf("==========================================\n");
    test_compact_telemetry_layout();
    test_button_injection_parsing();
    test_sync_handlers();
    test_streaming_state();
    test_qapp_sideload_validation();
    printf("\nALL Q-LINK PROTOCOL TESTS PASSED!\n");
    return 0;
}
