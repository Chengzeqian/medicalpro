# Navigation Page Workspace Shell Design

## 背景

当前欢迎页、模块选择页、数据管理页、病例工作台和系统设置页已经接入 `UI/styles/three_pages_theme.qss`，整体使用深色渐变背景、低饱和卡片、状态徽标和明确的标题层级。`NavigationPage` 仍保留旧式内联 QSS、顶部 tab 导航和偏旧的右侧控制栏视觉，进入导航页后与前几页不一致。

用户选择第 3 种方案：把导航页升级为左侧流程导航、中央工作区、右侧状态栏的专业工作区结构。

## 目标

- 导航页视觉风格与欢迎页、模块选择页、病例工作台保持一致。
- 保留现有踝关节导航五阶段：准备、规划、配准、导航、评估。
- 保留现有插件调用、病例上下文、VTK 嵌入点、配准和导航运行逻辑。
- 把旧顶部 `QTabWidget` 视觉转为左侧流程导航，但继续复用 `QTabWidget` 作为内部页面容器，降低业务风险。
- 右侧状态栏持续展示病例、追踪器、配准误差、导航准入、可信度、探针标定和精度信息。
- 用户可从左侧流程导航感知当前阶段，并可按阶段切换页面。

## 非目标

- 不改算法、配准、分割、评估导出、追踪器服务和插件生命周期。
- 不引入 QML、KDDockWidgets、QWindowKit 或其他新依赖。
- 不重做 VTK 渲染控件内部外观。
- 不改病例数据模型和运行目录规则。
- 不修复已有中文乱码源文本，除本次导航页新增或直接可见文案外不做大范围编码治理。

## 现状约束

- `UI/Forms/NavigationPage.ui` 由 `ui_NavigationPage.h` 生成，`NavigationPage.cpp` 大量通过 `ui->xxx` 访问已有控件。
- `NavigationVtkBridge` 依赖 `planningViewLayout`、`fourViewLayout`、`registrationViewLayout` 等现有布局对象。
- `NavigationWorkflowCoordinator` 通过 `setWorkflowStage()` 切换 `ui->tabWidget` 当前页。
- `refreshNavigationConfidenceState()`、`updateProbeCalibrationUi()`、`updateTrackerStatus()` 通过对象名查找或直接操作状态控件。
- 当前 `three_pages_theme.qss` 未包含 `QWidget#NavigationPage` 的共享主题选择器。

## 设计方案

### 页面骨架

导航页采用三段式工作区：

- 顶部标题区：返回按钮、页面 eyebrow、标题、副标题、病例摘要。
- 主体区：左侧流程导航栏、中央工作区、右侧状态栏。
- 中央工作区继续承载现有 `QTabWidget`，但隐藏顶部 `QTabBar`，让左侧流程栏成为主要流程入口。

### 左侧流程导航栏

左侧创建 `navigationWorkflowRailFrame`，包含五个阶段按钮：

- 准备
- 规划
- 配准
- 导航
- 评估

每个按钮使用现有 `setWorkflowStage()` 驱动页面切换。当前阶段按钮设置 `workflowState="active"`，可进入但非当前阶段设置 `workflowState="idle"`。按钮文本保持简洁，配合小号说明标签展示当前病例工作流。

### 中央工作区

中央创建 `navigationWorkspaceFrame`，内部继续放置现有 `ui->tabWidget`。现有每个 tab 的内容、布局对象和控件对象名保持不变：

- `instrumentTab`
- `planningTab`
- `registrationTab`
- `navigationTab`
- `evaluationTab`

实现时隐藏 `ui->tabWidget->tabBar()`，并通过左侧流程按钮调用 `setWorkflowStage()`。现有插件、VTK 和按钮 slot 仍使用原对象。

### 右侧状态栏

右侧创建 `navigationStatusRailFrame`，包含固定状态卡片：

- 当前病例：复用 `patientInfoLabel` 的内容或同步到新状态值。
- 追踪器：复用 `trackerStatusLabel`。
- 配准误差：复用 `regErrorLabel`。
- 导航准入：复用或移动 `navigationReadinessLabel`。
- 可信度评分：复用或移动 `navigationConfidenceLabel`。
- 探针标定：复用或移动 `calibrationStatusLabel`。
- 当前精度：复用 `accuracyValueLabel` 和 `accuracyBar`。

