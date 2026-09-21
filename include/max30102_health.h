#ifndef MAX30102_HEALTH_H
#define MAX30102_HEALTH_H

#include <Arduino.h>
#include <MAX30105.h>

struct HealthLiveData {
    uint32_t ir;
    uint32_t red;
    int bpm;
    int spo2;
    float sensor_temp;
    uint8_t signal_quality;
    bool finger_detected;
    bool bpm_valid;
    bool spo2_valid;
};

struct HealthHistoryEntry {
    int64_t timestamp;
    int16_t bpm;
    int16_t spo2;
    int16_t temp_x10;
    uint8_t quality;
    uint8_t valid;
};

class Max30102Health {
public:
    Max30102Health();

    bool begin();
    void loop();

    void startLive();
    void stopLive();
    bool isLiveActive() const { return live_active; }

    const HealthLiveData& getLiveData() const { return live; }
    int getPage() const { return page; }
    void setPage(int p);
    void nextPage();
    void previousPage();

    bool isDetected() const { return sensor_ok; }

    bool logBackgroundSample();
    bool getTodayHistory(HealthHistoryEntry* buffer, int max_entries) const;
    int getTodayHistoryCount() const;

private:
    MAX30105 sensor;
    bool sensor_ok;
    bool live_active;
    int page;
    HealthLiveData live;

    static const int LIVE_POINTS = 64;
    uint32_t ir_history[LIVE_POINTS];
    uint32_t red_history[LIVE_POINTS];
    uint8_t history_index;
    uint8_t history_count;

    static const int SPO2_WINDOW = 100;
    uint32_t ir_window[SPO2_WINDOW];
    uint32_t red_window[SPO2_WINDOW];
    uint16_t window_count;

    float dc_ir;
    float dc_red;
    float prev_ac;
    uint32_t last_peak_ms;
    uint32_t last_temp_ms;
    float bpm_sum;
    uint8_t bpm_samples;

    void configureSensor();
    void processSample(uint32_t ir, uint32_t red);
    void updateMetrics();
    void calculateSpO2();
    uint8_t calculateQuality(uint32_t ir, uint32_t red) const;
    bool hasFinger(uint32_t ir) const;

    bool appendHistory(const HealthHistoryEntry& entry);
    int readHistory(HealthHistoryEntry* buffer, int max_entries) const;
    int64_t nowEpoch() const;
    int64_t startOfToday() const;
};

extern Max30102Health healthManager;

#endif
