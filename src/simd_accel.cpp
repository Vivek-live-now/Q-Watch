#include "simd_accel.h"
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#define ALIGNED16 __attribute__((aligned(16)))
#else
#define ALIGNED16
#endif

// 32-tap FIR filter coefficients (Q15 fixed point)
// Passband: 0.5Hz - 4.0Hz at 50Hz sample rate (Heart Rate 30 - 240 BPM)
static const ALIGNED16 int16_t s_fir_coeffs[32] = {
    -39,   -82,  -134,  -180,  -190,  -128,    39,   308,
    672,  1104,  1566,  2005,  2366,  2598,  2680,  2598,
   2366,  2005,  1566,  1104,   672,   308,    39,  -128,
   -190,  -180,  -134,   -82,   -39,     3,    26,    33
};

// 16-byte aligned static delay line for MAX30102 PPG samples
static ALIGNED16 int16_t s_fir_history[32] = {0};

void simd_invert_128(uint8_t* dst, const uint8_t* src, size_t len) {
    if (!dst || !src || len == 0) return;

    // Fast alignment and size verification
    if ((((uintptr_t)dst | (uintptr_t)src) & 0xF) != 0 || len < 16) {
        for (size_t i = 0; i < len; i++) {
            dst[i] = ~src[i];
        }
        return;
    }

#if defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(NO_SIMD)
    size_t chunks = len / 16;
    size_t remainder = len % 16;
    uint8_t* d = dst;
    const uint8_t* s = src;
    asm volatile (
        "1:\n"
        "ee.vld.128.ip  q0, %1, 16\n"
        "ee.notq        q0, q0\n"
        "ee.vst.128.ip  q0, %0, 16\n"
        "addi           %2, %2, -1\n"
        "bnez           %2, 1b\n"
        : "+r"(d), "+r"(s), "+r"(chunks)
        :
        : "memory"
    );
    for (size_t i = 0; i < remainder; i++) {
        d[i] = ~s[i];
    }
#else
    for (size_t i = 0; i < len; i++) {
        dst[i] = ~src[i];
    }
#endif
}

void simd_xor_mask_128(uint8_t* dst, const uint8_t* src, const uint8_t* mask, size_t len) {
    if (!dst || !src || !mask || len == 0) return;

    if ((((uintptr_t)dst | (uintptr_t)src | (uintptr_t)mask) & 0xF) != 0 || len < 16) {
        for (size_t i = 0; i < len; i++) {
            dst[i] = src[i] ^ mask[i];
        }
        return;
    }

#if defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(NO_SIMD)
    size_t chunks = len / 16;
    size_t remainder = len % 16;
    uint8_t* d = dst;
    const uint8_t* s = src;
    const uint8_t* m = mask;
    asm volatile (
        "1:\n"
        "ee.vld.128.ip  q0, %1, 16\n"
        "ee.vld.128.ip  q1, %2, 16\n"
        "ee.xorq        q0, q0, q1\n"
        "ee.vst.128.ip  q0, %0, 16\n"
        "addi           %3, %3, -1\n"
        "bnez           %3, 1b\n"
        : "+r"(d), "+r"(s), "+r"(m), "+r"(chunks)
        :
        : "memory"
    );
    for (size_t i = 0; i < remainder; i++) {
        d[i] = s[i] ^ m[i];
    }
#else
    for (size_t i = 0; i < len; i++) {
        dst[i] = src[i] ^ mask[i];
    }
#endif
}

void simd_and_mask_128(uint8_t* dst, const uint8_t* src, const uint8_t* mask, size_t len) {
    if (!dst || !src || !mask || len == 0) return;

    if ((((uintptr_t)dst | (uintptr_t)src | (uintptr_t)mask) & 0xF) != 0 || len < 16) {
        for (size_t i = 0; i < len; i++) {
            dst[i] = src[i] & mask[i];
        }
        return;
    }

#if defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(NO_SIMD)
    size_t chunks = len / 16;
    size_t remainder = len % 16;
    uint8_t* d = dst;
    const uint8_t* s = src;
    const uint8_t* m = mask;
    asm volatile (
        "1:\n"
        "ee.vld.128.ip  q0, %1, 16\n"
        "ee.vld.128.ip  q1, %2, 16\n"
        "ee.andq        q0, q0, q1\n"
        "ee.vst.128.ip  q0, %0, 16\n"
        "addi           %3, %3, -1\n"
        "bnez           %3, 1b\n"
        : "+r"(d), "+r"(s), "+r"(m), "+r"(chunks)
        :
        : "memory"
    );
    for (size_t i = 0; i < remainder; i++) {
        d[i] = s[i] & m[i];
    }
#else
    for (size_t i = 0; i < len; i++) {
        dst[i] = src[i] & mask[i];
    }
#endif
}

void simd_or_mask_128(uint8_t* dst, const uint8_t* src, const uint8_t* mask, size_t len) {
    if (!dst || !src || !mask || len == 0) return;

    if ((((uintptr_t)dst | (uintptr_t)src | (uintptr_t)mask) & 0xF) != 0 || len < 16) {
        for (size_t i = 0; i < len; i++) {
            dst[i] = src[i] | mask[i];
        }
        return;
    }

#if defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(NO_SIMD)
    size_t chunks = len / 16;
    size_t remainder = len % 16;
    uint8_t* d = dst;
    const uint8_t* s = src;
    const uint8_t* m = mask;
    asm volatile (
        "1:\n"
        "ee.vld.128.ip  q0, %1, 16\n"
        "ee.vld.128.ip  q1, %2, 16\n"
        "ee.orq         q0, q0, q1\n"
        "ee.vst.128.ip  q0, %0, 16\n"
        "addi           %3, %3, -1\n"
        "bnez           %3, 1b\n"
        : "+r"(d), "+r"(s), "+r"(m), "+r"(chunks)
        :
        : "memory"
    );
    for (size_t i = 0; i < remainder; i++) {
        d[i] = s[i] | m[i];
    }
#else
    for (size_t i = 0; i < len; i++) {
        dst[i] = src[i] | mask[i];
    }
#endif
}

int16_t simd_fir_filter_32(const int16_t* history_32, const int16_t* coeffs_32) {
    if (!history_32 || !coeffs_32) return 0;

    int32_t acc = 0;
    for (int i = 0; i < 32; i++) {
        acc += (int32_t)history_32[i] * coeffs_32[i];
    }
    return (int16_t)(acc >> 15);
}

int16_t simd_max30102_fir_sample(int16_t sample) {
    // Shift delay line by 1
    for (int i = 31; i > 0; i--) {
        s_fir_history[i] = s_fir_history[i - 1];
    }
    s_fir_history[0] = sample;

    return simd_fir_filter_32(s_fir_history, s_fir_coeffs);
}

void simd_max30102_fir_reset(void) {
    memset(s_fir_history, 0, sizeof(s_fir_history));
}

void simd_fir_filter_batch(const int16_t* input, const int16_t* coeffs, int16_t* output, size_t num_samples) {
    if (!input || !coeffs || !output || num_samples < 32) return;
    for (size_t i = 31; i < num_samples; i++) {
        int32_t acc = 0;
        for (int tap = 0; tap < 32; tap++) {
            acc += (int32_t)input[i - tap] * coeffs[tap];
        }
        output[i - 31] = (int16_t)(acc >> 15);
    }
}
