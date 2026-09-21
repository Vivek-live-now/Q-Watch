#include "max30102_manager.h"
#include "file_manager.h"
#include "settings_data.h"
#include "hw_config.h"
#include "clock.h"
#include <Wire.h>

MAX30102Manager max30102Manager;

MAX30102Manager::MAX30102Manager() :
    sensor_ok(false),
    sensor_enabled(false),
    ppg_head(0),
    last_sample_time(0),
    last_temp_read_time(0),
    sample_idx(0) {
    memset(&current_metrics, 0, sizeof(HealthMetrics));
    memset(ppg_buffer, 128, sizeof(ppg_buffer));
}

void MAX30102Manager::begin() {
    // Sensor shared I2C bus setup handled in SensorManager (400kHz Wire)
    if (particleSensor.begin(Wire, I2C_SPEED_FAST)) {
        sensor_ok = true;
        Serial.println("MAX30102 sensor detected at I2C address 0x57.");
        disableSensor();
    } else {
        sensor_ok = false;
        Serial.println("MAX30102 sensor not found on I2C bus.");
    }
}

void MAX30102Manager::enableSensor() {
    if (!sensor_ok || sensor_enabled) return;

    // Power on / setup MAX30102
    byte ledBrightness = 0x1F; // ~6.4mA options for finger detection and low power
    byte sampleAverage = 4;
    byte ledMode = 2; // Red + IR
    int sampleRate = 100;
    int pulseWidth = 411;
    int adcRange = 4096;

    particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
    particleSensor.enableDIETEMPRDY();
    sensor_enabled = true;
    sample_idx = 0;
    readTemperature();
}

void MAX30102Manager::disableSensor() {
    if (!sensor_ok) return;
    particleSensor.shutDown();
    sensor_enabled = false;
    current_metrics.finger_detected = false;
    current_metrics.bpm = 0;
    current_metrics.spo2 = 0;
}

void MAX30102Manager::readTemperature() {
    if (!sensor_ok) return;
    float t = particleSensor.readTemperature();
    if (t > -100.0f && t < 100.0f) {
        current_metrics.temperature = t;
    }
}

void MAX30102Manager::getPPGWaveformChronological(uint8_t* out_buffer) const {
    if (!out_buffer) return;
    for (int i = 0; i < 64; i++) {
        int idx = (ppg_head + i) % 64;
        out_buffer[i] = ppg_buffer[idx];
    }
}

void MAX30102Manager::loop() {
    if (!sensor_ok || !sensor_enabled) return;

    particleSensor.check(); // Poll sensor FIFO

    while (particleSensor.available()) {
        uint32_t red = particleSensor.getFIFORed();
        uint32_t ir = particleSensor.getFIFOIR();
        particleSensor.nextSample();

        current_metrics.red_value = red;
        current_metrics.ir_value = ir;

        // Finger Detection Threshold (IR > 20000 counts)
        if (ir > 20000) {
            current_metrics.finger_detected = true;

            // Normalize IR signal to 0-255 PPG waveform graph
            static uint32_t dc_filter = 50000;
            dc_filter = (dc_filter * 15 + ir) / 16;
            int32_t ac = (int32_t)ir - (int32_t)dc_filter;
            int graph_val = 128 + (ac / 30);
            if (graph_val < 0) graph_val = 0;
            if (graph_val > 255) graph_val = 255;

            ppg_buffer[ppg_head] = (uint8_t)graph_val;
            ppg_head = (ppg_head + 1) % 64;

            // Buffer samples for algorithm processing
            if (sample_idx < SAMPLE_SIZE) {
                red_samples[sample_idx] = red;
                ir_samples[sample_idx] = ir;
                sample_idx++;
            } else {
                calculateBPMAndSpO2();
                // Shift buffer by 25 samples
                for (int i = 0; i < SAMPLE_SIZE - 25; i++) {
                    red_samples[i] = red_samples[i + 25];
                    ir_samples[i] = ir_samples[i + 25];
                }
                sample_idx = SAMPLE_SIZE - 25;
            }

        } else {
            current_metrics.finger_detected = false;
            current_metrics.bpm = 0; // Clear stale values when no finger detected
            current_metrics.spo2 = 0;
            sample_idx = 0;
            ppg_buffer[ppg_head] = 128;
            ppg_head = (ppg_head + 1) % 64;
        }
    }

    // Read temperature every 5 seconds
    if (millis() - last_temp_read_time >= 5000) {
        readTemperature();
        last_temp_read_time = millis();
    }
}

