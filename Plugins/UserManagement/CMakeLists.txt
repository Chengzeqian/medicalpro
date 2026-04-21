# ============================================================================
# UserManagement Plugin Configuration (Requirement 1.2, 1.5)
# ============================================================================
cmake_minimum_required(VERSION 3.16)

project(UserManagement VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set encoding to UTF-8
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Qt组件查找
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets Sql)

# ============================================================================
# Plugin Library Configuration (MODULE - Requirement 1.2)
# ============================================================================
# Design Decision (ADR-002): Why MODULE library?
# - Plugins are dynamically loaded at runtime by CTK framework
# - MODULE libraries are not linked at build time, only loaded via dlopen/LoadLibrary
# - This enables plugin hot-swapping and reduces startup memory footprint
# ============================================================================
if(CTK_FOUND)
    # Plugin sources
    set(PLUGIN_SOURCES
        # CTK框架必需文件
        UserManagementActivator.cpp
        
        # 服务接口和实现
        UserManagementServiceImpl.cpp
        
        # 数据结构
        UserDataStructures.cpp
        
        # 资源文件
    )

    set(PLUGIN_HEADERS
        UserManagementActivator.h
        UserManagementService.h
        UserManagementServiceImpl.h
        UserDataStructures.h
    )

    set(PLUGIN_RESOURCES
        UserManagement.qrc
    )

    add_medical_plugin(UserManagement
        SOURCES ${PLUGIN_SOURCES}
        HEADERS ${PLUGIN_HEADERS}
        RESOURCES ${PLUGIN_RESOURCES}
        DEPENDENCIES Qt${QT_VERSION_MAJOR}::Sql
    )

    add_custom_command(TARGET UserManagement POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/MANIFEST.MF"
            "$<TARGET_FILE_DIR:UserManagement>/UserManagement.manifest"
        COMMENT "Copying UserManagement manifest sidecar for runtime contract verification"
        VERBATIM
    )
    
    message(STATUS "UserManagement plugin configured as MODULE library")
else()
    message(WARNING "CTK Plugin Framework not available - UserManagement will not be built")
endif()
