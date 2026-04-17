# NewPages - 简化UI框架

## 概述

这是一个简化的UI框架，用于替换原有的复杂嵌套UI结构。

## 设计理念

1. **一个页面 = 一个 .ui 文件 + 一个类**
2. **扁平化页面结构**，取消嵌套容器
3. **统一导航机制**

## 文件结构

```
UI/
├── Forms/                    # .ui 文件目录
│   ├── WelcomePage.ui
│   ├── LoginPage.ui
│   ├── ModuleSelectionPage.ui
│   ├── SystemSettingsPage.ui
│   ├── ManagementPage.ui
│   ├── DashboardPage.ui
│   └── NavigationPage.ui
│
└── NewPages/                 # 页面类目录
    ├── CMakeLists.txt
    ├── README.md
    ├── BasePage.h            # 页面基类
    ├── PageIndex.h           # 页面索引枚举
    ├── MainWindow.h/cpp      # 主窗口类
    ├── WelcomePage.h/cpp
    ├── LoginPage.h/cpp
    ├── ModuleSelectionPage.h/cpp
    ├── SystemSettingsPage.h/cpp
    ├── ManagementPage.h/cpp
    ├── DashboardPage.h/cpp
    └── NavigationPage.h/cpp
```

## 页面列表

| 索引 | 页面名称 | 功能描述 |
|------|----------|----------|
| 0 | WelcomePage | 欢迎页，系统入口 |
| 1 | LoginPage | 用户登录 |
| 2 | ModuleSelectionPage | 模块选择（踝关节手术/系统设置）|
| 3 | SystemSettingsPage | 系统配置 |
| 4 | ManagementPage | 数据管理（医生/患者/手术）|
| 5 | DashboardPage | 患者总览 |
| 6 | NavigationPage | 手术导航（器械/规划/配准/导航）|

## 导航流程

```
WelcomePage → LoginPage → ModuleSelectionPage
                               ↓
                    ┌──────────┴──────────┐
                    ↓                      ↓
            SystemSettingsPage      ManagementPage
                                          ↓
                                    DashboardPage
                                          ↓
                                    NavigationPage
```

## 集成方法

### 方法1：替换主程序入口

在 `main.cpp` 中：

```cpp
#include "UI/NewPages/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MainWindowNew mainWindow;
    mainWindow.showMaximized();

    return app.exec();
}
```

### 方法2：添加到 CMakeLists.txt

```cmake
# 在主 CMakeLists.txt 中添加
add_subdirectory(UI/NewPages)

# 链接到主程序
target_link_libraries(medicalpro PRIVATE NewPagesLib)
```

## 与旧代码的兼容

新框架与旧代码并行存在，不会影响现有功能。可以逐步迁移：

1. 先保留旧的 MainInterfaceWidget
2. 测试新的 MainWindowNew
3. 确认功能正常后，删除旧代码

## 使用 Qt Designer 编辑 .ui 文件

1. 在 Qt Creator 中打开 `UI/Forms/*.ui` 文件
2. 使用可视化编辑器修改布局
3. 保存后自动重新编译

## 添加新页面

1. 在 `UI/Forms/` 创建新的 .ui 文件
2. 在 `UI/NewPages/` 创建对应的 .h/.cpp 文件
3. 继承 `BasePage` 类
4. 在 `PageIndex.h` 添加页面索引
5. 在 `MainWindow` 中添加页面实例和连接
6. 更新 `CMakeLists.txt`

## 信号规范

所有页面使用统一的信号进行导航：

```cpp
signals:
    void navigateTo(int pageIndex);  // 导航到指定页面
    void goBack();                   // 返回上一页
    void exitRequested();            // 退出系统
    void logoutRequested();          // 登出
```