/**
 * @file main.cpp
 * @brief Probe Tip Calibration Main Program
 *
 * This program demonstrates the complete workflow for Atracsys probe tip calibration:
 *
 * 1. Initialize Atracsys SDK and connect to tracker
 * 2. Load probe geometry
 * 3. Enter calibration mode (pivot calibration)
 * 4. Doctor pivots probe tip on a fixed point
 * 5. Compute tip offset using least squares
 * 6. Apply offset in real-time tracking
 *
 * Usage:
 *   probe_calibration [geometry_file.ini] [calibration_file.txt]
 *
 * Commands during operation:
 *   'c' - Start calibration recording
 *   's' - Stop calibration and compute
 *   'q' - Quit
 *   'l' - Load calibration from file
 *   'w' - Save calibration to file
 */

#include "realtime_transform.h"
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
#endif

using namespace ProbeCalib;

// ============================================================================
// Default geometry file paths (from futrack project - verified working)
// ============================================================================
const std::string DEFAULT_PROBE_GEOMETRY = "geometry/geometry40.ini";      // 探针
const std::string DEFAULT_REFERENCE_GEOMETRY = "geometry/geometry20.ini"; // 参考标记

// Alternative paths to search for geometry files
const std::vector<std::string> GEOMETRY_SEARCH_PATHS = {
    "./geometry/geometry40.ini",
    "../geometry/geometry40.ini",
    "E:/ICPtry/ProbeCalibration/geometry/geometry40.ini",
    "E:/ICPtry/futrack/medicalview_0/geometry/geometry40.ini",
    "E:/ICPtry/Atracsys/fusionTrack SDK x64/data/geometry072.ini"
};

/**
 * Find a valid geometry file from search paths
 */
std::string findGeometryFile(const std::string& specified_path = "") {
    // If user specified a path, try it first
    if (!specified_path.empty()) {
        std::ifstream test(specified_path);
        if (test.good()) {
            return specified_path;
        }
        std::cerr << "WARNING: Specified geometry file not found: " << specified_path << std::endl;
    }

    // Search default paths
    for (const auto& path : GEOMETRY_SEARCH_PATHS) {
        std::ifstream test(path);
        if (test.good()) {
            std::cout << "Found geometry file: " << path << std::endl;
            return path;
        }
    }

    return "";
}

// Global flag for clean shutdown
std::atomic<bool> g_running(true);

void printUsage(const char* program) {
    std::cout << "\nUsage: " << program << " [geometry_file.ini] [calibration_file.txt]\n" << std::endl;
    std::cout << "Arguments:" << std::endl;
    std::cout << "  geometry_file.ini   Path to the probe geometry file (optional)" << std::endl;
    std::cout << "                      Default: geometry/geometry40.ini" << std::endl;
    std::cout << "  calibration_file    (Optional) Load/save calibration from/to this file\n" << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << program << "                                    # Use default geometry40.ini" << std::endl;
    std::cout << "  " << program << " geometry/geometry40.ini            # Specify probe geometry" << std::endl;
    std::cout << "  " << program << " geometry/geometry40.ini calib.txt  # With calibration file" << std::endl;
    std::cout << "\nAvailable geometry files:" << std::endl;
    std::cout << "  geometry/geometry40.ini  - Probe (探针)" << std::endl;
    std::cout << "  geometry/geometry20.ini  - Reference marker (参考标记)" << std::endl;
}

void printCommands() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Available Commands:" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  [c] Start calibration (pivot mode)" << std::endl;
    std::cout << "  [s] Stop calibration and compute" << std::endl;
    std::cout << "  [l] Load calibration from file" << std::endl;
    std::cout << "  [w] Save calibration to file" << std::endl;
    std::cout << "  [t] Show current tip position" << std::endl;
    std::cout << "  [h] Show this help" << std::endl;
    std::cout << "  [q] Quit" << std::endl;
    std::cout << "========================================\n" << std::endl;
}

