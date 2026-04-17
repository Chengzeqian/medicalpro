# ManagementPage Phase 2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 `ManagementPage` 升级为四页主链中的正式数据管理中台，并同步修正 `Welcome -> ModuleSelection -> Management -> Dashboard` 的文案和主题收口。

**Architecture:** 保持 `Qt Widgets + .ui + NewPages` 架构不变，保留 `QTabWidget + 表格 + CRUD` 的底层行为，只重构 `ManagementPage.ui` 骨架、`ManagementPage.cpp` 展示层映射和 `three_pages_theme.qss` 的统一样式。同时把少量链路文案继续下沉到 `ThreePagePresentationUtils`，让 `ModuleSelectionPage` 与 `DashboardPage` 对四页链路的表述保持一致。

**Tech Stack:** Qt Widgets、Qt Designer `.ui`、Qt Style Sheet、Qt Test、CMake

---

## File Structure

- Modify: `tests/unit/ThreePagePresentationUtilsTest.cpp`
- Modify: `UI/NewPages/ThreePagePresentationUtils.h`
- Modify: `UI/NewPages/ThreePagePresentationUtils.cpp`
- Modify: `UI/Forms/ManagementPage.ui`
- Modify: `UI/NewPages/ManagementPage.h`
- Modify: `UI/NewPages/ManagementPage.cpp`
- Modify: `UI/NewPages/ModuleSelectionPage.cpp`
- Modify: `UI/NewPages/DashboardPage.cpp`
- Modify: `UI/styles/three_pages_theme.qss`
- Modify: `docs/current_status_and_project_overview.md`
- Modify: `docs/build_x64.md`
- Modify: `docs/superpowers/plans/2026-04-14-ui-phase2-three-pages-implementation.md`

## Task 1: 为四页链路补齐可测试展示层文案

**Files:**
- Modify: `tests/unit/ThreePagePresentationUtilsTest.cpp`
- Modify: `UI/NewPages/ThreePagePresentationUtils.h`
- Modify: `UI/NewPages/ThreePagePresentationUtils.cpp`

- [ ] **Step 1: 先写失败的展示层测试**

```cpp
void ThreePagePresentationUtilsTest::buildManagementOverviewValue_reportsCount()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildManagementOverviewValue(
            QStringLiteral("医生数据"),
            3),
        QStringLiteral("3 位医生"));
}

void ThreePagePresentationUtilsTest::buildManagementOverviewHint_reportsEntityMessage()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildManagementOverviewHint(
            QStringLiteral("患者数据"),
            0),
        QStringLiteral("当前可切换到患者视图继续查看、检索和维护病例基础资料。"));
}

void ThreePagePresentationUtilsTest::buildManagementEntryHint_reportsReady()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildManagementEntryHint(true),
        QStringLiteral("基础数据已确认，可进入病例工作台继续病例与影像流程。"));
}
```

- [ ] **Step 2: 运行测试确认先失败**

Run: `cmake --build build_x64 --config Release --target three_page_presentation_utils_test`

Expected: 编译失败，提示新增函数未声明或未定义

- [ ] **Step 3: 实现最小展示层函数**

```cpp
QString buildManagementOverviewValue(const QString& entityName, int count)
{
    if (entityName == QStringLiteral("医生数据")) {
        return QStringLiteral("%1 位医生").arg(count);
    }
    if (entityName == QStringLiteral("患者数据")) {
        return QStringLiteral("%1 位患者").arg(count);
    }
    return QStringLiteral("%1 台手术").arg(count);
}

QString buildManagementOverviewHint(const QString& entityName, int count)
{
    Q_UNUSED(count);
    if (entityName == QStringLiteral("医生数据")) {
        return QStringLiteral("当前可切换到医生视图继续查看、检索和维护术者资料。");
    }
    if (entityName == QStringLiteral("患者数据")) {
        return QStringLiteral("当前可切换到患者视图继续查看、检索和维护病例基础资料。");
    }
    return QStringLiteral("当前可切换到手术视图继续核对计划任务与流程状态。");
}

QString buildManagementEntryHint(bool readyToEnterDashboard)
{
    return readyToEnterDashboard
        ? QStringLiteral("基础数据已确认，可进入病例工作台继续病例与影像流程。")
        : QStringLiteral("建议先确认当前管理对象，再进入病例工作台。");
}
```

- [ ] **Step 4: 再跑测试确认通过**

Run: `ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure`

Expected: `three_page_presentation_utils_test` 通过

## Task 2: 重写 ManagementPage 页面骨架

**Files:**
- Modify: `UI/Forms/ManagementPage.ui`

- [ ] **Step 1: 重写头部区和概览卡区**

