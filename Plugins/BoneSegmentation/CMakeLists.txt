# ============================================================================
# BoneSegmentation Plugin Configuration
# ============================================================================
cmake_minimum_required(VERSION 3.16)

project(BoneSegmentation VERSION 1.0.0 LANGUAGES CXX)

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
# ITK Dependencies (用于 DICOM 处理)
# ============================================================================
find_package(ITK REQUIRED COMPONENTS
    ITKImageIO
    ITKIOImageBase
    ITKIOGDCM
    ITKIONIFTI
    ITKImageIntensity
    ITKCommon
    ITKImageFilterBase
)
if(ITK_FOUND)
    message(STATUS "BoneSegmentation: ITK found - Version ${ITK_VERSION}")
    include(${ITK_USE_FILE})
endif()

# VTK is inherited from Framework (PUBLIC), no need to find_package here

# ============================================================================
# Plugin Library Configuration
# ============================================================================
if(CTK_FOUND)
    set(PLUGIN_SOURCES
        SegmentationActivator.cpp
        SegmentationServiceImpl.cpp
    )

    set(PLUGIN_HEADERS
        SegmentationActivator.h
        SegmentationService.h
        SegmentationServiceImpl.h
    )

    set(PLUGIN_RESOURCES
        BoneSegmentation.qrc
    )

    add_medical_plugin(BoneSegmentation
        SOURCES ${PLUGIN_SOURCES}
        HEADERS ${PLUGIN_HEADERS}
        RESOURCES ${PLUGIN_RESOURCES}
        DEPENDENCIES
            Qt${QT_VERSION_MAJOR}::Core
            Qt${QT_VERSION_MAJOR}::Gui
            Qt${QT_VERSION_MAJOR}::Widgets
            ${ITK_LIBRARIES}
    )

    message(STATUS "BoneSegmentation plugin configured as MODULE library")
else()
    message(WARNING "CTK Plugin Framework not available - BoneSegmentation will not be built")
endif()