#include "anim_engine.h"
#include <math.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <LittleFS.h>
#include "display.h"
#include "sound_manager.h"
#include "hw_config.h"
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <strings.h>

static inline uint32_t millis() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
#endif

AnimEngine animEngine;

// Bit reversal table helper for BMP MSB-to-LSB conversion
static inline uint8_t reverse_byte_bits(uint8_t b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

AnimEngine::AnimEngine() :
#ifdef ARDUINO
    anim_file(),
#else
    anim_file(NULL),
#endif
    is_open(false),
    is_bmp(false),
    is_playing(false),
    is_paused(false),
    is_looping(true),
    speed_multiplier(1.0f),
    current_frame_idx(0),
    last_frame_time(0),
    bmp_pixel_offset(62),
    bmp_flip_y(true)
{
    memset(&current_header, 0, sizeof(current_header));
    memset(current_filepath, 0, sizeof(current_filepath));
    memset(frame_buffer, 0, sizeof(frame_buffer));
}

AnimEngine::~AnimEngine() {
    close();
}

bool AnimEngine::begin() {
#ifdef ARDUINO
    if (!LittleFS.exists("/boot")) {
        LittleFS.mkdir("/boot");
    }
    if (!LittleFS.exists("/anim")) {
        LittleFS.mkdir("/anim");
    }
#endif
    return true;
}

void AnimEngine::close() {
#ifdef ARDUINO
    if (anim_file) {
        anim_file.close();
    }
#else
    if (anim_file) {
        fclose(anim_file);
        anim_file = NULL;
    }
#endif
    is_open = false;
    is_bmp = false;
    is_playing = false;
    is_paused = false;
    current_frame_idx = 0;
    memset(current_filepath, 0, sizeof(current_filepath));
    memset(&current_header, 0, sizeof(current_header));
}

#ifdef ARDUINO
bool AnimEngine::parseBmpFile(File& f) {
    uint8_t file_hdr[14];
    if (f.read(file_hdr, sizeof(file_hdr)) != sizeof(file_hdr)) return false;
    if (file_hdr[0] != 'B' || file_hdr[1] != 'M') return false;

    bmp_pixel_offset = *(uint32_t*)&file_hdr[10];

    uint8_t info_hdr[40];
    if (f.read(info_hdr, sizeof(info_hdr)) != sizeof(info_hdr)) return false;

    int32_t w = *(int32_t*)&info_hdr[4];
    int32_t h = *(int32_t*)&info_hdr[8];
    uint16_t bpp = *(uint16_t*)&info_hdr[14];

    if (w != ANIM_WIDTH || (h != ANIM_HEIGHT && h != -ANIM_HEIGHT) || bpp != 1) {
        return false;
    }

    bmp_flip_y = (h > 0); // positive height means rows stored bottom-up

    current_header.magic = ANIM_MAGIC;
    current_header.version = ANIM_VERSION;
    current_header.width = ANIM_WIDTH;
    current_header.height = ANIM_HEIGHT;
    current_header.frame_count = 1;
    current_header.frame_delay_ms = 1000;
    current_header.flags = 0;
    current_header.reserved = 0;

    is_bmp = true;
    return readFrameData(0);
}
#else
bool AnimEngine::parseBmpFile(FILE* f) {
    uint8_t file_hdr[14];
    if (fread(file_hdr, 1, sizeof(file_hdr), f) != sizeof(file_hdr)) return false;
    if (file_hdr[0] != 'B' || file_hdr[1] != 'M') return false;

    bmp_pixel_offset = *(uint32_t*)&file_hdr[10];

    uint8_t info_hdr[40];
    if (fread(info_hdr, 1, sizeof(info_hdr), f) != sizeof(info_hdr)) return false;

    int32_t w = *(int32_t*)&info_hdr[4];
    int32_t h = *(int32_t*)&info_hdr[8];
    uint16_t bpp = *(uint16_t*)&info_hdr[14];

    if (w != ANIM_WIDTH || (h != ANIM_HEIGHT && h != -ANIM_HEIGHT) || bpp != 1) {
        return false;
    }

    bmp_flip_y = (h > 0);

    current_header.magic = ANIM_MAGIC;
    current_header.version = ANIM_VERSION;
    current_header.width = ANIM_WIDTH;
    current_header.height = ANIM_HEIGHT;
    current_header.frame_count = 1;
    current_header.frame_delay_ms = 1000;
    current_header.flags = 0;
    current_header.reserved = 0;

    is_bmp = true;
    return readFrameData(0);
}
#endif

bool AnimEngine::open(const char* filepath) {
    if (!filepath || strlen(filepath) == 0) return false;
    close();

#ifdef ARDUINO
    if (!LittleFS.exists(filepath)) return false;
    anim_file = LittleFS.open(filepath, "r");
    if (!anim_file) return false;
#else
    anim_file = fopen(filepath, "rb");
    if (!anim_file) return false;
#endif

    strncpy(current_filepath, filepath, sizeof(current_filepath) - 1);
    current_filepath[sizeof(current_filepath) - 1] = '\0';

    size_t f_len = strlen(filepath);
    bool check_bmp = false;
    if (f_len >= 4) {
        const char* ext = filepath + f_len - 4;
        if (strcasecmp(ext, ".bmp") == 0) {
            check_bmp = true;
        }
    }

    if (check_bmp) {
        if (!parseBmpFile(anim_file)) {
            close();
            return false;
        }
    } else {
#ifdef ARDUINO
        if (anim_file.read((uint8_t*)&current_header, sizeof(AnimHeader)) != sizeof(AnimHeader)) {
            close();
            return false;
        }
#else
        if (fread(&current_header, 1, sizeof(AnimHeader), anim_file) != sizeof(AnimHeader)) {
            close();
            return false;
        }
#endif

        if (current_header.magic != ANIM_MAGIC ||
            current_header.version != ANIM_VERSION ||
            current_header.width != ANIM_WIDTH ||
            current_header.height != ANIM_HEIGHT ||
            current_header.frame_count == 0) {
            close();
            return false;
        }

        is_bmp = false;
        is_looping = (current_header.flags & ANIM_FLAG_LOOP) != 0;

        if (!readFrameData(0)) {
            close();
            return false;
        }
    }

    is_open = true;
    is_playing = true;
    is_paused = false;
    current_frame_idx = 0;
    speed_multiplier = 1.0f;
    last_frame_time = millis();

    return true;
}

bool AnimEngine::readFrameData(uint16_t frame_idx) {
    if (!anim_file) return false;

    if (is_bmp) {
        const size_t row_bytes = ANIM_WIDTH / 8; // 16 bytes
        uint8_t temp_row[row_bytes];

        for (int row = 0; row < ANIM_HEIGHT; row++) {
            int src_row = bmp_flip_y ? (ANIM_HEIGHT - 1 - row) : row;
            size_t offset = bmp_pixel_offset + (size_t)src_row * row_bytes;
#ifdef ARDUINO
            anim_file.seek(offset);
            if (anim_file.read(temp_row, row_bytes) != row_bytes) {
                return false;
            }
#else
            fseek(anim_file, offset, SEEK_SET);
            if (fread(temp_row, 1, row_bytes, anim_file) != row_bytes) {
                return false;
            }
#endif
            for (size_t col = 0; col < row_bytes; col++) {
                frame_buffer[row * row_bytes + col] = reverse_byte_bits(temp_row[col]);
            }
        }
        return true;
    }

    size_t offset = sizeof(AnimHeader) + (size_t)frame_idx * ANIM_FRAME_BYTES;
#ifdef ARDUINO
    if (!anim_file.seek(offset)) return false;
    size_t bytes_read = anim_file.read(frame_buffer, ANIM_FRAME_BYTES);
#else
    if (fseek(anim_file, offset, SEEK_SET) != 0) return false;
    size_t bytes_read = fread(frame_buffer, 1, ANIM_FRAME_BYTES, anim_file);
#endif
    return (bytes_read == ANIM_FRAME_BYTES);
}

void AnimEngine::play() {
    if (is_open) {
        is_playing = true;
        is_paused = false;
        last_frame_time = millis();
    }
}

void AnimEngine::pause() {
    is_paused = true;
}

void AnimEngine::resume() {
    if (is_open) {
        is_paused = false;
        last_frame_time = millis();
    }
}

void AnimEngine::stop() {
    is_playing = false;
    is_paused = false;
    seekFrame(0);
}

void AnimEngine::togglePlayPause() {
    if (!is_playing) {
        play();
    } else if (is_paused) {
        resume();
    } else {
        pause();
    }
}

void AnimEngine::setSpeedMultiplier(float mult) {
    if (mult >= 0.25f && mult <= 4.0f) {
        speed_multiplier = mult;
    }
}

void AnimEngine::cycleSpeed() {
    if (speed_multiplier < 0.9f) {
        speed_multiplier = 1.0f;
    } else if (speed_multiplier < 1.4f) {
        speed_multiplier = 1.5f;
    } else if (speed_multiplier < 1.9f) {
        speed_multiplier = 2.0f;
    } else {
        speed_multiplier = 0.5f;
    }
}

uint16_t AnimEngine::getEffectiveDelayMs() const {
    uint16_t base = current_header.frame_delay_ms;
    if (base < 10) base = 50;
    uint16_t eff = (uint16_t)((float)base / speed_multiplier);
    return (eff < 15) ? 15 : eff;
}

bool AnimEngine::seekFrame(uint16_t frame_idx) {
    if (!is_open || frame_idx >= current_header.frame_count) return false;
    if (readFrameData(frame_idx)) {
        current_frame_idx = frame_idx;
        return true;
    }
    return false;
}

void AnimEngine::stepForward() {
    if (!is_open || current_header.frame_count <= 1) return;
    uint16_t next = current_frame_idx + 1;
    if (next >= current_header.frame_count) next = 0;
    seekFrame(next);
}

void AnimEngine::stepBackward() {
    if (!is_open || current_header.frame_count <= 1) return;
    uint16_t prev = (current_frame_idx == 0) ? (current_header.frame_count - 1) : (current_frame_idx - 1);
    seekFrame(prev);
}

bool AnimEngine::update() {
    if (!is_open || !is_playing || is_paused || current_header.frame_count <= 1) {
        return false;
    }

    uint16_t delay_ms = getEffectiveDelayMs();
    if (millis() - last_frame_time < delay_ms) {
        return false;
    }

    last_frame_time = millis();
    uint16_t next_frame = current_frame_idx + 1;

    if (next_frame >= current_header.frame_count) {
        if (is_looping) {
            next_frame = 0;
        } else {
            is_playing = false;
            return false;
        }
    }

    if (seekFrame(next_frame)) {
        return true;
    }
    return false;
}

void AnimEngine::drawFrame(int16_t x, int16_t y) {
#ifdef ARDUINO
    oled.setDrawColor(1);
    oled.drawXBMP(x, y, ANIM_WIDTH, ANIM_HEIGHT, frame_buffer);

    if (current_header.flags & ANIM_FLAG_INVERT) {
        oled.setDrawColor(2); // XOR invert mode
        oled.drawBox(x, y, ANIM_WIDTH, ANIM_HEIGHT);
        oled.setDrawColor(1);
    }
#else
    (void)x; (void)y;
#endif
}

void AnimEngine::playDefaultBootAnimation() {
#ifdef ARDUINO
    const int total_frames = 24;
    const uint32_t frame_delay = 50; // ~1.2s total

    for (int t = 0; t < total_frames; t++) {
        // Instant button skip check
        if (digitalRead(BTN_UP) == LOW || digitalRead(BTN_OK) == LOW ||
            digitalRead(BTN_DN) == LOW || digitalRead(BTN_CANCEL) == LOW) {
            soundManager.stop();
            break;
        }

        oled.clearBuffer();
        oled.setDrawColor(1);

        // 1. Tactical Frame Corners
        oled.drawFrame(0, 0, 128, 64);
        oled.drawHLine(2, 2, 8);
        oled.drawVLine(2, 2, 8);
        oled.drawHLine(118, 2, 8);
        oled.drawVLine(125, 2, 8);
        oled.drawHLine(2, 61, 8);
        oled.drawVLine(2, 54, 8);
        oled.drawHLine(118, 61, 8);
        oled.drawVLine(125, 54, 8);

        // 2. Reticle and Radar Sweep
        int cx = 32;
        int cy = 32;
        int r = (t < 8) ? (t * 3) : 22;
        oled.drawCircle(cx, cy, r);
        oled.drawCircle(cx, cy, r / 2);
        oled.drawHLine(cx - r - 4, cy, 2 * r + 8);
        oled.drawVLine(cx, cy - r - 4, 2 * r + 8);

        // Rotating radar line using single-precision math
        if (t >= 4) {
            float angle = (float)t * 0.45f;
            int rx = cx + (int)(cosf(angle) * (float)r);
            int ry = cy + (int)(sinf(angle) * (float)r);
            oled.drawLine(cx, cy, rx, ry);

            // Tactical radar blips
            if (t > 10) oled.drawDisc(cx + 8, cy - 6, 1);
            if (t > 14) oled.drawDisc(cx - 10, cy + 9, 1);
        }

        // 3. Futuristic Title and Initialization Status
        oled.setFont(u8g2_font_6x10_tf);
        oled.drawStr(60, 20, "007 Q-WATCH");

        oled.setFont(u8g2_font_4x6_tr);
        oled.drawStr(60, 30, "MI6 TACTICAL OS");

        // Progress bar
        oled.drawFrame(60, 36, 62, 8);
        int prog = (t * 58) / total_frames;
        if (prog > 58) prog = 58;
        if (prog > 0) oled.drawBox(62, 38, prog, 4);

        if (t < 8) {
            oled.drawStr(60, 52, "SYSTEM INIT...");
        } else if (t < 16) {
            oled.drawStr(60, 52, "SENSORS ONLINE");
        } else {
            oled.drawStr(60, 52, "SYSTEM READY");
        }

        oled.sendBuffer();
        soundManager.loop();
        delay(frame_delay);
    }
#endif
}

bool AnimEngine::playBootAnimation() {
#ifdef ARDUINO
    // 1. Check custom boot animation file
    if (LittleFS.exists("/boot/boot.anim")) {
        if (open("/boot/boot.anim")) {
            setLoop(false);
            play();
            uint16_t delay_ms = getEffectiveDelayMs();
            if (delay_ms < 15) delay_ms = 50;

            for (uint16_t f = 0; f < current_header.frame_count; f++) {
                if (digitalRead(BTN_UP) == LOW || digitalRead(BTN_OK) == LOW ||
                    digitalRead(BTN_DN) == LOW || digitalRead(BTN_CANCEL) == LOW) {
                    soundManager.stop();
                    break;
                }

                if (seekFrame(f)) {
                    oled.clearBuffer();
                    drawFrame(0, 0);
                    oled.sendBuffer();
                }

                soundManager.loop();
                delay(delay_ms);
            }

            close();
            return true;
        }
    }

    // 2. Check static boot bitmap splash
    if (LittleFS.exists("/boot/boot.bmp")) {
        if (open("/boot/boot.bmp")) {
            oled.clearBuffer();
            drawFrame(0, 0);
            oled.sendBuffer();

            uint32_t start = millis();
            while (millis() - start < 1500) {
                if (digitalRead(BTN_UP) == LOW || digitalRead(BTN_OK) == LOW ||
                    digitalRead(BTN_DN) == LOW || digitalRead(BTN_CANCEL) == LOW) {
                    soundManager.stop();
                    break;
                }
                soundManager.loop();
                delay(20);
            }

            close();
            return true;
        }
    }

    // 3. Fallback to procedural tactical animation
    playDefaultBootAnimation();
    return true;
#else
    return true;
#endif
}

bool AnimEngine::setAsBootAnimation(const char* source_path) {
    if (!source_path || strlen(source_path) == 0) return false;

#ifdef ARDUINO
    if (!LittleFS.exists(source_path)) return false;

    if (!LittleFS.exists("/boot")) {
        LittleFS.mkdir("/boot");
    }

    String src = String(source_path);
    const char* dest_path = "/boot/boot.anim";
    if (src.endsWith(".bmp") || src.endsWith(".BMP")) {
        dest_path = "/boot/boot.bmp";
    }

    File srcFile = LittleFS.open(source_path, "r");
    if (!srcFile) return false;

    File destFile = LittleFS.open(dest_path, "w");
    if (!destFile) {
        srcFile.close();
        return false;
    }

    uint8_t buffer[512];
    while (srcFile.available()) {
        size_t bytes = srcFile.read(buffer, sizeof(buffer));
        if (bytes > 0) {
            destFile.write(buffer, bytes);
        }
    }

    srcFile.close();
    destFile.close();
    return true;
#else
    FILE* srcFile = fopen(source_path, "rb");
    if (!srcFile) return false;

    const char* dest_path = "boot.anim";
    size_t slen = strlen(source_path);
    if (slen >= 4 && strcasecmp(source_path + slen - 4, ".bmp") == 0) {
        dest_path = "boot.bmp";
    }

    FILE* destFile = fopen(dest_path, "wb");
    if (!destFile) {
        fclose(srcFile);
        return false;
    }

    uint8_t buffer[512];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), srcFile)) > 0) {
        fwrite(buffer, 1, bytes, destFile);
    }

    fclose(srcFile);
    fclose(destFile);
    return true;
#endif
}
