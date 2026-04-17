cmake_minimum_required(VERSION 3.16)

project(FourViewDisplay VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set encoding to UTF-8
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Qt组件查找
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)

# VTK查找
if(NOT VTK_FOUND)
    message(WARNING "VTK not found in parent project - FourViewDisplay may not build correctly")
endif()

# ITK查找（用于读取nii.gz文件）
if(NOT ITK_FOUND)
    message(WARNING "ITK not found in parent project - FourViewDisplay may not build correctly")
endif()

# CTK Plugin Framework 配置 (继承父项目设置)
if(CTK_FOUND)
    set(PLUGIN_SOURCES
        FourViewDisplayActivator.cpp
        FourViewDisplayServiceImpl.cpp
        FourViewVTKWidget.cpp
    )

    set(PLUGIN_HEADERS
        FourViewDisplayActivator.h
        FourViewDisplayService.h
        FourViewDisplayServiceImpl.h
        FourViewVTKWidget.h
    )

    set(PLUGIN_RESOURCES
        FourViewDisplay.qrc
    )
    
    add_medical_plugin(FourViewDisplay
        SOURCES ${PLUGIN_SOURCES}
        HEADERS ${PLUGIN_HEADERS}
        RESOURCES ${PLUGIN_RESOURCES}
        DEPENDENCIES ${ITK_LIBRARIES}
    )

    # Include directories
    target_include_directories(FourViewDisplay PRIVATE
        ${CTK_INCLUDE_DIRS}
        ${VTK_INCLUDE_DIRS}
        ${ITK_INCLUDE_DIRS}
    )

    if(VTK_FOUND)
        target_link_libraries(FourViewDisplay PRIVATE ${VTK_LIBRARIES})
    endif()
    
    # 注意：不使用 vtk_module_autoinit，因为在插件 DLL 中会引起 VTK 初始化冲突
    # 参考 DicomViewer 插件的成功经验，让 VTK 自然初始化
    
    message(STATUS "FourViewDisplay configured successfully")
else()
    message(WARNING "CTK Plugin Framework not available - FourViewDisplay will not be built")
endif()
