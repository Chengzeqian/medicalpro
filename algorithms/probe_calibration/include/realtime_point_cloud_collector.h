#pragma once

/**
 * @file realtime_point_cloud_collector.h
 * @brief Real-time Point Cloud Collection with Voxel Fusion
 *
 * This module is the bridge between probe tracking and mesh registration.
 * It collects probe tip positions at 60Hz, applies voxel fusion for noise
 * reduction, and exports "Super Points" for GPU registration.
 *
 * Data Flow:
 *   Atracsys Tracker (60Hz)
 *       -> RealtimeTransform (calibrated tip position)
 *       -> VoxelFuser (noise reduction, deduplication)
 *       -> Super Points (for MeshGPU registration)
 */

#include "types.h"
#include "voxel_fuser.h"
#include "realtime_transform.h"
#include "atracsys_tracker.h"
#include <functional>
#include <mutex>
#include <atomic>
#include <memory>
#include <string>
#include <chrono>
#include <iostream>

namespace ProbeCalib {

// ============================================================================
// Collection Statistics
// ============================================================================

struct CollectionStats {
    uint64_t raw_points_received;       // Total points from tracker (60Hz)
    uint64_t new_voxels_created;        // Number of unique voxels
    uint64_t duplicate_points_merged;   // Points merged into existing voxels
    float compression_ratio;            // raw_points / unique_voxels
    float collection_rate_hz;           // Current point acquisition rate
    std::chrono::milliseconds elapsed_time;

    CollectionStats()
        : raw_points_received(0)
        , new_voxels_created(0)
        , duplicate_points_merged(0)
        , compression_ratio(0.0f)
        , collection_rate_hz(0.0f)
        , elapsed_time(0)
    {}
};

// ============================================================================
// Export Format for MeshGPU
// ============================================================================

/**
 * Structure matching MeshGPU's SourcePointCloud for seamless integration
 */
struct ExportedPointCloud {
    std::vector<float> points_x;
    std::vector<float> points_y;
    std::vector<float> points_z;
    uint32_t num_points;

    ExportedPointCloud() : num_points(0) {}

    void clear() {
        points_x.clear();
        points_y.clear();
        points_z.clear();
        num_points = 0;
    }

    void reserve(size_t n) {
        points_x.reserve(n);
        points_y.reserve(n);
        points_z.reserve(n);
    }
};

// ============================================================================
// RealtimePointCloudCollector
// ============================================================================

/**
 * @class RealtimePointCloudCollector
 * @brief Collects probe tip positions and produces fused Super Points
 *
 * Usage:
 *   1. Create collector with voxel cell size (default 1mm)
 *   2. Call setCalibration() with tip offset
 *   3. Call startCollection() to begin
 *   4. Points are automatically collected via tracker callback
 *   5. Call exportForGPU() to get fused points for registration
 *   6. Call stopCollection() when done
 */
class RealtimePointCloudCollector {
public:
    // Callback when new Super Point is created (not every raw point)
    using NewPointCallback = std::function<void(const Vector3f& position, int voxel_count)>;

    // Callback when export is ready
    using ExportReadyCallback = std::function<void(const ExportedPointCloud& cloud)>;

    /**
     * Constructor
     * @param voxel_size Voxel cell size in mm (default 1.0mm)
     */
    explicit RealtimePointCloudCollector(float voxel_size = 1.0f);
    ~RealtimePointCloudCollector();

    // Disable copy
    RealtimePointCloudCollector(const RealtimePointCloudCollector&) = delete;
    RealtimePointCloudCollector& operator=(const RealtimePointCloudCollector&) = delete;

    // ========================================================================
    // Configuration
    // ========================================================================

    /**
     * Set the calibrated tip offset.
     * Must be called before starting collection.
     */
    void setCalibration(const CalibrationResult& result, uint32_t geometry_id);

    /**
     * Set the calibrated tip offset directly.
     */
    void setCalibration(const Vector3f& tip_offset, uint32_t geometry_id);

    /**
     * Set minimum samples per voxel for export.
     * Voxels with fewer samples are considered noise and filtered out.
     */
    void setMinSamplesPerVoxel(int min_samples);

    /**
     * Set the voxel cell size.
     * Only effective before collection starts or after reset.
     */
    void setVoxelSize(float size_mm);

