#pragma once

#include "mesh_gpu_interface.h"

namespace mesh_gpu {

struct RuntimeRegistrationResult {
    Transform4x4 transform;
    float rmse = 0.0f;
    int iterations = 0;
    bool converged = false;
};

struct RuntimeTransformCandidateScore {
    int candidateIndex = -1;
    int score = 0;
    float meanDistanceMm = 0.0f;
    float normalConsistencyScore = 0.0f;
    float curvatureScore = 0.0f;
    bool geometryScoreAvailable = false;
    bool success = false;
};

struct RuntimeConstraintFilterResult {
    std::vector<int> selectedIndices;
    bool success = false;
};

struct RuntimeRefineCandidateRequest {
    int candidateIndex = -1;
    Transform4x4 initialTransform;
};

struct RuntimeRefineCandidateResult {
    int candidateIndex = -1;
    Transform4x4 transform;
    float rmse = 0.0f;
    int iterations = 0;
    bool converged = false;
    bool success = false;
};

class MeshGPURuntimeApi {
public:
    virtual ~MeshGPURuntimeApi() = default;

    virtual bool loadTargetMesh(const std::string& meshPath, float cellSize = 1.0f) = 0;
    virtual bool hasTargetMesh() const = 0;
    virtual bool setTargetMesh(const std::vector<Point3D>& vertices,
                               const std::vector<Normal3D>& normals,
                               const std::vector<std::array<int, 3>>& triangles,
                               float cellSize = 1.0f) = 0;
    virtual bool setSourcePointCloud(const std::vector<Point3D>& points) = 0;
    virtual RuntimeRegistrationResult runRegistration(const RegistrationParams& params) = 0;
    virtual RuntimeRegistrationResult runRegistrationWithRotationSearch(
        const RotationSearchParams& rotationParams,
        const RegistrationParams& params) = 0;
    virtual std::vector<RuntimeTransformCandidateScore> scoreTransformCandidates(
        const std::vector<Transform4x4>& candidates,
        float cutoffMm = 12.0f) = 0;
    virtual std::vector<RuntimeRefineCandidateResult> refineTransformCandidates(
        const std::vector<RuntimeRefineCandidateRequest>& candidates,
        const RegistrationParams& params) = 0;
    virtual RuntimeConstraintFilterResult filterSourcePointsByConstraints(
        const std::vector<Point3D>& points,
        const Point3D& targetRegionCenter,
        float targetRegionRadiusMm,
        float membershipRadiusMm,
        const std::vector<Point3D>& constraintPoints,
        int minimumPointCount) = 0;
    virtual RuntimeConstraintFilterResult filterTargetPointsByConstraints(
        const Point3D& targetRegionCenter,
        float targetRegionRadiusMm,
        float membershipRadiusMm,
        const std::vector<Point3D>& constraintPoints,
        int minimumPointCount) = 0;
    virtual ConstrainedMeshResult buildConstrainedTargetMesh(
        const Point3D& targetRegionCenter,
        float targetRegionRadiusMm,
        float membershipRadiusMm,
        const std::vector<Point3D>& constraintPoints,
        int minimumPointCount) = 0;
};

} // namespace mesh_gpu

extern "C" {
MESHGPU_API mesh_gpu::MeshGPURuntimeApi* CreateMeshGPURuntimeApi();
MESHGPU_API void DestroyMeshGPURuntimeApi(mesh_gpu::MeshGPURuntimeApi* instance);
}
