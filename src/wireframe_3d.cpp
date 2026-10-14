#include "wireframe_3d.h"
#include "sensors.h"
#include <Arduino.h>
#include <string.h>

Wireframe3D wireframe3d;

Wireframe3D::Wireframe3D()
    : current_model(WireframeModel::CUBE),
      control_mode(WireframeControlMode::IMU_TILT),
      show_fps(true),
      angle_pitch(0),
      angle_roll(0),
      angle_yaw(0),
      vertex_count(0),
      edge_count(0),
      last_frame_time_ms(0),
      render_time_us(0),
      current_fps(50.0f) {
    memset(model_vertices, 0, sizeof(model_vertices));
    memset(projected_vertices, 0, sizeof(projected_vertices));
    memset(model_edges, 0, sizeof(model_edges));
    memset(rot_matrix_q14, 0, sizeof(rot_matrix_q14));
}

void Wireframe3D::begin() {
    loadModelGeometry(current_model);
    last_frame_time_ms = millis();
}

const char* Wireframe3D::getModelName(WireframeModel model) const {
    switch (model) {
        case WireframeModel::CUBE:    return "Q-CUBE";
        case WireframeModel::PYRAMID: return "PYRAMID";
        case WireframeModel::DIAMOND: return "DIAMOND";
        case WireframeModel::ARROW:   return "ARROW";
        case WireframeModel::PRISM:   return "PRISM";
        default:                      return "UNKNOWN";
    }
}

void Wireframe3D::setModel(WireframeModel model) {
    current_model = (WireframeModel)((uint8_t)model % (uint8_t)WireframeModel::MODEL_COUNT);
    loadModelGeometry(current_model);
}

void Wireframe3D::nextModel() {
    uint8_t next = ((uint8_t)current_model + 1) % (uint8_t)WireframeModel::MODEL_COUNT;
    setModel((WireframeModel)next);
}

void Wireframe3D::prevModel() {
    uint8_t prev = ((uint8_t)current_model + (uint8_t)WireframeModel::MODEL_COUNT - 1) % (uint8_t)WireframeModel::MODEL_COUNT;
    setModel((WireframeModel)prev);
}

void Wireframe3D::toggleControlMode() {
    control_mode = (control_mode == WireframeControlMode::IMU_TILT) 
                 ? WireframeControlMode::AUTO_SPIN 
                 : WireframeControlMode::IMU_TILT;
}

void Wireframe3D::setControlMode(WireframeControlMode mode) {
    control_mode = mode;
}