    /**
     * Get current voxel size.
     */
    float getVoxelSize() const { return voxel_fuser_.getCellSize(); }

    // ========================================================================
    // Collection Control
    // ========================================================================

    /**
     * Connect to a tracker and start collection.
     * This internally connects to the tracker's pose callback.
     * @param tracker Pointer to initialized AtracsysTracker
     * @return true if successful
     */
    bool startCollection(AtracsysTracker* tracker);

    /**
     * Start collection using ProbeTrackingPipeline.
     * More convenient if using the full pipeline.
     */
    bool startCollection(ProbeTrackingPipeline* pipeline);

    /**
     * Stop collection (does not clear data).
     */
    void stopCollection();

    /**
     * Clear all collected data and reset statistics.
     */
    void reset();

    /**
     * Check if collection is active.
     */
    bool isCollecting() const { return is_collecting_.load(); }

    // ========================================================================
    // Manual Point Addition
    // ========================================================================

    /**
     * Manually add a point (for testing or non-Atracsys sources).
     * @param position Point position in world coordinates
     * @param timestamp Optional timestamp in microseconds
     * @return true if this creates a new voxel
     */
    bool addPoint(const Vector3f& position, uint64_t timestamp = 0);

    /**
     * Add point from raw coordinates.
     */
    bool addPoint(float x, float y, float z, uint64_t timestamp = 0);

    // ========================================================================
    // Export for MeshGPU
    // ========================================================================

    /**
     * Export fused points for GPU registration.
     * This is the main output - "Super Points" ready for ICP.
     *
     * @param cloud Output structure matching MeshGPU format
     * @return Number of points exported
     */
    size_t exportForGPU(ExportedPointCloud& cloud) const;

    /**
     * Export as vector of Eigen vectors.
     */
    std::vector<Vector3f> exportAsVectors() const;

    /**
     * Export with per-point sample counts (for weighted ICP).
     */
    std::vector<std::pair<Vector3f, int>> exportWithWeights() const;

    /**
     * Get raw pointer arrays for direct GPU upload.
     * The data is valid until next modification.
     */
    size_t exportToArrays(std::vector<float>& x,
                          std::vector<float>& y,
                          std::vector<float>& z) const;

    // ========================================================================
    // Callbacks
    // ========================================================================

    /**
     * Set callback for new Super Points.
     * Called only when a new voxel is created, not for every raw point.
     */
    void setNewPointCallback(NewPointCallback callback);

    /**
     * Set callback when export is ready.
     * Called after stopCollection() with the final point cloud.
     */
    void setExportReadyCallback(ExportReadyCallback callback);

    // ========================================================================
    // Statistics
    // ========================================================================

    /**
     * Get collection statistics.
     */
    CollectionStats getStats() const;

    /**
     * Get number of unique Super Points (fused voxels).
     */
    size_t getSuperPointCount() const { return voxel_fuser_.getVoxelCount(); }

    /**
     * Get total raw points received.
     */
    size_t getRawPointCount() const { return raw_points_received_.load(); }

    /**
     * Get compression ratio (raw points / super points).
     */
    float getCompressionRatio() const { return voxel_fuser_.getCompressionRatio(); }

    /**
     * Get bounding box of collected points.
     */
    void getBoundingBox(Vector3f& min_pt, Vector3f& max_pt) const;

    /**
     * Print statistics to stdout.
     */
    void printStats() const;

private:
    // Internal callback for tip position updates
    void onTipPosition(const TipPosition& tip);

    // Update statistics
    void updateStats();

    // Components
    VoxelFuser voxel_fuser_;
    RealtimeTransform transform_;

    // State
    std::atomic<bool> is_collecting_;
    std::atomic<bool> is_calibrated_;

    // Statistics
    std::atomic<uint64_t> raw_points_received_;
    std::atomic<uint64_t> new_voxels_created_;
    std::chrono::steady_clock::time_point start_time_;
    mutable std::mutex stats_mutex_;

    // Callbacks
    NewPointCallback new_point_callback_;
    ExportReadyCallback export_ready_callback_;
    mutable std::mutex callback_mutex_;

