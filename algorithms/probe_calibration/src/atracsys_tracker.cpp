/**
 * @file atracsys_tracker.cpp
 * @brief Atracsys SDK Data Acquisition Implementation
 */

#include "atracsys_tracker.h"
#include <ftkInterface.h>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <fstream>
#include <sstream>

namespace ProbeCalib {

// ============================================================================
// Static callback functions (cannot use lambdas with C callbacks)
// ============================================================================

struct DeviceEnumData {
    uint64_t serial;
    ftkDeviceType type;
    bool found;
};

static void _CDECL_ deviceEnumCallback(uint64 sn, void* user, ftkDeviceType type) {
    DeviceEnumData* data = static_cast<DeviceEnumData*>(user);
    data->serial = sn;
    data->type = type;
    data->found = true;
}

struct OptionEnumData {
    uint32_t embeddedProcessing;
    uint32_t imagesSending;
};

static void _CDECL_ optionEnumCallback(uint64 sn, void* user, ftkOptionsInfo* oi) {
    OptionEnumData* data = static_cast<OptionEnumData*>(user);
    std::string optName(oi->name);
    if (optName == "Enable embedded processing") {
        data->embeddedProcessing = oi->id;
    } else if (optName == "Enable images sending") {
        data->imagesSending = oi->id;
    }
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

AtracsysTracker::AtracsysTracker()
    : lib_(nullptr)
    , frame_(nullptr)
    , device_serial_(0)
    , device_type_(0)
    , is_tracking_(false)
    , should_stop_(false)
    , status_(TrackerStatus::Disconnected)
{
}

AtracsysTracker::~AtracsysTracker() {
    shutdown();
}

// ============================================================================
// Initialization
// ============================================================================

bool AtracsysTracker::initialize(const std::string& config_file) {
    std::cout << "[AtracsysTracker] Initializing SDK..." << std::endl;

    // Initialize the library
    ftkBuffer buffer;
    buffer.reset();

    if (!config_file.empty()) {
        std::cout << "[AtracsysTracker] Using config file: " << config_file << std::endl;
        lib_ = ftkInitExt(config_file.c_str(), &buffer);
    } else {
        lib_ = ftkInitExt(nullptr, &buffer);
    }

    if (lib_ == nullptr) {
        std::cerr << "[AtracsysTracker] ERROR: Cannot initialize fusionTrack library" << std::endl;
        if (buffer.size > 0) {
            std::cerr << "[AtracsysTracker] Details: " << buffer.data << std::endl;
        }
        std::cerr << "\n*** Network Configuration Tips ***" << std::endl;
        std::cerr << "  1. fusionTrack uses Ethernet connection (default port 3509)" << std::endl;
        std::cerr << "  2. Device default IP is usually 192.168.1.x" << std::endl;
        std::cerr << "  3. Set your PC's network adapter to same subnet:" << std::endl;
        std::cerr << "     - IP: 192.168.1.100" << std::endl;
        std::cerr << "     - Subnet: 255.255.255.0" << std::endl;
        std::cerr << "  4. Or create ftk_config.json with device IP" << std::endl;
        status_ = TrackerStatus::Error;
        return false;
    }

    // Enumerate devices
    DeviceEnumData enumData = {0, ftkDeviceType::DEV_UNKNOWN_DEVICE, false};

    if (ftkEnumerateDevices(lib_, deviceEnumCallback, &enumData) != ftkError::FTK_OK) {
        std::cerr << "[AtracsysTracker] ERROR: Failed to enumerate devices" << std::endl;
        ftkClose(&lib_);
        status_ = TrackerStatus::Error;
        return false;
    }

    if (!enumData.found) {
        std::cerr << "[AtracsysTracker] ERROR: No tracker device found!" << std::endl;
        std::cerr << "  Please ensure the device is connected and powered on." << std::endl;
        ftkClose(&lib_);
        status_ = TrackerStatus::Disconnected;
        return false;
    }

    device_serial_ = enumData.serial;
    device_type_ = static_cast<int>(enumData.type);

    std::cout << "[AtracsysTracker] Device found:" << std::endl;
    std::cout << "  Serial: " << device_serial_ << std::endl;
    std::cout << "  Type: " << getDeviceTypeName() << std::endl;

    // Create frame query structure
    frame_ = ftkCreateFrame();
    if (frame_ == nullptr) {
        std::cerr << "[AtracsysTracker] ERROR: Cannot create frame" << std::endl;
        ftkClose(&lib_);
        status_ = TrackerStatus::Error;
        return false;
    }

    // Initialize frame options
    // Parameters: getImageHeader, getPixels, rawDataLeft, rawDataRight, 3DFiducials, markers
    // Match reference project: NO images, NO fiducials, ONLY markers.
    // Requesting fiducials adds computation overhead in the SDK.
    ftkError err = ftkSetFrameOptions(false, false, 0u, 0u, 0u, 16u, frame_);
    if (err != ftkError::FTK_OK) {
        std::cerr << "[AtracsysTracker] ERROR: Cannot set frame options" << std::endl;
        ftkDeleteFrame(frame_);
        ftkClose(&lib_);
        status_ = TrackerStatus::Error;
        return false;
    }
    std::cout << "[AtracsysTracker] Frame options set: markers only (max 16)" << std::endl;

    // Configure for SpryTrack (enable embedded processing, disable image sending)
    if (device_type_ == static_cast<int>(ftkDeviceType::DEV_SPRYTRACK_180) ||
        device_type_ == static_cast<int>(ftkDeviceType::DEV_SPRYTRACK_300)) {

        OptionEnumData optData = {0, 0};
        ftkEnumerateOptions(lib_, device_serial_, optionEnumCallback, &optData);

        if (optData.embeddedProcessing != 0) {
            ftkSetInt32(lib_, device_serial_, optData.embeddedProcessing, 1);
            std::cout << "[AtracsysTracker] Enabled onboard processing" << std::endl;
        }
        if (optData.imagesSending != 0) {
            ftkSetInt32(lib_, device_serial_, optData.imagesSending, 0);
            std::cout << "[AtracsysTracker] Disabled image sending" << std::endl;
        }
    }

    status_ = TrackerStatus::Connected;
    std::cout << "[AtracsysTracker] Initialization complete!" << std::endl;
    return true;
}

bool AtracsysTracker::loadGeometry(const std::string& geometry_path) {
    if (lib_ == nullptr) {
        std::cerr << "[AtracsysTracker] ERROR: SDK not initialized" << std::endl;
        return false;
    }

    std::cout << "[AtracsysTracker] Loading geometry: " << geometry_path << std::endl;

    // Read geometry file into buffer
    std::ifstream file(geometry_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "[AtracsysTracker] ERROR: Cannot open geometry file: " << geometry_path << std::endl;
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size > BUFFER_MAX_SIZE) {
        std::cerr << "[AtracsysTracker] ERROR: Geometry file too large" << std::endl;
        return false;
    }

    ftkBuffer buffer;
    buffer.reset();
    file.read(buffer.data, size);
    buffer.size = static_cast<uint32>(size);
    file.close();

    // Parse geometry from buffer
    ftkRigidBody geom{};
    ftkError err = ftkLoadRigidBodyFromFile(lib_, &buffer, &geom);
    if (err != ftkError::FTK_OK) {
        std::cerr << "[AtracsysTracker] ERROR: Cannot parse geometry file: " << geometry_path << std::endl;
        return false;
    }

    // Set geometry on device
    err = ftkSetRigidBody(lib_, device_serial_, &geom);
    if (err != ftkError::FTK_OK) {
        std::cerr << "[AtracsysTracker] ERROR: Cannot set geometry on device" << std::endl;
        return false;
    }

    loaded_geometries_.push_back(geom.geometryId);
    std::cout << "[AtracsysTracker] Geometry loaded: ID=" << geom.geometryId
              << ", Points=" << geom.pointsCount << std::endl;

    return true;
}

void AtracsysTracker::shutdown() {
    stopTracking();

    if (frame_ != nullptr) {
        ftkDeleteFrame(frame_);
        frame_ = nullptr;
    }

    if (lib_ != nullptr) {
        ftkClose(&lib_);
        lib_ = nullptr;
    }

    status_ = TrackerStatus::Disconnected;
    std::cout << "[AtracsysTracker] Shutdown complete" << std::endl;
}

// ============================================================================
// Tracking Control
// ============================================================================

bool AtracsysTracker::startTracking() {
    if (lib_ == nullptr || frame_ == nullptr) {
        std::cerr << "[AtracsysTracker] ERROR: Not initialized" << std::endl;
        return false;
    }

    if (is_tracking_.load()) {
        std::cout << "[AtracsysTracker] Already tracking" << std::endl;
        return true;
    }

    should_stop_ = false;
    is_tracking_ = true;
    status_ = TrackerStatus::Tracking;

    tracking_thread_ = std::thread(&AtracsysTracker::trackingLoop, this);
    std::cout << "[AtracsysTracker] Tracking started (60 Hz)" << std::endl;
    return true;
}

void AtracsysTracker::stopTracking() {
    if (!is_tracking_.load()) {
        return;
    }

    should_stop_ = true;

    if (tracking_thread_.joinable()) {
        tracking_thread_.join();
    }

    is_tracking_ = false;
    status_ = TrackerStatus::Connected;
    std::cout << "[AtracsysTracker] Tracking stopped" << std::endl;
}

// ============================================================================
// Tracking Thread
// ============================================================================

void AtracsysTracker::trackingLoop() {
    using namespace std::chrono;

    std::cout << "[AtracsysTracker] Tracking loop started" << std::endl;

    while (!should_stop_.load()) {
        processFrame();
        // No sleep - ftkGetLastFrame already blocks with timeout
    }

    std::cout << "[AtracsysTracker] Tracking loop ended" << std::endl;
}

void AtracsysTracker::processFrame() {
    static int frame_count = 0;
    static int no_marker_count = 0;
    static bool first_marker_detected = false;

    // Get latest frame - short timeout to keep loop responsive
    ftkError err = ftkGetLastFrame(lib_, device_serial_, frame_, 20u);

    if (err > ftkError::FTK_OK) {
        return;
    }

    if (err < ftkError::FTK_OK && err != ftkError::FTK_WAR_NO_FRAME) {
        return;
    }

    frame_count++;

    // Check markers status
    if (frame_->markersStat != ftkQueryStatus::QS_OK &&
        frame_->markersStat != ftkQueryStatus::QS_ERR_OVERFLOW) {
        return;
    }

    // ========================================================================
    // Minimal console output - no fiducial diagnostics (fiducials not requested)
    // ========================================================================
    if (frame_->markersCount == 0) {
        no_marker_count++;

        // Only print once every 30 seconds when no marker
        if (no_marker_count % 1800 == 1) {
            std::cout << "[INFO] No marker detected - check probe visibility" << std::endl;
        }
    } else {
        if (no_marker_count > 0) {
            // Just recovered - log it
            std::cout << "[OK] Marker recovered after " << no_marker_count << " empty frames" << std::endl;
        }
        no_marker_count = 0;

        // First detection announcement
        if (!first_marker_detected) {
            first_marker_detected = true;
            std::cout << "\n[OK] Probe detected! GeomID=" << frame_->markers[0].geometryId
                      << ", RegError=" << frame_->markers[0].registrationErrorMM << "mm" << std::endl;
            std::cout << "     Ready for calibration.\n" << std::endl;
        }

        // Tracking status every ~0.5 seconds (30 frames at 60Hz)
        if (frame_count % 30 == 0) {
            std::cout << "[Tracking] Pos=("
                      << std::fixed << std::setprecision(1)
                      << frame_->markers[0].translationMM[0] << ", "
                      << frame_->markers[0].translationMM[1] << ", "
                      << frame_->markers[0].translationMM[2] << ")mm"
                      << ", Err=" << std::setprecision(2)
                      << frame_->markers[0].registrationErrorMM << "mm" << std::endl;
        }
    }

    // Process markers
    std::vector<PoseData> new_poses;
    new_poses.reserve(frame_->markersCount);

    for (size_t i = 0; i < frame_->markersCount; ++i) {
        PoseData pose;
        convertMarkerToPose(&frame_->markers[i], pose);

        if (pose.is_valid) {
            new_poses.push_back(pose);

            // Call callback if set
            if (pose_callback_) {
                pose_callback_(pose);
            }
        }
    }

    // Update current poses (thread-safe)
    {
        std::lock_guard<std::mutex> lock(pose_mutex_);
        current_poses_ = std::move(new_poses);
    }
}

void AtracsysTracker::convertMarkerToPose(const void* marker_ptr, PoseData& pose) {
    const ftkMarker* marker = static_cast<const ftkMarker*>(marker_ptr);

    pose.geometry_id = marker->geometryId;
    pose.tracking_id = marker->id;
    pose.registration_error = marker->registrationErrorMM;

    // Build 4x4 transformation matrix
    pose.transform.setIdentity();

    // Copy rotation (3x3)
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            pose.transform(row, col) = marker->rotation[row][col];
        }
    }

