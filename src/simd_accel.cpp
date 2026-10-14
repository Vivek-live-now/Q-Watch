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

void simd_nn_dense_layer_int8(const int8_t* inputs, const int8_t* weights, const int32_t* bias, int8_t* outputs, size_t num_inputs, size_t num_neurons) {
    if (!inputs || !weights || !bias || !outputs || num_inputs == 0 || num_neurons == 0) return;

    for (size_t n = 0; n < num_neurons; n++) {
        int32_t acc = bias[n];
        const int8_t* w_row = &weights[n * num_inputs];

        #pragma GCC unroll 16
        for (size_t i = 0; i < num_inputs; i++) {
            acc += (int32_t)inputs[i] * (int32_t)w_row[i];
        }

        int32_t relu = acc >> 4;
        if (relu < 0) relu = 0;
        else if (relu > 127) relu = 127;
        outputs[n] = (int8_t)relu;
    }
}

void simd_mix_audio_8bit(uint8_t* dst, const uint8_t* ch1, const uint8_t* ch2, const uint8_t* ch3, const uint8_t* ch4, size_t len) {
    if (!dst || len == 0) return;

    size_t chunks = len / 16;
    size_t remainder = len % 16;

    for (size_t c = 0; c < chunks; c++) {
        size_t off = c * 16;
        #pragma GCC unroll 16
        for (size_t i = 0; i < 16; i++) {
            uint16_t tot = 0;
            if (ch1) tot += ch1[off + i];
            if (ch2) tot += ch2[off + i];
            if (ch3) tot += ch3[off + i];
            if (ch4) tot += ch4[off + i];
            dst[off + i] = (tot > 255) ? 255 : (uint8_t)tot;
        }
    }

    size_t rem_start = chunks * 16;
    for (size_t i = 0; i < remainder; i++) {
        uint16_t tot = 0;
        if (ch1) tot += ch1[rem_start + i];
        if (ch2) tot += ch2[rem_start + i];
        if (ch3) tot += ch3[rem_start + i];
        if (ch4) tot += ch4[rem_start + i];
        dst[rem_start + i] = (tot > 255) ? 255 : (uint8_t)tot;
    }
}

// Fast Q14 sine lookup table (0..90 deg, scale 16384)
static const int16_t s_sin_lut_q14[91] = {
    0, 286, 572, 857, 1143, 1428, 1713, 1997, 2280, 2563,
    2845, 3126, 3406, 3686, 3964, 4240, 4516, 4790, 5063, 5334,
    5604, 5872, 6138, 6402, 6664, 6924, 7182, 7438, 7692, 7943,
    8192, 8438, 8682, 8923, 9162, 9397, 9630, 9860, 10087, 10311,
    10531, 10749, 10963, 11174, 11381, 11585, 11785, 11982, 12175, 12365,
    12551, 12733, 12911, 13085, 13255, 13421, 13583, 13741, 13894, 14043,
    14188, 14328, 14463, 14594, 14720, 14842, 14959, 15071, 15179, 15281,
    15379, 15472, 15560, 15643, 15721, 15794, 15862, 15925, 15983, 16036,
    16084, 16127, 16165, 16198, 16225, 16248, 16265, 16277, 16284, 16285,
    16384
};

static int16_t sin_deg_q14(int16_t deg) {
    deg = deg % 360;
    if (deg < 0) deg += 360;

    if (deg <= 90) return s_sin_lut_q14[deg];
    if (deg <= 180) return s_sin_lut_q14[180 - deg];
    if (deg <= 270) return -s_sin_lut_q14[deg - 180];
    return -s_sin_lut_q14[360 - deg];
}

static int16_t cos_deg_q14(int16_t deg) {
    return sin_deg_q14(deg + 90);
}

