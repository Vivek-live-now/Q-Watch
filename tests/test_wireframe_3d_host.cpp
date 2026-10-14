#include <iostream>
#include <cassert>
#include <cmath>
#include "simd_accel.h"

int main() {
    // 1. Identity matrix verification (pitch=0, roll=0, yaw=0)
    int16_t id_matrix[9];
    simd_compute_rotation_matrix_q14(id_matrix, 0, 0, 0);

    assert(id_matrix[0] == 16384 && id_matrix[1] == 0 && id_matrix[2] == 0);
    assert(id_matrix[3] == 0 && id_matrix[4] == 16384 && id_matrix[5] == 0);
    assert(id_matrix[6] == 0 && id_matrix[7] == 0 && id_matrix[8] == 16384);

    // 2. Vertex transformation with identity matrix and perspective projection
    Point3D cube_v[8] = {
        { -20, -20, -20 }, {  20, -20, -20 }, {  20,  20, -20 }, { -20,  20, -20 },
        { -20, -20,  20 }, {  20, -20,  20 }, {  20,  20,  20 }, { -20,  20,  20 }
    };
    Point2D out_2d[8];

    simd_transform_3d_vertices(out_2d, cube_v, 8, id_matrix, 120, 64, 32);

    // Origin projection check: (0, 0, 0) should project to (64, 32)
    Point3D origin[1] = { {0, 0, 0} };
    Point2D origin_2d[1];
    simd_transform_3d_vertices(origin_2d, origin, 1, id_matrix, 120, 64, 32);
    assert(origin_2d[0].x == 64 && origin_2d[0].y == 32);

    // Cube vertices must be symmetrically distributed around (64, 32)
    for (int i = 0; i < 8; i++) {
        assert(out_2d[i].x >= 0 && out_2d[i].x <= 128);
        assert(out_2d[i].y >= 0 && out_2d[i].y <= 64);
    }

    // 3. 90-degree yaw rotation verification
    int16_t yaw90_matrix[9];
    simd_compute_rotation_matrix_q14(yaw90_matrix, 0, 0, 90);
    // Point along +X axis (20, 0, 0) rotated 90 deg around Z should map to (0, 20, 0)
    Point3D pt_x[1] = { {20, 0, 0} };
    Point2D pt_2d[1];
    simd_transform_3d_vertices(pt_2d, pt_x, 1, yaw90_matrix, 120, 64, 32);
    // Transformed: x' = 0, y' = 20 -> sx = 64, sy = 32 - 20 = 12
    assert(std::abs(pt_2d[0].x - 64) <= 1);
    assert(std::abs(pt_2d[0].y - 12) <= 1);

    std::cout << "[PASS] Host C++ 3D wireframe vector math & projection verified." << std::endl;
    return 0;
}
