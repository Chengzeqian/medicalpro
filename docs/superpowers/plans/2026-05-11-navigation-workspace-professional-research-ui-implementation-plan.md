# Navigation Workspace Professional Research UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将手术导航工作区五阶段界面优化为深色专业科研工作站，并确保规划页和导航页都采用单窗口主视图。

**Architecture:** 保留现有 `NavigationPage + NavigationWorkspaceUiBinder + NavigationWorkspaceApplicationService + NavigationVtkBridge` 结构。视觉统一放在 `UI/styles/three_pages_theme.qss`，页面结构和动态控件由 `NavigationPage.cpp` 负责，真实内容继续从 workspace snapshot、runtime state 和现有 controller 获取。

**Tech Stack:** C++20, Qt Widgets, Qt Designer `.ui`, QSS, QtTest, CMake, CTest

---

## File Structure

- `UI/styles/three_pages_theme.qss`
  - 统一导航工作区科研风格：深色面板、流程轨、状态卡、阶段面板、单窗口视图、表格、状态 tone。
- `UI/AppTheme.h`
  - 仅在需要新增主题 token 时修改；不在本轮新增依赖。
- `UI/NewPages/NavigationPage.h`
  - 声明少量 UI helper：科研面板、指标卡、规划单窗口约束、HUD 标签刷新。
- `UI/NewPages/NavigationPage.cpp`
  - 优化五阶段布局和动态控件。
  - 保持业务状态从 `NavigationWorkspaceApplicationService` 和 `NavigationWorkspaceUiBinder` 进入 UI。
- `UI/Forms/NavigationPage.ui`
  - 移除或弱化旧内联样式，保留对象名和必要容器。
- `UI/NewPages/Navigation/navigation_workspace_ui_binder.h`
  - 必要时扩展绑定项，用于准备/规划/配准/评估的专业摘要标签。
- `UI/NewPages/Navigation/navigation_workspace_ui_binder.cpp`
  - 优化摘要文案和状态 tone，不伪造业务状态。
- `tests/unit/AnkleNavigationWorkflowContractTest.cpp`
  - 增加 UI 契约测试，锁定规划单窗口、导航科研选择器、旧入口退出。
- `tests/unit/NavigationWorkspaceApplicationServiceTest.cpp`
  - 增加 binder 摘要测试，锁定准备/规划/配准/评估文案。
- `tests/unit/NavigationEvaluationSummaryFormatterTest.cpp`
  - 如评估摘要格式调整，补充病例级指标测试。

## Task 1: 锁定专业科研 UI 契约

**Files:**
- Modify: `tests/unit/AnkleNavigationWorkflowContractTest.cpp`
- Modify: `tests/unit/NavigationWorkspaceApplicationServiceTest.cpp`

- [ ] **Step 1: 在 `AnkleNavigationWorkflowContractTest` 声明 UI 契约测试**

在 private slots 末尾追加：

```cpp
void navigation_page_professional_research_theme_contract();
void navigation_page_planning_tab_uses_single_viewport_contract();
void navigation_page_stage_panels_expose_research_workspace_objects();
```

- [ ] **Step 2: 写专业科研主题契约测试**

在文件底部 `QTEST_APPLESS_MAIN` 前追加：

```cpp
void AnkleNavigationWorkflowContractTest::navigation_page_professional_research_theme_contract()
{
    const QString theme = readFile(QStringLiteral("UI/styles/three_pages_theme.qss"));

    QVERIFY2(theme.contains(QStringLiteral("QWidget#NavigationPage")),
        "NavigationPage must stay in the shared professional theme");
    QVERIFY2(theme.contains(QStringLiteral("QFrame#navigationResearchPanel")),
        "NavigationPage must style reusable research panels");
    QVERIFY2(theme.contains(QStringLiteral("QFrame#navigationMetricCard")),
        "NavigationPage must style metric cards for scientific indicators");
    QVERIFY2(theme.contains(QStringLiteral("QFrame#navigationSingleViewportFrame")),
        "NavigationPage must style single-window planning/navigation viewports");
    QVERIFY2(theme.contains(QStringLiteral("rgba(63, 183, 200"))
              || theme.contains(QStringLiteral("#3fb7c8"))
              || theme.contains(QStringLiteral("${ACCENT_SECONDARY}")),
        "NavigationPage must use blue/cyan scientific accent instead of the old magenta primary look");
    QVERIFY2(!theme.contains(QStringLiteral("#e94560")),
        "NavigationPage professional theme must not keep the old magenta accent");
}
```

- [ ] **Step 3: 写规划页单窗口契约测试**

继续追加：

```cpp
void AnkleNavigationWorkflowContractTest::navigation_page_planning_tab_uses_single_viewport_contract()
{
    const QString pageCode = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));
    const QString pageHeader = readFile(QStringLiteral("UI/NewPages/NavigationPage.h"));
    const QString pageUi = readFile(QStringLiteral("UI/Forms/NavigationPage.ui"));

    QVERIFY2(pageHeader.contains(QStringLiteral("setupPlanningSingleViewport")),
        "NavigationPage must declare a single-window planning viewport setup helper");
    QVERIFY2(pageCode.contains(QStringLiteral("setupPlanningSingleViewport();")),
        "NavigationPage constructor must install the planning single viewport");
    QVERIFY2(pageCode.contains(QStringLiteral("navigationSingleViewportFrame")),
        "Planning page must use the same single viewport object family as navigation");
    QVERIFY2(!pageCode.contains(QStringLiteral("fourViewLayout->addWidget")),
        "Planning page must not embed planning content into a four-view layout");
    QVERIFY2(!pageUi.contains(QStringLiteral("Axial"))
              && !pageUi.contains(QStringLiteral("Sagittal"))
              && !pageUi.contains(QStringLiteral("Coronal")),
        "NavigationPage.ui must not expose four-view planning labels");
}
```

