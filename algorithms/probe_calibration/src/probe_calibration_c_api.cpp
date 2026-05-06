#include "probe_calibration_c_api.h"

#include "realtime_point_cloud_collector.h"
#include "realtime_transform.h"

#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <string>

namespace {

struct PipelineHandleImpl {
    ProbeCalib::ProbeTrackingPipeline pipeline;
    ProbeCalib::RealtimePointCloudCollector collector;
    std::string last_error;

    PipelineHandleImpl() : collector(1.0f) {}
};

PipelineHandleImpl* castHandle(PC_PipelineHandle handle) {
    return reinterpret_cast<PipelineHandleImpl*>(handle);
}

bool setError(PipelineHandleImpl* impl, const char* message) {
    if (impl) {
        impl->last_error = message ? message : "unknown error";
    }
    return false;
}

void clearError(PipelineHandleImpl* impl) {
    if (impl) {
        impl->last_error.clear();
    }
}

} // namespace

extern "C" {

PC_PipelineHandle PC_CreatePipeline(void) {
    auto* impl = new (std::nothrow) PipelineHandleImpl();
    return reinterpret_cast<PC_PipelineHandle>(impl);
}

void PC_DestroyPipeline(PC_PipelineHandle handle) {
    auto* impl = castHandle(handle);
    if (!impl) {
        return;
    }
    impl->pipeline.shutdown();
    delete impl;
}

int PC_InitializePipeline(PC_PipelineHandle handle, const char* geometry_path) {
    auto* impl = castHandle(handle);
    if (!impl) {
        return 0;
    }
    if (!geometry_path || geometry_path[0] == '\0') {
        return setError(impl, "geometry_path is empty") ? 1 : 0;
    }

    clearError(impl);
    if (!impl->pipeline.initialize(geometry_path)) {
        return setError(impl, "pipeline initialization failed") ? 1 : 0;
    }
    return 1;
}

void PC_ShutdownPipeline(PC_PipelineHandle handle) {
    auto* impl = castHandle(handle);
    if (!impl) {
        return;
    }
    impl->pipeline.shutdown();
    clearError(impl);
}

int PC_StartCalibration(PC_PipelineHandle handle) {
    auto* impl = castHandle(handle);
    if (!impl) {
        return 0;
    }
    if (!impl->pipeline.isInitialized()) {
        return setError(impl, "pipeline is not initialized") ? 1 : 0;
    }
    clearError(impl);
    impl->pipeline.startCalibration();
    return 1;
}

int PC_FinishCalibration(PC_PipelineHandle handle) {
    auto* impl = castHandle(handle);
    if (!impl) {
        return 0;
    }
    if (!impl->pipeline.isInitialized()) {
        return setError(impl, "pipeline is not initialized") ? 1 : 0;
    }
    clearError(impl);
    if (!impl->pipeline.finishCalibration()) {
        return setError(impl, "finishCalibration failed") ? 1 : 0;
    }

    // Mirror calibration into collector so GUI/demo can fuse points immediately.
    const auto result = impl->pipeline.getCalibrationResult();
    if (result.is_valid) {
        impl->collector.setCalibration(result, impl->pipeline.geometryId());
    }
    return 1;
}

int PC_SaveCalibration(PC_PipelineHandle handle, const char* calibration_path) {
    auto* impl = castHandle(handle);
    if (!impl) {
        return 0;
    }
    if (!calibration_path || calibration_path[0] == '\0') {
        return setError(impl, "calibration_path is empty") ? 1 : 0;
    }
    clearError(impl);
    if (!impl->pipeline.saveCalibration(calibration_path)) {
        return setError(impl, "saveCalibration failed") ? 1 : 0;
    }
    return 1;
}

int PC_LoadCalibration(PC_PipelineHandle handle, const char* calibration_path) {
    auto* impl = castHandle(handle);
    if (!impl) {
        return 0;
    }
    if (!calibration_path || calibration_path[0] == '\0') {
        return setError(impl, "calibration_path is empty") ? 1 : 0;
    }
    clearError(impl);
    if (!impl->pipeline.loadCalibration(calibration_path)) {
        return setError(impl, "loadCalibration failed") ? 1 : 0;
    }

    const auto result = impl->pipeline.getCalibrationResult();
    if (result.is_valid) {
        impl->collector.setCalibration(result, impl->pipeline.geometryId());
    }
    return 1;
}

int PC_GetTipPose(PC_PipelineHandle handle, PC_TipPose* out_tip) {
    auto* impl = castHandle(handle);
    if (!impl || !out_tip) {
        return 0;
    }

    ProbeCalib::TipPosition tip;
    if (!impl->pipeline.getTipPosition(tip)) {
        return setError(impl, "no valid tip pose available") ? 1 : 0;
    }

    out_tip->position.x = tip.position.x();
    out_tip->position.y = tip.position.y();
    out_tip->position.z = tip.position.z();
    out_tip->orientation.x = tip.orientation.x();
    out_tip->orientation.y = tip.orientation.y();
    out_tip->orientation.z = tip.orientation.z();
    out_tip->geometry_id = tip.geometry_id;
    out_tip->timestamp_us = tip.timestamp_us;
    out_tip->registration_error = tip.registration_error;
    out_tip->is_valid = tip.is_valid ? 1 : 0;
    clearError(impl);
    return 1;
}

int PC_IsInitialized(PC_PipelineHandle handle) {
    auto* impl = castHandle(handle);
    return (impl && impl->pipeline.isInitialized()) ? 1 : 0;
}

int PC_IsCalibrated(PC_PipelineHandle handle) {
    auto* impl = castHandle(handle);
    return (impl && impl->pipeline.isCalibrated()) ? 1 : 0;
}

int PC_ConfigureGeometry(PC_PipelineHandle handle, const char* geometry_path, uint32_t geometry_id) {
    auto* impl = castHandle(handle);
    if (!impl) {
        return 0;
    }
    if (!geometry_path || geometry_path[0] == '\0') {
        return setError(impl, "geometry_path is empty") ? 1 : 0;
    }

    clearError(impl);
    if (!impl->pipeline.configureGeometry(geometry_path, geometry_id)) {
        return setError(impl, "configureGeometry failed") ? 1 : 0;
    }
    return 1;
}

int PC_ResetCalibrationSession(PC_PipelineHandle handle) {
    auto* impl = castHandle(handle);
    if (!impl) {
        return 0;
    }

    impl->pipeline.resetCalibrationSession();
    clearError(impl);
    return 1;
}

int PC_AddPoseSample(PC_PipelineHandle handle, const PC_PoseSample* sample) {
    auto* impl = castHandle(handle);
    if (!impl || !sample) {
        return 0;
    }

    ProbeCalib::PoseData pose;
    pose.geometry_id = sample->geometry_id;
    pose.timestamp_us = sample->timestamp_us;
    pose.registration_error = sample->registration_error;
    pose.is_valid = sample->is_valid != 0;
    pose.tracking_id = 0;

    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            pose.transform(row, col) = sample->transform.m[row * 4 + col];
        }
    }

    clearError(impl);
    if (!impl->pipeline.addPoseSample(pose)) {
        return setError(impl, "pose sample was rejected") ? 1 : 0;
    }
    return 1;
}

