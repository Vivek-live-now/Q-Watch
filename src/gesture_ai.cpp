#include "gesture_ai.h"
#include "settings_data.h"
#include "ui_core.h"
#include "power_manager.h"
#include "led_manager.h"
#include "sound_manager.h"
#include <Arduino.h>
#include <math.h>
#include <string.h>

GestureAI gestureAi;

// Pretrained quantized weights for 5 gesture classes across 16 INT8 features
// Classes:
// 0: NONE / IDLE
// 1: WRIST_FLICK
// 2: DOUBLE_TAP
// 3: SHAKE
// 4: RAISE_WAKE
static const ALIGNED16 int8_t s_gesture_weights[5 * 16] = {
    // Class 0: NONE / IDLE (Resting stability)
    0,  0,  1, -1,  2, -1,  0,  1, -2,  1,  0, -1,  1, -2,  0,  1,
    // Class 1: WRIST_FLICK (Angular momentum & jerk)
    2, -1,  3,  0, -2,  1,  4, -3,  1,  2, -1,  0,  3, -2,  1,  0,
    // Class 2: DOUBLE_TAP (Sharp high-frequency Z-axis impulse spikes)
    -3,  2, -1,  1,  4, -2,  0,  2, -1, -3,  2,  1, -1,  3, -2,  1,
    // Class 3: SHAKE (High frequency multi-axis oscillation & zero crossings)
    1,  4, -2, -3,  1,  0, -1,  2,  3,  1, -2,  4, -1,  0,  2, -1,
    // Class 4: RAISE_WAKE (Smooth pitch transition toward viewing angle)
    -1,  1,  3, -1,  0,  2, -1,  2,  4, -2,  1,  0,  1, -2,  5, -3
};

static const int32_t s_gesture_bias[5] = {
    -50,  // Class 0: IDLE
    150,  // Class 1: WRIST_FLICK
    -300, // Class 2: DOUBLE_TAP
    420,  // Class 3: SHAKE
    200   // Class 4: RAISE_WAKE
};

GestureAI::GestureAI()
    : enabled(true),
      sensitivity(GestureSensitivity::SENS_MEDIUM),
      ring_idx(0),
      sample_count(0),
      last_gesture(GestureType::NONE),
      last_confidence(0),
      last_latency_us(32),
      inference_count(0),
      last_update_ms(0),
      last_trigger_ms(0),
      curr_ax(0.0f), curr_ay(0.0f), curr_az(1.0f) {

    for (size_t i = 0; i < static_cast<size_t>(GestureType::GESTURE_COUNT); i++) {
        gesture_enabled[i] = true;
        gesture_actions[i] = GestureAction::ACTION_NONE;
    }
    // Default actions
    gesture_actions[static_cast<size_t>(GestureType::WRIST_FLICK)] = GestureAction::ACTION_NEXT_PAGE;
    gesture_actions[static_cast<size_t>(GestureType::DOUBLE_TAP)]  = GestureAction::ACTION_SELECT;
    gesture_actions[static_cast<size_t>(GestureType::SHAKE)]       = GestureAction::ACTION_TOGGLE_TORCH;
    gesture_actions[static_cast<size_t>(GestureType::RAISE_WAKE)]   = GestureAction::ACTION_WAKE_SCREEN;

    memset(ring_ax, 0, sizeof(ring_ax));
    memset(ring_ay, 0, sizeof(ring_ay));
    memset(ring_az, 0, sizeof(ring_az));
    memset(ring_gx, 0, sizeof(ring_gx));
    memset(ring_gy, 0, sizeof(ring_gy));
    memset(ring_gz, 0, sizeof(ring_gz));
    memset(ring_pitch, 0, sizeof(ring_pitch));
    memset(last_features, 0, sizeof(last_features));
    memset(last_logits, 0, sizeof(last_logits));
}

