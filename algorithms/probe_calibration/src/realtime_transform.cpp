/**
 * @file realtime_transform.cpp
 * @brief Real-time Tip Position Transform Implementation
 */

#include "realtime_transform.h"
#include "calibration_recorder.h"
#include "tip_calibration_solver.h"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace ProbeCalib {

// ============================================================================
// RealtimeTransform Implementation
// ============================================================================

RealtimeTransform::RealtimeTransform()
    : tip_offset_(Vector3f::Zero())
    , target_geometry_id_(0)
    , is_calibrated_(false)
    , total_computed_(0)
{
}

void RealtimeTransform::setTipOffset(const Vector3f& offset, uint32_t geometry_id) {
    tip_offset_ = offset;
    target_geometry_id_ = geometry_id;
    is_calibrated_ = true;

    std::cout << "[RealtimeTransform] Tip offset set:" << std::endl;
    std::cout << "  Offset: (" << offset.x() << ", " << offset.y() << ", " << offset.z() << ") mm" << std::endl;
    std::cout << "  Geometry ID: " << geometry_id << std::endl;
}

void RealtimeTransform::setTipOffset(const CalibrationResult& result, uint32_t geometry_id) {
    if (result.is_valid) {
        setTipOffset(result.tip_offset, geometry_id);
    } else {
        std::cerr << "[RealtimeTransform] WARNING: Invalid calibration result" << std::endl;
    }
}

TipPosition RealtimeTransform::computeTipPosition(const PoseData& pose) {
    TipPosition tip;

    // Check if calibrated
    if (!is_calibrated_.load()) {
        tip.is_valid = false;
        return tip;
    }

    // Check geometry ID (if filtering is enabled)
    if (target_geometry_id_ != 0 && pose.geometry_id != target_geometry_id_) {
        tip.is_valid = false;
        return tip;
    }

    // Check if pose is valid
    if (!pose.is_valid) {
        tip.is_valid = false;
        return tip;
    }

    // Core computation: P_tip_world = T_marker * P_tip_local
    Vector4f tip_local(tip_offset_.x(), tip_offset_.y(), tip_offset_.z(), 1.0f);
    Vector4f tip_world = pose.transform * tip_local;

    tip.position = Vector3f(tip_world.x(), tip_world.y(), tip_world.z());

    // Extract orientation (Z-axis of marker in world coordinates)
    // This is the third column of the rotation matrix
    tip.orientation = pose.rotation().col(2);

    tip.geometry_id = pose.geometry_id;
    tip.timestamp_us = pose.timestamp_us;
    tip.registration_error = pose.registration_error;
    tip.is_valid = true;

    return tip;
}

void RealtimeTransform::processPose(const PoseData& pose) {
    TipPosition tip = computeTipPosition(pose);

    if (tip.is_valid) {
        // Update latest
        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            latest_tip_ = tip;
        }

        total_computed_++;

        // Invoke callback
        if (tip_callback_) {
            tip_callback_(tip);
        }
    }
}

void RealtimeTransform::setTipCallback(TipCallback callback) {
    tip_callback_ = callback;
}

TipPosition RealtimeTransform::getLatestTipPosition() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return latest_tip_;
}

void RealtimeTransform::reset() {
    tip_offset_ = Vector3f::Zero();
    target_geometry_id_ = 0;
    is_calibrated_ = false;
    total_computed_ = 0;

    std::lock_guard<std::mutex> lock(data_mutex_);
    latest_tip_ = TipPosition();
}

// ============================================================================
// ProbeTrackingPipeline Implementation
// ============================================================================

ProbeTrackingPipeline::ProbeTrackingPipeline()
    : is_initialized_(false)
    , has_geometry_configured_(false)
    , geometry_id_(0)
{
}

ProbeTrackingPipeline::~ProbeTrackingPipeline() {
    shutdown();
}

bool ProbeTrackingPipeline::initialize(const std::string& geometry_path) {
    return configureGeometry(geometry_path, 0);
}