    // Copy translation
    pose.transform(0, 3) = marker->translationMM[0];
    pose.transform(1, 3) = marker->translationMM[1];
    pose.transform(2, 3) = marker->translationMM[2];

    // Get timestamp from frame
    if (frame_->imageHeader != nullptr) {
        pose.timestamp_us = frame_->imageHeader->timestampUS;
    }

    // Mark as valid if registration error is reasonable
    pose.is_valid = (marker->registrationErrorMM < 5.0f); // 5mm threshold
}

// ============================================================================
// Data Access
// ============================================================================

bool AtracsysTracker::getLatestPose(uint32_t geometry_id, PoseData& pose) {
    std::lock_guard<std::mutex> lock(pose_mutex_);

    for (const auto& p : current_poses_) {
        if (p.geometry_id == geometry_id) {
            pose = p;
            return true;
        }
    }

    return false;
}

std::vector<PoseData> AtracsysTracker::getAllPoses() {
    std::lock_guard<std::mutex> lock(pose_mutex_);
    return current_poses_;
}

void AtracsysTracker::setPoseCallback(PoseCallback callback) {
    pose_callback_ = callback;
}

// ============================================================================
// Status
// ============================================================================

std::string AtracsysTracker::getDeviceTypeName() const {
    switch (static_cast<ftkDeviceType>(device_type_)) {
        case ftkDeviceType::DEV_FUSIONTRACK_500: return "fusionTrack 500";
        case ftkDeviceType::DEV_FUSIONTRACK_250: return "fusionTrack 250";
        case ftkDeviceType::DEV_SPRYTRACK_180: return "spryTrack 180";
        case ftkDeviceType::DEV_SPRYTRACK_300: return "spryTrack 300";
        case ftkDeviceType::SIM_FUSIONTRACK_500: return "Simulated fusionTrack 500";
        case ftkDeviceType::SIM_FUSIONTRACK_250: return "Simulated fusionTrack 250";
        case ftkDeviceType::SIM_SPRYTRACK_180: return "Simulated spryTrack 180";
        case ftkDeviceType::SIM_SPRYTRACK_300: return "Simulated spryTrack 300";
        default: return "Unknown Device";
    }
}

} // namespace ProbeCalib
