/**
 * Open3D Real-time Visualizer Implementation
 *
 * This file implements the RealtimeVisualizer class for real-time
 * point cloud registration visualization using Open3D C++ API.
 *
 * NOTE: This file is pure C++ (no CUDA dependencies)
 */

#include "visualizer.h"

#include <open3d/Open3D.h>
#include <chrono>
#include <iostream>

// ============================================================================
// Implementation Details (PIMPL)
// ============================================================================
struct RealtimeVisualizer::Impl {
    // Open3D visualizer
    std::shared_ptr<open3d::visualization::Visualizer> vis;

    // Geometry objects
    std::shared_ptr<open3d::geometry::TriangleMesh> target_mesh;
    std::shared_ptr<open3d::geometry::PointCloud> source_cloud;
    std::shared_ptr<open3d::geometry::TriangleMesh> coord_axes;

    // State
    bool initialized = false;
    bool running = false;

    // Colors
    Eigen::Vector3d source_color{1.0, 0.0, 0.0};  // Red
    Eigen::Vector3d target_color{1.0, 1.0, 1.0};  // White

    // FPS tracking
    std::chrono::high_resolution_clock::time_point last_frame_time;
    double fps = 0.0;
    int frame_count = 0;
    double fps_accumulator = 0.0;

    // Point size
    double point_size = 3.0;
};

// ============================================================================
// Constructor / Destructor
// ============================================================================
RealtimeVisualizer::RealtimeVisualizer() : pImpl(std::make_unique<Impl>()) {
    pImpl->vis = std::make_shared<open3d::visualization::Visualizer>();
}

RealtimeVisualizer::~RealtimeVisualizer() {
    close();
}

// ============================================================================
// Step 1: Initialize Visualizer Window
// ============================================================================
bool RealtimeVisualizer::initialize(const std::string& target_ply_path,
                                     const std::string& window_name,
                                     int width, int height) {
    std::cout << "[Visualizer] Initializing with target: " << target_ply_path << std::endl;

    // Create window
    if (!pImpl->vis->CreateVisualizerWindow(window_name, width, height)) {
        std::cerr << "[Visualizer] Failed to create window!" << std::endl;
        return false;
    }

    // Load target mesh (the bone)
    pImpl->target_mesh = std::make_shared<open3d::geometry::TriangleMesh>();
    if (!open3d::io::ReadTriangleMesh(target_ply_path, *pImpl->target_mesh)) {
        std::cerr << "[Visualizer] Failed to load target mesh: " << target_ply_path << std::endl;
        return false;
    }

    // Compute normals if not present
    if (!pImpl->target_mesh->HasVertexNormals()) {
        pImpl->target_mesh->ComputeVertexNormals();
    }

    // Set target mesh color (white/gray)
    pImpl->target_mesh->PaintUniformColor(pImpl->target_color);

    // Add target mesh to visualizer
    pImpl->vis->AddGeometry(pImpl->target_mesh);

    // Create empty source point cloud (red)
    pImpl->source_cloud = std::make_shared<open3d::geometry::PointCloud>();
    pImpl->source_cloud->PaintUniformColor(pImpl->source_color);

    // Add source cloud to visualizer
    pImpl->vis->AddGeometry(pImpl->source_cloud);

    // Set up render options
    auto& render_opt = pImpl->vis->GetRenderOption();
    render_opt.point_size_ = pImpl->point_size;
    render_opt.background_color_ = Eigen::Vector3d(0.1, 0.1, 0.1);  // Dark gray background
    render_opt.light_on_ = true;

    // Reset view to see all geometry
    pImpl->vis->ResetViewPoint(true);

    pImpl->initialized = true;
    pImpl->running = true;
    pImpl->last_frame_time = std::chrono::high_resolution_clock::now();

    std::cout << "[Visualizer] Initialized successfully!" << std::endl;
    std::cout << "  Target mesh: " << pImpl->target_mesh->vertices_.size() << " vertices, "
              << pImpl->target_mesh->triangles_.size() << " triangles" << std::endl;

    return true;
}

