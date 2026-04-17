# SystemSettingsPage Phase 2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在保持 `SystemSettingsPage` 现有功能、跳转关系、状态来源和 `QSettings` 键不变的前提下，完成基于最新 Phase 2 spec 的“首屏强化”微调，让页头、状态总览和当前建议真正成为第一视觉层，并把配置区收口成更稳的辅助编辑区。

**Architecture:** 不再重做 `SystemSettingsPage` 的页面结构，而是基于当前已经落地的 `UI/Forms/SystemSettingsPage.ui`、`UI/NewPages/SystemSettingsPage.cpp` 和 `UI/styles/three_pages_theme.qss` 做二次收口。`.ui` 负责重新分配首屏空间、压缩配置区留白；`QSS` 负责建立“首屏强化 + 配置区降级”的层级；`SystemSettingsPage.cpp` 只做少量文案与状态标签语义微调，不引入新逻辑链。

**Tech Stack:** Qt Widgets、Qt Designer `.ui`、QSS、CMake、CTest、手工运行验收

---

## Files and Responsibilities

- Modify: `UI/Forms/SystemSettingsPage.ui`
  - 调整 `Header`、三张状态卡、建议条和三张配置卡的几何关系、留白、卡片高度与表单密度。
- Modify: `UI/styles/three_pages_theme.qss`
  - 重新分配 `SystemSettingsPage` 内部的视觉层级，让首屏更强、配置区更稳、路径按钮更弱。
- Modify: `UI/NewPages/SystemSettingsPage.cpp`
  - 仅微调运行期文案与 badge 语义，让建议条更像“当前判断结果”。
- Modify: `docs/current_status_and_project_overview.md`
  - 在实现完成后记录 `SystemSettingsPage` 本轮视觉微调结果。
- Modify: `docs/build_x64.md`
  - 在实现完成后记录本轮人工验收重点和运行路径。
- Modify: `docs/superpowers/plans/2026-04-16-system-settings-page-phase2-implementation.md`
  - 在实现完成后于文档尾部补实际修改文件和验证记录。

---

### Task 1: 抬高页头与状态总览的首屏权重

**Files:**
- Modify: `UI/Forms/SystemSettingsPage.ui`

- [ ] **Step 1: 先把页面外层间距改成更适合首屏聚焦的节奏**

把根布局和滚动区的基础间距收口为下面这组值，避免首屏被零散小留白切碎：

```xml
<!-- UI/Forms/SystemSettingsPage.ui -->
<layout class="QVBoxLayout" name="mainLayout">
 <property name="spacing">
  <number>24</number>
 </property>
 <property name="leftMargin">
  <number>40</number>
 </property>
 <property name="topMargin">
  <number>32</number>
 </property>
 <property name="rightMargin">
  <number>40</number>
 </property>
 <property name="bottomMargin">
  <number>20</number>
 </property>
</layout>
```

```xml
<layout class="QVBoxLayout" name="scrollLayout">
 <property name="spacing">
  <number>20</number>
 </property>
 <property name="bottomMargin">
  <number>4</number>
 </property>
</layout>
```

- [ ] **Step 2: 把 Header 卡片做厚，提升标题区身份感**

把 `systemSettingsHeaderFrame` 从薄工具栏改成完整页头卡，直接调整最小高度、内边距和按钮尺寸：

```xml
<widget class="QFrame" name="systemSettingsHeaderFrame">
 <property name="minimumSize">
  <size>
   <width>0</width>
   <height>148</height>
  </size>
 </property>
 <property name="frameShape">
  <enum>QFrame::StyledPanel</enum>
 </property>
 <layout class="QHBoxLayout" name="headerLayout">
  <property name="spacing">
   <number>22</number>
  </property>
  <property name="leftMargin">
   <number>28</number>
  </property>
  <property name="topMargin">
   <number>24</number>
  </property>
  <property name="rightMargin">
   <number>28</number>
  </property>
  <property name="bottomMargin">
   <number>24</number>
  </property>
 </layout>
</widget>
```

