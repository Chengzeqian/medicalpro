# Navigation Page Workspace Shell Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `NavigationPage` 从旧顶部 tab 布局升级为左侧流程导航、中央工作区、右侧状态栏的统一工作区外壳。

**Architecture:** 继续复用 `UI/Forms/NavigationPage.ui` 生成的现有控件和 `QTabWidget` 页面容器，在 `NavigationPageNew` 构造后用 C++ 重新组织外层布局。左侧流程按钮调用现有 `setWorkflowStage()`，中央工作区保留原 tab 内容，右侧状态栏复用现有状态控件或同步新状态标签。

**Tech Stack:** Qt Widgets、C++17、Qt Test、QSS、现有 `three_pages_theme.qss` 主题令牌。

---

## File Structure

- Modify: `tests/unit/AnkleNavigationWorkflowContractTest.cpp`
  - 添加源码级契约测试，先验证当前旧导航页不满足三段式工作区和主题接入。
- Modify: `UI/NewPages/NavigationPage.h`
  - 声明三段式外壳初始化、流程栏同步和状态样式刷新方法。
  - 增加左侧流程按钮与右侧状态标签的 `QPointer` 成员。
- Modify: `UI/NewPages/NavigationPage.cpp`
  - 构造函数调用外壳初始化。
  - 创建左流程栏、中央工作区、右状态栏。
  - 隐藏旧 tab bar，并将流程按钮连接到现有阶段切换。
  - 同步 `statusTone`、`workflowState` 属性。
- Modify: `UI/styles/three_pages_theme.qss`
  - 将 `QWidget#NavigationPage` 加入共享主题选择器。
  - 增加导航页专属的流程栏、工作区、状态栏、表格、GroupBox、进度条样式。

## Task 1: Add Workspace Shell Contract Test

**Files:**
- Modify: `tests/unit/AnkleNavigationWorkflowContractTest.cpp`

- [ ] **Step 1: Add failing contract test declarations**

在 `AnkleNavigationWorkflowContractTest` 的 `private slots:` 区域添加：

```cpp
void navigation_page_uses_three_panel_workspace_shell();
void navigation_page_theme_includes_navigation_selectors();
```

- [ ] **Step 2: Add failing contract test bodies**

在文件末尾、`QTEST_APPLESS_MAIN(AnkleNavigationWorkflowContractTest)` 之前添加：

```cpp
void AnkleNavigationWorkflowContractTest::navigation_page_uses_three_panel_workspace_shell()
{
    const QString navigationHeader = readFile(QStringLiteral("UI/NewPages/NavigationPage.h"));
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));

    QVERIFY2(navigationHeader.contains(QStringLiteral("setupNavigationWorkspaceShell")),
        "NavigationPage must declare a setup hook for the modern workspace shell");
    QVERIFY2(navigationHeader.contains(QStringLiteral("syncWorkflowRailState")),
        "NavigationPage must declare a sync hook for the left workflow rail");
    QVERIFY2(navigationSource.contains(QStringLiteral("navigationWorkflowRailFrame")),
        "NavigationPage must create a left workflow rail frame");
    QVERIFY2(navigationSource.contains(QStringLiteral("navigationWorkspaceFrame")),
        "NavigationPage must create a central workspace frame");
    QVERIFY2(navigationSource.contains(QStringLiteral("navigationStatusRailFrame")),
        "NavigationPage must create a right status rail frame");
    QVERIFY2(navigationSource.contains(QStringLiteral("ui->tabWidget->tabBar()->hide()")),
        "NavigationPage must hide the old top tab bar after adding the left workflow rail");
}

void AnkleNavigationWorkflowContractTest::navigation_page_theme_includes_navigation_selectors()
{
    const QString theme = readFile(QStringLiteral("UI/styles/three_pages_theme.qss"));

    QVERIFY2(theme.contains(QStringLiteral("QWidget#NavigationPage")),
        "three page theme must include NavigationPage in the shared page selectors");
    QVERIFY2(theme.contains(QStringLiteral("QFrame#navigationWorkflowRailFrame")),
        "theme must style the navigation workflow rail");
    QVERIFY2(theme.contains(QStringLiteral("QFrame#navigationStatusRailFrame")),
        "theme must style the navigation status rail");
}
```