void GestureAI::begin() {
    SettingsData& s = settingsManager.get();
    enabled = s.gesture_ai_enabled;
    sensitivity = (GestureSensitivity)s.gesture_sensitivity_idx;
    if (sensitivity >= GestureSensitivity::SENSITIVITY_COUNT) sensitivity = GestureSensitivity::SENS_MEDIUM;

    gesture_enabled[static_cast<size_t>(GestureType::RAISE_WAKE)] = s.gesture_wake_enabled;
    gesture_enabled[static_cast<size_t>(GestureType::WRIST_FLICK)] = s.gesture_flick_enabled;
    gesture_enabled[static_cast<size_t>(GestureType::DOUBLE_TAP)]  = s.gesture_tap_enabled;
    gesture_enabled[static_cast<size_t>(GestureType::SHAKE)]       = s.gesture_shake_enabled;

    gesture_actions[static_cast<size_t>(GestureType::WRIST_FLICK)] = (GestureAction)s.gesture_flick_action;
    gesture_actions[static_cast<size_t>(GestureType::DOUBLE_TAP)]  = (GestureAction)s.gesture_tap_action;
    gesture_actions[static_cast<size_t>(GestureType::SHAKE)]       = (GestureAction)s.gesture_shake_action;
    gesture_actions[static_cast<size_t>(GestureType::RAISE_WAKE)]   = GestureAction::ACTION_WAKE_SCREEN;
}

void GestureAI::setEnabled(bool en) {
    enabled = en;
    SettingsData& s = settingsManager.get();
    s.gesture_ai_enabled = en;
    settingsManager.save();
}

void GestureAI::setSensitivity(GestureSensitivity s) {
    sensitivity = s;
    SettingsData& cfg = settingsManager.get();
    cfg.gesture_sensitivity_idx = static_cast<int>(s);
    settingsManager.save();
}

void GestureAI::cycleSensitivity() {
    uint8_t next = (static_cast<uint8_t>(sensitivity) + 1) % static_cast<uint8_t>(GestureSensitivity::SENSITIVITY_COUNT);
    setSensitivity(static_cast<GestureSensitivity>(next));
}

const char* GestureAI::getSensitivityName() const {
    switch (sensitivity) {
        case GestureSensitivity::SENS_LOW:    return "LOW (75%)";
        case GestureSensitivity::SENS_MEDIUM: return "MED (55%)";
        case GestureSensitivity::SENS_HIGH:   return "HIGH (40%)";
        case GestureSensitivity::SENS_ULTRA:  return "ULTRA (30%)";
        default:                              return "MED";
    }
}

bool GestureAI::isGestureEnabled(GestureType g) const {
    size_t idx = static_cast<size_t>(g);
    if (idx < static_cast<size_t>(GestureType::GESTURE_COUNT)) {
        return gesture_enabled[idx];
    }
    return false;
}

void GestureAI::setGestureEnabled(GestureType g, bool en) {
    size_t idx = static_cast<size_t>(g);
    if (idx < static_cast<size_t>(GestureType::GESTURE_COUNT)) {
        gesture_enabled[idx] = en;
        SettingsData& s = settingsManager.get();
        if (g == GestureType::RAISE_WAKE)  s.gesture_wake_enabled = en;
        if (g == GestureType::WRIST_FLICK) s.gesture_flick_enabled = en;
        if (g == GestureType::DOUBLE_TAP)  s.gesture_tap_enabled = en;
        if (g == GestureType::SHAKE)       s.gesture_shake_enabled = en;
        settingsManager.save();
    }
}

void GestureAI::toggleGestureEnabled(GestureType g) {
    setGestureEnabled(g, !isGestureEnabled(g));
}

GestureAction GestureAI::getGestureAction(GestureType g) const {
    size_t idx = static_cast<size_t>(g);
    if (idx < static_cast<size_t>(GestureType::GESTURE_COUNT)) {
        return gesture_actions[idx];
    }
    return GestureAction::ACTION_NONE;
}

