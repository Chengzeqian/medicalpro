# ============================================================================
# BoneSegmentation Plugin Configuration
# ============================================================================
cmake_minimum_required(VERSION 3.16)

project(BoneSegmentation VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)

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
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/ITKFactoryRegistration")
    include(${ITK_USE_FILE})
endif()

    set(BONESEGMENTATION_PLATFORM_SOURCES
        SegmentationServiceImpl.cpp
        bone_segmentation_module.cpp
    )

    set(BONESEGMENTATION_PLATFORM_HEADERS
        SegmentationService.h
        SegmentationServiceImpl.h
        bone_segmentation_module.h
    )

    add_library(BoneSegmentationPlatformModuleLib STATIC
        ${BONESEGMENTATION_PLATFORM_SOURCES}
        ${BONESEGMENTATION_PLATFORM_HEADERS}
    )

    target_include_directories(BoneSegmentationPlatformModuleLib PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(BoneSegmentationPlatformModuleLib PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        ${ITK_LIBRARIES}
    )

    register_platform_descriptor(BoneSegmentation "${CMAKE_CURRENT_SOURCE_DIR}/platform/plugin.json")

    if(VTK_FOUND)
        target_compile_definitions(BoneSegmentationPlatformModuleLib PRIVATE VTK_FOUND)
        if(TARGET VTK::CommonCore)
            target_link_libraries(BoneSegmentationPlatformModuleLib PUBLIC
                VTK::CommonCore
                VTK::CommonDataModel
                VTK::FiltersCore
                VTK::FiltersGeneral
                VTK::FiltersGeometry
                VTK::FiltersModeling
                VTK::IOGeometry
                VTK::IOImage
                VTK::IOLegacy
                VTK::RenderingCore
                VTK::RenderingOpenGL2
            )

            vtk_module_autoinit(
                TARGETS BoneSegmentationPlatformModuleLib
                MODULES ${VTK_LIBRARIES}
            )
        endif()
    endif()

    message(STATUS "BoneSegmentation configured successfully (platform module)")
