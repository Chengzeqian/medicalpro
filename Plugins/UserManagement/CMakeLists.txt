# ============================================================================
# UserManagement Plugin Configuration
# ============================================================================
cmake_minimum_required(VERSION 3.16)

project(UserManagement VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets Sql)

    set(USERMANAGEMENT_PLATFORM_SOURCES
        user_management_module.cpp
        UserManagementServiceImpl.cpp
        UserDataStructures.cpp
    )

    set(USERMANAGEMENT_PLATFORM_HEADERS
        user_management_module.h
        UserManagementService.h
        UserManagementServiceImpl.h
        UserDataStructures.h
    )

    add_library(UserManagementPlatformModuleLib STATIC
        ${USERMANAGEMENT_PLATFORM_SOURCES}
        ${USERMANAGEMENT_PLATFORM_HEADERS}
    )

    target_include_directories(UserManagementPlatformModuleLib PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(UserManagementPlatformModuleLib PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Sql
    )

    register_platform_descriptor(UserManagement "${CMAKE_CURRENT_SOURCE_DIR}/platform/plugin.json")

    message(STATUS "UserManagement platform module configured successfully")
