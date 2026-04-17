# InstrumentManagement Plugin CMakeLists.txt

cmake_minimum_required(VERSION 3.16)

project(InstrumentManagement VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set encoding to UTF-8
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Qt组件查找
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets Sql)

# CTK Plugin Framework 配置
if(CTK_FOUND)
    set(PLUGIN_SOURCES
        InstrumentManagementActivator.cpp
        InstrumentManagementServiceImpl.cpp
    )

    set(PLUGIN_HEADERS
        InstrumentManagementActivator.h
        InstrumentManagementService.h
        InstrumentManagementServiceImpl.h
        InstrumentDataStructures.h
    )

    set(PLUGIN_RESOURCES
        InstrumentManagement.qrc
    )

    add_medical_plugin(InstrumentManagement
        SOURCES ${PLUGIN_SOURCES}
        HEADERS ${PLUGIN_HEADERS}
        RESOURCES ${PLUGIN_RESOURCES}
        DEPENDENCIES Qt${QT_VERSION_MAJOR}::Sql
    )

    if(VTK_FOUND)
        target_link_libraries(InstrumentManagement PRIVATE ${VTK_LIBRARIES})
        target_include_directories(InstrumentManagement PRIVATE ${VTK_INCLUDE_DIRS})
        target_compile_definitions(InstrumentManagement PRIVATE VTK_FOUND)
        message(STATUS "InstrumentManagement: TODO - Refactor to use Framework's PUBLIC VTK linking")
    endif()

    message(STATUS "InstrumentManagement configured successfully")
else()
    message(WARNING "CTK Plugin Framework not available - InstrumentManagement will not be built")
endif()

