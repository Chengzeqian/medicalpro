cmake_minimum_required(VERSION 3.16)

project(OpticalTracking VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set encoding to UTF-8
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Qt组件查找
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Network)

# VTK和ITK支持（ThirdParty预编译库）
set(VTK_DIR "${CMAKE_SOURCE_DIR}/ThirdParty/VTK/VTK-install/lib/cmake/vtk-9.4")
set(ITK_DIR "${CMAKE_SOURCE_DIR}/ThirdParty/ITK/ITK-install/lib/cmake/ITK-5.4")

# 查找VTK
find_package(VTK QUIET PATHS ${VTK_DIR} NO_DEFAULT_PATH)
if(VTK_FOUND)
    message(STATUS "OpticalTracking: VTK found - version ${VTK_VERSION} at ${VTK_DIR}")
else()
    message(STATUS "OpticalTracking: VTK not found - trying alternative path")
    # 尝试其他可能的路径
    file(GLOB VTK_POSSIBLE_DIRS "${CMAKE_SOURCE_DIR}/ThirdParty/VTK/VTK-install/lib/cmake/vtk*")
    foreach(VTK_POSSIBLE_DIR ${VTK_POSSIBLE_DIRS})
        if(IS_DIRECTORY ${VTK_POSSIBLE_DIR})
            set(VTK_DIR ${VTK_POSSIBLE_DIR})
            find_package(VTK QUIET PATHS ${VTK_DIR} NO_DEFAULT_PATH)
            if(VTK_FOUND)
                message(STATUS "OpticalTracking: VTK found at alternative path: ${VTK_DIR}")
                break()
            endif()
        endif()
    endforeach()
endif()

# 查找ITK
find_package(ITK QUIET PATHS ${ITK_DIR} NO_DEFAULT_PATH)
if(ITK_FOUND)
    message(STATUS "OpticalTracking: ITK found - version ${ITK_VERSION} at ${ITK_DIR}")
    include(${ITK_USE_FILE})
else()
    message(STATUS "OpticalTracking: ITK not found - trying alternative path")
    # 尝试其他可能的路径  
    file(GLOB ITK_POSSIBLE_DIRS "${CMAKE_SOURCE_DIR}/ThirdParty/ITK/ITK-install/lib/cmake/ITK*")
    foreach(ITK_POSSIBLE_DIR ${ITK_POSSIBLE_DIRS})
        if(IS_DIRECTORY ${ITK_POSSIBLE_DIR})
            set(ITK_DIR ${ITK_POSSIBLE_DIR})
            find_package(ITK QUIET PATHS ${ITK_DIR} NO_DEFAULT_PATH)
            if(ITK_FOUND)
                message(STATUS "OpticalTracking: ITK found at alternative path: ${ITK_DIR}")
                include(${ITK_USE_FILE})
                break()
            endif()
        endif()
    endforeach()
endif()

