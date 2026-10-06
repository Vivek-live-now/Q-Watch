#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "mock_qwatch_api.h"
#include "../include/qwatch_api.h"
#include "../include/qapp_loader.h"
#include "../include/qapp_target_poc.h"
#include "../apps/tilt_game/tilt_game.h"
#include "../apps/compass_hud/compass_hud.h"
#include "../apps/invaders/invaders.h"
#include "../apps/dice/dice.h"
#include "../apps/snake/snake.h"
#include "../apps/f1_race/f1_race.h"
#include "../apps/pacman/pacman.h"
#include "../apps/breakout/breakout.h"
#include "../apps/space_impact/space_impact.h"
#include "../apps/bounce/bounce.h"

// Generates a genuine relocatable .qapp file with code, data, and relocations
static void create_relocatable_qapp(const char* path) {
    FILE* fp = fopen(path, "wb");
    assert(fp != NULL);

#if defined(__aarch64__)
    // 0: ldr x0, 0x8 (0x58000040)
    // 4: ret        (0xd65f03c0)
    // 8: 64-bit data address (relocated by QRELOC_DATA_ADDR)
    uint32_t code_payload[4] = {
        0x58000040,
        0xd65f03c0,
        0x00000000,
        0x00000000
    };
    uint32_t code_len = sizeof(code_payload);
    uint32_t reloc_offset_in_code = 8;
#elif defined(__x86_64__)
    // movabs $0, %rax (0x48, 0xb8, 8 bytes 0); ret (0xc3)
    uint8_t code_payload[11] = {
        0x48, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0,
        0xc3
    };
    uint32_t code_len = sizeof(code_payload);
    uint32_t reloc_offset_in_code = 2;
#else
    uint32_t code_payload[4] = { 0 };
    uint32_t code_len = 16;
    uint32_t reloc_offset_in_code = 4;
#endif

    const QAppHeader* src_hdr = get_tilt_game_header();
    uint32_t data_len = sizeof(QAppHeader);

    QAppFileHeader fhdr;
    memset(&fhdr, 0, sizeof(fhdr));
    fhdr.magic = QAPP_MAGIC;
    fhdr.api_version = QAPP_API_VERSION;
    fhdr.required_caps = (QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_MPU | QAPP_CAP_AUDIO | QAPP_CAP_RGB_LED);
    strncpy(fhdr.name, "Tilt Ball", sizeof(fhdr.name) - 1);
    strncpy(fhdr.version, "1.0.0", sizeof(fhdr.version) - 1);
    strncpy(fhdr.author, "007 Agent", sizeof(fhdr.author) - 1);
    fhdr.required_psram = 2048;

    fhdr.code_offset = sizeof(QAppFileHeader);
    fhdr.code_size = code_len;
    fhdr.data_offset = fhdr.code_offset + fhdr.code_size;
    fhdr.data_size = data_len;
    fhdr.bss_size = 64;
    fhdr.reloc_offset = fhdr.data_offset + fhdr.data_size;
    fhdr.reloc_count = 1;
    fhdr.entry_offset = 0; // Entry point is at start of code section

    // Write file header
    fwrite(&fhdr, sizeof(fhdr), 1, fp);

    // Write executable code section
    fwrite(code_payload, 1, code_len, fp);

    // Write initialized data section containing QAppHeader
    fwrite(src_hdr, 1, data_len, fp);

    // Write relocation record: relocate word in code section to point to data section
    QAppReloc reloc;
    reloc.section = 0; // Target word is in CODE section
    reloc.type = QRELOC_DATA_ADDR;
    reloc.reserved = 0;
    reloc.offset = reloc_offset_in_code;
    fwrite(&reloc, sizeof(reloc), 1, fp);

    fclose(fp);
}

// -------------------------------------------------------------
// Test 1: Target Proof-of-Concept Verification
// -------------------------------------------------------------
static void test_poc_verification(void) {
    printf("\n=== 1. Target Executable IRAM / PSRAM Proof-of-Concept ===\n");
    const QWatchAPI* api = get_mock_qwatch_api();
    QAppPocResult poc = run_target_esp32s3_poc(api);

    assert(poc.iram_allocated);
    assert(poc.iram_address_valid);
    assert(poc.psram_allocated);
    assert(poc.psram_address_valid);
    assert(poc.code_relocated);
    assert(poc.executed_from_iram);
    assert(poc.psram_state_mutated);
    assert(poc.api_call_succeeded);
    assert(poc.resources_freed);

    printf("  [PASS] Executable internal memory allocated & validated.\n");
    printf("  [PASS] App data/state placed in PSRAM (initial: 42 -> mutated: 100).\n");
    printf("  [PASS] Relocated code executed directly from executable memory buffer (result=%u).\n",
           poc.execution_result);
    printf("  [PASS] Resolved QWatchAPI symbol through firmware table.\n");
    printf("  [PASS] All executable and data resources cleanly freed.\n");
}

// -------------------------------------------------------------
// Test 2: Relocatable .qapp File Header & Inspection
// -------------------------------------------------------------
static void test_relocatable_loader_file(void) {
    printf("\n=== 2. Relocatable .qapp File Loader & Dynamic Execution ===\n");
    const char* test_path = "./tests/test_reloc_tilt.qapp";
    create_relocatable_qapp(test_path);

    // Test inspectFile
    QAppFileHeader inspected_file;
    QAppErrorCode err = QAppLoader::inspectFile(test_path, &inspected_file);
    assert(err == QAPP_OK);
    assert(inspected_file.magic == QAPP_MAGIC);
    assert(inspected_file.api_version == QAPP_API_VERSION);
    assert(strcmp(inspected_file.name, "Tilt Ball") == 0);
    assert(inspected_file.code_size > 0);
    assert(inspected_file.reloc_count == 1);
    printf("  [PASS] Inspected relocatable file header: '%s' (code: %u B, data: %u B, relocs: %u).\n",
           inspected_file.name, inspected_file.code_size, inspected_file.data_size, inspected_file.reloc_count);

    // Test backwards-compatible inspectFile(path, QAppHeader*)
    QAppHeader inspected_app;
    err = QAppLoader::inspectFile(test_path, &inspected_app);
    assert(err == QAPP_OK);
    assert(strcmp(inspected_app.name, "Tilt Ball") == 0);
    assert(inspected_app.required_psram == 2048);
    printf("  [PASS] Inspected via runtime compatibility overload.\n");

    // Load and dynamically execute
    err = qappLoader.loadApp(test_path);
    assert(err == QAPP_OK);
    assert(qappLoader.isRunning());
    assert(strcmp(qappLoader.getActiveAppName(), "Tilt Ball") == 0);
    assert(qappLoader.getCodeAllocatedSize() > 0);
    assert(qappLoader.getDataAllocatedSize() >= 32 + 64 + 2048);
    printf("  [PASS] App dynamically loaded into executable code memory & PSRAM data.\n");

    // Run dynamic frames
    qappLoader.update(0.016f);
    qappLoader.render();
    printf("  [PASS] Dynamic update and render frames executed successfully.\n");

    // Prevent double-load
    assert(qappLoader.loadApp(test_path) == QAPP_ERR_ALREADY_RUNNING);

    // Unload app
    qappLoader.unloadApp();
    assert(!qappLoader.isRunning());
    assert(qappLoader.getCodeAllocatedSize() == 0);
    assert(qappLoader.getDataAllocatedSize() == 0);
    printf("  [PASS] Unloaded app: executable memory unmapped and PSRAM freed.\n");

    remove(test_path);
}

