#include <iostream>
#include <cassert>
#include <vector>
#include <cmath>
#include <cstring>
#include "simd_accel.h"

// 3-sample median calculation
static inline int16_t calc_median3(int16_t a, int16_t b, int16_t c) {
    if (a > b) {
        if (b > c) return b;
        return (a > c) ? c : a;
    } else {
        if (a > c) return a;
        return (b > c) ? c : b;
    }
}

// 1D Kalman filter state
struct Kalman1D {
    float x;
    float p;
    float q;
    float r;

    void init(float init_val, float proc_q = 0.05f, float meas_r = 4.0f) {
        x = init_val;
        p = 1.0f;
        q = proc_q;
        r = meas_r;
    }

    float update(float meas) {
        p += q;
        float k = p / (p + r);
        x += k * (meas - x);
        p *= (1.0f - k);
        return x;
    }
};

int main() {
    std::cout << "Starting Host C++ Magnetometer Filter Suite verification..." << std::endl;

    // 1. Test simd_filter_mag_3d parallel vector filter
    {
        ALIGNED16 int16_t filtered_mag[4] = { 200, -300, 450, 0 };
        ALIGNED16 int16_t raw_mag[4] = { 280, -220, 530, 0 };
        uint16_t alpha_q15 = 8192; // 0.25 in Q15

        simd_filter_mag_3d(filtered_mag, raw_mag, alpha_q15);

        // Expected: filtered = prev + ((raw - prev) * 8192) >> 15
        // diff_x = 80, (80 * 8192) >> 15 = 20 -> filtered_x = 220
        // diff_y = 80, (80 * 8192) >> 15 = 20 -> filtered_y = -280
        // diff_z = 80, (80 * 8192) >> 15 = 20 -> filtered_z = 470
        assert(filtered_mag[0] == 220);
        assert(filtered_mag[1] == -280);
        assert(filtered_mag[2] == 470);
        assert(filtered_mag[3] == 0);

        // Alpha = 32768 (1.0 passthrough)
        ALIGNED16 int16_t cur_state[4] = { 100, 100, 100, 0 };
        ALIGNED16 int16_t new_input[4] = { 500, -500, 900, 0 };
        simd_filter_mag_3d(cur_state, new_input, 32768);
        assert(cur_state[0] == 500);
        assert(cur_state[1] == -500);
        assert(cur_state[2] == 900);
        assert(cur_state[3] == 0);

        // Exponential step response convergence over 20 iterations
        ALIGNED16 int16_t step_state[4] = { 0, 0, 0, 0 };
        ALIGNED16 int16_t step_target[4] = { 1000, 1000, 1000, 0 };
        for (int i = 0; i < 25; i++) {
            simd_filter_mag_3d(step_state, step_target, 8192);
        }
        assert(step_state[0] >= 995 && step_state[0] <= 1000);
        assert(step_state[1] >= 995 && step_state[1] <= 1000);
        assert(step_state[2] >= 995 && step_state[2] <= 1000);
    }

    // 2. Test 3-Axis Median Filter Spike Rejection
    {
        int16_t baseline = 150;
        int16_t history[3] = { baseline, baseline, baseline };

        // Test normal steady baseline
        assert(calc_median3(history[0], history[1], history[2]) == baseline);

        // Inject massive EMI / buzzer spike on 1 sample
        int16_t spike_sample = 2800; // buzzer EMF glitch
        history[0] = spike_sample; // window is now {2800, 150, 150}
        int16_t med_out = calc_median3(history[0], history[1], history[2]);
        assert(med_out == baseline); // Spike completely stripped!

        // Next sample returns to normal baseline
        history[1] = 155; // window is now {2800, 155, 150}
        med_out = calc_median3(history[0], history[1], history[2]);
        assert(med_out == 155); // Median tracks actual signal

        // Negative EMI spike
        history[2] = -3000; // window is now {2800, 155, -3000}
        med_out = calc_median3(history[0], history[1], history[2]);
        assert(med_out == 155); // Both high and low outliers rejected!
    }

    // 3. Test 1D/3D Kalman Filter Convergence & Covariance Reduction
    {
        Kalman1D kf;
        // SensorManager initializes Kalman with first sample
        kf.init(500.0f, 0.05f, 4.0f);
        assert(kf.p == 1.0f);

        // Drive with noisy measurements around 500.0f (noise: +/- 10)
        float sum_err = 0.0f;
        for (int i = 0; i < 50; i++) {
            float noise = (i % 2 == 0) ? 8.0f : -8.0f;
            float filtered = kf.update(500.0f + noise);
            sum_err += std::fabs(filtered - 500.0f);
        }
        // State should remain tightly near 500.0f
        assert(std::fabs(kf.x - 500.0f) < 2.0f);
        // Error covariance p should stabilize
        assert(kf.p > 0.0f && kf.p < 1.0f);
    }

    // 4. Test Hybrid Cascade: Median filter -> SIMD Vector LPF
    {
        int16_t history[3] = { 300, 300, 300 };
        ALIGNED16 int16_t lpf_state[4] = { 300, 300, 300, 0 };

        // An anomaly arrives: 2900 (buzzer spike)
        history[0] = 2900;
        int16_t med = calc_median3(history[0], history[1], history[2]);
        assert(med == 300); // Median removes the spike

        ALIGNED16 int16_t stage2_in[4] = { med, med, med, 0 };
        simd_filter_mag_3d(lpf_state, stage2_in, 8192);

        // State remains pristine 300 despite massive input spike
        assert(lpf_state[0] == 300);
        assert(lpf_state[1] == 300);
        assert(lpf_state[2] == 300);
    }

    std::cout << "[PASS] Host C++ Magnetometer Filter Suite verification passed." << std::endl;
    return 0;
}
