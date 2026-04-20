# Platform Diagnostics Page Matrix Follow-up Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在不回退到 UI 直连 CTK、尽量不改动 diagnostics 契约层的前提下，把 `PlatformDiagnosticsPage` 从当前已验收 subset 扩展到 2026-04-17 设计稿要求的完整字段矩阵与展示规则。

**Architecture:** 本切片优先视为“页面矩阵补齐”而不是新的诊断聚合专项。继续复用现有 `PlatformDiagnosticSnapshot`、`PlatformPluginLifecycleSnapshot`、`PlatformDiagnosticProblem` 和 `PlatformStartupTraceEntry` 数据契约，只在 `PlatformDiagnosticsPage` 内补齐格式化、显示规则和高亮逻辑；仅在实现中发现已有字段完全不足以支撑页面矩阵时，才允许回到 contracts/service 层做最小补线。

**Tech Stack:** Qt 6、Qt Widgets、QtTest、现有 `PlatformDiagnosticsPage`、现有 `PlatformDiagnosticSnapshot` 管道、`build_x64_noctk`

---

## Files and Responsibilities

- Modify: `UI/Forms/PlatformDiagnosticsPage.ui`
  - 扩展顶部摘要区，补齐 `full observed startup`、`slowest plugin`
  - 保持现有 problems / plugins / timeline 三块布局，但允许增加更适合完整矩阵的标题和默认占位文本
- Modify: `UI/NewPages/PlatformDiagnosticsPage.h`
  - 增加页面矩阵所需的格式化 helper、行高亮 helper 和新的 `populate...` 方法签名
- Modify: `UI/NewPages/PlatformDiagnosticsPage.cpp`
  - 实现完整摘要字段、`skipped_by_mode` 展示、slowest plugin 行高亮、完整插件表、完整时间线表、完整问题表
- Modify: `tests/unit/PlatformDiagnosticsPageTest.cpp`
  - 先写红灯测试锁定完整矩阵、显示规则和高亮规则
- Modify: `docs/current_status_and_project_overview.md`
  - 实现完成后回写“full diagnostics page matrix landed”结论和验收命令
- Modify: `docs/superpowers/tracking/platform-migration-decision-log.md`
  - 记录 diagnostics page 从 subset 扩到 full matrix 的决策
- Modify: `docs/superpowers/specs/2026-04-17-startup-performance-and-plugin-lifecycle-diagnostics-design.md`
  - 若实现命名或展示规则有微调，回写专项 spec 的实际 landed 口径

## Scope Guardrails

- 本计划不重做 `PlatformPluginLifecycleAggregator` 选择语义；沿用当前已验收的 summary / problems / trace 数据契约。
- 本计划不新增页面层 CTK 依赖，不允许在 `PlatformDiagnosticsPage` 中直接读取 `CTKManager` 或执行 service lookup。
- 本计划不把 `PlatformDiagnosticsPage` 改造成新的 view-model 层；优先用页面内部 helper 消化已有快照字段。
- 若某个设计稿字段当前没有可靠数据来源，必须先补测试说明“缺哪个字段”，再决定是否扩 contracts/service，而不是直接在 UI 侧猜状态。

### Task 1: 补齐摘要区矩阵与摘要显示规则

**Files:**
- Modify: `UI/Forms/PlatformDiagnosticsPage.ui`
- Modify: `UI/NewPages/PlatformDiagnosticsPage.h`
- Modify: `UI/NewPages/PlatformDiagnosticsPage.cpp`
- Modify: `tests/unit/PlatformDiagnosticsPageTest.cpp`

- [ ] **Step 1: 先写 failing tests，锁定完整摘要字段、`skipped_by_mode` 和 blocking point 高亮**

```cpp
// tests/unit/PlatformDiagnosticsPageTest.cpp
void refreshSnapshot_renders_extended_summary_fields_and_highlighted_blocking_point();
void refreshSnapshot_renders_warmup_tail_as_skipped_by_mode_outside_orchestrate_core();
```

