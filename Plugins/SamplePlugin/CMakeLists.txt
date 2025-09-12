cmake_minimum_required(VERSION 3.16)

project(SamplePlugin VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set encoding to UTF-8
if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

# CTK Plugin Framework 配置 (继承父项目设置)
if(CTK_FOUND)
    # Plugin sources
    set(PLUGIN_SOURCES
        SampleActivator.h
        SampleActivator.cpp
        SampleService.h
        SampleServiceImpl.h
        SampleServiceImpl.cpp
        SamplePlugin.qrc
    )
    
    # Create plugin library
    add_library(SamplePlugin SHARED ${PLUGIN_SOURCES})
    
    # Set plugin properties
    set_target_properties(SamplePlugin PROPERTIES
        OUTPUT_NAME "SamplePlugin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/plugins"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/plugins"
    )
    
    # Link libraries
    target_link_libraries(SamplePlugin PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        ${CTK_LIBRARIES}
    )

    # Include directories
    target_include_directories(SamplePlugin PRIVATE
        ${CTK_INCLUDE_DIRS}
    )

    # Compile definitions
    target_compile_definitions(SamplePlugin PRIVATE 
        CTK_PLUGIN_FRAMEWORK
        CTK_PLUGIN_FRAMEWORK_EXPORT
    )
    

    
    message(STATUS "SamplePlugin configured successfully")
else()
    message(WARNING "CTK Plugin Framework not available - SamplePlugin will not be built")
endif()
