#pragma once

/**
 * @file tip_calibration_solver.h
 * @brief Probe Tip Calibration Solver Module
 *
 * Mathematical solver for computing the probe tip offset from recorded poses.
 *
 * Algorithm (Pivot Calibration):
 * =============================
 *
 * When pivoting a probe with tip fixed at point P_world:
 *   P_world = T_marker * P_local  (for all poses)
 *
 * Where:
 *   T_marker = [R | t]  is the 4x4 marker transform (rotation R, translation t)
 *   P_local = [x, y, z, 1]^T  is the tip position in marker coordinates (UNKNOWN)
 *   P_world = [X, Y, Z, 1]^T  is the pivot point in world coordinates (UNKNOWN)
 *
 * For pose i:
 *   R_i * P_local + t_i = P_world
 *
 * Rearranging:
 *   R_i * P_local - P_world = -t_i
 *
 * This gives us a linear system:
 *   [R_i | -I] * [P_local; P_world] = -t_i
 *
 * Stacking all N poses:
 *   A * x = b
 *
 * Where:
 *   A is (3N x 6) matrix
 *   x = [P_local; P_world]^T is (6 x 1) unknown vector
 *   b is (3N x 1) vector
 *
 * Solve using least squares:
 *   x = (A^T A)^(-1) A^T b
 *
 * Or more robustly using SVD.
 */

#include "types.h"
#include <vector>

namespace ProbeCalib {

/**
 * @class TipCalibrationSolver
 * @brief Solves for probe tip offset using least squares
 */
class TipCalibrationSolver {
public:
    TipCalibrationSolver() = default;
    ~TipCalibrationSolver() = default;

    // ========================================================================
    // Calibration
    // ========================================================================

    /**
     * Solve for tip offset from recorded poses.
     *
     * @param poses Vector of recorded poses during pivot motion
     * @return Calibration result containing tip offset and residual error
     */
    CalibrationResult solve(const std::vector<PoseData>& poses);

    /**
     * Validate a calibration result by computing reprojection error.
     *
     * @param poses Vector of poses used for calibration
     * @param tip_offset The computed tip offset
     * @return RMS reprojection error in mm
     */
    float validateCalibration(const std::vector<PoseData>& poses, const Vector3f& tip_offset);

    // ========================================================================
    // Advanced Methods
    // ========================================================================

    /**
     * Solve using RANSAC for robustness against outliers.
     *
     * @param poses Vector of recorded poses
     * @param inlier_threshold Maximum residual for inlier classification (mm)
     * @param max_iterations Maximum RANSAC iterations
     * @return Calibration result
     */
    CalibrationResult solveRobust(
        const std::vector<PoseData>& poses,
        float inlier_threshold = 1.0f,
        int max_iterations = 100
    );

private:
    // Build the linear system A*x = b
    void buildLinearSystem(
        const std::vector<PoseData>& poses,
        Eigen::MatrixXf& A,
        Eigen::VectorXf& b
    );

    // Solve using SVD
    bool solveSVD(
        const Eigen::MatrixXf& A,
        const Eigen::VectorXf& b,
        Eigen::VectorXf& x
    );

    // Compute residual errors for each pose
    std::vector<float> computeResiduals(
        const std::vector<PoseData>& poses,
        const Vector3f& tip_offset,
        const Vector3f& pivot_point
    );
};

} // namespace ProbeCalib
