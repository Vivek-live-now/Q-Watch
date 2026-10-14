#ifndef POLY_SYNTH_H
#define POLY_SYNTH_H

#include <stdint.h>
#include <stddef.h>
#include "simd_accel.h"

enum class WaveType : uint8_t {
    SQUARE_50 = 0,
    PULSE_25  = 1,
    PULSE_12  = 2,
    TRIANGLE  = 3,
    SAWTOOTH  = 4,
    NOISE     = 5,
    WAVE_COUNT = 6
};

struct PolyVoice {
    uint16_t freq_hz;
    uint8_t volume;     // 0 - 15
    WaveType wave_type;
    bool active;
    bool muted;
    uint32_t phase;
    uint32_t phase_inc;
    uint16_t lfsr;
};

class PolySynth {
public:
    static const size_t NUM_CHANNELS = 4;
    static const size_t BUFFER_SIZE = 64;
    static const uint32_t SAMPLE_RATE = 8000;

    PolySynth();

    void begin();
    void loop();

    // Voice & Channel Controls
    void noteOn(uint8_t ch, uint16_t freq_hz, uint8_t volume = 15, WaveType wave = WaveType::SQUARE_50);
    void noteOff(uint8_t ch);
    void stopAll();

    void setWaveType(uint8_t ch, WaveType wave);
    WaveType getWaveType(uint8_t ch) const;
    const char* getWaveName(WaveType wave) const;

    void setMute(uint8_t ch, bool mute);
    void toggleMute(uint8_t ch);
    bool isMuted(uint8_t ch) const;

    uint16_t getChannelFreq(uint8_t ch) const;
    uint8_t getChannelVolume(uint8_t ch) const;
    bool isChannelActive(uint8_t ch) const;

    // Buffer Synthesis (SIMD Accelerated)
    void renderBuffer(uint8_t* out_mix, size_t len);
    const uint8_t* getLastRenderedBuffer() const { return last_mix_buf; }

    // Chiptune Song Player
    static const uint8_t SONG_COUNT = 4;
    void playSong(uint8_t song_id);
    void stopSong();
    void toggleSong(uint8_t song_id);
    void nextSong();
    void prevSong();
    bool isPlaying() const { return song_playing; }
    uint8_t getActiveSong() const { return current_song_id; }
    const char* getSongName(uint8_t song_id) const;
    uint16_t getSongStep() const { return song_step; }
    uint16_t getSongTotalSteps() const;

private:
    void generateChannelSamples(uint8_t ch, uint8_t* buf, size_t len);
    void updateSongSequence();
    void driveBuzzerHardware();

    PolyVoice voices[NUM_CHANNELS];
    ALIGNED16 uint8_t ch_buffers[NUM_CHANNELS][BUFFER_SIZE];
    ALIGNED16 uint8_t last_mix_buf[BUFFER_SIZE];

    // Chiptune Player state
    bool song_playing;
    uint8_t current_song_id;
    uint16_t song_step;
    uint32_t last_step_ms;
    uint32_t step_duration_ms;

    // Hardware buzzer multiplexer state
    uint8_t current_buzzer_voice;
    uint32_t last_buzzer_toggle_ms;
};

extern PolySynth polySynth;

#endif // POLY_SYNTH_H
