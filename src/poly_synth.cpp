#include "poly_synth.h"
#include "sound_manager.h"
#include <Arduino.h>
#include <string.h>

PolySynth polySynth;

// 16-step polyphonic song patterns (frequencies in Hz, 0 = rest/silent)
struct SongRow {
    uint16_t ch0_lead;
    uint16_t ch1_harm;
    uint16_t ch2_bass;
    uint8_t  ch3_perc; // 0 = off, 1 = hat, 2 = snare, 3 = kick
};

static const SongRow s_song_fanfare[16] = {
    { 523, 330, 131, 3 }, // C5, E4, C3, Kick
    { 523, 330, 131, 1 }, // C5, E4, C3, Hat
    { 659, 392, 165, 2 }, // E5, G4, E3, Snare
    { 659, 392, 165, 1 }, // E5, G4, E3, Hat
    { 784, 523, 196, 3 }, // G5, C5, G3, Kick
    { 784, 523, 196, 1 }, // G5, C5, G3, Hat
    { 1046, 659, 262, 2 }, // C6, E5, C4, Snare
    { 1046, 659, 262, 1 }, // C6, E5, C4, Hat
    { 880, 587, 220, 3 }, // A5, D5, A3, Kick
    { 880, 587, 220, 1 }, // A5, D5, A3, Hat
    { 988, 659, 247, 2 }, // B5, E5, B3, Snare
    { 988, 659, 247, 1 }, // B5, E5, B3, Hat
    { 1046, 784, 262, 3 }, // C6, G5, C4, Kick
    { 1046, 784, 262, 1 }, // C6, G5, C4, Hat
    { 1046, 784, 262, 2 }, // C6, G5, C4, Snare
    {    0,   0,   0, 0 }  // Rest
};

static const SongRow s_song_cyber[16] = {
    { 440, 220, 110, 3 }, // A4
    { 554, 277, 110, 1 }, // C#5
    { 659, 330, 110, 2 }, // E5
    { 880, 440, 110, 1 }, // A5
    { 659, 330, 110, 3 }, // E5
    { 554, 277, 110, 1 }, // C#5
    { 440, 220, 110, 2 }, // A4
    { 554, 277, 110, 1 }, // C#5
    { 494, 247, 123, 3 }, // B4
    { 587, 294, 123, 1 }, // D5
    { 740, 370, 123, 2 }, // F#5
    { 988, 494, 123, 1 }, // B5
    { 740, 370, 123, 3 }, // F#5
    { 587, 294, 123, 1 }, // D5
    { 494, 247, 123, 2 }, // B4
    { 587, 294, 123, 1 }  // D5
};

static const SongRow s_song_march[16] = {
    { 392, 196,  98, 3 }, // G4
    {   0,   0,   0, 1 },
    { 392, 196,  98, 2 }, // G4
    { 392, 196,  98, 1 }, // G4
    { 311, 156,  78, 3 }, // Eb4
    { 466, 233, 117, 1 }, // Bb4
    { 392, 196,  98, 2 }, // G4
    { 311, 156,  78, 1 }, // Eb4
    { 466, 233, 117, 3 }, // Bb4
    { 392, 196,  98, 2 }, // G4
    {   0,   0,   0, 1 },
    { 587, 294, 147, 3 }, // D5
    { 587, 294, 147, 1 }, // D5
    { 587, 294, 147, 2 }, // D5
    { 622, 311, 156, 3 }, // Eb5
    { 466, 233, 117, 2 }  // Bb4
};

static const SongRow s_song_mission[16] = {
    { 659, 330, 165, 3 }, // E5
    {   0,   0,   0, 1 },
    { 659, 330, 165, 2 }, // E5
    {   0,   0,   0, 1 },
    { 784, 392, 196, 3 }, // G5
    {   0,   0,   0, 1 },
    { 880, 440, 220, 2 }, // A5
    {   0,   0,   0, 1 },
    { 659, 330, 165, 3 }, // E5
    {   0,   0,   0, 1 },
    { 587, 294, 147, 2 }, // D5
    {   0,   0,   0, 1 },
    { 622, 311, 156, 3 }, // Eb5
    {   0,   0,   0, 1 },
    { 659, 330, 165, 2 }, // E5
    {   0,   0,   0, 1 }
};

PolySynth::PolySynth()
    : song_playing(false),
      current_song_id(0),
      song_step(0),
      last_step_ms(0),
      step_duration_ms(125), // 120 BPM 16th notes
      current_buzzer_voice(0),
      last_buzzer_toggle_ms(0) {

    for (size_t ch = 0; ch < NUM_CHANNELS; ch++) {
        voices[ch].freq_hz = 0;
        voices[ch].volume = 15;
        voices[ch].active = false;
        voices[ch].muted = false;
        voices[ch].phase = 0;
        voices[ch].phase_inc = 0;
        voices[ch].lfsr = 0xACE1u + (uint16_t)ch;
    }

    voices[0].wave_type = WaveType::SQUARE_50;
    voices[1].wave_type = WaveType::PULSE_25;
    voices[2].wave_type = WaveType::TRIANGLE;
    voices[3].wave_type = WaveType::NOISE;

    memset(ch_buffers, 0, sizeof(ch_buffers));
    memset(last_mix_buf, 0, sizeof(last_mix_buf));
}

