#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include <Arduino.h>
#include <FastLED.h>

enum class LedMode {
    OFF,
    SOLID,
    BREATHING,
    PULSE,
    RAINBOW,
    COMPASS_SYNC,
    BOOT_PULSE,
    ERROR_FLASH,
    LOW_BATTERY_PULSE
};

class LedManager {
public:
    LedManager();
    void begin();
    void loop();

    void setMode(LedMode mode);
    void setColor(CRGB color);
    void setBrightness(uint8_t brightness);
    void triggerPulse(CRGB color, int count, int speed_ms);

    bool isMasterSwitchOn() const { return master_switch; }
    void setMasterSwitch(bool state);

    LedMode getMode() const { return current_mode; }
    uint8_t getBrightness() const { return current_brightness; }

    // Configs for the UI
    static const int LED_PRESET_COUNT = 7;
    CRGB preset_colors[LED_PRESET_COUNT];

private:
    CRGB leds[1];
    LedMode current_mode;
    LedMode previous_mode;
    CRGB current_color;
    uint8_t current_brightness;
    bool master_switch;

    // Animation state
    uint32_t last_update;
    float anim_phase;
    int pulse_count;
    int pulse_speed;
    CRGB pulse_color;

    void updateCompassSync();
};

extern LedManager ledManager;

#endif