```xml
<widget class="QPushButton" name="backButton">
 <property name="minimumSize">
  <size>
   <width>156</width>
   <height>46</height>
  </size>
 </property>
</widget>

<widget class="QPushButton" name="saveButton">
 <property name="minimumSize">
  <size>
   <width>176</width>
   <height>52</height>
  </size>
 </property>
</widget>
```

```xml
<layout class="QVBoxLayout" name="headerTitleLayout">
 <property name="spacing">
  <number>8</number>
 </property>
</layout>
```

- [ ] **Step 3: 把三张状态卡从“扁信息块”改成更完整的首屏状态摘要卡**

三张卡继续并排，但统一抬高卡片高度、内边距和卡内节奏。先改 `frameworkStatusCard`，然后把同样的数值同步到 `serviceStatusCard` 和 `pathStatusCard`：

```xml
<widget class="QFrame" name="frameworkStatusCard">
 <property name="minimumSize">
  <size>
   <width>0</width>
   <height>228</height>
  </size>
 </property>
 <layout class="QVBoxLayout" name="frameworkStatusCardLayout">
  <property name="spacing">
   <number>12</number>
  </property>
  <property name="leftMargin">
   <number>24</number>
  </property>
  <property name="topMargin">
   <number>22</number>
  </property>
  <property name="rightMargin">
   <number>24</number>
  </property>
  <property name="bottomMargin">
   <number>22</number>
  </property>
 </layout>
</widget>
```

同时把状态总览容器本身也留出更稳定的横向呼吸感：

```xml
<layout class="QHBoxLayout" name="systemHealthOverviewLayout">
 <property name="spacing">
  <number>18</number>
 </property>
</layout>
```

- [ ] **Step 4: 把建议条从长横条提示改成更像“当前判断卡”的尺寸**

给 `systemRecommendationFrame` 和 `systemRecommendationBadge` 更明确的高度与边距，让它在三张状态卡下方形成完整结论区：

```xml
<widget class="QFrame" name="systemRecommendationFrame">
 <property name="minimumSize">
  <size>
   <width>0</width>
   <height>92</height>
  </size>
 </property>
 <layout class="QHBoxLayout" name="systemRecommendationLayout">
  <property name="spacing">
   <number>16</number>
  </property>
  <property name="leftMargin">
   <number>24</number>
  </property>
  <property name="topMargin">
   <number>20</number>
  </property>
  <property name="rightMargin">
   <number>24</number>
  </property>
  <property name="bottomMargin">
   <number>20</number>
  </property>
 </layout>
</widget>
```

```xml
<widget class="QLabel" name="systemRecommendationBadge">
 <property name="minimumSize">
  <size>
   <width>112</width>
   <height>34</height>
  </size>
 </property>
 <property name="alignment">
  <set>Qt::AlignCenter</set>
 </property>
</widget>
```

- [ ] **Step 5: 先编译 `NewPagesLib`，确认 `.ui` 调整没有破坏 UIC 生成**

Run:

```powershell
cmake --build build_x64 --config Release --target NewPagesLib
```

Expected:
- `ui_SystemSettingsPage.h` 正常重新生成
- 没有对象名缺失、XML 闭合错误或布局树损坏

---

### Task 2: 收紧配置区密度并消掉页面底部的空场

**Files:**
- Modify: `UI/Forms/SystemSettingsPage.ui`

- [ ] **Step 1: 把三张配置卡的头部虚空感收紧**

统一缩小配置卡标题区与表单区之间的空隙。先改 `generalSettingsCard`，再同步到 `deviceSettingsCard` 和 `pathSettingsCard`：