void PolySynth::begin() {
    stopAll();
}

void PolySynth::noteOn(uint8_t ch, uint16_t freq_hz, uint8_t volume, WaveType wave) {
    if (ch >= NUM_CHANNELS) return;

    voices[ch].freq_hz = freq_hz;
    voices[ch].volume = volume > 15 ? 15 : volume;
    voices[ch].wave_type = wave;
    voices[ch].active = (freq_hz > 0 && volume > 0);
    voices[ch].phase = 0;
    voices[ch].phase_inc = (uint32_t)(((uint64_t)freq_hz << 32) / SAMPLE_RATE);
}

void PolySynth::noteOff(uint8_t ch) {
    if (ch >= NUM_CHANNELS) return;
    voices[ch].active = false;
    voices[ch].freq_hz = 0;
    voices[ch].phase_inc = 0;
}

void PolySynth::stopAll() {
    for (size_t ch = 0; ch < NUM_CHANNELS; ch++) {
        noteOff(ch);
    }
    soundManager.stop();
}

void PolySynth::setWaveType(uint8_t ch, WaveType wave) {
    if (ch < NUM_CHANNELS) {
        voices[ch].wave_type = wave;
    }
}

WaveType PolySynth::getWaveType(uint8_t ch) const {
    if (ch < NUM_CHANNELS) return voices[ch].wave_type;
    return WaveType::SQUARE_50;
}

const char* PolySynth::getWaveName(WaveType wave) const {
    switch (wave) {
        case WaveType::SQUARE_50: return "SQR 50%";
        case WaveType::PULSE_25:  return "PULSE 25%";
        case WaveType::PULSE_12:  return "PULSE 12%";
        case WaveType::TRIANGLE:  return "TRIANGLE";
        case WaveType::SAWTOOTH:  return "SAWTOOTH";
        case WaveType::NOISE:     return "NOISE";
        default:                  return "SQUARE";
    }
}

void PolySynth::setMute(uint8_t ch, bool mute) {
    if (ch < NUM_CHANNELS) {
        voices[ch].muted = mute;
    }
}

void PolySynth::toggleMute(uint8_t ch) {
    if (ch < NUM_CHANNELS) {
        setMute(ch, !voices[ch].muted);
    }
}

bool PolySynth::isMuted(uint8_t ch) const {
    if (ch < NUM_CHANNELS) return voices[ch].muted;
    return false;
}

uint16_t PolySynth::getChannelFreq(uint8_t ch) const {
    if (ch < NUM_CHANNELS && voices[ch].active && !voices[ch].muted) {
        return voices[ch].freq_hz;
    }
    return 0;
}

uint8_t PolySynth::getChannelVolume(uint8_t ch) const {
    if (ch < NUM_CHANNELS && voices[ch].active && !voices[ch].muted) {
        return voices[ch].volume;
    }
    return 0;
}

bool PolySynth::isChannelActive(uint8_t ch) const {
    if (ch < NUM_CHANNELS) {
        return voices[ch].active && !voices[ch].muted;
    }
    return false;
}

const char* PolySynth::getSongName(uint8_t song_id) const {
    switch (song_id % SONG_COUNT) {
        case 0: return "RETRO FANFARE";
        case 1: return "CYBER ARPEGGIO";
        case 2: return "SPACE MARCH";
        case 3: return "MISSION THEME";
        default: return "UNKNOWN";
    }
}

uint16_t PolySynth::getSongTotalSteps() const {
    return 16;
}

void PolySynth::playSong(uint8_t song_id) {
    current_song_id = song_id % SONG_COUNT;
    song_step = 0;
    song_playing = true;
    last_step_ms = millis();
    updateSongSequence();
}

void PolySynth::stopSong() {
    song_playing = false;
    stopAll();
}

void PolySynth::toggleSong(uint8_t song_id) {
    if (song_playing && current_song_id == (song_id % SONG_COUNT)) {
        stopSong();
    } else {
        playSong(song_id);
    }
}

void PolySynth::nextSong() {
    uint8_t next = (current_song_id + 1) % SONG_COUNT;
    playSong(next);
}

void PolySynth::prevSong() {
    uint8_t prev = (current_song_id + SONG_COUNT - 1) % SONG_COUNT;
    playSong(prev);
}

