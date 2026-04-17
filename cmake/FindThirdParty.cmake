# ============================================================================
# FindThirdParty.cmake - 统一第三方库查找逻辑
# ============================================================================
# 提供统一的第三方库查找函数，支持VTK、ITK、CTK等库的自动发现
# 
# 功能：
# - 统一的路径搜索顺序
# - 详细的错误提示和诊断信息
# - 支持环境变量和多个搜索路径
# 
# 使用方法：
#   include(cmake/FindThirdParty.cmake)
#   find_third_party_library(VTK)
#   find_third_party_library(ITK)
#   find_third_party_library(CTK)
# ============================================================================

# ============================================================================
# find_third_party_library - 统一查找第三方库
# ============================================================================
# 参数：
#   LIB_NAME - 库名称（VTK、ITK、CTK等）
# 
# 搜索顺序：
#   1. 项目ThirdParty目录下的安装目录
#   2. 项目ThirdParty目录下的构建目录
#   3. 环境变量指定的路径
#   4. 系统常见安装路径
# 
# 输出变量：
#   ${LIB_NAME}_FOUND - 是否找到库
#   ${LIB_NAME}_DIR - 库的路径
# ============================================================================
function(find_third_party_library LIB_NAME)
    message(STATUS "========================================")
    message(STATUS "Searching for ${LIB_NAME}...")
    message(STATUS "========================================")
    
    # 定义搜索路径（按优先级排序）
    set(SEARCH_PATHS
        # 1. 项目本地安装目录（最高优先级）
        "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/${LIB_NAME}/${LIB_NAME}-install"
        "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/${LIB_NAME}/${LIB_NAME}_install"
        
        # 2. 项目本地构建目录
        "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/${LIB_NAME}/${LIB_NAME}-build"
        "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/${LIB_NAME}/${LIB_NAME}_build"
        
        # 3. 项目ThirdParty根目录
        "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/${LIB_NAME}"
        
        # 4. 环境变量指定路径
        "$ENV{${LIB_NAME}_DIR}"
        
        # 5. 系统常见安装路径（Windows）
        "D:/${LIB_NAME}/${LIB_NAME}-install"
        "C:/${LIB_NAME}/${LIB_NAME}-install"
        "D:/${LIB_NAME}/${LIB_NAME}_install"
        "C:/${LIB_NAME}/${LIB_NAME}_install"
        
        # 6. 系统常见安装路径（Unix）
        "/usr/local/${LIB_NAME}"
        "/opt/${LIB_NAME}"
    )
    
    # 显示搜索路径
    message(STATUS "${LIB_NAME} search paths:")
    foreach(PATH ${SEARCH_PATHS})
        message(STATUS "  - ${PATH}")
    endforeach()
    
    # 遍历搜索路径
    set(FOUND_PATH "")
    foreach(PATH ${SEARCH_PATHS})
        if(EXISTS "${PATH}")
            set(FOUND_PATH "${PATH}")
            message(STATUS "✓ Found ${LIB_NAME} at: ${PATH}")
            break()
        endif()
    endforeach()
    
    # 如果找到路径，设置变量并尝试find_package
    if(FOUND_PATH)
        set(${LIB_NAME}_DIR "${FOUND_PATH}" PARENT_SCOPE)
        message(STATUS "${LIB_NAME}_DIR set to: ${FOUND_PATH}")
        message(STATUS "========================================")
        return()
    else()
        # 未找到，显示详细错误信息
        message(WARNING "========================================")
        message(WARNING "✗ ${LIB_NAME} NOT FOUND")
        message(WARNING "========================================")
        message(WARNING "Searched in the following locations:")
        foreach(PATH ${SEARCH_PATHS})
            message(WARNING "  ✗ ${PATH}")
        endforeach()
        message(WARNING "")
        message(WARNING "To fix this issue:")
        message(WARNING "  1. Install ${LIB_NAME} to one of the search paths above")
        message(WARNING "  2. Set environment variable ${LIB_NAME}_DIR to your installation path")
        message(WARNING "  3. Add your custom path to CMakeLists.txt")
        message(WARNING "")
        message(WARNING "Example installation paths:")
        message(WARNING "  - ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/${LIB_NAME}/${LIB_NAME}-install")
        message(WARNING "  - D:/${LIB_NAME}/${LIB_NAME}-install")
        message(WARNING "========================================")
        
        set(${LIB_NAME}_DIR "" PARENT_SCOPE)
    endif()