- [ ] **Step 3: Build the existing contract test**

Run:

```powershell
cmake --build build_x64_v142 --target ankle_navigation_workflow_contract_test --config Release
```

Expected: build succeeds.

- [ ] **Step 4: Verify RED**

Run:

```powershell
ctest --test-dir build_x64_v142 -C Release -R ankle_navigation_workflow_contract_test --output-on-failure
```

Expected: FAIL. The failure should mention missing `setupNavigationWorkspaceShell` or missing `QWidget#NavigationPage`.

## Task 2: Declare Navigation Workspace Shell API

**Files:**
- Modify: `UI/NewPages/NavigationPage.h`

- [ ] **Step 1: Add required includes**

Add `QHash` and `QLabel` forward declarations/includes as needed:

```cpp
#include <QHash>
```

Forward declarations near the other class declarations:

```cpp
class QFrame;
class QLabel;
class QPushButton;
class QVBoxLayout;
```

- [ ] **Step 2: Add private method declarations**

In the private method section, after `setWorkflowStage(AnkleWorkflowStage stage);`, add:

```cpp
void setupNavigationWorkspaceShell();
QFrame* createNavigationStatusCard(const QString& title, QWidget* valueWidget);
QLabel* createNavigationStatusValueLabel(const QString& objectName, const QString& initialText);
QPushButton* createWorkflowRailButton(const QString& text, AnkleWorkflowStage stage);
void syncWorkflowRailState();
void syncNavigationStatusSummary();
void setStatusTone(QWidget* widget, const QString& tone);
void polishNavigationWidget(QWidget* widget);
```

- [ ] **Step 3: Add private members**

Near the existing UI member block, add:

```cpp
QHash<AnkleWorkflowStage, QPointer<QPushButton>> m_workflowRailButtons;
QPointer<QLabel> m_navigationPatientSummaryLabel;
QPointer<QLabel> m_navigationStageSummaryLabel;
QPointer<QLabel> m_navigationCaseStatusLabel;
```

- [ ] **Step 4: Build to verify declarations are syntactically valid after implementation task**

Do not build yet if Task 3 is not implemented. This task is complete after header edits.

## Task 3: Build the Three-Panel Shell in NavigationPage.cpp

**Files:**
- Modify: `UI/NewPages/NavigationPage.cpp`

- [ ] **Step 1: Add Qt includes**

Add these includes with the existing Qt includes:

```cpp
#include <QButtonGroup>
#include <QFrame>
#include <QGridLayout>
#include <QHash>
#include <QStyle>
#include <QTabBar>
```

- [ ] **Step 2: Call shell setup from constructor**

After `setObjectName("NavigationPage");` and before service initialization, add:

```cpp
setupNavigationWorkspaceShell();
```

- [ ] **Step 3: Implement helper methods**

Add these methods before `onActivated()`:

