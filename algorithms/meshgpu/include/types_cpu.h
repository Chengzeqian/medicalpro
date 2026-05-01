#pragma once

// ============================================================================
// CPU-only Types for Tools (no CUDA dependency)
// ============================================================================

#include <cstdint>
#include <cfloat>
#include <cmath>
#include <algorithm>

constexpr int MAX_VERTICES = 200000;
constexpr int MAX_FACES = 400000;

struct float3_t {
    float x, y, z;

    float3_t() : x(0), y(0), z(0) {}
    float3_t(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    float3_t operator+(const float3_t& other) const {
        return float3_t(x + other.x, y + other.y, z + other.z);
    }

    float3_t operator-(const float3_t& other) const {
        return float3_t(x - other.x, y - other.y, z - other.z);
    }

    float3_t operator*(float s) const {
        return float3_t(x * s, y * s, z * s);
    }

    float dot(const float3_t& other) const {
        return x * other.x + y * other.y + z * other.z;
    }

    float3_t cross(const float3_t& other) const {
        return float3_t(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }

    float length() const {
        return sqrtf(x * x + y * y + z * z);
    }

    float3_t normalized() const {
        float len = length();
        if (len > 1e-8f) {
            return float3_t(x / len, y / len, z / len);
        }
        return float3_t(0, 0, 0);
    }
};

struct Triangle {
    uint32_t v0, v1, v2;

    Triangle() : v0(0), v1(0), v2(0) {}
    Triangle(uint32_t _v0, uint32_t _v1, uint32_t _v2)
        : v0(_v0), v1(_v1), v2(_v2) {}
};

// Mesh Data Structure - Structure of Arrays (SoA)
struct MeshSoA {
    float* vertices_x;
    float* vertices_y;
    float* vertices_z;

    float* normals_x;
    float* normals_y;
    float* normals_z;

    float* curvature;
    float* gaussian_curv;

    uint32_t* faces_v0;
    uint32_t* faces_v1;
    uint32_t* faces_v2;

    float* face_normals_x;
    float* face_normals_y;
    float* face_normals_z;

    float* face_areas;

    uint32_t* vertex_face_offset;
    uint32_t* vertex_face_indices;

    // Vertex colors (RGBA)
    uint8_t* colors_r;
    uint8_t* colors_g;
    uint8_t* colors_b;
    uint8_t* colors_a;
    bool has_colors;

    uint32_t num_vertices;
    uint32_t num_faces;

    bool on_device;
};

// 4x4 Transformation Matrix
struct Matrix4x4 {
    float m[16];  // Row-major: m[row*4 + col]

    Matrix4x4() {
        for (int i = 0; i < 16; ++i) m[i] = 0.0f;
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }

    static Matrix4x4 rotationZ(float angle) {
        Matrix4x4 mat;
        float c = cosf(angle);
        float s = sinf(angle);
        mat.m[0] = c;  mat.m[1] = -s; mat.m[2] = 0;  mat.m[3] = 0;
        mat.m[4] = s;  mat.m[5] = c;  mat.m[6] = 0;  mat.m[7] = 0;
        mat.m[8] = 0;  mat.m[9] = 0;  mat.m[10] = 1; mat.m[11] = 0;
        mat.m[12] = 0; mat.m[13] = 0; mat.m[14] = 0; mat.m[15] = 1;
        return mat;
    }

    static Matrix4x4 rotationX(float angle) {
        Matrix4x4 mat;
        float c = cosf(angle);
        float s = sinf(angle);
        mat.m[0] = 1;  mat.m[1] = 0;  mat.m[2] = 0;  mat.m[3] = 0;
        mat.m[4] = 0;  mat.m[5] = c;  mat.m[6] = -s; mat.m[7] = 0;
        mat.m[8] = 0;  mat.m[9] = s;  mat.m[10] = c; mat.m[11] = 0;
        mat.m[12] = 0; mat.m[13] = 0; mat.m[14] = 0; mat.m[15] = 1;
        return mat;
    }

    static Matrix4x4 rotationY(float angle) {
        Matrix4x4 mat;
        float c = cosf(angle);
        float s = sinf(angle);
        mat.m[0] = c;  mat.m[1] = 0;  mat.m[2] = s;  mat.m[3] = 0;
        mat.m[4] = 0;  mat.m[5] = 1;  mat.m[6] = 0;  mat.m[7] = 0;
        mat.m[8] = -s; mat.m[9] = 0;  mat.m[10] = c; mat.m[11] = 0;
        mat.m[12] = 0; mat.m[13] = 0; mat.m[14] = 0; mat.m[15] = 1;
        return mat;
    }

    static Matrix4x4 translation(float tx, float ty, float tz) {
        Matrix4x4 mat;
        mat.m[3] = tx;
        mat.m[7] = ty;
        mat.m[11] = tz;
        return mat;
    }

    Matrix4x4 operator*(const Matrix4x4& other) const {
        Matrix4x4 result;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                result.m[i * 4 + j] = 0;
                for (int k = 0; k < 4; ++k) {
                    result.m[i * 4 + j] += m[i * 4 + k] * other.m[k * 4 + j];
                }
            }
        }
        return result;
    }

    float3_t transformPoint(const float3_t& p) const {
        float3_t result;
        result.x = m[0] * p.x + m[1] * p.y + m[2] * p.z + m[3];
        result.y = m[4] * p.x + m[5] * p.y + m[6] * p.z + m[7];
        result.z = m[8] * p.x + m[9] * p.y + m[10] * p.z + m[11];
        return result;
    }

    float3_t transformVector(const float3_t& v) const {
        float3_t result;
        result.x = m[0] * v.x + m[1] * v.y + m[2] * v.z;
        result.y = m[4] * v.x + m[5] * v.y + m[6] * v.z;
        result.z = m[8] * v.x + m[9] * v.y + m[10] * v.z;
        return result;
    }
};