```xml
<widget class="QFrame" name="generalSettingsCard">
 <layout class="QVBoxLayout" name="generalSettingsCardLayout">
  <property name="spacing">
   <number>10</number>
  </property>
  <property name="leftMargin">
   <number>24</number>
  </property>
  <property name="topMargin">
   <number>20</number>
  </property>
  <property name="rightMargin">
   <number>24</number>
  </property>
  <property name="bottomMargin">
   <number>20</number>
  </property>
 </layout>
</widget>
```

- [ ] **Step 2: 把表单的纵向节奏调成“更稳但不松垮”**

把三张配置卡内的 `QFormLayout` 统一到同一套 spacing，让表单区更整齐：

```xml
<layout class="QFormLayout" name="generalFormLayout">
 <property name="horizontalSpacing">
  <number>16</number>
 </property>
 <property name="verticalSpacing">
  <number>14</number>
 </property>
</layout>
```

```xml
<layout class="QFormLayout" name="deviceFormLayout">
 <property name="horizontalSpacing">
  <number>16</number>
 </property>
 <property name="verticalSpacing">
  <number>14</number>
 </property>
</layout>
```

```xml
<layout class="QFormLayout" name="pathFormLayout">
 <property name="horizontalSpacing">
  <number>16</number>
 </property>
 <property name="verticalSpacing">
  <number>14</number>
 </property>
</layout>
```

- [ ] **Step 3: 把路径区两行按钮和输入框重新对齐**

压缩路径行内部空隙，并把两个 `浏览` 按钮宽度固定一致：

```xml
<layout class="QHBoxLayout" name="dataPathLayout">
 <property name="spacing">
  <number>10</number>
 </property>
</layout>

<layout class="QHBoxLayout" name="dicomPathLayout">
 <property name="spacing">
  <number>10</number>
 </property>
</layout>
```

```xml
<widget class="QPushButton" name="browseDataPathButton">
 <property name="minimumSize">
  <size>
   <width>104</width>
   <height>42</height>
  </size>
 </property>
</widget>

<widget class="QPushButton" name="browseDicomPathButton">
 <property name="minimumSize">
  <size>
   <width>104</width>
   <height>42</height>
  </size>
 </property>
</widget>
```

- [ ] **Step 4: 直接收掉底部大留白**

删除 `bottomSpacer`，不要再靠一个垂直弹簧把页面尾部撑空。如果不方便直接删，就至少把它改成零高度：

```xml
<spacer name="bottomSpacer">
 <property name="orientation">
  <enum>Qt::Vertical</enum>
 </property>
 <property name="sizeHint" stdset="0">
  <size>
   <width>20</width>
   <height>0</height>
  </size>
 </property>
</spacer>
```

优先方案：直接从 `scrollLayout` 中移除这个 `spacer` 节点。

- [ ] **Step 5: 再编译一次 `NewPagesLib`，确认第二轮 `.ui` 微调仍可生成**

Run:

```powershell
cmake --build build_x64 --config Release --target NewPagesLib
```

Expected:
- `SystemSettingsPage.ui` 调整后的布局树仍可正常编译
- 没有新的对象名冲突或布局缺失问题

---

### Task 3: 用 QSS 建立“首屏强化 + 配置区降级”的视觉层级

**Files:**
- Modify: `UI/styles/three_pages_theme.qss`

- [ ] **Step 1: 让页头卡片比配置卡更像正式的系统检查台 Header**

把 `systemSettingsHeaderFrame` 从普通深色卡调成更有身份感的页头卡：

```qss
QWidget#SystemSettingsPage QFrame#systemSettingsHeaderFrame {
    background: qlineargradient(
        x1: 0, y1: 0, x2: 1, y2: 1,
        stop: 0 rgba(10, 25, 40, 0.96),
        stop: 1 rgba(8, 18, 30, 0.88)
    );
    border: 1px solid rgba(124, 160, 191, 0.18);
    border-radius: 22px;
}

QWidget#SystemSettingsPage QLabel#systemSettingsEyebrowLabel {
    color: ${ACCENT_SECONDARY};
    font-size: 14px;
    font-weight: 700;
    letter-spacing: 1px;
}

QWidget#SystemSettingsPage QLabel#titleLabel {
    color: ${TEXT_PRIMARY};
    font-size: 34px;
    font-weight: 700;
}

QWidget#SystemSettingsPage QLabel#subtitleLabel {
    color: ${TEXT_SECONDARY};
    font-size: 15px;
}
```

