/**
 * @file calibration_recorder.cpp
 * @brief Calibration Pose Recorder Implementation
 */

#include "calibration_recorder.h"
#include <iostream>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ProbeCalib {

// ============================================================================
// Constructor
// ============================================================================

CalibrationRecorder::CalibrationRecorder()
    : state_(CalibrationState::Idle)
{
}

// ============================================================================
// Recording Control
// ============================================================================

void CalibrationRecorder::startRecording(const RecordingConfig& config) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    // Clear previous data
    pose_buffer_.clear();
    stats_ = RecordingStats();
    config_ = config;
    state_ = CalibrationState::Recording;

    std::cout << "[CalibrationRecorder] Recording started" << std::endl;
    std::cout << "  Target Geometry ID: " << (config_.target_geometry_id == 0 ? "Any" : std::to_string(config_.target_geometry_id)) << std::endl;
    std::cout << "  Min poses: " << config_.min_poses << std::endl;
    std::cout << "  Max poses: " << config_.max_poses << std::endl;

    if (status_callback_) {
        status_callback_(state_, stats_);
    }
}

bool CalibrationRecorder::stopRecording() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    if (state_ != CalibrationState::Recording) {
        return false;
    }

    bool success = pose_buffer_.size() >= config_.min_poses;

    if (success) {
        state_ = CalibrationState::Complete;
        std::cout << "[CalibrationRecorder] Recording stopped successfully" << std::endl;
        std::cout << "  Poses collected: " << pose_buffer_.size() << std::endl;
        std::cout << "  Angular coverage: " << stats_.angular_coverage << " degrees" << std::endl;
    } else {
        state_ = CalibrationState::Failed;
        std::cout << "[CalibrationRecorder] Recording stopped - NOT ENOUGH POSES" << std::endl;
        std::cout << "  Poses collected: " << pose_buffer_.size() << " (need " << config_.min_poses << ")" << std::endl;
    }

    if (status_callback_) {
        status_callback_(state_, stats_);
    }

    return success;
}

void CalibrationRecorder::clear() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    pose_buffer_.clear();
    stats_ = RecordingStats();
    state_ = CalibrationState::Idle;

    std::cout << "[CalibrationRecorder] Buffer cleared" << std::endl;
}

// ============================================================================
// Pose Input
// ============================================================================

bool CalibrationRecorder::addPose(const PoseData& pose) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    if (state_ != CalibrationState::Recording) {
        return false;
    }

    stats_.total_received++;

    // Check if pose is valid
    if (!pose.is_valid) {
        stats_.rejected_invalid++;
        return false;
    }

    // Check geometry ID filter
    if (config_.target_geometry_id != 0 && pose.geometry_id != config_.target_geometry_id) {
        return false;
    }

    // Check registration error
    if (pose.registration_error > config_.max_registration_error) {
        stats_.rejected_high_error++;
        return false;
    }

    // Check if pose is sufficiently different from the last one
    if (!isPoseSufficientlyDifferent(pose)) {
        stats_.rejected_similar++;
        return false;
    }

    // Accept the pose
    pose_buffer_.push_back(pose);
    stats_.total_accepted++;

    // Update mean registration error
    stats_.mean_registration_error =
        (stats_.mean_registration_error * (stats_.total_accepted - 1) + pose.registration_error) / stats_.total_accepted;

    // Trim buffer if exceeded max
    if (pose_buffer_.size() > config_.max_poses) {
        pose_buffer_.pop_front();
    }

    // Update angular coverage periodically
    if (stats_.total_accepted % 50 == 0) {
        updateAngularCoverage();
    }

    // Notify callback periodically
    if (status_callback_ && stats_.total_accepted % 100 == 0) {
        status_callback_(state_, stats_);
    }

    return true;
}

// ============================================================================
// Data Access
// ============================================================================

std::vector<PoseData> CalibrationRecorder::getRecordedPoses() const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return std::vector<PoseData>(pose_buffer_.begin(), pose_buffer_.end());
}

size_t CalibrationRecorder::getPoseCount() const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return pose_buffer_.size();
}

RecordingStats CalibrationRecorder::getStats() const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return stats_;
}

bool CalibrationRecorder::hasEnoughPoses() const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return pose_buffer_.size() >= config_.min_poses;
}

// ============================================================================
// Callbacks
// ============================================================================

void CalibrationRecorder::setStatusCallback(StatusCallback callback) {
    status_callback_ = callback;
}

// ============================================================================
// Private Methods
// ============================================================================

bool CalibrationRecorder::isPoseSufficientlyDifferent(const PoseData& pose) const {
    if (pose_buffer_.empty()) {
        return true;
    }

    const PoseData& last = pose_buffer_.back();
    float angle = calculateRotationAngle(last.rotation(), pose.rotation());

    return angle >= config_.min_rotation_change;
}

float CalibrationRecorder::calculateRotationAngle(const Matrix3f& R1, const Matrix3f& R2) const {
    // Calculate relative rotation: R_rel = R2 * R1^T
    Matrix3f R_rel = R2 * R1.transpose();

    // Calculate rotation angle from trace: trace(R) = 1 + 2*cos(theta)
    // theta = acos((trace(R) - 1) / 2)
    float trace = R_rel.trace();
    float cos_theta = (trace - 1.0f) / 2.0f;

    // Clamp to [-1, 1] to handle numerical errors
    cos_theta = std::max(-1.0f, std::min(1.0f, cos_theta));

    float angle_rad = std::acos(cos_theta);
    return angle_rad * 180.0f / M_PI; // Convert to degrees
}

void CalibrationRecorder::updateAngularCoverage() {
    if (pose_buffer_.size() < 2) {
        stats_.angular_coverage = 0.0f;
        return;
    }

    // Calculate total angular coverage by summing up rotation changes
    float total_angle = 0.0f;
    float max_angle = 0.0f;

    // Sample every 10th pose to speed up calculation
    const int step = std::max(1, static_cast<int>(pose_buffer_.size()) / 100);

    for (size_t i = step; i < pose_buffer_.size(); i += step) {
        float angle = calculateRotationAngle(pose_buffer_[i - step].rotation(), pose_buffer_[i].rotation());
        total_angle += angle;
        max_angle = std::max(max_angle, angle);
    }

    // Angular coverage is approximate - use total accumulated angle
    // A good calibration should have coverage > 90 degrees
    stats_.angular_coverage = total_angle;
}

} // namespace ProbeCalib