# CTK Plugin Framework 配置 (继承父项目设置)
if(CTK_FOUND)
    # Plugin sources (完全CTK架构)
    set(PLUGIN_SOURCES
        # CTK框架必需文件
        OpticalTrackingActivator.h
        OpticalTrackingActivator.cpp
        
        # 服务接口和实现（完全CTK架构）
        OpticalTrackingService.h
        OpticalTrackingServiceImpl.h
        OpticalTrackingServiceImpl.cpp
        
        # 服务接口声明（完全CTK架构）
        ServiceInterfaces.h
        
        # UI界面组件（统一架构命名）
        OpticalTrackingWidget.h
        OpticalTrackingWidget.cpp
        
        # 注意：所有独立模块已整合到ServiceImpl中：
        # - AtracsysOfficialWrapper (硬件抽象)
        # - PivotCalibrationModule (校准功能)
        # - UDPCommunicationModule (网络通信)
        # - OpticalTrackingServiceAccessor (服务访问)
        
        # 资源文件
        OpticalTracking.qrc
    )
    
    # Create plugin library
    add_library(OpticalTracking SHARED ${PLUGIN_SOURCES})
    
    # Set plugin properties
    set_target_properties(OpticalTracking PROPERTIES
        OUTPUT_NAME "OpticalTracking"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/plugins"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/plugins"
    )
    
    # Link libraries（遵循完全CTK架构）
    target_link_libraries(OpticalTracking PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
        ${CTK_LIBRARIES}
        # 遵循CTK架构：不直接链接其他插件DLL，通过服务接口通信
    )

    # VTK集成（完整的可视化支持）
    if(VTK_FOUND)
        # VTK库列表（包含所有需要的组件）
        set(VTK_LIBRARIES_TO_LINK
            VTK::CommonCore
            VTK::CommonDataModel
            VTK::CommonTransforms
            VTK::CommonColor
            VTK::RenderingCore
            VTK::RenderingOpenGL2
            VTK::RenderingVolumeOpenGL2
            VTK::RenderingAnnotation
            VTK::InteractionStyle
            VTK::InteractionWidgets
            VTK::IOImage
            VTK::IOLegacy
            VTK::IOXML
            VTK::FiltersCore
            VTK::FiltersSources
            VTK::FiltersGeometry
            VTK::GUISupportQt
        )
        
        # 尝试添加DICOM和NIFTI支持
        if(TARGET VTK::IODICOM)
            list(APPEND VTK_LIBRARIES_TO_LINK VTK::IODICOM)
        endif()
        if(TARGET VTK::IONIFTI)
            list(APPEND VTK_LIBRARIES_TO_LINK VTK::IONIFTI)
        endif()
        
        target_link_libraries(OpticalTracking PRIVATE ${VTK_LIBRARIES_TO_LINK})
        target_include_directories(OpticalTracking PRIVATE ${VTK_INCLUDE_DIRS})
        target_compile_definitions(OpticalTracking PRIVATE VTK_FOUND VTK_FOUND_AND_LINKED)
        
        message(STATUS "OpticalTracking: VTK integration enabled - libraries: ${VTK_LIBRARIES_TO_LINK}")
    else()
        message(STATUS "OpticalTracking: VTK not available - using fallback implementation")
    endif()
    
    # ITK集成（医学图像处理支持）
    if(ITK_FOUND)
        target_link_libraries(OpticalTracking PRIVATE ${ITK_LIBRARIES})
        target_include_directories(OpticalTracking PRIVATE ${ITK_INCLUDE_DIRS})
        target_compile_definitions(OpticalTracking PRIVATE ITK_FOUND)
        
        message(STATUS "OpticalTracking: ITK integration enabled")
    else()
        message(STATUS "OpticalTracking: ITK not available - medical image processing limited")
    endif()
    
    # 可选的硬件SDK支持（保留但不强制依赖）
    set(ATRACSYS_SDK_ROOT "${CMAKE_SOURCE_DIR}/ThirdParty/Atracsys/fusionTrack SDK x64")
    if(EXISTS "${ATRACSYS_SDK_ROOT}/include/ftkInterface.h")
        message(STATUS "OpticalTracking: Atracsys SDK found - hardware support enabled")
        target_include_directories(OpticalTracking PRIVATE "${ATRACSYS_SDK_ROOT}/include")
        target_compile_definitions(OpticalTracking PRIVATE ATRACSYS_SDK_FOUND)
        
        # 链接Atracsys库（如果存在）
        if(EXISTS "${ATRACSYS_SDK_ROOT}/lib/fusionTrack64.lib")
            target_link_libraries(OpticalTracking PRIVATE "${ATRACSYS_SDK_ROOT}/lib/fusionTrack64.lib")
        endif()
    else()
        message(STATUS "OpticalTracking: Atracsys SDK not found - using simulation mode")
    endif()
    
    # Include directories（遵循完全CTK架构）
    target_include_directories(OpticalTracking PRIVATE
        ${CTK_INCLUDE_DIRS}
        # 注意：完全CTK架构下不包含其他插件或共享数据结构
        # 所有通信通过CTK服务接口完成
    )

    # Compile definitions
    target_compile_definitions(OpticalTracking PRIVATE 
        CTK_PLUGIN_FRAMEWORK
        CTK_PLUGIN_FRAMEWORK_EXPORT
    )
    
    message(STATUS "OpticalTracking plugin configured successfully (统一CTK架构)")
else()
    message(WARNING "CTK Plugin Framework not available - OpticalTracking will not be built")
endif()