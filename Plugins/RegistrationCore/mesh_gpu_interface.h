#pragma once

#include <array>
#include <string>
#include <vector>

namespace mesh_gpu {

struct Point3D
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Normal3D
{
    float nx = 0.0f;
    float ny = 0.0f;
    float nz = 0.0f;
};

enum class CurvatureWeightMode
{
    Disabled = 0,
    Enabled = 1
};

struct RegistrationParams
{
    int max_iterations = 0;
    float convergence_threshold = 0.0f;
    float distance_threshold = 0.0f;
    bool use_point_to_plane = false;
    bool verbose = false;
    CurvatureWeightMode curvature_weight_mode = CurvatureWeightMode::Disabled;
};

struct RotationSearchParams
{
};

struct Transform4x4
{
    float data[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
};

struct RegistrationResult
{
    Transform4x4 transform;
    float rmse = 0.0f;
    int iterations = 0;
    bool converged = false;
};

class MeshGPUInterface
{
public:
    virtual ~MeshGPUInterface() = default;

    virtual bool loadTargetMesh(const std::string& meshPath) = 0;
    virtual bool hasTargetMesh() const = 0;
    virtual bool setTargetMesh(
        const std::vector<Point3D>& vertices,
        const std::vector<Normal3D>& normals,
        const std::vector<std::array<int, 3>>& triangles,
        float cellSize) = 0;
    virtual void setSourcePointCloud(const std::vector<Point3D>& points) = 0;
    virtual RegistrationResult runRegistration(const RegistrationParams& params) = 0;
    virtual RegistrationResult runRegistrationWithRotationSearch(
        const RotationSearchParams& rotationParams,
        const RegistrationParams& params) = 0;
};

} // namespace mesh_gpu
