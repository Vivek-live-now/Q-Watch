#include "max30102_health.h"
#include "file_manager.h"
#include "settings_data.h"
#include <Wire.h>
#include <time.h>
#include <math.h>

Max30102Health healthManager;

static const char* HEALTH_HISTORY_FILE = "/health_history.bin";
static const uint8_t MAX30102_PART_ID = 0x15;
static const uint32_t SAMPLE_INTERVAL_MS = 10;

Max30102Health::Max30102Health()
    : sensor_ok(false), live_active(false), page(0),
      history_index(0), history_count(0), window_count(0),
      dc_ir(0.0f), dc_red(0.0f), prev_ac(0.0f),
      last_peak_ms(0), last_temp_ms(0), bpm_sum(0.0f), bpm_samples(0) {
    memset(&live, 0, sizeof(live));
    memset(ir_history, 0, sizeof(ir_history));
    memset(red_history, 0, sizeof(red_history));
    memset(ir_window, 0, sizeof(ir_window));
    memset(red_window, 0, sizeof(red_window));
}

bool Max30102Health::begin() {
    Wire.beginTransmission(0x57);
    if (Wire.endTransmission() != 0) {
        sensor_ok = false;
        return false;
    }

    if (!sensor.begin(Wire, I2C_SPEED_FAST, 0x57)) {
        sensor_ok = false;
        return false;
    }

    if (sensor.readPartID() != MAX30102_PART_ID) {
        sensor_ok = false;
        return false;
    }

    configureSensor();
    sensor.shutDown();
    sensor_ok = true;
    return true;
}

void Max30102Health::configureSensor() {
    // Conservative starting point for a wearable: RED+IR, 100 sps,
    // 4-sample averaging, 18-bit conversion, moderate LED current.
    sensor.setup(0x3F, 4, 2, 100, 411, 4096);
    sensor.setPulseAmplitudeGreen(0);
    sensor.enableDIETEMPRDY();
    sensor.clearFIFO();
}

void Max30102Health::startLive() {
    if (!sensor_ok) begin();
    if (!sensor_ok) return;

    configureSensor();
    sensor.wakeUp();

    live_active = true;
    page = 0;
    history_index = 0;
    history_count = 0;
    window_count = 0;
    dc_ir = 0.0f;
    dc_red = 0.0f;
    prev_ac = 0.0f;
    last_peak_ms = 0;
    bpm_sum = 0.0f;
    bpm_samples = 0;
    memset(ir_history, 0, sizeof(ir_history));
    memset(red_history, 0, sizeof(red_history));
    memset(ir_window, 0, sizeof(ir_window));
    memset(red_window, 0, sizeof(red_window));
    memset(&live, 0, sizeof(live));
}

void Max30102Health::stopLive() {
    if (sensor_ok) sensor.shutDown();
    live_active = false;
}

void Max30102Health::setPage(int p) {
    page = (p <= 0) ? 0 : 1;
}

void Max30102Health::nextPage() {
    if (page < 1) page++;
}

void Max30102Health::previousPage() {
    if (page > 0) page--;
}

bool Max30102Health::hasFinger(uint32_t ir) const {
    // This is intentionally only a contact gate, not a physiological threshold.
    // The value will need hardware testing with the actual breakout and placement.
    return ir > 10000;
}

uint8_t Max30102Health::calculateQuality(uint32_t ir, uint32_t red) const {
    if (!hasFinger(ir)) return 0;

    float ac = fabsf((float)ir - dc_ir);
    if (ac < 150.0f) return 1;
    if (ac < 500.0f) return 2;
    if (ac < 1000.0f) return 3;
    return 4;
}

