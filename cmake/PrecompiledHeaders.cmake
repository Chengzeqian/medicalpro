# ============================================================================
# PrecompiledHeaders.cmake - 预编译头配置
# ============================================================================
# 提供预编译头（PCH）配置函数，加速编译
# 
# 功能：
# - configure_framework_pch: 为Framework配置预编译头
# - reuse_framework_pch: 让插件复用Framework的预编译头
# 
# 使用方法：
#   include(cmake/PrecompiledHeaders.cmake)
#   configure_framework_pch(Framework)
#   reuse_framework_pch(MyPlugin)
# 
# 注意：
# - 预编译头需要CMake 3.16+
# - 只包含稳定的、频繁使用的头文件
# - 避免包含经常变化的项目头文件
# ============================================================================

# ============================================================================
# configure_framework_pch - 为Framework配置预编译头
# ============================================================================
# 参数：
#   TARGET_NAME - 目标名称（通常是Framework）
# 
# 预编译头包含：
# - Qt常用头文件（QWidget、QLayout等）
# - VTK常用头文件（vtkSmartPointer、vtkRenderer等）
# - C++标准库头文件（<memory>、<vector>等）
# ============================================================================
function(configure_framework_pch TARGET_NAME)
    # 检查CMake版本
    if(CMAKE_VERSION VERSION_LESS "3.16")
        message(WARNING "Precompiled headers require CMake 3.16+, current version: ${CMAKE_VERSION}")
        return()
    endif()
    
    # 检查目标是否存在
    if(NOT TARGET ${TARGET_NAME})
        message(WARNING "Target '${TARGET_NAME}' not found, cannot configure PCH")
        return()
    endif()
    
    message(STATUS "========================================")
    message(STATUS "Configuring Precompiled Headers for ${TARGET_NAME}")
    message(STATUS "========================================")
    
    # 定义预编译头列表
    set(PCH_HEADERS
        # ====== Qt Core ======
        <QObject>
        <QString>
        <QStringList>
        <QList>
        <QVector>
        <QMap>
        <QHash>
        <QVariant>
        <QDebug>
        <QFile>
        <QDir>
        <QFileInfo>
        <QDateTime>
        <QTimer>
        <QThread>
        
        # ====== Qt Widgets ======
        <QWidget>
        <QMainWindow>
        <QDialog>
        <QLabel>
        <QPushButton>
        <QLineEdit>
        <QTextEdit>
        <QComboBox>
        <QCheckBox>
        <QRadioButton>
        <QSlider>
        <QSpinBox>
        <QTableWidget>
        <QTreeWidget>
        <QListWidget>
        <QStackedWidget>
        <QTabWidget>
        <QGroupBox>
        <QScrollArea>
        <QSplitter>
        
        # ====== Qt Layouts ======
        <QVBoxLayout>
        <QHBoxLayout>
        <QGridLayout>
        <QFormLayout>
        
        # ====== Qt Graphics ======
        <QPixmap>
        <QImage>
        <QPainter>
        <QColor>
        <QFont>
        <QIcon>
        
        # ====== Qt SQL ======
        <QSqlDatabase>
        <QSqlQuery>
        <QSqlError>
        <QSqlRecord>
        
        # ====== VTK Core ======
        <vtkSmartPointer.h>
        <vtkObject.h>
        <vtkObjectFactory.h>
        
        # ====== VTK Rendering ======
        <vtkRenderer.h>
        <vtkRenderWindow.h>
        <vtkRenderWindowInteractor.h>
        <vtkInteractorStyleTrackballCamera.h>
        <vtkCamera.h>
        
        # ====== VTK Common ======
        <vtkActor.h>
        <vtkProperty.h>
        <vtkPolyData.h>
        <vtkPolyDataMapper.h>
        <vtkPoints.h>
        <vtkCellArray.h>
        
        # ====== VTK Imaging ======
        <vtkImageData.h>
        <vtkImageActor.h>
        <vtkImageMapper3D.h>
        
        # ====== VTK IO ======
        <vtkSTLReader.h>
        <vtkSTLWriter.h>
        <vtkPolyDataReader.h>
        <vtkPolyDataWriter.h>
        
        # ====== C++ Standard Library ======
        <memory>
        <vector>
        <string>
        <map>
        <unordered_map>
        <set>
        <algorithm>
        <functional>
        <iostream>
        <fstream>
        <sstream>
        <stdexcept>
        <cmath>
        <cstdint>
    )
    
    # 应用预编译头
    target_precompile_headers(${TARGET_NAME} PRIVATE ${PCH_HEADERS})
    
    message(STATUS "✓ Precompiled headers configured for ${TARGET_NAME}")
    message(STATUS "  Total headers: ${CMAKE_MATCH_COUNT}")
    message(STATUS "========================================")
endfunction()

# ============================================================================
# reuse_framework_pch - 让插件复用Framework的预编译头
# ============================================================================
# 参数：
#   PLUGIN_NAME - 插件名称
# 
# 插件复用Framework的PCH可以：
# - 避免重复编译相同的头文件
# - 加速插件编译
# - 减少编译产物大小
# ============================================================================
function(reuse_framework_pch PLUGIN_NAME)
    # 检查CMake版本
    if(CMAKE_VERSION VERSION_LESS "3.16")
        return()
    endif()
    
    # 检查目标是否存在
    if(NOT TARGET ${PLUGIN_NAME})
        message(WARNING "Target '${PLUGIN_NAME}' not found, cannot reuse PCH")
        return()
    endif()
    
    # 检查Framework是否存在
    if(NOT TARGET Framework)
        message(WARNING "Framework target not found, cannot reuse PCH for ${PLUGIN_NAME}")
        return()
    endif()
    
    # 复用Framework的预编译头
    target_precompile_headers(${PLUGIN_NAME} REUSE_FROM Framework)
    
    message(STATUS "  ${PLUGIN_NAME}: Reusing Framework PCH")
