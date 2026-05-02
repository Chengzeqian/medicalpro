#pragma once

/**
 * Simplified types for Open3D visualizer (no CUDA dependencies)
 * This allows visualizer.cpp to be compiled with pure C++ compiler
 */

#include <cstdint>

// Simple float3 without CUDA decorators
struct VisFloat3 {
    float x, y, z;
    VisFloat3() : x(0), y(0), z(0) {}
    VisFloat3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
};

// Simple 4x4 matrix without CUDA decorators
struct VisMatrix4x4 {
    float m[16];

    VisMatrix4x4() {
        for (int i = 0; i < 16; ++i) m[i] = 0.0f;
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }
};

// Simple mesh structure without CUDA decorators
struct VisMeshSoA {
    float* vertices_x;
    float* vertices_y;
    float* vertices_z;
    float* normals_x;
    float* normals_y;
    float* normals_z;
    uint32_t* faces_v0;
    uint32_t* faces_v1;
    uint32_t* faces_v2;
    uint32_t num_vertices;
    uint32_t num_faces;
};
