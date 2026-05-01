cmake_minimum_required(VERSION 3.18)

option(MESHGPU_ENABLE_CUDA "Build CUDA core targets and demos" ON)
option(MESHGPU_BUILD_TOOLS "Build MeshGPU CLI tools and demos" OFF)
option(BUILD_ASCEND_STUB_PLUGIN "Build sample Ascend backend plugin template" OFF)
option(BUILD_ASCEND_ALGO_SAMPLE "Build sample Ascend algorithm hook plugin" OFF)
option(BUILD_ASCEND_CANN_KERNEL_SAMPLE "Build sample external CANN kernel hook plugin" OFF)

if(MESHGPU_ENABLE_CUDA)
    project(MeshGPU LANGUAGES CXX CUDA)
else()
    project(MeshGPU LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/utf-8>)
endif()

set(MESHGPU_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(MESHGPU_EIGEN_DIR "${MEDICALPRO_EIGEN_ROOT}")

if(NOT MESHGPU_ENABLE_CUDA)
    message(STATUS "MeshGPU core build skipped because MESHGPU_ENABLE_CUDA=OFF")
    return()
endif()

if(NOT EXISTS "${MESHGPU_EIGEN_DIR}/Eigen/Core")
    message(FATAL_ERROR "MeshGPU requires Eigen at ${MESHGPU_EIGEN_DIR}")
endif()

include_directories("${MESHGPU_INCLUDE_DIR}" "${MESHGPU_EIGEN_DIR}")

set(MESHGPU_CORE_SOURCES
    src/mesh_gpu.cu
    src/mesh_kernels.cu
    src/gicp_kernels.cu
    src/gicp_registration.cu
    src/rotation_search.cpp
    src/rotation_search_kernels.cu
    src/mesh_gpu_interface.cu
    src/incremental_registration.cu
)

set(MESHGPU_ASCEND_PLUGIN_SOURCES
    src/ascend_backend_stub.cpp
    src/ascend_algo_sample.cpp
    src/ascend_cann_kernel_sample.cpp
)

set(MESHGPU_TOOL_SOURCES
    src/main.cu
    src/main_ascend_direct.cpp
    src/main_backend_probe.cpp
    src/main_benchmark.cu
    src/main_new.cu
    src/main_realtime.cu
    src/main_unified_backend.cpp
    src/main_visualization.cu
    src/normalize_mesh.cpp
    src/visualizer.cpp
)

find_package(CUDAToolkit REQUIRED)

add_library(MeshGPULib SHARED
    ${MESHGPU_CORE_SOURCES}
)

target_include_directories(MeshGPULib PUBLIC
    "${MESHGPU_INCLUDE_DIR}"
    "${MESHGPU_EIGEN_DIR}"
)

target_compile_options(MeshGPULib PRIVATE
    $<$<COMPILE_LANGUAGE:CUDA>:--extended-lambda>
    $<$<COMPILE_LANGUAGE:CUDA>:-O3>
)

target_link_libraries(MeshGPULib PUBLIC
    CUDA::cudart
)

set_target_properties(MeshGPULib PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON
    CUDA_RESOLVE_DEVICE_SYMBOLS ON
)

add_custom_command(TARGET MeshGPULib POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:MeshGPULib>"
        "${CMAKE_BINARY_DIR}/Release/MeshGPULib.dll"
    COMMENT "Syncing MeshGPULib runtime DLL to unified output directory"
)

target_compile_definitions(MeshGPULib PRIVATE MESHGPU_EXPORTS)

if(MSVC)
    set_target_properties(MeshGPULib PROPERTIES
        LINK_FLAGS "/NODEFAULTLIB:LIBCMT"
    )
endif()

if(MESHGPU_BUILD_TOOLS)
    add_executable(mesh_gpu
        src/main.cu
        ${MESHGPU_CORE_SOURCES}
    )

    target_include_directories(mesh_gpu PRIVATE
        "${MESHGPU_INCLUDE_DIR}"
        "${MESHGPU_EIGEN_DIR}"
    )

    target_compile_options(mesh_gpu PRIVATE
        $<$<COMPILE_LANGUAGE:CUDA>:--extended-lambda>
        $<$<COMPILE_LANGUAGE:CUDA>:-O3>
    )

    target_link_libraries(mesh_gpu PRIVATE
        CUDA::cudart
    )

    set_target_properties(mesh_gpu PROPERTIES
        CUDA_SEPARABLE_COMPILATION ON
        CUDA_RESOLVE_DEVICE_SYMBOLS ON
    )

    if(MSVC)
        set_target_properties(mesh_gpu PROPERTIES
            LINK_FLAGS "/NODEFAULTLIB:LIBCMT"
        )
    endif()

    target_compile_definitions(mesh_gpu PRIVATE MESHGPU_STATIC)

    add_executable(mesh_gpu_backend_probe
        src/main_backend_probe.cpp
    )

    target_include_directories(mesh_gpu_backend_probe PRIVATE
        "${MESHGPU_INCLUDE_DIR}"
        "${MESHGPU_EIGEN_DIR}"
    )

    target_link_libraries(mesh_gpu_backend_probe PRIVATE
        MeshGPULib
    )

    add_executable(mesh_gpu_unified
        src/main_unified_backend.cpp
    )

    target_include_directories(mesh_gpu_unified PRIVATE
        "${MESHGPU_INCLUDE_DIR}"
        "${MESHGPU_EIGEN_DIR}"
    )

    target_link_libraries(mesh_gpu_unified PRIVATE
        MeshGPULib
    )

    add_executable(mesh_gpu_ascend_direct
        src/main_ascend_direct.cpp
    )

    target_compile_features(mesh_gpu_ascend_direct PRIVATE cxx_std_14)
    target_include_directories(mesh_gpu_ascend_direct PRIVATE
        "${MESHGPU_INCLUDE_DIR}"
        "${MESHGPU_EIGEN_DIR}"
    )

    add_executable(normalize_mesh
        src/normalize_mesh.cpp
    )

    target_compile_features(normalize_mesh PRIVATE cxx_std_14)
    target_include_directories(normalize_mesh PRIVATE
        "${MESHGPU_INCLUDE_DIR}"
        "${MESHGPU_EIGEN_DIR}"
    )

    if(MSVC)
        target_compile_options(normalize_mesh PRIVATE /W3)
    endif()

    add_executable(mesh_gpu_vis
        src/main_visualization.cu
        ${MESHGPU_CORE_SOURCES}
    )

    target_include_directories(mesh_gpu_vis PRIVATE
        "${MESHGPU_INCLUDE_DIR}"
        "${MESHGPU_EIGEN_DIR}"
    )

    target_compile_options(mesh_gpu_vis PRIVATE
        $<$<COMPILE_LANGUAGE:CUDA>:--extended-lambda>
        $<$<COMPILE_LANGUAGE:CUDA>:-O3>
    )

    target_link_libraries(mesh_gpu_vis PRIVATE
        CUDA::cudart
    )

    set_target_properties(mesh_gpu_vis PROPERTIES
        CUDA_SEPARABLE_COMPILATION ON
        CUDA_RESOLVE_DEVICE_SYMBOLS ON
    )

    if(MSVC)
        set_target_properties(mesh_gpu_vis PROPERTIES
            LINK_FLAGS "/NODEFAULTLIB:LIBCMT"
        )
    endif()

    target_compile_definitions(mesh_gpu_vis PRIVATE MESHGPU_STATIC)

    add_executable(mesh_gpu_benchmark
        src/main_benchmark.cu
        ${MESHGPU_CORE_SOURCES}
    )

    target_include_directories(mesh_gpu_benchmark PRIVATE
        "${MESHGPU_INCLUDE_DIR}"
        "${MESHGPU_EIGEN_DIR}"
    )

    target_compile_options(mesh_gpu_benchmark PRIVATE
        $<$<COMPILE_LANGUAGE:CUDA>:--extended-lambda>
        $<$<COMPILE_LANGUAGE:CUDA>:-O3>
    )

    target_link_libraries(mesh_gpu_benchmark PRIVATE
        CUDA::cudart
    )

    set_target_properties(mesh_gpu_benchmark PROPERTIES
        CUDA_SEPARABLE_COMPILATION ON
        CUDA_RESOLVE_DEVICE_SYMBOLS ON
    )

    if(MSVC)
        set_target_properties(mesh_gpu_benchmark PROPERTIES
            LINK_FLAGS "/NODEFAULTLIB:LIBCMT"
        )
    endif()

    target_compile_definitions(mesh_gpu_benchmark PRIVATE MESHGPU_STATIC)

    find_package(Open3D QUIET)

    if(Open3D_FOUND)
        add_executable(mesh_gpu_realtime
            src/main_realtime.cu
            src/visualizer.cpp
            ${MESHGPU_CORE_SOURCES}
        )

        target_include_directories(mesh_gpu_realtime PRIVATE
            "${MESHGPU_INCLUDE_DIR}"
            "${MESHGPU_EIGEN_DIR}"
            ${Open3D_INCLUDE_DIRS}
        )

        target_compile_options(mesh_gpu_realtime PRIVATE
            $<$<COMPILE_LANGUAGE:CUDA>:--extended-lambda>
            $<$<COMPILE_LANGUAGE:CUDA>:-O3>
        )

        target_link_libraries(mesh_gpu_realtime PRIVATE
            CUDA::cudart
            Open3D::Open3D
        )

        set_target_properties(mesh_gpu_realtime PROPERTIES
            CUDA_SEPARABLE_COMPILATION ON
            CUDA_RESOLVE_DEVICE_SYMBOLS ON
        )

        if(MSVC)
            set_target_properties(mesh_gpu_realtime PROPERTIES
                LINK_FLAGS "/NODEFAULTLIB:LIBCMT"
            )
        endif()

        target_compile_definitions(mesh_gpu_realtime PRIVATE MESHGPU_STATIC)
    endif()
endif()

if(BUILD_ASCEND_STUB_PLUGIN)
    add_library(meshgpu_ascend_backend_plugin SHARED
        src/ascend_backend_stub.cpp
    )

    target_compile_features(meshgpu_ascend_backend_plugin PRIVATE cxx_std_14)
    target_include_directories(meshgpu_ascend_backend_plugin PRIVATE
        "${MESHGPU_INCLUDE_DIR}"
        "${MESHGPU_EIGEN_DIR}"
    )

    set_target_properties(meshgpu_ascend_backend_plugin PROPERTIES
        OUTPUT_NAME "mesgpu_ascend_backend"
    )
endif()

if(BUILD_ASCEND_ALGO_SAMPLE)
    add_library(meshgpu_ascend_algo_sample SHARED
        src/ascend_algo_sample.cpp
    )

    target_compile_features(meshgpu_ascend_algo_sample PRIVATE cxx_std_14)
    target_include_directories(meshgpu_ascend_algo_sample PRIVATE
        "${MESHGPU_INCLUDE_DIR}"
        "${MESHGPU_EIGEN_DIR}"
    )

    set_target_properties(meshgpu_ascend_algo_sample PROPERTIES
        OUTPUT_NAME "mesgpu_ascend_algo"
    )
endif()

if(BUILD_ASCEND_CANN_KERNEL_SAMPLE)
    add_library(meshgpu_ascend_cann_kernel_sample SHARED
        src/ascend_cann_kernel_sample.cpp
    )

    target_compile_features(meshgpu_ascend_cann_kernel_sample PRIVATE cxx_std_14)
    target_include_directories(meshgpu_ascend_cann_kernel_sample PRIVATE
        "${MESHGPU_INCLUDE_DIR}"
        "${MESHGPU_EIGEN_DIR}"
    )

    set_target_properties(meshgpu_ascend_cann_kernel_sample PROPERTIES
        OUTPUT_NAME "mesgpu_ascend_cann_kernel"
    )
endif()
