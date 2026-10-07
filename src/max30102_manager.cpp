#include "max30102_manager.h"
#include "file_manager.h"
#include "settings_data.h"
#include "hw_config.h"
#include "clock.h"
#include "simd_accel.h"
#include <Wire.h>

MAX30102Manager max30102Manager;

MAX30102Manager::MAX30102Manager() :
    sensor_ok(false),
    sensor_enabled(false),
    ppg_head(0),
    last_sample_time(0),
    last_temp_read_time(0),
    last_beat_time(0),
    prev_filtered_ac(0),
    cycle_peak(0),
    cycle_peak_time(0),
    pulse_rising(false),
    dynamic_p2p(100.0f),
    ppg_envelope(100.0f),
    bpm_history_cnt(0),
    bpm_history_idx(0),
    spo2_history_cnt(0),
    spo2_history_idx(0),
    cycle_ir_min(0xFFFFFFFF),
    cycle_ir_max(0),
    cycle_red_min(0xFFFFFFFF),
    cycle_red_max(0),
    cycle_ir_dc_sum(0),
    cycle_red_dc_sum(0),
    cycle_samples(0),
    ir_dc_filter(50000),
    red_dc_filter(50000) {
    memset(&current_metrics, 0, sizeof(HealthMetrics));
    memset(ppg_buffer, 128, sizeof(ppg_buffer));
    memset(bpm_history, 0, sizeof(bpm_history));
    memset(spo2_history, 0, sizeof(spo2_history));
}

void MAX30102Manager::begin() {
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000);
    Wire.setTimeOut(10);

    // Direct probe of 0x57 address
    Wire.beginTransmission(0x57);
    bool ack_57 = (Wire.endTransmission() == 0);

    if (particleSensor.begin(Wire, I2C_SPEED_FAST)) {
        sensor_ok = true;
        Serial.println("MAX30102 sensor detected at I2C address 0x57 (Fast 400kHz).");
    } else if (ack_57) {
        delay(10);
        if (particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
            sensor_ok = true;
            Serial.println("MAX30102 sensor detected at I2C address 0x57 (Standard 100kHz).");
        }
    } else {
        sensor_ok = false;
        Serial.println("MAX30102 sensor not found on I2C bus (0x57 NACK).");
    }

    // CRITICAL: Reassert custom ESP32 pins (Wire.begin(15, 16)) because SparkFun begin() calls _i2cPort->begin()
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000);
    Wire.setTimeOut(10);

    if (sensor_ok) {
        disableSensor();
    }
}

bool MAX30102Manager::retryInit() {
    begin();
    if (sensor_ok) {
        enableSensor();
        return true;
    }
    return false;
}

void MAX30102Manager::enableSensor() {
    if (!sensor_ok || sensor_enabled) return;

    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000);
    Wire.setTimeOut(10);

    particleSensor.wakeUp();

    // Configuration for prominent AC pulse & stable optical reading:
    // ledBrightness = 50 (~10mA): Optimal signal without photodiode saturation
    // sampleAverage = 4: 4x hardware oversampling
    // ledMode = 2: Dual Red + IR LEDs
    // sampleRate = 100: 100Hz / 4 = 25 sps effective rate
    // pulseWidth = 411: 18-bit ADC resolution
    // adcRange = 4096: 15.63 pA per LSB
    byte ledBrightness = 60;
    byte sampleAverage = 4;
    byte ledMode = 2;
    int sampleRate = 100;
    int pulseWidth = 411;
    int adcRange = 4096;

    particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
    particleSensor.clearFIFO();
    particleSensor.enableDIETEMPRDY();

    // Reassert custom pins
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000);
    Wire.setTimeOut(10);

    sensor_enabled = true;
    resetBeatState();
    readTemperature();
}

void MAX30102Manager::disableSensor() {
    if (!sensor_ok) return;
    particleSensor.shutDown();
    sensor_enabled = false;
    current_metrics.finger_detected = false;
    current_metrics.bpm = 0;
    current_metrics.spo2 = 0;
    resetBeatState();
}

void MAX30102Manager::resetBeatState() {
    last_beat_time = 0;
    prev_filtered_ac = 0;
    cycle_peak = 0;
    cycle_peak_time = 0;
    pulse_rising = false;
    dynamic_p2p = 100.0f;
    ppg_envelope = 100.0f;
    bpm_history_cnt = 0;
    bpm_history_idx = 0;
    spo2_history_cnt = 0;
    spo2_history_idx = 0;
    cycle_ir_min = 0xFFFFFFFF;
    cycle_ir_max = 0;
    cycle_red_min = 0xFFFFFFFF;
    cycle_red_max = 0;
    cycle_ir_dc_sum = 0;
    cycle_red_dc_sum = 0;
    cycle_samples = 0;
    ir_dc_filter = 50000;
    red_dc_filter = 50000;
    simd_max30102_fir_reset();
    memset(ppg_buffer, 128, sizeof(ppg_buffer));
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
    for (int i = 0; i < PPG_BUF_LEN; i++) {
        int idx = (ppg_head + i) % PPG_BUF_LEN;
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

        processSample(red, ir);
    }

    // Read temperature every 5 seconds
    if (millis() - last_temp_read_time >= 5000) {
        readTemperature();
        last_temp_read_time = millis();
    }
}

