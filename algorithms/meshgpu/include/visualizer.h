#pragma once

/**
 * Open3D Real-time Visualizer for MeshGPU
 *
 * Provides real-time visualization of point cloud registration using Open3D C++ API.
 *
 * Usage:
 *   RealtimeVisualizer vis;
 *   vis.initialize("E:/ICPtry/fixed_tibia_normalized.ply");
 *
 *   while (vis.isRunning()) {
 *       // Update from CPU data
 *       vis.updateSourceCloud(points_x, points_y, points_z, num_points);
 *       vis.applyTransform(transform_data);
 *       vis.render();
 *   }
 */

#include <memory>
#include <vector>
#include <string>
#include <cstdint>

#include "visualizer_types.h"

// ============================================================================
// RealtimeVisualizer - Open3D based real-time point cloud visualization
// ============================================================================
class RealtimeVisualizer {
public:
    RealtimeVisualizer();
    ~RealtimeVisualizer();

    // ========================================
    // Step 1: Initialize Visualizer Window
    // ========================================

    /**
     * Initialize the visualizer with a target mesh (bone)
     * @param target_ply_path Path to target PLY file (e.g., tibia_normalized.ply)
     * @param window_name Window title
     * @param width Window width
     * @param height Window height
     * @return true if initialization successful
     */
    bool initialize(const std::string& target_ply_path,
                    const std::string& window_name = "MeshGPU Real-time Registration",
                    int width = 1280,
                    int height = 720);

    /**
     * Initialize with an existing mesh structure
     */
    bool initializeWithMesh(const VisMeshSoA& target_mesh,
                            const std::string& window_name = "MeshGPU Real-time Registration",
                            int width = 1280,
                            int height = 720);

    // ========================================
    // Step 2: Update Data (The Update Loop)
    // ========================================

    /**
     * Update source point cloud from CPU data (vector of VisFloat3)
     */
    void updateSourceCloud(const std::vector<VisFloat3>& points);

    /**
     * Update source point cloud from separate coordinate arrays (CPU)
     */
    void updateSourceCloud(const float* points_x,
                           const float* points_y,
                           const float* points_z,
                           uint32_t num_points);

    /**
     * Apply transformation matrix to source cloud
     * @param transform 16 floats in row-major order (4x4 matrix)
     */
    void applyTransform(const float* transform);

    /**
     * Apply transformation matrix using VisMatrix4x4
     */
    void applyTransform(const VisMatrix4x4& transform);

    /**
     * Set source cloud color (default: red)
     */
    void setSourceColor(double r, double g, double b);

    /**
     * Set target mesh/cloud color (default: white)
     */
    void setTargetColor(double r, double g, double b);

    // ========================================
    // Step 3: Render Frame
    // ========================================

    /**
     * Render one frame and handle events
     * Call this at the end of your main loop
     * @return true if window is still open, false if closed
     */
    bool render();

    /**
     * Check if visualizer is still running
     */
    bool isRunning() const;

    /**
     * Close the visualizer window
     */
    void close();

    // ========================================
    // Additional utilities
    // ========================================

    /**
     * Reset camera to view all geometry
     */
    void resetCamera();

    /**
     * Add coordinate axes to the scene
     */
    void addCoordinateAxes(double size = 50.0);

    /**
     * Set point size for point clouds
     */
    void setPointSize(double size);

    /**
     * Get current FPS
     */
    double getFPS() const;

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;

    // Prevent copying
    RealtimeVisualizer(const RealtimeVisualizer&) = delete;
    RealtimeVisualizer& operator=(const RealtimeVisualizer&) = delete;
};