void MAX30102Manager::calculateBPMAndSpO2() {
    int peaks = 0;
    uint32_t ir_mean = 0;
    uint32_t red_mean = 0;

    for (int i = 0; i < SAMPLE_SIZE; i++) {
        ir_mean += ir_samples[i];
        red_mean += red_samples[i];
    }
    ir_mean /= SAMPLE_SIZE;
    red_mean /= SAMPLE_SIZE;

    int32_t ir_max = 0, ir_min = 0xFFFFFF;
    int32_t red_max = 0, red_min = 0xFFFFFF;

    for (int i = 0; i < SAMPLE_SIZE; i++) {
        if ((int32_t)ir_samples[i] > ir_max) ir_max = ir_samples[i];
        if ((int32_t)ir_samples[i] < ir_min) ir_min = ir_samples[i];
        if ((int32_t)red_samples[i] > red_max) red_max = red_samples[i];
        if ((int32_t)red_samples[i] < red_min) red_min = red_samples[i];

        if (i > 1 && i < SAMPLE_SIZE - 1) {
            if (ir_samples[i] > ir_samples[i - 1] && ir_samples[i] > ir_samples[i + 1] && ir_samples[i] > ir_mean + 100) {
                peaks++;
            }
        }
    }

    int32_t ir_ac = ir_max - ir_min;
    int32_t red_ac = red_max - red_min;

    // Reset metrics to invalid (0) unless signal quality passes
    current_metrics.bpm = 0;
    current_metrics.spo2 = 0;

    if (peaks >= 2 && peaks <= 10 && ir_mean > 0 && red_mean > 0 && ir_ac > 200 && red_ac > 200) {
        int calculated_bpm = (peaks * 60) / 4;
        if (calculated_bpm >= 40 && calculated_bpm <= 200) {
            current_metrics.bpm = calculated_bpm;
        }

        float red_ratio = (float)red_ac / (float)red_mean;
        float ir_ratio = (float)ir_ac / (float)ir_mean;
        if (ir_ratio > 0.0001f) {
            float R = red_ratio / ir_ratio;
            // Experimental estimation: SpO2 = 104 - 17 * R
            int calculated_spo2 = (int)(104.0f - 17.0f * R);
            // Strict signal quality check: Do NOT clamp arbitrary out-of-bound values to 80/99
            if (calculated_spo2 >= 70 && calculated_spo2 <= 100) {
                current_metrics.spo2 = calculated_spo2;
            } else {
                current_metrics.spo2 = 0; // Mark as uncalculated/invalid --
            }
        }
    }
}

bool MAX30102Manager::takeSampleAndSave(uint32_t duration_ms) {
    if (!sensor_ok) return false;

    enableSensor();
    uint32_t start_time = millis();

    while (millis() - start_time < duration_ms) {
        loop();
        delay(10);
    }

    // Always log sample if die temperature is valid, storing 0 for invalid HR or SpO2
    bool recorded = false;
    if (current_metrics.temperature > -50.0f && current_metrics.temperature < 100.0f) {
        uint8_t bpm_to_log = (current_metrics.finger_detected && current_metrics.bpm >= 40) ? (uint8_t)current_metrics.bpm : 0;
        uint8_t spo2_to_log = (current_metrics.finger_detected && current_metrics.spo2 >= 70) ? (uint8_t)current_metrics.spo2 : 0;
        logSample(bpm_to_log, spo2_to_log, current_metrics.temperature);
        recorded = true;
    }

    disableSensor();
    return recorded;
}

void MAX30102Manager::logSample(uint8_t bpm, uint8_t spo2, float temp) {
    // Save snapshot even if HR/SpO2 is 0 as long as temperature reading is valid

    HealthHistoryEntry entry;
    entry.timestamp = qclock.getEpoch(); // Store real Unix epoch timestamp
    entry.bpm = bpm;
    entry.spo2 = spo2;
    entry.temp_x10 = (int16_t)(temp * 10.0f);

    fileManager.append("/health_history.bin", (const uint8_t*)&entry, sizeof(HealthHistoryEntry));

    // Keep history file capped at 288 records (24 hours at 5 min intervals)
    size_t sz = fileManager.fileSize("/health_history.bin");
    const int MAX_ENTRIES = 288;
    if (sz > (size_t)(MAX_ENTRIES * sizeof(HealthHistoryEntry))) {
        int total = sz / sizeof(HealthHistoryEntry);
        HealthHistoryEntry* buf = new HealthHistoryEntry[total];
        fileManager.read("/health_history.bin", (uint8_t*)buf, sz);

        int keep = MAX_ENTRIES;
        fileManager.write("/health_history.bin", (const uint8_t*)&buf[total - keep], keep * sizeof(HealthHistoryEntry));
        delete[] buf;
    }
}

int MAX30102Manager::getHistoryCount() const {
    if (!fileManager.exists("/health_history.bin")) return 0;
    size_t sz = fileManager.fileSize("/health_history.bin");
    return sz / sizeof(HealthHistoryEntry);
}

bool MAX30102Manager::getHistory(HealthHistoryEntry* buffer, int max_entries) const {
    if (!fileManager.exists("/health_history.bin")) return false;
    size_t sz = fileManager.fileSize("/health_history.bin");
    int total = sz / sizeof(HealthHistoryEntry);
    if (total <= 0) return false;

    int to_read = (total < max_entries) ? total : max_entries;
    HealthHistoryEntry* full_buf = new HealthHistoryEntry[total];
    fileManager.read("/health_history.bin", (uint8_t*)full_buf, sz);
    memcpy(buffer, &full_buf[total - to_read], to_read * sizeof(HealthHistoryEntry));
    delete[] full_buf;
    return true;
}