void MAX30102Manager::processSample(uint32_t red, uint32_t ir) {
    current_metrics.red_value = red;
    current_metrics.ir_value = ir;

    // Hysteresis finger detection:
    // Requires IR > 10000 to acquire; releases when IR drops below 7000.
    bool finger_present = current_metrics.finger_detected ? (ir > 7000) : (ir > 10000);

    if (!finger_present) {
        if (current_metrics.finger_detected) {
            current_metrics.finger_detected = false;
            current_metrics.bpm = 0;
            current_metrics.spo2 = 0;
            resetBeatState();
        }
        ppg_buffer[ppg_head] = 128;
        ppg_head = (ppg_head + 1) % PPG_BUF_LEN;
        prev_filtered_ac = 0;
        return;
    }

    current_metrics.finger_detected = true;

    // Slow baseline DC tracking (~2.5 sec time constant at 25Hz)
    if (ir_dc_filter == 50000 && ir > 7000) {
        ir_dc_filter = ir;
        red_dc_filter = red;
    } else {
        ir_dc_filter = (ir_dc_filter * 63 + ir) / 64;
        red_dc_filter = (red_dc_filter * 63 + red) / 64;
    }

    // Invert signal so systolic cardiac surge (light absorption increase) is POSITIVE
    int32_t ac_raw = (int32_t)ir_dc_filter - (int32_t)ir;

    // 32-tap SIMD FIR bandpass filter (0.5Hz - 4.0Hz: preserves 30 - 240 BPM cardiac pulses)
    int16_t filtered_ac = simd_max30102_fir_sample((int16_t)constrain(ac_raw, -32768, 32767));

    // Dynamic AGC envelope tracking for prominent OLED waveform
    float abs_val = (float)abs(filtered_ac);
    if (abs_val > ppg_envelope) {
        ppg_envelope = ppg_envelope * 0.90f + abs_val * 0.10f;
    } else {
        ppg_envelope = ppg_envelope * 0.992f + abs_val * 0.008f;
    }
    if (ppg_envelope < 20.0f) ppg_envelope = 20.0f;

    // Map filtered AC into 128 +/- 110 (range 18..238)
    float norm = (filtered_ac / ppg_envelope) * 110.0f;
    int graph_val = 128 + (int)norm;
    if (graph_val < 5) graph_val = 5;
    if (graph_val > 250) graph_val = 250;

    ppg_buffer[ppg_head] = (uint8_t)graph_val;
    ppg_head = (ppg_head + 1) % PPG_BUF_LEN;

    // Adaptive peak-to-peak tracking for beat threshold
    if (abs_val > dynamic_p2p) {
        dynamic_p2p = dynamic_p2p * 0.90f + abs_val * 0.10f;
    } else {
        dynamic_p2p = dynamic_p2p * 0.995f + abs_val * 0.005f;
    }
    if (dynamic_p2p < 25.0f) dynamic_p2p = 25.0f;

    int16_t threshold = (int16_t)(dynamic_p2p * 0.35f);
    if (threshold < 15) threshold = 15;

    // Track cycle min/max for SpO2 R-ratio calculation
    if (ir < cycle_ir_min) cycle_ir_min = ir;
    if (ir > cycle_ir_max) cycle_ir_max = ir;
    if (red < cycle_red_min) cycle_red_min = red;
    if (red > cycle_red_max) cycle_red_max = red;
    cycle_ir_dc_sum += ir;
    cycle_red_dc_sum += red;
    cycle_samples++;

    // Real-Time Beat Detection & Inter-Beat Interval (IBI) Tracking
    uint32_t now = millis();

    if (!pulse_rising && prev_filtered_ac <= threshold && filtered_ac > threshold) {
        pulse_rising = true;
        cycle_peak = filtered_ac;
        cycle_peak_time = now;
    } else if (pulse_rising) {
        if (filtered_ac > cycle_peak) {
            cycle_peak = filtered_ac;
            cycle_peak_time = now;
        }

        // Peak confirmed when signal drops below 60% of peak or falls below threshold
        if (filtered_ac < (cycle_peak * 60) / 100 || filtered_ac < threshold) {
            pulse_rising = false;

            if (last_beat_time > 0) {
                uint32_t delta_ms = cycle_peak_time - last_beat_time;
                // Physiological human heart rate: 40 BPM (1500ms) to 200 BPM (300ms)
                // Refractory period: Ignore reflections / dicrotic notch within 300ms (delta_ms < 300 rejected)
                if (delta_ms >= 300 && delta_ms <= 1600) {
                    float instant_bpm = 60000.0f / (float)delta_ms;

                    if (bpm_history_cnt == 0) {
                        bpm_history[0] = instant_bpm;
                        bpm_history_cnt = 1;
                        bpm_history_idx = 1;
                    } else {
                        bpm_history[bpm_history_idx] = instant_bpm;
                        bpm_history_idx = (bpm_history_idx + 1) % BEAT_HIST_SIZE;
                        if (bpm_history_cnt < BEAT_HIST_SIZE) bpm_history_cnt++;
                    }

                    float final_bpm = 0;
                    for (int k = 0; k < bpm_history_cnt; k++) final_bpm += bpm_history[k];
                    final_bpm /= bpm_history_cnt;
                    current_metrics.bpm = (int)(final_bpm + 0.5f);
                    last_beat_time = cycle_peak_time;

                    // SpO2 calculation for completed beat cycle
                    if (cycle_samples >= 8) {
                        uint32_t ir_ac = (cycle_ir_max > cycle_ir_min) ? (cycle_ir_max - cycle_ir_min) : 0;
                        uint32_t red_ac = (cycle_red_max > cycle_red_min) ? (cycle_red_max - cycle_red_min) : 0;
                        uint32_t ir_dc = (uint32_t)(cycle_ir_dc_sum / cycle_samples);
                        uint32_t red_dc = (uint32_t)(cycle_red_dc_sum / cycle_samples);

                        if (ir_ac >= 15 && red_ac >= 15 && ir_dc > 7000 && red_dc > 7000) {
                            float r_red = (float)red_ac / (float)red_dc;
                            float r_ir = (float)ir_ac / (float)ir_dc;
                            if (r_ir > 0.00001f) {
                                float R = r_red / r_ir;
                                float calc_spo2 = -45.060f * (R * R) + 30.354f * R + 94.845f;
                                if (calc_spo2 >= 70.0f && calc_spo2 <= 100.0f) {
                                    if (spo2_history_cnt == 0) {
                                        spo2_history[0] = calc_spo2;
                                        spo2_history_cnt = 1;
                                        spo2_history_idx = 1;
                                    } else {
                                        spo2_history[spo2_history_idx] = calc_spo2;
                                        spo2_history_idx = (spo2_history_idx + 1) % BEAT_HIST_SIZE;
                                        if (spo2_history_cnt < BEAT_HIST_SIZE) spo2_history_cnt++;
                                    }

                                    float final_spo2 = 0;
                                    for (int k = 0; k < spo2_history_cnt; k++) final_spo2 += spo2_history[k];
                                    final_spo2 /= spo2_history_cnt;
                                    current_metrics.spo2 = (int)(final_spo2 + 0.5f);
                                }
                            }
                        }
                    }
                } else if (delta_ms > 1600) {
                    bpm_history_cnt = 0;
                    bpm_history_idx = 0;
                    last_beat_time = cycle_peak_time;
                }
            } else {
                last_beat_time = cycle_peak_time;
            }

            // Reset cycle accumulators for next beat
            cycle_ir_min = 0xFFFFFFFF;
            cycle_ir_max = 0;
            cycle_red_min = 0xFFFFFFFF;
            cycle_red_max = 0;
            cycle_ir_dc_sum = 0;
            cycle_red_dc_sum = 0;
            cycle_samples = 0;
        }
    }

    // Signal timeout: If no valid beat detected for 2.5 seconds, clear stale metrics
    if (last_beat_time > 0 && (now - last_beat_time > 2500)) {
        current_metrics.bpm = 0;
        bpm_history_cnt = 0;
    }

    prev_filtered_ac = filtered_ac;
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

    // Batched Pruning: Allow hysteresis up to MAX_ENTRIES + 32 before trimming.
    // This avoids rewriting the entire file on every 5-minute sample (reduces flash wear by 97%).
    size_t sz = fileManager.fileSize("/health_history.bin");
    const int MAX_ENTRIES = 288;
    const int BATCH_TRIM = 32;
    if (sz >= (size_t)((MAX_ENTRIES + BATCH_TRIM) * sizeof(HealthHistoryEntry))) {
        int total = sz / sizeof(HealthHistoryEntry);
        int keep = MAX_ENTRIES;
        HealthHistoryEntry* buf = new HealthHistoryEntry[keep];
        size_t offset = (total - keep) * sizeof(HealthHistoryEntry);
        if (fileManager.readSeek("/health_history.bin", offset, (uint8_t*)buf, keep * sizeof(HealthHistoryEntry)) == keep * sizeof(HealthHistoryEntry)) {
            fileManager.write("/health_history.bin", (const uint8_t*)buf, keep * sizeof(HealthHistoryEntry));
        }
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
    size_t offset_bytes = (total - to_read) * sizeof(HealthHistoryEntry);

    // Direct seek read of only the requested tail - zero temporary heap buffer
    size_t bytes_read = fileManager.readSeek("/health_history.bin", offset_bytes, (uint8_t*)buffer, to_read * sizeof(HealthHistoryEntry));
    return (bytes_read == to_read * sizeof(HealthHistoryEntry));
}
