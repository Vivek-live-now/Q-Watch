#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#if defined(ARDUINO)
#include <Arduino.h>
#else
#include <cstdint>
#endif

enum class LEDMode {
    OFF,
    SOLID,
    PRESET,
    CUSTOM,
    EFFECT,
    STATUS_REACTION,
    NOTIFICATION
};

enum class LEDPreset {
    RED,
    GREEN,
    BLUE,
    PURPLE,
    AMBER,
    WHITE,
    RAINBOW,
    CUSTOM
};

enum class LEDEffect {
    SOLID,
    BREATHING,
    PULSE,
    FADE,
    RAINBOW,
    COLOR_CYCLE,
    FLASH,
    HEARTBEAT_PULSE
};

enum class LEDStatusEffect {
    NONE,
    COMPASS,
    BATTERY,
    HEALTH,
    VOICE,
    IR,
    SLEEP,
    ERROR
};

enum class LEDNotification {
    NONE,
    BOOT,
    SUCCESS,
    WARNING,
    ERROR,
    LOW_BATTERY
};

struct RGBColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

class LEDController {
public:
    LEDController();
    void begin();
    void update();

    // On/Off Master Toggle
    void setEnabled(bool state);
    bool isEnabled() const { return enabled; }

    // Brightness Control (0-255)
    void setBrightness(uint8_t b);
    uint8_t getBrightness() const { return brightness; }

    // Preset & Color
    void setPreset(LEDPreset p);
    LEDPreset getPreset() const { return current_preset; }

    void setCustomRGB(uint8_t r, uint8_t g, uint8_t b);
    RGBColor getCustomRGB() const { return custom_color; }

    // Effects
    void setEffect(LEDEffect effect);
    LEDEffect getEffect() const { return current_effect; }

    // Watch Status Reaction
    void setStatusEffect(LEDStatusEffect statusEffect);
    LEDStatusEffect getStatusEffect() const { return current_status_effect; }

    // Q-Branch Notification Signal Trigger
    void triggerNotification(LEDNotification notif);
    LEDNotification getActiveNotification() const { return current_notification; }

    RGBColor getCurrentColor() const { return active_color; }

private:
    bool enabled;
    uint8_t brightness;

    LEDMode mode;
    LEDPreset current_preset;
    LEDEffect current_effect;
    LEDStatusEffect current_status_effect;
    LEDNotification current_notification;

    RGBColor custom_color;
    RGBColor active_color;

    uint32_t last_update;
    uint32_t notif_start_time;
    uint16_t anim_step;

    RGBColor presetToRGB(LEDPreset p);
    RGBColor hsvToRGB(uint16_t h, uint8_t s, uint8_t v);
    void applyHardwareColor(RGBColor col);
};

extern LEDController ledController;

#endif
