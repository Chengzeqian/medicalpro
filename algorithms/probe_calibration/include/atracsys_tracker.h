#pragma once

/**
 * @file atracsys_tracker.h
 * @brief Atracsys SDK Data Acquisition Module
 *
 * This module interfaces with the Atracsys fusionTrack/spryTrack SDK
 * to acquire marker pose data at 60Hz.
 *
 * Features:
 * - Initialize and connect to the tracker device
 * - Load probe geometry from .ini file
 * - Continuously receive 4x4 transformation matrices
 * - Access geometry ID and quality metrics
 */

#include "types.h"
#include <functional>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include <vector>

// Forward declarations for Atracsys SDK types
struct ftkLibraryImp;
typedef ftkLibraryImp* ftkLibrary;
struct ftkFrameQuery;

namespace ProbeCalib {

/**
 * @class AtracsysTracker
 * @brief Wrapper for Atracsys fusionTrack/spryTrack SDK
 *
 * This class handles all communication with the Atracsys tracker device.
 * It provides a callback mechanism for real-time pose updates.
 */
class AtracsysTracker {
public:
    // Callback type: called when a new pose is available
    using PoseCallback = std::function<void(const PoseData& pose)>;

    AtracsysTracker();
    ~AtracsysTracker();

    // Disable copy
    AtracsysTracker(const AtracsysTracker&) = delete;
    AtracsysTracker& operator=(const AtracsysTracker&) = delete;

    // ========================================================================
    // Initialization
    // ========================================================================

    /**
     * Initialize the SDK and connect to the first available device.
     * @param config_file Optional JSON config file for network settings
     * @return true if successful
     */
    bool initialize(const std::string& config_file = "");

    /**
     * Load a geometry file for the probe marker.
     * @param geometry_path Path to the .ini geometry file
     * @return true if successful
     */
    bool loadGeometry(const std::string& geometry_path);

    /**
     * Shutdown the tracker and release resources.
     */
    void shutdown();

    // ========================================================================
    // Tracking Control
    // ========================================================================

    /**
     * Start the tracking thread.
     * This will begin acquiring frames at ~60Hz.
     * @return true if tracking started successfully
     */
    bool startTracking();

    /**
     * Stop the tracking thread.
     */
    void stopTracking();

    /**
     * Check if currently tracking.
     */
    bool isTracking() const { return is_tracking_.load(); }

    // ========================================================================
    // Data Access
    // ========================================================================

    /**
     * Get the latest pose for a specific geometry ID.
     * Thread-safe.
     * @param geometry_id The geometry ID to query
     * @param pose Output pose data
     * @return true if a valid pose was found
     */
    bool getLatestPose(uint32_t geometry_id, PoseData& pose);

    /**
     * Get all currently tracked markers.
     * Thread-safe.
     * @return Vector of pose data for all visible markers
     */
    std::vector<PoseData> getAllPoses();

    /**
     * Set a callback to be called when new pose data is available.
     * @param callback The callback function
     */
    void setPoseCallback(PoseCallback callback);

    // ========================================================================
    // Status
    // ========================================================================

    /**
     * Get current tracker status.
     */
    TrackerStatus getStatus() const { return status_.load(); }

    /**
     * Get device serial number.
     */
    uint64_t getDeviceSerial() const { return device_serial_; }

    /**
     * Get device type name.
     */
    std::string getDeviceTypeName() const;

private:
    // Tracking thread function
    void trackingLoop();

    // Process a single frame
    void processFrame();

    // Convert ftkMarker to PoseData
    void convertMarkerToPose(const void* marker, PoseData& pose);

    // SDK handles
    ftkLibrary lib_;
    ftkFrameQuery* frame_;
    uint64_t device_serial_;
    int device_type_;

    // Thread control
    std::thread tracking_thread_;
    std::atomic<bool> is_tracking_;
    std::atomic<bool> should_stop_;
    std::atomic<TrackerStatus> status_;

    // Data storage (thread-safe)
    std::mutex pose_mutex_;
    std::vector<PoseData> current_poses_;
    PoseCallback pose_callback_;

    // Configuration
    std::vector<uint32_t> loaded_geometries_;
};

} // namespace ProbeCalib