endfunction()

# ============================================================================
# find_third_party_library_with_cmake - 查找并执行find_package
# ============================================================================
# 参数：
#   LIB_NAME - 库名称
#   REQUIRED_COMPONENTS - 可选的必需组件列表
# 
# 此函数在find_third_party_library基础上，自动调用find_package
# ============================================================================
function(find_third_party_library_with_cmake LIB_NAME)
    # 解析可选参数
    set(options REQUIRED)
    set(oneValueArgs VERSION)
    set(multiValueArgs COMPONENTS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # 首先查找库路径
    find_third_party_library(${LIB_NAME})
    
    # 获取父作用域的变量
    set(LIB_DIR ${${LIB_NAME}_DIR})
    
    if(LIB_DIR)
        # 根据库类型设置特定的CMake路径
        set(CMAKE_PATHS "")
        
        if(${LIB_NAME} STREQUAL "VTK")
            list(APPEND CMAKE_PATHS
                "${LIB_DIR}/lib/cmake/vtk-9.4"
                "${LIB_DIR}/lib/cmake/vtk-9.3"
                "${LIB_DIR}/lib/cmake/vtk-9.2"
                "${LIB_DIR}"
            )
        elseif(${LIB_NAME} STREQUAL "ITK")
            list(APPEND CMAKE_PATHS
                "${LIB_DIR}/lib/cmake/ITK-5.4"
                "${LIB_DIR}/lib/cmake/ITK-5.3"
                "${LIB_DIR}/lib/cmake/ITK-5.2"
                "${LIB_DIR}"
            )
        elseif(${LIB_NAME} STREQUAL "CTK")
            list(APPEND CMAKE_PATHS
                "${LIB_DIR}/lib/ctk-0.1"
                "${LIB_DIR}"
            )
        else()
            list(APPEND CMAKE_PATHS "${LIB_DIR}")
        endif()
        
        # 执行find_package
        if(ARG_COMPONENTS)
            find_package(${LIB_NAME} ${ARG_VERSION} QUIET 
                COMPONENTS ${ARG_COMPONENTS}
                PATHS ${CMAKE_PATHS}
                NO_DEFAULT_PATH
            )
        else()
            find_package(${LIB_NAME} ${ARG_VERSION} QUIET 
                PATHS ${CMAKE_PATHS}
                NO_DEFAULT_PATH
            )
        endif()
        
        # 设置父作用域变量
        if(${LIB_NAME}_FOUND)
            message(STATUS "✓ ${LIB_NAME} package found successfully")
            if(DEFINED ${LIB_NAME}_VERSION)
                message(STATUS "  Version: ${${LIB_NAME}_VERSION}")
            endif()
            set(${LIB_NAME}_FOUND TRUE PARENT_SCOPE)
        else()
            message(WARNING "✗ find_package(${LIB_NAME}) failed at ${LIB_DIR}")
            if(ARG_REQUIRED)
                message(FATAL_ERROR "${LIB_NAME} is required but not found")
            endif()
        endif()
    else()
        if(ARG_REQUIRED)
            message(FATAL_ERROR "${LIB_NAME} is required but not found in any search path")
        endif()
    endif()
endfunction()

# ============================================================================
# print_third_party_summary - 打印第三方库查找摘要
# ============================================================================
# 在配置结束时调用，显示所有第三方库的状态
# ============================================================================
function(print_third_party_summary)
    message(STATUS "")
    message(STATUS "========================================")
    message(STATUS "Third-Party Libraries Summary")
    message(STATUS "========================================")
    
    # 检查常见的第三方库
    set(COMMON_LIBS VTK ITK CTK Qt${QT_VERSION_MAJOR})
    
    foreach(LIB ${COMMON_LIBS})
        if(${LIB}_FOUND)
            message(STATUS "✓ ${LIB}: FOUND")
            if(DEFINED ${LIB}_VERSION)
                message(STATUS "    Version: ${${LIB}_VERSION}")
            endif()
            if(DEFINED ${LIB}_DIR)
                message(STATUS "    Path: ${${LIB}_DIR}")
            endif()
        else()
            message(STATUS "✗ ${LIB}: NOT FOUND")
        endif()
    endforeach()
    
    message(STATUS "========================================")
    message(STATUS "")
endfunction()
