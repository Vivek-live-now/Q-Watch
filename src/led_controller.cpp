#include "led_controller.h"

#if defined(ARDUINO)
#include "hw_config.h"
#include "battery.h"
#include "sensors.h"
#endif

LEDController ledController;

LEDController::LEDController() :
    enabled(true),
    brightness(128),
    mode(LEDMode::PRESET),
    current_preset(LEDPreset::BLUE),
    current_effect(LEDEffect::SOLID),
    current_status_effect(LEDStatusEffect::NONE),
    current_notification(LEDNotification::NONE),
    custom_color{0, 255, 255},
    active_color{0, 0, 255},
    last_update(0),
    notif_start_time(0),
    anim_step(0) {}

void LEDController::begin() {
#if defined(ARDUINO)
    pinMode(RGB_LED, OUTPUT);
#endif
    active_color = presetToRGB(current_preset);
    update();
}

void LEDController::setEnabled(bool state) {
    enabled = state;
    if (!enabled) {
        applyHardwareColor({0, 0, 0});
    } else {
        update();
    }
}

void LEDController::setBrightness(uint8_t b) {
    brightness = b;
    update();
}

void LEDController::setPreset(LEDPreset p) {
    current_preset = p;
    mode = LEDMode::PRESET;
    active_color = presetToRGB(p);
    update();
}

void LEDController::setCustomRGB(uint8_t r, uint8_t g, uint8_t b) {
    custom_color = {r, g, b};
    current_preset = LEDPreset::CUSTOM;
    mode = LEDMode::CUSTOM;
    active_color = custom_color;
    update();
}

void LEDController::setEffect(LEDEffect effect) {
    current_effect = effect;
    mode = LEDMode::EFFECT;
    anim_step = 0;
    update();
}

void LEDController::setStatusEffect(LEDStatusEffect statusEffect) {
    current_status_effect = statusEffect;
    mode = LEDMode::STATUS_REACTION;
    anim_step = 0;
    update();
}

void LEDController::triggerNotification(LEDNotification notif) {
    current_notification = notif;
    notif_start_time = millis();
    anim_step = 0;
    update();
}

RGBColor LEDController::presetToRGB(LEDPreset p) {
    switch (p) {
        case LEDPreset::RED:     return {255, 0, 0};
        case LEDPreset::GREEN:   return {0, 255, 0};
        case LEDPreset::BLUE:    return {0, 0, 255};
        case LEDPreset::PURPLE:  return {128, 0, 128};
        case LEDPreset::AMBER:   return {255, 120, 0};
        case LEDPreset::WHITE:   return {255, 255, 255};
        case LEDPreset::RAINBOW: return hsvToRGB((anim_step * 5) % 360, 255, 255);
        case LEDPreset::CUSTOM:  return custom_color;
        default: return {0, 0, 255};
    }
}

RGBColor LEDController::hsvToRGB(uint16_t h, uint8_t s, uint8_t v) {
    float r = 0, g = 0, b = 0;
    float h_f = h / 60.0f;
    float s_f = s / 255.0f;
    float v_f = v / 255.0f;

    int i = (int)h_f % 6;
    float f = h_f - (int)h_f;
    float p = v_f * (1.0f - s_f);
    float q = v_f * (1.0f - f * s_f);
    float t = v_f * (1.0f - (1.0f - f) * s_f);

    switch (i) {
        case 0: r = v_f; g = t; b = p; break;
        case 1: r = q; g = v_f; b = p; break;
        case 2: r = p; g = v_f; b = t; break;
        case 3: r = p; g = q; b = v_f; break;
        case 4: r = t; g = p; b = v_f; break;
        case 5: r = v_f; g = p; b = q; break;
    }
    return {(uint8_t)(r * 255), (uint8_t)(g * 255), (uint8_t)(b * 255)};
}

