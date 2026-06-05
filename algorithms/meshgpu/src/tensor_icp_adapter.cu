#include "tensor_icp_adapter.h"
#include "gicp_registration.h"

#include <cstring>

#ifdef MESHGPU_HAS_OPEN3D_BACKEND
#include <cuda_runtime.h>

#include <open3d/core/Tensor.h>
#include <open3d/core/Device.h>
#include <open3d/core/Dtype.h>
#include <open3d/core/EigenConverter.h>
#include <open3d/t/geometry/PointCloud.h>
#include <open3d/t/pipelines/registration/Registration.h>
#include <open3d/t/pipelines/registration/TransformationEstimation.h>
#include <open3d/t/pipelines/registration/RobustKernel.h>

#include <Eigen/Dense>
#endif

namespace meshgpu_open3d_backend {

#ifndef MESHGPU_HAS_OPEN3D_BACKEND

bool isAvailable() { return false; }

GICPResult align(const MeshSoA&, const SourcePointCloud&,
                 const Matrix4x4&, const GICPParams& params) {
    GICPResult result;
    result.converged = false;
    result.iterations = 0;
    result.final_rmse = params.distance_threshold;
    result.rmse_history.push_back(params.distance_threshold);
    return result;
}

#else

namespace o3c = open3d::core;
namespace o3tg = open3d::t::geometry;
namespace o3treg = open3d::t::pipelines::registration;

bool isAvailable() { return true; }

namespace {

// Download a SoA float component (device -> host) into a flat XYZ host buffer.
// Out buffer layout: [x0, y0, z0, x1, y1, z1, ...].
bool downloadXYZ(const float* dx, const float* dy, const float* dz,
                 uint32_t n, std::vector<float>& out_xyz) {
    out_xyz.assign(static_cast<size_t>(n) * 3, 0.0f);
    if (n == 0) return true;
    std::vector<float> hx(n), hy(n), hz(n);
    if (cudaMemcpy(hx.data(), dx, n * sizeof(float),
                   cudaMemcpyDeviceToHost) != cudaSuccess) return false;
    if (cudaMemcpy(hy.data(), dy, n * sizeof(float),
                   cudaMemcpyDeviceToHost) != cudaSuccess) return false;
    if (cudaMemcpy(hz.data(), dz, n * sizeof(float),
                   cudaMemcpyDeviceToHost) != cudaSuccess) return false;
    for (uint32_t i = 0; i < n; ++i) {
        out_xyz[i * 3 + 0] = hx[i];
        out_xyz[i * 3 + 1] = hy[i];
        out_xyz[i * 3 + 2] = hz[i];
    }
    return true;
}

// Build an Open3D Tensor PointCloud on a given device from interleaved XYZ.
o3tg::PointCloud makeTensorPC(const std::vector<float>& xyz,
                              const std::vector<float>* normals_xyz,
                              const o3c::Device& device) {
    const int64_t n = static_cast<int64_t>(xyz.size() / 3);
    o3c::Tensor positions = o3c::Tensor(xyz, {n, 3}, o3c::Float32, device);
    o3tg::PointCloud pc(positions);
    if (normals_xyz != nullptr && normals_xyz->size() == xyz.size()) {
        o3c::Tensor n_t = o3c::Tensor(*normals_xyz, {n, 3},
                                      o3c::Float32, device);
        pc.SetPointNormals(n_t);
    }
    return pc;
}

Eigen::Matrix4d toEigen(const Matrix4x4& m) {
    Eigen::Matrix4d e;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            e(r, c) = static_cast<double>(m.m[r * 4 + c]);
    return e;
}

Matrix4x4 fromEigen(const Eigen::Matrix4d& e) {
    Matrix4x4 m;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            m.m[r * 4 + c] = static_cast<float>(e(r, c));
    return m;
}

}  // namespace

GICPResult align(const MeshSoA& target_mesh_device,
                 const SourcePointCloud& source_cloud_device,
                 const Matrix4x4& initial_transform,
                 const GICPParams& params) {
    GICPResult result;
    result.converged = false;
    result.iterations = 0;
    result.final_rmse = params.distance_threshold;
    result.final_transform = initial_transform;

    // --- Stage 1: Download device SoA into host XYZ buffers
    std::vector<float> tgt_xyz, tgt_n_xyz, src_xyz, src_n_xyz;
    if (!downloadXYZ(target_mesh_device.vertices_x,
                     target_mesh_device.vertices_y,
                     target_mesh_device.vertices_z,
                     target_mesh_device.num_vertices, tgt_xyz)) {
        result.rmse_history.push_back(params.distance_threshold);
        return result;
    }
    bool tgt_has_normals = target_mesh_device.normals_x != nullptr;
    if (tgt_has_normals) {
        if (!downloadXYZ(target_mesh_device.normals_x,
                         target_mesh_device.normals_y,
                         target_mesh_device.normals_z,
                         target_mesh_device.num_vertices, tgt_n_xyz)) {
            tgt_has_normals = false;
        }
    }
    if (!downloadXYZ(source_cloud_device.points_x,
                     source_cloud_device.points_y,
                     source_cloud_device.points_z,
                     source_cloud_device.num_points, src_xyz)) {
        result.rmse_history.push_back(params.distance_threshold);
        return result;
    }
    bool src_has_normals = source_cloud_device.normals_x != nullptr;
    if (src_has_normals) {
        if (!downloadXYZ(source_cloud_device.normals_x,
                         source_cloud_device.normals_y,
                         source_cloud_device.normals_z,
                         source_cloud_device.num_points, src_n_xyz)) {
            src_has_normals = false;
        }
    }

    // --- Stage 2: Build Open3D Tensor PointClouds on CUDA:0
    o3c::Device device("CUDA:0");
    bool device_ok = false;
    try {
        device_ok = device.IsAvailable();
    } catch (...) {
        device_ok = false;
    }
    if (!device_ok) {
        device = o3c::Device("CPU:0");
    }

    o3tg::PointCloud target_pc = makeTensorPC(
        tgt_xyz, tgt_has_normals ? &tgt_n_xyz : nullptr, device);
    o3tg::PointCloud source_pc = makeTensorPC(
        src_xyz, src_has_normals ? &src_n_xyz : nullptr, device);

    // --- Stage 3: Configure ICP
    o3c::Tensor init_t = o3c::eigen_converter::EigenMatrixToTensor(
        toEigen(initial_transform));
    init_t = init_t.To(device, o3c::Float64);

    std::shared_ptr<o3treg::TransformationEstimation> estimation;
    if (params.use_point_to_plane && tgt_has_normals) {
        estimation = std::make_shared<
            o3treg::TransformationEstimationPointToPlane>();
    } else {
        estimation = std::make_shared<
            o3treg::TransformationEstimationPointToPoint>();
    }

    o3treg::ICPConvergenceCriteria criteria(
        static_cast<double>(params.convergence_threshold),
        static_cast<double>(params.convergence_threshold),
        params.max_iterations);

    // --- Stage 4: Run ICP
    o3treg::RegistrationResult o3r;
    try {
        o3r = o3treg::ICP(source_pc, target_pc,
                          static_cast<double>(params.distance_threshold),
                          init_t, *estimation, criteria);
    } catch (const std::exception& e) {
        (void)e;
        result.rmse_history.push_back(params.distance_threshold);
        return result;
    }

    // --- Stage 5: Translate result back
    Eigen::Matrix4d final_e =
        o3c::eigen_converter::TensorToEigenMatrixXd(
            o3r.transformation_.To(o3c::Device("CPU:0"), o3c::Float64));
    result.final_transform = fromEigen(final_e);
    result.final_rmse = static_cast<float>(o3r.inlier_rmse_);
    result.iterations = static_cast<int>(o3r.num_iterations_);
    result.converged = (o3r.fitness_ > 0.0);
    result.rmse_history.push_back(result.final_rmse);
    return result;
}

#endif  // MESHGPU_HAS_OPEN3D_BACKEND

}  // namespace meshgpu_open3d_backend
