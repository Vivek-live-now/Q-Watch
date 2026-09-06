#include <iostream>
#include <cassert>
#include <vector>
#include <cmath>

#include "../include/battery.h"

// Mock BatteryMonitor where readVoltage returns a mock value
class MockBatteryMonitor : public BatteryMonitor {
public:
    float mockVoltage = 0.0f;

    void setVoltage(float v) {
        mockVoltage = v;
    }

    float readVoltage() override {
        return mockVoltage;
    }
};

struct TestCase {
    float voltage;
    int expectedPercentage;
    const char* description;
};

int main() {
    MockBatteryMonitor mockBattery;

    std::vector<TestCase> testCases = {
        // Upper boundary and above
        {4.50f, 100, "Overcharged / USB plugged (4.50V)"},
        {4.20f, 100, "Exact 100% threshold (4.20V)"},
        {4.21f, 100, "Slightly above 100% threshold (4.21V)"},

        // 90% boundary
        {4.19f, 90, "Slightly below 4.20V (4.19V)"},
        {4.10f, 90, "Exact 90% threshold (4.10V)"},

        // 80% boundary
        {4.09f, 80, "Slightly below 4.10V (4.09V)"},
        {4.00f, 80, "Exact 80% threshold (4.00V)"},

        // 60% boundary
        {3.99f, 60, "Slightly below 4.00V (3.99V)"},
        {3.90f, 60, "Exact 60% threshold (3.90V)"},

        // 40% boundary
        {3.89f, 40, "Slightly below 3.90V (3.89V)"},
        {3.80f, 40, "Exact 40% threshold (3.80V)"},

        // 20% boundary
        {3.79f, 20, "Slightly below 3.80V (3.79V)"},
        {3.70f, 20, "Exact 20% threshold (3.70V)"},

        // 10% boundary
        {3.69f, 10, "Slightly below 3.70V (3.69V)"},
        {3.60f, 10, "Exact 10% threshold (3.60V)"},

        // 5% boundary
        {3.59f, 5, "Slightly below 3.60V (3.59V)"},
        {3.50f, 5, "Exact 5% threshold (3.50V)"},

        // 0% boundary and below
        {3.49f, 0, "Slightly below 3.50V (3.49V)"},
        {3.00f, 0, "Deeply discharged battery (3.00V)"},
        {0.00f, 0, "Zero voltage (0.00V)"},
        {-1.00f, 0, "Negative voltage fault (-1.00V)"}
    };

    int passed = 0;
    int failed = 0;

    std::cout << "=========================================" << std::endl;
    std::cout << " Running BatteryMonitor::readPercentage Tests" << std::endl;
    std::cout << "=========================================" << std::endl;

    for (const auto& tc : testCases) {
        mockBattery.setVoltage(tc.voltage);
        int result = mockBattery.readPercentage();

        if (result == tc.expectedPercentage) {
            std::cout << "[PASS] " << tc.description
                      << " -> Voltage: " << tc.voltage << "V, Expected: "
                      << tc.expectedPercentage << "%, Got: " << result << "%" << std::endl;
            passed++;
        } else {
            std::cerr << "[FAIL] " << tc.description
                      << " -> Voltage: " << tc.voltage << "V, Expected: "
                      << tc.expectedPercentage << "%, Got: " << result << "%" << std::endl;
            failed++;
        }
    }

    std::cout << "=========================================" << std::endl;
    std::cout << " Summary: " << passed << " passed, " << failed << " failed." << std::endl;
    std::cout << "=========================================" << std::endl;

    return failed == 0 ? 0 : 1;
}
