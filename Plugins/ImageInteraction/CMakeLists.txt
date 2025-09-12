cmake_minimum_required(VERSION 3.16)

project(ImageInteraction VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set encoding to UTF-8
if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets)

# CTK Plugin Framework 配置 (继承父项目设置)
if(CTK_FOUND)
    # Plugin sources (完全CTK架构)
    set(PLUGIN_SOURCES
        # CTK框架必需文件
        ImageInteractionActivator.h
        ImageInteractionActivator.cpp
        
        # 服务接口和实现（完全CTK架构）
        ImageInteractionService.h
        ImageInteractionServiceImpl.h
        ImageInteractionServiceImpl.cpp
        
        # 服务接口声明（完全CTK架构）
        ServiceInterfaces.h
        
        # UI界面组件（统一架构命名）
        ImageInteractionWidget.h
        ImageInteractionWidget.cpp
        
        # 注意：所有独立模块已整合到ServiceImpl中
        # 遵循统一CTK架构标准
        
        # 资源文件
        ImageInteraction.qrc
    )
    
    # Create plugin library
    add_library(ImageInteraction SHARED ${PLUGIN_SOURCES})
    
    # Set plugin properties
    set_target_properties(ImageInteraction PROPERTIES
        OUTPUT_NAME "ImageInteraction"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/plugins"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/plugins"
    )
    
    # Link libraries（遵循完全CTK架构）
    target_link_libraries(ImageInteraction PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        ${CTK_LIBRARIES}
        # 遵循CTK架构：不直接链接其他插件DLL，通过服务接口通信
    )

    # VTK 配置 (现代方式，用于3D交互功能)
    find_package(VTK QUIET)
    if(VTK_FOUND)
        # VTK库列表 (现代方式)
        set(VTK_LIBRARIES_TO_LINK)
        if(VTK_VERSION VERSION_GREATER_EQUAL "9.0")
            set(VTK_LIBRARIES_TO_LINK
                VTK::CommonCore
                VTK::CommonDataModel
                VTK::RenderingCore
                VTK::InteractionStyle
                VTK::RenderingOpenGL2
            )
        else()
            set(VTK_LIBRARIES_TO_LINK ${VTK_LIBRARIES})
        endif()
        
        target_link_libraries(ImageInteraction PRIVATE ${VTK_LIBRARIES_TO_LINK})
        target_include_directories(ImageInteraction PRIVATE ${VTK_INCLUDE_DIRS})
        target_compile_definitions(ImageInteraction PRIVATE VTK_FOUND)
        
        message(STATUS "ImageInteraction: VTK integration enabled for 3D interaction")
    else()
        message(WARNING "ImageInteraction: VTK not found - 3D interaction features will be limited")
    endif()
    
    # Include directories（遵循完全CTK架构）
    target_include_directories(ImageInteraction PRIVATE
        ${CTK_INCLUDE_DIRS}
        # 注意：完全CTK架构下不包含其他插件或共享数据结构
        # 所有通信通过CTK服务接口完成
    )

    # Link directories  

    # Compile definitions
    target_compile_definitions(ImageInteraction PRIVATE 
        CTK_PLUGIN_FRAMEWORK
        CTK_PLUGIN_FRAMEWORK_EXPORT
    )
    
    message(STATUS "ImageInteraction plugin configured successfully (统一CTK架构)")
else()
    message(WARNING "CTK Plugin Framework not available - ImageInteraction will not be built")
endif()