```cpp
void PlatformDiagnosticsPageTest::refreshSnapshot_renders_extended_summary_fields_and_highlighted_blocking_point()
{
    PlatformDiagnosticSnapshot snapshot;
    snapshot.summary.runtimeMode = PlatformRuntimeMode::OrchestrateCore;
    snapshot.summary.frameworkReady = true;
    snapshot.summary.platformReady = false;
    snapshot.summary.startupReadyPathMs = 680;
    snapshot.summary.startupWarmupTailMs = 120;
    snapshot.summary.fullObservedStartupMs = 910;
    snapshot.summary.slowestPluginId = QStringLiteral("org.medicalpro.registration_core");
    snapshot.summary.blockingSpanLabel = QStringLiteral("registration_core service_ready");
    snapshot.summary.failurePointLabel = QStringLiteral("registration_core service_ready_timeout");

    PlatformPluginLifecycleSnapshot plugin;
    plugin.pluginId = QStringLiteral("org.medicalpro.registration_core");
    plugin.ctkSymbolicName = QStringLiteral("RegistrationCore");
    plugin.state = PlatformPluginState::Failed;
    snapshot.pluginLifecycle.append(plugin);

    PlatformDiagnosticsPage page(nullptr, [snapshot]() { return snapshot; });
    page.refreshSnapshot();

    QCOMPARE(
        page.findChild<QLabel*>(QStringLiteral("fullObservedStartupValueLabel"))->text(),
        QStringLiteral("910 ms"));
    QCOMPARE(
        page.findChild<QLabel*>(QStringLiteral("slowestPluginValueLabel"))->text(),
        QStringLiteral("RegistrationCore (org.medicalpro.registration_core)"));
    QVERIFY(page.findChild<QLabel*>(QStringLiteral("blockingPointValueLabel"))->font().bold());
}

void PlatformDiagnosticsPageTest::refreshSnapshot_renders_warmup_tail_as_skipped_by_mode_outside_orchestrate_core()
{
    PlatformDiagnosticSnapshot snapshot;
    snapshot.summary.runtimeMode = PlatformRuntimeMode::FacadeMode;
    snapshot.summary.frameworkReady = true;
    snapshot.summary.platformReady = true;
    snapshot.summary.startupReadyPathMs = 320;
    snapshot.summary.startupWarmupTailMs = 0;
    snapshot.summary.fullObservedStartupMs = 320;
    snapshot.summary.blockingSpanLabel = QStringLiteral("none");
    snapshot.summary.failurePointLabel = QStringLiteral("none");

    PlatformDiagnosticsPage page(nullptr, [snapshot]() { return snapshot; });
    page.refreshSnapshot();

    QCOMPARE(
        page.findChild<QLabel*>(QStringLiteral("warmupTailValueLabel"))->text(),
        QStringLiteral("skipped_by_mode"));
    QVERIFY(!page.findChild<QLabel*>(QStringLiteral("blockingPointValueLabel"))->font().bold());
}
```

- [ ] **Step 2: 运行页面测试，确认红灯**

Run:

```powershell
cmake --build build_x64_noctk --config Release --target platform_diagnostics_page_test
ctest --test-dir build_x64_noctk -C Release -R platform_diagnostics_page_test --output-on-failure
```

Expected:

- `platform_diagnostics_page_test` FAIL
- 失败原因指向缺少 `fullObservedStartupValueLabel` / `slowestPluginValueLabel` 或摘要文案规则不符合预期

- [ ] **Step 3: 以最小实现补齐摘要 labels、格式化 helper 和 blocking point 高亮**

```xml
<!-- UI/Forms/PlatformDiagnosticsPage.ui -->
<item row="3" column="0">
 <widget class="QLabel" name="fullObservedStartupCaptionLabel">
  <property name="text">
   <string/>
  </property>
 </widget>
</item>
<item row="3" column="1">
 <widget class="QLabel" name="fullObservedStartupValueLabel">
  <property name="text">
   <string>0 ms</string>
  </property>
 </widget>
</item>
<item row="3" column="2">
 <widget class="QLabel" name="slowestPluginCaptionLabel">
  <property name="text">
   <string/>
  </property>
 </widget>
</item>
<item row="3" column="3">
 <widget class="QLabel" name="slowestPluginValueLabel">
  <property name="text">
   <string>none</string>
  </property>
 </widget>
</item>
```

```cpp
// UI/NewPages/PlatformDiagnosticsPage.h
void applyBlockingPointEmphasis(bool highlight);
QString warmupTailText(const PlatformDiagnosticSnapshot& snapshot) const;
QString slowestPluginText(const PlatformDiagnosticSnapshot& snapshot) const;
QString pluginDisplayText(const PlatformPluginLifecycleSnapshot& plugin) const;
```

