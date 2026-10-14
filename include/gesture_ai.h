#ifndef GESTURE_AI_H
#define GESTURE_AI_H

#include <stdint.h>
#include <stddef.h>
#include "simd_accel.h"

enum class GestureType : uint8_t {
    NONE = 0,
    WRIST_FLICK = 1,
    DOUBLE_TAP = 2,
    SHAKE = 3,
    RAISE_WAKE = 4,
    GESTURE_COUNT = 5
};

enum class GestureSensitivity : uint8_t {
    SENS_LOW = 0,     // 75% confidence threshold (strict)
    SENS_MEDIUM = 1,  // 55% confidence threshold (balanced)
    SENS_HIGH = 2,    // 40% confidence threshold (responsive)
    SENS_ULTRA = 3,   // 30% confidence threshold (instant)
    SENSITIVITY_COUNT = 4
};

enum class GestureAction : uint8_t {
    ACTION_NONE = 0,
    ACTION_WAKE_SCREEN = 1,
    ACTION_NEXT_PAGE = 2,
    ACTION_PREV_PAGE = 3,
    ACTION_SELECT = 4,
    ACTION_BACK = 5,
    ACTION_TOGGLE_TORCH = 6,
    ACTION_MUTE_SOUND = 7,
    ACTION_COUNT = 8
};

class GestureAI {
public:
    GestureAI();

    void begin();
    void update(float ax, float ay, float az, float gx, float gy, float gz, float pitch, float roll);

    // Master Switch
    bool isEnabled() const { return enabled; }
    void setEnabled(bool en);
    void toggleEnabled() { setEnabled(!enabled); }

    // Sensitivity
    GestureSensitivity getSensitivity() const { return sensitivity; }
    void setSensitivity(GestureSensitivity s);
    void cycleSensitivity();
    const char* getSensitivityName() const;

    // Gesture Individual Toggles
    bool isGestureEnabled(GestureType g) const;
    void setGestureEnabled(GestureType g, bool en);
    void toggleGestureEnabled(GestureType g);

    // Action Mappings
    GestureAction getGestureAction(GestureType g) const;
    void setGestureAction(GestureType g, GestureAction a);
    void cycleGestureAction(GestureType g);

    // Reset Defaults
    void resetDefaults();

    // Telemetry & Monitor APIs
    GestureType getLastDetectedGesture() const { return last_gesture; }
    uint8_t getLastConfidence() const { return last_confidence; }
    uint32_t getLastInferenceLatencyUs() const { return last_latency_us; }
    uint32_t getInferenceCount() const { return inference_count; }
    const int8_t* getLastLogits() const { return last_logits; }
    const int8_t* getLastFeatures() const { return last_features; }
    void getRecentAccel(float& ax, float& ay, float& az) const { ax = curr_ax; ay = curr_ay; az = curr_az; }

    const char* getGestureName(GestureType g) const;
    const char* getActionName(GestureAction a) const;

    // Simulated gesture test trigger (for testing/demo)
    void triggerSimulatedGesture(GestureType g);

private:
    void extractFeatures(int8_t* out_features);
    void classifyAndDispatch();

    bool enabled;
    GestureSensitivity sensitivity;
    bool gesture_enabled[static_cast<size_t>(GestureType::GESTURE_COUNT)];
    GestureAction gesture_actions[static_cast<size_t>(GestureType::GESTURE_COUNT)];

    // Sliding Window Buffer (16 samples of 6-DOF IMU)
    static const size_t WINDOW_SIZE = 16;
    float ring_ax[WINDOW_SIZE];
    float ring_ay[WINDOW_SIZE];
    float ring_az[WINDOW_SIZE];
    float ring_gx[WINDOW_SIZE];
    float ring_gy[WINDOW_SIZE];
    float ring_gz[WINDOW_SIZE];
    float ring_pitch[WINDOW_SIZE];
    size_t ring_idx;
    size_t sample_count;

    // Telemetry and Execution State
    GestureType last_gesture;
    uint8_t last_confidence;
    uint32_t last_latency_us;
    uint32_t inference_count;
    uint32_t last_update_ms;
    uint32_t last_trigger_ms;
    float curr_ax, curr_ay, curr_az;

    ALIGNED16 int8_t last_features[16];
    ALIGNED16 int8_t last_logits[static_cast<size_t>(GestureType::GESTURE_COUNT)];
};

extern GestureAI gestureAi;

#endif // GESTURE_AI_H
