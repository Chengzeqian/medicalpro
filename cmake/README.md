# CMake模块说明

本目录包含MedicalPro项目的CMake辅助模块，用于简化构建配置和标准化插件开发。

## 模块列表

### 1. FindThirdParty.cmake
统一的第三方库查找逻辑

**功能**:
- 自动搜索VTK、ITK、CTK等第三方库
- 提供清晰的搜索路径优先级
- 详细的错误提示和诊断信息

**使用方法**:
```cmake
include(cmake/FindThirdParty.cmake)

# 查找库并设置<LIB_NAME>_DIR变量
find_third_party_library(VTK)
find_third_party_library(ITK)
find_third_party_library(CTK)

# 查找库并自动调用find_package
find_third_party_library_with_cmake(VTK)
find_third_party_library_with_cmake(ITK COMPONENTS ITKCommon ITKImageIO)

# 打印查找摘要
print_third_party_summary()
```

**搜索路径优先级**:
1. 项目ThirdParty目录（最高优先级）
2. 环境变量指定路径
3. 系统常见安装路径

### 2. PluginMacros.cmake
插件构建宏和函数

**功能**:
- `add_medical_plugin`: 创建标准化插件
- `configure_plugin_output`: 配置插件输出目录
- `copy_plugin_to_exe_dir`: 自动复制插件到exe目录
- `add_plugin_dependency`: 添加插件构建依赖
- `print_plugin_summary`: 打印插件配置摘要

**使用方法**:
```cmake
include(cmake/PluginMacros.cmake)

# 基本用法
add_medical_plugin(MyPlugin
    VERSION 1.0.0
    SOURCES
        MyPluginActivator.cpp
        MyPluginServiceImpl.cpp
        MyPluginWidget.cpp
)

# 带额外依赖
add_medical_plugin(MyPlugin
    VERSION 1.0.0
    SOURCES ${PLUGIN_SOURCES}
    DEPENDENCIES ${ITK_LIBRARIES}
    INCLUDE_DIRS ${CUSTOM_INCLUDE_DIR}
    COMPILE_DEFINITIONS MY_DEFINE
)

# 禁用自动复制
add_medical_plugin(MyPlugin
    SOURCES ${PLUGIN_SOURCES}
    NO_AUTO_COPY
)

# 添加插件构建依赖到主程序
 add_plugin_dependency(medicalpro
     UserManagement
     DicomViewer
 )

# 打印插件配置摘要
print_plugin_summary()
```

**自动配置**:
- 输出目录: `${CMAKE_BINARY_DIR}/$<CONFIG>/plugins`
- 自动链接Framework（获取VTK、CTK）
- 自动链接Qt组件
- 自动复制到exe目录的plugins子目录

### 3. PrecompiledHeaders.cmake
预编译头配置

**功能**:
- `configure_framework_pch`: 为Framework配置预编译头
- `reuse_framework_pch`: 让插件复用Framework的PCH
- `configure_all_plugins_pch`: 为所有插件配置PCH复用
- `disable_pch_for_target`: 禁用特定目标的PCH
- `print_pch_summary`: 打印PCH配置摘要

**使用方法**:
```cmake
include(cmake/PrecompiledHeaders.cmake)

# 为Framework配置PCH
configure_framework_pch(Framework)

# 让插件复用Framework的PCH
reuse_framework_pch(MyPlugin)

# 为所有插件配置PCH复用
configure_all_plugins_pch()

# 禁用特定目标的PCH
disable_pch_for_target(MyPlugin)

# 打印PCH配置摘要
print_pch_summary()
```

**预编译头内容**:
- Qt常用头文件（QWidget、QLayout等）
- VTK常用头文件（vtkSmartPointer、vtkRenderer等）
- C++标准库头文件（<memory>、<vector>等）

**性能提升**:
- 增量编译快30-50%
- 清理重编译快40-60%

## 完整使用示例

### 主CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.16)
project(medicalpro)

# 包含CMake模块
include(cmake/FindThirdParty.cmake)
include(cmake/PluginMacros.cmake)
include(cmake/PrecompiledHeaders.cmake)