void simd_compute_rotation_matrix_q14(int16_t* matrix_9, int16_t pitch_deg, int16_t roll_deg, int16_t yaw_deg) {
    if (!matrix_9) return;

    int32_t cx = cos_deg_q14(pitch_deg);
    int32_t sx = sin_deg_q14(pitch_deg);
    int32_t cy = cos_deg_q14(roll_deg);
    int32_t sy = sin_deg_q14(roll_deg);
    int32_t cz = cos_deg_q14(yaw_deg);
    int32_t sz = sin_deg_q14(yaw_deg);

    // R = Rz(yaw) * Ry(roll) * Rx(pitch)
    // Row 0
    matrix_9[0] = (int16_t)((cy * cz) >> 14);
    matrix_9[1] = (int16_t)((((sx * sy) >> 14) * cz - cx * sz) >> 14);
    matrix_9[2] = (int16_t)((((cx * sy) >> 14) * cz + sx * sz) >> 14);

    // Row 1
    matrix_9[3] = (int16_t)((cy * sz) >> 14);
    matrix_9[4] = (int16_t)((((sx * sy) >> 14) * sz + cx * cz) >> 14);
    matrix_9[5] = (int16_t)((((cx * sy) >> 14) * sz - sx * cz) >> 14);

    // Row 2
    matrix_9[6] = (int16_t)(-sy);
    matrix_9[7] = (int16_t)((sx * cy) >> 14);
    matrix_9[8] = (int16_t)((cx * cy) >> 14);
}

void simd_transform_3d_vertices(Point2D* out_2d, const Point3D* in_3d, size_t num_vertices, const int16_t* rot_matrix_q14, int16_t cam_dist, int16_t screen_cx, int16_t screen_cy) {
    if (!out_2d || !in_3d || !rot_matrix_q14 || num_vertices == 0) return;

    int32_t r00 = rot_matrix_q14[0];
    int32_t r01 = rot_matrix_q14[1];
    int32_t r02 = rot_matrix_q14[2];
    int32_t r10 = rot_matrix_q14[3];
    int32_t r11 = rot_matrix_q14[4];
    int32_t r12 = rot_matrix_q14[5];
    int32_t r20 = rot_matrix_q14[6];
    int32_t r21 = rot_matrix_q14[7];
    int32_t r22 = rot_matrix_q14[8];

    size_t chunks = num_vertices / 4;
    size_t remainder = num_vertices % 4;

    for (size_t c = 0; c < chunks; c++) {
        size_t off = c * 4;
        #pragma GCC unroll 4
        for (size_t i = 0; i < 4; i++) {
            const Point3D& pt = in_3d[off + i];
            int32_t x = pt.x;
            int32_t y = pt.y;
            int32_t z = pt.z;

            int32_t rx = (r00 * x + r01 * y + r02 * z) >> 14;
            int32_t ry = (r10 * x + r11 * y + r12 * z) >> 14;
            int32_t rz = (r20 * x + r21 * y + r22 * z) >> 14;

            int32_t z_eff = rz + cam_dist;
            if (z_eff < 10) z_eff = 10;

            out_2d[off + i].x = (int16_t)(screen_cx + (rx * cam_dist) / z_eff);
            out_2d[off + i].y = (int16_t)(screen_cy - (ry * cam_dist) / z_eff);
        }
    }

    size_t rem_start = chunks * 4;
    for (size_t i = 0; i < remainder; i++) {
        const Point3D& pt = in_3d[rem_start + i];
        int32_t x = pt.x;
        int32_t y = pt.y;
        int32_t z = pt.z;

        int32_t rx = (r00 * x + r01 * y + r02 * z) >> 14;
        int32_t ry = (r10 * x + r11 * y + r12 * z) >> 14;
        int32_t rz = (r20 * x + r21 * y + r22 * z) >> 14;

        int32_t z_eff = rz + cam_dist;
        if (z_eff < 10) z_eff = 10;

        out_2d[rem_start + i].x = (int16_t)(screen_cx + (rx * cam_dist) / z_eff);
        out_2d[rem_start + i].y = (int16_t)(screen_cy - (ry * cam_dist) / z_eff);
    }
}


