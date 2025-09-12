cmake_minimum_required(VERSION 3.16)
project(MedicalProcessing VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set encoding to UTF-8
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Qt组件查找
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets)

# ITK 配置 (现代方式，避免使用已弃用的ITK_USE_FILE)
find_package(ITK QUIET)
if(ITK_FOUND)
    message(STATUS "MedicalProcessing: ITK found - Version ${ITK_VERSION}")
else()
    message(WARNING "MedicalProcessing: ITK not found - processing features will be limited")
    return()
endif()

# 注意：不再使用 include(${ITK_USE_FILE})，因为它在ITK 5.x中已被弃用

# CTK Plugin Framework 配置 (继承父项目设置)
if(CTK_FOUND)
    # Plugin sources (完全CTK架构 + 现代化UI)
    set(PLUGIN_SOURCES
        # CTK框架必需文件
        MedicalProcessingActivator.h
        MedicalProcessingActivator.cpp
        
        # 服务接口和实现（完全CTK架构）
        MedicalProcessingService.h
        MedicalProcessingServiceImpl.h
        MedicalProcessingServiceImpl.cpp
        
        # 服务接口声明（完全CTK架构）
        ServiceInterfaces.h
        
        # 现代化UI界面组件（医学图像处理工作台）
        MedicalProcessingWidget.h
        MedicalProcessingWidget.cpp
        
        # 资源文件
        MedicalProcessing.qrc
    )
    
    # 🔑 关键：使用标准add_library创建插件
    add_library(MedicalProcessing SHARED ${PLUGIN_SOURCES})
    
    # 确保依赖顺序：MedicalProcessing必须在MedicalImageCore之后构建
    add_dependencies(MedicalProcessing MedicalImageCore)
    
    # 设置插件属性
    set_target_properties(MedicalProcessing PROPERTIES
        OUTPUT_NAME "MedicalProcessing"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/plugins"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/plugins"
    )
    
    # 链接库
    target_link_libraries(MedicalProcessing PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        ${CTK_LIBRARIES}
        # 遵循CTK架构：不直接链接其他插件DLL，通过服务接口通信
    )

    # ITK 库链接 (现代方式)
    if(ITK_FOUND)
        target_link_libraries(MedicalProcessing PRIVATE ${ITK_LIBRARIES})
        target_include_directories(MedicalProcessing PRIVATE ${ITK_INCLUDE_DIRS})
        target_compile_definitions(MedicalProcessing PRIVATE ITK_FOUND)
        
        # 抑制ITK 5.x的meta data警告和信息输出
        if(MSVC)
            target_compile_options(MedicalProcessing PRIVATE 
                "/wd4996"  # 忽略弃用警告
                "/wd4244"  # 忽略类型转换警告
            )
        endif()
        
        # ITK特定配置，减少冗余输出
        target_compile_definitions(MedicalProcessing PRIVATE
            ITK_LEGACY_SILENT  # 抑制ITK遗留代码警告
            ITK_IO_FACTORY_REGISTER_MANAGER  # 静态注册IO工厂
        )
    endif()
    
    # 包含目录（遵循完全CTK架构）
    target_include_directories(MedicalProcessing PRIVATE
        ${CTK_INCLUDE_DIRS}
        # 注意：完全CTK架构下不包含其他插件或共享数据结构
        # 所有通信通过CTK服务接口完成
    )


    # 编译定义
    target_compile_definitions(MedicalProcessing PRIVATE 
        CTK_PLUGIN_FRAMEWORK
        CTK_PLUGIN_FRAMEWORK_EXPORT
        ITK_FOUND
    )
    
    message(STATUS "MedicalProcessing configured successfully (PatientManagement-style)")
else()
    message(WARNING "CTK Plugin Framework not available - MedicalProcessing will not be built")
endif()