- [ ] **Step 4: 写五阶段科研面板对象契约测试**

继续追加：

```cpp
void AnkleNavigationWorkflowContractTest::navigation_page_stage_panels_expose_research_workspace_objects()
{
    const QString pageCode = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));
    const QString binderHeader =
        readFile(QStringLiteral("UI/NewPages/Navigation/navigation_workspace_ui_binder.h"));

    QVERIFY2(pageCode.contains(QStringLiteral("preparationAssetMatrixFrame")),
        "Preparation page must expose the case asset matrix");
    QVERIFY2(pageCode.contains(QStringLiteral("planningEvidencePanel")),
        "Planning page must expose a read-only planning evidence panel");
    QVERIFY2(pageCode.contains(QStringLiteral("registrationResultPanel")),
        "Registration page must expose a registration result panel");
    QVERIFY2(pageCode.contains(QStringLiteral("navigationHudFrame")),
        "Navigation page must expose a lightweight digital twin HUD");
    QVERIFY2(pageCode.contains(QStringLiteral("evaluationMetricMatrixFrame")),
        "Evaluation page must expose a case-level metric matrix");
    QVERIFY2(binderHeader.contains(QStringLiteral("preparationAssetSummaryLabel")),
        "Workspace binder must accept preparation asset summary binding");
    QVERIFY2(binderHeader.contains(QStringLiteral("registrationMetricSummaryLabel")),
        "Workspace binder must accept registration metric summary binding");
}
```

- [ ] **Step 5: 在 `NavigationWorkspaceApplicationServiceTest` 声明 binder 文案测试**

在 private slots 末尾追加：

```cpp
void binder_formats_preparation_assets_for_research_workspace();
void binder_formats_registration_metrics_for_research_workspace();
```

- [ ] **Step 6: 写准备页资产摘要测试**

在文件底部 `QTEST_MAIN` 前追加：

```cpp
void NavigationWorkspaceApplicationServiceTest::binder_formats_preparation_assets_for_research_workspace()
{
    QLabel preparationAssetSummaryLabel;
    preparationAssetSummaryLabel.setObjectName(QStringLiteral("preparationAssetSummaryLabel"));

    NavigationWorkspaceUiBinder::Bindings bindings;
    bindings.preparationAssetSummaryLabel = &preparationAssetSummaryLabel;
    NavigationWorkspaceUiBinder binder(bindings);

    NavigationWorkspaceSnapshot snapshot;
    snapshot.assetState.selectedInstrumentId = QStringLiteral("instrument:probe-main");
    snapshot.assetState.selectedInstrumentDisplayName = QStringLiteral("主探针");
    snapshot.assetState.instrumentModelPath = QStringLiteral("instruments/probe.stl");
    snapshot.assetState.geometryFilePath = QStringLiteral("geometry/geometry40.ini");
    snapshot.assetState.trackingMarkerId = QStringLiteral("40");
    snapshot.preparationState.allRequiredInstrumentsCalibrated = false;
    snapshot.preparationState.blockingReasons = QStringList { QStringLiteral("存在未完成标定的器械") };

    binder.applyWorkspaceSummary(snapshot);

    const QString text = preparationAssetSummaryLabel.text();
    QVERIFY(text.contains(QStringLiteral("主探针")));
    QVERIFY(text.contains(QStringLiteral("instruments/probe.stl")));
    QVERIFY(text.contains(QStringLiteral("geometry40.ini")));
    QVERIFY(text.contains(QStringLiteral("marker")));
    QVERIFY(text.contains(QStringLiteral("存在未完成标定的器械")));
}
```

- [ ] **Step 7: 写配准指标摘要测试**

继续追加：

```cpp
void NavigationWorkspaceApplicationServiceTest::binder_formats_registration_metrics_for_research_workspace()
{
    QLabel registrationMetricSummaryLabel;
    registrationMetricSummaryLabel.setObjectName(QStringLiteral("registrationMetricSummaryLabel"));

    NavigationWorkspaceUiBinder::Bindings bindings;
    bindings.registrationMetricSummaryLabel = &registrationMetricSummaryLabel;
    NavigationWorkspaceUiBinder binder(bindings);

    NavigationWorkspaceRegistrationState state;
    state.pointCount = 8;
    state.success = true;
    state.fre = 0.62;
    state.targetTre = 1.10;
    state.coverageScore = 0.91;
    state.fusedNavigationSpaceReady = true;
    state.fusedNavigationSpacePath = QStringLiteral("registration/fused_navigation_space.json");

    binder.applyRegistrationSummary(state);

    const QString text = registrationMetricSummaryLabel.text();
    QVERIFY(text.contains(QStringLiteral("8")));
    QVERIFY(text.contains(QStringLiteral("0.62")));
    QVERIFY(text.contains(QStringLiteral("1.10")));
    QVERIFY(text.contains(QStringLiteral("0.91")));
    QVERIFY(text.contains(QStringLiteral("fused_navigation_space.json")));
}
```

- [ ] **Step 8: 运行测试确认 RED**

Run:

```powershell
cmake --build build_x64_v142 --config Release --target ankle_navigation_workflow_contract_test navigation_workspace_application_service_test
ctest --test-dir build_x64_v142 -C Release -R "^(ankle_navigation_workflow_contract_test|navigation_workspace_application_service_test)$" --output-on-failure
```

Expected:

- `navigation_page_professional_research_theme_contract` 失败，因为新选择器未齐全。
- `navigation_page_planning_tab_uses_single_viewport_contract` 失败，因为 `setupPlanningSingleViewport` 未实现。
- binder 新测试失败，因为新增 binding 字段未实现。

