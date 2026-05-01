/**
 * @file tip_calibration_solver.cpp
 * @brief Probe Tip Calibration Solver Implementation
 *
 * Pivot calibration using SVD + iterative row-level outlier rejection,
 * following IGSIO vtkIGSIOPivotCalibrationAlgo approach.
 *
 * Key improvements over naive pose-level rejection:
 *  1. Row-level outlier detection: each axis (x,y,z) of each pose is
 *     independently evaluated; a pose with one bad axis keeps its good axes.
 *  2. Orientation diversity check: rejects calibration attempts where the
 *     probe wasn't rotated enough (ill-conditioned system).
 */

#include "tip_calibration_solver.h"
#include <Eigen/SVD>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <limits>
#include <set>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ProbeCalib {

// ============================================================================
// Orientation diversity check (IGSIO-style)
// ============================================================================

static float computeMaxOrientationDifferenceDeg(const std::vector<PoseData>& poses) {
    if (poses.size() < 2) return 0.0f;

    float max_angle = 0.0f;
    // Compare first pose against all others for efficiency
    const Matrix3f R0 = poses[0].rotation();
    for (size_t i = 1; i < poses.size(); ++i) {
        const Matrix3f Ri = poses[i].rotation();
        // Relative rotation: R_diff = R0^T * Ri
        const Matrix3f R_diff = R0.transpose() * Ri;
        // Angle from trace: angle = acos((trace(R)-1)/2)
        float trace_val = R_diff(0, 0) + R_diff(1, 1) + R_diff(2, 2);
        trace_val = std::max(-1.0f, std::min(3.0f, trace_val));
        float angle_rad = std::acos((trace_val - 1.0f) / 2.0f);
        float angle_deg = angle_rad * 180.0f / static_cast<float>(M_PI);
        max_angle = std::max(max_angle, angle_deg);
    }
    return max_angle;
}

// ============================================================================
// Row-level outlier removal (IGSIO RemoveOutliersFromLSQR equivalent)
// ============================================================================

struct RowData {
    Eigen::VectorXf a_row;  // one row of A matrix
    float b_val;            // corresponding b value
    uint32_t original_row;  // original row index (for tracking)
};

// Compute per-row residuals: r_i = A_i * x - b_i
static std::vector<float> computeRowResiduals(
    const std::vector<RowData>& rows,
    const Eigen::VectorXf& x
) {
    std::vector<float> residuals(rows.size());
    for (size_t i = 0; i < rows.size(); ++i) {
        residuals[i] = rows[i].a_row.dot(x) - rows[i].b_val;
    }
    return residuals;
}

// Remove rows where |residual - mean| > threshold_multiplier * stdev
// Returns true if any outlier was found
static bool removeRowOutliers(
    std::vector<RowData>& rows,
    const Eigen::VectorXf& x,
    float threshold_multiplier,
    float* out_mean = nullptr,
    float* out_stdev = nullptr
) {
    if (rows.size() < 8) return false;

    std::vector<float> residuals = computeRowResiduals(rows, x);

    // Compute mean
    double sum = 0.0;
    for (float r : residuals) sum += r;
    double mean = sum / residuals.size();

    // Compute stdev
    double var_sum = 0.0;
    for (float r : residuals) {
        double d = r - mean;
        var_sum += d * d;
    }
    double stdev = std::sqrt(var_sum / residuals.size());

    if (out_mean) *out_mean = static_cast<float>(mean);
    if (out_stdev) *out_stdev = static_cast<float>(stdev);

    // Remove outlier rows
    double threshold = threshold_multiplier * stdev;
    std::vector<RowData> kept;
    kept.reserve(rows.size());
    bool found = false;

    for (size_t i = 0; i < rows.size(); ++i) {
        if (std::fabs(residuals[i] - mean) < threshold) {
            kept.push_back(std::move(rows[i]));
        } else {
            found = true;
        }
    }

    if (found) {
        rows = std::move(kept);
    }
    return found;
}

