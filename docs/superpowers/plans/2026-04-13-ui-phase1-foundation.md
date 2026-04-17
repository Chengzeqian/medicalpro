# UI Phase 1 Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 WelcomePage、ModuleSelectionPage、DashboardPage 的乱码问题，并建立三页共用的主题令牌与公共 QSS 基础层。

**Architecture:** 保持现有 `Qt Widgets + .ui + NewPages` 架构不变，在应用级注入公共主题，在页面级移除主要内联样式，并修正三页 `.ui` / `.cpp` 中所有用户可见乱码文案。动态创建控件统一改为可被公共 QSS 命中的对象名样式。

**Tech Stack:** Qt Widgets、Qt Style Sheet、CMake、VS Code x64 Release 预设

---

### Task 1: 写入项目现状与 Phase 1 留档

**Files:**
- Create: `docs/current_status_and_project_overview.md`
- Create: `docs/superpowers/plans/2026-04-13-ui-phase1-foundation.md`

- [ ] **Step 1: 写入项目现状文档**

```md
# MedicalPro 当前完成事项与项目现状说明

- 已固定 x64 构建与运行链路
- 已固定 VS Code 的 launch/tasks
- 已保留欢迎页并取消强制登录
- 当前进入 Phase 1：三页乱码修复 + 主题令牌 + 公共 QSS
```

- [ ] **Step 2: 写入 Phase 1 实施计划**

```md
### Task 2: 主题层
### Task 3: 三页 .ui 重写
### Task 4: 三页 C++ 可见字符串修复
### Task 5: x64 Release 构建验证
```

- [ ] **Step 3: 确认文档已保存**

Run: `rg -n "Phase 1|x64|欢迎页" docs/current_status_and_project_overview.md docs/superpowers/plans/2026-04-13-ui-phase1-foundation.md`

Expected: 两份文档都能检索到关键字段

### Task 2: 建立应用级主题令牌与公共 QSS

**Files:**
- Create: `UI/AppTheme.h`
- Create: `UI/styles/three_pages_theme.qss`
- Modify: `resources.qrc`
- Modify: `main.cpp`

- [ ] **Step 1: 新增主题令牌加载器**

```cpp
namespace AppTheme {
inline void applyThreePageTheme(QApplication& app);
}
```

- [ ] **Step 2: 新增公共 QSS 模板**

```qss
QWidget#WelcomePage { ... }
QWidget#ModuleSelectionPage { ... }
QWidget#DashboardPage { ... }
```

- [ ] **Step 3: 把 QSS 加入资源系统**

```xml
<file>UI/styles/three_pages_theme.qss</file>
```

- [ ] **Step 4: 在 `main.cpp` 中加载主题**

```cpp
#include "UI/AppTheme.h"
AppTheme::applyThreePageTheme(app);
```

- [ ] **Step 5: 运行构建验证主题层未引入编译错误**

Run: `cmake --build --preset x64-release`

Expected: `medicalpro.exe` 成功生成

### Task 3: 重写三页 `.ui`，移除主要内联样式并修正乱码

**Files:**
- Modify: `UI/Forms/WelcomePage.ui`
- Modify: `UI/Forms/ModuleSelectionPage.ui`
- Modify: `UI/Forms/DashboardPage.ui`

- [ ] **Step 1: 重写 WelcomePage 结构与文案**

```xml
<widget class="QPushButton" name="enterButton">
    <property name="text">
        <string>进入系统</string>
    </property>
</widget>
```

- [ ] **Step 2: 重写 ModuleSelectionPage 卡片结构与文案**

```xml
<widget class="QLabel" name="ankleSurgeryTitle">
    <property name="text">
        <string>踝关节手术</string>
    </property>
</widget>
```

- [ ] **Step 3: 重写 DashboardPage 文案并保留现有对象名**

```xml
<widget class="QLabel" name="patientListTitle">
    <property name="text">
        <string>患者列表</string>
    </property>
</widget>
```

- [ ] **Step 4: 运行检索确认三页不再含乱码关键串**

Run: `rg -n "娆|鍖|閫|鎮|璇" UI/Forms/WelcomePage.ui UI/Forms/ModuleSelectionPage.ui UI/Forms/DashboardPage.ui`

Expected: 不应再命中三页用户文案中的乱码串

### Task 4: 修复三页 C++ 可见字符串与动态样式

**Files:**
- Modify: `UI/NewPages/WelcomePage.cpp`
- Modify: `UI/NewPages/ModuleSelectionPage.cpp`
- Modify: `UI/NewPages/DashboardPage.cpp`

- [ ] **Step 1: 修复 WelcomePage 可见文本相关逻辑**

```cpp
ui->logoLabel->setText(QStringLiteral("MNS"));
```

- [ ] **Step 2: 修复 ModuleSelectionPage 的用户提示与确认框文案**

```cpp
ui->userInfoLabel->setText(QStringLiteral("欢迎，%1").arg(username));
```

- [ ] **Step 3: 修复 DashboardPage 的测试数据、警告提示与空态文案**

```cpp
showWarning(QStringLiteral("进入导航"), QStringLiteral("请先选择一位患者。"));
```

- [ ] **Step 4: 把 Dashboard 动态 DICOM 卡片改为对象名样式**

```cpp
card->setObjectName("dicomStudyCard");
thumbLabel->setObjectName("dicomThumbLabel");
dateLabel->setObjectName("dicomDateLabel");
```

- [ ] **Step 5: 检索确认三页 `.cpp` 不再保留主要乱码文案**

Run: `rg -n "娆|鍖|閫|鎮|璇" UI/NewPages/WelcomePage.cpp UI/NewPages/ModuleSelectionPage.cpp UI/NewPages/DashboardPage.cpp`

Expected: 不应再命中三页用户可见文案中的乱码串

### Task 5: x64 Release 构建验证

**Files:**
- Test: `build_x64/Release/medicalpro.exe`

- [ ] **Step 1: 执行完整构建**

Run: `cmake --build --preset x64-release`

Expected: `medicalpro.vcxproj -> ...\\build_x64\\Release\\medicalpro.exe`

- [ ] **Step 2: 确认产物存在**

Run: `Get-Item build_x64/Release/medicalpro.exe | Select-Object FullName, LastWriteTime`

Expected: 输出正确的可执行文件路径和最新时间戳