- [ ] **Step 9: Commit RED tests**

```powershell
git add tests/unit/AnkleNavigationWorkflowContractTest.cpp tests/unit/NavigationWorkspaceApplicationServiceTest.cpp
git commit -m "test: lock professional navigation workspace ui contract"
```

## Task 2: 统一深色科研工作站主题

**Files:**
- Modify: `UI/AppTheme.h`
- Modify: `UI/styles/three_pages_theme.qss`

- [ ] **Step 1: 调整主题 token**

在 `UI/AppTheme.h` 的 `threePageTokens()` 中将导航相关主色收敛到蓝绿色低饱和风格。保留其它页面可用 token 名称，不新增依赖：

```cpp
{ "${PAGE_BG_START}", "#060c12" },
{ "${PAGE_BG_END}", "#0c1822" },
{ "${PAGE_BG_ACCENT}", "#102935" },
{ "${SURFACE_SOFT}", "rgba(10, 19, 27, 0.84)" },
{ "${SURFACE_CARD}", "rgba(14, 27, 36, 0.90)" },
{ "${SURFACE_CARD_HOVER}", "rgba(18, 36, 47, 0.98)" },
{ "${SURFACE_PANEL}", "rgba(7, 15, 22, 0.78)" },
{ "${SURFACE_ELEVATED}", "rgba(12, 26, 34, 0.96)" },
{ "${ACCENT_PRIMARY}", "#3fb7c8" },
{ "${ACCENT_PRIMARY_HOVER}", "#54c8d8" },
{ "${ACCENT_SECONDARY}", "#47d1bd" },
{ "${SUCCESS}", "#47b881" },
{ "${WARNING}", "#d6a642" },
{ "${DANGER}", "#d96c6c" },
```

- [ ] **Step 2: 替换 NavigationPage 选择器中的大圆角和旧色**

在 `UI/styles/three_pages_theme.qss` 中更新 NavigationPage 区块：

```css
QWidget#NavigationPage QFrame#navigationHeaderFrame,
QWidget#NavigationPage QFrame#navigationBodyFrame,
QWidget#NavigationPage QFrame#navigationWorkflowRailFrame,
QWidget#NavigationPage QFrame#navigationStatusRailFrame,
QWidget#NavigationPage QFrame#navigationWorkspaceFrame {
    background-color: rgba(8, 17, 24, 0.88);
    border: 1px solid rgba(124, 160, 191, 0.14);
    border-radius: 8px;
}

QWidget#NavigationPage QFrame#navigationWorkspaceFrame {
    background-color: rgba(5, 10, 15, 0.58);
    border-color: rgba(124, 160, 191, 0.10);
}

QWidget#NavigationPage QFrame#navigationResearchPanel,
QWidget#NavigationPage QFrame#navigationMetricCard,
QWidget#NavigationPage QFrame#navigationStatusCard {
    background-color: rgba(12, 24, 32, 0.88);
    border: 1px solid rgba(124, 160, 191, 0.14);
    border-radius: 8px;
}
```

- [ ] **Step 3: 添加单窗口视图和 HUD 样式**

继续在 NavigationPage 区块追加：

```css
QWidget#NavigationPage QFrame#navigationSingleViewportFrame,
QWidget#NavigationPage QFrame#planningSingleViewportFrame,
QWidget#NavigationPage QFrame#navigationDigitalTwinViewportFrame {
    background-color: rgba(3, 7, 10, 0.94);
    border: 1px solid rgba(63, 183, 200, 0.22);
    border-radius: 8px;
}

QWidget#NavigationPage QFrame#navigationHudFrame {
    background-color: rgba(3, 7, 10, 0.78);
    border: 1px solid rgba(63, 183, 200, 0.24);
    border-radius: 6px;
}

QWidget#NavigationPage QLabel#navigationHudTitleLabel,
QWidget#NavigationPage QLabel#planningEvidenceTitleLabel,
QWidget#NavigationPage QLabel#evaluationReportTitleLabel {
    color: ${ACCENT_SECONDARY};
    font-size: 14px;
    font-weight: 700;
}
```

- [ ] **Step 4: 添加指标卡和阶段摘要字体**

追加：

```css
QWidget#NavigationPage QLabel#navigationMetricTitleLabel,
QWidget#NavigationPage QLabel#navigationStatusCardTitleLabel {
    color: ${TEXT_MUTED};
    font-size: 12px;
    font-weight: 700;
}

QWidget#NavigationPage QLabel#navigationMetricValueLabel,
QWidget#NavigationPage QLabel#navigationStagePrimaryValueLabel {
    color: ${TEXT_PRIMARY};
    font-size: 18px;
    font-weight: 700;
}

QWidget#NavigationPage QLabel#navigationMetricDetailLabel,
QWidget#NavigationPage QLabel#navigationStageDetailLabel {
    color: ${TEXT_SECONDARY};
    font-size: 14px;
}
```

- [ ] **Step 5: 将流程按钮圆角压到 8px**

修改已有 `QPushButton[workflowState="idle"]` 和 `QPushButton[workflowState="active"]`：

```css
QWidget#NavigationPage QPushButton[workflowState="idle"],
QWidget#NavigationPage QPushButton[workflowState="active"] {
    min-height: 44px;
    padding: 0 14px;
    border-radius: 8px;
    text-align: left;
}
```

- [ ] **Step 6: 运行主题契约测试**

Run:

```powershell
cmake --build build_x64_v142 --config Release --target ankle_navigation_workflow_contract_test
ctest --test-dir build_x64_v142 -C Release -R ankle_navigation_workflow_contract_test --output-on-failure
```

