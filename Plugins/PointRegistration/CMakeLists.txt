# ============================================================================
# PointRegistration Plugin Configuration
# ============================================================================
cmake_minimum_required(VERSION 3.16)

project(PointRegistration VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)

    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)

    set(POINTREGISTRATION_PLATFORM_SOURCES
        PointRegistrationServiceImpl.cpp
        ProbeSimulator.cpp
        RegistrationWorkflow.cpp
        target_sensitive_point_selector.cpp
        point_registration_module.cpp
        widgets/PointRegistrationWidget.cpp
        internal/PointRegistrationVTKWidget.cpp
    )

    set(POINTREGISTRATION_PLATFORM_HEADERS
        PointRegistrationService.h
        PointRegistrationServiceImpl.h
        PointRegistrationDataStructures.h
        ProbeSimulator.h
        RegistrationWorkflow.h
        target_sensitive_point_selector.h
        point_registration_module.h
        widgets/PointRegistrationWidget.h
        internal/PointRegistrationVTKWidget.h
    )

    add_library(PointRegistrationPlatformModuleLib STATIC
        ${POINTREGISTRATION_PLATFORM_SOURCES}
        ${POINTREGISTRATION_PLATFORM_HEADERS}
    )

    target_include_directories(PointRegistrationPlatformModuleLib PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(PointRegistrationPlatformModuleLib PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        RegistrationCorePlatformModuleLib
    )

    register_platform_descriptor(PointRegistration "${CMAKE_CURRENT_SOURCE_DIR}/platform/plugin.json")

    if(VTK_FOUND)
        target_compile_definitions(PointRegistrationPlatformModuleLib PRIVATE VTK_FOUND)

        if(TARGET VTK::CommonCore)
            target_link_libraries(PointRegistrationPlatformModuleLib PUBLIC
                VTK::CommonCore
                VTK::CommonMath
                VTK::CommonTransforms
                VTK::FiltersCore
                VTK::FiltersGeneral
                VTK::FiltersSources
                VTK::GUISupportQt
                VTK::InteractionStyle
                VTK::IOGeometry
                VTK::IOPLY
                VTK::RenderingCore
                VTK::RenderingOpenGL2
            )

            vtk_module_autoinit(
                TARGETS PointRegistrationPlatformModuleLib
                MODULES ${VTK_LIBRARIES}
            )
        endif()

        message(STATUS "PointRegistration: VTK support enabled in platform module")
    else()
        message(WARNING "PointRegistration: VTK not found - plugin will be built without VTK features")
    endif()

    message(STATUS "PointRegistration configured successfully (platform module)")
