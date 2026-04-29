cmake_minimum_required(VERSION 3.16)

project(OpticalRegistration VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set encoding to UTF-8
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Qt组件查找
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Gui)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Gui)

# Platform 模块配置 (继承父项目设置)
    set(OPTICALREGISTRATION_PLATFORM_SOURCES
        OpticalRegistrationServiceImpl.cpp
        optical_registration_module.cpp
        widgets/OpticalRegistrationWidget.cpp
        internal/OpticalRegistrationVTKWidget.cpp
    )

    set(OPTICALREGISTRATION_PLATFORM_HEADERS
        OpticalRegistrationService.h
        OpticalRegistrationServiceImpl.h
        optical_registration_module.h
        widgets/OpticalRegistrationWidget.h
        internal/OpticalRegistrationVTKWidget.h
    )

    add_library(OpticalRegistrationPlatformModuleLib STATIC
        ${OPTICALREGISTRATION_PLATFORM_SOURCES}
        ${OPTICALREGISTRATION_PLATFORM_HEADERS}
    )

    target_include_directories(OpticalRegistrationPlatformModuleLib PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/Plugins/OpticalTracking
    )

    target_link_libraries(OpticalRegistrationPlatformModuleLib PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Gui
    )

    register_platform_descriptor(OpticalRegistration "${CMAKE_CURRENT_SOURCE_DIR}/platform/plugin.json")

    # 添加OpticalTracking插件的头文件目录（用于服务接口引用）
    target_include_directories(OpticalRegistrationPlatformModuleLib PUBLIC
        ${CMAKE_SOURCE_DIR}/Plugins/OpticalTracking
    )

    # VTK依赖 - 用于配准算法和3D渲染
    if(VTK_FOUND)
        target_compile_definitions(OpticalRegistrationPlatformModuleLib PRIVATE VTK_FOUND)
        message(STATUS "OpticalRegistration: Using Framework-provided VTK linkage")

        # 链接VTK模块（配准算法 + 3D渲染）
        if(TARGET VTK::CommonCore)
            target_link_libraries(OpticalRegistrationPlatformModuleLib PUBLIC
                VTK::CommonCore
                VTK::CommonMath
                VTK::CommonTransforms
                VTK::RenderingCore
                VTK::RenderingOpenGL2
                VTK::InteractionStyle
                VTK::FiltersSources
                VTK::GUISupportQt
            )

            # VTK模块初始化（VTK 8.90+需要）
            vtk_module_autoinit(
                TARGETS OpticalRegistrationPlatformModuleLib
                MODULES ${VTK_LIBRARIES}
            )
        endif()
    else()
        message(WARNING "OpticalRegistration: VTK not found - plugin will use simplified registration algorithm")
    endif()

    # ITK集成（可选，用于高级配准）
    if(ITK_FOUND)
        target_link_libraries(OpticalRegistrationPlatformModuleLib PUBLIC ${ITK_LIBRARIES})
        target_include_directories(OpticalRegistrationPlatformModuleLib PUBLIC ${ITK_INCLUDE_DIRS})
        target_compile_definitions(OpticalRegistrationPlatformModuleLib PRIVATE ITK_FOUND)
        
        message(STATUS "OpticalRegistration: ITK integration enabled")
    else()
        message(STATUS "OpticalRegistration: ITK not available - using VTK-based registration only")
    endif()
    
    # 可选的Atracsys SDK支持（与OpticalTracking共享）
    set(ATRACSYS_SDK_ROOT "${CMAKE_SOURCE_DIR}/ThirdParty/Atracsys/fusionTrack SDK x64")
    if(EXISTS "${ATRACSYS_SDK_ROOT}/include/ftkInterface.h")
        message(STATUS "OpticalRegistration: Atracsys SDK found")
        target_include_directories(OpticalRegistrationPlatformModuleLib PRIVATE "${ATRACSYS_SDK_ROOT}/include")
        target_compile_definitions(OpticalRegistrationPlatformModuleLib PRIVATE ATRACSYS_SDK_FOUND)
    endif()
    
    message(STATUS "OpticalRegistration configured successfully (platform module)")