Expected:

- 主题相关断言通过。
- `setupPlanningSingleViewport` 等结构断言仍可能失败，留给后续任务。

- [ ] **Step 7: Commit**

```powershell
git add UI/AppTheme.h UI/styles/three_pages_theme.qss
git commit -m "style: apply professional navigation workspace theme"
```

## Task 3: 增强页面 UI helper 和三栏壳层

**Files:**
- Modify: `UI/NewPages/NavigationPage.h`
- Modify: `UI/NewPages/NavigationPage.cpp`

- [ ] **Step 1: 在 `NavigationPage.h` 声明 helper**

在 private 方法区加入：

```cpp
QFrame* createResearchPanel(const QString& objectName, const QString& title, QWidget* contentWidget);
QFrame* createMetricCard(const QString& title, const QString& value, const QString& detail, const QString& tone);
QLabel* createStageTextLabel(const QString& objectName, const QString& text);
void setupPlanningSingleViewport();
void setupNavigationHud();
void refreshNavigationHud();
```

在成员变量区加入：

```cpp
QPointer<QFrame> m_navigationHudFrame;
QPointer<QLabel> m_navigationHudSummaryLabel;
QPointer<QLabel> m_navigationHudPoseLabel;
QPointer<QLabel> m_navigationHudReadinessLabel;
```

- [ ] **Step 2: 实现 `createStageTextLabel`**

在 `createNavigationStatusValueLabel` 后加入：

```cpp
QLabel* NavigationPageNew::createStageTextLabel(const QString& objectName, const QString& text)
{
    auto* label = new QLabel(text, this);
    label->setObjectName(objectName);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}
```

- [ ] **Step 3: 实现 `createResearchPanel`**

加入：

```cpp
QFrame* NavigationPageNew::createResearchPanel(const QString& objectName, const QString& title, QWidget* contentWidget)
{
    auto* panel = new QFrame(this);
    panel->setObjectName(objectName);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    auto* titleLabel = new QLabel(title, panel);
    titleLabel->setObjectName(QStringLiteral("navigationStagePrimaryValueLabel"));
    layout->addWidget(titleLabel);
    layout->addWidget(contentWidget);
    return panel;
}
```

- [ ] **Step 4: 实现 `createMetricCard`**

加入：

```cpp
QFrame* NavigationPageNew::createMetricCard(
    const QString& title,
    const QString& value,
    const QString& detail,
    const QString& tone)
{
    auto* card = new QFrame(this);
    card->setObjectName(QStringLiteral("navigationMetricCard"));
    card->setProperty("statusTone", tone);

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(5);

    auto* titleLabel = new QLabel(title, card);
    titleLabel->setObjectName(QStringLiteral("navigationMetricTitleLabel"));
    auto* valueLabel = new QLabel(value, card);
    valueLabel->setObjectName(QStringLiteral("navigationMetricValueLabel"));
    valueLabel->setProperty("statusTone", tone);
    auto* detailLabel = new QLabel(detail, card);
    detailLabel->setObjectName(QStringLiteral("navigationMetricDetailLabel"));
    detailLabel->setWordWrap(true);

    layout->addWidget(titleLabel);
    layout->addWidget(valueLabel);
    layout->addWidget(detailLabel);
    return card;
}
```

- [ ] **Step 5: 更新 `createWorkflowRailButton` 文案**

将按钮文本改为带编号：

```cpp
const QHash<AnkleWorkflowStage, QString> stageCodes = {
    { AnkleWorkflowStage::Preparation, QStringLiteral("01  准备") },
    { AnkleWorkflowStage::Planning, QStringLiteral("02  规划") },
    { AnkleWorkflowStage::Registration, QStringLiteral("03  配准") },
    { AnkleWorkflowStage::Navigation, QStringLiteral("04  导航") },
    { AnkleWorkflowStage::Evaluation, QStringLiteral("05  评估") }
};
button->setText(stageCodes.value(stage, text));
```

- [ ] **Step 6: 在构造函数中调用后续 setup**

在 `setupPlanningReadOnlyPanels();` 后加入：

```cpp
setupPlanningSingleViewport();
```

在 `setupSingleNavigationWorkspace();` 后加入：

```cpp
setupNavigationHud();
```

- [ ] **Step 7: 构建页面目标**

Run:

```powershell
cmake --build build_x64_v142 --config Release --target medicalpro
```

Expected:

- BUILD SUCCESS 或仅出现本任务引入的可定位编译错误。

- [ ] **Step 8: Commit**

```powershell
git add UI/NewPages/NavigationPage.h UI/NewPages/NavigationPage.cpp
git commit -m "refactor: add navigation research ui helpers"
```

## Task 4: 优化准备页资产矩阵和 binder 摘要

**Files:**
- Modify: `UI/NewPages/Navigation/navigation_workspace_ui_binder.h`
- Modify: `UI/NewPages/Navigation/navigation_workspace_ui_binder.cpp`
- Modify: `UI/NewPages/NavigationPage.cpp`

- [ ] **Step 1: 扩展 binder bindings**

在 `NavigationWorkspaceUiBinder::Bindings` 中加入：

```cpp
QLabel* preparationAssetSummaryLabel = nullptr;
QLabel* registrationMetricSummaryLabel = nullptr;
```

- [ ] **Step 2: 在 `applyWorkspaceSummary` 中输出准备页资产摘要**

在 `applyWorkspaceSummary` 尾部加入：