void Max30102Health::processSample(uint32_t ir, uint32_t red) {
    live.ir = ir;
    live.red = red;
    live.finger_detected = hasFinger(ir);

    ir_history[history_index] = ir;
    red_history[history_index] = red;
    history_index = (history_index + 1) % LIVE_POINTS;
    if (history_count < LIVE_POINTS) history_count++;

    if (dc_ir == 0.0f) {
        dc_ir = (float)ir;
        dc_red = (float)red;
    } else {
        dc_ir = dc_ir * 0.98f + (float)ir * 0.02f;
        dc_red = dc_red * 0.98f + (float)red * 0.02f;
    }

    float ac = (float)ir - dc_ir;
    uint32_t now = millis();

    if (live.finger_detected && prev_ac <= 0.0f && ac > 0.0f) {
        // Zero crossing alone is too sensitive, so only accept it when the
        // waveform has enough amplitude and has a physiological refractory time.
        if (fabsf(ac) > 250.0f && (last_peak_ms == 0 || now - last_peak_ms >= 300)) {
            if (last_peak_ms != 0) {
                uint32_t interval = now - last_peak_ms;
                if (interval >= 300 && interval <= 2000) {
                    float bpm = 60000.0f / (float)interval;
                    if (bpm >= 30.0f && bpm <= 220.0f) {
                        bpm_sum += bpm;
                        if (bpm_samples < 8) bpm_samples++;
                        live.bpm = (int)lroundf(bpm_sum / (float)bpm_samples);
                        live.bpm_valid = true;
                    }
                }
            }
            last_peak_ms = now;
        }
    }

    prev_ac = ac;

    if (window_count < SPO2_WINDOW) {
        ir_window[window_count] = ir;
        red_window[window_count] = red;
        window_count++;
    } else {
        memmove(ir_window, ir_window + 1, (SPO2_WINDOW - 1) * sizeof(uint32_t));
        memmove(red_window, red_window + 1, (SPO2_WINDOW - 1) * sizeof(uint32_t));
        ir_window[SPO2_WINDOW - 1] = ir;
        red_window[SPO2_WINDOW - 1] = red;
    }

    live.signal_quality = calculateQuality(ir, red);

    if (window_count >= SPO2_WINDOW) {
        calculateSpO2();
    }

    if (now - last_temp_ms >= 1000) {
        float t = sensor.readTemperature();
        if (t > -40.0f && t < 85.0f) live.sensor_temp = t;
        last_temp_ms = now;
    }
}

void Max30102Health::calculateSpO2() {
    if (!live.finger_detected || live.signal_quality < 2) {
        live.spo2_valid = false;
        return;
    }

    double ir_mean = 0.0;
    double red_mean = 0.0;
    for (int i = 0; i < SPO2_WINDOW; i++) {
        ir_mean += ir_window[i];
        red_mean += red_window[i];
    }
    ir_mean /= SPO2_WINDOW;
    red_mean /= SPO2_WINDOW;

    if (ir_mean < 1.0 || red_mean < 1.0) {
        live.spo2_valid = false;
        return;
    }

    double ir_sq = 0.0;
    double red_sq = 0.0;
    for (int i = 0; i < SPO2_WINDOW; i++) {
        double ir_ac = (double)ir_window[i] - ir_mean;
        double red_ac = (double)red_window[i] - red_mean;
        ir_sq += ir_ac * ir_ac;
        red_sq += red_ac * red_ac;
    }

    double ir_rms = sqrt(ir_sq / SPO2_WINDOW);
    double red_rms = sqrt(red_sq / SPO2_WINDOW);

    if (ir_rms < 1.0 || red_rms < 1.0) {
        live.spo2_valid = false;
        return;
    }

    double ratio = (red_rms / red_mean) / (ir_rms / ir_mean);
    int value = (int)lround(110.0 - 25.0 * ratio);

    if (value >= 70 && value <= 100) {
        live.spo2 = value;
        live.spo2_valid = true;
    } else {
        live.spo2_valid = false;
    }
}

void Max30102Health::updateMetrics() {
    // Reserved for future filtering improvements without changing the UI API.
}

void Max30102Health::loop() {
    if (!live_active || !sensor_ok) return;

    sensor.check();
    while (sensor.available()) {
        uint32_t red = sensor.getFIFORed();
        uint32_t ir = sensor.getFIFOIR();
        sensor.nextSample();
        processSample(ir, red);
    }
}

int64_t Max30102Health::nowEpoch() const {
    time_t now = time(nullptr);
    if (now > 1700000000) return (int64_t)now;
    return (int64_t)(millis() / 1000);
}

int64_t Max30102Health::startOfToday() const {
    time_t now = time(nullptr);
    if (now <= 1700000000) return 0;
    struct tm local_tm;
    localtime_r(&now, &local_tm);
    local_tm.tm_hour = 0;
    local_tm.tm_min = 0;
    local_tm.tm_sec = 0;
    return (int64_t)mktime(&local_tm);
}

bool Max30102Health::appendHistory(const HealthHistoryEntry& entry) {
    if (!fileManager.append(HEALTH_HISTORY_FILE,
                            (const uint8_t*)&entry,
                            sizeof(entry))) {
        return false;
    }

    const size_t max_entries = 288;
    size_t size = fileManager.fileSize(HEALTH_HISTORY_FILE);
    if (size <= max_entries * sizeof(HealthHistoryEntry)) return true;

    size_t total = size / sizeof(HealthHistoryEntry);
    HealthHistoryEntry* all = new HealthHistoryEntry[total];
    if (!all) return false;

    if (!fileManager.read(HEALTH_HISTORY_FILE,
                          (uint8_t*)all,
                          size)) {
        delete[] all;
        return false;
    }

    fileManager.write(HEALTH_HISTORY_FILE,
                      (const uint8_t*)&all[total - max_entries],
                      max_entries * sizeof(HealthHistoryEntry));
    delete[] all;
    return true;
}