为避免破坏旧布局，状态控件优先保持原对象名。需要在右侧展示的运行时标签由 `setupNavigationWorkspaceShell()` 创建并加入右侧栏；旧控制分组内的同名标签如果已存在，则直接移动到右侧栏或保持位置并同步样式。

### 样式策略

`three_pages_theme.qss` 扩展到 `QWidget#NavigationPage`：

- 页面背景加入统一渐变。
- 按钮、标签、Tab 容器、表格、GroupBox 使用现有颜色令牌。
- 左侧流程栏、中央工作区、右侧状态栏使用与 `DashboardPage`、`SystemSettingsPage` 一致的深色卡片。
- 状态标签使用 `statusTone="ok|warning|danger"` 属性映射颜色。
- 旧 `.ui` 内联 `styleSheet` 尽量清空或被全局主题覆盖，避免红色旧主题残留。

### 运行时行为

- `NavigationPageNew` 构造后调用 `setupNavigationWorkspaceShell()`。
- `setWorkflowStage()` 在切换 tab 后调用 `syncWorkflowRailState()`，同步左侧按钮状态和标题文案。
- `refreshPatientInfoLabel()` 更新病例摘要后同步右侧病例状态。
- `updateTrackerStatus()` 设置 `trackerStatusLabel` 文本，并设置 `statusTone`。
- `refreshNavigationConfidenceState()` 设置导航准入和可信度文本，并设置对应 `statusTone`。
- `updateRegistrationResultDisplay()` 设置配准误差文本，并按误差范围设置 `statusTone`。
- `updateAccuracyDisplay()` 设置精度文本、进度条，并按精度范围设置 `statusTone`。

## 文件变更

- 修改 `UI/NewPages/NavigationPage.h`
  - 增加 `setupNavigationWorkspaceShell()`、`syncWorkflowRailState()`、`polishNavigationWidget(QWidget*)` 等私有方法声明。
  - 增加左侧流程按钮成员，使用 `QPointer<QPushButton>` 保存，避免裸指针悬挂。

- 修改 `UI/NewPages/NavigationPage.cpp`
  - 构造函数中初始化三段式工作区。
  - 创建左侧流程栏、中央工作区、右侧状态栏，并把现有 `tabWidget` 放入中央工作区。
  - 隐藏旧 tab bar。
  - 同步流程按钮和状态属性。

- 修改 `UI/styles/three_pages_theme.qss`
  - 把 `NavigationPage` 加入共享页面选择器。
  - 增加导航页三段式工作区、状态栏、流程按钮、表格、GroupBox、进度条样式。

- 修改 `tests/unit/AnkleNavigationWorkflowContractTest.cpp` 或新增轻量契约测试
  - 验证导航页保留五阶段 tab。
  - 验证源码包含三段式工作区初始化入口。
  - 验证主题包含 `NavigationPage` 和新对象名选择器。

## 测试策略

- 红绿测试：先增加源文件契约测试，确认当前旧导航页失败。
- 实现后运行新契约测试，确认三段式结构和主题接入。
- 运行 `three_page_presentation_utils_test`，确认共享展示工具不受影响。
- 运行 `ankle_navigation_workflow_contract_test`，确认导航工作流契约不被破坏。
- 构建 `medicalpro` 或 `NewPagesLib`，确认 `.ui`、MOC、UIC 和 C++ 编译通过。

## 验收标准

- 导航页入口不再显示旧顶部红色 tab 风格。
- 页面首屏呈现顶部标题、左侧流程栏、中央工作区、右侧状态栏。
- 左侧流程栏切换准备、规划、配准、导航、评估时，现有功能页正常切换。
- 规划、配准、导航阶段的 VTK 视图仍能嵌入原工作区。
- 追踪器连接、配准误差、导航准入、可信度、标定、精度状态仍会刷新。
- 样式使用 `three_pages_theme.qss`，与欢迎页、模块选择页、病例工作台同属一套视觉语言。
- 不新增第三方依赖。

## 自审结果

- 无占位项。
- 范围集中在导航页 UI 外壳和主题，不涉及算法或插件服务重构。
- 三段式布局与用户选择的第 3 种方案一致。
- 测试策略覆盖源码契约、主题接入和现有导航工作流契约。