```cpp
// UI/NewPages/PlatformDiagnosticsPage.cpp
ui->fullObservedStartupCaptionLabel->setText(QStringLiteral("Full observed startup"));
ui->slowestPluginCaptionLabel->setText(QStringLiteral("Slowest plugin"));

ui->warmupTailValueLabel->setText(warmupTailText(snapshot));
ui->fullObservedStartupValueLabel->setText(QStringLiteral("%1 ms").arg(snapshot.summary.fullObservedStartupMs));
ui->slowestPluginValueLabel->setText(slowestPluginText(snapshot));
applyBlockingPointEmphasis(!snapshot.summary.platformReady);

QString PlatformDiagnosticsPage::warmupTailText(const PlatformDiagnosticSnapshot& snapshot) const
{
    if (snapshot.summary.runtimeMode != PlatformRuntimeMode::OrchestrateCore) {
        return QStringLiteral("skipped_by_mode");
    }
    return QStringLiteral("%1 ms").arg(snapshot.summary.startupWarmupTailMs);
}

QString PlatformDiagnosticsPage::slowestPluginText(const PlatformDiagnosticSnapshot& snapshot) const
{
    for (const auto& plugin : snapshot.pluginLifecycle) {
        if (plugin.pluginId != snapshot.summary.slowestPluginId) continue;
        return pluginDisplayText(plugin);
    }
    return snapshot.summary.slowestPluginId.isEmpty() ? QStringLiteral("none") : snapshot.summary.slowestPluginId;
}

QString PlatformDiagnosticsPage::pluginDisplayText(const PlatformPluginLifecycleSnapshot& plugin) const
{
    if (!plugin.displayName.isEmpty() && plugin.displayName != plugin.pluginId) {
        return QStringLiteral("%1 (%2)").arg(plugin.displayName, plugin.pluginId);
    }
    if (!plugin.ctkSymbolicName.isEmpty() && plugin.ctkSymbolicName != plugin.pluginId) {
        return QStringLiteral("%1 (%2)").arg(plugin.ctkSymbolicName, plugin.pluginId);
    }
    return plugin.pluginId;
}

void PlatformDiagnosticsPage::applyBlockingPointEmphasis(bool highlight)
{
    auto font = ui->blockingPointValueLabel->font();
    font.setBold(highlight);
    ui->blockingPointValueLabel->setFont(font);
}
```

- [ ] **Step 4: 重新运行页面测试，确认摘要规则转绿**

Run:

```powershell
cmake --build build_x64_noctk --config Release --target platform_diagnostics_page_test
ctest --test-dir build_x64_noctk -C Release -R platform_diagnostics_page_test --output-on-failure
```

Expected:

- `platform_diagnostics_page_test` PASS
- 新增摘要字段和 `skipped_by_mode` 规则通过

- [ ] **Step 5: 提交摘要区矩阵补齐**

```powershell
git add UI/Forms/PlatformDiagnosticsPage.ui UI/NewPages/PlatformDiagnosticsPage.h UI/NewPages/PlatformDiagnosticsPage.cpp tests/unit/PlatformDiagnosticsPageTest.cpp
git commit -m "feat: extend platform diagnostics summary matrix"
```

### Task 2: 扩展插件生命周期表到完整矩阵并高亮最慢插件

**Files:**
- Modify: `UI/NewPages/PlatformDiagnosticsPage.h`
- Modify: `UI/NewPages/PlatformDiagnosticsPage.cpp`
- Modify: `tests/unit/PlatformDiagnosticsPageTest.cpp`

- [ ] **Step 1: 写 failing tests，锁定完整插件表字段和 slowest row 高亮**

```cpp
// tests/unit/PlatformDiagnosticsPageTest.cpp
void refreshSnapshot_renders_full_plugin_lifecycle_matrix_and_highlights_slowest_plugin();
```

