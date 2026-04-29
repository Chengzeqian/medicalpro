cmake_minimum_required(VERSION 3.16)

project(FourViewDisplay VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)

if(NOT VTK_FOUND)
    message(WARNING "VTK not found in parent project - FourViewDisplay may not build correctly")
endif()

if(NOT ITK_FOUND)
    message(WARNING "ITK not found in parent project - FourViewDisplay may not build correctly")
endif()

    set(FOURVIEWDISPLAY_PLATFORM_SOURCES
        four_view_display_module.cpp
        FourViewDisplayServiceImpl.cpp
        FourViewVTKWidget.cpp
    )

    set(FOURVIEWDISPLAY_PLATFORM_HEADERS
        four_view_display_module.h
        FourViewDisplayService.h
        FourViewDisplayServiceImpl.h
        FourViewVTKWidget.h
    )

    add_library(FourViewDisplayPlatformModuleLib STATIC
        ${FOURVIEWDISPLAY_PLATFORM_SOURCES}
        ${FOURVIEWDISPLAY_PLATFORM_HEADERS}
    )

    target_include_directories(FourViewDisplayPlatformModuleLib PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${VTK_INCLUDE_DIRS}
        ${ITK_INCLUDE_DIRS}
    )

    target_link_libraries(FourViewDisplayPlatformModuleLib PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        ${VTK_LIBRARIES}
        ${ITK_LIBRARIES}
    )

    register_platform_descriptor(FourViewDisplay "${CMAKE_CURRENT_SOURCE_DIR}/platform/plugin.json")

    message(STATUS "FourViewDisplay platform module configured successfully")