    // Configuration
    uint32_t target_geometry_id_;
    int min_samples_per_voxel_;
};

// ============================================================================
// Implementation (header-only for simplicity)
// ============================================================================

inline RealtimePointCloudCollector::RealtimePointCloudCollector(float voxel_size)
    : voxel_fuser_(voxel_size)
    , is_collecting_(false)
    , is_calibrated_(false)
    , raw_points_received_(0)
    , new_voxels_created_(0)
    , target_geometry_id_(0)
    , min_samples_per_voxel_(1)
{
}

inline RealtimePointCloudCollector::~RealtimePointCloudCollector() {
    stopCollection();
}

inline void RealtimePointCloudCollector::setCalibration(const CalibrationResult& result,
                                                        uint32_t geometry_id) {
    if (result.is_valid) {
        transform_.setTipOffset(result, geometry_id);
        target_geometry_id_ = geometry_id;
        is_calibrated_ = true;
    }
}

inline void RealtimePointCloudCollector::setCalibration(const Vector3f& tip_offset,
                                                        uint32_t geometry_id) {
    transform_.setTipOffset(tip_offset, geometry_id);
    target_geometry_id_ = geometry_id;
    is_calibrated_ = true;
}

inline void RealtimePointCloudCollector::setMinSamplesPerVoxel(int min_samples) {
    min_samples_per_voxel_ = min_samples;
    voxel_fuser_.setMinSampleCount(min_samples);
}

inline void RealtimePointCloudCollector::setVoxelSize(float size_mm) {
    if (!is_collecting_.load()) {
        voxel_fuser_ = VoxelFuser(size_mm);
        voxel_fuser_.setMinSampleCount(min_samples_per_voxel_);
    }
}

inline bool RealtimePointCloudCollector::startCollection(AtracsysTracker* tracker) {
    if (!tracker) {
        std::cerr << "RealtimePointCloudCollector: Tracker is null" << std::endl;
        return false;
    }

    if (!is_calibrated_.load()) {
        std::cerr << "RealtimePointCloudCollector: Not calibrated" << std::endl;
        return false;
    }

    // Set up callback chain: Tracker -> Transform -> VoxelFuser
    tracker->setPoseCallback([this](const PoseData& pose) {
        if (pose.is_valid && pose.geometry_id == target_geometry_id_) {
            TipPosition tip = transform_.computeTipPosition(pose);
            if (tip.is_valid) {
                onTipPosition(tip);
            }
        }
    });

    start_time_ = std::chrono::steady_clock::now();
    is_collecting_ = true;

    std::cout << "RealtimePointCloudCollector: Started (voxel size: "
              << voxel_fuser_.getCellSize() << "mm)" << std::endl;

    return true;
}

inline bool RealtimePointCloudCollector::startCollection(ProbeTrackingPipeline* pipeline) {
    if (!pipeline) {
        std::cerr << "RealtimePointCloudCollector: Pipeline is null" << std::endl;
        return false;
    }

    if (!pipeline->isCalibrated()) {
        std::cerr << "RealtimePointCloudCollector: Pipeline not calibrated" << std::endl;
        return false;
    }

    // Get calibration from pipeline
    CalibrationResult result = pipeline->getCalibrationResult();
    // Note: We assume the geometry ID is stored elsewhere or passed separately

    // Set up callback: Pipeline -> VoxelFuser
    pipeline->setTipCallback([this](const TipPosition& tip) {
        if (tip.is_valid) {
            onTipPosition(tip);
        }
    });

    start_time_ = std::chrono::steady_clock::now();
    is_collecting_ = true;

    std::cout << "RealtimePointCloudCollector: Started via pipeline" << std::endl;

    return true;
}

inline void RealtimePointCloudCollector::stopCollection() {
    if (is_collecting_.load()) {
        is_collecting_ = false;

        std::cout << "RealtimePointCloudCollector: Stopped" << std::endl;
        printStats();

        // Invoke export ready callback
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (export_ready_callback_) {
            ExportedPointCloud cloud;
            exportForGPU(cloud);
            export_ready_callback_(cloud);
        }
    }
}

inline void RealtimePointCloudCollector::reset() {
    is_collecting_ = false;
    voxel_fuser_.clear();
    raw_points_received_ = 0;
    new_voxels_created_ = 0;
}

inline bool RealtimePointCloudCollector::addPoint(const Vector3f& position, uint64_t timestamp) {
    return addPoint(position.x(), position.y(), position.z(), timestamp);
}

inline bool RealtimePointCloudCollector::addPoint(float x, float y, float z, uint64_t timestamp) {
    bool is_new_voxel = voxel_fuser_.addPoint(x, y, z, timestamp);
    raw_points_received_++;

    if (is_new_voxel) {
        new_voxels_created_++;

        // Invoke callback for new point
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (new_point_callback_) {
            new_point_callback_(Vector3f(x, y, z), voxel_fuser_.getVoxelCount());
        }
    }

    return is_new_voxel;
}

inline void RealtimePointCloudCollector::onTipPosition(const TipPosition& tip) {
    if (!is_collecting_.load()) return;

    bool is_new_voxel = voxel_fuser_.addPoint(
        tip.position.x(),
        tip.position.y(),
        tip.position.z(),
        tip.timestamp_us
    );

    raw_points_received_++;

    if (is_new_voxel) {
        new_voxels_created_++;

        // Invoke callback for new Super Point
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (new_point_callback_) {
            new_point_callback_(tip.position, voxel_fuser_.getVoxelCount());
        }
    }
}

inline size_t RealtimePointCloudCollector::exportForGPU(ExportedPointCloud& cloud) const {
    cloud.clear();

    size_t count = voxel_fuser_.exportToArrays(cloud.points_x, cloud.points_y, cloud.points_z);
    cloud.num_points = static_cast<uint32_t>(count);

    return count;
}

inline std::vector<Vector3f> RealtimePointCloudCollector::exportAsVectors() const {
    return voxel_fuser_.getFusedPoints();
}

inline std::vector<std::pair<Vector3f, int>> RealtimePointCloudCollector::exportWithWeights() const {
    return voxel_fuser_.getFusedPointsWithCounts();
}

inline size_t RealtimePointCloudCollector::exportToArrays(std::vector<float>& x,
                                                          std::vector<float>& y,
                                                          std::vector<float>& z) const {
    return voxel_fuser_.exportToArrays(x, y, z);
}

inline void RealtimePointCloudCollector::setNewPointCallback(NewPointCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    new_point_callback_ = callback;
}

inline void RealtimePointCloudCollector::setExportReadyCallback(ExportReadyCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    export_ready_callback_ = callback;
}

inline CollectionStats RealtimePointCloudCollector::getStats() const {
    CollectionStats stats;
    stats.raw_points_received = raw_points_received_.load();
    stats.new_voxels_created = voxel_fuser_.getVoxelCount();
    stats.duplicate_points_merged = stats.raw_points_received - stats.new_voxels_created;
    stats.compression_ratio = voxel_fuser_.getCompressionRatio();

    auto now = std::chrono::steady_clock::now();
    stats.elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);