void Wireframe3D::loadModelGeometry(WireframeModel model) {
    switch (model) {
        case WireframeModel::CUBE: {
            // 8 vertices of a cube (-20 to +20)
            vertex_count = 8;
            static const Point3D cube_v[8] = {
                { -20, -20, -20 }, {  20, -20, -20 }, {  20,  20, -20 }, { -20,  20, -20 },
                { -20, -20,  20 }, {  20, -20,  20 }, {  20,  20,  20 }, { -20,  20,  20 }
            };
            memcpy(model_vertices, cube_v, sizeof(cube_v));

            edge_count = 12;
            static const WireframeEdge cube_e[12] = {
                {0, 1}, {1, 2}, {2, 3}, {3, 0}, // Front face
                {4, 5}, {5, 6}, {6, 7}, {7, 4}, // Back face
                {0, 4}, {1, 5}, {2, 6}, {3, 7}  // Connecting ribs
            };
            memcpy(model_edges, cube_e, sizeof(cube_e));
            break;
        }

        case WireframeModel::PYRAMID: {
            // 5 vertices (square base + apex)
            vertex_count = 5;
            static const Point3D pyr_v[5] = {
                {   0,  24,   0 }, // Apex (top)
                { -22, -18, -22 }, {  22, -18, -22 }, {  22, -18,  22 }, { -22, -18,  22 }
            };
            memcpy(model_vertices, pyr_v, sizeof(pyr_v));

            edge_count = 8;
            static const WireframeEdge pyr_e[8] = {
                {1, 2}, {2, 3}, {3, 4}, {4, 1}, // Base square
                {0, 1}, {0, 2}, {0, 3}, {0, 4}  // Edges to apex
            };
            memcpy(model_edges, pyr_e, sizeof(pyr_e));
            break;
        }

        case WireframeModel::DIAMOND: {
            // 6 vertices (Octahedron)
            vertex_count = 6;
            static const Point3D oct_v[6] = {
                {   0,  26,   0 }, // Top apex
                {   0, -26,   0 }, // Bottom apex
                { -24,   0,   0 }, {   0,   0, -24 }, {  24,   0,   0 }, {   0,   0,  24 } // Waist ring
            };
            memcpy(model_vertices, oct_v, sizeof(oct_v));

            edge_count = 12;
            static const WireframeEdge oct_e[12] = {
                {2, 3}, {3, 4}, {4, 5}, {5, 2}, // Waist ring
                {0, 2}, {0, 3}, {0, 4}, {0, 5}, // Top pyramid
                {1, 2}, {1, 3}, {1, 4}, {1, 5}  // Bottom pyramid
            };
            memcpy(model_edges, oct_e, sizeof(oct_e));
            break;
        }

        case WireframeModel::ARROW: {
            // 7 vertices: 3D arrow / directional compass pointer
            vertex_count = 7;
            static const Point3D arr_v[7] = {
                {   0,  28,   0 }, // Arrow tip
                { -18, -10,  12 }, {  18, -10,  12 }, // Front fins
                { -18, -10, -12 }, {  18, -10, -12 }, // Back fins
                {   0, -10,   0 }, // Fin center
                {   0, -26,   0 }  // Tail end
            };
            memcpy(model_vertices, arr_v, sizeof(arr_v));

            edge_count = 10;
            static const WireframeEdge arr_e[10] = {
                {0, 1}, {0, 2}, {0, 3}, {0, 4}, // Tip to fins
                {1, 5}, {2, 5}, {3, 5}, {4, 5}, // Fins to center
                {0, 5}, {5, 6}                  // Shaft
            };
            memcpy(model_edges, arr_e, sizeof(arr_e));
            break;
        }

        case WireframeModel::PRISM: {
            // 12 vertices: Hexagonal prism
            vertex_count = 12;
            static const Point3D prism_v[12] = {
                // Top hexagon (y = 18)
                { -12,  18, -20 }, {  12,  18, -20 }, {  22,  18,   0 },
                {  12,  18,  20 }, { -12,  18,  20 }, { -22,  18,   0 },
                // Bottom hexagon (y = -18)
                { -12, -18, -20 }, {  12, -18, -20 }, {  22, -18,   0 },
                {  12, -18,  20 }, { -12, -18,  20 }, { -22, -18,   0 }
            };
            memcpy(model_vertices, prism_v, sizeof(prism_v));

            edge_count = 18;
            static const WireframeEdge prism_e[18] = {
                {0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 0}, // Top
                {6, 7}, {7, 8}, {8, 9}, {9, 10}, {10, 11}, {11, 6}, // Bottom
                {0, 6}, {1, 7}, {2, 8}, {3, 9}, {4, 10}, {5, 11}  // Vertical ribs
            };
            memcpy(model_edges, prism_e, sizeof(prism_e));
            break;
        }

        default:
            vertex_count = 0;
            edge_count = 0;
            break;
    }
}

void Wireframe3D::update() {
    uint32_t now = millis();
    uint32_t dt_ms = now - last_frame_time_ms;
    if (dt_ms > 0) {
        last_frame_time_ms = now;
        float inst_fps = 1000.0f / (float)dt_ms;
        // Smooth FPS calculation
        current_fps = current_fps * 0.9f + inst_fps * 0.1f;
    }

    if (control_mode == WireframeControlMode::IMU_TILT) {
        OrientationData o = sensors.getOrientation();
        angle_pitch = (int16_t)o.pitch;
        angle_roll = (int16_t)o.roll;
        angle_yaw = (int16_t)o.yaw;
    } else {
        // Auto-spin with distinct 3-axis angular rates
        angle_pitch = (angle_pitch + 2) % 360;
        angle_roll = (angle_roll + 3) % 360;
        angle_yaw = (angle_yaw + 1) % 360;
    }

    // Benchmark SIMD 3D transformation & perspective projection
    uint32_t t0 = micros();

    simd_compute_rotation_matrix_q14(rot_matrix_q14, angle_pitch, angle_roll, angle_yaw);
    simd_transform_3d_vertices(projected_vertices, model_vertices, vertex_count, rot_matrix_q14, 110, 64, 32);

    render_time_us = (uint16_t)(micros() - t0);
}