```cpp
QPushButton* NavigationPageNew::createWorkflowRailButton(const QString& text, AnkleWorkflowStage stage)
{
    auto* button = new QPushButton(text, this);
    button->setCursor(Qt::PointingHandCursor);
    button->setCheckable(true);
    button->setProperty("workflowState", QStringLiteral("idle"));
    connect(button, &QPushButton::clicked, this, [this, stage]() {
        setWorkflowStage(stage);
    });
    m_workflowRailButtons.insert(stage, button);
    return button;
}

QLabel* NavigationPageNew::createNavigationStatusValueLabel(const QString& objectName, const QString& initialText)
{
    auto* label = new QLabel(initialText, this);
    label->setObjectName(objectName);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setProperty("statusTone", QStringLiteral("warning"));
    return label;
}

QFrame* NavigationPageNew::createNavigationStatusCard(const QString& title, QWidget* valueWidget)
{
    auto* card = new QFrame(this);
    card->setObjectName(QStringLiteral("navigationStatusCard"));
    card->setFrameShape(QFrame::StyledPanel);

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(8);

    auto* titleLabel = new QLabel(title, card);
    titleLabel->setObjectName(QStringLiteral("navigationStatusCardTitleLabel"));
    layout->addWidget(titleLabel);
    layout->addWidget(valueWidget);

    return card;
}

void NavigationPageNew::setupNavigationWorkspaceShell()
{
    if (!ui || !ui->mainLayout || !ui->tabWidget) {
        return;
    }

    setStyleSheet(QString());
    if (ui->tabWidget->tabBar()) {
        ui->tabWidget->tabBar()->hide();
    }

    auto* oldHeaderItem = ui->mainLayout->takeAt(0);
    auto* oldTabItem = ui->mainLayout->takeAt(0);
    Q_UNUSED(oldTabItem);
    delete oldHeaderItem;

    auto* headerFrame = new QFrame(this);
    headerFrame->setObjectName(QStringLiteral("navigationHeaderFrame"));
    auto* headerLayout = new QHBoxLayout(headerFrame);
    headerLayout->setContentsMargins(24, 20, 24, 20);
    headerLayout->setSpacing(18);

    ui->backButton->setParent(headerFrame);
    ui->backButton->setText(QStringLiteral("返回病例工作台"));
    headerLayout->addWidget(ui->backButton, 0, Qt::AlignTop);

    auto* titleContainer = new QWidget(headerFrame);
    auto* titleLayout = new QVBoxLayout(titleContainer);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(6);

    auto* eyebrowLabel = new QLabel(QStringLiteral("ANKLE NAVIGATION WORKSPACE"), titleContainer);
    eyebrowLabel->setObjectName(QStringLiteral("navigationEyebrowLabel"));
    titleLayout->addWidget(eyebrowLabel);

    ui->titleLabel->setParent(titleContainer);
    ui->titleLabel->setText(QStringLiteral("手术导航工作区"));
    titleLayout->addWidget(ui->titleLabel);

    auto* subtitleLabel = new QLabel(QStringLiteral("按准备、规划、配准、导航、评估推进当前病例。"), titleContainer);
    subtitleLabel->setObjectName(QStringLiteral("navigationSubtitleLabel"));
    subtitleLabel->setWordWrap(true);
    titleLayout->addWidget(subtitleLabel);

    headerLayout->addWidget(titleContainer, 1);

    ui->patientInfoLabel->setParent(headerFrame);
    ui->patientInfoLabel->setObjectName(QStringLiteral("patientInfoLabel"));
    headerLayout->addWidget(ui->patientInfoLabel, 0, Qt::AlignTop);

    auto* bodyFrame = new QFrame(this);
    bodyFrame->setObjectName(QStringLiteral("navigationBodyFrame"));
    auto* bodyLayout = new QHBoxLayout(bodyFrame);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(18);

    auto* workflowRail = new QFrame(bodyFrame);
    workflowRail->setObjectName(QStringLiteral("navigationWorkflowRailFrame"));
    workflowRail->setMinimumWidth(192);
    workflowRail->setMaximumWidth(224);
    auto* workflowLayout = new QVBoxLayout(workflowRail);
    workflowLayout->setContentsMargins(16, 18, 16, 18);
    workflowLayout->setSpacing(10);

    auto* workflowTitleLabel = new QLabel(QStringLiteral("导航流程"), workflowRail);
    workflowTitleLabel->setObjectName(QStringLiteral("navigationRailTitleLabel"));
    workflowLayout->addWidget(workflowTitleLabel);

    m_navigationStageSummaryLabel = new QLabel(QStringLiteral("当前阶段：准备"), workflowRail);
    m_navigationStageSummaryLabel->setObjectName(QStringLiteral("navigationRailSummaryLabel"));
    m_navigationStageSummaryLabel->setWordWrap(true);
    workflowLayout->addWidget(m_navigationStageSummaryLabel);
    workflowLayout->addSpacing(10);

    workflowLayout->addWidget(createWorkflowRailButton(QStringLiteral("准备"), AnkleWorkflowStage::Preparation));
    workflowLayout->addWidget(createWorkflowRailButton(QStringLiteral("规划"), AnkleWorkflowStage::Planning));
    workflowLayout->addWidget(createWorkflowRailButton(QStringLiteral("配准"), AnkleWorkflowStage::Registration));
    workflowLayout->addWidget(createWorkflowRailButton(QStringLiteral("导航"), AnkleWorkflowStage::Navigation));
    workflowLayout->addWidget(createWorkflowRailButton(QStringLiteral("评估"), AnkleWorkflowStage::Evaluation));
    workflowLayout->addStretch();

    auto* workspaceFrame = new QFrame(bodyFrame);
    workspaceFrame->setObjectName(QStringLiteral("navigationWorkspaceFrame"));
    auto* workspaceLayout = new QVBoxLayout(workspaceFrame);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    workspaceLayout->setSpacing(0);
    ui->tabWidget->setParent(workspaceFrame);
    workspaceLayout->addWidget(ui->tabWidget);

    auto* statusRail = new QFrame(bodyFrame);
    statusRail->setObjectName(QStringLiteral("navigationStatusRailFrame"));
    statusRail->setMinimumWidth(264);
    statusRail->setMaximumWidth(320);
    auto* statusLayout = new QVBoxLayout(statusRail);
    statusLayout->setContentsMargins(16, 18, 16, 18);
    statusLayout->setSpacing(12);

    auto* statusTitleLabel = new QLabel(QStringLiteral("运行状态"), statusRail);
    statusTitleLabel->setObjectName(QStringLiteral("navigationStatusTitleLabel"));
    statusLayout->addWidget(statusTitleLabel);

    m_navigationCaseStatusLabel = createNavigationStatusValueLabel(QStringLiteral("navigationCaseStatusLabel"), QStringLiteral("未选择病例"));
    statusLayout->addWidget(createNavigationStatusCard(QStringLiteral("当前病例"), m_navigationCaseStatusLabel));
    statusLayout->addWidget(createNavigationStatusCard(QStringLiteral("追踪器"), ui->trackerStatusLabel));
    statusLayout->addWidget(createNavigationStatusCard(QStringLiteral("配准误差"), ui->regErrorLabel));

    auto* readinessLabel = findChild<QLabel*>(QStringLiteral("navigationReadinessLabel"));
    if (!readinessLabel) {
        readinessLabel = createNavigationStatusValueLabel(QStringLiteral("navigationReadinessLabel"), QStringLiteral("导航准入：未就绪"));
    }
    statusLayout->addWidget(createNavigationStatusCard(QStringLiteral("导航准入"), readinessLabel));

    auto* confidenceLabel = findChild<QLabel*>(QStringLiteral("navigationConfidenceLabel"));
    if (!confidenceLabel) {
        confidenceLabel = createNavigationStatusValueLabel(QStringLiteral("navigationConfidenceLabel"), QStringLiteral("可信度评分：待评估"));
    }
    statusLayout->addWidget(createNavigationStatusCard(QStringLiteral("可信度"), confidenceLabel));

    auto* calibrationLabel = findChild<QLabel*>(QStringLiteral("calibrationStatusLabel"));
    if (!calibrationLabel) {
        calibrationLabel = createNavigationStatusValueLabel(QStringLiteral("calibrationStatusLabel"), QStringLiteral("标定状态：待开始"));
    }
    statusLayout->addWidget(createNavigationStatusCard(QStringLiteral("探针标定"), calibrationLabel));
    statusLayout->addWidget(createNavigationStatusCard(QStringLiteral("当前精度"), ui->accuracyValueLabel));
    statusLayout->addWidget(ui->accuracyBar);
    statusLayout->addStretch();

    bodyLayout->addWidget(workflowRail);
    bodyLayout->addWidget(workspaceFrame, 1);
    bodyLayout->addWidget(statusRail);

    ui->mainLayout->addWidget(headerFrame);
    ui->mainLayout->addWidget(bodyFrame, 1);
    ui->mainLayout->setContentsMargins(32, 26, 32, 26);
    ui->mainLayout->setSpacing(18);

    syncWorkflowRailState();
    syncNavigationStatusSummary();
}
```

