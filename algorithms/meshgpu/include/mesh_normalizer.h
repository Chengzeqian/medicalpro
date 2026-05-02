#pragma once

#include "types.h"
#include <Eigen/Dense>
#include <Eigen/Eigenvalues>
#include <vector>
#include <iostream>
#include <cmath>

// ============================================================================
// Mesh Normalizer - PCA-based Mesh Alignment
// Aligns mesh principal axis to world Z axis (Professional Mode)
// ============================================================================

class MeshNormalizer {
public:
    struct NormalizationResult {
        Matrix4x4 transform;           // Transform applied to normalize
        float3_t centroid;             // Original centroid
        float3_t principal_axis;       // Principal axis (longest)
        float3_t secondary_axis;       // Secondary axis
        float3_t tertiary_axis;        // Tertiary axis (shortest)
        float eigenvalue_ratio;        // Ratio of largest to smallest eigenvalue
        bool flipped_z;                // Whether Z was flipped (head was down)
        bool mirrored;                 // Whether mesh was mirrored (handedness fix)
    };

    // Normalize mesh: center at origin, align principal axis to Z
    static NormalizationResult normalize(MeshSoA& mesh) {
        NormalizationResult result;
        result.flipped_z = false;
        result.mirrored = false;

        uint32_t n = mesh.num_vertices;
        if (n == 0) {
            std::cerr << "[MeshNormalizer] Error: Empty mesh" << std::endl;
            return result;
        }

        std::cout << "\n========================================" << std::endl;
        std::cout << "Mesh Normalization (PCA Alignment)" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "  Vertices: " << n << std::endl;

        // Step 1: Compute centroid
        double cx = 0, cy = 0, cz = 0;
        for (uint32_t i = 0; i < n; ++i) {
            cx += mesh.vertices_x[i];
            cy += mesh.vertices_y[i];
            cz += mesh.vertices_z[i];
        }
        cx /= n; cy /= n; cz /= n;
        result.centroid = float3_t((float)cx, (float)cy, (float)cz);

        std::cout << "  Original centroid: (" << cx << ", " << cy << ", " << cz << ")" << std::endl;

        // Step 2: Build covariance matrix
        // Cov = (1/n) * sum((p - centroid) * (p - centroid)^T)
        Eigen::Matrix3d cov = Eigen::Matrix3d::Zero();

        for (uint32_t i = 0; i < n; ++i) {
            double dx = mesh.vertices_x[i] - cx;
            double dy = mesh.vertices_y[i] - cy;
            double dz = mesh.vertices_z[i] - cz;

            cov(0, 0) += dx * dx;
            cov(0, 1) += dx * dy;
            cov(0, 2) += dx * dz;
            cov(1, 1) += dy * dy;
            cov(1, 2) += dy * dz;
            cov(2, 2) += dz * dz;
        }

        // Symmetric
        cov(1, 0) = cov(0, 1);
        cov(2, 0) = cov(0, 2);
        cov(2, 1) = cov(1, 2);
        cov /= n;

        // Step 3: Eigendecomposition
        Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> solver(cov);
        Eigen::Vector3d eigenvalues = solver.eigenvalues();   // Ascending order
        Eigen::Matrix3d eigenvectors = solver.eigenvectors(); // Columns are eigenvectors

        std::cout << "  Eigenvalues: " << eigenvalues(0) << ", " << eigenvalues(1) << ", " << eigenvalues(2) << std::endl;

        // Principal axis = eigenvector with largest eigenvalue (column 2)
        // Secondary axis = eigenvector with middle eigenvalue (column 1)
        // Tertiary axis = eigenvector with smallest eigenvalue (column 0)
        Eigen::Vector3d v_principal = eigenvectors.col(2);  // Largest -> Z axis
        Eigen::Vector3d v_secondary = eigenvectors.col(1);  // Middle -> Y axis
        Eigen::Vector3d v_tertiary = eigenvectors.col(0);   // Smallest -> X axis

        result.principal_axis = float3_t((float)v_principal(0), (float)v_principal(1), (float)v_principal(2));
        result.secondary_axis = float3_t((float)v_secondary(0), (float)v_secondary(1), (float)v_secondary(2));
        result.tertiary_axis = float3_t((float)v_tertiary(0), (float)v_tertiary(1), (float)v_tertiary(2));
        result.eigenvalue_ratio = (float)(eigenvalues(2) / std::max(eigenvalues(0), 1e-10));

        std::cout << "  Principal axis: (" << v_principal(0) << ", " << v_principal(1) << ", " << v_principal(2) << ")" << std::endl;
        std::cout << "  Eigenvalue ratio: " << result.eigenvalue_ratio << std::endl;

        // Step 4: Build rotation matrix to align principal axis to Z
        // R * v_principal = [0, 0, 1]^T
        // We want: new_X = v_tertiary, new_Y = v_secondary, new_Z = v_principal
        Eigen::Matrix3d R;
        R.col(0) = v_tertiary;   // X
        R.col(1) = v_secondary;  // Y
        R.col(2) = v_principal;  // Z

        // Check determinant - if negative, we have a reflection (mirror)
        double det = R.determinant();
        std::cout << "  Rotation determinant: " << det << std::endl;

        if (det < 0) {
            std::cout << "  [WARNING] Reflection detected! Flipping X axis to maintain handedness." << std::endl;
            R.col(0) = -R.col(0);  // Flip X to make it right-handed
            result.mirrored = true;
        }

        // R transforms from PCA frame to world frame
        // We need R^T to transform points from original to PCA-aligned
        Eigen::Matrix3d R_align = R.transpose();

        // Step 5: Apply transformation: center then rotate
        // p' = R_align * (p - centroid)
        for (uint32_t i = 0; i < n; ++i) {
            double px = mesh.vertices_x[i] - cx;
            double py = mesh.vertices_y[i] - cy;
            double pz = mesh.vertices_z[i] - cz;

            mesh.vertices_x[i] = (float)(R_align(0, 0) * px + R_align(0, 1) * py + R_align(0, 2) * pz);
            mesh.vertices_y[i] = (float)(R_align(1, 0) * px + R_align(1, 1) * py + R_align(1, 2) * pz);
            mesh.vertices_z[i] = (float)(R_align(2, 0) * px + R_align(2, 1) * py + R_align(2, 2) * pz);
        }

        // Also transform normals (rotation only, no translation)
        if (mesh.normals_x && mesh.normals_y && mesh.normals_z) {
            for (uint32_t i = 0; i < n; ++i) {
                double nx = mesh.normals_x[i];
                double ny = mesh.normals_y[i];
                double nz = mesh.normals_z[i];

                mesh.normals_x[i] = (float)(R_align(0, 0) * nx + R_align(0, 1) * ny + R_align(0, 2) * nz);
                mesh.normals_y[i] = (float)(R_align(1, 0) * nx + R_align(1, 1) * ny + R_align(1, 2) * nz);
                mesh.normals_z[i] = (float)(R_align(2, 0) * nx + R_align(2, 1) * ny + R_align(2, 2) * nz);
            }
        }

        // Step 6: Check Z orientation - ensure "head" is up
        // For tibia: distal end (ankle) should have smaller Z than proximal end (knee)
        // We check if most vertices are in negative Z half
        float z_min = mesh.vertices_z[0], z_max = mesh.vertices_z[0];
        double z_sum = 0;
        for (uint32_t i = 0; i < n; ++i) {
            z_min = std::min(z_min, mesh.vertices_z[i]);
            z_max = std::max(z_max, mesh.vertices_z[i]);
            z_sum += mesh.vertices_z[i];
        }
        float z_mean = (float)(z_sum / n);

        std::cout << "  Z range after alignment: [" << z_min << ", " << z_max << "]" << std::endl;
        std::cout << "  Z mean: " << z_mean << std::endl;

        // If mean Z is significantly negative, flip the bone
        // This is a heuristic - for tibia, we want ankle at bottom (smaller Z)
        // The "wider" end (proximal/knee) typically has more surface area
        // We can also check by looking at the vertex distribution

        // Count vertices in upper vs lower half
        int count_positive = 0, count_negative = 0;
        for (uint32_t i = 0; i < n; ++i) {
            if (mesh.vertices_z[i] > 0) count_positive++;
            else count_negative++;
        }

        std::cout << "  Vertices in Z>0: " << count_positive << ", Z<0: " << count_negative << std::endl;

        // For long bones, we use a different heuristic:
        // Compute the "spread" (variance) of XY coordinates at top vs bottom
        // The wider end (proximal for tibia) should be at top
        float spread_top = 0, spread_bottom = 0;
        int count_top = 0, count_bottom = 0;
        float z_mid = (z_min + z_max) / 2;
        float z_quarter = (z_max - z_min) / 4;

        for (uint32_t i = 0; i < n; ++i) {
            float xy_dist_sq = mesh.vertices_x[i] * mesh.vertices_x[i] +
                               mesh.vertices_y[i] * mesh.vertices_y[i];

            if (mesh.vertices_z[i] > z_mid + z_quarter) {
                spread_top += xy_dist_sq;
                count_top++;
            } else if (mesh.vertices_z[i] < z_mid - z_quarter) {
                spread_bottom += xy_dist_sq;
                count_bottom++;
            }
        }

        if (count_top > 0) spread_top /= count_top;
        if (count_bottom > 0) spread_bottom /= count_bottom;

        std::cout << "  XY spread at top (Z+): " << sqrtf(spread_top) << " mm" << std::endl;
        std::cout << "  XY spread at bottom (Z-): " << sqrtf(spread_bottom) << " mm" << std::endl;

        // For tibia: proximal end (knee) is wider, should be at top (larger Z)
        // If bottom is wider, we need to flip
        if (spread_bottom > spread_top * 1.2f) {  // 20% threshold
            std::cout << "  [INFO] Bottom is wider than top. Flipping Z axis (rotating 180° around X)." << std::endl;

            // Flip Z: negate Z coordinates
            for (uint32_t i = 0; i < n; ++i) {
                mesh.vertices_z[i] = -mesh.vertices_z[i];
                if (mesh.normals_z) mesh.normals_z[i] = -mesh.normals_z[i];
            }
            result.flipped_z = true;
        }

        // Step 7: Re-center at Z=0 (optional: shift so bottom is at Z=0)
        // Find new Z range
        z_min = mesh.vertices_z[0];
        for (uint32_t i = 1; i < n; ++i) {
            z_min = std::min(z_min, mesh.vertices_z[i]);
        }

        // Shift so bottom is at Z=0
        for (uint32_t i = 0; i < n; ++i) {
            mesh.vertices_z[i] -= z_min;
        }

        std::cout << "  Shifted so Z_min = 0" << std::endl;

        // Build the final transform matrix for reference
        // T = Translate(-z_min) * FlipZ? * R_align * Translate(-centroid)
        result.transform = Matrix4x4();  // Will be computed if needed

        // Print final stats
        float final_z_min = FLT_MAX, final_z_max = -FLT_MAX;
        float final_x_min = FLT_MAX, final_x_max = -FLT_MAX;
        float final_y_min = FLT_MAX, final_y_max = -FLT_MAX;

        for (uint32_t i = 0; i < n; ++i) {
            final_x_min = std::min(final_x_min, mesh.vertices_x[i]);
            final_x_max = std::max(final_x_max, mesh.vertices_x[i]);
            final_y_min = std::min(final_y_min, mesh.vertices_y[i]);
            final_y_max = std::max(final_y_max, mesh.vertices_y[i]);
            final_z_min = std::min(final_z_min, mesh.vertices_z[i]);
            final_z_max = std::max(final_z_max, mesh.vertices_z[i]);
        }

        std::cout << "\n--- Normalized Mesh Stats ---" << std::endl;
        std::cout << "  X range: [" << final_x_min << ", " << final_x_max << "] mm" << std::endl;
        std::cout << "  Y range: [" << final_y_min << ", " << final_y_max << "] mm" << std::endl;
        std::cout << "  Z range: [" << final_z_min << ", " << final_z_max << "] mm (height: " << (final_z_max - final_z_min) << " mm)" << std::endl;
        std::cout << "  Flipped Z: " << (result.flipped_z ? "YES" : "NO") << std::endl;
        std::cout << "  Mirrored: " << (result.mirrored ? "YES" : "NO") << std::endl;
        std::cout << "========================================\n" << std::endl;

        return result;
    }