```cpp
if (m_bindings.preparationAssetSummaryLabel) {
    QStringList lines;
    lines << QStringLiteral("活动器械：%1")
                 .arg(snapshot.assetState.selectedInstrumentDisplayName.isEmpty()
                          ? snapshot.assetState.selectedInstrumentId
                          : snapshot.assetState.selectedInstrumentDisplayName);
    lines << QStringLiteral("器械 STL：%1")
                 .arg(snapshot.assetState.instrumentModelPath.isEmpty()
                          ? QStringLiteral("未绑定")
                          : snapshot.assetState.instrumentModelPath);
    lines << QStringLiteral("geometry：%1")
                 .arg(snapshot.assetState.geometryFilePath.isEmpty()
                          ? QStringLiteral("未绑定")
                          : snapshot.assetState.geometryFilePath);
    lines << QStringLiteral("marker：%1")
                 .arg(snapshot.assetState.trackingMarkerId.isEmpty()
                          ? QStringLiteral("未绑定")
                          : snapshot.assetState.trackingMarkerId);
    if (!snapshot.preparationState.blockingReasons.isEmpty()) {
        lines << QStringLiteral("阻塞原因：%1")
                     .arg(snapshot.preparationState.blockingReasons.join(QStringLiteral("；")));
    }
    m_bindings.preparationAssetSummaryLabel->setText(lines.join(QStringLiteral("\n")));
    applyTone(
        m_bindings.preparationAssetSummaryLabel,
        snapshot.preparationState.allRequiredInstrumentsCalibrated ? QStringLiteral("ok") : QStringLiteral("warning"));
}
```

- [ ] **Step 3: 在 `NavigationPage.cpp` 创建准备页资产面板**

在 `setupNavigationWorkspaceShell()` 之后新增函数或在 `setupPlanningReadOnlyPanels()` 前创建：

```cpp
auto* preparationAssetSummaryLabel = findChild<QLabel*>(QStringLiteral("preparationAssetSummaryLabel"));
if (!preparationAssetSummaryLabel) {
    preparationAssetSummaryLabel = createStageTextLabel(
        QStringLiteral("preparationAssetSummaryLabel"),
        QStringLiteral("病例资产：等待加载工作包"));
    auto* panel = createResearchPanel(
        QStringLiteral("preparationAssetMatrixFrame"),
        QStringLiteral("病例资产矩阵"),
        preparationAssetSummaryLabel);
    ui->instrumentTabLayout->insertWidget(0, panel);
}
```

- [ ] **Step 4: 将 binding 接入构造函数**

在 `NavigationWorkspaceUiBinder::Bindings` 初始化中加入：

```cpp
.preparationAssetSummaryLabel = findChild<QLabel*>(QStringLiteral("preparationAssetSummaryLabel")),
.registrationMetricSummaryLabel = findChild<QLabel*>(QStringLiteral("registrationMetricSummaryLabel")),
```

- [ ] **Step 5: 更新 instrument card 样式对象名**

在 `loadInstruments()` 中给器械卡设置对象名：

```cpp
card->setObjectName(QStringLiteral("navigationInstrumentAssetCard"));
thumbLabel->setObjectName(QStringLiteral("navigationInstrumentPreviewLabel"));
detailLabel->setObjectName(QStringLiteral("navigationStageDetailLabel"));
```

移除对应 `setStyleSheet(...)` 内联样式，交由 QSS。

- [ ] **Step 6: 运行准备页相关测试**

Run:

```powershell
cmake --build build_x64_v142 --config Release --target navigation_workspace_application_service_test ankle_navigation_workflow_contract_test
ctest --test-dir build_x64_v142 -C Release -R "^(navigation_workspace_application_service_test|ankle_navigation_workflow_contract_test)$" --output-on-failure
```

Expected:

- 准备页 binder 测试通过。

- [ ] **Step 7: Commit**

```powershell
git add UI/NewPages/Navigation/navigation_workspace_ui_binder.h UI/NewPages/Navigation/navigation_workspace_ui_binder.cpp UI/NewPages/NavigationPage.cpp
git commit -m "feat: present preparation assets as research matrix"
```

## Task 5: 规划页收口为单窗口只读证据页

**Files:**
- Modify: `UI/NewPages/NavigationPage.h`
- Modify: `UI/NewPages/NavigationPage.cpp`
- Modify: `UI/Forms/NavigationPage.ui`

- [ ] **Step 1: 实现 `setupPlanningSingleViewport`**

在 `NavigationPage.cpp` 加入：

```cpp
void NavigationPageNew::setupPlanningSingleViewport()
{
    if (!ui || !ui->planningViewFrame) {
        return;
    }

    ui->planningViewFrame->setObjectName(QStringLiteral("planningSingleViewportFrame"));
    ui->planningViewFrame->setProperty("viewportRole", QStringLiteral("planning"));
    if (ui->planningViewPlaceholder) {
        ui->planningViewPlaceholder->setText(QStringLiteral("单窗口规划证据视图\n目标骨 / 目标区域 / 约束区 / 已绑定影像"));
        ui->planningViewPlaceholder->setObjectName(QStringLiteral("planningSingleViewportPlaceholder"));
    }
}
```

- [ ] **Step 2: 优化 `setupPlanningReadOnlyPanels` 标题和对象名**

将 group 对象名和标题改为：

```cpp
auto* group = new QGroupBox(QStringLiteral("规划证据"), ui->planningControlPanel);
group->setObjectName(QStringLiteral("planningEvidencePanel"));
```

将 label 初始文本改为：

```cpp
label->setText(QStringLiteral("规划证据：等待加载病例规划\n主视图：单窗口只读"));
```

- [ ] **Step 3: 隐藏旧分割和假体 group**

在 `setupPlanningReadOnlyPanels()` 尾部加入：

```cpp
if (ui->segmentationGroup) {
    ui->segmentationGroup->hide();
}
if (ui->prosthesisGroup) {
    ui->prosthesisGroup->hide();
}
```

