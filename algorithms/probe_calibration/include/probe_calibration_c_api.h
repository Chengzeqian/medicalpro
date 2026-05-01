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
    int is_valid;
} PC_TipPose;

// Lifecycle
PROBECALIB_API PC_PipelineHandle PC_CreatePipeline(void);
PROBECALIB_API void PC_DestroyPipeline(PC_PipelineHandle handle);

// Tracking + calibration workflow
PROBECALIB_API int PC_InitializePipeline(PC_PipelineHandle handle, const char* geometry_path);
PROBECALIB_API void PC_ShutdownPipeline(PC_PipelineHandle handle);
PROBECALIB_API int PC_StartCalibration(PC_PipelineHandle handle);
PROBECALIB_API int PC_FinishCalibration(PC_PipelineHandle handle);
PROBECALIB_API int PC_SaveCalibration(PC_PipelineHandle handle, const char* calibration_path);
PROBECALIB_API int PC_LoadCalibration(PC_PipelineHandle handle, const char* calibration_path);
PROBECALIB_API int PC_GetTipPose(PC_PipelineHandle handle, PC_TipPose* out_tip);
PROBECALIB_API int PC_IsInitialized(PC_PipelineHandle handle);
PROBECALIB_API int PC_IsCalibrated(PC_PipelineHandle handle);

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