bool ProbeTrackingPipeline::configureGeometry(const std::string& geometry_path, uint32_t geometry_id) {
    if (geometry_path.empty()) {
        std::cerr << "[ProbeTrackingPipeline] ERROR: Geometry path is empty" << std::endl;
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(geometry_path, ec) || ec) {
        std::cerr << "[ProbeTrackingPipeline] ERROR: Geometry file does not exist: " << geometry_path << std::endl;
        return false;
    }

    geometry_path_ = geometry_path;
    geometry_id_ = geometry_id;
    has_geometry_configured_ = true;
    is_initialized_ = true;

    resetCalibrationSession();

    std::cout << "[ProbeTrackingPipeline] Geometry configured" << std::endl;
    std::cout << "  Path: " << geometry_path_ << std::endl;
    std::cout << "  Geometry ID: " << geometry_id_ << std::endl;
    return true;
}

void ProbeTrackingPipeline::shutdown() {
    resetCalibrationSession();
    has_geometry_configured_ = false;
    is_initialized_ = false;
    geometry_id_ = 0;
    geometry_path_.clear();
}

void ProbeTrackingPipeline::startCalibration() {
    if (!has_geometry_configured_) {
        std::cerr << "[ProbeTrackingPipeline] ERROR: Geometry is not configured" << std::endl;
        return;
    }

    RecordingConfig config;
    config.target_geometry_id = geometry_id_;
    config.min_poses = 100;                    // 少量高质量样本（IGSIO风格）
    config.max_poses = 400;
    config.max_registration_error = 0.4f;      // 严格控制 tracker 配准误差
    config.min_rotation_change = 8.0f;         // 每个样本至少旋转8°（师兄用15°）

    recorder_.startRecording(config);

    std::cout << "\n========================================" << std::endl;
    std::cout << "CALIBRATION STARTED" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Instructions:" << std::endl;
    std::cout << "  1. Place the probe tip on a fixed point" << std::endl;
    std::cout << "  2. Rotate the probe around the tip" << std::endl;
    std::cout << "  3. Keep the tip stationary while rotating" << std::endl;
    std::cout << "  4. Cover various orientations (at least 90 deg)" << std::endl;
    std::cout << "  5. Press Enter or call finishCalibration() when done" << std::endl;
    std::cout << "========================================\n" << std::endl;
}

void ProbeTrackingPipeline::resetCalibrationSession() {
    recorder_.clear();
    calibration_result_ = CalibrationResult();
    transform_.reset();
}

bool ProbeTrackingPipeline::finishCalibration() {
    if (!recorder_.isRecording()) {
        std::cerr << "[ProbeTrackingPipeline] ERROR: Not in calibration mode" << std::endl;
        return false;
    }

    // Stop recording
    bool has_enough = recorder_.stopRecording();

    if (!has_enough) {
        std::cerr << "[ProbeTrackingPipeline] ERROR: Not enough poses recorded" << std::endl;
        return false;
    }

    // Get recorded poses
    std::vector<PoseData> poses = recorder_.getRecordedPoses();

    std::cout << "\n========================================" << std::endl;
    std::cout << "COMPUTING CALIBRATION" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  Poses recorded: " << poses.size() << std::endl;

    // --- Pre-solve quality checks (IGSIO-style) ---

    // 1. Check angular coverage: need at least 60° between most different orientations
    if (poses.size() >= 2) {
        float max_angle = 0.0f;
        const auto& R0 = poses.front().rotation();
        size_t step = poses.size() / 50;
        if (step < 1) step = 1;
        for (size_t i = 1; i < poses.size(); i += step) {
            Eigen::Matrix3f R_rel = poses[i].rotation() * R0.transpose();
            float trace = R_rel.trace();
            float cos_theta = (trace - 1.0f) / 2.0f;
            if (cos_theta > 1.0f) cos_theta = 1.0f;
            if (cos_theta < -1.0f) cos_theta = -1.0f;
            float angle = std::acos(cos_theta) * 180.0f / 3.14159265f;
            if (angle > max_angle) max_angle = angle;
        }
        std::cout << "  Angular coverage: " << max_angle << " deg" << std::endl;
        if (max_angle < 60.0f) {
            std::cerr << "[ProbeTrackingPipeline] ERROR: Insufficient angular coverage ("
                      << max_angle << " deg < 60 deg required)" << std::endl;
            std::cerr << "  -> Rotate the probe more widely around the tip!" << std::endl;
            return false;
        }
    }

    // Solve for tip offset
    calibration_result_ = solver_.solveRobust(poses);

    if (!calibration_result_.is_valid) {
        std::cerr << "[ProbeTrackingPipeline] ERROR: Calibration failed" << std::endl;
        return false;
    }

    // Apply to real-time transform
    transform_.setTipOffset(calibration_result_, geometry_id_);

    std::cout << "\n========================================" << std::endl;
    std::cout << "CALIBRATION COMPLETE!" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  Tip offset: (" << calibration_result_.tip_offset.x() << ", "
              << calibration_result_.tip_offset.y() << ", "
              << calibration_result_.tip_offset.z() << ") mm" << std::endl;
    std::cout << "  Residual error: " << calibration_result_.residual_error << " mm" << std::endl;
    std::cout << "========================================\n" << std::endl;

    return true;
}

bool ProbeTrackingPipeline::addPoseSample(const PoseData& pose) {
    if (!has_geometry_configured_) {
        return false;
    }

    const bool accepted = recorder_.isRecording() ? recorder_.addPose(pose) : false;

    if (transform_.isCalibrated()) {
        transform_.processPose(pose);
    }

    return accepted;
}

RecordingStats ProbeTrackingPipeline::getRecordingStats() const {
    return recorder_.getStats();
}

bool ProbeTrackingPipeline::saveCalibration(const std::string& path) {
    if (!calibration_result_.is_valid) {
        std::cerr << "[ProbeTrackingPipeline] ERROR: No valid calibration to save" << std::endl;
        return false;
    }

    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "[ProbeTrackingPipeline] ERROR: Cannot open file: " << path << std::endl;
        return false;
    }

    file << "# Probe Tip Calibration File" << std::endl;
    file << "# Generated by ProbeCalibration" << std::endl;
    file << "geometry_id=" << geometry_id_ << std::endl;
    file << "tip_offset_x=" << calibration_result_.tip_offset.x() << std::endl;
    file << "tip_offset_y=" << calibration_result_.tip_offset.y() << std::endl;
    file << "tip_offset_z=" << calibration_result_.tip_offset.z() << std::endl;
    file << "residual_error=" << calibration_result_.residual_error << std::endl;
    file << "num_poses_used=" << calibration_result_.num_poses_used << std::endl;

    file.close();
    std::cout << "[ProbeTrackingPipeline] Calibration saved to: " << path << std::endl;
    return true;
}

