#pragma once

#include <Eigen/Dense>
#include <cstdint>
#include <vector>
#include <chrono>

namespace ProbeCalib {

// ============================================================================
// Basic Types
// ============================================================================

using Matrix4f = Eigen::Matrix4f;
using Matrix3f = Eigen::Matrix3f;
using Vector3f = Eigen::Vector3f;
using Vector4f = Eigen::Vector4f;

// ============================================================================
// Pose Data Structure
// ============================================================================

/**
 * Represents a single pose measurement from the Atracsys tracker.
 * Contains the 4x4 transformation matrix and metadata.
 */
struct PoseData {
    Matrix4f transform;         // 4x4 transformation matrix (marker -> tracker)
    uint32_t geometry_id;       // Geometry ID (identifies which marker)
    uint32_t tracking_id;       // Tracking ID (persistent across frames)
    float registration_error;   // Registration error in mm
    uint64_t timestamp_us;      // Timestamp in microseconds
    bool is_valid;              // Whether the pose is valid

    PoseData()
        : transform(Matrix4f::Identity())
        , geometry_id(0)
        , tracking_id(0)
        , registration_error(0.0f)
        , timestamp_us(0)
        , is_valid(false)
    {}

    // Get rotation matrix (3x3)
    Matrix3f rotation() const {
        return transform.block<3,3>(0,0);
    }

    // Get translation vector
    Vector3f translation() const {
        return transform.block<3,1>(0,3);
    }

    // Set from rotation and translation
    void setFromRT(const Matrix3f& R, const Vector3f& t) {
        transform.setIdentity();
        transform.block<3,3>(0,0) = R;
        transform.block<3,1>(0,3) = t;
    }
};

// ============================================================================
// Calibration Result
// ============================================================================

/**
 * Result of the probe tip calibration.
 */
struct CalibrationResult {
    Vector3f tip_offset;        // Tip offset in marker local coordinates (mm)
    float residual_error;       // Residual error of the calibration (mm)
    int num_poses_used;         // Number of poses used in calibration
    bool is_valid;              // Whether calibration succeeded

    CalibrationResult()
        : tip_offset(Vector3f::Zero())
        , residual_error(0.0f)
        , num_poses_used(0)
        , is_valid(false)
    {}
};

// ============================================================================
// Tracker Status
// ============================================================================

enum class TrackerStatus {
    Disconnected,       // No tracker connected
    Connected,          // Tracker connected but not tracking
    Tracking,           // Actively tracking markers
    Error               // Error state
};

// ============================================================================
// Calibration State
// ============================================================================

enum class CalibrationState {
    Idle,               // Not calibrating
    Recording,          // Recording poses
    Computing,          // Computing calibration
    Complete,           // Calibration complete
    Failed              // Calibration failed
};

} // namespace ProbeCalib
