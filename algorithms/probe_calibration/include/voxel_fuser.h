#pragma once

#include "types.h"
#include <unordered_map>
#include <vector>
#include <cmath>
#include <functional>
#include <iostream>
#include <climits>

namespace ProbeCalib {

// ============================================================================
// Voxel Key - 3D grid cell identifier
// ============================================================================
struct VoxelKey {
    int x, y, z;

    bool operator==(const VoxelKey& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

// Hash function for VoxelKey
struct VoxelKeyHash {
    std::size_t operator()(const VoxelKey& k) const {
        // Combine x, y, z into a single hash
        std::size_t h1 = std::hash<int>{}(k.x);
        std::size_t h2 = std::hash<int>{}(k.y);
        std::size_t h3 = std::hash<int>{}(k.z);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

// ============================================================================
// Voxel Data - Accumulated point data in a voxel
// ============================================================================
struct VoxelData {
    float sum_x, sum_y, sum_z;  // Sum of positions
    int count;                   // Number of points accumulated
    uint64_t last_timestamp;     // Last update timestamp

    VoxelData() : sum_x(0), sum_y(0), sum_z(0), count(0), last_timestamp(0) {}

    // Get averaged position (the "Super Point")
    Vector3f getAveragePosition() const {
        if (count == 0) return Vector3f::Zero();
        return Vector3f(sum_x / count, sum_y / count, sum_z / count);
    }

    // Add a new point
    void addPoint(float x, float y, float z, uint64_t timestamp) {
        sum_x += x;
        sum_y += y;
        sum_z += z;
        count++;
        last_timestamp = timestamp;
    }
};

// ============================================================================
// VoxelFuser - Real-time voxel fusion for noise reduction
// ============================================================================
/**
 * Converts 60Hz noisy probe tip positions into high-SNR "Super Points"
 *
 * Principle:
 * - Divide space into voxel grid (default 1mm cell size)
 * - Points in same voxel are averaged together
 * - Output: One point per visited voxel (much less redundancy)
 *
 * Benefits:
 * - Removes redundant points (doctor staying in same position)
 * - Reduces noise through averaging
 * - Maintains spatial coverage information
 */
class VoxelFuser {
public:
    /**
     * Constructor
     * @param cell_size Voxel cell size in mm (default 1.0mm)
     */
    explicit VoxelFuser(float cell_size = 1.0f);

    // ========================================================================
    // Core Operations
    // ========================================================================

    /**
     * Add a new point to the fuser
     * @param position 3D position in mm
     * @param timestamp Timestamp in microseconds (optional)
     * @return true if this is a new voxel (first point in this cell)
     */
    bool addPoint(const Vector3f& position, uint64_t timestamp = 0);

    /**
     * Add point from raw coordinates
     */
    bool addPoint(float x, float y, float z, uint64_t timestamp = 0);

    /**
     * Get all fused "Super Points"
     * @return Vector of averaged positions, one per visited voxel
     */
    std::vector<Vector3f> getFusedPoints() const;

    /**
     * Get fused points with their sample counts
     * @return Vector of (position, count) pairs
     */
    std::vector<std::pair<Vector3f, int>> getFusedPointsWithCounts() const;

    /**
     * Export fused points to raw arrays (for GPU upload)
     * @param out_x Output X coordinates
     * @param out_y Output Y coordinates
     * @param out_z Output Z coordinates
     * @return Number of points exported
     */
    size_t exportToArrays(std::vector<float>& out_x,
                          std::vector<float>& out_y,
                          std::vector<float>& out_z) const;

    // ========================================================================
    // State Management
    // ========================================================================

    /**
     * Clear all accumulated data
     */
    void clear();

    /**
     * Get number of unique voxels (output point count)
     */
    size_t getVoxelCount() const { return voxel_map_.size(); }

    /**
     * Get total number of raw points added
     */
    size_t getTotalPointCount() const { return total_points_; }

    /**
     * Get compression ratio (raw points / fused points)
     */
    float getCompressionRatio() const {
        if (voxel_map_.empty()) return 0.0f;
        return static_cast<float>(total_points_) / voxel_map_.size();
    }

    /**
     * Get cell size
     */
    float getCellSize() const { return cell_size_; }

    /**
     * Set minimum sample count for output
     * Voxels with fewer samples will be filtered out
     */
    void setMinSampleCount(int min_count) { min_sample_count_ = min_count; }

    // ========================================================================
    // Statistics
    // ========================================================================

    /**
     * Print statistics about the fusion
     */
    void printStats() const;

    /**
     * Get bounding box of all fused points
     */
    void getBoundingBox(Vector3f& min_pt, Vector3f& max_pt) const;

private:
    // Convert world position to voxel key
    VoxelKey positionToKey(float x, float y, float z) const;

    float cell_size_;           // Voxel cell size in mm
    float inv_cell_size_;       // 1.0 / cell_size (for fast division)
    int min_sample_count_;      // Minimum samples for valid output
    size_t total_points_;       // Total raw points added

    // Hash map: VoxelKey -> VoxelData
    std::unordered_map<VoxelKey, VoxelData, VoxelKeyHash> voxel_map_;
};

// ============================================================================
// Implementation (header-only for simplicity)
// ============================================================================

inline VoxelFuser::VoxelFuser(float cell_size)
    : cell_size_(cell_size)
    , inv_cell_size_(1.0f / cell_size)
    , min_sample_count_(1)
    , total_points_(0)
{
}

inline VoxelKey VoxelFuser::positionToKey(float x, float y, float z) const {
    return VoxelKey{
        static_cast<int>(std::floor(x * inv_cell_size_)),
        static_cast<int>(std::floor(y * inv_cell_size_)),
        static_cast<int>(std::floor(z * inv_cell_size_))
    };
}

inline bool VoxelFuser::addPoint(const Vector3f& position, uint64_t timestamp) {
    return addPoint(position.x(), position.y(), position.z(), timestamp);
}

inline bool VoxelFuser::addPoint(float x, float y, float z, uint64_t timestamp) {
    VoxelKey key = positionToKey(x, y, z);

    auto it = voxel_map_.find(key);
    bool is_new_voxel = (it == voxel_map_.end());

    if (is_new_voxel) {
        VoxelData data;
        data.addPoint(x, y, z, timestamp);
        voxel_map_[key] = data;
    } else {
        it->second.addPoint(x, y, z, timestamp);
    }

    total_points_++;
    return is_new_voxel;
}

inline std::vector<Vector3f> VoxelFuser::getFusedPoints() const {
    std::vector<Vector3f> result;
    result.reserve(voxel_map_.size());

    for (const auto& pair : voxel_map_) {
        if (pair.second.count >= min_sample_count_) {
            result.push_back(pair.second.getAveragePosition());
        }
    }

    return result;
}

inline std::vector<std::pair<Vector3f, int>> VoxelFuser::getFusedPointsWithCounts() const {
    std::vector<std::pair<Vector3f, int>> result;
    result.reserve(voxel_map_.size());

    for (const auto& pair : voxel_map_) {
        if (pair.second.count >= min_sample_count_) {
            result.emplace_back(pair.second.getAveragePosition(), pair.second.count);
        }
    }

    return result;
}

inline size_t VoxelFuser::exportToArrays(std::vector<float>& out_x,
                                          std::vector<float>& out_y,
                                          std::vector<float>& out_z) const {
    size_t count = 0;

    // Count valid voxels first
    for (const auto& pair : voxel_map_) {
        if (pair.second.count >= min_sample_count_) {
            count++;
        }
    }

    out_x.resize(count);
    out_y.resize(count);
    out_z.resize(count);

    size_t idx = 0;
    for (const auto& pair : voxel_map_) {
        if (pair.second.count >= min_sample_count_) {
            Vector3f pos = pair.second.getAveragePosition();
            out_x[idx] = pos.x();
            out_y[idx] = pos.y();
            out_z[idx] = pos.z();
            idx++;
        }
    }

    return count;
}

inline void VoxelFuser::clear() {
    voxel_map_.clear();
    total_points_ = 0;
}

inline void VoxelFuser::printStats() const {
    std::cout << "\n========================================" << std::endl;
    std::cout << "VoxelFuser Statistics" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  Cell size: " << cell_size_ << " mm" << std::endl;
    std::cout << "  Raw points added: " << total_points_ << std::endl;
    std::cout << "  Unique voxels: " << voxel_map_.size() << std::endl;
    std::cout << "  Compression ratio: " << getCompressionRatio() << "x" << std::endl;

    // Calculate average samples per voxel
    if (!voxel_map_.empty()) {
        int min_count = INT_MAX, max_count = 0;
        float sum_count = 0;
        for (const auto& pair : voxel_map_) {
            int c = pair.second.count;
            min_count = std::min(min_count, c);
            max_count = std::max(max_count, c);
            sum_count += c;
        }
        std::cout << "  Samples per voxel: min=" << min_count
                  << ", max=" << max_count
                  << ", avg=" << (sum_count / voxel_map_.size()) << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}

inline void VoxelFuser::getBoundingBox(Vector3f& min_pt, Vector3f& max_pt) const {
    if (voxel_map_.empty()) {
        min_pt = Vector3f::Zero();
        max_pt = Vector3f::Zero();
        return;
    }

    min_pt = Vector3f(FLT_MAX, FLT_MAX, FLT_MAX);
    max_pt = Vector3f(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (const auto& pair : voxel_map_) {
        if (pair.second.count >= min_sample_count_) {
            Vector3f pos = pair.second.getAveragePosition();
            min_pt.x() = std::min(min_pt.x(), pos.x());
            min_pt.y() = std::min(min_pt.y(), pos.y());
            min_pt.z() = std::min(min_pt.z(), pos.z());
            max_pt.x() = std::max(max_pt.x(), pos.x());
            max_pt.y() = std::max(max_pt.y(), pos.y());
            max_pt.z() = std::max(max_pt.z(), pos.z());
        }
    }
}

} // namespace ProbeCalib
