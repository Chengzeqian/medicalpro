# Plugins CMakeLists.txt - CTK插件根目录
cmake_minimum_required(VERSION 3.16.0)

project(OpticalTrackingPlugins)

# 设置编码为UTF-8
if(MSVC)
    add_compile_options(/utf-8)
endif()

# ============================================================================
# Include CMake Modules for Plugin Configuration
# ============================================================================
# Include PrecompiledHeaders.cmake for PCH reuse functionality
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/PrecompiledHeaders.cmake)
    message(STATUS "Precompiled Headers module included for plugins")
endif()

# 添加CTK插件的通用配置
if(CTK_FOUND)
    message(STATUS "Configuring CTK plugins...")

    	# 构建开关 - 核心插件
    option(ENABLE_PLUGIN_OPTICAL_TRACKING "Build OpticalTracking plugin" ON)
    option(ENABLE_PLUGIN_USER_MANAGEMENT "Build UserManagement plugin" ON)
    option(ENABLE_PLUGIN_DICOM_VIEWER "Build DicomViewer plugin" ON)
    option(ENABLE_PLUGIN_INSTRUMENT_MANAGEMENT "Build InstrumentManagement plugin" ON)
    option(ENABLE_PLUGIN_FOUR_VIEW_DISPLAY "Build FourViewDisplay plugin" ON)
    option(ENABLE_PLUGIN_REGISTRATION_2D3D "Build Registration2D3D plugin" ON)
    option(ENABLE_PLUGIN_POINT_REGISTRATION "Build PointRegistration plugin - 基于点的配准" ON)
    option(ENABLE_PLUGIN_OPTICAL_REGISTRATION "Build OpticalRegistration plugin - 光学配准" ON)

    # 新增插件 - 手术导航系统
    option(ENABLE_PLUGIN_BONE_SEGMENTATION "Build BoneSegmentation plugin - AI骨骼分割" ON)
    option(ENABLE_PLUGIN_REGISTRATION_CORE "Build RegistrationCore plugin - 配准算法核心" ON)

    # 添加各个插件子目录（按开关控制）
    # OpticalTracking插件
    if(ENABLE_PLUGIN_OPTICAL_TRACKING)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/OpticalTracking/CMakeLists.txt")
            add_subdirectory(OpticalTracking)
            message(STATUS "OpticalTracking plugin subdirectory added")
        else()
            message(WARNING "OpticalTracking plugin CMakeLists.txt not found")
        endif()
    else()
        message(STATUS "OpticalTracking plugin is disabled by option")
    endif()

    # UserManagement插件 - 用户认证和管理
    if(ENABLE_PLUGIN_USER_MANAGEMENT)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/UserManagement/CMakeLists.txt")
            add_subdirectory(UserManagement)
            message(STATUS "UserManagement plugin subdirectory added")
        else()
            message(WARNING "UserManagement plugin CMakeLists.txt not found")
        endif()
    else()
        message(STATUS "UserManagement plugin is disabled by option")
    endif()

    # DicomViewer插件 - DICOM影像查看器
    if(ENABLE_PLUGIN_DICOM_VIEWER)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/DicomViewer/CMakeLists.txt")
            add_subdirectory(DicomViewer)
            message(STATUS "DicomViewer plugin subdirectory added")
        else()
            message(WARNING "DicomViewer plugin CMakeLists.txt not found")
        endif()
    else()
        message(STATUS "DicomViewer plugin is disabled by option")
    endif()

    # ProsthesisPlanning插件已移除 - 功能与FourViewDisplay重复且未被使用
    # 原ProsthesisPlanning插件的构建逻辑已删除

    # InstrumentManagement插件 - 器械管理
    if(ENABLE_PLUGIN_INSTRUMENT_MANAGEMENT)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/InstrumentManagement/CMakeLists.txt")
            add_subdirectory(InstrumentManagement)
            message(STATUS "InstrumentManagement plugin subdirectory added")
        else()
            message(WARNING "InstrumentManagement plugin CMakeLists.txt not found")
        endif()
    else()
        message(STATUS "InstrumentManagement plugin is disabled by option")
    endif()

    # FourViewDisplay插件 - 四视图显示
    if(ENABLE_PLUGIN_FOUR_VIEW_DISPLAY)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/FourViewDisplay/CMakeLists.txt")
            add_subdirectory(FourViewDisplay)
            message(STATUS "FourViewDisplay plugin subdirectory added")
        else()
            message(WARNING "FourViewDisplay plugin CMakeLists.txt not found")
        endif()
    else()
        message(STATUS "FourViewDisplay plugin is disabled by option")
    endif()

    # Registration2D3D插件 - 2D3D医学图像配准
    if(ENABLE_PLUGIN_REGISTRATION_2D3D)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Registration2D3D/CMakeLists.txt")
            add_subdirectory(Registration2D3D)
            message(STATUS "Registration2D3D plugin subdirectory added")
        else()
            message(WARNING "Registration2D3D plugin CMakeLists.txt not found")
        endif()
    else()
        message(STATUS "Registration2D3D plugin is disabled by option")
    endif()

    # PointRegistration插件 - 基于点的配准
    if(ENABLE_PLUGIN_POINT_REGISTRATION)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/PointRegistration/CMakeLists.txt")
            add_subdirectory(PointRegistration)
            message(STATUS "PointRegistration plugin subdirectory added")
        else()
            message(WARNING "PointRegistration plugin CMakeLists.txt not found")
        endif()
    else()
        message(STATUS "PointRegistration plugin is disabled by option")
    endif()

    # OpticalRegistration插件 - 光学配准（依赖OpticalTracking）
    if(ENABLE_PLUGIN_OPTICAL_REGISTRATION)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/OpticalRegistration/CMakeLists.txt")
            add_subdirectory(OpticalRegistration)
            message(STATUS "OpticalRegistration plugin subdirectory added")
        else()
            message(WARNING "OpticalRegistration plugin CMakeLists.txt not found")
        endif()
    else()
        message(STATUS "OpticalRegistration plugin is disabled by option")
    endif()

    # BoneSegmentation插件 - AI骨骼分割（TotalSegmentator）
    if(ENABLE_PLUGIN_BONE_SEGMENTATION)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/BoneSegmentation/CMakeLists.txt")
            add_subdirectory(BoneSegmentation)
            message(STATUS "BoneSegmentation plugin subdirectory added")
        else()
            message(WARNING "BoneSegmentation plugin CMakeLists.txt not found")
        endif()
    else()
        message(STATUS "BoneSegmentation plugin is disabled by option")
    endif()

    # RegistrationCore插件 - 配准算法核心（Landmark + ICP）
    if(ENABLE_PLUGIN_REGISTRATION_CORE)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/RegistrationCore/CMakeLists.txt")
            add_subdirectory(RegistrationCore)
            message(STATUS "RegistrationCore plugin subdirectory added")
        else()
            message(WARNING "RegistrationCore plugin CMakeLists.txt not found")
        endif()
    else()
        message(STATUS "RegistrationCore plugin is disabled by option")
    endif()

    # Configure Precompiled Headers for All Plugins (Requirement 8.4)
    # ========================================================================
    # After all plugins are added, configure them to reuse Framework's PCH
    # This significantly speeds up plugin compilation
    # ========================================================================
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.16" AND TARGET Framework)
        message(STATUS "")
        message(STATUS "========================================")
        message(STATUS "Configuring PCH Reuse for All Plugins")
        message(STATUS "========================================")
        
        message(STATUS "PCH reuse for plugins已跳过（按需可手动开启）")
    else()
        if(CMAKE_VERSION VERSION_LESS "3.16")
            message(STATUS "Precompiled headers not available (CMake < 3.16)")
        elseif(NOT TARGET Framework)
            message(WARNING "Framework target not found, cannot configure PCH for plugins")
        endif()
    endif()

else()
    message(WARNING "CTK not found - plugins will not be built")
endif()
