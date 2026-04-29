# Registration2D3D Plugin CMakeLists.txt

cmake_minimum_required(VERSION 3.16)

project(Registration2D3D VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set encoding to UTF-8
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Qt组件查找
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets Sql)

# Python查找（必需）
# 使用系统安装的 Python 3.9 进行编译（因为有完整的开发文件：include 和 libs）
# 运行时代码配置为使用项目目录下的嵌入式 Python39
set(SYSTEM_PYTHON39_DIR "C:/Users/cheng/AppData/Local/Programs/Python/Python39")
if(EXISTS "${SYSTEM_PYTHON39_DIR}/python.exe" AND EXISTS "${SYSTEM_PYTHON39_DIR}/include" AND EXISTS "${SYSTEM_PYTHON39_DIR}/libs")
    # 直接设置所有 Python3 查找变量，避免 CMake 搜索其他位置
    set(Python3_ROOT_DIR "${SYSTEM_PYTHON39_DIR}")
    set(Python3_EXECUTABLE "${SYSTEM_PYTHON39_DIR}/python.exe")
    set(Python3_INCLUDE_DIR "${SYSTEM_PYTHON39_DIR}/include")
    set(Python3_LIBRARY "${SYSTEM_PYTHON39_DIR}/libs/python39.lib")
    # 禁止 CMake 在注册表和其他位置搜索 Python（避免找到其他版本）
    set(Python3_FIND_REGISTRY NEVER)
    set(Python3_FIND_STRATEGY LOCATION)
    message(STATUS "Registration2D3D: Using system Python 3.9 for development at ${SYSTEM_PYTHON39_DIR}")
else()
    message(WARNING "System Python 3.9 with development files not found at ${SYSTEM_PYTHON39_DIR}")
endif()
# 只查找 Interpreter，手动设置开发组件
find_package(Python3 COMPONENTS Interpreter REQUIRED)
if(Python3_FOUND)
    # 手动设置 Python 开发变量（因为只查找了 Interpreter）
    if(NOT Python3_INCLUDE_DIRS AND Python3_INCLUDE_DIR)
        set(Python3_INCLUDE_DIRS "${Python3_INCLUDE_DIR}")
    endif()
    if(NOT Python3_LIBRARIES AND Python3_LIBRARY)
        set(Python3_LIBRARIES "${Python3_LIBRARY}")
    endif()
    message(STATUS "Python3 found:")
    message(STATUS "  - Python3_EXECUTABLE: ${Python3_EXECUTABLE}")
    message(STATUS "  - Python3_INCLUDE_DIRS: ${Python3_INCLUDE_DIRS}")
    message(STATUS "  - Python3_LIBRARIES: ${Python3_LIBRARIES}")
    message(STATUS "  - Python3_VERSION: ${Python3_VERSION}")
else()
    message(FATAL_ERROR "Python3 not found! This plugin requires Python.")
endif()

# Platform 模块配置
    set(REGISTRATION2D3D_PLATFORM_SOURCES
        Registration2D3DServiceImpl.cpp
        Registration2D3DWidget.cpp
        registration_2d3d_module.cpp
    )

    set(REGISTRATION2D3D_PLATFORM_HEADERS
        Registration2D3DService.h
        Registration2D3DServiceImpl.h
        Registration2D3DDataStructures.h
        Registration2D3DWidget.h
        registration_2d3d_module.h
    )

    add_library(Registration2D3DPlatformModuleLib STATIC
        ${REGISTRATION2D3D_PLATFORM_SOURCES}
        ${REGISTRATION2D3D_PLATFORM_HEADERS}
    )

    target_include_directories(Registration2D3DPlatformModuleLib PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(Registration2D3DPlatformModuleLib PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Sql
    )

    register_platform_descriptor(Registration2D3D "${CMAKE_CURRENT_SOURCE_DIR}/platform/plugin.json")

    # 手动链接 Python 库（不使用 Python3::Python 导入目标）
    target_link_libraries(Registration2D3DPlatformModuleLib PUBLIC ${Python3_LIBRARIES})

    # 运行时使用项目目录下的嵌入式 Python39（不是编译时的系统 Python）
    set(RUNTIME_PYTHON39_DIR "D:/Qtproject/medicalpro/Python39")
    target_compile_definitions(Registration2D3DPlatformModuleLib PRIVATE
        MEDICALPRO_PYTHONHOME_DEFAULT="${RUNTIME_PYTHON39_DIR}"
    )
    message(STATUS "Registration2D3D: runtime Python home set to ${RUNTIME_PYTHON39_DIR}")

    target_include_directories(Registration2D3DPlatformModuleLib PUBLIC
        ${Python3_INCLUDE_DIRS}
    )

    if(ITK_FOUND)
        target_link_libraries(Registration2D3DPlatformModuleLib PUBLIC ${ITK_LIBRARIES})
        target_include_directories(Registration2D3DPlatformModuleLib PUBLIC ${ITK_INCLUDE_DIRS})
        target_compile_definitions(Registration2D3DPlatformModuleLib PRIVATE ITK_FOUND)
        message(STATUS "Registration2D3D: ITK support enabled")
    endif()

    if(VTK_FOUND)
        target_compile_definitions(Registration2D3DPlatformModuleLib PRIVATE VTK_FOUND)
        message(STATUS "Registration2D3D: Using Framework-provided VTK linkage")
    else()
        message(WARNING "Registration2D3D: VTK not found - plugin will be built without VTK features")
    endif()
    
    if(WIN32)
        # 从项目目录的嵌入式 Python39 拷贝 DLL（确保版本一致）
        set(PYTHON_DLL_PATH "${RUNTIME_PYTHON39_DIR}/python39.dll")

        if(EXISTS "${PYTHON_DLL_PATH}")
            add_custom_command(TARGET Registration2D3DPlatformModuleLib POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${PYTHON_DLL_PATH}"
                    "${CMAKE_BINARY_DIR}/$<CONFIG>/python39.dll"
                COMMENT "Copying Python 3.9 DLL from project directory to output directory"
            )
            message(STATUS "Python DLL will be copied from: ${PYTHON_DLL_PATH}")
        else()
            message(WARNING "Python DLL not found at: ${PYTHON_DLL_PATH}")
        endif()
    endif()
    
    message(STATUS "Registration2D3D platform module configured successfully")
