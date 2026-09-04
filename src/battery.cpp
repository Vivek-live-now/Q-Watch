#include "battery.h"
#include "hw_config.h"

BatteryMonitor battery;

void BatteryMonitor::begin() {
    // Configure ADC1
    // 12-bit resolution gives 0-4095
    analogReadResolution(12);

    // 11dB attenuation gives full scale reading up to ~3.1V
    analogSetAttenuation(ADC_11db);
    pinMode(BATTERY_ADC, INPUT);
}

float BatteryMonitor::readVoltage() {
    uint32_t mv_sum = 0;
    const int NUM_SAMPLES = 20;

    // Take multiple readings to average out ADC noise.
    // We use analogReadMilliVolts() which automatically applies the ESP32-S3's internal
    // factory eFuse calibration curves, satisfying the requirement to use proper calibration
    // instead of a raw mathematical multiplier.
    for (int i = 0; i < NUM_SAMPLES; i++) {
        mv_sum += analogReadMilliVolts(BATTERY_ADC);
        // Removed the delay(2) to keep this function strictly non-blocking.
        // 20 sequential ADC reads take microseconds.
    }

    uint32_t mv_avg = mv_sum / NUM_SAMPLES;

    // The voltage at the pin is exactly half the battery voltage due to the 100k/100k divider.
    // So we multiply the millivolt reading by 2.
    float actual_voltage = (mv_avg * 2.0f) / 1000.0f;

    // Apply the user's manual multimeter calibration multiplier
    return actual_voltage * CALIBRATION_MULTIPLIER;
}

int BatteryMonitor::readPercentage() {
    float v = readVoltage();

    // Approximate Piecewise Linear (PWL) Estimation for standard 3.7V/4.2V LiPo
    // NOTE: This is an approximation. A true fuel gauge requires coulomb counting.
    if (v >= 4.20) return 100;
    if (v >= 4.10) return 90;
    if (v >= 4.00) return 80;
    if (v >= 3.90) return 60;
    if (v >= 3.80) return 40;
    if (v >= 3.70) return 20;
    if (v >= 3.60) return 10;
    if (v >= 3.50) return 5;
    if (v <  3.50) return 0;

    return 0;
}