- [ ] **Step 4: Build to see remaining compile errors**

Run:

```powershell
cmake --build build_x64_v142 --target NewPagesLib --config Release
```

Expected: may fail for missing method implementations from Task 4. If it fails for different reasons, fix the relevant declarations before continuing.

## Task 4: Sync Workflow and Status State

**Files:**
- Modify: `UI/NewPages/NavigationPage.cpp`

- [ ] **Step 1: Implement polish and tone helpers**

Add after `setupNavigationWorkspaceShell()`:

```cpp
void NavigationPageNew::polishNavigationWidget(QWidget* widget)
{
    if (!widget) {
        return;
    }
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

void NavigationPageNew::setStatusTone(QWidget* widget, const QString& tone)
{
    if (!widget) {
        return;
    }
    widget->setProperty("statusTone", tone);
    polishNavigationWidget(widget);
}
```

- [ ] **Step 2: Implement workflow rail sync**

Add:

```cpp
void NavigationPageNew::syncWorkflowRailState()
{
    if (!m_workflowContext) {
        return;
    }

    const AnkleWorkflowStage currentStage = m_workflowContext->currentStage();
    const QHash<AnkleWorkflowStage, QString> stageNames = {
        { AnkleWorkflowStage::Preparation, QStringLiteral("准备") },
        { AnkleWorkflowStage::Planning, QStringLiteral("规划") },
        { AnkleWorkflowStage::Registration, QStringLiteral("配准") },
        { AnkleWorkflowStage::Navigation, QStringLiteral("导航") },
        { AnkleWorkflowStage::Evaluation, QStringLiteral("评估") }
    };

    for (auto it = m_workflowRailButtons.begin(); it != m_workflowRailButtons.end(); ++it) {
        auto* button = it.value().data();
        if (!button) {
            continue;
        }
        const bool active = it.key() == currentStage;
        button->setChecked(active);
        button->setProperty("workflowState", active ? QStringLiteral("active") : QStringLiteral("idle"));
        polishNavigationWidget(button);
    }

    if (m_navigationStageSummaryLabel) {
        m_navigationStageSummaryLabel->setText(QStringLiteral("当前阶段：%1").arg(stageNames.value(currentStage)));
    }
}
```

