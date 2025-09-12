cmake_minimum_required(VERSION 3.16)
project(MedicalImageCore VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set encoding to UTF-8
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Qt组件查找
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Concurrent)

# ITK 配置 (现代方式，避免使用已弃用的ITK_USE_FILE)
find_package(ITK QUIET)
if(ITK_FOUND)
    message(STATUS "MedicalImageCore: ITK found - Version ${ITK_VERSION}")
else()
    message(WARNING "MedicalImageCore: ITK not found - DICOM and advanced features will be limited")
endif()

# 注意：不再使用 include(${ITK_USE_FILE})，因为它在ITK 5.x中已被弃用

# VTK 配置 (现代方式，无需VTK_USE_FILE)
find_package(VTK QUIET)
if(VTK_FOUND)
    message(STATUS "MedicalImageCore: VTK found - Version ${VTK_VERSION}")
else()
    message(WARNING "MedicalImageCore: VTK not found - 3D visualization features disabled")
endif()

# CTK Plugin Framework 配置 (继承父项目设置)
if(CTK_FOUND)
    # Plugin sources (完全CTK架构)
    set(PLUGIN_SOURCES
        # CTK框架必需文件
        MedicalImageCoreActivator.h
        MedicalImageCoreActivator.cpp
        
        # 服务接口和实现（标准CTK架构）
        MedicalImageCoreService.h
        MedicalImageCoreServiceImpl.h
        MedicalImageCoreServiceImpl.cpp
        
        # 服务接口声明（完全CTK架构）
        ServiceInterfaces.h
        
        # UI界面组件（统一架构命名）
        MedicalImageCoreWidget.h
        MedicalImageCoreWidget.cpp
        
        # 共享数据结构（从Framework/Core引入）
        ${CMAKE_SOURCE_DIR}/Framework/Core/MedicalImageData.h
        ${CMAKE_SOURCE_DIR}/Framework/Core/MedicalImageData.cpp
        
        # 资源文件
        MedicalImageCore.qrc
    )
    
    # 🔑 关键：使用标准add_library创建插件
    add_library(MedicalImageCore SHARED ${PLUGIN_SOURCES})
    
    # 设置插件属性
    set_target_properties(MedicalImageCore PROPERTIES
        OUTPUT_NAME "MedicalImageCore"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/plugins"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/plugins"
    )
    
    # 链接库
    target_link_libraries(MedicalImageCore PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Concurrent
        ${CTK_LIBRARIES}
    )

    # ITK 库链接 (现代方式)
    if(ITK_FOUND)
        target_link_libraries(MedicalImageCore PRIVATE ${ITK_LIBRARIES})
        target_include_directories(MedicalImageCore PRIVATE ${ITK_INCLUDE_DIRS})
        target_compile_definitions(MedicalImageCore PRIVATE ITK_FOUND)
        
        # ITK特定配置，减少冗余输出
        target_compile_definitions(MedicalImageCore PRIVATE
            ITK_LEGACY_SILENT  # 抑制ITK遗留代码警告
            ITK_IO_FACTORY_REGISTER_MANAGER  # 静态注册IO工厂
            ITK_FUTURE_LEGACY_REMOVE  # 移除未来遗留代码警告
            VCL_STATIC_CONST_INIT_FLOAT_NO_WARNING  # 禁用VCL浮点初始化警告
        )
        
        # 抑制ITK/VNL的meta data警告和信息输出
        if(MSVC)
            target_compile_options(MedicalImageCore PRIVATE 
                "/wd4996"  # 忽略弃用警告
                "/wd4244"  # 忽略类型转换警告
                "/wd4267"  # 忽略尺寸转换警告
                "/wd4305"  # 忽略截断警告
                "/wd4068"  # 忽略未知编译指示警告
                "/wd4800"  # 忽略布尔值强制转换警告
                "/wd4005"  # 忽略宏重定义警告
            )
        endif()
    endif()
    
    # VTK 库链接 (现代方式)
    if(VTK_FOUND)
        if(VTK_VERSION VERSION_GREATER_EQUAL "9.0")
            # VTK 9.0+ 现代组件方式
            target_link_libraries(MedicalImageCore PRIVATE 
                VTK::CommonCore 
                VTK::CommonDataModel 
                VTK::IOImage 
                VTK::ImagingCore
            )
        else()
            # 兼容老版本VTK
            target_link_libraries(MedicalImageCore PRIVATE ${VTK_LIBRARIES})
        endif()
        target_compile_definitions(MedicalImageCore PRIVATE VTK_FOUND)
    endif()
    
    # 包含目录
    target_include_directories(MedicalImageCore PRIVATE
        ${CTK_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/Framework/Core  # 访问共享数据结构
    )

    # 编译定义
    target_compile_definitions(MedicalImageCore PRIVATE 
        CTK_PLUGIN_FRAMEWORK
        CTK_PLUGIN_FRAMEWORK_EXPORT
        MedicalImageCore_EXPORTS  # DLL导出宏
    )
    
    message(STATUS "MedicalImageCore configured successfully (统一CTK架构 - 核心服务提供者)")
else()
    message(WARNING "CTK Plugin Framework not available - MedicalImageCore will not be built")
endif()