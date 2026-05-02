/**
 * @file main_collection_test.cpp
 * @brief Point Cloud Collection Test with Voxel Fusion
 *
 * This program demonstrates the complete workflow:
 *   1. Initialize Atracsys tracker
 *   2. Load probe geometry and calibration
 *   3. Collect probe tip positions in real-time
 *   4. Apply voxel fusion for noise reduction
 *   5. Export "Super Points" for MeshGPU registration
 *
 * Workflow:
 *   标定 -> 融合 -> 导出给 MeshGPU
 *
 * Usage:
 *   collection_test [geometry_file.ini] [calibration_file.txt]
 *
 * Commands:
 *   [c] Start calibration
 *   [s] Stop calibration
 *   [r] Start point collection (recording)
 *   [e] Stop collection and export
 *   [p] Print statistics
 *   [x] Clear collected data
 *   [q] Quit
 */

#include "realtime_transform.h"
#include "realtime_point_cloud_collector.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <fstream>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#define GET_CHAR() _getch()
#define KBHIT() _kbhit()
#else
#include <termios.h>
#include <unistd.h>
int GET_CHAR() {
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}
#define KBHIT() 0
#endif

using namespace ProbeCalib;

// Search paths for geometry files
const std::vector<std::string> GEOMETRY_SEARCH_PATHS = {
    "./my_probe_geometry.ini",
    "./geometry/geometry420.ini",
    "../geometry/geometry420.ini",
    "E:/ICPtry/ProbeCalibration/my_probe_geometry.ini",
    "E:/ICPtry/Atracsys/fusionTrack SDK x64/data/geometry420.ini"
};

std::string findGeometryFile(const std::string& specified = "") {
    if (!specified.empty()) {
        std::ifstream test(specified);
        if (test.good()) return specified;
    }
    for (const auto& path : GEOMETRY_SEARCH_PATHS) {
        std::ifstream test(path);
        if (test.good()) {
            std::cout << "Found geometry: " << path << std::endl;
            return path;
        }
    }
    return "";
}

std::atomic<bool> g_running(true);

void printCommands() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Point Cloud Collection Commands" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  [c] Start calibration (pivot mode)" << std::endl;
    std::cout << "  [s] Stop calibration and compute" << std::endl;
    std::cout << "  [r] Start point collection (recording)" << std::endl;
    std::cout << "  [e] Stop collection and export" << std::endl;
    std::cout << "  [p] Print statistics" << std::endl;
    std::cout << "  [x] Clear collected data" << std::endl;
    std::cout << "  [h] Show this help" << std::endl;
    std::cout << "  [q] Quit" << std::endl;
    std::cout << "========================================\n" << std::endl;
}