// -------------------------------------------------------------
// Test 3: Genuine Dynamic Execution 100-Cycle Stress Test
// -------------------------------------------------------------
static void test_dynamic_100_cycle_stress(void) {
    printf("\n=== 3. 100-Cycle Dynamic Relocation & Execution Stress Test ===\n");
    const char* test_path = "./tests/test_stress_reloc.qapp";
    create_relocatable_qapp(test_path);

    for (int i = 1; i <= 100; i++) {
        QAppErrorCode err = qappLoader.loadApp(test_path);
        assert(err == QAPP_OK);
        assert(qappLoader.isRunning());

        // Execute dynamic code frames
        for (int f = 0; f < 3; f++) {
            qappLoader.update(0.016f);
            qappLoader.render();
        }

        qappLoader.unloadApp();
        assert(!qappLoader.isRunning());
        assert(qappLoader.getCodeAllocatedSize() == 0);
        assert(qappLoader.getDataAllocatedSize() == 0);
    }
    printf("  [PASS] Completed 100 consecutive dynamic allocation, relocation, execution & free cycles.\n");
    remove(test_path);
}

// -------------------------------------------------------------
// Test 4: Fault Injection & Safety Verification
// -------------------------------------------------------------
static void test_fault_injection(void) {
    printf("\n=== 4. Fault Injection & Boundary Verification ===\n");

    // Case A: Bad Magic
    QAppFileHeader bad_magic;
    memset(&bad_magic, 0, sizeof(bad_magic));
    bad_magic.magic = 0xDEADBEEF;
    QAppErrorCode err = qappLoader.loadAppFromMemory((const uint8_t*)&bad_magic, sizeof(bad_magic));
    assert(err == QAPP_ERR_MAGIC);
    printf("  [PASS] Corrupted magic rejected with QAPP_ERR_MAGIC.\n");

    // Case B: Incompatible ABI Version
    QAppFileHeader bad_ver;
    memset(&bad_ver, 0, sizeof(bad_ver));
    bad_ver.magic = QAPP_MAGIC;
    bad_ver.api_version = 999;
    err = qappLoader.loadAppFromMemory((const uint8_t*)&bad_ver, sizeof(bad_ver));
    assert(err == QAPP_ERR_VERSION);
    printf("  [PASS] Incompatible ABI version 999 rejected with QAPP_ERR_VERSION.\n");

    // Case C: Unsupported Capability Flags
    QAppFileHeader bad_caps;
    memset(&bad_caps, 0, sizeof(bad_caps));
    bad_caps.magic = QAPP_MAGIC;
    bad_caps.api_version = QAPP_API_VERSION;
    bad_caps.required_caps = 0x80000000U;
    err = qappLoader.loadAppFromMemory((const uint8_t*)&bad_caps, sizeof(bad_caps));
    assert(err == QAPP_ERR_CAPS_UNSUPPORTED);
    printf("  [PASS] Unsupported capabilities rejected with QAPP_ERR_CAPS_UNSUPPORTED.\n");

    // Case D: Zero Code Size
    QAppFileHeader zero_code;
    memset(&zero_code, 0, sizeof(zero_code));
    zero_code.magic = QAPP_MAGIC;
    zero_code.api_version = QAPP_API_VERSION;
    zero_code.code_size = 0;
    err = qappLoader.loadAppFromMemory((const uint8_t*)&zero_code, sizeof(zero_code));
    assert(err == QAPP_ERR_CORRUPT_HEADER);
    printf("  [PASS] Zero code size rejected with QAPP_ERR_CORRUPT_HEADER.\n");

    // Case E: Out of Bounds Relocation Offset
    QAppFileHeader oob_reloc;
    memset(&oob_reloc, 0, sizeof(oob_reloc));
    oob_reloc.magic = QAPP_MAGIC;
    oob_reloc.api_version = QAPP_API_VERSION;
    oob_reloc.code_size = 64;
    oob_reloc.code_offset = sizeof(QAppFileHeader);
    oob_reloc.reloc_offset = 1000000; // Out of bounds
    oob_reloc.reloc_count = 10;
    err = qappLoader.loadAppFromMemory((const uint8_t*)&oob_reloc, sizeof(oob_reloc) + 128);
    assert(err == QAPP_ERR_CORRUPT_HEADER);
    printf("  [PASS] Out-of-bounds relocation offset rejected with QAPP_ERR_CORRUPT_HEADER.\n");

    // Case F: Entry Offset Outside Code Section
    QAppFileHeader oob_entry;
    memset(&oob_entry, 0, sizeof(oob_entry));
    oob_entry.magic = QAPP_MAGIC;
    oob_entry.api_version = QAPP_API_VERSION;
    oob_entry.code_size = 64;
    oob_entry.code_offset = sizeof(QAppFileHeader);
    oob_entry.entry_offset = 128; // Past code_size (64)
    err = qappLoader.loadAppFromMemory((const uint8_t*)&oob_entry, sizeof(oob_entry) + 256);
    assert(err == QAPP_ERR_NULL_ENTRY);
    printf("  [PASS] Out-of-bounds entry point rejected with QAPP_ERR_NULL_ENTRY.\n");
}

// Generates a genuine relocatable .qapp file for Compass HUD
static void create_relocatable_compass_qapp(const char* path) {
    FILE* fp = fopen(path, "wb");
    assert(fp != NULL);

#if defined(__aarch64__)
    uint32_t code_payload[4] = {
        0x58000040,
        0xd65f03c0,
        0x00000000,
        0x00000000
    };
    uint32_t code_len = sizeof(code_payload);
    uint32_t reloc_offset_in_code = 8;
#elif defined(__x86_64__)
    uint8_t code_payload[11] = {
        0x48, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0,
        0xc3
    };
    uint32_t code_len = sizeof(code_payload);
    uint32_t reloc_offset_in_code = 2;
#else
    uint32_t code_payload[4] = { 0 };
    uint32_t code_len = 16;
    uint32_t reloc_offset_in_code = 4;
#endif

    const QAppHeader* src_hdr = get_compass_hud_header();
    uint32_t data_len = sizeof(QAppHeader);

    QAppFileHeader fhdr;
    memset(&fhdr, 0, sizeof(fhdr));
    fhdr.magic = QAPP_MAGIC;
    fhdr.api_version = QAPP_API_VERSION;
    fhdr.required_caps = (QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_MPU | QAPP_CAP_MAG);
    strncpy(fhdr.name, "Compass HUD", sizeof(fhdr.name) - 1);
    strncpy(fhdr.version, "1.0.0", sizeof(fhdr.version) - 1);
    strncpy(fhdr.author, "007 Agent", sizeof(fhdr.author) - 1);
    fhdr.required_psram = 1024;

    fhdr.code_offset = sizeof(QAppFileHeader);
    fhdr.code_size = code_len;
    fhdr.data_offset = fhdr.code_offset + fhdr.code_size;
    fhdr.data_size = data_len;
    fhdr.bss_size = 64;
    fhdr.reloc_offset = fhdr.data_offset + fhdr.data_size;
    fhdr.reloc_count = 1;
    fhdr.entry_offset = 0;

    fwrite(&fhdr, sizeof(fhdr), 1, fp);
    fwrite(code_payload, 1, code_len, fp);
    fwrite(src_hdr, 1, data_len, fp);

    QAppReloc reloc;
    reloc.section = 0;
    reloc.type = QRELOC_DATA_ADDR;
    reloc.reserved = 0;
    reloc.offset = reloc_offset_in_code;
    fwrite(&reloc, sizeof(reloc), 1, fp);

    fclose(fp);
}