int PC_GetCalibrationResult(PC_PipelineHandle handle, PC_CalibrationResult* out_result) {
    auto* impl = castHandle(handle);
    if (!impl || !out_result) {
        return 0;
    }

    const auto result = impl->pipeline.getCalibrationResult();
    out_result->tip_offset.x = result.tip_offset.x();
    out_result->tip_offset.y = result.tip_offset.y();
    out_result->tip_offset.z = result.tip_offset.z();
    out_result->residual_error = result.residual_error;
    out_result->geometry_id = impl->pipeline.geometryId();
    out_result->num_poses_used = result.num_poses_used >= 0
        ? static_cast<uint32_t>(result.num_poses_used)
        : 0u;
    out_result->is_valid = result.is_valid ? 1 : 0;
    clearError(impl);
    return 1;
}

int PC_GetCalibrationStats(PC_PipelineHandle handle, PC_CalibrationStats* out_stats) {
    auto* impl = castHandle(handle);
    if (!impl || !out_stats) {
        return 0;
    }

    const auto stats = impl->pipeline.getRecordingStats();
    out_stats->total_received = static_cast<uint32_t>(stats.total_received);
    out_stats->total_accepted = static_cast<uint32_t>(stats.total_accepted);
    out_stats->rejected_invalid = static_cast<uint32_t>(stats.rejected_invalid);
    out_stats->rejected_high_error = static_cast<uint32_t>(stats.rejected_high_error);
    out_stats->rejected_similar = static_cast<uint32_t>(stats.rejected_similar);
    out_stats->angular_coverage = stats.angular_coverage;
    out_stats->mean_registration_error = stats.mean_registration_error;
    clearError(impl);
    return 1;
}