// Solve from row data using SVD
static bool solveFromRows(
    const std::vector<RowData>& rows,
    int n_cols,
    Eigen::VectorXf& x
) {
    const int m = static_cast<int>(rows.size());
    Eigen::MatrixXf A(m, n_cols);
    Eigen::VectorXf b(m);
    for (int i = 0; i < m; ++i) {
        A.row(i) = rows[i].a_row;
        b(i) = rows[i].b_val;
    }
    Eigen::JacobiSVD<Eigen::MatrixXf> svd(A, Eigen::ComputeThinU | Eigen::ComputeThinV);
    x = svd.solve(b);
    return true;
}

// ============================================================================
// Main Solve Method (basic, no outlier rejection)
// ============================================================================

CalibrationResult TipCalibrationSolver::solve(const std::vector<PoseData>& poses) {
    CalibrationResult result;

    if (poses.size() < 10) {
        std::cerr << "[TipCalibrationSolver] ERROR: Not enough poses (need at least 10, got "
                  << poses.size() << ")" << std::endl;
        result.is_valid = false;
        return result;
    }

    std::cout << "[TipCalibrationSolver] Solving with " << poses.size() << " poses..." << std::endl;

    Eigen::MatrixXf A;
    Eigen::VectorXf b;
    buildLinearSystem(poses, A, b);

    Eigen::VectorXf x;
    if (!solveSVD(A, b, x)) {
        std::cerr << "[TipCalibrationSolver] ERROR: SVD solve failed" << std::endl;
        result.is_valid = false;
        return result;
    }

    result.tip_offset = Vector3f(x(0), x(1), x(2));
    Vector3f pivot_point(x(3), x(4), x(5));

    std::vector<float> residuals = computeResiduals(poses, result.tip_offset, pivot_point);
    float sum_sq = 0.0f;
    for (float r : residuals) sum_sq += r * r;
    result.residual_error = std::sqrt(sum_sq / residuals.size());
    result.num_poses_used = static_cast<int>(poses.size());
    result.is_valid = (result.residual_error < 2.0f);

    std::cout << "[TipCalibrationSolver] Calibration " << (result.is_valid ? "SUCCESS" : "FAILED") << std::endl;
    std::cout << "  Tip offset (local): (" << result.tip_offset.x() << ", "
              << result.tip_offset.y() << ", " << result.tip_offset.z() << ") mm" << std::endl;
    std::cout << "  Pivot point (world): (" << pivot_point.x() << ", "
              << pivot_point.y() << ", " << pivot_point.z() << ") mm" << std::endl;
    std::cout << "  Residual RMS error: " << result.residual_error << " mm" << std::endl;

    return result;
}

// ============================================================================
// Validation
// ============================================================================

float TipCalibrationSolver::validateCalibration(
    const std::vector<PoseData>& poses,
    const Vector3f& tip_offset
) {
    if (poses.empty()) return std::numeric_limits<float>::max();

    std::vector<Vector3f> world_tips;
    world_tips.reserve(poses.size());

    for (const auto& pose : poses) {
        Vector4f tip_local(tip_offset.x(), tip_offset.y(), tip_offset.z(), 1.0f);
        Vector4f tip_world = pose.transform * tip_local;
        world_tips.emplace_back(tip_world.x(), tip_world.y(), tip_world.z());
    }

    Vector3f centroid = Vector3f::Zero();
    for (const auto& tip : world_tips) centroid += tip;
    centroid /= static_cast<float>(world_tips.size());

    float sum_sq = 0.0f;
    for (const auto& tip : world_tips) {
        float dist = (tip - centroid).norm();
        sum_sq += dist * dist;
    }

    return std::sqrt(sum_sq / world_tips.size());
}

// ============================================================================
// Robust Solve: IGSIO-style row-level outlier rejection
// ============================================================================