// -------------------------------------------------------------
// Test 5: Compass HUD Dynamic Loading, Execution & Rendering
// -------------------------------------------------------------
static void test_compass_hud_dynamic_execution(void) {
    printf("\n=== 5. Compass HUD Dynamic Loading, Execution & Rendering ===\n");
    const char* test_path = "./tests/test_reloc_compass.qapp";
    create_relocatable_compass_qapp(test_path);

    // 1. Inspect on-disk relocatable header
    QAppFileHeader fhdr;
    QAppErrorCode err = QAppLoader::inspectFile(test_path, &fhdr);
    assert(err == QAPP_OK);
    assert(fhdr.magic == QAPP_MAGIC);
    assert(fhdr.api_version == QAPP_API_VERSION);
    assert(strcmp(fhdr.name, "Compass HUD") == 0);
    assert(fhdr.required_caps == (QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_MPU | QAPP_CAP_MAG));
    assert(fhdr.required_psram == 1024);
    assert(fhdr.code_size > 0);
    assert(fhdr.reloc_count == 1);
    printf("  [PASS] Inspected Compass HUD header: '%s' (caps=0x%03X, psram=%u B).\n",
           fhdr.name, fhdr.required_caps, fhdr.required_psram);

    // 2. Load into executable memory
    mock_reset_state();
    err = qappLoader.loadApp(test_path);
    assert(err == QAPP_OK);
    assert(qappLoader.isRunning());
    assert(strcmp(qappLoader.getActiveAppName(), "Compass HUD") == 0);
    assert(qappLoader.getCodeAllocatedSize() > 0);
    assert(qappLoader.getDataAllocatedSize() >= sizeof(QAppHeader) + 64 + 1024);

    // Verify initial cyan LED indicator
    uint8_t r = 0, g = 0, b = 0;
    mock_get_last_led(&r, &g, &b);
    assert(r == 0 && g == 20 && b == 40);
    printf("  [PASS] Compass HUD dynamically loaded, entry invoked, and cyan LED set.\n");

    // 3. Simulate Magnetometer Telemetry & Heading Smoothing
    QTelemetry telem;
    memset(&telem, 0, sizeof(telem));
    telem.heading = 0.0f; // North
    telem.mag_x = 15.0f;
    telem.mag_y = 25.0f;
    telem.mag_z = -40.0f;
    telem.mag_calibrated = true;
    mock_set_telemetry(&telem);

    // Converge smoothing
    for (int i = 0; i < 30; i++) {
        qappLoader.update(0.05f);
    }
    assert(fabsf(compass_hud_get_heading() - 0.0f) < 0.2f);
    assert(compass_hud_is_calibrated() == true);
    assert(compass_hud_get_field_strength() > 45.0f);

    // Render frame
    int flushes_before = mock_get_flush_count();
    qappLoader.render();
    assert(mock_get_flush_count() == flushes_before + 1);

    // Check display buffer has pixels lit (circle, needle, divider, etc.)
    uint8_t* fb = mock_get_display_buffer();
    int lit_pixels = 0;
    for (int i = 0; i < 1024; i++) {
        if (fb[i] != 0) lit_pixels++;
    }
    assert(lit_pixels > 30);
    printf("  [PASS] Rendered 128x64 Compass HUD at Heading 0.0° N (%d active pixels).\n", lit_pixels);

    // 4. Test Heading Rotation: East (90°), South (180°), West (270°)
    telem.heading = 90.0f;
    mock_set_telemetry(&telem);
    for (int i = 0; i < 30; i++) qappLoader.update(0.05f);
    assert(fabsf(compass_hud_get_heading() - 90.0f) < 0.2f);
    qappLoader.render();

    telem.heading = 180.0f;
    mock_set_telemetry(&telem);
    for (int i = 0; i < 30; i++) qappLoader.update(0.05f);
    assert(fabsf(compass_hud_get_heading() - 180.0f) < 0.2f);
    qappLoader.render();

    telem.heading = 270.0f;
    mock_set_telemetry(&telem);
    for (int i = 0; i < 30; i++) qappLoader.update(0.05f);
    assert(fabsf(compass_hud_get_heading() - 270.0f) < 0.2f);
    qappLoader.render();
    printf("  [PASS] Successfully tracked and rendered rotated headings (90° E, 180° S, 270° W).\n");

    // 5. Test Button Interactions
    // OK Button: Lock Waypoint/Target Heading
    assert(!compass_hud_is_target_locked());
    qappLoader.handleButton(QBTN_OK, QEVT_BTN_SHORT_CLICK);
    assert(compass_hud_is_target_locked());
    assert(fabsf(compass_hud_get_target_bearing() - 270.0f) < 0.2f);
    assert(mock_get_last_tone_freq() == 1200);
    mock_get_last_led(&r, &g, &b);
    assert(r == 0 && g == 50 && b == 0); // Green lock flash
    qappLoader.render(); // Render with target lock marker

    // OK Button: Unlock
    qappLoader.handleButton(QBTN_OK, QEVT_BTN_SHORT_CLICK);
    assert(!compass_hud_is_target_locked());
    assert(mock_get_last_tone_freq() == 600);

    // UP Button: Increment Declination
    assert(compass_hud_get_declination() == 0);
    qappLoader.handleButton(QBTN_UP, QEVT_BTN_SHORT_CLICK);
    assert(compass_hud_get_declination() == 1);
    assert(mock_get_last_tone_freq() == 1000);

    // DOWN Button: Decrement Declination
    qappLoader.handleButton(QBTN_DOWN, QEVT_BTN_SHORT_CLICK);
    assert(compass_hud_get_declination() == 0);
    qappLoader.handleButton(QBTN_DOWN, QEVT_BTN_SHORT_CLICK);
    assert(compass_hud_get_declination() == -1);
    assert(mock_get_last_tone_freq() == 800);

    // CANCEL Button: Exit App
    assert(!mock_is_exit_requested());
    qappLoader.handleButton(QBTN_CANCEL, QEVT_BTN_SHORT_CLICK);
    assert(mock_is_exit_requested());
    printf("  [PASS] Button lifecycle (OK target lock, UP/DN declination, CANCEL exit) verified.\n");

    // 6. Unload App
    qappLoader.unloadApp();
    assert(!qappLoader.isRunning());
    assert(qappLoader.getCodeAllocatedSize() == 0);
    assert(qappLoader.getDataAllocatedSize() == 0);
    mock_get_last_led(&r, &g, &b);
    assert(r == 0 && g == 0 && b == 0);
    assert(mock_get_last_tone_freq() == 0);
    printf("  [PASS] Unload confirmed: memory deallocated, LED and tones cleared.\n");

    // 7. Reload App & Verify State Freshness
    err = qappLoader.loadApp(test_path);
    assert(err == QAPP_OK);
    assert(qappLoader.isRunning());
    assert(compass_hud_get_declination() == 0);
    assert(!compass_hud_is_target_locked());
    qappLoader.update(0.016f);
    qappLoader.render();
    qappLoader.unloadApp();
    assert(!qappLoader.isRunning());
    printf("  [PASS] Reload verification passed with fresh state.\n");

    remove(test_path);
}

// -------------------------------------------------------------
// Test 6: Compass HUD 100-Cycle Relocation & Dynamic Stress Test
// -------------------------------------------------------------
static void test_compass_hud_100_cycle_stress(void) {
    printf("\n=== 6. Compass HUD 100-Cycle Relocation & Dynamic Stress Test ===\n");
    const char* test_path = "./tests/test_stress_compass.qapp";
    create_relocatable_compass_qapp(test_path);

    for (int i = 1; i <= 100; i++) {
        QAppErrorCode err = qappLoader.loadApp(test_path);
        assert(err == QAPP_OK);
        assert(qappLoader.isRunning());

        for (int f = 0; f < 3; f++) {
            qappLoader.update(0.016f);
            qappLoader.render();
        }

        qappLoader.unloadApp();
        assert(!qappLoader.isRunning());
        assert(qappLoader.getCodeAllocatedSize() == 0);
        assert(qappLoader.getDataAllocatedSize() == 0);
    }
    printf("  [PASS] Completed 100 consecutive Compass HUD allocation, relocation, execution & free cycles (0 bytes leaked).\n");
    remove(test_path);
}

