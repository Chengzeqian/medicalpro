# ============================================================================
# RegistrationCore Plugin Configuration
# ============================================================================
cmake_minimum_required(VERSION 3.16)

project(RegistrationCore VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set encoding to UTF-8
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Qt组件查找
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)

# VTK is inherited from Framework (PUBLIC), no need to find_package here

# ============================================================================
# Plugin Library Configuration
# ============================================================================
if(CTK_FOUND)
    set(PLUGIN_SOURCES
        RegistrationActivator.cpp
        RegistrationServiceImpl.cpp
    )

    set(PLUGIN_HEADERS
        RegistrationActivator.h
        RegistrationService.h
        RegistrationServiceImpl.h
    )

    set(PLUGIN_RESOURCES
        RegistrationCore.qrc
    )

    # 添加 Registration2D3D 插件头文件路径
    include_directories(${CMAKE_SOURCE_DIR}/Plugins/Registration2D3D)

    # 添加 MeshGPU 头文件路径
    set(MESHGPU_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/ICPtry/MeshGPU/include")
    if(EXISTS "${MESHGPU_INCLUDE_DIR}")
        include_directories(${MESHGPU_INCLUDE_DIR})
        message(STATUS "  MeshGPU include: ${MESHGPU_INCLUDE_DIR}")
    else()
        message(WARNING "  MeshGPU include dir not found: ${MESHGPU_INCLUDE_DIR}")
    endif()

    add_medical_plugin(RegistrationCore
        SOURCES ${PLUGIN_SOURCES}
        HEADERS ${PLUGIN_HEADERS}
        RESOURCES ${PLUGIN_RESOURCES}
        DEPENDENCIES
            Qt${QT_VERSION_MAJOR}::Core
            Qt${QT_VERSION_MAJOR}::Gui
            Qt${QT_VERSION_MAJOR}::Widgets
    )

    set(MESHGPU_IMPORT_LIB "")
    if(EXISTS "${CMAKE_SOURCE_DIR}/ICPtry/MeshGPU/build/Release/MeshGPULib.lib")
        set(MESHGPU_IMPORT_LIB "${CMAKE_SOURCE_DIR}/ICPtry/MeshGPU/build/Release/MeshGPULib.lib")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/ICPtry/MeshGPU/build/Debug/MeshGPULib.lib")
        set(MESHGPU_IMPORT_LIB "${CMAKE_SOURCE_DIR}/ICPtry/MeshGPU/build/Debug/MeshGPULib.lib")
    endif()

    if(NOT MESHGPU_IMPORT_LIB)
        message(FATAL_ERROR "MeshGPU import library not found. Expected under ICPtry/MeshGPU/build/[Release|Debug].")
    endif()

    target_link_libraries(RegistrationCore PRIVATE ${MESHGPU_IMPORT_LIB})
    message(STATUS "  MeshGPU import lib: ${MESHGPU_IMPORT_LIB}")

    set(MESHGPU_DLL "")
    if(EXISTS "${CMAKE_SOURCE_DIR}/ICPtry/MeshGPU/build/Release/MeshGPULib.dll")
        set(MESHGPU_DLL "${CMAKE_SOURCE_DIR}/ICPtry/MeshGPU/build/Release/MeshGPULib.dll")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/ICPtry/MeshGPU/build/Debug/MeshGPULib.dll")
        set(MESHGPU_DLL "${CMAKE_SOURCE_DIR}/ICPtry/MeshGPU/build/Debug/MeshGPULib.dll")
    endif()

    if(MESHGPU_DLL)
        add_custom_command(TARGET RegistrationCore POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MESHGPU_DLL}"
                "${CMAKE_BINARY_DIR}/$<CONFIG>/MeshGPULib.dll"
            COMMENT "Copying MeshGPULib runtime DLL to application output directory"
        )
        message(STATUS "  MeshGPU dll copy scheduled: ${MESHGPU_DLL}")
    else()
        message(WARNING "  MeshGPU dll not found: ${MESHGPU_DLL} - GPU-GICP runtime will not be available")
    endif()

    message(STATUS "RegistrationCore plugin configured as MODULE library")
    message(STATUS "  - Supports: Landmark, ICP, 2D-3D registration (via Registration2D3D plugin)")
else()
    message(WARNING "CTK Plugin Framework not available - RegistrationCore will not be built")
endif()
