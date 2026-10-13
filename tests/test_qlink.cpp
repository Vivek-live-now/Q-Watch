#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include "qlink.h"
#include "anim_engine.h"

// Compact SHA-256 implementation
struct SHA256_Simple {
    uint32_t state[8];
    uint64_t count;
    uint8_t buffer[64];

    static uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
    static uint32_t choose(uint32_t e, uint32_t f, uint32_t g) { return (e & f) ^ (~e & g); }
    static uint32_t majority(uint32_t a, uint32_t b, uint32_t c) { return (a & b) ^ (a & c) ^ (b & c); }
    static uint32_t sig0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
    static uint32_t sig1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
    static uint32_t theta0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
    static uint32_t theta1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

    void transform(const uint8_t data[64]) {
        static const uint32_t K[64] = {
            0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
            0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
            0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
            0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
            0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
            0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
            0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
            0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
        };
        uint32_t W[64];
        for (int i = 0; i < 16; i++) {
            W[i] = ((uint32_t)data[i * 4] << 24) | ((uint32_t)data[i * 4 + 1] << 16) |
                   ((uint32_t)data[i * 4 + 2] << 8) | ((uint32_t)data[i * 4 + 3]);
        }
        for (int i = 16; i < 64; i++) {
            W[i] = theta1(W[i - 2]) + W[i - 7] + theta0(W[i - 15]) + W[i - 16];
        }
        uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
        uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
        for (int i = 0; i < 64; i++) {
            uint32_t T1 = h + sig1(e) + choose(e, f, g) + K[i] + W[i];
            uint32_t T2 = sig0(a) + majority(a, b, c);
            h = g; g = f; f = e; e = d + T1;
            d = c; c = b; b = a; a = T1 + T2;
        }
        state[0] += a; state[1] += b; state[2] += c; state[3] += d;
        state[4] += e; state[5] += f; state[6] += g; state[7] += h;
    }

    void init() {
        state[0] = 0x6a09e667; state[1] = 0xbb67ae85; state[2] = 0x3c6ef372; state[3] = 0xa54ff53a;
        state[4] = 0x510e527f; state[5] = 0x9b05688c; state[6] = 0x1f83d9ab; state[7] = 0x5be0cd19;
        count = 0;
    }

    void update(const uint8_t* data, size_t len) {
        for (size_t i = 0; i < len; i++) {
            buffer[count % 64] = data[i];
            count++;
            if (count % 64 == 0) transform(buffer);
        }
    }

    std::string final() {
        uint64_t total_bits = count * 8;
        buffer[count % 64] = 0x80;
        count++;
        if (count % 64 > 56) {
            while (count % 64 != 0) { buffer[count % 64] = 0; count++; }
            transform(buffer);
        }
        while (count % 64 != 56) { buffer[count % 64] = 0; count++; }
        for (int i = 7; i >= 0; i--) {
            buffer[count % 64] = (uint8_t)((total_bits >> (i * 8)) & 0xFF);
            count++;
        }
        transform(buffer);
        char hex[65];
        for (int i = 0; i < 8; i++) {
            snprintf(hex + i * 8, 9, "%08x", state[i]);
        }
        hex[64] = '\0';
        return std::string(hex);
    }
};

static std::string compute_sha256(const uint8_t* data, size_t len) {
    SHA256_Simple s;
    s.init();
    s.update(data, len);
    return s.final();
}

static std::string compute_file_sha256(const char* filepath) {
    FILE* fp = fopen(filepath, "rb");
    if (!fp) return "";
    SHA256_Simple s;
    s.init();
    uint8_t buf[256];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        s.update(buf, n);
    }
    fclose(fp);
    return s.final();
}

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
    assert(qlink.startFileUpload("/ir/Samsung_BN59.ir", 200) == true);
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