void PolySynth::updateSongSequence() {
    const SongRow* pattern = nullptr;
    switch (current_song_id) {
        case 0: pattern = s_song_fanfare; break;
        case 1: pattern = s_song_cyber; break;
        case 2: pattern = s_song_march; break;
        case 3: pattern = s_song_mission; break;
        default: pattern = s_song_fanfare; break;
    }

    const SongRow& row = pattern[song_step];

    // CH0 Lead (Square 50%)
    if (row.ch0_lead > 0) noteOn(0, row.ch0_lead, 15, WaveType::SQUARE_50);
    else noteOff(0);

    // CH1 Harmony (Pulse 25%)
    if (row.ch1_harm > 0) noteOn(1, row.ch1_harm, 12, WaveType::PULSE_25);
    else noteOff(1);

    // CH2 Bass (Triangle)
    if (row.ch2_bass > 0) noteOn(2, row.ch2_bass, 14, WaveType::TRIANGLE);
    else noteOff(2);

    // CH3 Percussion (Noise or Kick pulse)
    if (row.ch3_perc == 1) { // Hi-hat (short noise)
        noteOn(3, 4000, 8, WaveType::NOISE);
    } else if (row.ch3_perc == 2) { // Snare (medium noise)
        noteOn(3, 2500, 14, WaveType::NOISE);
    } else if (row.ch3_perc == 3) { // Kick (low pitch pulse)
        noteOn(3, 120, 15, WaveType::PULSE_12);
    } else {
        noteOff(3);
    }
}

void PolySynth::generateChannelSamples(uint8_t ch, uint8_t* buf, size_t len) {
    if (!buf || len == 0 || ch >= NUM_CHANNELS) return;
    PolyVoice& v = voices[ch];

    if (!v.active || v.muted || v.volume == 0) {
        memset(buf, 0, len);
        return;
    }

    uint8_t vol_scale = v.volume; // 0..15

    for (size_t i = 0; i < len; i++) {
        uint8_t sample = 0;

        switch (v.wave_type) {
            case WaveType::SQUARE_50:
                sample = (v.phase < 0x80000000u) ? 255 : 0;
                break;
            case WaveType::PULSE_25:
                sample = (v.phase < 0x40000000u) ? 255 : 0;
                break;
            case WaveType::PULSE_12:
                sample = (v.phase < 0x20000000u) ? 255 : 0;
                break;
            case WaveType::TRIANGLE: {
                uint32_t top = v.phase >> 24; // 0..255
                if (top < 128) sample = (uint8_t)(top * 2);
                else sample = (uint8_t)((255 - top) * 2);
                break;
            }
            case WaveType::SAWTOOTH:
                sample = (uint8_t)(v.phase >> 24);
                break;
            case WaveType::NOISE:
                v.lfsr = (v.lfsr >> 1) ^ (-(v.lfsr & 1) & 0xB400u);
                sample = (uint8_t)(v.lfsr & 0xFF);
                break;
            default:
                sample = 0;
                break;
        }

        v.phase += v.phase_inc;

        // Scale by channel volume (0..15 mapped to 0..255 peak amplitude)
        buf[i] = (uint8_t)(((uint16_t)sample * vol_scale) / 15);
    }
}

void PolySynth::renderBuffer(uint8_t* out_mix, size_t len) {
    if (!out_mix || len == 0) return;

    size_t chunk = len > BUFFER_SIZE ? BUFFER_SIZE : len;

    // Synthesize independent voices
    for (size_t ch = 0; ch < NUM_CHANNELS; ch++) {
        generateChannelSamples(ch, ch_buffers[ch], chunk);
    }

    // Mix across all 4 channels using SIMD saturated vector engine
    simd_mix_audio_8bit(out_mix, ch_buffers[0], ch_buffers[1], ch_buffers[2], ch_buffers[3], chunk);

    // Preserve latest mixed samples for OLED visualizer
    memcpy(last_mix_buf, out_mix, chunk);
}

void PolySynth::driveBuzzerHardware() {
    uint32_t now = millis();

    // Fast frequency interleaving multiplexer for hardware piezo buzzer (~12ms per voice)
    if (now - last_buzzer_toggle_ms >= 12) {
        last_buzzer_toggle_ms = now;

        // Find active voices
        uint8_t active_indices[NUM_CHANNELS];
        uint8_t active_count = 0;
        for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
            if (voices[i].active && !voices[i].muted && voices[i].freq_hz > 0) {
                active_indices[active_count++] = i;
            }
        }

        if (active_count == 0) {
            soundManager.stop();
            return;
        }

        current_buzzer_voice = (current_buzzer_voice + 1) % active_count;
        uint8_t ch = active_indices[current_buzzer_voice];
        uint16_t freq = voices[ch].freq_hz;
        uint8_t vol = voices[ch].volume; // 0..15

        // Map volume to duty cycle (15% to 50%)
        uint8_t duty = (uint8_t)((vol * 50) / 15);
        if (duty < 5) duty = 5;

        soundManager.setToneDuty(freq, duty);
    }
}

void PolySynth::loop() {
    if (!soundManager.isMasterSwitchOn()) {
        if (song_playing) stopSong();
        return;
    }

    uint32_t now = millis();

    // Step tracker sequence
    if (song_playing) {
        if (now - last_step_ms >= step_duration_ms) {
            last_step_ms = now;
            song_step = (song_step + 1) % 16;
            updateSongSequence();
        }
    }

    // Drive buzzer output
    driveBuzzerHardware();

    // Periodically update audio buffer for visualizer
    renderBuffer(last_mix_buf, BUFFER_SIZE);
}
