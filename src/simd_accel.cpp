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
    // 4 passes unrolled across 8 taps for compiler auto-vectorization and pipelined MAC execution
    for (int p = 0; p < 4; p++) {
        int idx = p * 8;
        acc += (int32_t)history_32[idx + 0] * coeffs_32[idx + 0];
        acc += (int32_t)history_32[idx + 1] * coeffs_32[idx + 1];
        acc += (int32_t)history_32[idx + 2] * coeffs_32[idx + 2];
        acc += (int32_t)history_32[idx + 3] * coeffs_32[idx + 3];
        acc += (int32_t)history_32[idx + 4] * coeffs_32[idx + 4];
        acc += (int32_t)history_32[idx + 5] * coeffs_32[idx + 5];
        acc += (int32_t)history_32[idx + 6] * coeffs_32[idx + 6];
        acc += (int32_t)history_32[idx + 7] * coeffs_32[idx + 7];
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
        output[i - 31] = simd_fir_filter_32(&input[i - 31], coeffs);
    }
}

void simd_blend_frames_128(uint8_t* dst, const uint8_t* old_buf, const uint8_t* new_buf, const uint8_t* mask, size_t len) {
    if (!dst || !old_buf || !new_buf || !mask || len == 0) return;

    if ((((uintptr_t)dst | (uintptr_t)old_buf | (uintptr_t)new_buf | (uintptr_t)mask) & 0xF) != 0 || len < 16) {
        for (size_t i = 0; i < len; i++) {
            dst[i] = (old_buf[i] & ~mask[i]) | (new_buf[i] & mask[i]);
        }
        return;
    }

#if defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(NO_SIMD)
    size_t chunks = len / 16;
    size_t remainder = len % 16;
    uint8_t* d = dst;
    const uint8_t* o = old_buf;
    const uint8_t* n = new_buf;
    const uint8_t* m = mask;
    asm volatile (
        "1:\n"
        "ee.vld.128.ip   q0, %1, 16\n"
        "ee.vld.128.ip   q1, %2, 16\n"
        "ee.vld.128.ip   q2, %3, 16\n"
        "ee.notq         q3, q2\n"
        "ee.andq         q0, q0, q3\n"
        "ee.andq         q1, q1, q2\n"
        "ee.orq          q0, q0, q1\n"
        "ee.vst.128.ip   q0, %0, 16\n"
        "addi            %4, %4, -1\n"
        "bnez            %4, 1b\n"
        : "+r"(d), "+r"(o), "+r"(n), "+r"(m), "+r"(chunks)
        :
        : "memory"
    );
    for (size_t i = 0; i < remainder; i++) {
        d[i] = (o[i] & ~m[i]) | (n[i] & m[i]);
    }
#else
    for (size_t i = 0; i < len; i++) {
        dst[i] = (old_buf[i] & ~mask[i]) | (new_buf[i] & mask[i]);
    }
#endif
}

void simd_generate_dither_mask(uint8_t* mask, uint8_t step, size_t len) {
    if (!mask || len == 0) return;

    if (step == 0) {
        memset(mask, 0x00, len);
        return;
    }
    if (step >= 16) {
        memset(mask, 0xFF, len);
        return;
    }

    static const uint8_t bayer_matrix[4][4] = {
        {  0,  8,  2, 10 },
        { 12,  4, 14,  6 },
        {  3, 11,  1,  9 },
        { 15,  7, 13,  5 }
    };

    uint8_t col_pattern[4] = {0, 0, 0, 0};
    for (int col = 0; col < 4; col++) {
        uint8_t b = 0;
        for (int row = 0; row < 4; row++) {
            if (bayer_matrix[row][col] < step) {
                b |= (1 << row);
                b |= (1 << (row + 4));
            }
        }
        col_pattern[col] = b;
    }

    uint8_t pattern16[16] ALIGNED16;
    for (int i = 0; i < 16; i++) {
        pattern16[i] = col_pattern[i % 4];
    }

    size_t chunks = len / 16;
    for (size_t c = 0; c < chunks; c++) {
        memcpy(&mask[c * 16], pattern16, 16);
    }
    for (size_t i = chunks * 16; i < len; i++) {
        mask[i] = col_pattern[i % 4];
    }
}

void simd_generate_wipe_mask(uint8_t* mask, uint8_t progress_pct, uint8_t direction, size_t len) {
    if (!mask || len == 0) return;

    if (progress_pct == 0) {
        memset(mask, 0x00, len);
        return;
    }
    if (progress_pct >= 100) {
        memset(mask, 0xFF, len);
        return;
    }

    if (direction == 0) {
        // Horizontal Wipe (Left to Right across 128 columns)
        uint16_t thresh_col = (uint16_t)((progress_pct * 128) / 100);
        for (size_t p = 0; p < 8 && (p * 128) < len; p++) {
            size_t page_offset = p * 128;
            for (size_t col = 0; col < 128 && (page_offset + col) < len; col++) {
                mask[page_offset + col] = (col < thresh_col) ? 0xFF : 0x00;
            }
        }
    } else {
        // Vertical Wipe (Top to Bottom across 64 rows / 8 pages)
        uint16_t thresh_row = (uint16_t)((progress_pct * 64) / 100);
        uint8_t active_page = thresh_row / 8;
        uint8_t row_remainder = thresh_row % 8;
        uint8_t partial_byte = (row_remainder > 0) ? (uint8_t)((1 << row_remainder) - 1) : 0x00;

        for (size_t p = 0; p < 8 && (p * 128) < len; p++) {
            size_t page_offset = p * 128;
            uint8_t val = (p < active_page) ? 0xFF : (p == active_page ? partial_byte : 0x00);
            memset(&mask[page_offset], val, 128);
        }
    }
}

void simd_filter_imu_6dof(int16_t* filtered_y, const int16_t* raw_x, uint16_t alpha_q15) {
    if (!filtered_y || !raw_x) return;

    int32_t a = (int32_t)alpha_q15;
    // 8-lane parallel execution (6 DOF: ax, ay, az, 0, gx, gy, gz, 0)
    #pragma GCC unroll 8
    for (int j = 0; j < 8; j++) {
        int32_t diff = (int32_t)raw_x[j] - (int32_t)filtered_y[j];
        filtered_y[j] = (int16_t)(filtered_y[j] + ((diff * a) >> 15));
    }
}
