#pragma once

/**
 * @file realtime_transform.h
 * @brief Real-time Tip Position Transform Module
 *
 * This module applies the calibrated tip offset to real-time tracking data
 * to compute the actual probe tip position in world coordinates.
 *
 * Operation:
 *   P_tip_world = T_marker * P_tip_local
 *
 * Where:
 *   T_marker is the 4x4 marker transformation from the tracker
 *   P_tip_local is the calibrated tip offset (fixed)
 *   P_tip_world is the real-time tip position in tracker coordinates
 */

#include "types.h"
#include "atracsys_tracker.h"
#include "calibration_recorder.h"
#include "tip_calibration_solver.h"
#include <functional>
#include <mutex>
#include <atomic>
#include <memory>
#include <string>

namespace ProbeCalib {

/**
 * @struct TipPosition
 * @brief Represents the computed tip position with metadata
 */
struct TipPosition {
    Vector3f position;              // Tip position in world coordinates (mm)
    Vector3f orientation;           // Tip orientation (Z-axis of marker, normalized)
    uint32_t geometry_id;           // Source geometry ID
    uint64_t timestamp_us;          // Timestamp
    float registration_error;       // Original marker registration error
    bool is_valid;                  // Whether the position is valid

    TipPosition()
        : position(Vector3f::Zero())
        , orientation(Vector3f::UnitZ())
        , geometry_id(0)
        , timestamp_us(0)
        , registration_error(0.0f)
        , is_valid(false)
    {}
};

/**
 * @class RealtimeTransform
 * @brief Applies calibrated tip offset to real-time tracking data
 */
class RealtimeTransform {
public:
    // Callback type: called when a new tip position is computed
    using TipCallback = std::function<void(const TipPosition& tip)>;

    RealtimeTransform();
    ~RealtimeTransform() = default;

    // Disable copy
    RealtimeTransform(const RealtimeTransform&) = delete;
    RealtimeTransform& operator=(const RealtimeTransform&) = delete;

    // ========================================================================
    // Calibration Setup
    // ========================================================================

    /**
     * Set the calibrated tip offset.
     * @param offset Tip offset in marker local coordinates (mm)
     * @param geometry_id The geometry ID this offset applies to
     */
    void setTipOffset(const Vector3f& offset, uint32_t geometry_id);

    /**
     * Set tip offset from calibration result.
     */
    void setTipOffset(const CalibrationResult& result, uint32_t geometry_id);

    /**
     * Get the current tip offset.
     */
    Vector3f getTipOffset() const { return tip_offset_; }

    /**
     * Check if tip offset is calibrated.
     */
    bool isCalibrated() const { return is_calibrated_.load(); }

    // ========================================================================
    // Real-time Transform
    // ========================================================================

    /**
     * Compute tip position from a pose.
     * This is the core function: P_tip = T_marker * offset
     *
     * @param pose The marker pose from the tracker
     * @return Computed tip position
     */
    TipPosition computeTipPosition(const PoseData& pose);

    /**
     * Process a pose and invoke callback if valid.
     * Convenience method for use with tracker callback.
     */
    void processPose(const PoseData& pose);

    // ========================================================================
    // Callbacks
    // ========================================================================

    /**
     * Set callback for tip position updates.
     */
    void setTipCallback(TipCallback callback);

    // ========================================================================
    // Statistics
    // ========================================================================

    /**
     * Get the latest computed tip position.
     * Thread-safe.
     */
    TipPosition getLatestTipPosition() const;

    /**
     * Get total number of tip positions computed.
     */
    uint64_t getTotalComputed() const { return total_computed_.load(); }

private:
    // Tip offset calibration
    Vector3f tip_offset_;
    uint32_t target_geometry_id_;
    std::atomic<bool> is_calibrated_;

    // Latest data (thread-safe)
    mutable std::mutex data_mutex_;
    TipPosition latest_tip_;
    std::atomic<uint64_t> total_computed_;

    // Callback
    TipCallback tip_callback_;
};

/**
 * @class ProbeTrackingPipeline
 * @brief High-level class that combines tracker, recorder, solver, and transform
 *
 * This provides a simple interface for the complete probe calibration workflow.
 */
class ProbeTrackingPipeline {
public:
    ProbeTrackingPipeline();
    ~ProbeTrackingPipeline();

    // ========================================================================
    // Initialization
    // ========================================================================

    /**
     * Initialize the tracking pipeline.
     * @param geometry_path Path to the probe geometry .ini file
     * @return true if successful
     */
    bool initialize(const std::string& geometry_path);

    /**
     * Shutdown the pipeline.
     */
    void shutdown();

    // ========================================================================
    // Calibration Workflow
    // ========================================================================

    /**
     * Start calibration recording.
     * Doctor should pivot the probe tip on a fixed point.
     */
    void startCalibration();

    /**
     * Stop calibration recording and compute tip offset.
     * @return true if calibration succeeded
     */
    bool finishCalibration();

    /**
     * Get calibration result.
     */
    CalibrationResult getCalibrationResult() const { return calibration_result_; }

    /**
     * Save calibration to file.
     */
    bool saveCalibration(const std::string& path);

    /**
     * Load calibration from file.
     */
    bool loadCalibration(const std::string& path);

    // ========================================================================
    // Real-time Tracking
    // ========================================================================

    /**
     * Get the current probe tip position.
     * @param tip Output tip position
     * @return true if valid tip position available
     */
    bool getTipPosition(TipPosition& tip);

    /**
     * Set callback for real-time tip updates.
     */
    void setTipCallback(RealtimeTransform::TipCallback callback);

    // ========================================================================
    // Status
    // ========================================================================

    bool isInitialized() const { return is_initialized_; }
    bool isCalibrated() const { return transform_.isCalibrated(); }
    bool isTracking() const { return tracker_ ? tracker_->isTracking() : false; }
    CalibrationState getCalibrationState() const { return recorder_.getState(); }

private:
    // Internal callback for tracker poses
    void onPoseReceived(const PoseData& pose);

    // Components
    std::unique_ptr<AtracsysTracker> tracker_;
    CalibrationRecorder recorder_;
    TipCalibrationSolver solver_;
    RealtimeTransform transform_;

    // State
    bool is_initialized_;
    uint32_t geometry_id_;
    CalibrationResult calibration_result_;
};

} // namespace ProbeCalib
