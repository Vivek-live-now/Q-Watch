#ifndef WIREFRAME_3D_H
#define WIREFRAME_3D_H

#include <stdint.h>
#include <stddef.h>
#include "simd_accel.h"

enum class WireframeModel : uint8_t {
    CUBE = 0,
    PYRAMID = 1,
    DIAMOND = 2,
    ARROW = 3,
    PRISM = 4,
    MODEL_COUNT = 5
};

enum class WireframeControlMode : uint8_t {
    IMU_TILT = 0,
    AUTO_SPIN = 1
};

struct WireframeEdge {
    uint8_t v1;
    uint8_t v2;
};

class Wireframe3D {
public:
    static const size_t MAX_VERTICES = 16;
    static const size_t MAX_EDGES = 24;

    Wireframe3D();

    void begin();
    void update();

    // Model selection
    void nextModel();
    void prevModel();
    void setModel(WireframeModel model);
    WireframeModel getModel() const { return current_model; }
    const char* getModelName(WireframeModel model) const;

    // Control mode
    void toggleControlMode();
    void setControlMode(WireframeControlMode mode);
    WireframeControlMode getControlMode() const { return control_mode; }

    // Metrics overlay
    void toggleFpsDisplay() { show_fps = !show_fps; }
    bool isFpsDisplayEnabled() const { return show_fps; }

    // Render accessors
    const Point2D* getProjectedVertices() const { return projected_vertices; }
    const WireframeEdge* getEdges() const { return model_edges; }
    size_t getVertexCount() const { return vertex_count; }
    size_t getEdgeCount() const { return edge_count; }

    // Performance telemetry
    uint16_t getRenderTimeUs() const { return render_time_us; }
    float getFps() const { return current_fps; }
    int16_t getPitch() const { return angle_pitch; }
    int16_t getRoll() const { return angle_roll; }
    int16_t getYaw() const { return angle_yaw; }

private:
    void loadModelGeometry(WireframeModel model);

    WireframeModel current_model;
    WireframeControlMode control_mode;
    bool show_fps;

    int16_t angle_pitch;
    int16_t angle_roll;
    int16_t angle_yaw;

    ALIGNED16 Point3D model_vertices[MAX_VERTICES];
    ALIGNED16 Point2D projected_vertices[MAX_VERTICES];
    WireframeEdge model_edges[MAX_EDGES];
    size_t vertex_count;
    size_t edge_count;

    ALIGNED16 int16_t rot_matrix_q14[9];

    uint32_t last_frame_time_ms;
    uint16_t render_time_us;
    float current_fps;
};

extern Wireframe3D wireframe3d;

#endif // WIREFRAME_3D_H