void test_ble_chunk_sequence_and_size_validation() {
    printf("\n--- Test: Q-Link BLE Chunk Sequence & Size Validation ---\n");
    // 1. Missing / out-of-order chunk rejection
    assert(qlink.startFileUpload("/tests/seq_test.bin", 300) == true);
    uint8_t pkt[106];
    pkt[0] = 0xFE; pkt[1] = 0x01;
    pkt[2] = 0x00; pkt[3] = 0x01; // Expected seq 0, but sending seq 1!
    pkt[4] = 0x00; pkt[5] = 100;
    memset(pkt + 6, 0x11, 100);
    qlink.handleBleFilePacket(pkt, sizeof(pkt));
    assert(qlink.isUploadInProgress() == false); // Must abort on sequence mismatch!
    printf("  [PASS] Out-of-order sequence packet aborted transfer.\n");

    // 2. Duplicate chunk rejection
    assert(qlink.startFileUpload("/tests/dup_test.bin", 300) == true);
    pkt[2] = 0x00; pkt[3] = 0x00; // Seq 0
    qlink.handleBleFilePacket(pkt, sizeof(pkt));
    assert(qlink.isUploadInProgress() == true);
    assert(qlink.getUploadReceivedBytes() == 100);
    // Send seq 0 again (duplicate chunk)
    qlink.handleBleFilePacket(pkt, sizeof(pkt));
    assert(qlink.isUploadInProgress() == false); // Duplicate chunk must abort transfer!
    printf("  [PASS] Duplicate sequence chunk rejected and transfer aborted.\n");

    // 3. Oversized transfer rejection
    assert(qlink.startFileUpload("/tests/size_test.bin", 50) == true);
    pkt[2] = 0x00; pkt[3] = 0x00; // Seq 0, but 100 bytes into a 50 byte upload
    qlink.handleBleFilePacket(pkt, sizeof(pkt));
    assert(qlink.isUploadInProgress() == false); // Oversized packet must abort!
    printf("  [PASS] Oversized chunk rejected and transfer aborted.\n");

    // 4. Invalid chunk length declared
    assert(qlink.startFileUpload("/tests/len_test.bin", 100) == true);
    pkt[2] = 0x00; pkt[3] = 0x00;
    pkt[4] = 0x00; pkt[5] = 0x00; // 0 length
    qlink.handleBleFilePacket(pkt, sizeof(pkt));
    assert(qlink.isUploadInProgress() == false); // 0-len must abort!
    printf("  [PASS] Zero-length chunk rejected and transfer aborted.\n");

    // 5. Incomplete transfer rejection (finishFileUpload requires exact count)
    assert(qlink.startFileUpload("/tests/incomp_test.bin", 200) == true);
    pkt[2] = 0x00; pkt[3] = 0x00;
    pkt[4] = 0x00; pkt[5] = 100;
    qlink.handleBleFilePacket(pkt, sizeof(pkt));
    assert(qlink.getUploadReceivedBytes() == 100);
    // Client attempts to finish with expected 200 bytes, but only 100 received
    assert(qlink.finishFileUpload(200) == false);
    assert(qlink.isUploadInProgress() == false);
    printf("  [PASS] finishFileUpload rejects incomplete byte count (100 received != 200 expected).\n");

    // 6. FINISH with mismatched size argument
    assert(qlink.startFileUpload("/tests/mismatch_test.bin", 100) == true);
    pkt[2] = 0x00; pkt[3] = 0x00;
    pkt[4] = 0x00; pkt[5] = 100;
    qlink.handleBleFilePacket(pkt, sizeof(pkt));
    assert(qlink.getUploadReceivedBytes() == 100);
    // FINISH packet claims size is 50 when received is 100
    assert(qlink.finishFileUpload(50) == false);
    assert(qlink.isUploadInProgress() == false);
    printf("  [PASS] finishFileUpload rejects mismatched expected size argument.\n");
}

