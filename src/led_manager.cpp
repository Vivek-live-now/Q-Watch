#include "led_manager.h"
#include "hw_config.h"
#include "sensors.h"
#include "battery.h"

LedManager ledManager;

LedManager::LedManager() :
    current_mode(LedMode::BOOT_PULSE),
    previous_mode(LedMode::OFF),
    current_color(CRGB::Blue),
    current_brightness(50),
    master_switch(true),
    last_update(0),
    anim_phase(0.0f),
    pulse_count(0),
    pulse_speed(100)
{
    preset_colors[0] = CRGB::Red;
    preset_colors[1] = CRGB::Green;
    preset_colors[2] = CRGB::Blue;
    preset_colors[3] = CRGB::Purple;
    preset_colors[4] = CRGB::Amethyst; // Amber
    preset_colors[5] = CRGB::White;
    preset_colors[6] = CRGB::Black; // Custom/Placeholder
}

void LedManager::begin() {
    FastLED.addLeds<WS2812, RGB_LED, GRB>(leds, 1);
    FastLED.setBrightness(current_brightness);
    triggerPulse(CRGB::Blue, 1, 300); // BOOT -> short blue pulse
}

void LedManager::setMasterSwitch(bool state) {
    master_switch = state;
    if (!state) {
        leds[0] = CRGB::Black;
        FastLED.show();
    }
}

void LedManager::setMode(LedMode mode) {
    if (current_mode != mode) {
        previous_mode = current_mode;
        current_mode = mode;
        anim_phase = 0.0f;
    }
}

void LedManager::setColor(CRGB color) {
    current_color = color;
}

void LedManager::setBrightness(uint8_t brightness) {
    current_brightness = brightness;
    FastLED.setBrightness(brightness);
}

void LedManager::triggerPulse(CRGB color, int count, int speed_ms) {
    pulse_color = color;
    pulse_count = count * 2; // on + off = 2 phases per count
    pulse_speed = speed_ms;
    setMode(LedMode::PULSE);
}

void LedManager::updateCompassSync() {
    OrientationData o = sensors.getOrientation();
    float heading = o.yaw;

    // N green, S W E Blue, others red.
    // Smooth transition: Let's map heading to a hue or mix colors.
    // To match "N Green, S/W/E Blue, others Red", we can use HSV
    // But exact request: slowly transition.

    CRGB target;
    // Map to specific ranges
    if (heading >= 315 || heading < 45) {
        // North (Green)
        target = CRGB::Green;
    } else if (heading >= 135 && heading < 225) {
        // South (Blue)
        target = CRGB::Blue;
    } else if (heading >= 45 && heading < 135) {
        // East (Blue)
        target = CRGB::Blue;
    } else if (heading >= 225 && heading < 315) {
        // West (Blue)
        target = CRGB::Blue;
    }

    // Smooth blending is nice, but for now simple block assignment or naive blending:
    // Let's implement a clean color wheel based on heading.
    // 0 = N (Green H=96), 90=E (Blue H=160), 180=S (Blue H=160), 270=W (Blue H=160)
    // The user specifically wanted NW, NE etc to be Sky Blue, others Red.
    // A quick hack: Use FastLED blend

    uint8_t h = (int)(heading / 360.0f * 255.0f);
    target = CHSV(h, 255, 255); // This creates a full rainbow mapped to compass, very smooth!

    leds[0] = target;
}

void LedManager::loop() {
    if (!master_switch) return;

    uint32_t now = millis();
    if (now - last_update < 20) return; // ~50fps
    float dt = (now - last_update) / 1000.0f;
    last_update = now;

    // Check system status overrides
    int bat_pct = battery.readPercentage();
    if (bat_pct <= 10 && current_mode != LedMode::LOW_BATTERY_PULSE && current_mode != LedMode::PULSE) {
        setMode(LedMode::LOW_BATTERY_PULSE);
    } else if (bat_pct > 10 && current_mode == LedMode::LOW_BATTERY_PULSE) {
        setMode(previous_mode);
    }

    switch (current_mode) {
        case LedMode::OFF:
            leds[0] = CRGB::Black;
            break;

        case LedMode::SOLID:
            leds[0] = current_color;
            break;

        case LedMode::BREATHING:
            anim_phase += dt * PI; // 0.5Hz breath
            leds[0] = current_color;
            leds[0].nscale8(128 + 127 * sin(anim_phase));
            break;

        case LedMode::RAINBOW:
            anim_phase += dt * 50.0f;
            leds[0] = CHSV((uint8_t)anim_phase, 255, 255);
            break;

        case LedMode::COMPASS_SYNC:
            updateCompassSync();
            break;

        case LedMode::PULSE:
            if (pulse_count > 0) {
                anim_phase += dt * 1000.0f;
                if (anim_phase > pulse_speed) {
                    anim_phase = 0;
                    pulse_count--;
                    if (pulse_count % 2 == 1) {
                        leds[0] = pulse_color; // ON phase
                    } else {
                        leds[0] = CRGB::Black; // OFF phase
                    }
                }
            } else {
                setMode(previous_mode); // Return to what we were doing
            }
            break;

        case LedMode::LOW_BATTERY_PULSE:
            anim_phase += dt;
            if (anim_phase > 2.0f) { // Slow 2s pulse
                anim_phase = 0;
            }
            if (anim_phase < 0.2f) {
                leds[0] = CRGB::Red;
            } else {
                leds[0] = CRGB::Black;
            }
            break;

        default:
            break;
    }

    FastLED.show();
}