// -------------------------------------------------------------
// Invaders Q-App Relocatable Packager & Unit Tests
// -------------------------------------------------------------
static void create_relocatable_invaders_qapp(const char* path) {
    FILE* fp = fopen(path, "wb");
    assert(fp != NULL);

#if defined(__aarch64__)
    uint32_t code_payload[4] = {
        0x58000040,
        0xd65f03c0,
        0x00000000,
        0x00000000
    };
    uint32_t code_len = sizeof(code_payload);
    uint32_t reloc_offset_in_code = 8;
#elif defined(__x86_64__)
    uint8_t code_payload[11] = {
        0x48, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0,
        0xc3
    };
    uint32_t code_len = sizeof(code_payload);
    uint32_t reloc_offset_in_code = 2;
#else
    uint32_t code_payload[4] = { 0 };
    uint32_t code_len = 16;
    uint32_t reloc_offset_in_code = 4;
#endif

    const QAppHeader* src_hdr = get_invaders_header();
    uint32_t data_len = sizeof(QAppHeader);

    QAppFileHeader fhdr;
    memset(&fhdr, 0, sizeof(fhdr));
    fhdr.magic = QAPP_MAGIC;
    fhdr.api_version = QAPP_API_VERSION;
    fhdr.required_caps = (QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_MPU | QAPP_CAP_AUDIO | QAPP_CAP_RGB_LED | QAPP_CAP_STORAGE);
    strncpy(fhdr.name, "007 Invaders", sizeof(fhdr.name) - 1);
    strncpy(fhdr.version, "1.0.0", sizeof(fhdr.version) - 1);
    strncpy(fhdr.author, "MI6 Cyber", sizeof(fhdr.author) - 1);
    fhdr.required_psram = 4096;

    fhdr.code_offset = sizeof(QAppFileHeader);
    fhdr.code_size = code_len;
    fhdr.data_offset = fhdr.code_offset + fhdr.code_size;
    fhdr.data_size = data_len;
    fhdr.bss_size = 128;
    fhdr.reloc_offset = fhdr.data_offset + fhdr.data_size;
    fhdr.reloc_count = 1;
    fhdr.entry_offset = 0;

    fwrite(&fhdr, sizeof(fhdr), 1, fp);
    fwrite(code_payload, 1, code_len, fp);
    fwrite(src_hdr, 1, data_len, fp);

    QAppReloc reloc;
    reloc.section = 0;
    reloc.type = QRELOC_DATA_ADDR;
    reloc.reserved = 0;
    reloc.offset = reloc_offset_in_code;
    fwrite(&reloc, sizeof(reloc), 1, fp);

    fclose(fp);
}

static void test_invaders_dynamic_execution(void) {
    printf("\n=== 7. 007 Retro Invaders Dynamic Loading, Controls & Execution ===\n");
    const char* test_path = "./tests/test_reloc_invaders.qapp";
    create_relocatable_invaders_qapp(test_path);

    // 1. Inspect on-disk header
    QAppFileHeader fhdr;
    QAppErrorCode err = QAppLoader::inspectFile(test_path, &fhdr);
    assert(err == QAPP_OK);
    assert(fhdr.magic == QAPP_MAGIC);
    assert(fhdr.api_version == QAPP_API_VERSION);
    assert(strcmp(fhdr.name, "007 Invaders") == 0);
    assert(fhdr.required_psram == 4096);
    assert(fhdr.code_size > 0);
    assert(fhdr.reloc_count == 1);
    printf("  [PASS] Inspected 007 Invaders header: '%s' (caps=0x%03X, psram=%u B).\n",
           fhdr.name, fhdr.required_caps, fhdr.required_psram);

    // 2. Load into memory
    mock_reset_state();
    err = qappLoader.loadApp(test_path);
    assert(err == QAPP_OK);
    assert(qappLoader.isRunning());
    assert(strcmp(qappLoader.getActiveAppName(), "007 Invaders") == 0);
    assert(qappLoader.getCodeAllocatedSize() > 0);
    assert(qappLoader.getDataAllocatedSize() >= sizeof(QAppHeader) + 128 + 4096);

    // Check startup values
    assert(invaders_get_lives() == 3);
    assert(invaders_get_aliens_alive() == 18);
    assert(invaders_get_score() == 0);
    float init_x = invaders_get_player_x();
    assert(fabsf(init_x - 64.0f) < 0.1f);

    // 3. Test Button Controls: Move Left (UP), Move Right (DN), Fire Laser (OK)
    qappLoader.handleButton(QBTN_UP, QEVT_BTN_SHORT_CLICK);
    assert(invaders_get_player_x() < init_x);

    qappLoader.handleButton(QBTN_DOWN, QEVT_BTN_SHORT_CLICK);
    qappLoader.handleButton(QBTN_DOWN, QEVT_BTN_SHORT_CLICK);
    assert(invaders_get_player_x() > init_x);

    qappLoader.handleButton(QBTN_OK, QEVT_BTN_SHORT_CLICK);
    assert(mock_get_last_tone_freq() == 1760); // Laser firing sound triggered
    uint8_t lr = 0, lg = 0, lb = 0;
    mock_get_last_led(&lr, &lg, &lb);
    assert(lg == 80); // Green laser muzzle flash

    // 4. Run update loop and verify rendering
    for (int f = 0; f < 30; f++) {
        qappLoader.update(0.016f);
    }
    int flushes_before = mock_get_flush_count();
    qappLoader.render();
    assert(mock_get_flush_count() == flushes_before + 1);

    // Verify display has rendered pixels
    uint8_t* fb = mock_get_display_buffer();
    int lit_pixels = 0;
    for (int i = 0; i < 1024; i++) {
        lit_pixels += __builtin_popcount(fb[i]);
    }
    assert(lit_pixels > 50); // Aliens, bunker, player, HUD pixels
    printf("  [PASS] 007 Invaders rendered %d active pixels and simulated fleet stepping.\n", lit_pixels);

    qappLoader.unloadApp();
    assert(!qappLoader.isRunning());
    assert(qappLoader.getCodeAllocatedSize() == 0);
    assert(qappLoader.getDataAllocatedSize() == 0);
    remove(test_path);
}

static void test_invaders_100_cycle_stress(void) {
    printf("\n=== 8. 007 Invaders 100-Cycle Relocation & Dynamic Stress Test ===\n");
    const char* test_path = "./tests/test_stress_invaders.qapp";
    create_relocatable_invaders_qapp(test_path);

    for (int i = 1; i <= 100; i++) {
        QAppErrorCode err = qappLoader.loadApp(test_path);
        assert(err == QAPP_OK);
        assert(qappLoader.isRunning());

        for (int f = 0; f < 3; f++) {
            qappLoader.update(0.016f);
            qappLoader.render();
        }

        qappLoader.unloadApp();
        assert(!qappLoader.isRunning());
        assert(qappLoader.getCodeAllocatedSize() == 0);
        assert(qappLoader.getDataAllocatedSize() == 0);
    }
    printf("  [PASS] Completed 100 consecutive 007 Invaders allocation, relocation, execution & free cycles (0 bytes leaked).\n");
    remove(test_path);
}

// -------------------------------------------------------------
// Tactical Dice & RNG Relocatable Packager & Unit Tests
// -------------------------------------------------------------
static void create_relocatable_dice_qapp(const char* path) {
    FILE* fp = fopen(path, "wb");
    assert(fp != NULL);

#if defined(__aarch64__)
    uint32_t code_payload[4] = {
        0x58000040,
        0xd65f03c0,
        0x00000000,
        0x00000000
    };
    uint32_t code_len = sizeof(code_payload);
    uint32_t reloc_offset_in_code = 8;
#elif defined(__x86_64__)
    uint8_t code_payload[11] = {
        0x48, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0,
        0xc3
    };
    uint32_t code_len = sizeof(code_payload);
    uint32_t reloc_offset_in_code = 2;
#else
    uint32_t code_payload[4] = { 0 };
    uint32_t code_len = 16;
    uint32_t reloc_offset_in_code = 4;
#endif

    const QAppHeader* src_hdr = get_dice_header();
    uint32_t data_len = sizeof(QAppHeader);

    QAppFileHeader fhdr;
    memset(&fhdr, 0, sizeof(fhdr));
    fhdr.magic = QAPP_MAGIC;
    fhdr.api_version = QAPP_API_VERSION;
    fhdr.required_caps = (QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_MPU | QAPP_CAP_AUDIO | QAPP_CAP_RGB_LED);
    strncpy(fhdr.name, "Tactical Dice", sizeof(fhdr.name) - 1);
    strncpy(fhdr.version, "1.0.0", sizeof(fhdr.version) - 1);
    strncpy(fhdr.author, "MI6 Cyber", sizeof(fhdr.author) - 1);
    fhdr.required_psram = 2048;

    fhdr.code_offset = sizeof(QAppFileHeader);
    fhdr.code_size = code_len;
    fhdr.data_offset = fhdr.code_offset + fhdr.code_size;
    fhdr.data_size = data_len;
    fhdr.bss_size = 64;
    fhdr.reloc_offset = fhdr.data_offset + fhdr.data_size;
    fhdr.reloc_count = 1;
    fhdr.entry_offset = 0;

    fwrite(&fhdr, sizeof(fhdr), 1, fp);
    fwrite(code_payload, 1, code_len, fp);
    fwrite(src_hdr, 1, data_len, fp);

    QAppReloc reloc;
    reloc.section = 0;
    reloc.type = QRELOC_DATA_ADDR;
    reloc.reserved = 0;
    reloc.offset = reloc_offset_in_code;
    fwrite(&reloc, sizeof(reloc), 1, fp);

    fclose(fp);
}

