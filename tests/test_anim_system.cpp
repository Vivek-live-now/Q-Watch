#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "../include/anim_engine.h"

static void create_test_anim(const char* filepath, uint16_t frame_count, uint16_t delay_ms, uint16_t flags) {
    FILE* fp = fopen(filepath, "wb");
    assert(fp != NULL);

    AnimHeader hdr;
    hdr.magic = ANIM_MAGIC;
    hdr.version = ANIM_VERSION;
    hdr.width = ANIM_WIDTH;
    hdr.height = ANIM_HEIGHT;
    hdr.frame_count = frame_count;
    hdr.frame_delay_ms = delay_ms;
    hdr.flags = flags;
    hdr.reserved = 0;

    fwrite(&hdr, 1, sizeof(hdr), fp);

    uint8_t frame_buf[ANIM_FRAME_BYTES];
    for (uint16_t f = 0; f < frame_count; f++) {
        memset(frame_buf, (uint8_t)(f + 1), sizeof(frame_buf));
        fwrite(frame_buf, 1, sizeof(frame_buf), fp);
    }

    fclose(fp);
}

static void create_test_bmp(const char* filepath) {
    FILE* fp = fopen(filepath, "wb");
    assert(fp != NULL);

    // 14-byte BMP file header
    uint8_t file_hdr[14] = {
        'B', 'M',           // Magic
        0x3E, 0x04, 0x00, 0x00, // File size = 14 + 40 + 8 + 1024 = 1086 bytes (0x043E)
        0x00, 0x00, 0x00, 0x00, // Reserved
        0x3E, 0x00, 0x00, 0x00  // Pixel offset = 62 bytes (0x3E)
    };
    fwrite(file_hdr, 1, sizeof(file_hdr), fp);

    // 40-byte BITMAPINFOHEADER
    uint8_t info_hdr[40] = {
        0x28, 0x00, 0x00, 0x00, // Header size = 40
        0x80, 0x00, 0x00, 0x00, // Width = 128
        0x40, 0x00, 0x00, 0x00, // Height = 64 (bottom-up)
        0x01, 0x00,             // Planes = 1
        0x01, 0x00,             // Bit count = 1-bit
        0x00, 0x00, 0x00, 0x00, // Compression = BI_RGB
        0x00, 0x04, 0x00, 0x00, // Image size = 1024
        0x00, 0x00, 0x00, 0x00, // X pixels per meter
        0x00, 0x00, 0x00, 0x00, // Y pixels per meter
        0x02, 0x00, 0x00, 0x00, // Colors used = 2
        0x00, 0x00, 0x00, 0x00  // Colors important
    };
    fwrite(info_hdr, 1, sizeof(info_hdr), fp);

    // 8-byte Palette (Color 0 = Black, Color 1 = White)
    uint8_t palette[8] = {
        0x00, 0x00, 0x00, 0x00,
        0xFF, 0xFF, 0xFF, 0x00
    };
    fwrite(palette, 1, sizeof(palette), fp);

    // 1024 bytes bitmap data (64 rows of 16 bytes each)
    // In bottom-up BMP, row 0 in file is bottom row (y=63)
    // Row 63 in file is top row (y=0)
    // Let's write byte 0x80 to first byte of top row (Row 63 in file)
    // 0x80 has MSB set. Bit reversal -> 0x01 (LSB set)
    uint8_t rows[1024];
    memset(rows, 0, sizeof(rows));
    // Row 63 is at offset 63 * 16 = 1008
    rows[63 * 16] = 0x80;
    // Row 0 in file (y=63 on screen)
    rows[0] = 0x03; // binary 00000011 -> bit reversed 11000000 (0xC0)

    fwrite(rows, 1, sizeof(rows), fp);
    fclose(fp);
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    printf("=== Running Animation Engine Unit Tests ===\n");

    // 1. Header Spec Verification
    printf("[1/5] Verifying AnimHeader specification...\n");
    assert(sizeof(AnimHeader) == 16);
    assert(ANIM_MAGIC == 0x4D4E4151);
    assert(ANIM_VERSION == 1);
    assert(ANIM_WIDTH == 128);
    assert(ANIM_HEIGHT == 64);
    assert(ANIM_FRAME_BYTES == 1024);
    printf("  PASS: Header size (16 bytes), magic (QANM), and geometry (128x64) verified.\n");

    // 2. Multi-Frame .anim Playback & State Machine Test
    printf("[2/5] Testing multi-frame .anim playback & controls...\n");
    const char* anim_file = "tests/test_sample.anim";
    create_test_anim(anim_file, 5, 50, ANIM_FLAG_LOOP);

    assert(animEngine.open(anim_file) == true);
    assert(animEngine.isOpen() == true);
    assert(animEngine.isBmpFormat() == false);
    assert(animEngine.getTotalFrames() == 5);
    assert(animEngine.getCurrentFrame() == 0);
    assert(animEngine.getEffectiveDelayMs() == 50);
    assert(animEngine.isLooping() == true);
    assert(animEngine.isPlaying() == true);
    assert(animEngine.isPaused() == false);

    // Verify first frame buffer content (0x01)
    const uint8_t* fb = animEngine.getFrameBuffer();
    assert(fb[0] == 0x01 && fb[1023] == 0x01);

    // Test step forward & backward
    animEngine.stepForward();
    assert(animEngine.getCurrentFrame() == 1);
    assert(animEngine.getFrameBuffer()[0] == 0x02);

    animEngine.stepBackward();
    assert(animEngine.getCurrentFrame() == 0);
    assert(animEngine.getFrameBuffer()[0] == 0x01);

    // Test seek
    assert(animEngine.seekFrame(4) == true);
    assert(animEngine.getCurrentFrame() == 4);
    assert(animEngine.getFrameBuffer()[0] == 0x05);
    assert(animEngine.seekFrame(5) == false); // Out of bounds

    // Test pause / resume / toggle
    animEngine.pause();
    assert(animEngine.isPaused() == true);
    assert(animEngine.isPlaying() == false);

    animEngine.resume();
    assert(animEngine.isPaused() == false);
    assert(animEngine.isPlaying() == true);

    animEngine.togglePlayPause();
    assert(animEngine.isPaused() == true);
    animEngine.togglePlayPause();
    assert(animEngine.isPaused() == false);

    // Test speed multipliers
    animEngine.setSpeedMultiplier(1.0f);
    assert(animEngine.getEffectiveDelayMs() == 50);

    animEngine.cycleSpeed(); // 1.0 -> 1.5
    assert(animEngine.getSpeedMultiplier() == 1.5f);
    assert(animEngine.getEffectiveDelayMs() == (uint16_t)(50.0f / 1.5f));

    animEngine.cycleSpeed(); // 1.5 -> 2.0
    assert(animEngine.getSpeedMultiplier() == 2.0f);
    assert(animEngine.getEffectiveDelayMs() == 25);

    animEngine.cycleSpeed(); // 2.0 -> 0.5
    assert(animEngine.getSpeedMultiplier() == 0.5f);
    assert(animEngine.getEffectiveDelayMs() == 100);

    animEngine.cycleSpeed(); // 0.5 -> 1.0
    assert(animEngine.getSpeedMultiplier() == 1.0f);

    animEngine.close();
    assert(animEngine.isOpen() == false);
    unlink(anim_file);
    printf("  PASS: Multi-frame .anim playback, step, seek, and speed scaling verified.\n");

    // 3. 1-Bit Windows BMP Parser & Bit Reversal Test
    printf("[3/5] Testing 1-bit BMP parser & MSB-to-LSB bit conversion...\n");
    const char* bmp_file = "tests/test_sample.bmp";
    create_test_bmp(bmp_file);

    assert(animEngine.open(bmp_file) == true);
    assert(animEngine.isOpen() == true);
    assert(animEngine.isBmpFormat() == true);
    assert(animEngine.getTotalFrames() == 1);
    assert(animEngine.getCurrentFrame() == 0);

    // Row 0 in display (row 63 in file) had 0x80 -> should be 0x01 in XBM
    const uint8_t* bmp_fb = animEngine.getFrameBuffer();
    assert(bmp_fb[0] == 0x01);

    // Row 63 in display (row 0 in file) had 0x03 -> bit reversed to 0xC0
    assert(bmp_fb[63 * 16] == 0xC0);

    animEngine.close();
    unlink(bmp_file);
    printf("  PASS: 1-bit BMP scanline parsing and MSB-to-LSB bit reversal verified.\n");

    // 4. Boot Animation Configuration Test
    printf("[4/5] Testing boot animation setup...\n");
    create_test_anim(anim_file, 3, 40, ANIM_FLAG_LOOP);
    assert(animEngine.setAsBootAnimation(anim_file) == true);
    assert(access("boot.anim", F_OK) == 0);
    unlink("boot.anim");
    unlink(anim_file);

    create_test_bmp(bmp_file);
    assert(animEngine.setAsBootAnimation(bmp_file) == true);
    assert(access("boot.bmp", F_OK) == 0);
    unlink("boot.bmp");
    unlink(bmp_file);
    printf("  PASS: setAsBootAnimation file copying and format routing verified.\n");

    // 5. Fault Injection & Malformed File Handling
    printf("[5/5] Testing fault injection & error handling...\n");
    // Non-existent file
    assert(animEngine.open("tests/nonexistent_file_xyz.anim") == false);

    // Invalid magic
    FILE* bad_fp = fopen("tests/bad_magic.anim", "wb");
    AnimHeader bad_hdr;
    bad_hdr.magic = 0xDEADBEEF;
    bad_hdr.version = 1;
    bad_hdr.width = 128;
    bad_hdr.height = 64;
    bad_hdr.frame_count = 1;
    bad_hdr.frame_delay_ms = 50;
    bad_hdr.flags = 0;
    bad_hdr.reserved = 0;
    fwrite(&bad_hdr, 1, sizeof(bad_hdr), bad_fp);
    fclose(bad_fp);
    assert(animEngine.open("tests/bad_magic.anim") == false);
    unlink("tests/bad_magic.anim");

    // Invalid dimensions
    bad_fp = fopen("tests/bad_dim.anim", "wb");
    bad_hdr.magic = ANIM_MAGIC;
    bad_hdr.width = 64; // Invalid
    bad_hdr.height = 32;
    fwrite(&bad_hdr, 1, sizeof(bad_hdr), bad_fp);
    fclose(bad_fp);
    assert(animEngine.open("tests/bad_dim.anim") == false);
    unlink("tests/bad_dim.anim");

    // Zero frames
    bad_fp = fopen("tests/zero_frame.anim", "wb");
    bad_hdr.width = 128;
    bad_hdr.height = 64;
    bad_hdr.frame_count = 0;
    fwrite(&bad_hdr, 1, sizeof(bad_hdr), bad_fp);
    fclose(bad_fp);
    assert(animEngine.open("tests/zero_frame.anim") == false);
    unlink("tests/zero_frame.anim");

    // Truncated file
    bad_fp = fopen("tests/trunc.anim", "wb");
    bad_hdr.frame_count = 1;
    fwrite(&bad_hdr, 1, sizeof(bad_hdr), bad_fp);
    uint8_t partial[100] = {0};
    fwrite(partial, 1, sizeof(partial), bad_fp); // Only 100 bytes instead of 1024
    fclose(bad_fp);
    assert(animEngine.open("tests/trunc.anim") == false);
    unlink("tests/trunc.anim");

    printf("  PASS: Corrupt magic, bad dimensions, zero frames, and truncation rejected safely.\n");

    printf("\nALL ANIMATION ENGINE TESTS PASSED!\n");
    return 0;
}