- [ ] **Step 2: 把三张状态卡做成真正的首屏主视觉**

状态卡要比下方配置卡更亮、更厚、更有结论感：

```qss
QWidget#SystemSettingsPage QFrame#frameworkStatusCard,
QWidget#SystemSettingsPage QFrame#serviceStatusCard,
QWidget#SystemSettingsPage QFrame#pathStatusCard {
    background-color: rgba(10, 22, 35, 0.92);
    border: 1px solid rgba(124, 160, 191, 0.18);
    border-radius: 20px;
}

QWidget#SystemSettingsPage QLabel#frameworkStatusTitleLabel,
QWidget#SystemSettingsPage QLabel#serviceStatusTitleLabel,
QWidget#SystemSettingsPage QLabel#pathStatusTitleLabel {
    color: ${TEXT_MUTED};
    font-size: 12px;
    font-weight: 700;
    letter-spacing: 1px;
}

QWidget#SystemSettingsPage QLabel#frameworkStatusBadgeLabel,
QWidget#SystemSettingsPage QLabel#serviceStatusBadgeLabel,
QWidget#SystemSettingsPage QLabel#pathStatusBadgeLabel {
    min-height: 30px;
    max-height: 30px;
    padding: 0 12px;
    border-radius: 15px;
    font-size: 13px;
    font-weight: 700;
}

QWidget#SystemSettingsPage QLabel#frameworkStatusSummaryLabel,
QWidget#SystemSettingsPage QLabel#serviceStatusSummaryLabel,
QWidget#SystemSettingsPage QLabel#pathStatusSummaryLabel {
    color: ${TEXT_PRIMARY};
    font-size: 22px;
    font-weight: 700;
}

QWidget#SystemSettingsPage QLabel#frameworkStatusDetailLabel,
QWidget#SystemSettingsPage QLabel#serviceStatusDetailLabel,
QWidget#SystemSettingsPage QLabel#pathStatusDetailLabel {
    color: ${TEXT_SECONDARY};
    font-size: 13px;
    line-height: 1.4em;
}
```

- [ ] **Step 3: 把建议条收成“判断结果卡”，不要再像提示横条**

让 `systemRecommendationFrame` 获得更明确的实体感，并且随着 `statusTone` 变化改变边框与背景：

```qss
QWidget#SystemSettingsPage QFrame#systemRecommendationFrame {
    background-color: rgba(10, 22, 35, 0.82);
    border: 1px solid rgba(124, 160, 191, 0.16);
    border-radius: 18px;
}

QWidget#SystemSettingsPage QFrame#systemRecommendationFrame[statusTone="ok"] {
    background-color: rgba(10, 28, 24, 0.88);
    border-color: ${STATUS_OK_BORDER};
}

QWidget#SystemSettingsPage QFrame#systemRecommendationFrame[statusTone="warning"] {
    background-color: rgba(36, 29, 15, 0.88);
    border-color: ${STATUS_WARNING_BORDER};
}

QWidget#SystemSettingsPage QFrame#systemRecommendationFrame[statusTone="danger"] {
    background-color: rgba(39, 18, 22, 0.88);
    border-color: ${STATUS_DANGER_BORDER};
}

QWidget#SystemSettingsPage QLabel#systemRecommendationBadge {
    min-width: 112px;
    max-width: 112px;
    min-height: 34px;
    padding: 0 12px;
    border-radius: 17px;
    font-size: 13px;
    font-weight: 700;
}

QWidget#SystemSettingsPage QLabel#systemRecommendationLabel {
    color: ${TEXT_PRIMARY};
    font-size: 16px;
    font-weight: 600;
}
```

