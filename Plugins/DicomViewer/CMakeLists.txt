# ============================================================================
# DicomViewer Plugin Configuration (Requirement 1.2, 1.5)
# ============================================================================
cmake_minimum_required(VERSION 3.16)

project(DicomViewer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set encoding to UTF-8
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Qt组件查找
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets OpenGL Sql PrintSupport)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets OpenGL Sql PrintSupport)

# ============================================================================
# ITK Dependencies (PRIVATE - Requirement 1.3)
# ============================================================================
# Linking Strategy: PRIVATE
# Reason: DicomViewer uses ITK for DICOM file reading and image processing.
#         ITK is only used internally in ServiceImpl, not exposed in interfaces.
# ============================================================================
find_package(ITK REQUIRED COMPONENTS 
    ITKImageIO
    ITKIOImageBase
    ITKIOGDCM
    ITKImageIntensity
    ITKCommon
    ITKImageFilterBase
)
if(ITK_FOUND)
    message(STATUS "DicomViewer: ITK found - Version ${ITK_VERSION}")
    include(${ITK_USE_FILE})
endif()

# VTK is inherited from Framework (PUBLIC), no need to find_package here

# ============================================================================
# Plugin Library Configuration (MODULE - Requirement 1.2)
# ============================================================================
# Design Decision (ADR-002): Why MODULE library?
# - Plugins are dynamically loaded at runtime by CTK framework
# - MODULE libraries are not linked at build time, only loaded via dlopen/LoadLibrary
# - This enables plugin hot-swapping and reduces startup memory footprint
# ============================================================================
if(CTK_FOUND)
    set(PLUGIN_SOURCES
        DicomViewerActivator.cpp
        DicomViewerServiceImpl.cpp
        DicomViewerWidget.cpp
        DicomDataStructures.cpp
    )

    set(PLUGIN_HEADERS
        DicomViewerActivator.h
        DicomViewerService.h
        DicomViewerServiceImpl.h
        DicomViewerWidget.h
        DicomDataStructures.h
    )

    set(PLUGIN_RESOURCES
        DicomViewer.qrc
    )

    add_medical_plugin(DicomViewer
        SOURCES ${PLUGIN_SOURCES}
        HEADERS ${PLUGIN_HEADERS}
        RESOURCES ${PLUGIN_RESOURCES}
        DEPENDENCIES
            Qt${QT_VERSION_MAJOR}::OpenGL
            Qt${QT_VERSION_MAJOR}::Sql
            Qt${QT_VERSION_MAJOR}::PrintSupport
            ${ITK_LIBRARIES}
    )
    
    message(STATUS "DicomViewer plugin configured as MODULE library")
else()
    message(WARNING "CTK Plugin Framework not available - DicomViewer will not be built")
endif()