```cpp
void PlatformDiagnosticsPageTest::refreshSnapshot_renders_full_plugin_lifecycle_matrix_and_highlights_slowest_plugin()
{
    PlatformDiagnosticSnapshot snapshot;
    snapshot.summary.runtimeMode = PlatformRuntimeMode::OrchestrateCore;
    snapshot.summary.slowestPluginId = QStringLiteral("org.medicalpro.registration_core");

    PlatformPluginLifecycleSnapshot failedPlugin;
    failedPlugin.pluginId = QStringLiteral("org.medicalpro.registration_core");
    failedPlugin.ctkSymbolicName = QStringLiteral("RegistrationCore");
    failedPlugin.displayName = QStringLiteral("Registration Core");
    failedPlugin.bootstrapLevel = PlatformBootstrapLevel::Core;
    failedPlugin.startupPolicy = PlatformStartupPolicy::Eager;
    failedPlugin.state = PlatformPluginState::Failed;
    failedPlugin.installMs = 80;
    failedPlugin.startMs = 140;
    failedPlugin.serviceReadyMs = 510;
    failedPlugin.warmupMs = 0;
    failedPlugin.blockingMs = 730;
    failedPlugin.slowestStep = PlatformLifecycleStep::ServiceReady;
    failedPlugin.lastReasonCode = QStringLiteral("service_ready_timeout");
    failedPlugin.recoveryHints = QStringList{QStringLiteral("Check service registration chain")};

    PlatformPluginLifecycleSnapshot readyPlugin;
    readyPlugin.pluginId = QStringLiteral("org.medicalpro.user_management");
    readyPlugin.ctkSymbolicName = QStringLiteral("UserManagement");
    readyPlugin.displayName = QStringLiteral("User Management");
    readyPlugin.bootstrapLevel = PlatformBootstrapLevel::Core;
    readyPlugin.startupPolicy = PlatformStartupPolicy::Eager;
    readyPlugin.state = PlatformPluginState::Ready;
    readyPlugin.installMs = 30;
    readyPlugin.startMs = 60;
    readyPlugin.serviceReadyMs = 90;
    readyPlugin.warmupMs = 40;
    readyPlugin.blockingMs = 180;
    readyPlugin.slowestStep = PlatformLifecycleStep::ServiceReady;

    snapshot.pluginLifecycle = {readyPlugin, failedPlugin};

    PlatformDiagnosticsPage page(nullptr, [snapshot]() { return snapshot; });
    page.refreshSnapshot();

    auto* pluginTable = page.findChild<QTableWidget*>(QStringLiteral("pluginTableWidget"));
    QCOMPARE(pluginTable->columnCount(), 14);
    QCOMPARE(pluginTable->horizontalHeaderItem(0)->text(), QStringLiteral("Plugin ID"));
    QCOMPARE(pluginTable->horizontalHeaderItem(3)->text(), QStringLiteral("Startup Policy"));
    QCOMPARE(pluginTable->horizontalHeaderItem(13)->text(), QStringLiteral("Recovery"));
    QCOMPARE(pluginTable->item(0, 0)->text(), QStringLiteral("org.medicalpro.registration_core"));
    QCOMPARE(pluginTable->item(0, 2)->text(), QStringLiteral("core"));
    QCOMPARE(pluginTable->item(0, 3)->text(), QStringLiteral("eager"));
    QCOMPARE(pluginTable->item(0, 8)->text(), QStringLiteral("0"));
    QCOMPARE(pluginTable->item(0, 12)->text(), QStringLiteral("service_ready_timeout"));
    QCOMPARE(pluginTable->item(0, 13)->text(), QStringLiteral("Check service registration chain"));
    QVERIFY(pluginTable->item(0, 0)->font().bold());
    QVERIFY(!pluginTable->item(1, 0)->font().bold());
}
```

- [ ] **Step 2: 运行页面测试，确认插件矩阵红灯**

Run:

```powershell
cmake --build build_x64_noctk --config Release --target platform_diagnostics_page_test
ctest --test-dir build_x64_noctk -C Release -R platform_diagnostics_page_test --output-on-failure
```

Expected:

- `platform_diagnostics_page_test` FAIL
- 失败原因指向插件表列数/列头/单元格值/高亮规则不满足

- [ ] **Step 3: 扩展插件表 headers、渲染 helper 和 slowest row 高亮**

```cpp
// UI/NewPages/PlatformDiagnosticsPage.h
void populatePluginTable(const QVector<PlatformPluginLifecycleSnapshot>& plugins, const QString& slowestPluginId);
QString bootstrapText(PlatformBootstrapLevel level) const;
QString startupPolicyText(PlatformStartupPolicy policy) const;
QString stepText(PlatformLifecycleStep step) const;
QString recoveryText(const QStringList& recoveryHints) const;
void applyRowEmphasis(QTableWidget* table, int row, bool highlight) const;
```

