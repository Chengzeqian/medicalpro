cmake_minimum_required(VERSION 3.16)
project(MedicalViewer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set encoding to UTF-8
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Qt组件查找
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets OpenGL)

# VTK 配置 (现代方式，无需VTK_USE_FILE)
find_package(VTK QUIET)
if(NOT VTK_FOUND)
    message(WARNING "VTK not found. Medical viewer features will be limited.")
    return()
endif()

# CTK Plugin Framework 配置 (继承父项目设置)
if(CTK_FOUND)
    # Plugin sources (完全CTK架构 + 现代化UI)
    set(PLUGIN_SOURCES
        # CTK框架必需文件
        MedicalViewerActivator.h
        MedicalViewerActivator.cpp
        
        # 服务接口和实现（完全CTK架构）
        MedicalViewerService.h
        MedicalViewerServiceImpl.h
        MedicalViewerServiceImpl.cpp
        
        # 服务接口声明（完全CTK架构）
        ServiceInterfaces.h
        
        # 现代化UI界面组件（整合了NrrdViewer功能）
        MedicalViewerWidget.h
        MedicalViewerWidget.cpp
        
        # 资源文件
        MedicalViewer.qrc
    )
    
    # 🔑 关键：使用标准add_library创建插件
    add_library(MedicalViewer SHARED ${PLUGIN_SOURCES})
    
    # 设置插件属性
    set_target_properties(MedicalViewer PROPERTIES
        OUTPUT_NAME "MedicalViewer"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/plugins"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/plugins"
    )
    
    # VTK库列表 (现代方式)
    set(VTK_LIBRARIES_TO_LINK)
    if(VTK_VERSION VERSION_GREATER_EQUAL "9.0")
        set(VTK_LIBRARIES_TO_LINK
            VTK::CommonCore
            VTK::CommonDataModel
            VTK::RenderingCore
            VTK::RenderingOpenGL2
            VTK::RenderingVolume
            VTK::RenderingVolumeOpenGL2
            VTK::InteractionStyle
            VTK::GUISupportQt
            VTK::RenderingQt
            VTK::IOImage
            VTK::ImagingCore
        )
    else()
        set(VTK_LIBRARIES_TO_LINK ${VTK_LIBRARIES})
    endif()
    
    # 链接库（遵循完全CTK架构）
    target_link_libraries(MedicalViewer PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::OpenGL
        ${CTK_LIBRARIES}
        ${VTK_LIBRARIES_TO_LINK}
        # 遵循CTK架构：不直接链接其他插件DLL，通过服务接口通信
    )

    # 包含目录（遵循完全CTK架构）
    target_include_directories(MedicalViewer PRIVATE
        ${CTK_INCLUDE_DIRS}
        # 注意：完全CTK架构下不包含其他插件或共享数据结构
        # 所有通信通过CTK服务接口完成
    )

    # VTK 包含目录 (现代方式)
    if(VTK_FOUND)
        target_include_directories(MedicalViewer PRIVATE ${VTK_INCLUDE_DIRS})
    endif()
    

    # 编译定义
    target_compile_definitions(MedicalViewer PRIVATE 
        CTK_PLUGIN_FRAMEWORK
        CTK_PLUGIN_FRAMEWORK_EXPORT
        VTK_FOUND
    )
    
    message(STATUS "MedicalViewer configured successfully (完全CTK架构)")
else()
    message(WARNING "CTK Plugin Framework not available - MedicalViewer will not be built")
endif()