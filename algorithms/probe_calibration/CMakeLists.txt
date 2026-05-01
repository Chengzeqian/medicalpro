cmake_minimum_required(VERSION 3.18)
project(ProbeCalibration LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(PROBECALIB_BUILD_SHARED "Build ProbeCalibration DLL with C API" ON)
option(PROBECALIB_BUILD_TOOLS "Build ProbeCalibration standalone tools" OFF)

if(MSVC)
    add_compile_options(/utf-8)
endif()

set(PROBE_CALIBRATION_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(PROBE_CALIBRATION_EIGEN_DIR "${MEDICALPRO_EIGEN_ROOT}")
set(PROBE_CALIBRATION_SDK_DIR "${MEDICALPRO_ATRACSYS_SDK_DIR}")

if(NOT EXISTS "${PROBE_CALIBRATION_EIGEN_DIR}/Eigen/Core")
    message(FATAL_ERROR "ProbeCalibration requires Eigen at ${PROBE_CALIBRATION_EIGEN_DIR}")
endif()

if(NOT EXISTS "${PROBE_CALIBRATION_SDK_DIR}/include/ftkInterface.h")
    message(FATAL_ERROR "Atracsys SDK not found at ${PROBE_CALIBRATION_SDK_DIR}")
endif()

include_directories(
    "${PROBE_CALIBRATION_INCLUDE_DIR}"
    "${PROBE_CALIBRATION_EIGEN_DIR}"
    "${PROBE_CALIBRATION_SDK_DIR}/include"
)

find_library(PROBE_CALIBRATION_FUSIONTRACK_LIBRARY
    NAMES fusionTrack64
    PATHS "${PROBE_CALIBRATION_SDK_DIR}/lib"
    NO_DEFAULT_PATH
)

if(NOT PROBE_CALIBRATION_FUSIONTRACK_LIBRARY)
    message(FATAL_ERROR "ProbeCalibration requires fusionTrack64 in ${PROBE_CALIBRATION_SDK_DIR}/lib")
endif()

set(PROBE_CALIBRATION_ATNET_HEADER "${PROBE_CALIBRATION_SDK_DIR}/atnet/atnetInterface.h")
find_library(PROBE_CALIBRATION_ATNET_LIBRARY
    NAMES atnetLib64
    PATHS "${PROBE_CALIBRATION_SDK_DIR}/lib"
    NO_DEFAULT_PATH
)

set(PROBE_CALIBRATION_CORE_HEADERS
    include/atracsys_tracker.h
    include/calibration_recorder.h
    include/meshgpu_adapter.h
    include/probe_calibration_c_api.h
    include/realtime_point_cloud_collector.h
    include/realtime_transform.h
    include/tip_calibration_solver.h
    include/types.h
    include/voxel_fuser.h
)

set(PROBE_CALIBRATION_CORE_SOURCES
    src/atracsys_tracker.cpp
    src/calibration_recorder.cpp
    src/realtime_transform.cpp
    src/tip_calibration_solver.cpp
)

set(PROBE_CALIBRATION_C_API_SOURCES
    src/probe_calibration_c_api.cpp
)

add_library(ProbeCalibrationLib STATIC
    ${PROBE_CALIBRATION_CORE_HEADERS}
    ${PROBE_CALIBRATION_CORE_SOURCES}
)

target_include_directories(ProbeCalibrationLib PUBLIC
    "${PROBE_CALIBRATION_INCLUDE_DIR}"
    "${PROBE_CALIBRATION_EIGEN_DIR}"
    "${PROBE_CALIBRATION_SDK_DIR}/include"
)

target_link_libraries(ProbeCalibrationLib PUBLIC
    "${PROBE_CALIBRATION_FUSIONTRACK_LIBRARY}"
)

if(PROBECALIB_BUILD_SHARED)
    add_library(ProbeCalibrationDLL SHARED
        ${PROBE_CALIBRATION_CORE_HEADERS}
        ${PROBE_CALIBRATION_CORE_SOURCES}
        ${PROBE_CALIBRATION_C_API_SOURCES}
    )

    target_compile_definitions(ProbeCalibrationDLL PRIVATE PROBECALIB_CAPI_EXPORTS)

    target_include_directories(ProbeCalibrationDLL PUBLIC
        "${PROBE_CALIBRATION_INCLUDE_DIR}"
        "${PROBE_CALIBRATION_EIGEN_DIR}"
        "${PROBE_CALIBRATION_SDK_DIR}/include"
    )

    target_link_libraries(ProbeCalibrationDLL PUBLIC
        "${PROBE_CALIBRATION_FUSIONTRACK_LIBRARY}"
    )

    set_target_properties(ProbeCalibrationDLL PROPERTIES
        OUTPUT_NAME "ProbeCalibration"
    )
endif()

if(PROBECALIB_BUILD_TOOLS)
    add_executable(probe_calibration
        src/main.cpp
    )

    target_link_libraries(probe_calibration PRIVATE
        ProbeCalibrationLib
    )

    add_executable(collection_test
        ${PROBE_CALIBRATION_CORE_SOURCES}
        src/main_collection_test.cpp
    )

    target_link_libraries(collection_test PRIVATE
        "${PROBE_CALIBRATION_FUSIONTRACK_LIBRARY}"
    )

    add_executable(geometry_finder
        src/geometry_finder.cpp
    )

    target_link_libraries(geometry_finder PRIVATE
        "${PROBE_CALIBRATION_FUSIONTRACK_LIBRARY}"
    )

    add_executable(geometry_creator
        src/geometry_creator.cpp
    )

    target_link_libraries(geometry_creator PRIVATE
        "${PROBE_CALIBRATION_FUSIONTRACK_LIBRARY}"
    )

    if(EXISTS "${PROBE_CALIBRATION_ATNET_HEADER}" AND PROBE_CALIBRATION_ATNET_LIBRARY)
        add_executable(atnet_discover
            src/atnet_discover.cpp
        )

        target_include_directories(atnet_discover PRIVATE
            "${PROBE_CALIBRATION_SDK_DIR}/atnet"
        )

        target_link_libraries(atnet_discover PRIVATE
            "${PROBE_CALIBRATION_ATNET_LIBRARY}"
        )
    endif()
endif()

if(WIN32 AND PROBECALIB_BUILD_SHARED)
    add_custom_command(TARGET ProbeCalibrationDLL POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROBE_CALIBRATION_SDK_DIR}/bin/fusionTrack64.dll"
            "$<TARGET_FILE_DIR:ProbeCalibrationDLL>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROBE_CALIBRATION_SDK_DIR}/bin/device64.dll"
            "$<TARGET_FILE_DIR:ProbeCalibrationDLL>"
    )
endif()

message(STATUS "ProbeCalibration Atracsys SDK: ${PROBE_CALIBRATION_SDK_DIR}")
message(STATUS "ProbeCalibration Eigen: ${PROBE_CALIBRATION_EIGEN_DIR}")
