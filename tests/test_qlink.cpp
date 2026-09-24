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

int main() {
    printf("==========================================\n");
    printf(" RUNNING Q-LINK PROTOCOL VERIFICATION\n");
    printf("==========================================\n");
    test_compact_telemetry_layout();
    test_button_injection_parsing();
    test_sync_handlers();
    test_streaming_state();
    printf("\nALL Q-LINK PROTOCOL TESTS PASSED!\n");
    return 0;
}