bool Max30102Health::logBackgroundSample() {
    if (!sensor_ok && !begin()) return false;

    configureSensor();
    sensor.wakeUp();
    sensor.clearFIFO();

    const uint32_t start = millis();
    const uint32_t duration = 12000;
    uint32_t sumBpm = 0;
    uint32_t sumSpo2 = 0;
    uint32_t sumTemp = 0;
    uint16_t validBpm = 0;
    uint16_t validSpo2 = 0;
    uint16_t validTemp = 0;
    uint8_t bestQuality = 0;

    // Reuse the live processing pipeline for a short, bounded snapshot.
    live_active = true;
    page = 0;
    history_index = 0;
    history_count = 0;
    window_count = 0;
    dc_ir = 0.0f;
    dc_red = 0.0f;
    prev_ac = 0.0f;
    last_peak_ms = 0;
    bpm_sum = 0.0f;
    bpm_samples = 0;
    memset(&live, 0, sizeof(live));

    while (millis() - start < duration) {
        loop();
        if (live.bpm_valid) {
            sumBpm += live.bpm;
            validBpm++;
        }
        if (live.spo2_valid) {
            sumSpo2 += live.spo2;
            validSpo2++;
        }
        if (live.sensor_temp > -40.0f && live.sensor_temp < 85.0f) {
            sumTemp += (uint32_t)lroundf(live.sensor_temp * 10.0f);
            validTemp++;
        }
        if (live.signal_quality > bestQuality) bestQuality = live.signal_quality;
        delay(1);
    }

    HealthHistoryEntry entry{};
    entry.timestamp = nowEpoch();
    entry.bpm = validBpm ? (int16_t)(sumBpm / validBpm) : -1;
    entry.spo2 = validSpo2 ? (int16_t)(sumSpo2 / validSpo2) : -1;
    entry.temp_x10 = validTemp ? (int16_t)(sumTemp / validTemp) : -32768;
    entry.quality = bestQuality;
    entry.valid = (entry.bpm >= 0 ? 1 : 0) | (entry.spo2 >= 0 ? 2 : 0);

    sensor.shutDown();
    live_active = false;

    return appendHistory(entry);
}

int Max30102Health::readHistory(HealthHistoryEntry* buffer, int max_entries) const {
    if (!buffer || max_entries <= 0 || !fileManager.exists(HEALTH_HISTORY_FILE)) return 0;

    size_t size = fileManager.fileSize(HEALTH_HISTORY_FILE);
    int total = (int)(size / sizeof(HealthHistoryEntry));
    if (total <= 0) return 0;

    int count = total < max_entries ? total : max_entries;
    HealthHistoryEntry* all = new HealthHistoryEntry[total];
    if (!all) return 0;

    if (!fileManager.read(HEALTH_HISTORY_FILE, (uint8_t*)all, size)) {
        delete[] all;
        return 0;
    }

    memcpy(buffer, &all[total - count], count * sizeof(HealthHistoryEntry));
    delete[] all;
    return count;
}

int Max30102Health::getTodayHistory(HealthHistoryEntry* buffer, int max_entries) const {
    int count = readHistory(buffer, max_entries);
    if (count <= 0) return 0;

    int64_t day = startOfToday();
    if (day == 0) return count;

    int write = 0;
    for (int i = 0; i < count; i++) {
        if (buffer[i].timestamp >= day) buffer[write++] = buffer[i];
    }
    return write;
}

int Max30102Health::getLiveWaveform(uint32_t* buffer, int max_points) const {
    if (!buffer || max_points <= 0) return 0;
    int count = history_count < max_points ? history_count : max_points;
    for (int i = 0; i < count; i++) {
        int src = (history_index - count + i + LIVE_POINTS) % LIVE_POINTS;
        buffer[i] = ir_history[src];
    }
    return count;
}

int Max30102Health::getTodayHistoryCount() const {
    HealthHistoryEntry tmp[288];
    int count = readHistory(tmp, 288);
    if (count <= 0) return 0;

    int64_t day = startOfToday();
    if (day == 0) return count;

    int today = 0;
    for (int i = 0; i < count; i++) {
        if (tmp[i].timestamp >= day) today++;
    }
    return today;
}
