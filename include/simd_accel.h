#ifndef SIMD_ACCEL_H
#define SIMD_ACCEL_H

#include <stdint.h>
#include <stddef.h>

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

#ifdef __cplusplus
}
#endif

#endif // SIMD_ACCEL_H