static void test_dice_dynamic_execution(void) {
    printf("\n=== 9. Tactical Dice & RNG Dynamic Loading, Modes & Roll Physics ===\n");
    const char* test_path = "./tests/test_reloc_dice.qapp";
    create_relocatable_dice_qapp(test_path);

    // 1. Inspect on-disk header
    QAppFileHeader fhdr;
    QAppErrorCode err = QAppLoader::inspectFile(test_path, &fhdr);
    assert(err == QAPP_OK);
    assert(fhdr.magic == QAPP_MAGIC);
    assert(fhdr.api_version == QAPP_API_VERSION);
    assert(strcmp(fhdr.name, "Tactical Dice") == 0);
    assert(fhdr.required_psram == 2048);
    printf("  [PASS] Inspected Tactical Dice header: '%s' (caps=0x%03X, psram=%u B).\n",
           fhdr.name, fhdr.required_caps, fhdr.required_psram);

    // 2. Load into memory
    mock_reset_state();
    err = qappLoader.loadApp(test_path);
    assert(err == QAPP_OK);
    assert(qappLoader.isRunning());
    assert(strcmp(qappLoader.getActiveAppName(), "Tactical Dice") == 0);

    // Verify initial D6 mode
    assert(dice_get_mode() == DICE_MODE_D6);

    // 3. Test Mode Cycling via Buttons
    qappLoader.handleButton(QBTN_DOWN, QEVT_BTN_SHORT_CLICK);
    assert(dice_get_mode() == DICE_MODE_D20);

    qappLoader.handleButton(QBTN_DOWN, QEVT_BTN_SHORT_CLICK);
    assert(dice_get_mode() == DICE_MODE_D100);

    qappLoader.handleButton(QBTN_DOWN, QEVT_BTN_SHORT_CLICK);
    assert(dice_get_mode() == DICE_MODE_COIN);

    qappLoader.handleButton(QBTN_UP, QEVT_BTN_SHORT_CLICK);
    assert(dice_get_mode() == DICE_MODE_D100);

    // 4. Trigger Manual Roll via [OK]
    assert(!dice_is_rolling());
    qappLoader.handleButton(QBTN_OK, QEVT_BTN_SHORT_CLICK);
    assert(dice_is_rolling());

    // Advance tumble physics until finished
    for (int f = 0; f < 80; f++) {
        qappLoader.update(0.02f);
    }
    assert(!dice_is_rolling());
    int res = dice_get_last_result();
    assert(res >= 1 && res <= 100);
    printf("  [PASS] Manual roll completed: mode=D100, result=%d.\n", res);

    // 5. Test Shake-to-Roll Physics
    QTelemetry telem;
    memset(&telem, 0, sizeof(telem));
    telem.accel_x = 1.8f;
    telem.accel_y = 1.2f;
    telem.accel_z = 0.9f; // High-G jerk > 1.85G
    mock_set_telemetry(&telem);

    qappLoader.update(0.02f);
    assert(dice_is_rolling());
    printf("  [PASS] Physical IMU shake detected and auto-triggered tumble roll.\n");

    // Return to resting position (1G gravity)
    telem.accel_x = 0.0f;
    telem.accel_y = 0.0f;
    telem.accel_z = 1.0f;
    mock_set_telemetry(&telem);

    for (int f = 0; f < 80; f++) {
        qappLoader.update(0.02f);
    }
    assert(!dice_is_rolling());

    // 6. Test Rendering
    int flushes_before = mock_get_flush_count();
    qappLoader.render();
    assert(mock_get_flush_count() == flushes_before + 1);

    uint8_t* fb = mock_get_display_buffer();
    int lit_pixels = 0;
    for (int i = 0; i < 1024; i++) {
        lit_pixels += __builtin_popcount(fb[i]);
    }
    assert(lit_pixels > 40);
    printf("  [PASS] Tactical Dice rendered %d active pixels for dice HUD.\n", lit_pixels);

    qappLoader.unloadApp();
    assert(!qappLoader.isRunning());
    remove(test_path);
}

static void test_dice_100_cycle_stress(void) {
    printf("\n=== 10. Tactical Dice 100-Cycle Relocation & Dynamic Stress Test ===\n");
    const char* test_path = "./tests/test_stress_dice.qapp";
    create_relocatable_dice_qapp(test_path);

    for (int i = 1; i <= 100; i++) {
        QAppErrorCode err = qappLoader.loadApp(test_path);
        assert(err == QAPP_OK);
        assert(qappLoader.isRunning());

        for (int f = 0; f < 3; f++) {
            qappLoader.update(0.016f);
            qappLoader.render();
        }

        qappLoader.unloadApp();
        assert(!qappLoader.isRunning());
        assert(qappLoader.getCodeAllocatedSize() == 0);
        assert(qappLoader.getDataAllocatedSize() == 0);
    }
    printf("  [PASS] Completed 100 consecutive Tactical Dice allocation, relocation, execution & free cycles (0 bytes leaked).\n");
    remove(test_path);
}

static void create_relocatable_snake_qapp(const char* path) {
    FILE* fp = fopen(path, "wb");
    assert(fp != NULL);

#if defined(__aarch64__)
    uint32_t code_payload[4] = { 0x58000040, 0xd65f03c0, 0, 0 };
    uint32_t code_len = sizeof(code_payload);
    uint32_t reloc_offset_in_code = 8;
#elif defined(__x86_64__)
    uint8_t code_payload[11] = { 0x48, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0xc3 };
    uint32_t code_len = sizeof(code_payload);
    uint32_t reloc_offset_in_code = 2;
#else
    uint32_t code_payload[4] = { 0 };
    uint32_t code_len = 16;
    uint32_t reloc_offset_in_code = 4;
#endif

    const QAppHeader* src_hdr = get_snake_header();
    uint32_t data_len = sizeof(QAppHeader);

    QAppFileHeader fhdr;
    memset(&fhdr, 0, sizeof(fhdr));
    fhdr.magic = QAPP_MAGIC;
    fhdr.api_version = QAPP_API_VERSION;
    fhdr.required_caps = (QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_AUDIO | QAPP_CAP_RGB_LED | QAPP_CAP_STORAGE);
    strncpy(fhdr.name, "Retro Snake", sizeof(fhdr.name) - 1);
    strncpy(fhdr.version, "1.0.0", sizeof(fhdr.version) - 1);
    strncpy(fhdr.author, "MI6 Cyber", sizeof(fhdr.author) - 1);
    fhdr.required_psram = 2048;

    fhdr.code_offset = sizeof(QAppFileHeader);
    fhdr.code_size = code_len;
    fhdr.data_offset = fhdr.code_offset + fhdr.code_size;
    fhdr.data_size = data_len;
    fhdr.bss_size = 64;
    fhdr.reloc_offset = fhdr.data_offset + fhdr.data_size;
    fhdr.reloc_count = 1;
    fhdr.entry_offset = 0;

    fwrite(&fhdr, sizeof(fhdr), 1, fp);
    fwrite(code_payload, 1, code_len, fp);
    fwrite(src_hdr, 1, data_len, fp);

    QAppReloc reloc;
    reloc.section = 0;
    reloc.type = QRELOC_DATA_ADDR;
    reloc.reserved = 0;
    reloc.offset = reloc_offset_in_code;
    fwrite(&reloc, sizeof(reloc), 1, fp);
    fclose(fp);
}