    if (stats.elapsed_time.count() > 0) {
        stats.collection_rate_hz = stats.raw_points_received * 1000.0f / stats.elapsed_time.count();
    }

    return stats;
}

inline void RealtimePointCloudCollector::getBoundingBox(Vector3f& min_pt, Vector3f& max_pt) const {
    voxel_fuser_.getBoundingBox(min_pt, max_pt);
}

inline void RealtimePointCloudCollector::printStats() const {
    CollectionStats stats = getStats();

    std::cout << "\n========================================" << std::endl;
    std::cout << "Point Cloud Collection Statistics" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  Voxel cell size: " << voxel_fuser_.getCellSize() << " mm" << std::endl;
    std::cout << "  Raw points received: " << stats.raw_points_received << std::endl;
    std::cout << "  Super Points created: " << stats.new_voxels_created << std::endl;
    std::cout << "  Points merged (deduped): " << stats.duplicate_points_merged << std::endl;
    std::cout << "  Compression ratio: " << stats.compression_ratio << "x" << std::endl;
    std::cout << "  Collection time: " << stats.elapsed_time.count() << " ms" << std::endl;
    std::cout << "  Collection rate: " << stats.collection_rate_hz << " Hz" << std::endl;

    Vector3f min_pt, max_pt;
    getBoundingBox(min_pt, max_pt);
    std::cout << "  Bounding box: ["
              << min_pt.x() << ", " << min_pt.y() << ", " << min_pt.z() << "] to ["
              << max_pt.x() << ", " << max_pt.y() << ", " << max_pt.z() << "]" << std::endl;
    std::cout << "========================================\n" << std::endl;
}

} // namespace ProbeCalib
