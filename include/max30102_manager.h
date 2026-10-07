#ifndef MAX30102_MANAGER_H
#define MAX30102_MANAGER_H

#include <Arduino.h>
#include <MAX30105.h>

struct HealthHistoryEntry {
    uint32_t timestamp;  // Real Unix timestamp (epoch seconds)
    uint8_t bpm;         // Heart rate (0-255, 0 = invalid)
    uint8_t spo2;        // SpO2 percentage (0-100, 0 = invalid)
    int16_t temp_x10;    // Temp * 10 in C
};

struct HealthMetrics {
    int bpm;             // 0 if invalid / unmeasured
    int spo2;            // 0 if invalid / unmeasured
    float temperature;   // MAX30102 die temperature in C
    bool finger_detected;
    uint32_t ir_value;
    uint32_t red_value;
};

class MAX30102Manager {
public:
    MAX30102Manager();
    void begin();
    void loop();

    bool isAvailable() const { return sensor_ok; }
    HealthMetrics getMetrics() const { return current_metrics; }

    // Chronological Waveform buffer access (64 samples for real-time OLED graph)
    void getPPGWaveformChronological(uint8_t* out_buffer) const;
    int getPPGBufferSize() const { return 64; }

    // Power / Sensor state controls
    void enableSensor();
    void disableSensor();
    bool isEnabled() const { return sensor_enabled; }
    bool retryInit();

    // Visual heart beat pulse detection for HUD
    bool isBeating() const { return (last_beat_time > 0 && (millis() - last_beat_time) < 220); }
    uint32_t getLastBeatTime() const { return last_beat_time; }

    // Background recording sample routine (for wake-from-sleep or interval logging)
    bool takeSampleAndSave(uint32_t duration_ms = 7000);

    // Storage history retrieval
    int getHistoryCount() const;
    bool getHistory(HealthHistoryEntry* buffer, int max_entries) const;
    void logSample(uint8_t bpm, uint8_t spo2, float temp);

private:
    MAX30105 particleSensor;
    bool sensor_ok;
    bool sensor_enabled;

    HealthMetrics current_metrics;

    // Live PPG Waveform Circular Buffer
    static const int PPG_BUF_LEN = 64;
    uint8_t ppg_buffer[PPG_BUF_LEN];
    int ppg_head;

    uint32_t last_sample_time;
    uint32_t last_temp_read_time;

    // Real-time Beat Detection & IBI Tracking
    uint32_t last_beat_time;
    int16_t prev_filtered_ac;
    int16_t cycle_peak;
    uint32_t cycle_peak_time;
    bool pulse_rising;
    float dynamic_p2p;
    float ppg_envelope;

    // Moving average buffers for BPM and SpO2
    static const int BEAT_HIST_SIZE = 4;
    float bpm_history[BEAT_HIST_SIZE];
    int bpm_history_cnt;
    int bpm_history_idx;

    float spo2_history[BEAT_HIST_SIZE];
    int spo2_history_cnt;
    int spo2_history_idx;

    // Current beat cycle metrics for SpO2 R-ratio calculation
    uint32_t cycle_ir_min;
    uint32_t cycle_ir_max;
    uint32_t cycle_red_min;
    uint32_t cycle_red_max;
    uint64_t cycle_ir_dc_sum;
    uint64_t cycle_red_dc_sum;
    int cycle_samples;

    // DC baseline tracking filter
    uint32_t ir_dc_filter;
    uint32_t red_dc_filter;

    void processSample(uint32_t red, uint32_t ir);
    void resetBeatState();
    void readTemperature();
};

extern MAX30102Manager max30102Manager;

#endif // MAX30102_MANAGER_H