void GestureAI::setGestureAction(GestureType g, GestureAction a) {
    size_t idx = static_cast<size_t>(g);
    if (idx < static_cast<size_t>(GestureType::GESTURE_COUNT)) {
        gesture_actions[idx] = a;
        SettingsData& s = settingsManager.get();
        if (g == GestureType::WRIST_FLICK) s.gesture_flick_action = static_cast<int>(a);
        if (g == GestureType::DOUBLE_TAP)  s.gesture_tap_action = static_cast<int>(a);
        if (g == GestureType::SHAKE)       s.gesture_shake_action = static_cast<int>(a);
        settingsManager.save();
    }
}

void GestureAI::cycleGestureAction(GestureType g) {
    size_t idx = static_cast<size_t>(g);
    if (idx < static_cast<size_t>(GestureType::GESTURE_COUNT)) {
        uint8_t next = (static_cast<uint8_t>(gesture_actions[idx]) + 1) % static_cast<uint8_t>(GestureAction::ACTION_COUNT);
        setGestureAction(g, static_cast<GestureAction>(next));
    }
}

void GestureAI::resetDefaults() {
    enabled = true;
    sensitivity = GestureSensitivity::SENS_MEDIUM;
    gesture_enabled[static_cast<size_t>(GestureType::RAISE_WAKE)]  = true;
    gesture_enabled[static_cast<size_t>(GestureType::WRIST_FLICK)] = true;
    gesture_enabled[static_cast<size_t>(GestureType::DOUBLE_TAP)]  = true;
    gesture_enabled[static_cast<size_t>(GestureType::SHAKE)]       = true;

    gesture_actions[static_cast<size_t>(GestureType::WRIST_FLICK)] = GestureAction::ACTION_NEXT_PAGE;
    gesture_actions[static_cast<size_t>(GestureType::DOUBLE_TAP)]  = GestureAction::ACTION_SELECT;
    gesture_actions[static_cast<size_t>(GestureType::SHAKE)]       = GestureAction::ACTION_TOGGLE_TORCH;
    gesture_actions[static_cast<size_t>(GestureType::RAISE_WAKE)]   = GestureAction::ACTION_WAKE_SCREEN;

    SettingsData& s = settingsManager.get();
    s.gesture_ai_enabled = enabled;
    s.gesture_sensitivity_idx = static_cast<int>(sensitivity);
    s.gesture_wake_enabled = true;
    s.gesture_flick_enabled = true;
    s.gesture_tap_enabled = true;
    s.gesture_shake_enabled = true;
    s.gesture_flick_action = static_cast<int>(GestureAction::ACTION_NEXT_PAGE);
    s.gesture_tap_action = static_cast<int>(GestureAction::ACTION_SELECT);
    s.gesture_shake_action = static_cast<int>(GestureAction::ACTION_TOGGLE_TORCH);
    settingsManager.save();
}

const char* GestureAI::getGestureName(GestureType g) const {
    switch (g) {
        case GestureType::NONE:        return "IDLE";
        case GestureType::WRIST_FLICK: return "WRIST FLICK";
        case GestureType::DOUBLE_TAP:  return "DOUBLE TAP";
        case GestureType::SHAKE:       return "SHAKE WATCH";
        case GestureType::RAISE_WAKE:  return "RAISE TO WAKE";
        default:                       return "UNKNOWN";
    }
}

const char* GestureAI::getActionName(GestureAction a) const {
    switch (a) {
        case GestureAction::ACTION_NONE:         return "NONE";
        case GestureAction::ACTION_WAKE_SCREEN:  return "WAKE SCREEN";
        case GestureAction::ACTION_NEXT_PAGE:    return "NEXT PAGE";
        case GestureAction::ACTION_PREV_PAGE:    return "PREV PAGE";
        case GestureAction::ACTION_SELECT:       return "SELECT (OK)";
        case GestureAction::ACTION_BACK:         return "BACK (CANCEL)";
        case GestureAction::ACTION_TOGGLE_TORCH: return "TOGGLE TORCH";
        case GestureAction::ACTION_MUTE_SOUND:   return "MUTE / UNMUTE";
        default:                                 return "NONE";
    }
}

void GestureAI::triggerSimulatedGesture(GestureType g) {
    last_gesture = g;
    last_confidence = 94;
    last_trigger_ms = millis();
    inference_count++;
    classifyAndDispatch();
}

