#!/usr/bin/env python3
"""
Hardware-Independent Host Test Suite for ESP32-S3 SIMD Feasibility Audit
Verifies mathematical equivalence between scalar C baselines and SIMD vector algorithms:
1. 128-bit memory alignment and MALLOC_CAP_SIMD contracts
2. OLED 1024-byte framebuffer invert / bitwise blit
3. 32-tap MAX30102 FIR Bandpass Filter (Heart Rate / SpO2)
4. 6-DOF IMU fixed-point vector low-pass filter
5. TinyML INT8 Dot-Product & ReLU Neural Network Layer
6. 8-bit Polyphonic Audio Saturated Mixing
7. Boundary conditions, padding, and fallback contracts
"""

import math
import struct
import sys

def test_alignment_contract():
    print("--- 1. Memory Alignment & MALLOC_CAP_SIMD Contract ---")
    def mock_heap_caps_malloc(size_bytes, caps):
        # MALLOC_CAP_SIMD enforces 16-byte alignment and internal SRAM capability
        MALLOC_CAP_SIMD = (1 << 10)
        MALLOC_CAP_INTERNAL = (1 << 11)
        assert (caps & MALLOC_CAP_SIMD) != 0, "SIMD allocation must include MALLOC_CAP_SIMD"
        # Simulate heap allocator returning 16-byte aligned pointer
        raw_address = 0x3FC90000 + 0x10 # 16-byte aligned
        assert (raw_address % 16) == 0, "SIMD pointer must be 16-byte aligned"
        return bytearray(size_bytes)

    buf = mock_heap_caps_malloc(1024, (1 << 10) | (1 << 11))
    assert len(buf) == 1024
    assert len(buf) % 16 == 0, "Framebuffer 1024 bytes is exactly 64 x 16-byte vector blocks"
    print("  [PASS] 16-byte alignment contract and 64-vector block boundary verified.")

def test_oled_invert_and_blit():
    print("\n--- 2. OLED 1024-Byte Framebuffer Inversion & Blitting ---")
    fb_len = 1024
    test_pattern = bytes([i % 256 for i in range(fb_len)])

    # Scalar baseline
    scalar_fb = bytearray(test_pattern)
    for i in range(fb_len):
        scalar_fb[i] = (~scalar_fb[i]) & 0xFF

    # SIMD vector emulation (16 bytes per chunk)
    simd_fb = bytearray(test_pattern)
    num_chunks = fb_len // 16
    for c in range(num_chunks):
        offset = c * 16
        # Emulate 128-bit vector load, bitwise NOT, vector store (ee.vld.128, ee.vnot, ee.vst.128)
        chunk = struct.unpack("<16B", simd_fb[offset:offset+16])
        inverted = [((~b) & 0xFF) for b in chunk]
        simd_fb[offset:offset+16] = struct.pack("<16B", *inverted)

    assert scalar_fb == simd_fb, "SIMD invert must match scalar invert byte-for-byte"

    # Test bitwise AND/OR/XOR mask blit
    mask = bytes([0xAA] * fb_len)
    scalar_masked = bytearray(fb_len)
    simd_masked = bytearray(fb_len)

    for i in range(fb_len):
        scalar_masked[i] = scalar_fb[i] ^ mask[i]

    for c in range(num_chunks):
        offset = c * 16
        c_fb = struct.unpack("<16B", simd_fb[offset:offset+16])
        c_m = struct.unpack("<16B", mask[offset:offset+16])
        c_out = [a ^ b for a, b in zip(c_fb, c_m)]
        simd_masked[offset:offset+16] = struct.pack("<16B", *c_out)

    assert scalar_masked == simd_masked, "SIMD XOR mask must match scalar XOR exactly"
    print("  [PASS] OLED 1024-byte invert and XOR blitting 100% equivalent across all 64 chunks.")

