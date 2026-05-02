#pragma once

/**
 * @file calibration_recorder.h
 * @brief Calibration Pose Recorder Module
 *
 * This module manages the recording of probe poses during the pivot calibration process.
 *
 * Operation Flow:
 * 1. Enter "calibration mode" - start recording
 * 2. Doctor pivots the probe tip on a fixed point
 * 3. Each frame's 4x4 transformation matrix is stored in a buffer
 * 4. After collecting enough poses (500-1000 frames), stop recording
 *
 * Quality Control:
 * - Filters out invalid poses (high registration error)
 * - Ensures sufficient pose diversity (angular coverage)
 * - Warns if poses are too similar (not enough rotation)
 */

#include "types.h"
#include <deque>
#include <mutex>
#include <functional>

namespace ProbeCalib {

/**
 * @struct RecordingConfig
 * @brief Configuration for pose recording
 */
struct RecordingConfig {
    uint32_t target_geometry_id = 0;        // Geometry ID to record (0 = any)
    size_t min_poses = 500;                 // Minimum poses required
    size_t max_poses = 2000;                // Maximum poses to store
    float max_registration_error = 1.0f;    // Max registration error to accept (mm)
    float min_rotation_change = 2.0f;       // Min rotation change between poses (degrees)

    RecordingConfig() = default;
};

/**
 * @struct RecordingStats
 * @brief Statistics about the recording session
 */
struct RecordingStats {
    size_t total_received = 0;              // Total poses received
    size_t total_accepted = 0;              // Poses accepted into buffer
    size_t rejected_invalid = 0;            // Rejected: invalid marker data
    size_t rejected_high_error = 0;         // Rejected: high registration error
    size_t rejected_similar = 0;            // Rejected: too similar to previous
    float angular_coverage = 0.0f;          // Estimated angular coverage (degrees)
    float mean_registration_error = 0.0f;   // Mean registration error of accepted poses
};

/**
 * @class CalibrationRecorder
 * @brief Manages pose recording for probe tip calibration
 */
class CalibrationRecorder {
public:
    // Callback type: called when recording status changes
    using StatusCallback = std::function<void(CalibrationState state, const RecordingStats& stats)>;

    CalibrationRecorder();
    ~CalibrationRecorder() = default;

    // Disable copy
    CalibrationRecorder(const CalibrationRecorder&) = delete;
    CalibrationRecorder& operator=(const CalibrationRecorder&) = delete;

    // ========================================================================
    // Recording Control
    // ========================================================================

    /**
     * Start recording poses.
     * @param config Recording configuration
     */
    void startRecording(const RecordingConfig& config = RecordingConfig());

    /**
     * Stop recording and finalize the buffer.
     * @return true if enough poses were collected
     */
    bool stopRecording();

    /**
     * Clear all recorded poses.
     */
    void clear();

    /**
     * Check if currently recording.
     */
    bool isRecording() const { return state_ == CalibrationState::Recording; }

    /**
     * Get current state.
     */
    CalibrationState getState() const { return state_; }

    // ========================================================================
    // Pose Input
    // ========================================================================

    /**
     * Add a new pose to the recording buffer.
     * Should be called from the tracker callback.
     * Thread-safe.
     *
     * @param pose The pose data from the tracker
     * @return true if pose was accepted
     */
    bool addPose(const PoseData& pose);

    // ========================================================================
    // Data Access
    // ========================================================================

    /**
     * Get all recorded poses.
     * Thread-safe.
     */
    std::vector<PoseData> getRecordedPoses() const;

    /**
     * Get the number of recorded poses.
     */
    size_t getPoseCount() const;

    /**
     * Get recording statistics.
     */
    RecordingStats getStats() const;

    /**
     * Check if enough poses have been collected.
     */
    bool hasEnoughPoses() const;

    // ========================================================================
    // Callbacks
    // ========================================================================

    /**
     * Set a callback for status changes.
     */
    void setStatusCallback(StatusCallback callback);

private:
    // Check if a new pose is sufficiently different from the last
    bool isPoseSufficientlyDifferent(const PoseData& pose) const;

    // Calculate rotation angle between two poses (degrees)
    float calculateRotationAngle(const Matrix3f& R1, const Matrix3f& R2) const;

    // Update angular coverage estimate
    void updateAngularCoverage();

    // State
    CalibrationState state_;
    RecordingConfig config_;
    RecordingStats stats_;

    // Pose buffer
    mutable std::mutex buffer_mutex_;
    std::deque<PoseData> pose_buffer_;

    // Callback
    StatusCallback status_callback_;
};

} // namespace ProbeCalib
