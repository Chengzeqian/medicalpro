# ============================================================================
# PluginMacros.cmake - CTK插件构建宏和函数
# ============================================================================
# 提供标准化的插件构建函数，简化插件CMakeLists.txt配置
# 
# 功能：
# - add_medical_plugin: 创建标准CTK插件
# - configure_plugin_output: 配置插件输出目录
# - copy_plugin_to_exe_dir: 复制插件到可执行文件目录
# 
# 使用方法：
#   include(cmake/PluginMacros.cmake)
#   add_medical_plugin(MyPlugin
#       SOURCES file1.cpp file2.cpp
#       DEPENDENCIES ${VTK_LIBRARIES}
#   )
# ============================================================================

# 初始化全局属性（配置阶段仅执行一次）
set_property(GLOBAL PROPERTY MEDICALPRO_AUTO_COPY_PLUGINS "")
set_property(GLOBAL PROPERTY MEDICALPRO_AUTO_COPY_CONFIGURED FALSE)

# ============================================================================
# add_medical_plugin - 创建标准化的医疗系统插件
# ============================================================================
# 参数：
#   PLUGIN_NAME - 插件名称（必需）
#   VERSION - 插件版本（可选，默认1.0.0）
#   SOURCES - 源文件列表（必需）
#   HEADERS - 头文件列表（可选）
#   RESOURCES - 资源文件列表（可选）
#   DEPENDENCIES - 额外的链接依赖（可选）
#   INCLUDE_DIRS - 额外的包含目录（可选）
#   COMPILE_DEFINITIONS - 编译定义（可选）
#   NO_AUTO_COPY - 禁用自动复制到exe目录（可选标志）
# 
# 示例：
#   add_medical_plugin(DicomViewer
#       VERSION 1.0.0
#       SOURCES
#           DicomViewerActivator.cpp
#           DicomViewerServiceImpl.cpp
#           DicomViewerWidget.cpp
#       DEPENDENCIES
#           ${ITK_LIBRARIES}
#   )
# ============================================================================
function(add_medical_plugin PLUGIN_NAME)
    # 解析参数
    set(options NO_AUTO_COPY)
    set(oneValueArgs VERSION)
    set(multiValueArgs SOURCES HEADERS RESOURCES DEPENDENCIES INCLUDE_DIRS COMPILE_DEFINITIONS)
    cmake_parse_arguments(PLUGIN "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # 验证必需参数
    if(NOT PLUGIN_SOURCES)
        message(FATAL_ERROR "add_medical_plugin: SOURCES is required for plugin ${PLUGIN_NAME}")
    endif()
    
    # 设置默认版本
    if(NOT PLUGIN_VERSION)
        set(PLUGIN_VERSION "1.0.0")
    endif()
    
    message(STATUS "========================================")
    message(STATUS "Configuring plugin: ${PLUGIN_NAME}")
    message(STATUS "  Version: ${PLUGIN_VERSION}")
    message(STATUS "  Sources: ${PLUGIN_SOURCES}")
    if(PLUGIN_DEPENDENCIES)
        message(STATUS "  Dependencies: ${PLUGIN_DEPENDENCIES}")
    endif()
    message(STATUS "========================================")
    
    # ========================================================================
    # 创建MODULE库（插件）
    # ========================================================================
    # MODULE库特点：
    # - 运行时动态加载，不在链接时使用
    # - 适合插件架构
    # - 不生成导入库（.lib）
    # ========================================================================
    add_library(${PLUGIN_NAME} MODULE 
        ${PLUGIN_SOURCES}
        ${PLUGIN_HEADERS}
        ${PLUGIN_RESOURCES}
    )
    
    # ========================================================================
    # 配置插件输出目录
    # ========================================================================
    configure_plugin_output(${PLUGIN_NAME})
    
    # ========================================================================
    # 标准链接配置
    # ========================================================================
    # 所有插件的标准依赖：
    # - Framework: 提供VTK、CTK、通用工具（PRIVATE链接）
    # - Qt: UI组件（PRIVATE链接）
    # ========================================================================
    target_link_libraries(${PLUGIN_NAME} PRIVATE
        Framework  # 通过Framework获取VTK和CTK（PUBLIC传递）
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        ${PLUGIN_DEPENDENCIES}  # 插件特定的额外依赖
    )
    
    # ========================================================================
    # 包含目录配置
    # ========================================================================
    target_include_directories(${PLUGIN_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PLUGIN_INCLUDE_DIRS}
    )
    
    # ========================================================================
    # 编译定义
    # ========================================================================
    target_compile_definitions(${PLUGIN_NAME} PRIVATE
        CTK_PLUGIN_FRAMEWORK
        CTK_PLUGIN_FRAMEWORK_EXPORT
        ${PLUGIN_COMPILE_DEFINITIONS}
    )
    
    # ========================================================================
    # 自动复制插件到exe目录（除非禁用）
    # ========================================================================
    if(NOT PLUGIN_NO_AUTO_COPY)
        copy_plugin_to_exe_dir(${PLUGIN_NAME})
    endif()
    
    message(STATUS "✓ Plugin ${PLUGIN_NAME} configured successfully")
endfunction()

# ============================================================================
# configure_plugin_output - 配置插件输出目录
# ============================================================================
# 参数：
#   PLUGIN_NAME - 插件名称
# 
# 设置插件的输出目录为 ${CMAKE_BINARY_DIR}/$<CONFIG>/plugins
# 确保Debug和Release配置都输出到正确位置
# ============================================================================
function(configure_plugin_output PLUGIN_NAME)
    set_target_properties(${PLUGIN_NAME} PROPERTIES
        OUTPUT_NAME "${PLUGIN_NAME}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/plugins"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/plugins"
        # MODULE库不生成导入库，但设置ARCHIVE以防万一
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/plugins"
        # 移除Unix系统的'lib'前缀
        PREFIX ""
        # 设置版本信息
        VERSION ${PLUGIN_VERSION}
    )
    
    message(STATUS "  Output directory: ${CMAKE_BINARY_DIR}/$<CONFIG>/plugins")
endfunction()

# ============================================================================
# copy_plugin_to_exe_dir - 复制插件到可执行文件目录
# ============================================================================
# 参数：
#   PLUGIN_NAME - 插件名称
# 
# 在构建后自动复制插件DLL到exe所在的plugins子目录
# 支持Debug和Release配置
# 直接添加复制命令到当前插件目标
# ============================================================================
function(copy_plugin_to_exe_dir PLUGIN_NAME)
    # 直接添加POST_BUILD复制命令到插件目标
    add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/$<CONFIG>/plugins"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${PLUGIN_NAME}>"
            "${CMAKE_BINARY_DIR}/$<CONFIG>/plugins/$<TARGET_FILE_NAME:${PLUGIN_NAME}>"
        COMMENT "Copying ${PLUGIN_NAME} plugin to output plugins directory"
        VERBATIM
    )
    
    # 仍然维护全局属性，以便于将来兼容性
    get_property(_auto_copy_plugins GLOBAL PROPERTY MEDICALPRO_AUTO_COPY_PLUGINS)
    if(_auto_copy_plugins STREQUAL "")
        set(_auto_copy_plugins "${PLUGIN_NAME}")
    else()
        list(APPEND _auto_copy_plugins "${PLUGIN_NAME}")
        list(REMOVE_DUPLICATES _auto_copy_plugins)
    endif()
    set_property(GLOBAL PROPERTY MEDICALPRO_AUTO_COPY_PLUGINS "${_auto_copy_plugins}")
    message(STATUS "  Auto-copy scheduled: ${PLUGIN_NAME} → medicalpro/plugins/")