void test_media_file_upload_integrity_and_decoding() {
    printf("\n--- Test: Q-Link Media File Upload Integrity & Host Decoding ---\n");

    // 1. Build a valid .anim file in memory: 16-byte header + 1 frame of 1024 bytes = 1040 bytes
    std::vector<uint8_t> valid_anim(1040);
    AnimHeader* a_hdr = (AnimHeader*)valid_anim.data();
    a_hdr->magic = ANIM_MAGIC;       // 0x4D4E4151
    a_hdr->version = ANIM_VERSION;   // 1
    a_hdr->width = 128;
    a_hdr->height = 64;
    a_hdr->frame_count = 1;
    a_hdr->frame_delay_ms = 60;
    a_hdr->flags = 0;
    a_hdr->reserved = 0;
    for (size_t i = 16; i < 1040; i++) {
        valid_anim[i] = (uint8_t)(i & 0xFF);
    }
    std::string anim_orig_sha = compute_sha256(valid_anim.data(), valid_anim.size());

    // Upload via framed BLE packets in 5 chunks of 208 bytes
    const char* anim_path = "tests/test_upload.anim";
    remove(anim_path);
    assert(qlink.startFileUpload(anim_path, valid_anim.size()) == true);
    size_t chunk_sz = 208;
    for (uint16_t s = 0; s < 5; s++) {
        uint8_t chunk_pkt[6 + 208];
        chunk_pkt[0] = 0xFE; chunk_pkt[1] = 0x01;
        chunk_pkt[2] = (uint8_t)((s >> 8) & 0xFF);
        chunk_pkt[3] = (uint8_t)(s & 0xFF);
        chunk_pkt[4] = (uint8_t)((chunk_sz >> 8) & 0xFF);
        chunk_pkt[5] = (uint8_t)(chunk_sz & 0xFF);
        memcpy(chunk_pkt + 6, valid_anim.data() + s * chunk_sz, chunk_sz);
        qlink.handleBleFilePacket(chunk_pkt, sizeof(chunk_pkt));
        assert(qlink.isUploadInProgress() == true);
    }
    assert(qlink.getUploadReceivedBytes() == valid_anim.size());
    assert(qlink.finishFileUpload(valid_anim.size()) == true);

    // Verify watch-side file size and SHA-256 equality
    FILE* af = fopen(anim_path, "rb");
    assert(af != NULL);
    fseek(af, 0, SEEK_END);
    long af_size = ftell(af);
    fclose(af);
    assert(af_size == 1040);
    std::string anim_disk_sha = compute_file_sha256(anim_path);
    assert(anim_disk_sha == anim_orig_sha);
    printf("  [PASS] .anim BLE upload: size=1040 B, SHA256=%s matches original byte-for-byte.\n", anim_orig_sha.c_str());

    // Verify decoded with real firmware AnimEngine
    assert(animEngine.open(anim_path) == true);
    assert(animEngine.isOpen() == true);
    assert(animEngine.getTotalFrames() == 1);
    assert(animEngine.getHeader().frame_delay_ms == 60);
    assert(strcmp(animEngine.getLastError(), "OK") == 0);
    animEngine.close();
    remove(anim_path);
    printf("  [PASS] .anim opens and parses successfully with AnimEngine.\n");

    // 2. Corrupt .anim rejection (bad magic)
    std::vector<uint8_t> corrupt_anim = valid_anim;
    corrupt_anim[0] = 'B'; corrupt_anim[1] = 'A'; corrupt_anim[2] = 'D'; corrupt_anim[3] = '!';
    assert(qlink.startFileUpload(anim_path, corrupt_anim.size()) == true);
    for (uint16_t s = 0; s < 5; s++) {
        uint8_t chunk_pkt[6 + 208];
        chunk_pkt[0] = 0xFE; chunk_pkt[1] = 0x01;
        chunk_pkt[2] = (uint8_t)((s >> 8) & 0xFF);
        chunk_pkt[3] = (uint8_t)(s & 0xFF);
        chunk_pkt[4] = (uint8_t)((chunk_sz >> 8) & 0xFF);
        chunk_pkt[5] = (uint8_t)(chunk_sz & 0xFF);
        memcpy(chunk_pkt + 6, corrupt_anim.data() + s * chunk_sz, chunk_sz);
        qlink.handleBleFilePacket(chunk_pkt, sizeof(chunk_pkt));
    }
    // finishFileUpload MUST detect invalid header, delete corrupt file, and return false
    assert(qlink.finishFileUpload(corrupt_anim.size()) == false);
    FILE* check_anim = fopen(anim_path, "rb");
    assert(check_anim == NULL); // File must be deleted!
    printf("  [PASS] Corrupt .anim rejected by finishFileUpload and purged from disk.\n");

    // 3. Build a valid 1-bit BMP file in memory: 14B file hdr + 40B DIB + 8B palette + 1024B raster = 1086 bytes
    std::vector<uint8_t> valid_bmp(1086, 0);
    valid_bmp[0] = 'B'; valid_bmp[1] = 'M';
    uint32_t bmp_size = 1086;
    valid_bmp[2] = (uint8_t)(bmp_size & 0xFF); valid_bmp[3] = (uint8_t)((bmp_size >> 8) & 0xFF);
    valid_bmp[10] = 0x3E; // pixel offset = 62
    // DIB Header (40 bytes)
    valid_bmp[14] = 40; // hdr size
    valid_bmp[18] = 128; // width
    valid_bmp[22] = 64;  // height
    valid_bmp[26] = 1;   // planes
    valid_bmp[28] = 1;   // bpp = 1
    valid_bmp[30] = 0;   // BI_RGB
    valid_bmp[34] = 0; valid_bmp[35] = 4; // image size = 1024
    valid_bmp[46] = 2;   // colors used = 2
    // Palette (2 colors * 4B = 8B)
    valid_bmp[54 + 4] = 0xFF; valid_bmp[54 + 5] = 0xFF; valid_bmp[54 + 6] = 0xFF;
    // Pixel raster
    for (size_t i = 62; i < 1086; i++) {
        valid_bmp[i] = (uint8_t)(i * 7);
    }
    std::string bmp_orig_sha = compute_sha256(valid_bmp.data(), valid_bmp.size());

    // Upload via framed BLE packets
    const char* bmp_path = "tests/test_upload.bmp";
    remove(bmp_path);
    assert(qlink.startFileUpload(bmp_path, valid_bmp.size()) == true);
    size_t bmp_chunk_sz = 181; // 6 chunks of 181 = 1086
    for (uint16_t s = 0; s < 6; s++) {
        uint8_t chunk_pkt[6 + 181];
        chunk_pkt[0] = 0xFE; chunk_pkt[1] = 0x01;
        chunk_pkt[2] = (uint8_t)((s >> 8) & 0xFF);
        chunk_pkt[3] = (uint8_t)(s & 0xFF);
        chunk_pkt[4] = (uint8_t)((bmp_chunk_sz >> 8) & 0xFF);
        chunk_pkt[5] = (uint8_t)(bmp_chunk_sz & 0xFF);
        memcpy(chunk_pkt + 6, valid_bmp.data() + s * bmp_chunk_sz, bmp_chunk_sz);
        qlink.handleBleFilePacket(chunk_pkt, sizeof(chunk_pkt));
        assert(qlink.isUploadInProgress() == true);
    }
    assert(qlink.getUploadReceivedBytes() == valid_bmp.size());
    assert(qlink.finishFileUpload(valid_bmp.size()) == true);

    FILE* bf = fopen(bmp_path, "rb");
    assert(bf != NULL);
    fseek(bf, 0, SEEK_END);
    long bf_size = ftell(bf);
    fclose(bf);
    assert(bf_size == 1086);
    std::string bmp_disk_sha = compute_file_sha256(bmp_path);
    assert(bmp_disk_sha == bmp_orig_sha);
    printf("  [PASS] .bmp BLE upload: size=1086 B, SHA256=%s matches original byte-for-byte.\n", bmp_orig_sha.c_str());

    // Verify decoded with real firmware AnimEngine
    assert(animEngine.open(bmp_path) == true);
    assert(animEngine.isOpen() == true);
    assert(animEngine.isBmpFormat() == true);
    assert(strcmp(animEngine.getLastError(), "OK") == 0);
    animEngine.close();
    remove(bmp_path);
    printf("  [PASS] .bmp opens and parses successfully with AnimEngine.\n");

    // 4. Corrupt .bmp rejection (unsupported bpp = 16)
    std::vector<uint8_t> corrupt_bmp = valid_bmp;
    corrupt_bmp[28] = 16; // bpp = 16 (unsupported)
    assert(qlink.startFileUpload(bmp_path, corrupt_bmp.size()) == true);
    for (uint16_t s = 0; s < 6; s++) {
        uint8_t chunk_pkt[6 + 181];
        chunk_pkt[0] = 0xFE; chunk_pkt[1] = 0x01;
        chunk_pkt[2] = (uint8_t)((s >> 8) & 0xFF);
        chunk_pkt[3] = (uint8_t)(s & 0xFF);
        chunk_pkt[4] = (uint8_t)((bmp_chunk_sz >> 8) & 0xFF);
        chunk_pkt[5] = (uint8_t)(bmp_chunk_sz & 0xFF);
        memcpy(chunk_pkt + 6, corrupt_bmp.data() + s * bmp_chunk_sz, bmp_chunk_sz);
        qlink.handleBleFilePacket(chunk_pkt, sizeof(chunk_pkt));
    }
    assert(qlink.finishFileUpload(corrupt_bmp.size()) == false);
    FILE* check_bmp = fopen(bmp_path, "rb");
    assert(check_bmp == NULL); // File must be deleted!
    printf("  [PASS] Corrupt .bmp (unsupported bpp) rejected by finishFileUpload and purged from disk.\n");
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
    test_ble_chunk_sequence_and_size_validation();
    test_media_file_upload_integrity_and_decoding();
    test_ir_parsed_protocol_transmission();
    test_ir_raw_timing_transfer_and_chunking();
    printf("\nALL Q-LINK PROTOCOL TESTS PASSED!\n");
    return 0;
}