- [ ] **Step 4: 明确把配置卡降级成第二视觉层**

配置区保留稳定性，但不要再和首屏争注意力：

```qss
QWidget#SystemSettingsPage QFrame#generalSettingsCard,
QWidget#SystemSettingsPage QFrame#deviceSettingsCard,
QWidget#SystemSettingsPage QFrame#pathSettingsCard {
    background-color: rgba(8, 18, 30, 0.74);
    border: 1px solid rgba(124, 160, 191, 0.10);
    border-radius: 18px;
}

QWidget#SystemSettingsPage QLabel#generalSettingsTitleLabel,
QWidget#SystemSettingsPage QLabel#deviceSettingsTitleLabel,
QWidget#SystemSettingsPage QLabel#pathSettingsTitleLabel {
    color: ${TEXT_PRIMARY};
    font-size: 19px;
    font-weight: 700;
}

QWidget#SystemSettingsPage QLabel#generalSettingsDescLabel,
QWidget#SystemSettingsPage QLabel#deviceSettingsDescLabel,
QWidget#SystemSettingsPage QLabel#pathSettingsDescLabel {
    color: ${TEXT_SECONDARY};
    font-size: 13px;
}
```

同时统一控件稳定感，但不再过分强调路径按钮：

```qss
QWidget#SystemSettingsPage QLineEdit,
QWidget#SystemSettingsPage QComboBox {
    min-height: 46px;
    padding: 0 14px;
    border-radius: 12px;
    background-color: rgba(7, 16, 27, 0.82);
    border: 1px solid ${BORDER_SOFT};
    color: ${TEXT_PRIMARY};
}

QWidget#SystemSettingsPage QPushButton#browseDataPathButton,
QWidget#SystemSettingsPage QPushButton#browseDicomPathButton {
    min-height: 42px;
    min-width: 104px;
    padding: 0 14px;
    border-radius: 12px;
    background-color: rgba(8, 18, 30, 0.30);
    border: 1px solid rgba(124, 160, 191, 0.14);
    color: ${TEXT_SECONDARY};
}
```

- [ ] **Step 5: 编译主程序，确认 QSS 修改未破坏当前主题加载链**

Run:

```powershell
cmake --build build_x64 --config Release --target medicalpro
```

Expected:
- `medicalpro` 构建成功
- `three_pages_theme.qss` 被继续正常加载，没有因为选择器拼写错误导致构建侧失败

---

### Task 4: 微调运行文案，让建议条更像“当前判断结果”

**Files:**
- Modify: `UI/NewPages/SystemSettingsPage.cpp`

- [ ] **Step 1: 只调整建议条 badge 文案，不改 tone 判断逻辑**

把 `refreshRecommendation()` 中 badge 文案收成判断语气：

```cpp
void SystemSettingsPageNew::refreshRecommendation(const RuntimeStatusSnapshot& snapshot)
{
    const QString tone = ThreePagePresentationUtils::buildSystemSettingsRecommendationTone(
        snapshot.frameworkReady,
        snapshot.readyServices,
        snapshot.totalServices,
        snapshot.dataDirectoryReadable,
        snapshot.dicomDirectoryReadable);

    ui->systemRecommendationFrame->setProperty("statusTone", tone);
    ui->systemRecommendationBadge->setText(
        tone == QStringLiteral("ok")
            ? QStringLiteral("当前判断")
            : (tone == QStringLiteral("warning") ? QStringLiteral("建议先检查") : QStringLiteral("优先排查")));
    ui->systemRecommendationBadge->setProperty("statusTone", tone);
    ui->systemRecommendationLabel->setText(
        ThreePagePresentationUtils::buildSystemSettingsRecommendation(
            snapshot.frameworkReady,
            snapshot.readyServices,
            snapshot.totalServices,
            snapshot.dataDirectoryReadable,
            snapshot.dicomDirectoryReadable));

    polishWidget(ui->systemRecommendationFrame);
    polishWidget(ui->systemRecommendationBadge);
    polishWidget(ui->systemRecommendationLabel);
}
```