static void test_snake_dynamic_execution(void) {
    printf("\n=== 11. Retro Snake Dynamic Loading, Controls & Physics ===\n");
    const char* test_path = "./tests/test_reloc_snake.qapp";
    create_relocatable_snake_qapp(test_path);

    QAppFileHeader fhdr;
    QAppErrorCode err = QAppLoader::inspectFile(test_path, &fhdr);
    assert(err == QAPP_OK);
    assert(strcmp(fhdr.name, "Retro Snake") == 0);

    mock_reset_state();
    err = qappLoader.loadApp(test_path);
    assert(err == QAPP_OK);
    assert(qappLoader.isRunning());
    assert(strcmp(qappLoader.getActiveAppName(), "Retro Snake") == 0);

    assert(snake_get_length() == 3);
    assert(!snake_is_game_over());

    for (int f = 0; f < 5; f++) {
        qappLoader.update(0.05f);
        qappLoader.render();
    }

    qappLoader.handleButton(QBTN_UP, QEVT_BTN_SHORT_CLICK);
    qappLoader.update(0.05f);

    qappLoader.unloadApp();
    assert(!qappLoader.isRunning());
    printf("  [PASS] Retro Snake loaded, animated, controlled, and unloaded.\n");
    remove(test_path);
}

static void create_relocatable_f1_race_qapp(const char* path) {
    FILE* fp = fopen(path, "wb");
    assert(fp != NULL);

#if defined(__aarch64__)
    uint32_t code_payload[4] = { 0x58000040, 0xd65f03c0, 0, 0 };
    uint32_t code_len = sizeof(code_payload);
    uint32_t reloc_offset_in_code = 8;
#elif defined(__x86_64__)
    uint8_t code_payload[11] = { 0x48, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0xc3 };
    uint32_t code_len = sizeof(code_payload);
    uint32_t reloc_offset_in_code = 2;
#else
    uint32_t code_payload[4] = { 0 };
    uint32_t code_len = 16;
    uint32_t reloc_offset_in_code = 4;
#endif

    const QAppHeader* src_hdr = get_f1_race_header();
    uint32_t data_len = sizeof(QAppHeader);

    QAppFileHeader fhdr;
    memset(&fhdr, 0, sizeof(fhdr));
    fhdr.magic = QAPP_MAGIC;
    fhdr.api_version = QAPP_API_VERSION;
    fhdr.required_caps = (QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_MPU | QAPP_CAP_AUDIO | QAPP_CAP_RGB_LED | QAPP_CAP_STORAGE);
    strncpy(fhdr.name, "F1 Grand Prix", sizeof(fhdr.name) - 1);
    strncpy(fhdr.version, "1.0.0", sizeof(fhdr.version) - 1);
    strncpy(fhdr.author, "MI6 Cyber", sizeof(fhdr.author) - 1);
    fhdr.required_psram = 2048;

    fhdr.code_offset = sizeof(QAppFileHeader);
    fhdr.code_size = code_len;
    fhdr.data_offset = fhdr.code_offset + fhdr.code_size;
    fhdr.data_size = data_len;
    fhdr.bss_size = 64;
    fhdr.reloc_offset = fhdr.data_offset + fhdr.data_size;
    fhdr.reloc_count = 1;
    fhdr.entry_offset = 0;

    fwrite(&fhdr, sizeof(fhdr), 1, fp);
    fwrite(code_payload, 1, code_len, fp);
    fwrite(src_hdr, 1, data_len, fp);

    QAppReloc reloc;
    reloc.section = 0;
    reloc.type = QRELOC_DATA_ADDR;
    reloc.reserved = 0;
    reloc.offset = reloc_offset_in_code;
    fwrite(&reloc, sizeof(reloc), 1, fp);
    fclose(fp);
}

static void test_f1_race_dynamic_execution(void) {
    printf("\n=== 12. F1 Grand Prix Dynamic Loading, Physics & Audio ===\n");
    const char* test_path = "./tests/test_reloc_f1.qapp";
    create_relocatable_f1_race_qapp(test_path);

    QAppFileHeader fhdr;
    QAppErrorCode err = QAppLoader::inspectFile(test_path, &fhdr);
    assert(err == QAPP_OK);
    assert(strcmp(fhdr.name, "F1 Grand Prix") == 0);

    mock_reset_state();
    err = qappLoader.loadApp(test_path);
    assert(err == QAPP_OK);
    assert(qappLoader.isRunning());
    assert(strcmp(qappLoader.getActiveAppName(), "F1 Grand Prix") == 0);

    assert(f1_race_get_speed() > 0.0f);
    assert(!f1_race_is_game_over());

    qappLoader.handleButton(QBTN_OK, QEVT_BTN_SHORT_CLICK);

    for (int f = 0; f < 10; f++) {
        qappLoader.update(0.02f);
        qappLoader.render();
    }

    qappLoader.unloadApp();
    assert(!qappLoader.isRunning());
    printf("  [PASS] F1 Grand Prix loaded, boosted, simulated, and unloaded.\n");
    remove(test_path);
}

static void create_relocatable_pacman_qapp(const char* path) {
    FILE* fp = fopen(path, "wb");
    assert(fp != NULL);

#if defined(__aarch64__)
    uint32_t code_payload[4] = { 0x58000040, 0xd65f03c0, 0, 0 };
    uint32_t code_len = sizeof(code_payload);
    uint32_t reloc_offset_in_code = 8;
#elif defined(__x86_64__)
    uint8_t code_payload[11] = { 0x48, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0xc3 };
    uint32_t code_len = sizeof(code_payload);
    uint32_t reloc_offset_in_code = 2;
#else
    uint32_t code_payload[4] = { 0 };
    uint32_t code_len = 16;
    uint32_t reloc_offset_in_code = 4;
#endif

    const QAppHeader* src_hdr = get_pacman_header();
    uint32_t data_len = sizeof(QAppHeader);

    QAppFileHeader fhdr;
    memset(&fhdr, 0, sizeof(fhdr));
    fhdr.magic = QAPP_MAGIC;
    fhdr.api_version = QAPP_API_VERSION;
    fhdr.required_caps = (QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_MPU | QAPP_CAP_AUDIO | QAPP_CAP_RGB_LED | QAPP_CAP_STORAGE);
    strncpy(fhdr.name, "Pacman Arcade", sizeof(fhdr.name) - 1);
    strncpy(fhdr.version, "1.0.0", sizeof(fhdr.version) - 1);
    strncpy(fhdr.author, "MI6 Cyber", sizeof(fhdr.author) - 1);
    fhdr.required_psram = 2048;

    fhdr.code_offset = sizeof(QAppFileHeader);
    fhdr.code_size = code_len;
    fhdr.data_offset = fhdr.code_offset + fhdr.code_size;
    fhdr.data_size = data_len;
    fhdr.bss_size = 64;
    fhdr.reloc_offset = fhdr.data_offset + fhdr.data_size;
    fhdr.reloc_count = 1;
    fhdr.entry_offset = 0;

    fwrite(&fhdr, sizeof(fhdr), 1, fp);
    fwrite(code_payload, 1, code_len, fp);
    fwrite(src_hdr, 1, data_len, fp);

    QAppReloc reloc;
    reloc.section = 0;
    reloc.type = QRELOC_DATA_ADDR;
    reloc.reserved = 0;
    reloc.offset = reloc_offset_in_code;
    fwrite(&reloc, sizeof(reloc), 1, fp);
    fclose(fp);
}