endfunction()

# ============================================================================
# configure_all_plugins_pch - 为所有插件配置PCH复用
# ============================================================================
# 自动为所有已知插件配置PCH复用
# ============================================================================
function(configure_all_plugins_pch)
    # 添加一个选项来禁用所有PCH，如果遭遇编译问题
    option(DISABLE_ALL_PCH "Disable PCH for all plugins to fix compilation issues" OFF)
    
    message(STATUS "========================================")
    message(STATUS "Configuring PCH Reuse for Plugins")
    message(STATUS "========================================")
    
    if(DISABLE_ALL_PCH)
        message(STATUS "PCH is globally disabled via DISABLE_ALL_PCH option")
        message(STATUS "This can help solve compiler compatibility issues")
        return()
    endif()
    
    # 定义所有已知插件
    set(ALL_PLUGINS
        UserManagement
        DicomViewer
        FourViewDisplay
        OpticalTracking
        InstrumentManagement
        Registration2D3D
    )
    
    # 定义可能有PCH问题的插件
    set(PROBLEMATIC_PLUGINS
        UserManagement
        # 如果有更多问题的插件，可以在这里添加
    )
    
    set(CONFIGURED_COUNT 0)
    foreach(PLUGIN ${ALL_PLUGINS})
        if(TARGET ${PLUGIN})
            # 检查插件是否在问题插件列表中
            list(FIND PROBLEMATIC_PLUGINS ${PLUGIN} PROBLEM_INDEX)
            if(PROBLEM_INDEX EQUAL -1)
                reuse_framework_pch(${PLUGIN})
                math(EXPR CONFIGURED_COUNT "${CONFIGURED_COUNT} + 1")
            else()
                message(STATUS "  ${PLUGIN}: PCH disabled (known compiler compatibility issues)")
                disable_pch_for_target(${PLUGIN})
            endif()
        endif()
    endforeach()
    
    message(STATUS "")
    message(STATUS "✓ PCH reuse configured for ${CONFIGURED_COUNT} plugins")
    message(STATUS "========================================")
endfunction()

# ============================================================================
# disable_pch_for_target - 禁用特定目标的预编译头
# ============================================================================
# 参数：
#   TARGET_NAME - 目标名称
# 
# 某些特殊情况下可能需要禁用PCH（例如编译问题）
# ============================================================================
function(disable_pch_for_target TARGET_NAME)
    if(CMAKE_VERSION VERSION_LESS "3.16")
        return()
    endif()
    
    if(NOT TARGET ${TARGET_NAME})
        message(WARNING "Target '${TARGET_NAME}' not found")
        return()
    endif()
    
    set_target_properties(${TARGET_NAME} PROPERTIES
        DISABLE_PRECOMPILE_HEADERS ON
    )
    
    message(STATUS "  ${TARGET_NAME}: PCH disabled")
endfunction()

# ============================================================================
# print_pch_summary - 打印预编译头配置摘要
# ============================================================================
function(print_pch_summary)
    if(CMAKE_VERSION VERSION_LESS "3.16")
        message(STATUS "")
        message(STATUS "========================================")
        message(STATUS "Precompiled Headers: NOT AVAILABLE")
        message(STATUS "  Reason: CMake version < 3.16")
        message(STATUS "  Current version: ${CMAKE_VERSION}")
        message(STATUS "========================================")
        message(STATUS "")
        return()
    endif()
    
    message(STATUS "")
    message(STATUS "========================================")
    message(STATUS "Precompiled Headers Summary")
    message(STATUS "========================================")
    
    if(TARGET Framework)
        get_target_property(PCH_HEADERS Framework PRECOMPILE_HEADERS)
        if(PCH_HEADERS)
            message(STATUS "✓ Framework: PCH Configured")
        else()
            message(STATUS "✗ Framework: PCH Not Configured")
        endif()
    else()
        message(STATUS "✗ Framework: Target Not Found")
    endif()
    
    message(STATUS "")
    message(STATUS "Plugins reusing Framework PCH:")
    
    set(ALL_PLUGINS
        UserManagement DicomViewer
        FourViewDisplay OpticalTracking InstrumentManagement
        MedicalImageCore MedicalProcessing MedicalViewer
        ImageInteraction Registration2D3D SamplePlugin
    )
    
    set(REUSE_COUNT 0)
    foreach(PLUGIN ${ALL_PLUGINS})
        if(TARGET ${PLUGIN})
            get_target_property(PCH_REUSE ${PLUGIN} PRECOMPILE_HEADERS_REUSE_FROM)
            if(PCH_REUSE STREQUAL "Framework")
                message(STATUS "  ✓ ${PLUGIN}")
                math(EXPR REUSE_COUNT "${REUSE_COUNT} + 1")
            endif()
        endif()
    endforeach()
    
    message(STATUS "")
    message(STATUS "Total plugins with PCH: ${REUSE_COUNT}")
    message(STATUS "========================================")
    message(STATUS "")
endfunction()