# 查找第三方库
find_third_party_library_with_cmake(VTK)
find_third_party_library_with_cmake(ITK)
find_third_party_library_with_cmake(CTK)

# 创建Framework库
add_library(Framework SHARED ${FRAMEWORK_SOURCES})
target_link_libraries(Framework PUBLIC ${VTK_LIBRARIES} ${CTK_LIBRARIES})

# 配置Framework预编译头
configure_framework_pch(Framework)

# 创建主程序
add_executable(medicalpro ${PROJECT_SOURCES})
target_link_libraries(medicalpro PRIVATE Framework)

# 添加插件子目录
add_subdirectory(Plugins)

# 添加插件构建依赖
 add_plugin_dependency(medicalpro
     UserManagement
     DicomViewer
 )

# 打印配置摘要
print_third_party_summary()
print_plugin_summary()
print_pch_summary()
```

### 插件CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.16)
project(MyPlugin VERSION 1.0.0)

# 包含插件宏
include(${CMAKE_SOURCE_DIR}/cmake/PluginMacros.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/PrecompiledHeaders.cmake)

# 定义源文件
set(PLUGIN_SOURCES
    MyPluginActivator.cpp
    MyPluginServiceImpl.cpp
    MyPluginWidget.cpp
)

# 创建插件
add_medical_plugin(MyPlugin
    VERSION 1.0.0
    SOURCES ${PLUGIN_SOURCES}
)

# 复用Framework的预编译头
reuse_framework_pch(MyPlugin)
```

## 设计原则

### 1. 简化配置
- 减少重复代码
- 提供合理的默认值
- 自动化常见任务

### 2. 标准化
- 统一的插件结构
- 一致的输出目录
- 标准的链接策略

### 3. 可扩展性
- 支持自定义参数
- 允许覆盖默认行为
- 提供禁用选项

### 4. 诊断友好
- 详细的错误提示
- 清晰的搜索路径
- 配置摘要输出

## 最佳实践

### 1. 第三方库查找
```cmake
# ✓ 推荐：使用统一的查找函数
find_third_party_library_with_cmake(VTK)

# ✗ 不推荐：手动设置路径和find_package
set(VTK_DIR "D:/VTK/VTK-install")
find_package(VTK REQUIRED)
```

### 2. 插件创建
```cmake
# ✓ 推荐：使用add_medical_plugin
add_medical_plugin(MyPlugin
    SOURCES ${PLUGIN_SOURCES}
)

# ✗ 不推荐：手动配置所有细节
add_library(MyPlugin MODULE ${PLUGIN_SOURCES})
set_target_properties(MyPlugin PROPERTIES ...)
target_link_libraries(MyPlugin PRIVATE ...)
add_custom_command(TARGET MyPlugin POST_BUILD ...)
```

### 3. 预编译头
```cmake
# ✓ 推荐：复用Framework的PCH
reuse_framework_pch(MyPlugin)

# ✗ 不推荐：为每个插件单独配置PCH
target_precompile_headers(MyPlugin PRIVATE <QWidget> <vtkSmartPointer.h> ...)
```

## 故障排除

### 问题1: 找不到第三方库
**解决方案**: 检查搜索路径，设置环境变量或使用推荐的安装位置
```bash
set VTK_DIR=D:/VTK/VTK-install
cmake ..
```

### 问题2: 插件未复制到exe目录
**解决方案**: 确认使用了`add_medical_plugin`且未设置`NO_AUTO_COPY`

### 问题3: PCH编译错误
**解决方案**: 清理构建目录或禁用PCH
```cmake
disable_pch_for_target(MyPlugin)
```

## 参考文档

- [构建配置指南](../docs/Build-Configuration-Guide.md)
- [插件开发指南](../docs/Plugin-Development-Guide.md)
- [CMake官方文档](https://cmake.org/documentation/)

## 更新日志

- **2024-11-13**: 初始版本
  - 添加FindThirdParty.cmake
  - 添加PluginMacros.cmake
  - 添加PrecompiledHeaders.cmake