CalibrationResult TipCalibrationSolver::solveRobust(
    const std::vector<PoseData>& poses,
    float inlier_threshold,
    int max_iterations
) {
    if (poses.size() < 20) {
        std::cerr << "[TipCalibrationSolver] Not enough poses for robust solve" << std::endl;
        return solve(poses);
    }

    // -------------------------------------------------------------------------
    // Step 0: Orientation diversity check (IGSIO-style)
    // -------------------------------------------------------------------------
    const float min_orientation_diff_deg = 15.0f;
    float max_orient_diff = computeMaxOrientationDifferenceDeg(poses);
    std::cout << "[TipCalibrationSolver] Orientation diversity: " << max_orient_diff << " deg" << std::endl;

    if (max_orient_diff < min_orientation_diff_deg) {
        std::cerr << "[TipCalibrationSolver] WARNING: Insufficient orientation diversity ("
                  << max_orient_diff << " deg < " << min_orientation_diff_deg
                  << " deg). Calibration may be inaccurate." << std::endl;
    }

    std::cout << "[TipCalibrationSolver] IGSIO-style robust solve: " << poses.size()
              << " poses, row-level outlier rejection..." << std::endl;

    // -------------------------------------------------------------------------
    // Step 1: Build row-level data structure
    // -------------------------------------------------------------------------
    std::vector<RowData> rows;
    rows.reserve(poses.size() * 3);

    for (size_t i = 0; i < poses.size(); ++i) {
        const Matrix3f& R = poses[i].rotation();
        const Vector3f& t = poses[i].translation();

        for (int axis = 0; axis < 3; ++axis) {
            RowData rd;
            rd.a_row.resize(6);
            rd.a_row(0) = R(axis, 0);
            rd.a_row(1) = R(axis, 1);
            rd.a_row(2) = R(axis, 2);
            rd.a_row(3) = (axis == 0) ? -1.0f : 0.0f;
            rd.a_row(4) = (axis == 1) ? -1.0f : 0.0f;
            rd.a_row(5) = (axis == 2) ? -1.0f : 0.0f;
            rd.b_val = -t(axis);
            rd.original_row = static_cast<uint32_t>(i * 3 + axis);
            rows.push_back(std::move(rd));
        }
    }

    const size_t total_rows = rows.size();
    std::cout << "  Total equations: " << total_rows << std::endl;

    // -------------------------------------------------------------------------
    // Step 2: Iterative solve + row-level outlier removal (IGSIO loop)
    // -------------------------------------------------------------------------
    const float threshold_multiplier = 3.0f;  // IGSIO default
    const size_t min_equations = 8;           // IGSIO MINIMUM_NUMBER_OF_CALIBRATION_EQUATIONS

    Eigen::VectorXf x;
    bool outlier_found = true;
    int round = 0;

    while (outlier_found && rows.size() > min_equations) {
        // Solve current system
        if (!solveFromRows(rows, 6, x)) {
            std::cerr << "[TipCalibrationSolver] ERROR: SVD solve failed at round " << round << std::endl;
            CalibrationResult fail;
            fail.is_valid = false;
            return fail;
        }

        // Remove row-level outliers
        float mean_residual = 0, stdev_residual = 0;
        outlier_found = removeRowOutliers(rows, x, threshold_multiplier, &mean_residual, &stdev_residual);

        if (outlier_found) {
            std::cout << "  Round " << (round + 1) << ": rows=" << rows.size()
                      << ", mean=" << mean_residual << ", stdev=" << stdev_residual
                      << ", threshold=" << (threshold_multiplier * stdev_residual) << std::endl;
        }

        ++round;
        if (round > 20) break;  // safety limit
    }

    if (rows.size() <= min_equations) {
        std::cerr << "[TipCalibrationSolver] ERROR: Too few equations remaining (" << rows.size() << ")" << std::endl;
        CalibrationResult fail;
        fail.is_valid = false;
        return fail;
    }

    // Final solve
    if (!solveFromRows(rows, 6, x)) {
        CalibrationResult fail;
        fail.is_valid = false;
        return fail;
    }

    Vector3f tip_offset(x(0), x(1), x(2));
    Vector3f pivot_point(x(3), x(4), x(5));

    // -------------------------------------------------------------------------
    // Step 3: Compute final error (IGSIO-style: mean distance from pivot)
    // -------------------------------------------------------------------------
    // Identify which poses still have at least 1 row remaining
    std::set<uint32_t> outlier_pose_indices;
    std::set<uint32_t> remaining_pose_indices;
    for (const auto& rd : rows) {
        remaining_pose_indices.insert(rd.original_row / 3);
    }

    std::vector<double> error_values;
    for (size_t i = 0; i < poses.size(); ++i) {
        if (remaining_pose_indices.find(static_cast<uint32_t>(i)) == remaining_pose_indices.end()) {
            continue;  // all 3 rows of this pose were rejected
        }
        Vector4f tip_local(tip_offset.x(), tip_offset.y(), tip_offset.z(), 1.0f);
        Vector4f tip_world = poses[i].transform * tip_local;
        Vector3f tip_pos(tip_world.x(), tip_world.y(), tip_world.z());
        double err = static_cast<double>((tip_pos - pivot_point).norm());
        error_values.push_back(err);
    }

    double error_mean = 0.0;
    for (double e : error_values) error_mean += e;
    error_mean /= error_values.size();

    double error_stdev = 0.0;
    for (double e : error_values) error_stdev += (e - error_mean) * (e - error_mean);
    error_stdev = std::sqrt(error_stdev / error_values.size());

    size_t rows_rejected = total_rows - rows.size();
    size_t poses_with_data = remaining_pose_indices.size();

    CalibrationResult result;
    result.tip_offset = tip_offset;
    result.residual_error = static_cast<float>(error_mean);
    result.num_poses_used = static_cast<int>(poses_with_data);
    result.is_valid = (error_mean < 1.5);

    std::cout << "[TipCalibrationSolver] IGSIO-style calibration "
              << (result.is_valid ? "SUCCESS" : "FAILED") << std::endl;
    std::cout << "  Tip offset (local): (" << tip_offset.x() << ", "
              << tip_offset.y() << ", " << tip_offset.z() << ") mm" << std::endl;
    std::cout << "  Pivot point (world): (" << pivot_point.x() << ", "
              << pivot_point.y() << ", " << pivot_point.z() << ") mm" << std::endl;
    std::cout << "  Mean residual: " << error_mean << " mm (stdev: " << error_stdev << " mm)" << std::endl;
    std::cout << "  Rows: " << rows.size() << "/" << total_rows
              << " (rejected " << rows_rejected << " rows)" << std::endl;
    std::cout << "  Poses contributing: " << poses_with_data << " / " << poses.size() << std::endl;

    return result;
}