int PC_CollectorSetVoxelSize(PC_PipelineHandle handle, float voxel_size_mm) {
    auto* impl = castHandle(handle);
    if (!impl) {
        return 0;
    }
    if (voxel_size_mm <= 0.0f) {
        return setError(impl, "voxel_size_mm must be > 0") ? 1 : 0;
    }
    impl->collector.setVoxelSize(voxel_size_mm);
    clearError(impl);
    return 1;
}

int PC_CollectorSetMinSamplesPerVoxel(PC_PipelineHandle handle, uint32_t min_samples) {
    auto* impl = castHandle(handle);
    if (!impl) {
        return 0;
    }
    if (min_samples == 0u) {
        return setError(impl, "min_samples must be >= 1") ? 1 : 0;
    }
    if (min_samples > static_cast<uint32_t>(std::numeric_limits<int>::max())) {
        return setError(impl, "min_samples is too large") ? 1 : 0;
    }
    impl->collector.setMinSamplesPerVoxel(static_cast<int>(min_samples));
    clearError(impl);
    return 1;
}

int PC_CollectorSetCalibrationOffset(PC_PipelineHandle handle,
                                     PC_Vector3f tip_offset,
                                     uint32_t geometry_id) {
    auto* impl = castHandle(handle);
    if (!impl) {
        return 0;
    }
    impl->collector.setCalibration(
        ProbeCalib::Vector3f(tip_offset.x, tip_offset.y, tip_offset.z), geometry_id);
    clearError(impl);
    return 1;
}

int PC_CollectorReset(PC_PipelineHandle handle) {
    auto* impl = castHandle(handle);
    if (!impl) {
        return 0;
    }
    impl->collector.reset();
    clearError(impl);
    return 1;
}

int PC_CollectorAddPoint(PC_PipelineHandle handle,
                         float x,
                         float y,
                         float z,
                         uint64_t timestamp_us) {
    auto* impl = castHandle(handle);
    if (!impl) {
        return 0;
    }
    impl->collector.addPoint(x, y, z, timestamp_us);
    clearError(impl);
    return 1;
}

int PC_CollectorGetSuperPointCount(PC_PipelineHandle handle, uint32_t* out_count) {
    auto* impl = castHandle(handle);
    if (!impl || !out_count) {
        return 0;
    }
    *out_count = static_cast<uint32_t>(impl->collector.getSuperPointCount());
    clearError(impl);
    return 1;
}

int PC_CollectorExport(PC_PipelineHandle handle,
                       float* out_x,
                       float* out_y,
                       float* out_z,
                       uint32_t capacity,
                       uint32_t* out_count) {
    auto* impl = castHandle(handle);
    if (!impl || !out_count) {
        return 0;
    }

    ProbeCalib::ExportedPointCloud cloud;
    impl->collector.exportForGPU(cloud);
    *out_count = cloud.num_points;

    // Size query mode.
    if (!out_x || !out_y || !out_z) {
        clearError(impl);
        return 1;
    }

    if (capacity < cloud.num_points) {
        return setError(impl, "capacity is smaller than point count") ? 1 : 0;
    }

    if (cloud.num_points > 0) {
        std::memcpy(out_x, cloud.points_x.data(), cloud.num_points * sizeof(float));
        std::memcpy(out_y, cloud.points_y.data(), cloud.num_points * sizeof(float));
        std::memcpy(out_z, cloud.points_z.data(), cloud.num_points * sizeof(float));
    }
    clearError(impl);
    return 1;
}

const char* PC_GetLastError(PC_PipelineHandle handle) {
    auto* impl = castHandle(handle);
    if (!impl) {
        return "invalid handle";
    }
    if (impl->last_error.empty()) {
        return "";
    }
    return impl->last_error.c_str();
}

} // extern "C"