```cpp
// UI/NewPages/PlatformDiagnosticsPage.cpp
ui->pluginTableWidget->setColumnCount(14);
ui->pluginTableWidget->setHorizontalHeaderLabels(QStringList{
    QStringLiteral("Plugin ID"),
    QStringLiteral("Symbolic Name"),
    QStringLiteral("Bootstrap"),
    QStringLiteral("Startup Policy"),
    QStringLiteral("State"),
    QStringLiteral("Install(ms)"),
    QStringLiteral("Start(ms)"),
    QStringLiteral("Service Ready(ms)"),
    QStringLiteral("Warmup(ms)"),
    QStringLiteral("Blocking(ms)"),
    QStringLiteral("Slowest Step"),
    QStringLiteral("Missing Dependencies"),
    QStringLiteral("Last Reason"),
    QStringLiteral("Recovery")
});

populatePluginTable(pluginLifecycle, snapshot.summary.slowestPluginId);

void PlatformDiagnosticsPage::populatePluginTable(
    const QVector<PlatformPluginLifecycleSnapshot>& plugins,
    const QString& slowestPluginId)
{
    ui->pluginTableWidget->clearContents();
    ui->pluginTableWidget->setRowCount(plugins.size());

    for (int row = 0; row < plugins.size(); ++row) {
        const auto& plugin = plugins.at(row);
        ui->pluginTableWidget->setItem(row, 0, makeReadOnlyItem(plugin.pluginId));
        ui->pluginTableWidget->setItem(row, 1, makeReadOnlyItem(plugin.ctkSymbolicName));
        ui->pluginTableWidget->setItem(row, 2, makeReadOnlyItem(bootstrapText(plugin.bootstrapLevel)));
        ui->pluginTableWidget->setItem(row, 3, makeReadOnlyItem(startupPolicyText(plugin.startupPolicy)));
        ui->pluginTableWidget->setItem(row, 4, makeReadOnlyItem(pluginStateText(plugin.state)));
        ui->pluginTableWidget->setItem(row, 5, makeReadOnlyItem(QString::number(plugin.installMs)));
        ui->pluginTableWidget->setItem(row, 6, makeReadOnlyItem(QString::number(plugin.startMs)));
        ui->pluginTableWidget->setItem(row, 7, makeReadOnlyItem(QString::number(plugin.serviceReadyMs)));
        ui->pluginTableWidget->setItem(row, 8, makeReadOnlyItem(QString::number(plugin.warmupMs)));
        ui->pluginTableWidget->setItem(row, 9, makeReadOnlyItem(QString::number(plugin.blockingMs)));
        ui->pluginTableWidget->setItem(row, 10, makeReadOnlyItem(stepText(plugin.slowestStep)));
        ui->pluginTableWidget->setItem(row, 11, makeReadOnlyItem(missingDependenciesText(plugin)));
        ui->pluginTableWidget->setItem(row, 12, makeReadOnlyItem(plugin.lastReasonCode.isEmpty() ? QStringLiteral("none") : plugin.lastReasonCode));
        ui->pluginTableWidget->setItem(row, 13, makeReadOnlyItem(recoveryText(plugin.recoveryHints)));
        applyRowEmphasis(ui->pluginTableWidget, row, plugin.pluginId == slowestPluginId);
    }
}
```

- [ ] **Step 4: 重新运行页面测试，确认插件矩阵转绿**

Run:

```powershell
cmake --build build_x64_noctk --config Release --target platform_diagnostics_page_test
ctest --test-dir build_x64_noctk -C Release -R platform_diagnostics_page_test --output-on-failure
```

Expected:

- `platform_diagnostics_page_test` PASS
- 插件表完整矩阵和 slowest row 高亮通过

- [ ] **Step 5: 提交插件生命周期矩阵**

```powershell
git add UI/NewPages/PlatformDiagnosticsPage.h UI/NewPages/PlatformDiagnosticsPage.cpp tests/unit/PlatformDiagnosticsPageTest.cpp
git commit -m "feat: expand platform diagnostics plugin matrix"
```

### Task 3: 扩展时间线表与问题表到完整字段矩阵

**Files:**
- Modify: `UI/NewPages/PlatformDiagnosticsPage.h`
- Modify: `UI/NewPages/PlatformDiagnosticsPage.cpp`
- Modify: `tests/unit/PlatformDiagnosticsPageTest.cpp`

- [ ] **Step 1: 写 failing tests，锁定 timeline scope/offset/step 和 problem impact/recovery 字段**

```cpp
// tests/unit/PlatformDiagnosticsPageTest.cpp
void refreshSnapshot_renders_expanded_timeline_fields_with_scope_and_subject();
void refreshSnapshot_renders_problem_matrix_with_impact_capability_and_recovery_hint();
```