保留 `loadDicomButton` 时，将其移动到规划证据面板或保留为只读查看入口：

```cpp
if (ui->loadDicomButton) {
    ui->loadDicomButton->setText(QStringLiteral("查看病例 DICOM"));
}
```

- [ ] **Step 4: 清理 `.ui` 中导航四视图标签的可见暴露**

在 `UI/Forms/NavigationPage.ui` 中确认规划页不包含四窗口标签。导航页旧四视图 frame 保留但由 `NavigationVtkBridge` 替换，不在规划页复用。

- [ ] **Step 5: 运行规划页契约测试**

Run:

```powershell
cmake --build build_x64_v142 --config Release --target ankle_navigation_workflow_contract_test
ctest --test-dir build_x64_v142 -C Release -R ankle_navigation_workflow_contract_test --output-on-failure
```

Expected:

- `navigation_page_planning_tab_uses_single_viewport_contract` PASS。

- [ ] **Step 6: Commit**

```powershell
git add UI/NewPages/NavigationPage.h UI/NewPages/NavigationPage.cpp UI/Forms/NavigationPage.ui
git commit -m "feat: use single planning viewport"
```

## Task 6: 配准页优化为采点和结果科研面板

**Files:**
- Modify: `UI/NewPages/Navigation/navigation_workspace_ui_binder.cpp`
- Modify: `UI/NewPages/NavigationPage.cpp`

- [ ] **Step 1: 在 `applyRegistrationSummary` 输出指标摘要**

在已有 `registrationSummaryLabel` 更新后加入：

```cpp
if (m_bindings.registrationMetricSummaryLabel) {
    QStringList metrics;
    metrics << QStringLiteral("采点数：%1").arg(registrationState.pointCount);
    metrics << QStringLiteral("FRE：%1 mm").arg(registrationState.fre, 0, 'f', 2);
    metrics << QStringLiteral("target TRE：%1 mm").arg(registrationState.targetTre, 0, 'f', 2);
    metrics << QStringLiteral("覆盖评分：%1").arg(registrationState.coverageScore, 0, 'f', 2);
    metrics << QStringLiteral("融合空间：%1")
                   .arg(registrationState.fusedNavigationSpaceReady
                            ? registrationState.fusedNavigationSpacePath
                            : QStringLiteral("未就绪"));
    m_bindings.registrationMetricSummaryLabel->setText(metrics.join(QStringLiteral("\n")));
    applyTone(
        m_bindings.registrationMetricSummaryLabel,
        registrationState.fusedNavigationSpaceReady ? QStringLiteral("ok") : QStringLiteral("warning"));
}
```

- [ ] **Step 2: 在 `setupRegistrationActionVisibility` 创建指标面板**

在配准摘要 group 后新增：

```cpp
auto* metricLabel = findChild<QLabel*>(QStringLiteral("registrationMetricSummaryLabel"));
if (!metricLabel) {
    metricLabel = createStageTextLabel(
        QStringLiteral("registrationMetricSummaryLabel"),
        QStringLiteral("配准指标：等待计算"));
    auto* metricPanel = createResearchPanel(
        QStringLiteral("registrationResultPanel"),
        QStringLiteral("配准结果"),
        metricLabel);
    ui->registrationControlLayout->insertWidget(1, metricPanel);
}
```

- [ ] **Step 3: 调整配准按钮文本**

在 `setupRegistrationActionVisibility()` 中加入：

```cpp
if (ui->collectPointButton) {
    ui->collectPointButton->setText(QStringLiteral("采集配准点"));
}
if (ui->deletePointButton) {
    ui->deletePointButton->setText(QStringLiteral("删除选中点"));
}
if (ui->clearAllPointsButton) {
    ui->clearAllPointsButton->setText(QStringLiteral("清空点集"));
}
if (ui->computeRegButton) {
    ui->computeRegButton->setText(QStringLiteral("计算配准"));
}
```

- [ ] **Step 4: 移除配准按钮内联红灰样式**

在 `UI/Forms/NavigationPage.ui` 中清空 `deletePointButton`、`clearAllPointsButton` 的内联 `styleSheet`，改用统一 QSS。

- [ ] **Step 5: 运行测试**

Run:

```powershell
cmake --build build_x64_v142 --config Release --target navigation_workspace_application_service_test ankle_navigation_workflow_contract_test
ctest --test-dir build_x64_v142 -C Release -R "^(navigation_workspace_application_service_test|ankle_navigation_workflow_contract_test)$" --output-on-failure
```

Expected:

- 配准指标 binder 测试通过。
- 旧 2D-3D 入口契约继续通过。

- [ ] **Step 6: Commit**

```powershell
git add UI/NewPages/Navigation/navigation_workspace_ui_binder.cpp UI/NewPages/NavigationPage.cpp UI/Forms/NavigationPage.ui
git commit -m "feat: present registration metrics professionally"
```

## Task 7: 导航页强化单 3D 数字孪生 HUD

**Files:**
- Modify: `UI/NewPages/NavigationPage.h`
- Modify: `UI/NewPages/NavigationPage.cpp`

- [ ] **Step 1: 实现 `setupNavigationHud`**

在 `NavigationPage.cpp` 加入：

