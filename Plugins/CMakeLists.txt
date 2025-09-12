# Plugins CMakeLists.txt - CTK插件根目录
cmake_minimum_required(VERSION 3.16.0)

project(OpticalTrackingPlugins)

# 设置编码为UTF-8
if(MSVC)
    add_compile_options(/utf-8)
endif()

# 添加CTK插件的通用配置
if(CTK_FOUND)
    message(STATUS "Configuring CTK plugins...")

    # 构建开关（便于清理不需要的代码）- 现全部默认开启
    option(ENABLE_PLUGIN_OPTICAL_TRACKING "Build OpticalTracking plugin" ON)
    option(ENABLE_PLUGIN_IMAGE_INTERACTION "Build ImageInteraction plugin" ON)
    option(ENABLE_PLUGIN_SAMPLE "Build SamplePlugin" ON)
    option(ENABLE_PLUGIN_PATIENT_MANAGEMENT "Build PatientManagement plugin" ON)
    option(ENABLE_PLUGIN_MEDICAL_IMAGE_CORE "Build MedicalImageCore plugin" ON)
    option(ENABLE_PLUGIN_MEDICAL_PROCESSING "Build MedicalProcessing plugin" ON)
    option(ENABLE_PLUGIN_MEDICAL_VIEWER "Build MedicalViewer plugin" ON)

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

    # NrrdViewer插件已整合到MedicalViewer中（5插件架构优化）
    # 原NrrdViewer功能现在在MedicalViewer中提供

    # ImageInteraction插件
    if(ENABLE_PLUGIN_IMAGE_INTERACTION)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ImageInteraction/CMakeLists.txt")
            add_subdirectory(ImageInteraction)
            message(STATUS "ImageInteraction plugin subdirectory added")
        endif()
    else()
        message(STATUS "ImageInteraction plugin is disabled by option")
    endif()

    # SamplePlugin插件
    if(ENABLE_PLUGIN_SAMPLE)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/SamplePlugin/CMakeLists.txt")
            add_subdirectory(SamplePlugin)
            message(STATUS "SamplePlugin plugin subdirectory added")
        endif()
    else()
        message(STATUS "SamplePlugin is disabled by option")
    endif()

    # PatientManagement插件
    if(ENABLE_PLUGIN_PATIENT_MANAGEMENT)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/PatientManagement/CMakeLists.txt")
            add_subdirectory(PatientManagement)
            message(STATUS "PatientManagement plugin subdirectory added")
        else()
            message(WARNING "PatientManagement plugin CMakeLists.txt not found")
        endif()
    else()
        message(STATUS "PatientManagement plugin is disabled by option")
    endif()


    # MedicalImageCore插件 - 核心医学图像服务
    if(ENABLE_PLUGIN_MEDICAL_IMAGE_CORE)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/MedicalImageCore/CMakeLists.txt")
            add_subdirectory(MedicalImageCore)
            message(STATUS "MedicalImageCore plugin subdirectory added")
        else()
            message(WARNING "MedicalImageCore plugin CMakeLists.txt not found")
        endif()
    else()
        message(STATUS "MedicalImageCore plugin is disabled by option")
    endif()

    # MedicalProcessing插件 - 医学图像处理算法
    if(ENABLE_PLUGIN_MEDICAL_PROCESSING)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/MedicalProcessing/CMakeLists.txt")
            add_subdirectory(MedicalProcessing)
            message(STATUS "MedicalProcessing plugin subdirectory added")
        else()
            message(WARNING "MedicalProcessing plugin CMakeLists.txt not found")
        endif()
    else()
        message(STATUS "MedicalProcessing plugin is disabled by option")
    endif()

    # MedicalViewer插件 - 医学图像显示和可视化
    if(ENABLE_PLUGIN_MEDICAL_VIEWER)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/MedicalViewer/CMakeLists.txt")
            add_subdirectory(MedicalViewer)
            message(STATUS "MedicalViewer plugin subdirectory added")
        else()
            message(WARNING "MedicalViewer plugin CMakeLists.txt not found")
        endif()
    else()
        message(STATUS "MedicalViewer plugin is disabled by option")
    endif()

else()
    message(WARNING "CTK not found - plugins will not be built")
endif()
