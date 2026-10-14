#ifndef SIMD_ACCEL_H
#define SIMD_ACCEL_H

#include <stdint.h>
#include <stddef.h>

#if !defined(ALIGNED16)
  #if defined(__GNUC__) || defined(__clang__)
    #define ALIGNED16 __attribute__((aligned(16)))
  #else
    #define ALIGNED16
  #endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Candidate 1: OLED 128-bit SIMD Vector Engine
// Inverts len bytes of buffer (128-bit / 16-byte vector chunks with scalar tail fallback)
void simd_invert_128(uint8_t* dst, const uint8_t* src, size_t len);

// Bitwise XOR mask (useful for transparent HUD overlays, reticles, cursor masks)
void simd_xor_mask_128(uint8_t* dst, const uint8_t* src, const uint8_t* mask, size_t len);

// Bitwise AND mask
void simd_and_mask_128(uint8_t* dst, const uint8_t* src, const uint8_t* mask, size_t len);

// Bitwise OR mask
void simd_or_mask_128(uint8_t* dst, const uint8_t* src, const uint8_t* mask, size_t len);

// Candidate 3: MAX30102 32-Tap Digital Bandpass FIR Filter (0.5Hz - 4.0Hz, 30 - 240 BPM)
// Processes a new PPG AC sample through 32-tap Q15 FIR filter
int16_t simd_max30102_fir_sample(int16_t sample);

// Reset filter state (clear history delay line)
void simd_max30102_fir_reset(void);

// Generic 32-tap Q15 FIR dot product (32 taps, 16-byte aligned history and coefficients)
int16_t simd_fir_filter_32(const int16_t* history_32, const int16_t* coeffs_32);

// Batch 32-tap FIR filter
void simd_fir_filter_batch(const int16_t* input, const int16_t* coeffs, int16_t* output, size_t num_samples);

// Candidate 2: Vector Screen Transitions (128-bit SIMD Animation Engine)
typedef enum {
    SIMD_TRANSITION_NONE = 0,
    SIMD_TRANSITION_DISSOLVE = 1,   // Bayer 4x4 matrix pseudo-random pixel melt
    SIMD_TRANSITION_WIPE_RIGHT = 2, // Left-to-right curtain wipe
    SIMD_TRANSITION_WIPE_DOWN = 3,  // Top-to-bottom page wipe
    SIMD_TRANSITION_CROSSFADE = 4   // 128-bit vector blend
} simd_transition_t;

// 128-bit vector blend between two 1024-byte framebuffers using a mask:
// dst[i] = (old_buf[i] & ~mask[i]) | (new_buf[i] & mask[i])
void simd_blend_frames_128(uint8_t* dst, const uint8_t* old_buf, const uint8_t* new_buf, const uint8_t* mask, size_t len);

// Generates a 128-bit aligned dither mask for step (0..16) using 4x4 Bayer threshold matrix
void simd_generate_dither_mask(uint8_t* mask, uint8_t step, size_t len);

// Generates a wipe mask for progress (0..100) (direction: 0=horizontal, 1=vertical)
void simd_generate_wipe_mask(uint8_t* mask, uint8_t progress_pct, uint8_t direction, size_t len);

// Candidate 4: 6-DOF IMU 128-bit Parallel Vector Low-Pass Filter
// Filters packed [ax, ay, az, 0, gx, gy, gz, 0] across 8 parallel lanes using Q15 alpha
void simd_filter_imu_6dof(int16_t* filtered_y, const int16_t* raw_x, uint16_t alpha_q15);

// Candidate 5: TinyML INT8 Dot-Product & ReLU Neural Network Layer
// Evaluates dense layer with num_inputs (16-byte aligned) and num_neurons:
// output[n] = relu((bias[n] + sum(inputs[i] * weights[n * num_inputs + i])) >> 4)
void simd_nn_dense_layer_int8(const int8_t* inputs, const int8_t* weights, const int32_t* bias, int8_t* outputs, size_t num_inputs, size_t num_neurons);

// Candidate 6: 4-Channel 8-Bit Polyphonic Audio Saturated Mixing
// Mixes 4 channels of 8-bit unsigned PCM audio samples into dst with saturation clamp:
// dst[i] = min(255, ch1[i] + ch2[i] + ch3[i] + ch4[i])
void simd_mix_audio_8bit(uint8_t* dst, const uint8_t* ch1, const uint8_t* ch2, const uint8_t* ch3, const uint8_t* ch4, size_t len);

#ifdef __cplusplus
}
#endif

#endif // SIMD_ACCEL_H