```cpp
void PlatformDiagnosticsPageTest::refreshSnapshot_renders_expanded_timeline_fields_with_scope_and_subject()
{
    PlatformDiagnosticSnapshot snapshot;
    snapshot.summary.runtimeMode = PlatformRuntimeMode::OrchestrateCore;

    PlatformStartupTraceEntry pluginEntry;
    pluginEntry.phaseKey = QStringLiteral("registration_service_ready");
    pluginEntry.phaseLabel = QStringLiteral("Registration service ready");
    pluginEntry.pluginId = QStringLiteral("org.medicalpro.registration_core");
    pluginEntry.ctkSymbolicName = QStringLiteral("RegistrationCore");
    pluginEntry.step = PlatformLifecycleStep::ServiceReady;
    pluginEntry.result = PlatformLifecycleResult::Timeout;
    pluginEntry.blockingStartup = true;
    pluginEntry.startOffsetMs = 280;
    pluginEntry.elapsedMs = 420;
    pluginEntry.detail = QStringLiteral("RegistrationService missing");

    PlatformStartupTraceEntry phaseEntry;
    phaseEntry.phaseKey = QStringLiteral("warmup_phase");
    phaseEntry.phaseLabel = QStringLiteral("Warmup optional plugins");
    phaseEntry.step = PlatformLifecycleStep::Warmup;
    phaseEntry.result = PlatformLifecycleResult::Skipped;
    phaseEntry.blockingStartup = false;
    phaseEntry.startOffsetMs = 760;
    phaseEntry.elapsedMs = 90;
    phaseEntry.detail = QStringLiteral("skipped by mode");

    snapshot.startupTrace = {phaseEntry, pluginEntry};

    PlatformDiagnosticsPage page(nullptr, [snapshot]() { return snapshot; });
    page.refreshSnapshot();

    auto* traceTable = page.findChild<QTableWidget*>(QStringLiteral("traceTableWidget"));
    QCOMPARE(traceTable->columnCount(), 8);
    QCOMPARE(traceTable->item(0, 0)->text(), QStringLiteral("280"));
    QCOMPARE(traceTable->item(0, 2)->text(), QStringLiteral("plugin"));
    QCOMPARE(traceTable->item(0, 3)->text(), QStringLiteral("RegistrationCore"));
    QCOMPARE(traceTable->item(0, 4)->text(), QStringLiteral("service_ready"));
    QCOMPARE(traceTable->item(0, 5)->text(), QStringLiteral("timeout"));
    QCOMPARE(traceTable->item(1, 2)->text(), QStringLiteral("phase"));
    QCOMPARE(traceTable->item(1, 3)->text(), QStringLiteral("Warmup optional plugins"));
}

void PlatformDiagnosticsPageTest::refreshSnapshot_renders_problem_matrix_with_impact_capability_and_recovery_hint()
{
    PlatformDiagnosticSnapshot snapshot;
    snapshot.summary.runtimeMode = PlatformRuntimeMode::OrchestrateCore;

    PlatformDiagnosticProblem problem;
    problem.severity = PlatformDiagnosticSeverity::Critical;
    problem.pluginId = QStringLiteral("org.medicalpro.registration_core");
    problem.reasonCode = QStringLiteral("service_ready_timeout");
    problem.detail = QStringLiteral("RegistrationService missing");
    problem.impactCapabilities = QStringList{
        QStringLiteral("navigation.registration"),
        QStringLiteral("navigation.guidance")
    };
    problem.recoveryHints = QStringList{
        QStringLiteral("Check service registration chain"),
        QStringLiteral("Verify required services are available")
    };
    snapshot.problems.append(problem);

    PlatformDiagnosticsPage page(nullptr, [snapshot]() { return snapshot; });
    page.refreshSnapshot();

    auto* problemTable = page.findChild<QTableWidget*>(QStringLiteral("problemTableWidget"));
    QCOMPARE(problemTable->columnCount(), 6);
    QCOMPARE(problemTable->item(0, 0)->text(), QStringLiteral("critical"));
    QCOMPARE(problemTable->item(0, 1)->text(), QStringLiteral("org.medicalpro.registration_core"));
    QCOMPARE(problemTable->item(0, 2)->text(), QStringLiteral("service_ready_timeout"));
    QCOMPARE(problemTable->item(0, 3)->text(), QStringLiteral("navigation.registration | navigation.guidance"));
    QCOMPARE(problemTable->item(0, 5)->text(), QStringLiteral("Check service registration chain | Verify required services are available"));
}
```

- [ ] **Step 2: 运行页面测试，确认时间线/问题矩阵红灯**

Run:

```powershell
cmake --build build_x64_noctk --config Release --target platform_diagnostics_page_test
ctest --test-dir build_x64_noctk -C Release -R platform_diagnostics_page_test --output-on-failure
```

Expected:

- `platform_diagnostics_page_test` FAIL
- 失败原因指向时间线列缺失、scope/subject 未展示或问题表未展示 impact/recovery

- [ ] **Step 3: 扩展 timeline/problem headers、格式化 helper 和渲染逻辑**

```cpp
// UI/NewPages/PlatformDiagnosticsPage.h
QString traceScopeText(const PlatformStartupTraceEntry& traceEntry) const;
QString traceSubjectText(const PlatformStartupTraceEntry& traceEntry) const;
QString listText(const QStringList& values) const;
```

```cpp
// UI/NewPages/PlatformDiagnosticsPage.cpp
ui->problemTableWidget->setColumnCount(6);
ui->problemTableWidget->setHorizontalHeaderLabels(QStringList{
    QStringLiteral("Severity"),
    QStringLiteral("Plugin"),
    QStringLiteral("Reason"),
    QStringLiteral("Impact Capability"),
    QStringLiteral("Detail"),
    QStringLiteral("Recovery Hint")
});

ui->traceTableWidget->setColumnCount(8);
ui->traceTableWidget->setHorizontalHeaderLabels(QStringList{
    QStringLiteral("Start Offset(ms)"),
    QStringLiteral("Duration(ms)"),
    QStringLiteral("Scope"),
    QStringLiteral("Plugin / Phase"),
    QStringLiteral("Step"),
    QStringLiteral("Result"),
    QStringLiteral("Blocking"),
    QStringLiteral("Detail")
});

ui->traceTableWidget->setItem(row, 0, makeReadOnlyItem(QString::number(traceEntry.startOffsetMs)));
ui->traceTableWidget->setItem(row, 1, makeReadOnlyItem(QString::number(traceEntry.elapsedMs)));
ui->traceTableWidget->setItem(row, 2, makeReadOnlyItem(traceScopeText(traceEntry)));
ui->traceTableWidget->setItem(row, 3, makeReadOnlyItem(traceSubjectText(traceEntry)));
ui->traceTableWidget->setItem(row, 4, makeReadOnlyItem(lifecycleStepText(traceEntry.step)));
ui->traceTableWidget->setItem(row, 5, makeReadOnlyItem(resultText(traceEntry.result)));
ui->traceTableWidget->setItem(row, 6, makeReadOnlyItem(boolText(traceEntry.blockingStartup)));
ui->traceTableWidget->setItem(row, 7, makeReadOnlyItem(traceEntry.detail));

ui->problemTableWidget->setItem(row, 0, makeReadOnlyItem(severityText(problem.severity)));
ui->problemTableWidget->setItem(row, 1, makeReadOnlyItem(problem.pluginId));
ui->problemTableWidget->setItem(row, 2, makeReadOnlyItem(problem.reasonCode));
ui->problemTableWidget->setItem(row, 3, makeReadOnlyItem(listText(problem.impactCapabilities)));
ui->problemTableWidget->setItem(row, 4, makeReadOnlyItem(problem.detail));
ui->problemTableWidget->setItem(row, 5, makeReadOnlyItem(listText(problem.recoveryHints)));

QString PlatformDiagnosticsPage::traceScopeText(const PlatformStartupTraceEntry& traceEntry) const
{
    return (!traceEntry.pluginId.isEmpty() || !traceEntry.ctkSymbolicName.isEmpty())
        ? QStringLiteral("plugin")
        : QStringLiteral("phase");
}

QString PlatformDiagnosticsPage::traceSubjectText(const PlatformStartupTraceEntry& traceEntry) const
{
    if (!traceEntry.ctkSymbolicName.isEmpty()) return traceEntry.ctkSymbolicName;
    if (!traceEntry.pluginId.isEmpty()) return traceEntry.pluginId;
    if (!traceEntry.phaseLabel.isEmpty()) return traceEntry.phaseLabel;
    return traceEntry.phaseKey;
}

QString PlatformDiagnosticsPage::listText(const QStringList& values) const
{
    return values.isEmpty() ? QStringLiteral("none") : values.join(QStringLiteral(" | "));
}
```

- [ ] **Step 4: 重新运行页面测试，确认完整矩阵转绿**

Run:

```powershell
cmake --build build_x64_noctk --config Release --target platform_diagnostics_page_test
ctest --test-dir build_x64_noctk -C Release -R platform_diagnostics_page_test --output-on-failure
```

Expected:

- `platform_diagnostics_page_test` PASS
- 时间线表和问题表完整矩阵通过

- [ ] **Step 5: 提交 timeline/problem matrix**

