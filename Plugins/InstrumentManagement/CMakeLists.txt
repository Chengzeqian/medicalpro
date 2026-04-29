# InstrumentManagement Plugin CMakeLists.txt

cmake_minimum_required(VERSION 3.16)

project(InstrumentManagement VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets Sql)

    set(INSTRUMENTMANAGEMENT_PLATFORM_SOURCES
        InstrumentManagementServiceImpl.cpp
        instrument_management_module.cpp
    )

    set(INSTRUMENTMANAGEMENT_PLATFORM_HEADERS
        InstrumentManagementService.h
        InstrumentManagementServiceImpl.h
        InstrumentDataStructures.h
        instrument_management_module.h
    )

    add_library(InstrumentManagementPlatformModuleLib STATIC
        ${INSTRUMENTMANAGEMENT_PLATFORM_SOURCES}
        ${INSTRUMENTMANAGEMENT_PLATFORM_HEADERS}
    )

    target_include_directories(InstrumentManagementPlatformModuleLib PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(InstrumentManagementPlatformModuleLib PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Sql
    )

    register_platform_descriptor(InstrumentManagement "${CMAKE_CURRENT_SOURCE_DIR}/platform/plugin.json")

    if(VTK_FOUND)
        target_compile_definitions(InstrumentManagementPlatformModuleLib PRIVATE VTK_FOUND)
        if(TARGET VTK::CommonCore)
            target_link_libraries(InstrumentManagementPlatformModuleLib PUBLIC
                VTK::CommonCore
                VTK::CommonDataModel
                VTK::FiltersCore
                VTK::FiltersGeneral
                VTK::IOGeometry
                VTK::IOImage
                VTK::RenderingCore
                VTK::RenderingOpenGL2
            )

            vtk_module_autoinit(
                TARGETS InstrumentManagementPlatformModuleLib
                MODULES ${VTK_LIBRARIES}
            )
        endif()

        message(STATUS "InstrumentManagement: VTK support enabled in platform module")
    endif()

    message(STATUS "InstrumentManagement configured successfully (platform module)")