int main(int argc, char** argv) {
    std::cout << "\n";
    std::cout << "========================================" << std::endl;
    std::cout << "   Atracsys Probe Tip Calibration" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Version: 1.1 (with futrack geometry)" << std::endl;
    std::cout << "Copyright (c) 2024" << std::endl;
    std::cout << "========================================\n" << std::endl;

    // Check for help flag
    if (argc > 1 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        printUsage(argv[0]);
        return 0;
    }

    // Parse arguments - geometry file is now optional
    std::string specified_geometry = (argc > 1) ? argv[1] : "";
    std::string calibration_path = (argc > 2) ? argv[2] : "probe_calibration.txt";

    // Find geometry file
    std::string geometry_path = findGeometryFile(specified_geometry);

    if (geometry_path.empty()) {
        std::cerr << "\nERROR: No geometry file found!" << std::endl;
        std::cerr << "\nSearched paths:" << std::endl;
        for (const auto& path : GEOMETRY_SEARCH_PATHS) {
            std::cerr << "  - " << path << std::endl;
        }
        std::cerr << "\nSolutions:" << std::endl;
        std::cerr << "  1. Copy geometry files to ./geometry/ folder" << std::endl;
        std::cerr << "  2. Specify geometry file path as argument" << std::endl;
        std::cerr << "  3. Use geometry from futrack project:" << std::endl;
        std::cerr << "     E:/ICPtry/futrack/medicalview_0/geometry/geometry40.ini" << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    std::cout << "Using geometry file: " << geometry_path << std::endl;

    // Create pipeline
    ProbeTrackingPipeline pipeline;

    // Set up real-time tip callback
    pipeline.setTipCallback([](const TipPosition& tip) {
        static int count = 0;
        if (++count % 60 == 0) {  // Print once per second at 60Hz
            std::cout << "\r  Tip: ("
                      << std::fixed << std::setprecision(2)
                      << tip.position.x() << ", "
                      << tip.position.y() << ", "
                      << tip.position.z() << ") mm  "
                      << std::flush;
        }
    });

    // Initialize
    if (!pipeline.initialize(geometry_path)) {
        std::cerr << "\nERROR: Failed to initialize pipeline" << std::endl;
        std::cerr << "Please check:" << std::endl;
        std::cerr << "  1. Atracsys device is connected and powered on" << std::endl;
        std::cerr << "  2. Geometry file exists: " << geometry_path << std::endl;
        std::cerr << "  3. fusionTrack64.dll is in the executable directory" << std::endl;
        return 1;
    }

    // Try to load existing calibration
    if (pipeline.loadCalibration(calibration_path)) {
        std::cout << "Loaded existing calibration from: " << calibration_path << std::endl;
    }

    // Print commands
    printCommands();

    std::cout << "Waiting for commands (press 'h' for help)...\n" << std::endl;

    // Main loop
    while (g_running.load()) {
        // Check for keyboard input
        if (_kbhit()) {
            int ch = GET_CHAR();

            switch (ch) {
                case 'c':
                case 'C':
                    std::cout << "\n";
                    pipeline.startCalibration();
                    break;

                case 's':
                case 'S':
                    std::cout << "\n";
                    if (pipeline.finishCalibration()) {
                        // Auto-save
                        pipeline.saveCalibration(calibration_path);
                    }
                    break;

                case 'l':
                case 'L':
                    std::cout << "\nLoading calibration from: " << calibration_path << std::endl;
                    pipeline.loadCalibration(calibration_path);
                    break;

                case 'w':
                case 'W':
                    std::cout << "\nSaving calibration to: " << calibration_path << std::endl;
                    pipeline.saveCalibration(calibration_path);
                    break;

                case 't':
                case 'T': {
                    TipPosition tip;
                    if (pipeline.getTipPosition(tip)) {
                        std::cout << "\n  Current Tip Position:" << std::endl;
                        std::cout << "    Position: (" << tip.position.x() << ", "
                                  << tip.position.y() << ", " << tip.position.z() << ") mm" << std::endl;
                        std::cout << "    Orientation: (" << tip.orientation.x() << ", "
                                  << tip.orientation.y() << ", " << tip.orientation.z() << ")" << std::endl;
                    } else {
                        std::cout << "\n  No valid tip position available" << std::endl;
                        if (!pipeline.isCalibrated()) {
                            std::cout << "    (Calibration required - press 'c' to start)" << std::endl;
                        }
                    }
                    break;
                }

                case 'h':
                case 'H':
                    printCommands();
                    break;

                case 'q':
                case 'Q':
                case 27:  // ESC
                    std::cout << "\nShutting down..." << std::endl;
                    g_running = false;
                    break;
            }
        }

        // Small sleep to prevent busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Cleanup
    pipeline.shutdown();

    std::cout << "\n========================================" << std::endl;
    std::cout << "Probe Calibration Terminated" << std::endl;
    std::cout << "========================================\n" << std::endl;

    return 0;
}