bool RealtimeVisualizer::initializeWithMesh(const VisMeshSoA& target_mesh,
                                             const std::string& window_name,
                                             int width, int height) {
    std::cout << "[Visualizer] Initializing with VisMeshSoA..." << std::endl;

    // Create window
    if (!pImpl->vis->CreateVisualizerWindow(window_name, width, height)) {
        std::cerr << "[Visualizer] Failed to create window!" << std::endl;
        return false;
    }

    // Create Open3D mesh from VisMeshSoA
    pImpl->target_mesh = std::make_shared<open3d::geometry::TriangleMesh>();

    // Copy vertices
    pImpl->target_mesh->vertices_.resize(target_mesh.num_vertices);
    for (uint32_t i = 0; i < target_mesh.num_vertices; ++i) {
        pImpl->target_mesh->vertices_[i] = Eigen::Vector3d(
            target_mesh.vertices_x[i],
            target_mesh.vertices_y[i],
            target_mesh.vertices_z[i]
        );
    }

    // Copy normals if available
    if (target_mesh.normals_x && target_mesh.normals_y && target_mesh.normals_z) {
        pImpl->target_mesh->vertex_normals_.resize(target_mesh.num_vertices);
        for (uint32_t i = 0; i < target_mesh.num_vertices; ++i) {
            pImpl->target_mesh->vertex_normals_[i] = Eigen::Vector3d(
                target_mesh.normals_x[i],
                target_mesh.normals_y[i],
                target_mesh.normals_z[i]
            );
        }
    }

    // Copy triangles
    pImpl->target_mesh->triangles_.resize(target_mesh.num_faces);
    for (uint32_t i = 0; i < target_mesh.num_faces; ++i) {
        pImpl->target_mesh->triangles_[i] = Eigen::Vector3i(
            target_mesh.faces_v0[i],
            target_mesh.faces_v1[i],
            target_mesh.faces_v2[i]
        );
    }

    // Compute normals if not present
    if (!pImpl->target_mesh->HasVertexNormals()) {
        pImpl->target_mesh->ComputeVertexNormals();
    }

    // Set target mesh color (white/gray)
    pImpl->target_mesh->PaintUniformColor(pImpl->target_color);

    // Add target mesh to visualizer
    pImpl->vis->AddGeometry(pImpl->target_mesh);

    // Create empty source point cloud (red)
    pImpl->source_cloud = std::make_shared<open3d::geometry::PointCloud>();
    pImpl->source_cloud->PaintUniformColor(pImpl->source_color);

    // Add source cloud to visualizer
    pImpl->vis->AddGeometry(pImpl->source_cloud);

    // Set up render options
    auto& render_opt = pImpl->vis->GetRenderOption();
    render_opt.point_size_ = pImpl->point_size;
    render_opt.background_color_ = Eigen::Vector3d(0.1, 0.1, 0.1);
    render_opt.light_on_ = true;

    // Reset view
    pImpl->vis->ResetViewPoint(true);

    pImpl->initialized = true;
    pImpl->running = true;
    pImpl->last_frame_time = std::chrono::high_resolution_clock::now();

    std::cout << "[Visualizer] Initialized successfully!" << std::endl;
    std::cout << "  Target mesh: " << pImpl->target_mesh->vertices_.size() << " vertices, "
              << pImpl->target_mesh->triangles_.size() << " triangles" << std::endl;

    return true;
}

// ============================================================================
// Step 2: Update Data (The Update Loop)
// ============================================================================
void RealtimeVisualizer::updateSourceCloud(const std::vector<VisFloat3>& points) {
    if (!pImpl->initialized || points.empty()) return;

    // Resize Open3D point cloud
    pImpl->source_cloud->points_.resize(points.size());

    // Copy points
    for (size_t i = 0; i < points.size(); ++i) {
        pImpl->source_cloud->points_[i] = Eigen::Vector3d(
            points[i].x, points[i].y, points[i].z
        );
    }

    // Set color
    pImpl->source_cloud->PaintUniformColor(pImpl->source_color);
}