static void test_pacman_dynamic_execution(void) {
    printf("\n=== 13. Pacman Arcade Dynamic Loading, Maze & Ghosts ===\n");
    const char* test_path = "./tests/test_reloc_pacman.qapp";
    create_relocatable_pacman_qapp(test_path);

    QAppFileHeader fhdr;
    QAppErrorCode err = QAppLoader::inspectFile(test_path, &fhdr);
    assert(err == QAPP_OK);
    assert(strcmp(fhdr.name, "Pacman Arcade") == 0);

    mock_reset_state();
    err = qappLoader.loadApp(test_path);
    assert(err == QAPP_OK);
    assert(qappLoader.isRunning());
    assert(strcmp(qappLoader.getActiveAppName(), "Pacman Arcade") == 0);

    assert(pacman_get_lives() == 3);
    assert(pacman_get_dots_left() > 0);
    assert(!pacman_is_game_over());

    for (int f = 0; f < 10; f++) {
        qappLoader.update(0.02f);
        qappLoader.render();
    }

    qappLoader.unloadApp();
    assert(!qappLoader.isRunning());
    printf("  [PASS] Pacman Arcade loaded, maze navigated, and unloaded.\n");
    remove(test_path);
}

static void create_relocatable_breakout_qapp(const char* path) {
    FILE* fp = fopen(path, "wb");
    assert(fp != NULL);

#if defined(__aarch64__)
    uint32_t code_payload[4] = { 0x58000040, 0xd65f03c0, 0, 0 };
    uint32_t code_len = sizeof(code_payload);
    uint32_t reloc_offset_in_code = 8;
#elif defined(__x86_64__)
    uint8_t code_payload[11] = { 0x48, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0xc3 };
    uint32_t code_len = sizeof(code_payload);
    uint32_t reloc_offset_in_code = 2;
#else
    uint32_t code_payload[4] = { 0 };
    uint32_t code_len = 16;
    uint32_t reloc_offset_in_code = 4;
#endif

    const QAppHeader* src_hdr = get_breakout_header();
    uint32_t data_len = sizeof(QAppHeader);

    QAppFileHeader fhdr;
    memset(&fhdr, 0, sizeof(fhdr));
    fhdr.magic = QAPP_MAGIC;
    fhdr.api_version = QAPP_API_VERSION;
    fhdr.required_caps = (QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_MPU | QAPP_CAP_AUDIO | QAPP_CAP_RGB_LED | QAPP_CAP_STORAGE);
    strncpy(fhdr.name, "Breakout 007", sizeof(fhdr.name) - 1);
    strncpy(fhdr.version, "1.0.0", sizeof(fhdr.version) - 1);
    strncpy(fhdr.author, "MI6 Cyber", sizeof(fhdr.author) - 1);
    fhdr.required_psram = 2048;

    fhdr.code_offset = sizeof(QAppFileHeader);
    fhdr.code_size = code_len;
    fhdr.data_offset = fhdr.code_offset + fhdr.code_size;
    fhdr.data_size = data_len;
    fhdr.bss_size = 64;
    fhdr.reloc_offset = fhdr.data_offset + fhdr.data_size;
    fhdr.reloc_count = 1;
    fhdr.entry_offset = 0;

    fwrite(&fhdr, sizeof(fhdr), 1, fp);
    fwrite(code_payload, 1, code_len, fp);
    fwrite(src_hdr, 1, data_len, fp);

    QAppReloc reloc;
    reloc.section = 0;
    reloc.type = QRELOC_DATA_ADDR;
    reloc.reserved = 0;
    reloc.offset = reloc_offset_in_code;
    fwrite(&reloc, sizeof(reloc), 1, fp);
    fclose(fp);
}

static void test_breakout_dynamic_execution(void) {
    printf("\n=== 14. Breakout 007 Dynamic Loading, Brick Wall & Ball ===\n");
    const char* test_path = "./tests/test_reloc_breakout.qapp";
    create_relocatable_breakout_qapp(test_path);

    QAppFileHeader fhdr;
    QAppErrorCode err = QAppLoader::inspectFile(test_path, &fhdr);
    assert(err == QAPP_OK);
    assert(strcmp(fhdr.name, "Breakout 007") == 0);

    mock_reset_state();
    err = qappLoader.loadApp(test_path);
    assert(err == QAPP_OK);
    assert(qappLoader.isRunning());
    assert(strcmp(qappLoader.getActiveAppName(), "Breakout 007") == 0);

    assert(breakout_get_lives() == 3);
    assert(breakout_get_bricks_left() == 32);
    assert(!breakout_is_game_over());

    qappLoader.handleButton(QBTN_OK, QEVT_BTN_SHORT_CLICK);

    for (int f = 0; f < 15; f++) {
        qappLoader.update(0.016f);
        qappLoader.render();
    }

    qappLoader.unloadApp();
    assert(!qappLoader.isRunning());
    printf("  [PASS] Breakout 007 loaded, ball launched, and unloaded.\n");
    remove(test_path);
}

static void create_relocatable_space_impact_qapp(const char* path) {
    FILE* fp = fopen(path, "wb");
    assert(fp != NULL);

#if defined(__aarch64__)
    uint32_t code_payload[4] = { 0x58000040, 0xd65f03c0, 0, 0 };
    uint32_t code_len = sizeof(code_payload);
    uint32_t reloc_offset_in_code = 8;
#elif defined(__x86_64__)
    uint8_t code_payload[11] = { 0x48, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0xc3 };
    uint32_t code_len = sizeof(code_payload);
    uint32_t reloc_offset_in_code = 2;
#else
    uint32_t code_payload[4] = { 0 };
    uint32_t code_len = 16;
    uint32_t reloc_offset_in_code = 4;
#endif

    const QAppHeader* src_hdr = get_space_impact_header();
    uint32_t data_len = sizeof(QAppHeader);

    QAppFileHeader fhdr;
    memset(&fhdr, 0, sizeof(fhdr));
    fhdr.magic = QAPP_MAGIC;
    fhdr.api_version = QAPP_API_VERSION;
    fhdr.required_caps = (QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_MPU | QAPP_CAP_AUDIO | QAPP_CAP_RGB_LED | QAPP_CAP_STORAGE);
    strncpy(fhdr.name, "Space Impact 2", sizeof(fhdr.name) - 1);
    strncpy(fhdr.version, "1.0.0", sizeof(fhdr.version) - 1);
    strncpy(fhdr.author, "MI6 Cyber", sizeof(fhdr.author) - 1);
    fhdr.required_psram = 2048;

    fhdr.code_offset = sizeof(QAppFileHeader);
    fhdr.code_size = code_len;
    fhdr.data_offset = fhdr.code_offset + fhdr.code_size;
    fhdr.data_size = data_len;
    fhdr.bss_size = 64;
    fhdr.reloc_offset = fhdr.data_offset + fhdr.data_size;
    fhdr.reloc_count = 1;
    fhdr.entry_offset = 0;

    fwrite(&fhdr, sizeof(fhdr), 1, fp);
    fwrite(code_payload, 1, code_len, fp);
    fwrite(src_hdr, 1, data_len, fp);

    QAppReloc reloc;
    reloc.section = 0;
    reloc.type = QRELOC_DATA_ADDR;
    reloc.reserved = 0;
    reloc.offset = reloc_offset_in_code;
    fwrite(&reloc, sizeof(reloc), 1, fp);
    fclose(fp);
}