- [ ] **Step 3: Implement status summary sync**

Add:

```cpp
void NavigationPageNew::syncNavigationStatusSummary()
{
    if (!m_navigationCaseStatusLabel || !m_workflowContext) {
        return;
    }

    const QString summary = m_workflowContext->patientSummary();
    m_navigationCaseStatusLabel->setText(summary.isEmpty() ? QStringLiteral("未选择病例") : summary);
    setStatusTone(m_navigationCaseStatusLabel, summary.isEmpty() ? QStringLiteral("warning") : QStringLiteral("ok"));
}
```

- [ ] **Step 4: Call sync hooks from existing methods**

In `setWorkflowStage()`, after the `switch` block, add:

```cpp
syncWorkflowRailState();
```

In `refreshPatientInfoLabel()`, after setting `ui->patientInfoLabel`, add:

```cpp
syncNavigationStatusSummary();
```

- [ ] **Step 5: Add statusTone updates to existing state methods**

In `updateTrackerStatus(bool connected)`, after text updates, add:

```cpp
setStatusTone(ui->trackerStatusLabel, connected ? QStringLiteral("ok") : QStringLiteral("danger"));
```

In `refreshNavigationConfidenceState(bool showWarnings)`, update the reset lambda:

```cpp
setStatusTone(navigationReadinessLabel, QStringLiteral("warning"));
setStatusTone(navigationConfidenceLabel, QStringLiteral("warning"));
```

In the allowed/blocked branch, after setting readiness text:

```cpp
setStatusTone(navigationReadinessLabel, m_lastConfidence.allowNavigation ? QStringLiteral("ok") : QStringLiteral("danger"));
setStatusTone(navigationConfidenceLabel, m_lastConfidence.allowNavigation ? QStringLiteral("ok") : QStringLiteral("warning"));
```