void GestureAI::update(float ax, float ay, float az, float gx, float gy, float gz, float pitch, float roll) {
    curr_ax = ax; curr_ay = ay; curr_az = az;

    // Buffer sample in ring buffer
    ring_ax[ring_idx] = ax;
    ring_ay[ring_idx] = ay;
    ring_az[ring_idx] = az;
    ring_gx[ring_idx] = gx;
    ring_gy[ring_idx] = gy;
    ring_gz[ring_idx] = gz;
    ring_pitch[ring_idx] = pitch;

    ring_idx = (ring_idx + 1) % WINDOW_SIZE;
    if (sample_count < WINDOW_SIZE) sample_count++;

    if (!enabled) return;

    // Rate limiter: evaluate inference every ~20ms (50Hz) once window is primed
    uint32_t now = millis();
    if (now - last_update_ms < 20 || sample_count < WINDOW_SIZE) {
        return;
    }
    last_update_ms = now;

    // Measure SIMD inference latency in microseconds
    uint32_t t_start = micros();

    extractFeatures(last_features);
    simd_nn_dense_layer_int8(last_features, s_gesture_weights, s_gesture_bias, last_logits, 16, 5);

    last_latency_us = micros() - t_start;
    if (last_latency_us == 0) last_latency_us = 1;
    inference_count++;

    classifyAndDispatch();
}

void GestureAI::extractFeatures(int8_t* out_features) {
    float sum_ax = 0, sum_ay = 0, sum_az = 0;
    float sum_abs_gx = 0, sum_abs_gy = 0, sum_abs_gz = 0;
    float min_ax = 999.0f, max_ax = -999.0f;
    float min_ay = 999.0f, max_ay = -999.0f;
    float min_az = 999.0f, max_az = -999.0f;
    float jerk_acc = 0.0f;
    float max_tap_diff = 0.0f;
    int zero_crossings = 0;

    for (size_t i = 0; i < WINDOW_SIZE; i++) {
        float x = ring_ax[i];
        float y = ring_ay[i];
        float z = ring_az[i];

        sum_ax += x; sum_ay += y; sum_az += z;
        sum_abs_gx += fabsf(ring_gx[i]);
        sum_abs_gy += fabsf(ring_gy[i]);
        sum_abs_gz += fabsf(ring_gz[i]);

        if (x < min_ax) min_ax = x;
        if (x > max_ax) max_ax = x;
        if (y < min_ay) min_ay = y;
        if (y > max_ay) max_ay = y;
        if (z < min_az) min_az = z;
        if (z > max_az) max_az = z;

        if (i > 0) {
            float dx = ring_ax[i] - ring_ax[i - 1];
            float dy = ring_ay[i] - ring_ay[i - 1];
            float dz = ring_az[i] - ring_az[i - 1];
            jerk_acc += fabsf(dx) + fabsf(dy) + fabsf(dz);
            if (fabsf(dz) > max_tap_diff) max_tap_diff = fabsf(dz);
            if ((ring_ax[i] >= 0 && ring_ax[i - 1] < 0) || (ring_ax[i] < 0 && ring_ax[i - 1] >= 0)) {
                zero_crossings++;
            }
        }
    }

    float mean_ax = sum_ax / WINDOW_SIZE;
    float mean_ay = sum_ay / WINDOW_SIZE;
    float mean_az = sum_az / WINDOW_SIZE;

    float var_ax = 0, var_ay = 0, var_az = 0;
    for (size_t i = 0; i < WINDOW_SIZE; i++) {
        var_ax += (ring_ax[i] - mean_ax) * (ring_ax[i] - mean_ax);
        var_ay += (ring_ay[i] - mean_ay) * (ring_ay[i] - mean_ay);
        var_az += (ring_az[i] - mean_az) * (ring_az[i] - mean_az);
    }

    size_t oldest_idx = ring_idx;
    size_t newest_idx = (ring_idx + WINDOW_SIZE - 1) % WINDOW_SIZE;
    float delta_pitch = ring_pitch[newest_idx] - ring_pitch[oldest_idx];

    auto clamp_int8 = [](int val) -> int8_t {
        if (val < -128) return -128;
        if (val > 127)  return 127;
        return (int8_t)val;
    };

    // Pack 16 quantized INT8 features
    out_features[0]  = clamp_int8((int)(mean_ax * 64.0f));
    out_features[1]  = clamp_int8((int)(mean_ay * 64.0f));
    out_features[2]  = clamp_int8((int)(mean_az * 64.0f));
    out_features[3]  = clamp_int8((int)(var_ax * 128.0f));
    out_features[4]  = clamp_int8((int)(var_ay * 128.0f));
    out_features[5]  = clamp_int8((int)(var_az * 128.0f));
    out_features[6]  = clamp_int8((int)((max_ax - min_ax) * 64.0f));
    out_features[7]  = clamp_int8((int)((max_ay - min_ay) * 64.0f));
    out_features[8]  = clamp_int8((int)((max_az - min_az) * 64.0f));
    out_features[9]  = clamp_int8((int)(sum_abs_gx * 0.25f));
    out_features[10] = clamp_int8((int)(sum_abs_gy * 0.25f));
    out_features[11] = clamp_int8((int)(sum_abs_gz * 0.25f));
    out_features[12] = clamp_int8((int)(jerk_acc * 32.0f));
    out_features[13] = clamp_int8((int)(max_tap_diff * 96.0f));
    out_features[14] = clamp_int8((int)(delta_pitch * 2.0f));
    out_features[15] = clamp_int8(zero_crossings * 16);
}

