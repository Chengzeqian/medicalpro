# CMake模块快速参考

## 快速开始

### 1. 查找第三方库

```cmake
include(cmake/FindThirdParty.cmake)

# 自动查找并设置路径
find_third_party_library(VTK)
find_third_party_library(ITK)
find_third_party_library(CTK)

# 查找并调用find_package
find_third_party_library_with_cmake(VTK)
```

### 2. 创建插件

```cmake
include(cmake/PluginMacros.cmake)

add_medical_plugin(MyPlugin
    VERSION 1.0.0
    SOURCES
        MyPluginActivator.cpp
        MyPluginServiceImpl.cpp
        MyPluginWidget.cpp
)
```

### 3. 配置预编译头

```cmake
include(cmake/PrecompiledHeaders.cmake)

# Framework
configure_framework_pch(Framework)

# 插件
reuse_framework_pch(MyPlugin)

# 所有插件
configure_all_plugins_pch()
```

## 常用函数

### FindThirdParty.cmake

| 函数 | 用途 |
|------|------|
| `find_third_party_library(LIB_NAME)` | 查找库并设置路径 |
| `find_third_party_library_with_cmake(LIB_NAME)` | 查找库并调用find_package |
| `print_third_party_summary()` | 打印查找摘要 |

### PluginMacros.cmake

| 函数 | 用途 |
|------|------|
| `add_medical_plugin(NAME ...)` | 创建标准插件 |
| `configure_plugin_output(NAME)` | 配置输出目录 |
| `copy_plugin_to_exe_dir(NAME)` | 复制到exe目录 |
| `add_plugin_dependency(TARGET ...)` | 添加构建依赖 |
| `print_plugin_summary()` | 打印插件摘要 |

### PrecompiledHeaders.cmake

| 函数 | 用途 |
|------|------|
| `configure_framework_pch(TARGET)` | 配置Framework PCH |
| `reuse_framework_pch(PLUGIN)` | 复用Framework PCH |
| `configure_all_plugins_pch()` | 为所有插件配置PCH |
| `disable_pch_for_target(TARGET)` | 禁用PCH |
| `print_pch_summary()` | 打印PCH摘要 |

## 插件参数

### add_medical_plugin参数

```cmake
add_medical_plugin(PluginName
    VERSION "1.0.0"              # 可选，默认1.0.0
    SOURCES file1.cpp file2.cpp  # 必需
    HEADERS file1.h file2.h      # 可选
    RESOURCES resources.qrc      # 可选
    DEPENDENCIES ${ITK_LIBRARIES} # 可选
    INCLUDE_DIRS ${CUSTOM_DIR}   # 可选
    COMPILE_DEFINITIONS MY_DEF   # 可选
    NO_AUTO_COPY                 # 可选标志
)
```

## 典型用例

### 用例1: 标准插件

```cmake
set(PLUGIN_SOURCES
    MyPluginActivator.cpp
    MyPluginServiceImpl.cpp
    MyPluginWidget.cpp
)

add_medical_plugin(MyPlugin
    SOURCES ${PLUGIN_SOURCES}
)

reuse_framework_pch(MyPlugin)
```

### 用例2: 带ITK依赖的插件

```cmake
add_medical_plugin(MyPlugin
    SOURCES ${PLUGIN_SOURCES}
    DEPENDENCIES ${ITK_LIBRARIES}
)
```

### 用例3: 自定义配置的插件

```cmake
add_medical_plugin(MyPlugin
    VERSION 2.0.0
    SOURCES ${PLUGIN_SOURCES}
    INCLUDE_DIRS ${CUSTOM_INCLUDE}
    COMPILE_DEFINITIONS USE_CUSTOM_FEATURE
    NO_AUTO_COPY
)

# 手动复制
copy_plugin_to_exe_dir(MyPlugin)
```

## 输出目录

| 类型 | 路径 |
|------|------|
| 插件构建输出 | `${CMAKE_BINARY_DIR}/$<CONFIG>/plugins/` |
| 插件运行时 | `<exe目录>/plugins/` |
| Framework | `${CMAKE_BINARY_DIR}/$<CONFIG>/` |

## 链接策略

| 库 | Framework | 主程序 | 插件 |
|----|-----------|--------|------|
| VTK | PUBLIC | PRIVATE | 继承 |
| CTK | PUBLIC | PRIVATE | 继承 |
| ITK | - | - | PRIVATE（按需） |
| Qt | PUBLIC | PRIVATE | PRIVATE |

## 搜索路径优先级

1. `ThirdParty/<LIB>/<LIB>-install`
2. `ThirdParty/<LIB>/<LIB>-build`
3. `ThirdParty/<LIB>`
4. `$ENV{<LIB>_DIR}`
5. `D:/<LIB>/<LIB>-install`
6. `C:/<LIB>/<LIB>-install`

## 性能提示

- **PCH**: 增量编译快30-50%
- **并行编译**: `cmake --build . -j8`
- **Ninja**: 比Make快20-30%
- **增量构建**: 避免频繁clean

## 故障排除

| 问题 | 解决方案 |
|------|----------|
| 找不到库 | 检查搜索路径，设置环境变量 |
| 插件未复制 | 确认使用add_medical_plugin |
| PCH错误 | 清理构建或禁用PCH |
| 链接错误 | 确认Framework.dll已复制 |

## 命令速查

```bash
# 配置
cmake -G "Visual Studio 16 2019" -A x64 ..

# 编译
cmake --build . --config Release -j8

# 清理
cmake --build . --target clean

# 重新配置
cmake ..

# 查看变量
cmake -L ..
```

## 更多信息

- 详细文档: [cmake/README.md](README.md)
- 构建指南: [docs/Build-Configuration-Guide.md](../docs/Build-Configuration-Guide.md)
- 集成指南: [docs/CMake-Integration-Guide.md](../docs/CMake-Integration-Guide.md)