void LEDController::update() {
    if (!enabled) {
        applyHardwareColor({0, 0, 0});
        return;
    }

    uint32_t now = millis();
    anim_step++;

    // Handle Q-Branch Notification
    if (current_notification != LEDNotification::NONE) {
        uint32_t elapsed = now - notif_start_time;

        switch (current_notification) {
            case LEDNotification::BOOT: // Short blue pulse (500ms)
                if (elapsed < 500) {
                    float factor = sinf((elapsed / 500.0f) * 3.14159f);
                    active_color = {0, 0, (uint8_t)(255 * factor)};
                } else {
                    current_notification = LEDNotification::NONE;
                }
                break;

            case LEDNotification::SUCCESS: // Green double pulse (800ms)
                if (elapsed < 800) {
                    float factor = fabsf(sinf((elapsed / 400.0f) * 3.14159f));
                    active_color = {0, (uint8_t)(255 * factor), 0};
                } else {
                    current_notification = LEDNotification::NONE;
                }
                break;

            case LEDNotification::WARNING: // Amber pulse (600ms)
                if (elapsed < 600) {
                    float factor = sinf((elapsed / 600.0f) * 3.14159f);
                    active_color = {(uint8_t)(255 * factor), (uint8_t)(120 * factor), 0};
                } else {
                    current_notification = LEDNotification::NONE;
                }
                break;

            case LEDNotification::ERROR: // Red triple flash (900ms)
                if (elapsed < 900) {
                    bool on = ((elapsed / 150) % 2) == 0;
                    active_color = on ? RGBColor{255, 0, 0} : RGBColor{0, 0, 0};
                } else {
                    current_notification = LEDNotification::NONE;
                }
                break;

            case LEDNotification::LOW_BATTERY: // Slow red pulse (1200ms)
                if (elapsed < 1200) {
                    float factor = sinf((elapsed / 1200.0f) * 3.14159f);
                    active_color = {(uint8_t)(255 * factor), 0, 0};
                } else {
                    current_notification = LEDNotification::NONE;
                }
                break;

            default:
                current_notification = LEDNotification::NONE;
                break;
        }

        applyHardwareColor(active_color);
        return;
    }

    // Handle Watch Status Reactions
    if (mode == LEDMode::STATUS_REACTION) {
        switch (current_status_effect) {
            case LEDStatusEffect::BATTERY: {
#if defined(ARDUINO)
                int pct = battery.readPercentage();
#else
                int pct = 80;
#endif
                if (pct > 50) active_color = {0, 255, 0};      // Green
                else if (pct > 20) active_color = {255, 120, 0}; // Yellow/Amber
                else active_color = {255, 0, 0};               // Red
                break;
            }
            case LEDStatusEffect::SLEEP:
                active_color = {0, 0, 0};
                break;
            case LEDStatusEffect::ERROR: {
                bool flash = (now / 200) % 2 == 0;
                active_color = flash ? RGBColor{255, 0, 0} : RGBColor{0, 0, 0};
                break;
            }
            case LEDStatusEffect::HEALTH: { // Pulse heart-rate
                float pulse = (sinf(now / 150.0f) + 1.0f) / 2.0f;
                active_color = {(uint8_t)(255 * pulse), 0, (uint8_t)(50 * pulse)};
                break;
            }
            case LEDStatusEffect::IR: { // Brief flash
                active_color = {100, 0, 255};
                break;
            }
            case LEDStatusEffect::COMPASS: {
#if defined(ARDUINO)
                OrientationData o = sensors.getOrientation();
                uint16_t heading = (uint16_t)o.yaw % 360;
#else
                uint16_t heading = 180;
#endif
                active_color = hsvToRGB(heading, 255, 255);
                break;
            }
            default:
                active_color = {0, 0, 255};
                break;
        }
        applyHardwareColor(active_color);
        return;
    }

    // Handle Lighting Effects
    if (mode == LEDMode::EFFECT) {
        switch (current_effect) {
            case LEDEffect::SOLID:
                active_color = presetToRGB(current_preset);
                break;
            case LEDEffect::BREATHING: {
                float factor = (sinf(now / 500.0f) + 1.0f) / 2.0f;
                RGBColor base = presetToRGB(current_preset);
                active_color = {(uint8_t)(base.r * factor), (uint8_t)(base.g * factor), (uint8_t)(base.b * factor)};
                break;
            }
            case LEDEffect::PULSE: {
                float factor = (sinf(now / 200.0f) + 1.0f) / 2.0f;
                RGBColor base = presetToRGB(current_preset);
                active_color = {(uint8_t)(base.r * factor), (uint8_t)(base.g * factor), (uint8_t)(base.b * factor)};
                break;
            }
            case LEDEffect::FADE: {
                uint8_t factor = (now / 20) % 256;
                RGBColor base = presetToRGB(current_preset);
                active_color = {(uint8_t)((base.r * factor) / 255), (uint8_t)((base.g * factor) / 255), (uint8_t)((base.b * factor) / 255)};
                break;
            }
            case LEDEffect::RAINBOW:
            case LEDEffect::COLOR_CYCLE:
                active_color = hsvToRGB((now / 10) % 360, 255, 255);
                break;
            case LEDEffect::FLASH: {
                bool on = (now / 100) % 2 == 0;
                RGBColor base = presetToRGB(current_preset);
                active_color = on ? base : RGBColor{0, 0, 0};
                break;
            }
            case LEDEffect::HEARTBEAT_PULSE: {
                float factor = fabsf(sinf(now / 150.0f));
                RGBColor base = presetToRGB(current_preset);
                active_color = {(uint8_t)(base.r * factor), (uint8_t)(base.g * factor), (uint8_t)(base.b * factor)};
                break;
            }
        }
        applyHardwareColor(active_color);
        return;
    }

    // Default Preset / Custom
    active_color = presetToRGB(current_preset);
    applyHardwareColor(active_color);
}

void LEDController::applyHardwareColor(RGBColor col) {
#if defined(ARDUINO)
    // Scale by master brightness setting
    uint8_t r_scaled = (col.r * brightness) / 255;
    uint8_t g_scaled = (col.g * brightness) / 255;
    uint8_t b_scaled = (col.b * brightness) / 255;

    // Simple digital output driving built-in pin or PWM
    bool on = (r_scaled > 0 || g_scaled > 0 || b_scaled > 0);
    digitalWrite(RGB_LED, on ? HIGH : LOW);
#endif
}