```powershell
git add UI/NewPages/PlatformDiagnosticsPage.h UI/NewPages/PlatformDiagnosticsPage.cpp tests/unit/PlatformDiagnosticsPageTest.cpp
git commit -m "feat: expand platform diagnostics page matrix"
```

### Task 4: 跑整专项验收并回写 full matrix landed 文档

**Files:**
- Modify: `docs/current_status_and_project_overview.md`
- Modify: `docs/superpowers/tracking/platform-migration-decision-log.md`
- Modify: `docs/superpowers/specs/2026-04-17-startup-performance-and-plugin-lifecycle-diagnostics-design.md`

- [ ] **Step 1: 回写 landed scope，关闭 “subset only” 口径**

```md
## 2026-04-20 Startup Diagnostics Page Matrix Follow-up

- The diagnostics page now renders the full summary matrix, full plugin lifecycle matrix, full timeline matrix, and full problem matrix defined by the 2026-04-17 diagnostics design.
- `warmup tail` now shows `skipped_by_mode` outside `orchestrate_core`.
- The slowest plugin row and blocking point emphasis are now part of the landed page contract in this worktree.
```

```md
## 2026-04-20

- Decision: accept the full diagnostics page matrix as landed on top of the previously accepted lifecycle-event-based diagnostics foundation.
- Rationale: the page now surfaces all required summary, plugin lifecycle, timeline, and problem-list fields from the 2026-04-17 diagnostics design without reintroducing UI direct-CTK access.
- Impact: the “implementation-plan subset only” wording can now be narrowed to historical rollout context instead of current functional limitation.
```

```md
## Scope Note (Update)

- The foundation + subset limitation documented on 2026-04-19 is historical rollout context.
- After the follow-up implementation lands, the diagnostics page field matrix defined in Section 9 should be treated as implemented in this worktree unless a later note says otherwise.
```

- [ ] **Step 2: 跑完整页面专项验收与 CTK 解耦回归**

Run:

```powershell
cmake --build build_x64_noctk --config Release --target medicalpro platform_diagnostics_page_test platform_diagnostics_service_test platform_ui_bridge_test ui_ctk_decoupling_acceptance_test
ctest --test-dir build_x64_noctk -C Release -R "platform_diagnostics_page_test|platform_diagnostics_service_test|platform_ui_bridge_test|ui_ctk_decoupling_acceptance_test" --output-on-failure
rg -n "CTKManager::instance\(|getService<" UI\NewPages UI\MainInterfaceWidget.cpp
```

Expected:

- `medicalpro` 编译通过
- `platform_diagnostics_page_test` PASS
- `platform_diagnostics_service_test` PASS
- `platform_ui_bridge_test` PASS
- `ui_ctk_decoupling_acceptance_test` PASS
- `rg` 无输出

- [ ] **Step 3: 记录验收结果并提交 full matrix follow-up**

```powershell
git add UI/Forms/PlatformDiagnosticsPage.ui UI/NewPages/PlatformDiagnosticsPage.h UI/NewPages/PlatformDiagnosticsPage.cpp tests/unit/PlatformDiagnosticsPageTest.cpp docs/current_status_and_project_overview.md docs/superpowers/tracking/platform-migration-decision-log.md docs/superpowers/specs/2026-04-17-startup-performance-and-plugin-lifecycle-diagnostics-design.md
git commit -m "feat: complete platform diagnostics page matrix"
```

## Coverage Review

- 摘要区剩余字段：Task 1
- `skipped_by_mode` 与 blocking point 高亮：Task 1
- 完整插件生命周期表字段与 slowest row 高亮：Task 2
- 完整时间线表字段：Task 3
- 完整问题表字段：Task 3
- 文档回写与 full-matrix 验收：Task 4
- UI 不回退到 CTK 直连：Task 4

## Review Notes

- 本计划故意不再扩 diagnostics contracts；当前 `PlatformDiagnosticSnapshot` 已含 `fullObservedStartupMs`、`slowestPluginId`、`impactCapabilities`、`recoveryHints`、`bootstrapLevel`、`startupPolicy`、`startOffsetMs` 等页面所需字段。
- 如果实施中发现某个字段只有“猜测值”没有可靠来源，应先停在对应任务的红灯测试，回到专项 spec 明确是否需要新增数据契约，不要在页面层自行推理。
- `docs/current_status_and_project_overview.md` 与 `docs/superpowers/tracking/platform-migration-decision-log.md` 当前写着 “subset” 边界；只有在本计划的 Task 4 验收通过后，才能改写为 full matrix landed。
