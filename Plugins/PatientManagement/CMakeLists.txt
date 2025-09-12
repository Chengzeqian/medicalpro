cmake_minimum_required(VERSION 3.16)

project(PatientManagement VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set encoding to UTF-8
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Qt组件查找
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets Sql)

# CTK Plugin Framework 配置 (继承父项目设置)
if(CTK_FOUND)
    # Plugin sources
    set(PLUGIN_SOURCES
        # CTK框架必需文件
        PatientManagementActivator.h
        PatientManagementActivator.cpp
        
        # 服务接口和实现
        PatientDatabaseService.h
        PatientDatabaseServiceImpl.h
        PatientDatabaseServiceImpl.cpp
        
        # 数据结构
        PatientDataStructures.h
        PatientDataStructures.cpp
        
        # 数据库管理
        SQLiteManager.h
        SQLiteManager.cpp
        
        # UI组件
        PatientInfoWidget.h
        PatientInfoWidget.cpp
        PatientInfoWidget.ui
        PatientListWidget.h
        PatientListWidget.cpp
        PatientListWidget.ui
        PatientSearchWidget.h
        PatientSearchWidget.cpp
        PatientSearchWidget.ui
        
        # 资源文件
        PatientManagement.qrc
    )
    
    # Create plugin library
    add_library(PatientManagement SHARED ${PLUGIN_SOURCES})
    
    # Set plugin properties
    set_target_properties(PatientManagement PROPERTIES
        OUTPUT_NAME "PatientManagement"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/plugins"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/plugins"
    )
    
    # Link libraries
    target_link_libraries(PatientManagement PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Sql
        ${CTK_LIBRARIES}
    )

    # Include directories
    target_include_directories(PatientManagement PRIVATE
        ${CTK_INCLUDE_DIRS}
    )

    # Compile definitions
    target_compile_definitions(PatientManagement PRIVATE 
        CTK_PLUGIN_FRAMEWORK
        CTK_PLUGIN_FRAMEWORK_EXPORT
    )
    
    message(STATUS "PatientManagement configured successfully")
else()
    message(WARNING "CTK Plugin Framework not available - PatientManagement will not be built")
endif()