endfunction()

# ============================================================================
# configure_plugin_runtime_copy - 在主程序定义后配置插件复制命令
# ============================================================================
function(configure_plugin_runtime_copy TARGET_NAME)
    if(NOT TARGET ${TARGET_NAME})
        message(WARNING "Target '${TARGET_NAME}' not found when configuring plugin runtime copy")
        return()
    endif()

    get_property(_configured GLOBAL PROPERTY MEDICALPRO_AUTO_COPY_CONFIGURED)
    if(_configured)
        return()
    endif()

    get_property(_auto_copy_plugins GLOBAL PROPERTY MEDICALPRO_AUTO_COPY_PLUGINS)
    if(_auto_copy_plugins STREQUAL "")
        message(STATUS "No plugins scheduled for auto-copy")
        set_property(GLOBAL PROPERTY MEDICALPRO_AUTO_COPY_CONFIGURED TRUE)
        return()
    endif()

    foreach(PLUGIN_NAME ${_auto_copy_plugins})
        if(TARGET ${PLUGIN_NAME})
            add_dependencies(${TARGET_NAME} ${PLUGIN_NAME})
            add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${TARGET_NAME}>/plugins"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:${PLUGIN_NAME}>"
                    "$<TARGET_FILE_DIR:${TARGET_NAME}>/plugins/$<TARGET_FILE_NAME:${PLUGIN_NAME}>"
                COMMENT "Copying ${PLUGIN_NAME} plugin to ${TARGET_NAME} runtime directory"
                VERBATIM
            )
        else()
            message(WARNING "Plugin target '${PLUGIN_NAME}' not found when configuring runtime copy")
        endif()
    endforeach()

    set_property(GLOBAL PROPERTY MEDICALPRO_AUTO_COPY_CONFIGURED TRUE)
