#include <iostream>
#include <cassert>
#include <vector>
#include <algorithm>
#include "simd_accel.h"

int main() {
    const size_t LEN = 64;
    ALIGNED16 uint8_t ch1[LEN];
    ALIGNED16 uint8_t ch2[LEN];
    ALIGNED16 uint8_t ch3[LEN];
    ALIGNED16 uint8_t ch4[LEN];
    ALIGNED16 uint8_t dst[LEN];

    for (size_t i = 0; i < LEN; i++) {
        ch1[i] = (uint8_t)(i * 4);
        ch2[i] = (uint8_t)(i * 2 + 10);
        ch3[i] = (uint8_t)(i * 3 + 5);
        ch4[i] = (uint8_t)(i + 1);
    }

    simd_mix_audio_8bit(dst, ch1, ch2, ch3, ch4, LEN);

    for (size_t i = 0; i < LEN; i++) {
        uint32_t expected = (uint32_t)ch1[i] + ch2[i] + ch3[i] + ch4[i];
        if (expected > 255) expected = 255;
        assert(dst[i] == (uint8_t)expected);
    }

    // Verify unaligned tail mixing (e.g. 19 samples)
    uint8_t dst_tail[19];
    simd_mix_audio_8bit(dst_tail, ch1, ch2, ch3, ch4, 19);
    for (size_t i = 0; i < 19; i++) {
        uint32_t expected = (uint32_t)ch1[i] + ch2[i] + ch3[i] + ch4[i];
        if (expected > 255) expected = 255;
        assert(dst_tail[i] == (uint8_t)expected);
    }

    std::cout << "[PASS] Host C++ simd_mix_audio_8bit unit verification passed." << std::endl;
    return 0;
}