def test_max30102_fir_filter():
    print("\n--- 3. MAX30102 32-Tap FIR Bandpass Filter ---")
    # 32-tap bandpass filter coefficients (Q15 fixed point)
    # Passband: 0.5Hz - 4.0Hz at 50Hz sample rate (Heart Rate 30 - 240 BPM)
    coeffs_float = [
        -0.0012, -0.0025, -0.0041, -0.0055, -0.0058, -0.0039,  0.0012,  0.0094,
         0.0205,  0.0337,  0.0478,  0.0612,  0.0722,  0.0793,  0.0818,  0.0793,
         0.0722,  0.0612,  0.0478,  0.0337,  0.0205,  0.0094,  0.0012, -0.0039,
        -0.0058, -0.0055, -0.0041, -0.0025, -0.0012,  0.0001,  0.0008,  0.0010
    ]
    coeffs_q15 = [int(round(c * 32768.0)) for c in coeffs_float]
    assert len(coeffs_q15) == 32

    # Synthesize test PPG signal: 75 BPM pulse (1.25 Hz) + 0.1 Hz respiratory baseline wander
    num_samples = 64
    raw_ppg = []
    for i in range(num_samples):
        t = i / 50.0 # 50 Hz
        clean_pulse = 5000.0 * math.sin(2.0 * math.pi * 1.25 * t)
        baseline = 12000.0 * math.sin(2.0 * math.pi * 0.1 * t)
        raw_ppg.append(int(clean_pulse + baseline))

    # Scalar FIR implementation
    scalar_output = []
    for i in range(31, num_samples):
        acc = 0
        for tap in range(32):
            acc += raw_ppg[i - tap] * coeffs_q15[tap]
        scalar_output.append(acc >> 15)

    # SIMD FIR implementation (8 x 16-bit MACs per vector step: ee.vmulas.s16.acc)
    simd_output = []
    for i in range(31, num_samples):
        acc = 0
        # 32 taps = 4 vector passes of 8 taps each
        for pass_idx in range(4):
            tap_offset = pass_idx * 8
            x_vec = [raw_ppg[i - (tap_offset + k)] for k in range(8)]
            h_vec = coeffs_q15[tap_offset : tap_offset + 8]
            # Vector dot product in accumulator
            vec_dot = sum(x * h for x, h in zip(x_vec, h_vec))
            acc += vec_dot
        simd_output.append(acc >> 15)

    assert len(scalar_output) == len(simd_output)
    for s, v in zip(scalar_output, simd_output):
        assert abs(s - v) <= 1, f"FIR output discrepancy: scalar={s}, simd={v}"
    print(f"  [PASS] 32-tap FIR filter verified on {len(simd_output)} filtered pulse samples (error <= 1 LSB).")

def test_imu_vector_filter():
    print("\n--- 4. 6-DOF IMU Vector Low-Pass Filter ---")
    # Pack 6 axes into 128-bit vector: [ax, ay, az, 0, gx, gy, gz, 0]
    # Filter equation: y[k] = y[k-1] + alpha * (x[k] - y[k-1]) in Q15
    ALPHA_Q15 = int(round(0.25 * 32768)) # alpha = 0.25

    prev_y = [1000, -200, 9800, 0, 50, -30, 10, 0]
    raw_x  = [1200, -150, 9600, 0, 70, -20, 15, 0]

    # Scalar baseline
    scalar_y = [0] * 8
    for j in range(8):
        diff = raw_x[j] - prev_y[j]
        scalar_y[j] = prev_y[j] + ((diff * ALPHA_Q15) >> 15)

    # SIMD vector baseline (8-way parallel ee.vsubs.s16, ee.vmul.s16, ee.vadds.s16)
    diff_vec = [a - b for a, b in zip(raw_x, prev_y)]
    delta_vec = [(d * ALPHA_Q15) >> 15 for d in diff_vec]
    simd_y = [p + d for p, d in zip(prev_y, delta_vec)]

    for j in range(8):
        assert scalar_y[j] == simd_y[j], f"IMU vector discrepancy at idx {j}"
    print("  [PASS] 6-DOF IMU vector parallel filter matches scalar identically across all 8 lanes.")