```cpp
void NavigationPageNew::setupNavigationHud()
{
    if (m_navigationHudFrame || !ui || !ui->navigationTabLayout) {
        return;
    }

    m_navigationHudFrame = new QFrame(ui->navigationTab);
    m_navigationHudFrame->setObjectName(QStringLiteral("navigationHudFrame"));
    auto* layout = new QVBoxLayout(m_navigationHudFrame);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(6);

    auto* titleLabel = new QLabel(QStringLiteral("数字孪生 HUD"), m_navigationHudFrame);
    titleLabel->setObjectName(QStringLiteral("navigationHudTitleLabel"));
    m_navigationHudSummaryLabel = createStageTextLabel(
        QStringLiteral("navigationHudSummaryLabel"),
        QStringLiteral("骨 STL：待加载\n器械 STL：待加载"));
    m_navigationHudPoseLabel = createStageTextLabel(
        QStringLiteral("navigationHudPoseLabel"),
        QStringLiteral("位姿：0.00 / 0.00 / 0.00 mm"));
    m_navigationHudReadinessLabel = createStageTextLabel(
        QStringLiteral("navigationHudReadinessLabel"),
        QStringLiteral("准入：未就绪"));

    layout->addWidget(titleLabel);
    layout->addWidget(m_navigationHudSummaryLabel);
    layout->addWidget(m_navigationHudPoseLabel);
    layout->addWidget(m_navigationHudReadinessLabel);
    ui->navigationControlLayout->insertWidget(0, m_navigationHudFrame);
}
```

- [ ] **Step 2: 实现 `refreshNavigationHud`**

加入：

```cpp
void NavigationPageNew::refreshNavigationHud()
{
    if (!m_navigationHudSummaryLabel || !m_runtimeCoordinator) {
        return;
    }

    const NavigationDisplayState displayState =
        m_runtimeCoordinator->buildDisplayState(activeBoneModelPaths(), activeInstrumentModelPath());
    m_navigationHudSummaryLabel->setText(
        QStringLiteral("骨 STL：%1\n器械 STL：%2\n器械可见：%3")
            .arg(displayState.boneModelPaths.isEmpty()
                     ? QStringLiteral("待加载")
                     : QString::number(displayState.boneModelPaths.size()))
            .arg(displayState.activeToolModelPath.isEmpty()
                     ? QStringLiteral("待加载")
                     : displayState.activeToolModelPath)
            .arg(displayState.toolVisible ? QStringLiteral("是") : QStringLiteral("否")));

    if (m_navigationHudReadinessLabel && m_workspaceApplicationService) {
        const NavigationWorkspaceSnapshot snapshot = m_workspaceApplicationService->currentSnapshot();
        m_navigationHudReadinessLabel->setText(
            QStringLiteral("准入：%1\n可信度：%2")
                .arg(snapshot.navigationState.allowNavigation ? QStringLiteral("允许") : QStringLiteral("阻塞"))
                .arg(snapshot.navigationState.confidence, 0, 'f', 2));
        setStatusTone(
            m_navigationHudReadinessLabel,
            snapshot.navigationState.allowNavigation ? QStringLiteral("ok") : QStringLiteral("warning"));
    }
}
```

- [ ] **Step 3: 在位姿刷新时更新 HUD**

在 `updatePositionDisplay` 末尾加入：

```cpp
if (m_navigationHudPoseLabel) {
    m_navigationHudPoseLabel->setText(
        QStringLiteral("位姿：%1 / %2 / %3 mm")
            .arg(x, 0, 'f', 2)
            .arg(y, 0, 'f', 2)
            .arg(z, 0, 'f', 2));
}
```

- [ ] **Step 4: 在 `refreshNavigationWorkspace` 和 `refreshRealtimeDigitalTwin` 后刷新 HUD**

在 `refreshNavigationWorkspace()` 中加入：

```cpp
refreshNavigationHud();
```

在 `refreshRealtimeDigitalTwin()` 末尾加入：

```cpp
refreshNavigationHud();
```

- [ ] **Step 5: 运行导航桥测试**

Run:

```powershell
cmake --build build_x64_v142 --config Release --target navigation_vtk_bridge_test ankle_navigation_workflow_contract_test
ctest --test-dir build_x64_v142 -C Release -R "^(navigation_vtk_bridge_test|ankle_navigation_workflow_contract_test)$" --output-on-failure
```

Expected:

- 单 3D 导航桥测试继续通过。
- HUD 对象契约通过。

- [ ] **Step 6: Commit**

```powershell
git add UI/NewPages/NavigationPage.h UI/NewPages/NavigationPage.cpp
git commit -m "feat: add digital twin navigation hud"
```

## Task 8: 评估页改为病例级科研报告面板

**Files:**
- Modify: `UI/NewPages/NavigationPage.cpp`
- Modify: `UI/NewPages/Navigation/navigation_workspace_ui_binder.cpp`
- Modify: `tests/unit/NavigationEvaluationSummaryFormatterTest.cpp`

- [ ] **Step 1: 扩展评估摘要格式测试**

在 `NavigationEvaluationSummaryFormatterTest` private slots 追加：

```cpp
void formatter_presents_case_level_research_metrics_for_ui();
```

在底部追加：

```cpp
void NavigationEvaluationSummaryFormatterTest::formatter_presents_case_level_research_metrics_for_ui()
{
    NavigationWorkspaceSnapshot snapshot;
    snapshot.caseId = QStringLiteral("ankle-case-research-ui-001");
    snapshot.registrationState.success = true;
    snapshot.registrationState.fre = 0.62;
    snapshot.registrationState.targetTre = 1.10;
    snapshot.navigationState.hasRunRecord = true;
    snapshot.navigationState.summaryText = QStringLiteral("导航过程稳定");
    snapshot.evaluationState.hasSummary = true;
    snapshot.evaluationState.reportReady = true;
    snapshot.evaluationState.errorMetrics.insert(QStringLiteral("trackingLatencyMs"), 31.0);
    snapshot.evaluationState.errorMetrics.insert(QStringLiteral("trackingJitterMm"), 0.28);
    snapshot.evaluationState.errorMetrics.insert(QStringLiteral("visibleFrameRatio"), 0.95);

    const NavigationEvaluationSummary summary = buildNavigationEvaluationSummary(snapshot);

    QVERIFY(summary.hasData);
    QVERIFY(summary.headerText.contains(QStringLiteral("ankle-case-research-ui-001")));
    QVERIFY(summary.registrationText.contains(QStringLiteral("0.62")));
    QVERIFY(summary.trackingText.contains(QStringLiteral("31")));
    QVERIFY(summary.trackingText.contains(QStringLiteral("0.28")));
    QVERIFY(summary.trackingText.contains(QStringLiteral("0.95"))
              || summary.trackingText.contains(QStringLiteral("95")));
}
```

