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
    uint8_t ppg_buffer[64];
    int ppg_head;

    uint32_t last_sample_time;
    uint32_t last_temp_read_time;

    // Heart rate and SpO2 calculation variables
    static const int SAMPLE_SIZE = 100;
    uint32_t red_samples[SAMPLE_SIZE];
    uint32_t ir_samples[SAMPLE_SIZE];
    int sample_idx;

    void calculateBPMAndSpO2();
    void readTemperature();
};

extern MAX30102Manager max30102Manager;

#endif // MAX30102_MANAGER_H