    // Check if a mesh needs normalization (quick heuristic)
    static bool needsNormalization(const MeshSoA& mesh) {
        if (mesh.num_vertices == 0) return false;

        // Compute bounding box
        float x_min = mesh.vertices_x[0], x_max = mesh.vertices_x[0];
        float y_min = mesh.vertices_y[0], y_max = mesh.vertices_y[0];
        float z_min = mesh.vertices_z[0], z_max = mesh.vertices_z[0];

        for (uint32_t i = 1; i < mesh.num_vertices; ++i) {
            x_min = std::min(x_min, mesh.vertices_x[i]);
            x_max = std::max(x_max, mesh.vertices_x[i]);
            y_min = std::min(y_min, mesh.vertices_y[i]);
            y_max = std::max(y_max, mesh.vertices_y[i]);
            z_min = std::min(z_min, mesh.vertices_z[i]);
            z_max = std::max(z_max, mesh.vertices_z[i]);
        }

        float dx = x_max - x_min;
        float dy = y_max - y_min;
        float dz = z_max - z_min;

        // If Z is not the longest axis, needs normalization
        if (dz < dx || dz < dy) {
            std::cout << "[MeshNormalizer] Mesh needs normalization: Z is not the longest axis" << std::endl;
            std::cout << "  Dimensions: X=" << dx << ", Y=" << dy << ", Z=" << dz << std::endl;
            return true;
        }

        return false;
    }
};
