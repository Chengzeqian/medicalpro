# ============================================================================
# RegistrationCore Plugin Configuration
# ============================================================================
cmake_minimum_required(VERSION 3.16)

project(RegistrationCore VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)

    set(MESHGPU_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/ICPtry/MeshGPU/include")
    if(EXISTS "${MESHGPU_INCLUDE_DIR}")
        message(STATUS "  MeshGPU include: ${MESHGPU_INCLUDE_DIR}")
    else()
        message(WARNING "  MeshGPU include dir not found: ${MESHGPU_INCLUDE_DIR}")
    endif()

    set(REGISTRATIONCORE_PLATFORM_SOURCES
        RegistrationServiceImpl.cpp
        registration_core_module.cpp
    )

    set(REGISTRATIONCORE_PLATFORM_HEADERS
        RegistrationService.h
        RegistrationServiceImpl.h
        registration_core_module.h
    )

    add_library(RegistrationCorePlatformModuleLib STATIC
        ${REGISTRATIONCORE_PLATFORM_SOURCES}
        ${REGISTRATIONCORE_PLATFORM_HEADERS}
    )

    target_include_directories(RegistrationCorePlatformModuleLib PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/Plugins/Registration2D3D
        ${VTK_INCLUDE_DIRS}
    )

    if(EXISTS "${MESHGPU_INCLUDE_DIR}")
        target_include_directories(RegistrationCorePlatformModuleLib PUBLIC
            ${MESHGPU_INCLUDE_DIR}
        )
    endif()

    target_link_libraries(RegistrationCorePlatformModuleLib PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        ${VTK_LIBRARIES}
    )

    register_platform_descriptor(RegistrationCore "${CMAKE_CURRENT_SOURCE_DIR}/platform/plugin.json")

    set(MESHGPU_IMPORT_LIB "")
    if(EXISTS "${CMAKE_SOURCE_DIR}/ICPtry/MeshGPU/build/Release/MeshGPULib.lib")
        set(MESHGPU_IMPORT_LIB "${CMAKE_SOURCE_DIR}/ICPtry/MeshGPU/build/Release/MeshGPULib.lib")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/ICPtry/MeshGPU/build/Debug/MeshGPULib.lib")
        set(MESHGPU_IMPORT_LIB "${CMAKE_SOURCE_DIR}/ICPtry/MeshGPU/build/Debug/MeshGPULib.lib")
    endif()

    if(MESHGPU_IMPORT_LIB)
        message(STATUS "  MeshGPU import lib: ${MESHGPU_IMPORT_LIB}")
    else()
        message(WARNING "  MeshGPU import library not found - RegistrationCore will rely on runtime QLibrary loading only")
    endif()

    set(MESHGPU_DLL "")
    if(EXISTS "${CMAKE_SOURCE_DIR}/ICPtry/MeshGPU/build/Release/MeshGPULib.dll")
        set(MESHGPU_DLL "${CMAKE_SOURCE_DIR}/ICPtry/MeshGPU/build/Release/MeshGPULib.dll")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/ICPtry/MeshGPU/build/Debug/MeshGPULib.dll")
        set(MESHGPU_DLL "${CMAKE_SOURCE_DIR}/ICPtry/MeshGPU/build/Debug/MeshGPULib.dll")
    endif()

    if(MESHGPU_DLL)
        add_custom_command(TARGET RegistrationCorePlatformModuleLib POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MESHGPU_DLL}"
                "${CMAKE_BINARY_DIR}/$<CONFIG>/MeshGPULib.dll"
            COMMENT "Copying MeshGPULib runtime DLL to application output directory"
        )
        message(STATUS "  MeshGPU dll copy scheduled: ${MESHGPU_DLL}")
    else()
        message(WARNING "  MeshGPU dll not found: ${MESHGPU_DLL} - GPU-GICP runtime will not be available")
    endif()

    message(STATUS "RegistrationCore platform module configured successfully")
    message(STATUS "  - Supports: Landmark, ICP, 2D-3D registration (via Registration2D3D plugin)")