bool ProbeTrackingPipeline::loadCalibration(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[ProbeTrackingPipeline] ERROR: Cannot open file: " << path << std::endl;
        return false;
    }

    std::string line;
    CalibrationResult result;
    uint32_t geom_id = 0;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        if (key == "geometry_id") geom_id = std::stoul(value);
        else if (key == "tip_offset_x") result.tip_offset.x() = std::stof(value);
        else if (key == "tip_offset_y") result.tip_offset.y() = std::stof(value);
        else if (key == "tip_offset_z") result.tip_offset.z() = std::stof(value);
        else if (key == "residual_error") result.residual_error = std::stof(value);
        else if (key == "num_poses_used") result.num_poses_used = std::stoi(value);
    }

    file.close();

    result.is_valid = true;
    calibration_result_ = result;
    geometry_id_ = geom_id;
    has_geometry_configured_ = true;
    is_initialized_ = true;
    transform_.setTipOffset(result, geom_id);

    std::cout << "[ProbeTrackingPipeline] Calibration loaded from: " << path << std::endl;
    std::cout << "  Tip offset: (" << result.tip_offset.x() << ", "
              << result.tip_offset.y() << ", " << result.tip_offset.z() << ") mm" << std::endl;

    return true;
}

bool ProbeTrackingPipeline::getTipPosition(TipPosition& tip) {
    if (!transform_.isCalibrated()) {
        return false;
    }

    tip = transform_.getLatestTipPosition();
    return tip.is_valid;
}

void ProbeTrackingPipeline::setTipCallback(RealtimeTransform::TipCallback callback) {
    transform_.setTipCallback(callback);
}

void ProbeTrackingPipeline::onPoseReceived(const PoseData& pose) {
    const bool accepted = addPoseSample(pose);
    if (!accepted || !recorder_.isRecording()) {
        return;
    }

    static int count = 0;
    if (++count % 100 == 0) {
        const auto stats = recorder_.getStats();
        std::cout << "\r  Recording: " << stats.total_accepted << " poses accepted, "
                  << "angular coverage: " << stats.angular_coverage << " deg" << std::flush;
    }
}

} // namespace ProbeCalib