In `updateRegistrationResultDisplay(const PointRegistrationResult& result)`, after setting `regErrorLabel`, add:

```cpp
setStatusTone(
    ui->regErrorLabel,
    result.rmsError < 1.0 ? QStringLiteral("ok") : (result.rmsError < 2.0 ? QStringLiteral("warning") : QStringLiteral("danger")));
```

In the `!result.success` branch, add:

```cpp
setStatusTone(ui->regErrorLabel, QStringLiteral("warning"));
```

In `updateAccuracyDisplay(double accuracy)`, after progress update, add:

```cpp
setStatusTone(
    ui->accuracyValueLabel,
    accuracy < 1.0 ? QStringLiteral("ok") : (accuracy < 2.0 ? QStringLiteral("warning") : QStringLiteral("danger")));
```

- [ ] **Step 6: Build Navigation UI library**

Run:

```powershell
cmake --build build_x64_v142 --target NewPagesLib --config Release
```

Expected: build succeeds.

## Task 5: Extend Navigation Theme

**Files:**
- Modify: `UI/styles/three_pages_theme.qss`

- [ ] **Step 1: Add NavigationPage to shared page selectors**

At the top selector groups, add `QWidget#NavigationPage` alongside the existing page names:

```css
QWidget#WelcomePage,
QWidget#ModuleSelectionPage,
QWidget#ManagementPage,
QWidget#DashboardPage,
QWidget#SystemSettingsPage,
QWidget#NavigationPage {
    ...
}
```

Repeat for shared `QLabel`, title label, `QPushButton`, hover, pressed, and secondary back button selector groups.

- [ ] **Step 2: Add navigation-specific QSS**

Append this section near the Dashboard/SystemSettings page-specific styles:

```css
QWidget#NavigationPage QFrame#navigationHeaderFrame,
QWidget#NavigationPage QFrame#navigationWorkflowRailFrame,
QWidget#NavigationPage QFrame#navigationWorkspaceFrame,
QWidget#NavigationPage QFrame#navigationStatusRailFrame {
    background-color: rgba(8, 18, 30, 0.88);
    border: 1px solid rgba(124, 160, 191, 0.16);
    border-radius: 18px;
}

QWidget#NavigationPage QFrame#navigationHeaderFrame {
    background: qlineargradient(
        x1: 0, y1: 0, x2: 1, y2: 1,
        stop: 0 rgba(10, 25, 40, 0.96),
        stop: 1 rgba(8, 18, 30, 0.88)
    );
}

QWidget#NavigationPage QLabel#navigationEyebrowLabel,
QWidget#NavigationPage QLabel#navigationRailTitleLabel,
QWidget#NavigationPage QLabel#navigationStatusTitleLabel {
    color: ${ACCENT_SECONDARY};
    font-size: 13px;
    font-weight: 700;
    letter-spacing: 1px;
}

QWidget#NavigationPage QLabel#titleLabel {
    color: ${TEXT_PRIMARY};
    font-size: 34px;
    font-weight: 700;
}

QWidget#NavigationPage QLabel#navigationSubtitleLabel,
QWidget#NavigationPage QLabel#navigationRailSummaryLabel,
QWidget#NavigationPage QLabel#patientInfoLabel {
    color: ${TEXT_SECONDARY};
    font-size: 14px;
}

QWidget#NavigationPage QPushButton[workflowState="idle"] {
    min-height: 44px;
    padding: 0 14px;
    border-radius: 14px;
    background-color: rgba(8, 18, 30, 0.40);
    border: 1px solid rgba(124, 160, 191, 0.12);
    color: ${TEXT_SECONDARY};
    text-align: left;
}

QWidget#NavigationPage QPushButton[workflowState="active"] {
    min-height: 44px;
    padding: 0 14px;
    border-radius: 14px;
    background-color: rgba(91, 182, 255, 0.16);
    border: 1px solid rgba(91, 182, 255, 0.34);
    color: ${TEXT_PRIMARY};
    text-align: left;
}

QWidget#NavigationPage QFrame#navigationStatusCard {
    background-color: rgba(9, 19, 31, 0.70);
    border: 1px solid rgba(124, 160, 191, 0.10);
    border-radius: 14px;
}

QWidget#NavigationPage QLabel#navigationStatusCardTitleLabel {
    color: ${TEXT_MUTED};
    font-size: 12px;
    font-weight: 700;
}

QWidget#NavigationPage QLabel[statusTone="ok"] {
    color: ${SUCCESS};
}

QWidget#NavigationPage QLabel[statusTone="warning"] {
    color: ${WARNING};
}

QWidget#NavigationPage QLabel[statusTone="danger"] {
    color: ${DANGER};
}

QWidget#NavigationPage QTabWidget::pane {
    border: none;
    background-color: transparent;
}

QWidget#NavigationPage QGroupBox {
    color: ${TEXT_PRIMARY};
    font-size: 14px;
    font-weight: 700;
    border: 1px solid rgba(124, 160, 191, 0.12);
    border-radius: 14px;
    margin-top: 14px;
    padding-top: 12px;
    background-color: rgba(7, 16, 27, 0.42);
}

QWidget#NavigationPage QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    padding: 0 8px;
    color: ${ACCENT_SECONDARY};
}

QWidget#NavigationPage QTableWidget {
    background-color: transparent;
    border: none;
    gridline-color: rgba(255, 255, 255, 0.06);
    color: ${TEXT_PRIMARY};
    font-size: 14px;
}

QWidget#NavigationPage QHeaderView::section {
    background-color: rgba(9, 19, 31, 0.94);
    color: ${TEXT_MUTED};
    padding: 8px 10px;
    border: none;
    border-bottom: 1px solid rgba(124, 160, 191, 0.10);
    font-size: 13px;
    font-weight: 700;
}

QWidget#NavigationPage QProgressBar {
    min-height: 8px;
    max-height: 8px;
    background-color: rgba(255, 255, 255, 0.08);
    border: none;
    border-radius: 4px;
}

QWidget#NavigationPage QProgressBar::chunk {
    background-color: ${ACCENT_SECONDARY};
    border-radius: 4px;
}
```