void RealtimeVisualizer::updateSourceCloud(const float* points_x,
                                            const float* points_y,
                                            const float* points_z,
                                            uint32_t num_points) {
    if (!pImpl->initialized || num_points == 0) return;

    // Resize Open3D point cloud
    pImpl->source_cloud->points_.resize(num_points);

    // Copy points
    for (uint32_t i = 0; i < num_points; ++i) {
        pImpl->source_cloud->points_[i] = Eigen::Vector3d(
            points_x[i], points_y[i], points_z[i]
        );
    }

    // Set color
    pImpl->source_cloud->PaintUniformColor(pImpl->source_color);
}

void RealtimeVisualizer::applyTransform(const float* transform) {
    if (!pImpl->initialized) return;

    // Convert float array to Eigen 4x4 matrix
    Eigen::Matrix4d eigen_transform;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            eigen_transform(i, j) = transform[i * 4 + j];
        }
    }

    // Transform the source point cloud
    pImpl->source_cloud->Transform(eigen_transform);
}

void RealtimeVisualizer::applyTransform(const VisMatrix4x4& transform) {
    applyTransform(transform.m);
}

void RealtimeVisualizer::setSourceColor(double r, double g, double b) {
    pImpl->source_color = Eigen::Vector3d(r, g, b);
    if (pImpl->source_cloud) {
        pImpl->source_cloud->PaintUniformColor(pImpl->source_color);
    }
}

void RealtimeVisualizer::setTargetColor(double r, double g, double b) {
    pImpl->target_color = Eigen::Vector3d(r, g, b);
    if (pImpl->target_mesh) {
        pImpl->target_mesh->PaintUniformColor(pImpl->target_color);
    }
}

// ============================================================================
// Step 3: Render Frame
// ============================================================================
bool RealtimeVisualizer::render() {
    if (!pImpl->initialized || !pImpl->running) return false;

    // Update geometry (tell GPU data changed)
    pImpl->vis->UpdateGeometry(pImpl->source_cloud);

    // Poll events (handle mouse/keyboard)
    if (!pImpl->vis->PollEvents()) {
        pImpl->running = false;
        return false;
    }

    // Render frame
    pImpl->vis->UpdateRender();

    // Calculate FPS
    auto now = std::chrono::high_resolution_clock::now();
    double frame_time = std::chrono::duration<double>(now - pImpl->last_frame_time).count();
    pImpl->last_frame_time = now;

    pImpl->fps_accumulator += frame_time;
    pImpl->frame_count++;

    if (pImpl->fps_accumulator >= 1.0) {
        pImpl->fps = pImpl->frame_count / pImpl->fps_accumulator;
        pImpl->frame_count = 0;
        pImpl->fps_accumulator = 0.0;
    }

    return pImpl->running;
}

bool RealtimeVisualizer::isRunning() const {
    return pImpl->initialized && pImpl->running;
}

void RealtimeVisualizer::close() {
    if (pImpl->initialized) {
        pImpl->vis->DestroyVisualizerWindow();
        pImpl->initialized = false;
        pImpl->running = false;
    }
}

// ============================================================================
// Additional Utilities
// ============================================================================
void RealtimeVisualizer::resetCamera() {
    if (pImpl->initialized) {
        pImpl->vis->ResetViewPoint(true);
    }
}

void RealtimeVisualizer::addCoordinateAxes(double size) {
    if (!pImpl->initialized) return;

    pImpl->coord_axes = open3d::geometry::TriangleMesh::CreateCoordinateFrame(size);
    pImpl->vis->AddGeometry(pImpl->coord_axes);
}

void RealtimeVisualizer::setPointSize(double size) {
    pImpl->point_size = size;
    if (pImpl->initialized) {
        pImpl->vis->GetRenderOption().point_size_ = size;
    }
}

double RealtimeVisualizer::getFPS() const {
    return pImpl->fps;
}
