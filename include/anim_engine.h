#ifndef ANIM_ENGINE_H
#define ANIM_ENGINE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <LittleFS.h>
#include "hw_config.h"
#else
#include <stdio.h>
#include <string.h>
#endif

#define ANIM_MAGIC          0x4D4E4151  // "QANM" in little-endian ('Q','A','N','M')
#define ANIM_VERSION        1
#define ANIM_WIDTH          128
#define ANIM_HEIGHT         64
#define ANIM_FRAME_BYTES    1024        // (128 * 64) / 8

#define ANIM_FLAG_LOOP      0x0001
#define ANIM_FLAG_INVERT    0x0002

#pragma pack(push, 1)
struct AnimHeader {
    uint32_t magic;           // 0x4D4E4151 ("QANM")
    uint16_t version;         // 1
    uint8_t  width;           // 128
    uint8_t  height;          // 64
    uint16_t frame_count;     // Total frames in animation
    uint16_t frame_delay_ms;  // Delay per frame in ms (e.g. 50ms = 20 FPS)
    uint16_t flags;           // ANIM_FLAG_LOOP, ANIM_FLAG_INVERT
    uint16_t reserved;        // 0
};
#pragma pack(pop)

class AnimEngine {
public:
    AnimEngine();
    ~AnimEngine();

    bool begin();

    // File playback controls
    bool open(const char* filepath);
    void close();

    void play();
    void pause();
    void resume();
    void stop();
    void togglePlayPause();

    void setLoop(bool loop) { is_looping = loop; }
    bool isLooping() const { return is_looping; }
    bool isPlaying() const { return is_playing && !is_paused; }
    bool isPaused() const { return is_paused; }
    bool isOpen() const { return is_open; }

    void setSpeedMultiplier(float mult);
    float getSpeedMultiplier() const { return speed_multiplier; }
    void cycleSpeed();

    void stepForward();
    void stepBackward();
    bool seekFrame(uint16_t frame_idx);

    // Frame update and rendering
    bool update();
    void drawFrame(int16_t x = 0, int16_t y = 0);

    // Boot animation
    bool playBootAnimation();
    void playDefaultBootAnimation();

    // Boot configuration
    bool setAsBootAnimation(const char* source_path);

    // Metadata accessors
    uint16_t getCurrentFrame() const { return current_frame_idx; }
    uint16_t getTotalFrames() const { return current_header.frame_count; }
    uint16_t getEffectiveDelayMs() const;
    const char* getFilePath() const { return current_filepath; }
    const AnimHeader& getHeader() const { return current_header; }
    const uint8_t* getFrameBuffer() const { return frame_buffer; }
    bool isBmpFormat() const { return is_bmp; }

private:
    bool readFrameData(uint16_t frame_idx);
#ifdef ARDUINO
    bool parseBmpFile(File& f);
    File anim_file;
#else
    bool parseBmpFile(FILE* f);
    FILE* anim_file;
#endif
    bool is_open;
    bool is_bmp;
    bool is_playing;
    bool is_paused;
    bool is_looping;
    float speed_multiplier;

    AnimHeader current_header;
    char current_filepath[64];
    uint16_t current_frame_idx;
    uint32_t last_frame_time;
    uint32_t bmp_pixel_offset;
    bool bmp_flip_y;

    uint8_t frame_buffer[ANIM_FRAME_BYTES];
};

extern AnimEngine animEngine;

#endif // ANIM_ENGINE_H