- [ ] **Step 3: Run theme contract test**

Run:

```powershell
cmake --build build_x64_v142 --target ankle_navigation_workflow_contract_test --config Release
ctest --test-dir build_x64_v142 -C Release -R ankle_navigation_workflow_contract_test --output-on-failure
```

Expected: test passes after Tasks 3-5 are complete.

## Task 6: Final Verification

**Files:**
- No new edits unless verification exposes a problem.

- [ ] **Step 1: Run focused contract tests**

Run:

```powershell
ctest --test-dir build_x64_v142 -C Release -R "ankle_navigation_workflow_contract_test|three_page_presentation_utils_test" --output-on-failure
```

Expected: both tests pass.

- [ ] **Step 2: Build main app target**

Run:

```powershell
cmake --build build_x64_v142 --target medicalpro --config Release
```

Expected: build succeeds.

- [ ] **Step 3: Inspect diff**

Run:

```powershell
git diff -- UI/NewPages/NavigationPage.h UI/NewPages/NavigationPage.cpp UI/styles/three_pages_theme.qss tests/unit/AnkleNavigationWorkflowContractTest.cpp
```

Expected: diff only includes navigation workspace shell, theme, and contract test changes.

- [ ] **Step 4: Manual smoke check**

Run the built app from the Release output directory, enter Welcome -> ModuleSelection -> Management/Dashboard -> Navigation, and check:

- Navigation page shows top header, left workflow rail, central workspace, right status rail.
- Left rail switches 准备、规划、配准、导航、评估.
- Planning and navigation VTK areas still embed content.
- Tracker status, registration error, confidence, calibration, and accuracy labels still refresh.

## Self-Review

- Spec coverage: every design requirement maps to a task.
- Placeholder scan: no `TBD`, no `TODO`, no open-ended implementation steps.
- Type consistency: method names in tests, header, and implementation tasks match.
- Scope: no new dependency, no algorithm change, no plugin lifecycle change.
