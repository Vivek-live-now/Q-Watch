#ifndef BATTERY_H
#define BATTERY_H

#include <Arduino.h>

class BatteryMonitor {
public:
    void begin();

    // Reads and averages the ADC using hardware calibration, returning the actual physical battery voltage
    float readVoltage();

    // Returns an estimated 0-100% value based on standard LiPo discharge curves
    int readPercentage();

private:
    // This value is calibrated by comparing the reported voltage to a physical multimeter measurement
    // Multiplier = Multimeter_Reading / Reported_Voltage_Without_Calibration
    // Currently set to 1.0. The user must manually determine this on their hardware.
    const float CALIBRATION_MULTIPLIER = 1.0f;
};

extern BatteryMonitor battery;

#endif