int main(int argc, char** argv) {
    std::cout << "\n";
    std::cout << "========================================" << std::endl;
    std::cout << "  Point Cloud Collection with Fusion" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Version: 1.0" << std::endl;
    std::cout << "========================================\n" << std::endl;

    // Parse arguments
    std::string specified_geometry = (argc > 1) ? argv[1] : "";
    std::string calibration_path = (argc > 2) ? argv[2] : "probe_calibration.txt";

    // Find geometry file
    std::string geometry_path = findGeometryFile(specified_geometry);
    if (geometry_path.empty()) {
        std::cerr << "ERROR: No geometry file found!" << std::endl;
        return 1;
    }

    std::cout << "Geometry: " << geometry_path << std::endl;
    std::cout << "Calibration: " << calibration_path << std::endl;

    // Create pipeline and collector
    ProbeTrackingPipeline pipeline;
    RealtimePointCloudCollector collector(1.0f);  // 1mm voxel size

    // Set up callback for new Super Points
    collector.setNewPointCallback([](const Vector3f& pos, int count) {
        // Print occasionally to show progress
        if (count % 100 == 0) {
            std::cout << "\r  Super Points: " << count
                      << " | Latest: (" << std::fixed << std::setprecision(1)
                      << pos.x() << ", " << pos.y() << ", " << pos.z() << ")    "
                      << std::flush;
        }
    });

    // Set up callback for export ready
    collector.setExportReadyCallback([](const ExportedPointCloud& cloud) {
        std::cout << "\n\n========================================" << std::endl;
        std::cout << "Export Complete!" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "  Total Super Points: " << cloud.num_points << std::endl;

        // Save to file
        std::string filename = "super_points.txt";
        std::ofstream file(filename);
        if (file.is_open()) {
            file << "# Super Points exported from ProbeCalibration" << std::endl;
            file << "# Format: x y z" << std::endl;
            file << "# Units: mm" << std::endl;
            file << "# Total points: " << cloud.num_points << std::endl;
            for (uint32_t i = 0; i < cloud.num_points; ++i) {
                file << cloud.points_x[i] << " "
                     << cloud.points_y[i] << " "
                     << cloud.points_z[i] << std::endl;
            }
            file.close();
            std::cout << "  Saved to: " << filename << std::endl;
        }
        std::cout << "========================================\n" << std::endl;
    });

    // Initialize pipeline
    if (!pipeline.initialize(geometry_path)) {
        std::cerr << "ERROR: Failed to initialize pipeline" << std::endl;
        return 1;
    }

    // Try to load existing calibration
    if (pipeline.loadCalibration(calibration_path)) {
        std::cout << "Loaded calibration from: " << calibration_path << std::endl;

        // Transfer calibration to collector
        CalibrationResult result = pipeline.getCalibrationResult();
        collector.setCalibration(result, 420);  // geometry ID 420
    } else {
        std::cout << "No existing calibration. Press 'c' to start calibration." << std::endl;
    }

    printCommands();

    // Main loop
    while (g_running.load()) {
        if (KBHIT()) {
            int ch = GET_CHAR();

            switch (ch) {
                case 'c':
                case 'C':
                    std::cout << "\n[Starting calibration - pivot the probe tip]" << std::endl;
                    pipeline.startCalibration();
                    break;

                case 's':
                case 'S':
                    std::cout << "\n[Stopping calibration...]" << std::endl;
                    if (pipeline.finishCalibration()) {
                        pipeline.saveCalibration(calibration_path);
                        // Transfer to collector
                        CalibrationResult result = pipeline.getCalibrationResult();
                        collector.setCalibration(result, 420);
                        std::cout << "Calibration complete. Ready for collection." << std::endl;
                    }
                    break;

                case 'r':
                case 'R':
                    if (!pipeline.isCalibrated()) {
                        std::cout << "\nERROR: Must calibrate first (press 'c')" << std::endl;
                    } else if (collector.isCollecting()) {
                        std::cout << "\nAlready collecting..." << std::endl;
                    } else {
                        std::cout << "\n[Starting point collection]" << std::endl;
                        std::cout << "Move the probe over the surface..." << std::endl;
                        collector.startCollection(&pipeline);
                    }
                    break;

                case 'e':
                case 'E':
                    if (collector.isCollecting()) {
                        std::cout << "\n[Stopping collection and exporting...]" << std::endl;
                        collector.stopCollection();
                    } else {
                        std::cout << "\nNot currently collecting." << std::endl;
                    }
                    break;

                case 'p':
                case 'P':
                    collector.printStats();
                    break;

                case 'x':
                case 'X':
                    std::cout << "\n[Clearing collected data]" << std::endl;
                    collector.reset();
                    break;

                case 'h':
                case 'H':
                    printCommands();
                    break;

                case 'q':
                case 'Q':
                case 27:
                    std::cout << "\nShutting down..." << std::endl;
                    g_running = false;
                    break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Cleanup
    collector.stopCollection();
    pipeline.shutdown();

    std::cout << "\n========================================" << std::endl;
    std::cout << "Collection Test Terminated" << std::endl;
    std::cout << "========================================\n" << std::endl;

    return 0;
}