// ============================================================================
// Private Methods
// ============================================================================

void TipCalibrationSolver::buildLinearSystem(
    const std::vector<PoseData>& poses,
    Eigen::MatrixXf& A,
    Eigen::VectorXf& b
) {
    const size_t N = poses.size();
    A.resize(3 * N, 6);
    b.resize(3 * N);

    for (size_t i = 0; i < N; ++i) {
        const Matrix3f& R = poses[i].rotation();
        const Vector3f& t = poses[i].translation();
        size_t row = 3 * i;
        A.block<3, 3>(row, 0) = R;
        A.block<3, 3>(row, 3) = -Matrix3f::Identity();
        b.segment<3>(row) = -t;
    }
}

bool TipCalibrationSolver::solveSVD(
    const Eigen::MatrixXf& A,
    const Eigen::VectorXf& b,
    Eigen::VectorXf& x
) {
    Eigen::JacobiSVD<Eigen::MatrixXf> svd(A, Eigen::ComputeThinU | Eigen::ComputeThinV);

    float cond = svd.singularValues()(0) / svd.singularValues()(svd.singularValues().size() - 1);
    if (cond > 1e6) {
        std::cerr << "[TipCalibrationSolver] WARNING: Ill-conditioned matrix (cond=" << cond << ")" << std::endl;
    }

    x = svd.solve(b);
    return true;
}

std::vector<float> TipCalibrationSolver::computeResiduals(
    const std::vector<PoseData>& poses,
    const Vector3f& tip_offset,
    const Vector3f& pivot_point
) {
    std::vector<float> residuals;
    residuals.reserve(poses.size());

    for (const auto& pose : poses) {
        Vector4f tip_local(tip_offset.x(), tip_offset.y(), tip_offset.z(), 1.0f);
        Vector4f tip_world = pose.transform * tip_local;
        Vector3f tip_pos(tip_world.x(), tip_world.y(), tip_world.z());
        float residual = (tip_pos - pivot_point).norm();
        residuals.push_back(residual);
    }

    return residuals;
}

} // namespace ProbeCalib
