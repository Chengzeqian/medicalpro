#pragma once

#include <stdint.h>

#ifdef _WIN32
    #ifdef PROBECALIB_CAPI_EXPORTS
        #define PROBECALIB_API __declspec(dllexport)
    #else
        #define PROBECALIB_API __declspec(dllimport)
    #endif
#else
    #define PROBECALIB_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void* PC_PipelineHandle;

typedef struct PC_Vector3f {
    float x;
    float y;
    float z;
} PC_Vector3f;

typedef struct PC_TipPose {
    PC_Vector3f position;
    PC_Vector3f orientation;
    uint32_t geometry_id;
    uint64_t timestamp_us;
    float registration_error;
    // Non-zero when this tip pose was produced from a valid tracked marker pose.
    int is_valid;
} PC_TipPose;

// 4x4 marker pose transform stored in row-major order.
// Semantics match the internal marker -> tracker/world rigid transform matrix.
typedef struct PC_Matrix4x4f {
    float m[16];
} PC_Matrix4x4f;

// One tracked marker pose sample for the unified tracking calibration flow.
// registration_error is in millimeters.
// is_valid is non-zero only when the tracking sample is valid and can participate in calibration.
typedef struct PC_PoseSample {
    uint32_t geometry_id;
    uint64_t timestamp_us;
    float registration_error;
    int is_valid;
    PC_Matrix4x4f transform;
} PC_PoseSample;

// Calibration result for either flow.
// residual_error is the calibration residual in millimeters.
// is_valid is non-zero only when calibration succeeded and tip_offset can be used.
typedef struct PC_CalibrationResult {
    PC_Vector3f tip_offset;
    float residual_error;
    uint32_t geometry_id;
    uint32_t num_poses_used;
    int is_valid;
} PC_CalibrationResult;

// Session statistics for the unified tracking calibration flow.
// angular_coverage is in degrees.
// mean_registration_error is in millimeters.
typedef struct PC_CalibrationStats {
    uint32_t total_received;
    uint32_t total_accepted;
    uint32_t rejected_invalid;
    uint32_t rejected_high_error;
    uint32_t rejected_similar;
    float angular_coverage;
    float mean_registration_error;
} PC_CalibrationStats;

// Lifecycle
PROBECALIB_API PC_PipelineHandle PC_CreatePipeline(void);
PROBECALIB_API void PC_DestroyPipeline(PC_PipelineHandle handle);

// Legacy tracking-owned workflow.
// These APIs own tracker initialization and calibration capture inside ProbeCalibration.dll.
// Callers should use this path only when ProbeCalibration.dll is responsible for acquiring poses.
PROBECALIB_API int PC_InitializePipeline(PC_PipelineHandle handle, const char* geometry_path);
PROBECALIB_API void PC_ShutdownPipeline(PC_PipelineHandle handle);
PROBECALIB_API int PC_StartCalibration(PC_PipelineHandle handle);
PROBECALIB_API int PC_FinishCalibration(PC_PipelineHandle handle);
PROBECALIB_API int PC_SaveCalibration(PC_PipelineHandle handle, const char* calibration_path);
PROBECALIB_API int PC_LoadCalibration(PC_PipelineHandle handle, const char* calibration_path);
PROBECALIB_API int PC_GetTipPose(PC_PipelineHandle handle, PC_TipPose* out_tip);
PROBECALIB_API int PC_IsInitialized(PC_PipelineHandle handle);
PROBECALIB_API int PC_IsCalibrated(PC_PipelineHandle handle);

// Unified tracking workflow.
// This path does not own tracking acquisition. The caller configures geometry, pushes pose samples,
// and reads back calibration state/results. Do not mix these calls with the legacy tracking-owned
// recording flow for the same calibration session.
PROBECALIB_API int PC_ConfigureGeometry(PC_PipelineHandle handle,
                                        const char* geometry_path,
                                        uint32_t geometry_id);
PROBECALIB_API int PC_ResetCalibrationSession(PC_PipelineHandle handle);
PROBECALIB_API int PC_AddPoseSample(PC_PipelineHandle handle, const PC_PoseSample* sample);
PROBECALIB_API int PC_GetCalibrationResult(PC_PipelineHandle handle,
                                           PC_CalibrationResult* out_result);
PROBECALIB_API int PC_GetCalibrationStats(PC_PipelineHandle handle,
                                          PC_CalibrationStats* out_stats);

// Super-point collection API (usable for simulation without hardware tracker)
PROBECALIB_API int PC_CollectorSetVoxelSize(PC_PipelineHandle handle, float voxel_size_mm);
PROBECALIB_API int PC_CollectorSetMinSamplesPerVoxel(PC_PipelineHandle handle, uint32_t min_samples);
PROBECALIB_API int PC_CollectorSetCalibrationOffset(PC_PipelineHandle handle,
                                                    PC_Vector3f tip_offset,
                                                    uint32_t geometry_id);
PROBECALIB_API int PC_CollectorReset(PC_PipelineHandle handle);
PROBECALIB_API int PC_CollectorAddPoint(PC_PipelineHandle handle,
                                        float x,
                                        float y,
                                        float z,
                                        uint64_t timestamp_us);
PROBECALIB_API int PC_CollectorGetSuperPointCount(PC_PipelineHandle handle, uint32_t* out_count);
PROBECALIB_API int PC_CollectorExport(PC_PipelineHandle handle,
                                      float* out_x,
                                      float* out_y,
                                      float* out_z,
                                      uint32_t capacity,
                                      uint32_t* out_count);

// Error
PROBECALIB_API const char* PC_GetLastError(PC_PipelineHandle handle);

#ifdef __cplusplus
}
#endif
