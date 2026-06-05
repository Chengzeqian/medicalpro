#pragma once

#include "types.h"
#include <vector>

class GICPRegistration;
struct GICPParams;
struct GICPResult;

namespace meshgpu_open3d_backend {

// Returns true if MeshGPULib was built with Open3D Tensor ICP support.
// Implementation lives in tensor_icp_adapter.cpp; a stub returning false
// is used when MESHGPU_HAS_OPEN3D_BACKEND is not defined.
bool isAvailable();

// Run Tensor ICP using current correspondences/source/target held by the
// owning GICPRegistration instance. The adapter is intentionally given a
// pointer to the owning registration object instead of raw device buffers
// so it can pull the latest target mesh and source point cloud without
// duplicating ownership state.
//
// Contract:
//   - target_mesh: device-resident MeshSoA already initialized in MeshGPU
//   - source_cloud: device-resident SourcePointCloud already uploaded
//   - initial_transform: row-major 4x4 in Matrix4x4
//   - params: max_iterations / convergence_threshold / distance_threshold
//             / use_point_to_plane are honored; curvature weighting is
//             passed through Open3D RobustKernel options when applicable
//
// On success, returns a GICPResult with final_transform / iterations /
// final_rmse / converged / rmse_history populated. On failure (Open3D
// missing or runtime error) returns a result with converged=false and an
// rmse_history starting with the input distance threshold so callers can
// detect the no-op.
GICPResult align(
    const MeshSoA& target_mesh_device,
    const SourcePointCloud& source_cloud_device,
    const Matrix4x4& initial_transform,
    const GICPParams& params
);

}  // namespace meshgpu_open3d_backend