- [ ] **Step 2: 优化 `setupEvaluationWorkspace` 对象名**

将 group 创建改为：

```cpp
auto* group = new QGroupBox(QStringLiteral("病例级科研评估报告"), ui->evaluationTab);
group->setObjectName(QStringLiteral("evaluationMetricMatrixFrame"));
```

将 label 初始文本改为：

```cpp
label->setText(QStringLiteral("评估报告：等待生成\n指标：配准误差 / tracking latency / jitter / visible frame ratio"));
```

- [ ] **Step 3: 优化 `applyEvaluationSummary` 文案**

在 `NavigationWorkspaceUiBinder::applyEvaluationSummary` 中将指标输出格式保持稳定：

```cpp
lines.append(QStringLiteral("报告导出：%1")
                 .arg(evaluationState.reportReady ? QStringLiteral("已就绪") : QStringLiteral("未就绪")));
```

确保 `trackingLatencyMs`、`trackingJitterMm`、`visibleFrameRatio` 从 `errorMetrics` 输出。

- [ ] **Step 4: 调整导出按钮文案**

在 `setupEvaluationWorkspace()` 中加入：

```cpp
if (ui->exportEvaluationSummaryButton) {
    ui->exportEvaluationSummaryButton->setText(QStringLiteral("导出病例级评估报告"));
}
```

- [ ] **Step 5: 运行评估测试**

Run:

```powershell
cmake --build build_x64_v142 --config Release --target navigation_evaluation_summary_formatter_test navigation_workspace_application_service_test
ctest --test-dir build_x64_v142 -C Release -R "^(navigation_evaluation_summary_formatter_test|navigation_workspace_application_service_test)$" --output-on-failure
```

Expected:

- 评估摘要测试通过。
- binder 评估摘要测试继续通过。

- [ ] **Step 6: Commit**

```powershell
git add UI/NewPages/NavigationPage.cpp UI/NewPages/Navigation/navigation_workspace_ui_binder.cpp tests/unit/NavigationEvaluationSummaryFormatterTest.cpp
git commit -m "feat: present evaluation as research report"
```

## Task 9: 清理 `.ui` 旧内联样式和验证

**Files:**
- Modify: `UI/Forms/NavigationPage.ui`
- Modify: `docs/superpowers/specs/2026-05-11-navigation-workspace-professional-research-ui-design.md`

- [ ] **Step 1: 清理旧紫红样式残留**

在 `UI/Forms/NavigationPage.ui` 中移除或清空包含以下旧色的内联样式：

```text
#e94560
#ff6b6b
rgba(233, 69, 96
```

保留 objectName 和布局结构，样式交由 `three_pages_theme.qss`。

- [ ] **Step 2: 增加实现状态到 spec**

在设计文档末尾追加：

```md
## 10. 实施状态

- 专业科研工作站主题：已实现
- 准备页病例资产矩阵：已实现
- 规划页单窗口只读证据页：已实现
- 配准页采点和结果面板：已实现
- 导航页单 3D 数字孪生 HUD：已实现
- 评估页病例级科研报告：已实现
```

- [ ] **Step 3: 运行完整导航 UI 回归**

Run:

```powershell
ctest --test-dir build_x64_v142 -C Release -R "^(ankle_navigation_workflow_contract_test|navigation_workspace_application_service_test|navigation_evaluation_summary_formatter_test|navigation_vtk_bridge_test)$" --output-on-failure
```

Expected:

- All listed tests PASS。

- [ ] **Step 4: 构建主程序**

Run:

```powershell
cmake --build build_x64_v142 --config Release --target medicalpro
```

Expected:

- BUILD SUCCESS。

- [ ] **Step 5: Commit**

```powershell
git add UI/Forms/NavigationPage.ui docs/superpowers/specs/2026-05-11-navigation-workspace-professional-research-ui-design.md
git commit -m "chore: verify professional navigation workspace ui"
```

## Self-Review

### Spec coverage

- 深色科研工作站视觉：Task 2。
- 左侧流程轨、中间主工作区、右侧状态栏：Task 2、Task 3。
- 准备页资产矩阵：Task 4。
- 规划页单窗口只读证据页：Task 5。
- 配准页采点和结果面板：Task 6。
- 导航页单 3D 数字孪生和 HUD：Task 7。
- 评估页病例级报告：Task 8。
- 验证和文档回写：Task 9。

### Placeholder scan

- 本计划不包含 TBD、TODO、implement later。
- 所有任务都有明确文件、代码片段、命令和预期结果。

### Type consistency

- 新增 helper 均声明在 `NavigationPage.h`，实现在 `NavigationPage.cpp`。
- 新增 binder 字段命名为 `preparationAssetSummaryLabel` 和 `registrationMetricSummaryLabel`，测试和实现一致。
- 规划单窗口 helper 命名为 `setupPlanningSingleViewport`，测试和实现一致。

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-05-11-navigation-workspace-professional-research-ui-implementation-plan.md`. Two execution options:

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

Which approach?
