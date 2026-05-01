/**
 * Geometry Creator - Get 3D fiducial positions and create geometry file
 * Uses Atracsys SDK to capture raw 3D point coordinates
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cmath>

#include <ftkInterface.h>

// Callback data
struct DeviceEnumData {
    uint64 serial;
    ftkDeviceType type;
    bool found;
};

static void _CDECL_ deviceEnumCallback(uint64 sn, void* user, ftkDeviceType type) {
    DeviceEnumData* data = static_cast<DeviceEnumData*>(user);
    data->serial = sn;
    data->type = type;
    data->found = true;
}

struct Point3D {
    float x, y, z;
};

int main(int argc, char** argv) {
    std::cout << "==========================================\n";
    std::cout << "   Geometry Creator for Atracsys Probe\n";
    std::cout << "==========================================\n\n";
    std::cout << "This tool will capture the 3D positions of\n";
    std::cout << "your probe's fiducials and create a geometry file.\n\n";
    std::cout << "Instructions:\n";
    std::cout << "  1. Hold the probe STILL in front of camera\n";
    std::cout << "  2. Make sure ALL 4 balls are visible\n";
    std::cout << "  3. Press Enter when ready...\n\n";

    std::cin.get();

    // Initialize SDK
    std::cout << "[1/4] Initializing SDK...\n";
    ftkLibrary lib = ftkInit();
    if (lib == nullptr) {
        std::cerr << "ERROR: Cannot initialize SDK\n";
        return 1;
    }

    // Find device
    std::cout << "[2/4] Finding device...\n";
    DeviceEnumData enumData = {0, ftkDeviceType::DEV_UNKNOWN_DEVICE, false};
    ftkEnumerateDevices(lib, deviceEnumCallback, &enumData);

    if (!enumData.found) {
        std::cerr << "ERROR: No device found\n";
        ftkClose(&lib);
        return 1;
    }

    uint64 sn = enumData.serial;
    std::cout << "  Device found: " << sn << "\n";

    // Create frame with 3D fiducials enabled
    std::cout << "[3/4] Capturing 3D fiducials...\n";
    ftkFrameQuery* frame = ftkCreateFrame();

    // Enable 3D fiducials in frame options
    // Parameters: pixels, eventsSize, leftRawDataSize, rightRawDataSize, threeDFiducialsSize, markersSize, frame
    ftkError err = ftkSetFrameOptions(false, 0u, 128u, 128u, 64u, 16u, frame);
    if (err != ftkError::FTK_OK) {
        std::cerr << "ERROR: Cannot set frame options\n";
        ftkDeleteFrame(frame);
        ftkClose(&lib);
        return 1;
    }

    // Collect multiple frames and average positions
    std::vector<std::vector<Point3D>> allCaptures;
    int targetFrames = 30;
    int capturedFrames = 0;

    std::cout << "  Capturing " << targetFrames << " frames...\n";
    std::cout << "  (Debug info will show below)\n\n";

    for (int attempt = 0; attempt < 100 && capturedFrames < targetFrames; ++attempt) {
        err = ftkGetLastFrame(lib, sn, frame, 100u);

        if (err != ftkError::FTK_OK) {
            if (attempt % 20 == 0) {
                std::cout << "  [Debug] ftkGetLastFrame error: " << (int)err << "\n";
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        // Debug: print status
        if (attempt % 10 == 0) {
            std::cout << "  [Debug] Frame " << attempt
                      << " - 3DFiducialsStat: " << (int)frame->threeDFiducialsStat
                      << ", Count: " << frame->threeDFiducialsCount << "\n";
        }

        // Accept QS_OK (0) or QS_ERR_OVERFLOW (1) - overflow just means more points than buffer
        if (frame->threeDFiducialsStat != ftkQueryStatus::QS_OK &&
            frame->threeDFiducialsStat != ftkQueryStatus::QS_ERR_OVERFLOW) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        uint32 count = frame->threeDFiducialsCount;

        if (count >= 4) {
            std::vector<Point3D> points;
            for (uint32 i = 0; i < count && i < 10; ++i) {
                Point3D p;
                p.x = frame->threeDFiducials[i].positionMM.x;
                p.y = frame->threeDFiducials[i].positionMM.y;
                p.z = frame->threeDFiducials[i].positionMM.z;
                points.push_back(p);
            }
            allCaptures.push_back(points);
            capturedFrames++;
            std::cout << "\r  Captured: " << capturedFrames << "/" << targetFrames << " frames   " << std::flush;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    std::cout << "\n";

    if (capturedFrames < 10) {
        std::cerr << "\nERROR: Not enough frames captured (" << capturedFrames << ")\n";
        std::cerr << "Make sure:\n";
        std::cerr << "  - All 4 balls are visible to camera\n";
        std::cerr << "  - Probe is held still\n";
        std::cerr << "  - Lighting is adequate\n";
        ftkDeleteFrame(frame);
        ftkClose(&lib);
        return 1;
    }

    // Check if we consistently see 4 fiducials
    int expectedCount = allCaptures[0].size();
    std::cout << "  Detected " << expectedCount << " fiducials\n";

    if (expectedCount < 4) {
        std::cerr << "\nERROR: Need at least 4 fiducials, only detected " << expectedCount << "\n";
        ftkDeleteFrame(frame);
        ftkClose(&lib);
        return 1;
    }

    // Average all detected points
    std::vector<Point3D> avgPoints(expectedCount);
    for (int i = 0; i < expectedCount; ++i) {
        avgPoints[i] = {0, 0, 0};
    }

    for (const auto& capture : allCaptures) {
        for (int i = 0; i < expectedCount && i < (int)capture.size(); ++i) {
            avgPoints[i].x += capture[i].x;
            avgPoints[i].y += capture[i].y;
            avgPoints[i].z += capture[i].z;
        }
    }

    for (int i = 0; i < expectedCount; ++i) {
        avgPoints[i].x /= capturedFrames;
        avgPoints[i].y /= capturedFrames;
        avgPoints[i].z /= capturedFrames;
    }

    // Show all detected points and let user select
    std::cout << "\n  All detected 3D points (in mm from camera):\n";
    for (int i = 0; i < expectedCount; ++i) {
        std::cout << "    [" << i << "] X=" << avgPoints[i].x
                  << ", Y=" << avgPoints[i].y
                  << ", Z=" << avgPoints[i].z << "\n";
    }

    // Find 4 points that are close together (within ~200mm, typical probe size)
    std::cout << "\n  Looking for 4 points close together (probe markers)...\n";

    std::vector<int> probeIndices;
    float maxProbeSize = 200.0f; // mm - typical probe marker span

    // Try to find a cluster of 4 points
    for (int i = 0; i < expectedCount && probeIndices.size() < 4; ++i) {
        std::vector<int> cluster;
        cluster.push_back(i);

        for (int j = 0; j < expectedCount; ++j) {
            if (i == j) continue;

            // Check if point j is close to all points in cluster
            bool closeToAll = true;
            for (int k : cluster) {
                float dx = avgPoints[j].x - avgPoints[k].x;
                float dy = avgPoints[j].y - avgPoints[k].y;
                float dz = avgPoints[j].z - avgPoints[k].z;
                float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
                if (dist > maxProbeSize) {
                    closeToAll = false;
                    break;
                }
            }
            if (closeToAll) {
                cluster.push_back(j);
            }
        }

        if (cluster.size() >= 4) {
            probeIndices = std::vector<int>(cluster.begin(), cluster.begin() + 4);
            break;
        }
    }

    if (probeIndices.size() < 4) {
        std::cout << "\n  Could not auto-detect probe markers.\n";
        std::cout << "  Please enter the indices of 4 probe markers (space-separated):\n";
        std::cout << "  Example: 0 1 2 3\n";
        std::cout << "  > ";

        probeIndices.clear();
        for (int i = 0; i < 4; ++i) {
            int idx;
            std::cin >> idx;
            if (idx >= 0 && idx < expectedCount) {
                probeIndices.push_back(idx);
            }
        }
        std::cin.ignore();
    } else {
        std::cout << "  Found probe markers at indices: ";
        for (int idx : probeIndices) std::cout << idx << " ";
        std::cout << "\n";
    }

    if (probeIndices.size() < 4) {
        std::cerr << "ERROR: Need 4 valid indices\n";
        ftkDeleteFrame(frame);
        ftkClose(&lib);
        return 1;
    }

    // Extract selected 4 points
    std::vector<Point3D> selectedPoints(4);
    for (int i = 0; i < 4; ++i) {
        selectedPoints[i] = avgPoints[probeIndices[i]];
    }

    // Transform to local coordinates (first point as origin)
    std::cout << "\n[4/4] Creating geometry file...\n";

    Point3D origin = selectedPoints[0];
    std::vector<Point3D> localPoints(4);

    for (int i = 0; i < 4; ++i) {
        localPoints[i].x = selectedPoints[i].x - origin.x;
        localPoints[i].y = selectedPoints[i].y - origin.y;
        localPoints[i].z = selectedPoints[i].z - origin.z;
    }

    // Print coordinates
    std::cout << "\n  3D Fiducial positions (local coordinates):\n";
    for (int i = 0; i < 4; ++i) {
        std::cout << "    Fiducial " << i << ": ("
                  << localPoints[i].x << ", "
                  << localPoints[i].y << ", "
                  << localPoints[i].z << ")\n";
    }

    // Calculate distances for verification
    std::cout << "\n  Distances between fiducials:\n";
    for (int i = 0; i < 4; ++i) {
        for (int j = i + 1; j < 4; ++j) {
            float dx = localPoints[i].x - localPoints[j].x;
            float dy = localPoints[i].y - localPoints[j].y;
            float dz = localPoints[i].z - localPoints[j].z;
            float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
            std::cout << "    " << i << "-" << j << ": " << dist << " mm\n";
        }
    }

    // Save geometry file
    std::string outputPath = "my_probe_geometry.ini";
    if (argc > 1) {
        outputPath = argv[1];
    }

    std::ofstream outFile(outputPath);
    if (!outFile.is_open()) {
        std::cerr << "ERROR: Cannot create output file: " << outputPath << "\n";
        ftkDeleteFrame(frame);
        ftkClose(&lib);
        return 1;
    }

    outFile << ";; Auto-generated geometry file\n";
    outFile << ";; Created by geometry_creator\n";
    outFile << "[geometry]\n";
    outFile << "count=4\n";
    outFile << "id=999\n";  // Custom ID

    for (int i = 0; i < 4; ++i) {
        outFile << "[fiducial" << i << "]\n";
        outFile << "x=" << localPoints[i].x << "\n";
        outFile << "y=" << localPoints[i].y << "\n";
        outFile << "z=" << localPoints[i].z << "\n";
    }

    outFile.close();

    std::cout << "\n==========================================\n";
    std::cout << "SUCCESS! Geometry file created:\n";
    std::cout << "  " << outputPath << "\n";
    std::cout << "==========================================\n";
    std::cout << "\nNow run:\n";
    std::cout << "  probe_calibration.exe \"" << outputPath << "\"\n";

    // Cleanup
    ftkDeleteFrame(frame);
    ftkClose(&lib);

    std::cout << "\nPress Enter to exit...";
    std::cin.get();

    return 0;
}
