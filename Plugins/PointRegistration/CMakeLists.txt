# ============================================================================
# PointRegistration Plugin Configuration
# 基于点的配准插件 - 提供手术导航中的点配准功能
# ============================================================================
cmake_minimum_required(VERSION 3.16)

project(PointRegistration VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set encoding to UTF-8
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Qt组件查找
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)

# ============================================================================
# CTK Plugin Framework Configuration
# ============================================================================
if(CTK_FOUND)
    # Enable Qt MOC for this plugin
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)
    
    # 插件源文件
    set(PLUGIN_SOURCES
        PointRegistrationActivator.cpp
        PointRegistrationServiceImpl.cpp
        ProbeSimulator.cpp
        RegistrationWorkflow.cpp
        widgets/PointRegistrationWidget.cpp
        internal/PointRegistrationVTKWidget.cpp
    )

    # 插件头文件
    set(PLUGIN_HEADERS
        PointRegistrationActivator.h
        PointRegistrationService.h
        PointRegistrationServiceImpl.h
        PointRegistrationDataStructures.h
        ProbeSimulator.h
        RegistrationWorkflow.h
        widgets/PointRegistrationWidget.h
        internal/PointRegistrationVTKWidget.h
    )

    # 资源文件
    set(PLUGIN_RESOURCES
        PointRegistration.qrc
    )
    
    # 使用统一的插件构建宏
    add_medical_plugin(PointRegistration
        SOURCES ${PLUGIN_SOURCES}
        HEADERS ${PLUGIN_HEADERS}
        RESOURCES ${PLUGIN_RESOURCES}
        DEPENDENCIES
            # 额外依赖
    )

    # VTK支持（用于vtkLandmarkTransform和3D点选取）
    if(VTK_FOUND)
        target_compile_definitions(PointRegistration PRIVATE VTK_FOUND)

        # VTK模块初始化（VTK 8.90+需要）
        vtk_module_autoinit(
            TARGETS PointRegistration
            MODULES ${VTK_LIBRARIES}
        )

        message(STATUS "PointRegistration: VTK support enabled (vtkLandmarkTransform, 3D picking)")
    else()
        message(WARNING "PointRegistration: VTK not found - plugin will be built without VTK features")
    endif()
    
    message(STATUS "PointRegistration Plugin configured successfully")
    message(STATUS "  - Plugin will be built to: ${CMAKE_BINARY_DIR}/$<CONFIG>/plugins")
else()
    message(WARNING "CTK Plugin Framework not available - PointRegistration will not be built")
endif()