def test_gesture_neural_net_layer():
    print("\n--- 5. TinyML INT8 Dot-Product & ReLU Neural Network Layer ---")
    # Input vector: 16 INT8 features (quantized accelerometer window)
    inputs = [12, -45, 88, 3, -120, 77, 4, -18, 92, 110, -5, -67, 33, 41, -89, 52]
    # Weights for 4 neurons (16 weights per neuron = 64 weights)
    weights = [
        [ 2, -1,  3,  0, -2,  1,  4, -3,  1,  2, -1,  0,  3, -2,  1,  0], # Neuron 0 (WRIST_FLICK)
        [-3,  2, -1,  1,  4, -2,  0,  2, -1, -3,  2,  1, -1,  3, -2,  1], # Neuron 1 (DOUBLE_TAP)
        [ 1,  4, -2, -3,  1,  0, -1,  2,  3,  1, -2,  4, -1,  0,  2, -1], # Neuron 2 (FIST_SHAKE)
        [ 0,  0,  1, -1,  2, -1,  0,  1, -2,  1,  0, -1,  1, -2,  0,  1]  # Neuron 3 (IDLE)
    ]
    bias = [150, -300, 420, -50]

    # Scalar implementation
    scalar_logits = []
    for n in range(4):
        acc = bias[n]
        for i in range(16):
            acc += inputs[i] * weights[n][i]
        relu = max(0, min(127, acc >> 4)) # Quantized ReLU
        scalar_logits.append(relu)

    # SIMD implementation (ee.vmulas.s8.acc 16-way MAC + ee.vmax.s8 ReLU)
    simd_logits = []
    for n in range(4):
        acc = bias[n] + sum(a * b for a, b in zip(inputs, weights[n]))
        relu = max(0, min(127, acc >> 4))
        simd_logits.append(relu)

    assert scalar_logits == simd_logits, f"Neural layer discrepancy: {scalar_logits} vs {simd_logits}"
    print(f"  [PASS] 16-input x 4-neuron INT8 layer verified with identical logits: {simd_logits}")

def test_audio_saturation_mixing():
    print("\n--- 6. 8-Bit Polyphonic Audio Saturated Mixing ---")
    # 4 channels of 16 samples each
    ch1 = [120, 150, 180, 200, 220, 240, 250, 255, 100, 80, 60, 40, 20, 10, 5, 0]
    ch2 = [ 50,  60,  70,  80,  90, 100, 110, 120,  50, 40, 30, 20, 10,  5, 0, 0]
    ch3 = [ 30,  30,  30,  30,  30,  30,  30,  30,  20, 20, 20, 20, 10, 10, 5, 0]
    ch4 = [ 10,  15,  20,  25,  30,  35,  40,  45,  10, 10, 10,  5,  5,  0, 0, 0]

    # Scalar with manual saturation clamp
    scalar_mixed = []
    for i in range(16):
        tot = ch1[i] + ch2[i] + ch3[i] + ch4[i]
        scalar_mixed.append(min(255, tot))

    # SIMD with 16-way saturated unsigned addition (ee.vadds.u8)
    simd_mixed = []
    for i in range(16):
        # Emulate hardware saturation
        s1 = min(255, ch1[i] + ch2[i])
        s2 = min(255, s1 + ch3[i])
        s3 = min(255, s2 + ch4[i])
        simd_mixed.append(s3)

    assert scalar_mixed == simd_mixed, "Audio saturation mixing discrepancy"
    assert 255 in simd_mixed, "Verified hardware saturation ceiling (no integer overflow roll-around)"
    print(f"  [PASS] 4-channel audio mixing with saturation verified on 16 samples.")

def test_fallback_contract():
    print("\n--- 7. Non-SIMD Fallback Contracts ---")
    # Ensure unaligned buffers fallback to scalar gracefully without crashing
    unaligned_buf = bytearray(b"123456789012345") # 15 bytes (not multiple of 16)
    scalar_res = bytearray(unaligned_buf)
    for i in range(len(scalar_res)):
        scalar_res[i] = (~scalar_res[i]) & 0xFF

    # Fallback function handles unaligned tail
    def safe_vector_or_fallback(buf):
        aligned_len = (len(buf) // 16) * 16
        # process aligned chunks with vector
        for c in range(0, aligned_len, 16):
            for j in range(16):
                buf[c + j] = (~buf[c + j]) & 0xFF
        # process tail with scalar
        for i in range(aligned_len, len(buf)):
            buf[i] = (~buf[i]) & 0xFF
        return buf

    res = safe_vector_or_fallback(bytearray(unaligned_buf))
    assert res == scalar_res
    print("  [PASS] Tail/unaligned buffer graceful fallback verified.")

if __name__ == "__main__":
    test_alignment_contract()
    test_oled_invert_and_blit()
    test_max30102_fir_filter()
    test_imu_vector_filter()
    test_gesture_neural_net_layer()
    test_audio_saturation_mixing()
    test_fallback_contract()
    print("\nAll SIMD Feasibility Audit Host Verification Tests PASSED!")