endfunction()

# ============================================================================
# add_plugin_dependency - 添加插件构建依赖
# ============================================================================
# 参数：
#   TARGET_NAME - 目标名称（通常是medicalpro）
#   PLUGIN_NAMES - 插件名称列表
# 
# 确保主程序构建时会自动构建所有插件
# ============================================================================
function(add_plugin_dependency TARGET_NAME)
    set(PLUGIN_NAMES ${ARGN})
    
    if(NOT TARGET ${TARGET_NAME})
        message(WARNING "Target '${TARGET_NAME}' not found, cannot add plugin dependencies")
        return()
    endif()
    
    foreach(PLUGIN_NAME ${PLUGIN_NAMES})
        if(TARGET ${PLUGIN_NAME})
            add_dependencies(${TARGET_NAME} ${PLUGIN_NAME})
        else()
            message(WARNING "Plugin target '${PLUGIN_NAME}' not found, skipping dependency")
        endif()
    endforeach()
    
    message(STATUS "Added plugin build dependencies to ${TARGET_NAME}: ${PLUGIN_NAMES}")
endfunction()

# ============================================================================
# print_plugin_summary - 打印插件配置摘要
# ============================================================================
# 在配置结束时调用，显示所有插件的状态
# ============================================================================
function(print_plugin_summary)
    message(STATUS "")
    message(STATUS "========================================")
    message(STATUS "Plugin Configuration Summary")
    message(STATUS "========================================")
    
    # 定义所有已知插件
    set(ALL_PLUGINS
        UserManagement
        DicomViewer
        FourViewDisplay
        OpticalTracking
        InstrumentManagement
        Registration2D3D
    )
    
    set(CONFIGURED_COUNT 0)
    foreach(PLUGIN ${ALL_PLUGINS})
        if(TARGET ${PLUGIN})
            message(STATUS "✓ ${PLUGIN}: Configured")
            math(EXPR CONFIGURED_COUNT "${CONFIGURED_COUNT} + 1")
        endif()
    endforeach()
    
    message(STATUS "")
    message(STATUS "Total plugins configured: ${CONFIGURED_COUNT}")
    message(STATUS "========================================")
    message(STATUS "")
endfunction()

# ============================================================================
# 辅助宏：简化常见插件配置
# ============================================================================

# 标准插件宏（最常用）
macro(add_standard_medical_plugin PLUGIN_NAME)
    add_medical_plugin(${PLUGIN_NAME} ${ARGN})
endmacro()

# 带ITK依赖的插件宏
macro(add_medical_plugin_with_itk PLUGIN_NAME)
    if(ITK_FOUND)
        add_medical_plugin(${PLUGIN_NAME} 
            ${ARGN}
            DEPENDENCIES ${ITK_LIBRARIES}
        )
    else()
        message(WARNING "ITK not found, ${PLUGIN_NAME} will not be built")
    endif()
endmacro()

# 带VTK依赖的插件宏（注意：VTK通常通过Framework传递，不需要显式链接）
macro(add_medical_plugin_with_vtk PLUGIN_NAME)
    # VTK通过Framework的PUBLIC链接自动传递，这里只是为了明确性
    add_medical_plugin(${PLUGIN_NAME} ${ARGN})
endmacro()