- [ ] **Step 2: 把三张状态卡 detail 文案压成更短、更好扫读的一行语气**

只改 detail 文案，不改状态来源：

```cpp
applyStatusCard(
    ui->frameworkStatusBadgeLabel,
    ui->frameworkStatusSummaryLabel,
    ui->frameworkStatusDetailLabel,
    frameworkTone,
    snapshot.frameworkReady ? QStringLiteral("已联通") : QStringLiteral("待检查"),
    ThreePagePresentationUtils::buildFrameworkSummary(snapshot.frameworkReady, snapshot.pluginCount),
    snapshot.frameworkReady
        ? QStringLiteral("插件框架已完成初始化，可继续检查服务与路径。")
        : QStringLiteral("请先确认 CTK 初始化和插件加载状态。"));
```

```cpp
applyStatusCard(
    ui->serviceStatusBadgeLabel,
    ui->serviceStatusSummaryLabel,
    ui->serviceStatusDetailLabel,
    serviceTone,
    serviceTone == QStringLiteral("ok")
        ? QStringLiteral("正常")
        : (serviceTone == QStringLiteral("warning") ? QStringLiteral("待确认") : QStringLiteral("异常")),
    ThreePagePresentationUtils::buildServiceGroupSummary(snapshot.readyServices, snapshot.totalServices),
    snapshot.readyServices == snapshot.totalServices
        ? QStringLiteral("关键服务当前均已可用。")
        : QStringLiteral("关键服务未完全就绪，建议先回模块页复核。"));
```

```cpp
applyStatusCard(
    ui->pathStatusBadgeLabel,
    ui->pathStatusSummaryLabel,
    ui->pathStatusDetailLabel,
    pathTone,
    pathTone == QStringLiteral("ok")
        ? QStringLiteral("可访问")
        : (pathTone == QStringLiteral("warning") ? QStringLiteral("需复核") : QStringLiteral("不可用")),
    ThreePagePresentationUtils::buildSystemSettingsPathSummary(
        snapshot.dataDirectoryReadable,
        snapshot.dicomDirectoryReadable),
    QStringLiteral("data：%1 | DICOM：%2")
        .arg(QDir::toNativeSeparators(ui->dataPathEdit->text()), QDir::toNativeSeparators(ui->dicomPathEdit->text())));
```

- [ ] **Step 3: 重新编译主程序，确认本轮只改文案没有引入行为回归**

Run:

```powershell
cmake --build build_x64 --config Release --target medicalpro
```

Expected:
- `SystemSettingsPage.cpp` 编译通过
- 没有因为文案调整或 `QDir::toNativeSeparators` 调用方式导致编译错误

---

### Task 5: 文档同步与真实运行验收

**Files:**
- Modify: `docs/current_status_and_project_overview.md`
- Modify: `docs/build_x64.md`
- Modify: `docs/superpowers/plans/2026-04-16-system-settings-page-phase2-implementation.md`

- [ ] **Step 1: 在项目现状文档里追加本轮落地记录**

在 `docs/current_status_and_project_overview.md` 追加类似条目：

```md
### 1.x 2026-04-16 SystemSettingsPage Phase 2 首屏强化微调
- `SystemSettingsPage` 已在首轮结构落地基础上继续完成首屏强化微调。
- 当前页头、状态总览与建议条已成为第一视觉层，配置区回落为稳定的辅助编辑区。
- 本轮未改页面跳转、设置项、`QSettings` key 和状态来源逻辑，只做结构密度与视觉层级收口。
```