void GestureAI::classifyAndDispatch() {
    // Find class with highest activation
    int8_t max_logit = 0;
    size_t best_class = 0;
    int32_t total_activation = 0;

    for (size_t c = 0; c < 5; c++) {
        total_activation += last_logits[c];
        if (last_logits[c] > max_logit) {
            max_logit = last_logits[c];
            best_class = c;
        }
    }

    if (total_activation <= 0 || max_logit <= 0) {
        last_gesture = GestureType::NONE;
        last_confidence = 0;
        return;
    }

    uint8_t conf = (uint8_t)((max_logit * 100) / total_activation);
    last_confidence = conf;

    // Check sensitivity threshold
    uint8_t threshold = 55; // MEDIUM
    if (sensitivity == GestureSensitivity::SENS_LOW)         threshold = 75;
    else if (sensitivity == GestureSensitivity::SENS_HIGH)  threshold = 40;
    else if (sensitivity == GestureSensitivity::SENS_ULTRA) threshold = 30;

    if (best_class == 0 || conf < threshold) {
        last_gesture = GestureType::NONE;
        return;
    }

    GestureType detected = static_cast<GestureType>(best_class);
    if (!isGestureEnabled(detected)) {
        return;
    }

    // Refractory period: avoid rapid duplicate triggers (450ms debounce)
    uint32_t now = millis();
    if (now - last_trigger_ms < 450) {
        return;
    }
    last_trigger_ms = now;
    last_gesture = detected;

    // Execute configured action
    GestureAction act = getGestureAction(detected);
    if (act == GestureAction::ACTION_NONE) return;

    if (act == GestureAction::ACTION_WAKE_SCREEN) {
        powerManager.boostCpuFrequency(240);
        ui.resetActivityTimer();
        soundManager.playNotification();
    } else if (act == GestureAction::ACTION_TOGGLE_TORCH) {
        ledManager.setMasterSwitch(!ledManager.isMasterSwitchOn());
        ui.showToast(ledManager.isMasterSwitchOn() ? "[TORCH ON]" : "[TORCH OFF]", 1000);
        soundManager.playNavSelect();
    } else if (act == GestureAction::ACTION_MUTE_SOUND) {
        soundManager.setMasterSwitch(!soundManager.isMasterSwitchOn());
        ui.showToast(soundManager.isMasterSwitchOn() ? "[SOUND ON]" : "[MUTED]", 1000);
    } else {
        ui.dispatchGestureAction(act);
    }
}