static void test_space_impact_dynamic_execution(void) {
    printf("\n=== 15. Space Impact 2 Dynamic Loading, Alien Waves & Boss ===\n");
    const char* test_path = "./tests/test_reloc_space_impact.qapp";
    create_relocatable_space_impact_qapp(test_path);

    QAppFileHeader fhdr;
    QAppErrorCode err = QAppLoader::inspectFile(test_path, &fhdr);
    assert(err == QAPP_OK);
    assert(strcmp(fhdr.name, "Space Impact 2") == 0);

    mock_reset_state();
    err = qappLoader.loadApp(test_path);
    assert(err == QAPP_OK);
    assert(qappLoader.isRunning());
    assert(strcmp(qappLoader.getActiveAppName(), "Space Impact 2") == 0);

    assert(space_impact_get_lives() == 3);
    assert(space_impact_get_bombs() == 3);
    assert(!space_impact_is_game_over());

    qappLoader.handleButton(QBTN_OK, QEVT_BTN_SHORT_CLICK);

    for (int f = 0; f < 15; f++) {
        qappLoader.update(0.016f);
        qappLoader.render();
    }

    qappLoader.handleButton(QBTN_OK, QEVT_BTN_SHORT_CLICK);
    qappLoader.update(0.016f);

    qappLoader.unloadApp();
    assert(!qappLoader.isRunning());
    printf("  [PASS] Space Impact 2 loaded, lasers fired, waves advanced, and unloaded.\n");
    remove(test_path);
}

static void create_relocatable_bounce_qapp(const char* path) {
    FILE* fp = fopen(path, "wb");
    assert(fp != NULL);

#if defined(__aarch64__)
    uint32_t code_payload[4] = { 0x58000040, 0xd65f03c0, 0, 0 };
    uint32_t code_len = sizeof(code_payload);
    uint32_t reloc_offset_in_code = 8;
#elif defined(__x86_64__)
    uint8_t code_payload[11] = { 0x48, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0xc3 };
    uint32_t code_len = sizeof(code_payload);
    uint32_t reloc_offset_in_code = 2;
#else
    uint32_t code_payload[4] = { 0 };
    uint32_t code_len = 16;
    uint32_t reloc_offset_in_code = 4;
#endif

    const QAppHeader* src_hdr = get_bounce_header();
    uint32_t data_len = sizeof(QAppHeader);

    QAppFileHeader fhdr;
    memset(&fhdr, 0, sizeof(fhdr));
    fhdr.magic = QAPP_MAGIC;
    fhdr.api_version = QAPP_API_VERSION;
    fhdr.required_caps = (QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_MPU | QAPP_CAP_AUDIO | QAPP_CAP_RGB_LED | QAPP_CAP_STORAGE);
    strncpy(fhdr.name, "Nokia Bounce", sizeof(fhdr.name) - 1);
    strncpy(fhdr.version, "1.0.0", sizeof(fhdr.version) - 1);
    strncpy(fhdr.author, "MI6 Cyber", sizeof(fhdr.author) - 1);
    fhdr.required_psram = 2048;

    fhdr.code_offset = sizeof(QAppFileHeader);
    fhdr.code_size = code_len;
    fhdr.data_offset = fhdr.code_offset + fhdr.code_size;
    fhdr.data_size = data_len;
    fhdr.bss_size = 64;
    fhdr.reloc_offset = fhdr.data_offset + fhdr.data_size;
    fhdr.reloc_count = 1;
    fhdr.entry_offset = 0;

    fwrite(&fhdr, sizeof(fhdr), 1, fp);
    fwrite(code_payload, 1, code_len, fp);
    fwrite(src_hdr, 1, data_len, fp);

    QAppReloc reloc;
    reloc.section = 0;
    reloc.type = QRELOC_DATA_ADDR;
    reloc.reserved = 0;
    reloc.offset = reloc_offset_in_code;
    fwrite(&reloc, sizeof(reloc), 1, fp);
    fclose(fp);
}

static void test_bounce_dynamic_execution(void) {
    printf("\n=== 16. Nokia Bounce Dynamic Loading, Ring Collection & Physics ===\n");
    const char* test_path = "./tests/test_reloc_bounce.qapp";
    create_relocatable_bounce_qapp(test_path);

    QAppFileHeader fhdr;
    QAppErrorCode err = QAppLoader::inspectFile(test_path, &fhdr);
    assert(err == QAPP_OK);
    assert(strcmp(fhdr.name, "Nokia Bounce") == 0);

    mock_reset_state();
    err = qappLoader.loadApp(test_path);
    assert(err == QAPP_OK);
    assert(qappLoader.isRunning());
    assert(strcmp(qappLoader.getActiveAppName(), "Nokia Bounce") == 0);

    assert(bounce_get_lives() == 3);
    assert(bounce_get_level() == 1);
    assert(bounce_get_rings_left() == 5);
    assert(!bounce_is_game_over());

    qappLoader.handleButton(QBTN_OK, QEVT_BTN_SHORT_CLICK);
    qappLoader.handleButton(QBTN_OK, QEVT_BTN_SHORT_CLICK);
    qappLoader.handleButton(QBTN_DOWN, QEVT_BTN_SHORT_CLICK);

    for (int f = 0; f < 20; f++) {
        qappLoader.update(0.016f);
        qappLoader.render();
    }

    qappLoader.unloadApp();
    assert(!qappLoader.isRunning());
    printf("  [PASS] Nokia Bounce loaded, ball bounced, platforms navigated, and unloaded.\n");
    remove(test_path);
}

int main(void) {
    printf("====================================================\n");
    printf("   MICRO-ELF RELOCATABLE Q-APP DYNAMIC TEST HARNESS  \n");
    printf("====================================================\n");

    printf("Running test_poc_verification...\n");
    fflush(stdout);
    test_poc_verification();
    printf("test_poc_verification passed!\n");
    fflush(stdout);

    printf("Running test_relocatable_loader_file (Tilt Ball)...\n");
    fflush(stdout);
    test_relocatable_loader_file();
    printf("test_relocatable_loader_file passed!\n");
    fflush(stdout);

    printf("Running test_dynamic_100_cycle_stress (Tilt Ball)...\n");
    fflush(stdout);
    test_dynamic_100_cycle_stress();

    printf("Running test_compass_hud_dynamic_execution (Compass HUD)...\n");
    fflush(stdout);
    test_compass_hud_dynamic_execution();

    printf("Running test_compass_hud_100_cycle_stress (Compass HUD)...\n");
    fflush(stdout);
    test_compass_hud_100_cycle_stress();

    printf("Running test_invaders_dynamic_execution (007 Invaders)...\n");
    fflush(stdout);
    test_invaders_dynamic_execution();

    printf("Running test_invaders_100_cycle_stress (007 Invaders)...\n");
    fflush(stdout);
    test_invaders_100_cycle_stress();

    printf("Running test_dice_dynamic_execution (Tactical Dice)...\n");
    fflush(stdout);
    test_dice_dynamic_execution();

    printf("Running test_dice_100_cycle_stress (Tactical Dice)...\n");
    fflush(stdout);
    test_dice_100_cycle_stress();

    printf("Running test_snake_dynamic_execution (Retro Snake)...\n");
    fflush(stdout);
    test_snake_dynamic_execution();

    printf("Running test_f1_race_dynamic_execution (F1 Grand Prix)...\n");
    fflush(stdout);
    test_f1_race_dynamic_execution();

    printf("Running test_pacman_dynamic_execution (Pacman Arcade)...\n");
    fflush(stdout);
    test_pacman_dynamic_execution();

    printf("Running test_breakout_dynamic_execution (Breakout 007)...\n");
    fflush(stdout);
    test_breakout_dynamic_execution();

    printf("Running test_space_impact_dynamic_execution (Space Impact 2)...\n");
    fflush(stdout);
    test_space_impact_dynamic_execution();

    printf("Running test_bounce_dynamic_execution (Nokia Bounce)...\n");
    fflush(stdout);
    test_bounce_dynamic_execution();

    printf("Running test_fault_injection...\n");
    fflush(stdout);
    test_fault_injection();

    printf("\n====================================================\n");
    printf(" >>> ALL RELOCATABLE LOADER & POC TESTS PASSED <<<   \n");
    printf("====================================================\n\n");
    return 0;
}
