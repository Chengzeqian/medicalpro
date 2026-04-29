cmake_minimum_required(VERSION 3.16)

project(OpticalTracking VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set encoding to UTF-8
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Qt组件查找
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Network)

# Platform 模块配置 (继承父项目设置)
    set(OPTICALTRACKING_PLATFORM_SOURCES
        OpticalTrackingServiceImpl.cpp
        BoneSurfaceMotionSimulator.cpp
        optical_tracking_module.cpp
    )

    set(OPTICALTRACKING_PLATFORM_HEADERS
        OpticalTrackingService.h
        OpticalTrackingServiceImpl.h
        ServiceInterfaces.h
        BoneSurfaceMotionSimulator.h
        optical_tracking_module.h
    )

    add_library(OpticalTrackingPlatformModuleLib STATIC
        ${OPTICALTRACKING_PLATFORM_SOURCES}
        ${OPTICALTRACKING_PLATFORM_HEADERS}
    )

    target_include_directories(OpticalTrackingPlatformModuleLib PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(OpticalTrackingPlatformModuleLib PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
    )

    register_platform_descriptor(OpticalTracking "${CMAKE_CURRENT_SOURCE_DIR}/platform/plugin.json")

    # VTK依赖由Framework统一暴露（PUBLIC）。
    if(VTK_FOUND)
        target_compile_definitions(OpticalTrackingPlatformModuleLib PRIVATE VTK_FOUND)
        message(STATUS "OpticalTracking: Using Framework-provided VTK linkage")
    else()
        message(WARNING "OpticalTracking: VTK not found - plugin will be built without VTK features")
    endif()

    # ITK集成（医学图像处理支持）
    # This is correct - ITK should be linked PRIVATE only in plugins that need it
    if(ITK_FOUND)
        target_link_libraries(OpticalTrackingPlatformModuleLib PRIVATE ${ITK_LIBRARIES})
        target_include_directories(OpticalTrackingPlatformModuleLib PRIVATE ${ITK_INCLUDE_DIRS})
        target_compile_definitions(OpticalTrackingPlatformModuleLib PRIVATE ITK_FOUND)
        
        message(STATUS "OpticalTracking: ITK integration enabled (PRIVATE - plugin-specific)")
    else()
        message(STATUS "OpticalTracking: ITK not available - medical image processing limited")
    endif()
    
    # 可选的硬件SDK支持（保留但不强制依赖）
    set(ATRACSYS_SDK_ROOT "${CMAKE_SOURCE_DIR}/ThirdParty/Atracsys/fusionTrack SDK x64")
    if(EXISTS "${ATRACSYS_SDK_ROOT}/include/ftkInterface.h")
        message(STATUS "OpticalTracking: Atracsys SDK found - hardware support enabled")
        target_include_directories(OpticalTrackingPlatformModuleLib PRIVATE "${ATRACSYS_SDK_ROOT}/include")
        target_compile_definitions(OpticalTrackingPlatformModuleLib PRIVATE ATRACSYS_SDK_FOUND)
        
        # 链接Atracsys库（如果存在）
        if(EXISTS "${ATRACSYS_SDK_ROOT}/lib/fusionTrack64.lib")
            target_link_libraries(OpticalTrackingPlatformModuleLib PRIVATE "${ATRACSYS_SDK_ROOT}/lib/fusionTrack64.lib")
        endif()
    else()
        message(STATUS "OpticalTracking: Atracsys SDK not found - using simulation mode")
    endif()
    
    message(STATUS "OpticalTracking platform module configured successfully")

    # 添加 ProbeCalibration DLL 头文件路径（探针标定引擎）
    set(PROBE_CALIBRATION_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/ICPtry/ProbeCalibration/include")
    if(EXISTS "${PROBE_CALIBRATION_INCLUDE_DIR}")
        target_include_directories(OpticalTrackingPlatformModuleLib PRIVATE ${PROBE_CALIBRATION_INCLUDE_DIR})
        message(STATUS "  ProbeCalibration include: ${PROBE_CALIBRATION_INCLUDE_DIR}")
    else()
        message(WARNING "  ProbeCalibration include dir not found: ${PROBE_CALIBRATION_INCLUDE_DIR}")
    endif()