```xml
<widget class="QFrame" name="managementHeaderFrame">
 <layout class="QHBoxLayout" name="managementHeaderLayout">
  <item><widget class="QPushButton" name="backButton"><property name="text"><string>返回模块页</string></property></widget></item>
  <item>
   <layout class="QVBoxLayout" name="managementTitleLayout">
    <item><widget class="QLabel" name="managementEyebrowLabel"><property name="text"><string>DATA OPERATIONS HUB</string></property></widget></item>
    <item><widget class="QLabel" name="titleLabel"><property name="text"><string>数据管理中台</string></property></widget></item>
    <item><widget class="QLabel" name="subtitleLabel"><property name="text"><string>先完成基础数据查看与对象切换，再进入病例工作台继续当前流程。</string></property></widget></item>
   </layout>
  </item>
  <item><widget class="QPushButton" name="enterDashboardButton"><property name="text"><string>进入病例工作台</string></property></widget></item>
 </layout>
</widget>
```

```xml
<widget class="QFrame" name="overviewCardsFrame">
 <layout class="QHBoxLayout" name="overviewCardsLayout">
  <item><widget class="QFrame" name="doctorOverviewCard"/></item>
  <item><widget class="QFrame" name="patientOverviewCard"/></item>
  <item><widget class="QFrame" name="surgeryOverviewCard"/></item>
 </layout>
</widget>
```

- [ ] **Step 2: 保留 Tab 底层逻辑，但重组为“左说明 + 右工作区”**

```xml
<widget class="QWidget" name="doctorTab">
 <layout class="QHBoxLayout" name="doctorTabShellLayout">
  <item><widget class="QFrame" name="doctorSidePanelFrame"/></item>
  <item><widget class="QFrame" name="doctorWorkspaceFrame"/></item>
 </layout>
</widget>
```

- [ ] **Step 3: 新增底部流程收口区**

```xml
<widget class="QFrame" name="managementFlowFrame">
 <layout class="QHBoxLayout" name="managementFlowLayout">
  <item>
   <layout class="QVBoxLayout" name="managementFlowTextLayout">
    <item><widget class="QLabel" name="managementFlowTitleLabel"><property name="text"><string>下一步：进入病例工作台</string></property></widget></item>
    <item><widget class="QLabel" name="managementFlowHintLabel"><property name="text"><string>在数据管理中确认当前对象后，可进入病例工作台继续病例与影像流程。</string></property></widget></item>
   </layout>
  </item>
  <item><widget class="QPushButton" name="enterDashboardButtonSecondary"><property name="text"><string>继续进入工作台</string></property></widget></item>
 </layout>
</widget>
```

## Task 3: 实现 ManagementPage 运行时映射和概览刷新

**Files:**
- Modify: `UI/NewPages/ManagementPage.h`
- Modify: `UI/NewPages/ManagementPage.cpp`

- [ ] **Step 1: 扩展头文件，加入概览卡与 Tab 衔接方法**

```cpp
private:
    void setupTable();
    void setupPageCopy();
    void refreshOverviewCards();
    void refreshCurrentTabPresentation();
    void applyOverviewCard(QFrame* card, QLabel* valueLabel, QLabel* hintLabel, const QString& entityName, int count);
    int visibleRowCount(QTableWidget* table) const;
```

- [ ] **Step 2: 在 `onActivated()` 中先加载数据，再刷新展示层**

```cpp
void ManagementPageNew::onActivated()
{
    BasePage::onActivated();
    loadDoctors();
    loadPatients();
    loadSurgeries();
    refreshOverviewCards();
    refreshCurrentTabPresentation();
}
```

- [ ] **Step 3: 给双入口 CTA 接同一跳转行为**

```cpp
void ManagementPageNew::on_enterDashboardButton_clicked()
{
    emit enterMainSystemRequested();
    emit navigateTo(toInt(PageIndex::Dashboard));
}

void ManagementPageNew::on_enterDashboardButtonSecondary_clicked()
{
    on_enterDashboardButton_clicked();
}
```

- [ ] **Step 4: 用当前数据量刷新三张概览卡**

```cpp
void ManagementPageNew::refreshOverviewCards()
{
    applyOverviewCard(
        ui->doctorOverviewCard,
        ui->doctorOverviewValueLabel,
        ui->doctorOverviewHintLabel,
        QStringLiteral("医生数据"),
        visibleRowCount(ui->doctorTable));

    applyOverviewCard(
        ui->patientOverviewCard,
        ui->patientOverviewValueLabel,
        ui->patientOverviewHintLabel,
        QStringLiteral("患者数据"),
        visibleRowCount(ui->patientTable));

    applyOverviewCard(
        ui->surgeryOverviewCard,
        ui->surgeryOverviewValueLabel,
        ui->surgeryOverviewHintLabel,
        QStringLiteral("手术任务"),
        visibleRowCount(ui->surgeryTable));
}
```

- [ ] **Step 5: 切换 Tab 或搜索时同步左侧说明和底部收口文案**

```cpp
connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this](int) {
    refreshCurrentTabPresentation();
});
```

```cpp
void ManagementPageNew::refreshCurrentTabPresentation()
{
    const QString entityName = ui->tabWidget->tabText(ui->tabWidget->currentIndex());
    ui->currentEntityValueLabel->setText(entityName);
    ui->managementFlowHintLabel->setText(
        ThreePagePresentationUtils::buildManagementEntryHint(true));
}
```