- [ ] **Step 2: 在构建说明里补本轮人工验收重点**

在 `docs/build_x64.md` 追加：

```md
## 10. 2026-04-16 SystemSettingsPage 首屏强化验收
- 验收路径：`ModuleSelectionPage -> SystemSettingsPage -> 返回模块页`
- 首屏重点：Header、三张状态卡、当前判断卡
- 配置区重点：表单密度、路径区按钮统一、底部留白是否收干净
```

- [ ] **Step 3: 跑构建和既有回归测试**

Run:

```powershell
cmake --build build_x64 --config Release --target medicalpro
ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure
```

Expected:
- `medicalpro` Release 构建通过
- `three_page_presentation_utils_test` 继续通过，说明本轮视觉微调没有误伤既有展示层逻辑

- [ ] **Step 4: 真实启动并做页面级人工验收**

Run:

```powershell
$exe = Resolve-Path 'build_x64/Release/medicalpro.exe'
$p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru
Start-Sleep -Seconds 8
```

Expected:
- 程序在 8 秒观察窗口内持续运行
- 进入 `SystemSettingsPage` 后，第一眼先看到更完整的 Header 和三张更厚的状态卡
- 建议条看起来像“当前判断结果”，而不是单纯提示横条
- 下半区三张配置卡更紧凑、更规整，不再和首屏争第一焦点
- 页面底部不再出现明显大留白

- [ ] **Step 5: 在本计划文件尾部补实施结果记录**

实现完成后，在本文件末尾追加：

- `Implementation Result Record`
- `Actual Modified Files`
- `Build Commands`
- `Test Commands`
- `Runtime Verification`
- `Verification Notes`

---

## Spec Coverage Check

- `13.1 首屏结构重心`：对应 Task 1，先抬页头、状态卡和建议条，再把配置区留在第二视觉层。
- `13.2 Header 微调`：对应 Task 1 Step 2 和 Task 3 Step 1。
- `13.3 状态总览卡微调`：对应 Task 1 Step 3 和 Task 3 Step 2。
- `13.4 当前建议条微调`：对应 Task 1 Step 4、Task 3 Step 3、Task 4 Step 1。
- `13.5 配置区层级收口`：对应 Task 2 Step 1 和 Task 3 Step 4。
- `13.6 表单密度与路径区收口`：对应 Task 2 Step 2、Step 3、Step 4。
- `14 实施边界`：本计划不包含插件链、服务链、跳转链和 `QSettings` key 修改。
- `15 验收标准`：集中对应 Task 5 Step 3 和 Step 4。

## Placeholder Scan

- 本计划不使用 `TODO`、`TBD`、`后续补` 等占位词。
- 所有任务都给出具体文件、对象名、代码片段和验证命令。
- 本轮计划不包含 git 提交步骤，按用户当前“先不用管 git”的要求执行。

## Implementation Result Record

### Actual Modified Files
- UI/Forms/SystemSettingsPage.ui
- UI/styles/three_pages_theme.qss
- UI/NewPages/SystemSettingsPage.cpp
- docs/current_status_and_project_overview.md
- docs/build_x64.md
- docs/superpowers/plans/2026-04-16-system-settings-page-phase2-implementation.md

### Build Commands
```powershell
cmake --build build_x64 --config Release --target NewPagesLib
cmake --build build_x64 --config Release --target medicalpro
```

### Test Commands
```powershell
ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure
```

### Runtime Verification
```powershell
$exe = Resolve-Path 'build_x64/Release/medicalpro.exe'
$p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru
Start-Sleep -Seconds 8
```

### Verification Notes
- Task 1 与 Task 2 的 `.ui` 微调均已分别通过 `NewPagesLib` 构建验证。
- Task 3 与 Task 4 完成后，`medicalpro` Release 构建连续通过。
- `three_page_presentation_utils_test` 本轮通过 `ctest` 直接执行验证，没有再出现测试路径注册问题。
- `medicalpro.exe` 在 8 秒观察窗口内持续运行，本轮改动未引入启动即退出回归。

