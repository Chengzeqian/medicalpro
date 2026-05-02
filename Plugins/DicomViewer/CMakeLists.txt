# ============================================================================
# DicomViewer Plugin Configuration
# ============================================================================
cmake_minimum_required(VERSION 3.16)

project(DicomViewer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets OpenGL Sql PrintSupport)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets OpenGL Sql PrintSupport)

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
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/ITKFactoryRegistration")
    include(${ITK_USE_FILE})
endif()

    set(DICOMVIEWER_PLATFORM_SOURCES
        dicom_viewer_module.cpp
        DicomViewerServiceImpl.cpp
        DicomViewerWidget.cpp
        DicomDataStructures.cpp
    )

    set(DICOMVIEWER_PLATFORM_HEADERS
        dicom_viewer_module.h
        DicomViewerService.h
        DicomViewerServiceImpl.h
        DicomViewerWidget.h
        DicomDataStructures.h
    )

    add_library(DicomViewerPlatformModuleLib STATIC
        ${DICOMVIEWER_PLATFORM_SOURCES}
        ${DICOMVIEWER_PLATFORM_HEADERS}
    )

    target_include_directories(DicomViewerPlatformModuleLib PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${VTK_INCLUDE_DIRS}
        ${ITK_INCLUDE_DIRS}
    )

    target_link_libraries(DicomViewerPlatformModuleLib PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::OpenGL
        Qt${QT_VERSION_MAJOR}::Sql
        Qt${QT_VERSION_MAJOR}::PrintSupport
        ${VTK_LIBRARIES}
        ${ITK_LIBRARIES}
    )

    register_platform_descriptor(DicomViewer "${CMAKE_CURRENT_SOURCE_DIR}/platform/plugin.json")

    message(STATUS "DicomViewer platform module configured successfully")