## Task 4: 把四页链路样式与文案统一收口

**Files:**
- Modify: `UI/NewPages/ModuleSelectionPage.cpp`
- Modify: `UI/NewPages/DashboardPage.cpp`
- Modify: `UI/styles/three_pages_theme.qss`

- [ ] **Step 1: 调整 ModuleSelectionPage 文案，让目标页明确指向 Management**

```cpp
ui->subtitleLabel->setText(QStringLiteral("先确认当前系统状态，再进入数据管理中台或系统设置工作区。"));
ui->ankleSurgeryHintLabel->setText(QStringLiteral("从模块门厅进入数据管理主链，再继续病例工作台流程。"));
```

- [ ] **Step 2: 调整 DashboardPage 文案，让来源页明确回指 Management**

```cpp
ui->dashboardSubtitleLabel->setText(QStringLiteral("承接数据管理中台后的病例、影像与导航准备工作，减少跨页往返。"));
```

- [ ] **Step 3: 在公共 QSS 中追加 ManagementPage 样式**

```qss
QWidget#ManagementPage,
QWidget#ManagementPage QLabel {
    color: ${TEXT_PRIMARY};
}

QWidget#ManagementPage QFrame#managementHeaderFrame,
QWidget#ManagementPage QFrame#overviewCardsFrame,
QWidget#ManagementPage QFrame#managementFlowFrame,
QWidget#ManagementPage QFrame#doctorOverviewCard,
QWidget#ManagementPage QFrame#patientOverviewCard,
QWidget#ManagementPage QFrame#surgeryOverviewCard,
QWidget#ManagementPage QFrame#doctorSidePanelFrame,
QWidget#ManagementPage QFrame#doctorWorkspaceFrame,
QWidget#ManagementPage QFrame#patientSidePanelFrame,
QWidget#ManagementPage QFrame#patientWorkspaceFrame,
QWidget#ManagementPage QFrame#surgerySidePanelFrame,
QWidget#ManagementPage QFrame#surgeryWorkspaceFrame {
    background-color: rgba(8, 18, 30, 0.88);
    border: 1px solid rgba(124, 160, 191, 0.16);
    border-radius: 20px;
}
```

## Task 5: 回写文档并完成构建验证

**Files:**
- Modify: `docs/current_status_and_project_overview.md`
- Modify: `docs/build_x64.md`
- Modify: `docs/superpowers/plans/2026-04-14-ui-phase2-three-pages-implementation.md`

- [ ] **Step 1: 更新现状文档中的主链描述**

```md
- UI Phase 2 主链已修正为：
  - `Welcome -> ModuleSelection -> Management -> Dashboard`
```

- [ ] **Step 2: 更新构建说明中的 UI Phase 2 状态**

```md
- `ManagementPage` 数据管理中台化改造已进入主构建链并编译通过。
```

- [ ] **Step 3: 运行构建与测试**

Run: `cmake --build build_x64 --config Release --target medicalpro`

Expected: `medicalpro` 构建成功

Run: `ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure`

Expected: `three_page_presentation_utils_test` 通过

- [ ] **Step 4: 记录本轮交付结论**

```md
- `ManagementPage` 已纳入 UI Phase 2 正式范围，四页链路文案、主题和视觉层级已统一。
```

## 2026-04-15 后续推进记录

- 在 `ManagementPage` 中台化改造完成后，已继续进行一轮四页联合视觉微调，`ManagementPage` 是其中一环。
- 本轮继续收口的重点为：
  - Header 副标题与中台语气
  - 当前工作视图上下文条的信息密度
  - Tab / Table 的节奏与行高
  - `继续进入病例工作台` 的底部 CTA 收口感
- 联动更新还覆盖：
  - `ModuleSelectionPage` 主入口文案与按钮分级
  - `DashboardPage` 导航 CTA 语气与病例工作台节奏
  - `UI/styles/three_pages_theme.qss` 的四页公共样式权重
- 2026-04-15 已再次完成：
  - `cmake --build build_x64 --config Release --target medicalpro`
  - `ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure`
  - `build_x64/Release/medicalpro.exe` 8 秒稳定启动检查

## 2026-04-16 联动微调记录

- 在四页联合视觉微调第二轮中，ManagementPage 继续按“中台壳层减厚、表格主内容增强”的方向收口。
- 本轮继续压缩：
  - `managementHeaderFrame`
  - 三张概览卡
  - `entityContextFrame`
  - `managementFlowFrame`
  - Tab、搜索栏、表格表头与表格行高
- 对应改动已落在：
  - `UI/Forms/ManagementPage.ui`
  - `UI/NewPages/ManagementPage.cpp`
  - `UI/styles/three_pages_theme.qss`
- 2026-04-16 已再次完成：
  - `cmake --build build_x64 --config Release --target medicalpro`
  - `ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure`
  - `build_x64/Release/medicalpro.exe` 8 秒稳定启动检查