## 2026-04-16 Bottom Whitespace Follow-up

### Root Cause
- `SystemSettingsPage` 底部留白未完全消失，并不是新的 `bottomSpacer` 回归。
- 实际根因在 `UI/Forms/SystemSettingsPage.ui` 的 `scrollAreaContents`：滚动内容容器仍保留了固定初始高度 `1400`。
- 页面本体高度只有 `860`，而滚动内容真实布局高度明显小于 `1400`，因此最后一张配置卡之后会继续被硬撑出一段空白区。

### Fix Applied
- 将 `scrollAreaContents` 的初始几何高度从 `1400` 收紧到更接近实际内容高度的 `960`。
- 为 `scrollLayout` 增加 `QLayout::SetMinAndMaxSize`，让滚动内容容器优先跟随真实布局高度，而不是长期受 Designer 初始几何值影响。
- 将 `scrollLayout` 的 `bottomMargin` 从 `4` 清到 `0`，去掉尾部残余边距。

### Verification
```powershell
cmake --build build_x64 --config Release --target NewPagesLib
cmake --build build_x64 --config Release --target medicalpro
ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure
$exe = Resolve-Path 'build_x64/Release/medicalpro.exe'
$p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru
Start-Sleep -Seconds 8
```

### Notes
- 本轮构建时出现过一次 `SystemSettingsPage.obj: Permission denied`，已确认是并行执行两个构建命令导致同一中间产物竞争，不是代码错误。
- 改为串行构建后，`medicalpro` Release 构建恢复通过。
- 启动验证结果为 `HasExited=False`，说明这次留白修复没有引入新的启动回归。

## 2026-04-16 Runtime Screenshot Follow-up

### Observed Issues
- 用户提供的真实运行截图显示，当前页的主要问题已不再是底部留白，而是滚动区域露出浅色底、建议条左侧 badge 被拉成高方块、配置卡块感偏重。
- 这三个问题叠加后，会让 `SystemSettingsPage` 看起来像“卡片摆在白板上”，和当前四页深色产品化主题不一致。

### Root Cause
- 白色分隔感的根因在于 `scrollArea` 的 viewport 没有跟页面主题统一，卡片之间的间距直接露出了默认浅色背景。
- 建议条左侧 `systemRecommendationBadge` 只有最小高度，没有最大高度或垂直固定策略，放进水平布局后被整条建议框拉伸。
- 下半区三张配置卡虽然结构没问题，但背景实色和边框权重偏重，进一步放大了“整块灰板”的观感。

### Fix Applied
- 在 `SystemSettingsPage.cpp` 给 `scrollArea` viewport 指定对象名 `systemSettingsScrollViewport`，并关闭自动填充背景。
- 在 `three_pages_theme.qss` 里为 `scrollArea`、viewport、`scrollAreaContents` 和状态总览容器统一设置透明背景，去掉白色分隔底。
- 为 `systemRecommendationBadge` 增加固定尺寸约束，并把建议条高度与上下内边距收紧，恢复为短胶囊 + 文案条的比例。
- 将三张配置卡背景透明度和边框存在感继续下调，让它们更像辅助编辑区而不是首屏主舞台。

### Verification
```powershell
cmake --build build_x64 --config Release --target medicalpro
ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure
$exe = Resolve-Path 'build_x64/Release/medicalpro.exe'
$p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru
Start-Sleep -Seconds 8
```

### Notes
- 本轮再次出现过一次并行构建引发的 `*.tlog` 占用报错，确认仍是并行构建竞争，不是页面代码问题。
- 改为串行构建后，`medicalpro` Release 构建通过，`three_page_presentation_utils_test` 通过，启动验证结果为 `HasExited=False`。
