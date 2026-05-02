/**
 * Geometry Finder - Auto-detect which geometry file matches your probe
 */

#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <fstream>

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

int main(int argc, char** argv) {
    std::cout << "==========================================\n";
    std::cout << "   Geometry Finder for Atracsys Probe\n";
    std::cout << "==========================================\n\n";
    std::cout << "Put your probe in front of the camera!\n\n";

    // List of 4-point geometry files to test
    std::vector<std::string> geometryFiles = {
        "geometry012.ini",
        "geometry014.ini",
        "geometry020.ini",
        "geometry022.ini",
        "geometry032.ini",
        "geometry034.ini",
        "geometry052.ini",
        "geometry054.ini",
        "geometry062.ini",
        "geometry064.ini",
        "geometry072.ini",
        "geometry074.ini",
        "geometry102.ini",
        "geometry104.ini",
        "geometry202.ini",
        "geometry204.ini",
        "geometry302.ini",
        "geometry304.ini",
        "stylus.ini"
    };

    std::string sdkDataPath = "E:\\ICPtry\\Atracsys\\fusionTrack SDK x64\\data\\";

    // Initialize SDK
    ftkLibrary lib = ftkInit();
    if (lib == nullptr) {
        std::cerr << "ERROR: Cannot initialize SDK\n";
        return 1;
    }

    // Find device
    DeviceEnumData enumData = {0, ftkDeviceType::DEV_UNKNOWN_DEVICE, false};
    ftkEnumerateDevices(lib, deviceEnumCallback, &enumData);

    if (!enumData.found) {
        std::cerr << "ERROR: No device found\n";
        ftkClose(&lib);
        return 1;
    }

    uint64 sn = enumData.serial;
    std::cout << "Device found: " << sn << "\n\n";

    // Create frame
    ftkFrameQuery* frame = ftkCreateFrame();
    ftkSetFrameOptions(false, false, 16u, 16u, 0u, 16u, frame);

    // Test each geometry
    std::string bestMatch = "";
    int bestCount = 0;

    for (const auto& geomFile : geometryFiles) {
        std::string fullPath = sdkDataPath + geomFile;

        // Read geometry file
        std::ifstream file(fullPath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            continue;
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        ftkBuffer buffer;
        buffer.reset();
        file.read(buffer.data, size);
        buffer.size = static_cast<uint32>(size);
        file.close();

        // Load geometry
        ftkRigidBody geom{};
        if (ftkLoadRigidBodyFromFile(lib, &buffer, &geom) != ftkError::FTK_OK) {
            continue;
        }

        if (ftkSetRigidBody(lib, sn, &geom) != ftkError::FTK_OK) {
            continue;
        }

        std::cout << "Testing: " << geomFile << " (ID=" << geom.geometryId << ") ... ";
        std::cout.flush();

        // Wait a bit for tracking to stabilize
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Count successful detections over 2 seconds
        int detections = 0;
        int frames = 0;

        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(2)) {
            ftkError err = ftkGetLastFrame(lib, sn, frame, 100u);

            if (err == ftkError::FTK_OK && frame->markersStat == ftkQueryStatus::QS_OK) {
                frames++;
                if (frame->markersCount > 0) {
                    detections++;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        float rate = frames > 0 ? (100.0f * detections / frames) : 0;
        std::cout << detections << "/" << frames << " (" << (int)rate << "%)\n";

        if (detections > bestCount) {
            bestCount = detections;
            bestMatch = geomFile;
        }

        // Clear geometry for next test
        geom.geometryId = 0;
        ftkSetRigidBody(lib, sn, &geom);
    }

    // Results
    std::cout << "\n==========================================\n";
    std::cout << "RESULT:\n";
    std::cout << "==========================================\n";

    if (bestCount > 10) {
        std::cout << "Best match: " << bestMatch << " (" << bestCount << " detections)\n";
        std::cout << "\nUse this command:\n";
        std::cout << "  probe_calibration.exe \"" << sdkDataPath << bestMatch << "\"\n";
    } else {
        std::cout << "No good match found!\n";
        std::cout << "Your probe geometry is not in the standard files.\n";
        std::cout << "You need to create a custom geometry file.\n";
    }

    // Cleanup
    ftkDeleteFrame(frame);
    ftkClose(&lib);

    std::cout << "\nPress Enter to exit...";
    std::cin.get();

    return 0;
}
