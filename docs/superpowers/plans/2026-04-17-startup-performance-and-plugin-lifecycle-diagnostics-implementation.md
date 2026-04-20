# Startup Performance and Plugin Lifecycle Diagnostics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在现有平台治理层上落地可量化的启动时间线、插件生命周期事件、问题诊断与 diagnostics page 增强，让开发者能直接看到启动慢点、阻塞点、失败点和恢复建议。

**Architecture:** 继续保留 `CTKManager + StartupOrchestrator + PlatformDiagnosticsService + PlatformDiagnosticsPage` 主体结构，在 `Framework/Platform/Diagnostics` 下增加生命周期 recorder 和 aggregator。`StartupOrchestrator`、`PlatformStartupCoordinator`、`CTKManager` 与 `main.cpp` 只负责产生日志化事实事件，`PlatformDiagnosticsService` 负责把这些事实聚合成 summary、timeline、plugin lifecycle 和 problem list，UI 只消费诊断快照，不回退到 UI 直连 CTK。

**Tech Stack:** Qt 6、QtTest、Qt Widgets、CTK Plugin Framework、CMake、现有 `Framework/Platform` 治理层、现有 `tests/unit`

---

## Files and Responsibilities

- Modify: `CMakeLists.txt`
  - 把新的 diagnostics 组件编入 `Framework`
- Modify: `Framework/Platform/Contracts/PlatformRuntimeTypes.h`
  - 增加生命周期事件、步骤、结果和问题等级枚举
- Modify: `Framework/Platform/Contracts/PlatformPluginDescriptor.h`
  - 增加 `diagnostics` 配置块
- Modify: `Framework/Platform/Contracts/PlatformSnapshots.h`
  - 扩展 `PlatformStartupTraceEntry`、新增 `PlatformLifecycleEvent`、`PlatformPluginLifecycleSnapshot`、`PlatformDiagnosticProblem`、`PlatformDiagnosticSummary`
- Modify: `Framework/Platform/Kernel/PlatformDescriptorLoader.cpp`
  - 解析 descriptor 中的 `diagnostics` 配置
- Modify: `Framework/Platform/Kernel/PlatformStateStore.h`
- Modify: `Framework/Platform/Kernel/PlatformStateStore.cpp`
  - 暴露 descriptor lookup，并为 capability 锁定、diagnostics 问题聚合提供基础状态
- Create: `Framework/Platform/Diagnostics/PlatformLifecycleTraceRecorder.h`
- Create: `Framework/Platform/Diagnostics/PlatformLifecycleTraceRecorder.cpp`
  - 记录 session、phase、plugin lifecycle 事件并派生 trace span
- Create: `Framework/Platform/Diagnostics/PlatformPluginLifecycleAggregator.h`
- Create: `Framework/Platform/Diagnostics/PlatformPluginLifecycleAggregator.cpp`
  - 从 recorder 事件、state store 与 runtime observation 派生 plugin lifecycle、summary 与 problem list
- Modify: `Framework/StartupOrchestrator.h`
- Modify: `Framework/StartupOrchestrator.cpp`
  - 记录 session/phase start/finish 事件
- Modify: `Framework/Platform/Kernel/PlatformStartupCoordinator.h`
- Modify: `Framework/Platform/Kernel/PlatformStartupCoordinator.cpp`
  - 记录 `skipped_by_mode`、`ensureReady()`、deferred/on-demand 生命周期事件
- Modify: `Framework/CTKManager.h`
- Modify: `Framework/CTKManager.cpp`
  - 暴露 install/start 成功失败事实事件，不把解释逻辑塞回 CTK 层
- Modify: `Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.h`
- Modify: `Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.cpp`
  - 把 CTK runtime observation 与 lifecycle recorder 输出汇总成 diagnostics 输入
- Modify: `Framework/Platform/Diagnostics/PlatformDiagnosticsService.h`
- Modify: `Framework/Platform/Diagnostics/PlatformDiagnosticsService.cpp`
  - 使用 aggregator 输出 enriched diagnostic snapshot
- Modify: `UI/MainInterfaceWidget.h`
- Modify: `UI/MainInterfaceWidget.cpp`
  - 持有 recorder，并继续通过现有 diagnostics service/page 管道暴露 snapshot
- Modify: `main.cpp`
  - 加载 descriptors，初始化 state store/runtime mode，连接 recorder、orchestrator、coordinator 与 CTKManager
- Modify: `UI/Forms/PlatformDiagnosticsPage.ui`
- Modify: `UI/NewPages/PlatformDiagnosticsPage.h`
- Modify: `UI/NewPages/PlatformDiagnosticsPage.cpp`
  - 新增摘要字段、问题列表、插件表排序与时间线字段
- Modify: `tests/unit/CMakeLists.txt`
  - 注册 recorder/aggregator 新测试目标
- Modify: `tests/unit/PlatformDescriptorLoaderTest.cpp`
  - 覆盖 `diagnostics` 配置块解析
- Create: `tests/unit/PlatformStartupTraceRecorderTest.cpp`
  - 覆盖 recorder 事件、span 生成与 mode skip 记录
- Modify: `tests/unit/PlatformStartupCoordinatorTest.cpp`
  - 覆盖三种模式事件与 `skipped_by_mode`
- Create: `tests/unit/PlatformPluginLifecycleAggregatorTest.cpp`
  - 覆盖 install/start/service ready/warmup 聚合、最慢插件、阻塞点、失败点
- Modify: `tests/unit/PlatformDiagnosticsServiceTest.cpp`
  - 覆盖 summary、problem list、recovery hints 与 ready-path/warmup-tail 分离
- Modify: `tests/unit/PlatformDiagnosticsPageTest.cpp`
  - 覆盖摘要区、问题列表、插件排序和时间线字段
- Modify: `tests/unit/PlatformUiBridgeTest.cpp`
  - 覆盖 `plugin active but service not ready` 与 service ready 迟到
- Modify: `docs/superpowers/specs/2026-04-16-platform-kernel-governance-design.md`
- Modify: `docs/superpowers/tracking/platform-plugin-governance-matrix.md`
- Modify: `docs/superpowers/tracking/platform-migration-decision-log.md`
- Modify: `docs/current_status_and_project_overview.md`
  - 回写专项落地结果

### Task 1: 扩展契约、descriptor diagnostics 配置和 lifecycle recorder 基础类型

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `Framework/Platform/Contracts/PlatformRuntimeTypes.h`
- Modify: `Framework/Platform/Contracts/PlatformPluginDescriptor.h`
- Modify: `Framework/Platform/Contracts/PlatformSnapshots.h`
- Modify: `Framework/Platform/Kernel/PlatformDescriptorLoader.cpp`
- Modify: `tests/unit/CMakeLists.txt`
- Modify: `tests/unit/PlatformDescriptorLoaderTest.cpp`
- Create: `tests/unit/PlatformStartupTraceRecorderTest.cpp`
- Create: `Framework/Platform/Diagnostics/PlatformLifecycleTraceRecorder.h`
- Create: `Framework/Platform/Diagnostics/PlatformLifecycleTraceRecorder.cpp`

- [ ] **Step 1: 先注册新测试 target，并写 failing tests 锁定 diagnostics block 与 recorder 输出**

```cmake
# tests/unit/CMakeLists.txt
add_executable(platform_startup_trace_recorder_test
    PlatformStartupTraceRecorderTest.cpp
)

target_include_directories(platform_startup_trace_recorder_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(platform_startup_trace_recorder_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME platform_startup_trace_recorder_test
    COMMAND platform_startup_trace_recorder_test
)
```

```cpp
// tests/unit/PlatformDescriptorLoaderTest.cpp
void PlatformDescriptorLoaderTest::loadFromFile_reads_diagnostics_block();

// tests/unit/PlatformStartupTraceRecorderTest.cpp
void PlatformStartupTraceRecorderTest::records_phase_and_plugin_spans();
void PlatformStartupTraceRecorderTest::records_skipped_step_for_runtime_mode();
```

```cpp
void PlatformDescriptorLoaderTest::loadFromFile_reads_diagnostics_block()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile file(dir.filePath(QStringLiteral("plugin.json")));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"json({
      "id": "org.medicalpro.dicom_viewer",
      "version": "1.0.0",
      "display_name": "DicomViewer",
      "domain": "imaging",
      "enabled": true,
      "runtime": {
        "ctk_symbolic_name": "DicomViewer",
        "startup_policy": "eager",
        "bootstrap_level": "core",
        "entry_capability": "imaging.data"
      },
      "provides": {"services": [], "capabilities": ["imaging.data"], "plugins": []},
      "requires": {"services": [], "capabilities": [], "plugins": []},
      "optional": {"services": [], "capabilities": [], "plugins": []},
      "health_checks": ["service_registered"],
      "diagnostics": {
        "required_services": ["DicomViewerService"],
        "service_ready_timeout_ms": 5000,
        "warmup_tasks": ["data_path_precheck"],
        "warmup_timeout_ms": 15000,
        "warmup_impacts_ready": false,
        "degrade_on": ["warmup_failed"]
      }
    })json");
    file.close();

    QString error;
    const auto descriptor = PlatformDescriptorLoader::loadFromFile(file.fileName(), &error);

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(descriptor.diagnostics.requiredServices, (QStringList{QStringLiteral("DicomViewerService")}));
    QCOMPARE(descriptor.diagnostics.serviceReadyTimeoutMs, 5000);
    QCOMPARE(descriptor.diagnostics.warmupTasks, (QStringList{QStringLiteral("data_path_precheck")}));
    QVERIFY(!descriptor.diagnostics.warmupImpactsReady);
}
```

- [ ] **Step 2: 运行测试，确认先失败**

Run:

```powershell
cmake --build build_x64_noctk --config Release --target platform_descriptor_loader_test platform_startup_trace_recorder_test
ctest --test-dir build_x64_noctk -C Release -R "platform_descriptor_loader_test|platform_startup_trace_recorder_test" --output-on-failure
```

Expected:

- `PlatformDescriptorLoaderTest` 因 `PlatformPluginDescriptor::diagnostics` 尚不存在而编译失败或断言失败
- `platform_startup_trace_recorder_test` 因 `PlatformLifecycleTraceRecorder` 与新 snapshots 尚不存在而编译失败

- [ ] **Step 3: 实现 diagnostics block、扩展 snapshots，并新增 recorder**

```cpp
// Framework/Platform/Contracts/PlatformRuntimeTypes.h
enum class PlatformLifecycleEventKind
{
    StartupSessionStarted,
    StartupSessionFinished,
    PhaseStarted,
    PhaseFinished,
    PluginInstallStarted,
    PluginInstallFinished,
    PluginStartStarted,
    PluginStartFinished,
    PluginServiceReady,
    PluginWarmupStarted,
    PluginWarmupFinished,
    PluginFailed,
    PluginDegraded,
    PluginSkippedByMode
};

enum class PlatformLifecycleStep
{
    None,
    Install,
    Start,
    ServiceReady,
    Warmup
};

enum class PlatformLifecycleResult
{
    Running,
    Succeeded,
    Failed,
    Degraded,
    Skipped,
    Timeout
};
```

```cpp
// Framework/Platform/Contracts/PlatformPluginDescriptor.h
struct PlatformDiagnosticsDescriptor
{
    QStringList requiredServices;
    int serviceReadyTimeoutMs = 0;
    QStringList warmupTasks;
    int warmupTimeoutMs = 0;
    bool warmupImpactsReady = false;
    QStringList degradeOn;
};
```

```cpp
// Framework/Platform/Contracts/PlatformSnapshots.h
struct PlatformLifecycleEvent
{
    QString sessionId;
    PlatformLifecycleEventKind kind = PlatformLifecycleEventKind::PhaseStarted;
    PlatformLifecycleStep step = PlatformLifecycleStep::None;
    PlatformLifecycleResult result = PlatformLifecycleResult::Running;
    PlatformRuntimeMode runtimeMode = PlatformRuntimeMode::ObserveOnly;
    QString phaseKey;
    QString phaseLabel;
    QString pluginId;
    QString ctkSymbolicName;
    qint64 offsetMs = 0;
    qint64 durationMs = 0;
    bool blockingStartup = false;
    bool critical = false;
    QString reasonCode;
    QString detail;
    QStringList missingServices;
    QStringList missingCapabilities;
    QStringList missingPlugins;
    QStringList recoveryHints;
};
```

```cpp
// Framework/Platform/Diagnostics/PlatformLifecycleTraceRecorder.h
class FRAMEWORK_EXPORT PlatformLifecycleTraceRecorder
{
public:
    void beginSession(PlatformRuntimeMode runtimeMode);
    void finishSession();
    void recordPhaseStarted(const QString& phaseKey, const QString& phaseLabel, bool blockingStartup);
    void recordPhaseFinished(
        const QString& phaseKey,
        const QString& phaseLabel,
        PlatformLifecycleResult result,
        const QString& detail,
        const QString& reasonCode = {});
    void recordPluginStepStarted(
        const QString& pluginId,
        const QString& ctkSymbolicName,
        PlatformLifecycleStep step,
        bool blockingStartup);
    void recordPluginStepFinished(
        const QString& pluginId,
        const QString& ctkSymbolicName,
        PlatformLifecycleStep step,
        PlatformLifecycleResult result,
        const QString& detail,
        const QString& reasonCode = {});
    QVector<PlatformLifecycleEvent> lifecycleEvents() const;
    QVector<PlatformStartupTraceEntry> startupTrace() const;
};
```

- [ ] **Step 4: 重新运行测试，确认通过**

Run:

```powershell
cmake --build build_x64_noctk --config Release --target platform_descriptor_loader_test platform_startup_trace_recorder_test
ctest --test-dir build_x64_noctk -C Release -R "platform_descriptor_loader_test|platform_startup_trace_recorder_test" --output-on-failure
```

Expected:

- `platform_descriptor_loader_test` PASS
- `platform_startup_trace_recorder_test` PASS

- [ ] **Step 5: 提交基础契约与 recorder**

```powershell
git add CMakeLists.txt Framework/Platform/Contracts/PlatformRuntimeTypes.h Framework/Platform/Contracts/PlatformPluginDescriptor.h Framework/Platform/Contracts/PlatformSnapshots.h Framework/Platform/Kernel/PlatformDescriptorLoader.cpp Framework/Platform/Diagnostics/PlatformLifecycleTraceRecorder.h Framework/Platform/Diagnostics/PlatformLifecycleTraceRecorder.cpp tests/unit/CMakeLists.txt tests/unit/PlatformDescriptorLoaderTest.cpp tests/unit/PlatformStartupTraceRecorderTest.cpp
git commit -m "feat: add startup lifecycle recorder contracts"
```

### Task 2: 把 session、phase、mode-skip 与 ready-path 事件接入启动主链

**Files:**
- Modify: `Framework/StartupOrchestrator.h`
- Modify: `Framework/StartupOrchestrator.cpp`
- Modify: `Framework/Platform/Kernel/PlatformStartupCoordinator.h`
- Modify: `Framework/Platform/Kernel/PlatformStartupCoordinator.cpp`
- Modify: `UI/MainInterfaceWidget.h`
- Modify: `UI/MainInterfaceWidget.cpp`
- Modify: `main.cpp`
- Modify: `Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.h`
- Modify: `Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.cpp`
- Modify: `tests/unit/PlatformStartupCoordinatorTest.cpp`

- [ ] **Step 1: 先扩展 coordinator 测试，锁定 mode-skip 与 ensureReady 事件**

```cpp
// tests/unit/PlatformStartupCoordinatorTest.cpp
void PlatformStartupCoordinatorTest::observe_only_records_skipped_start_event();
void PlatformStartupCoordinatorTest::facade_mode_records_successful_on_demand_start();
void PlatformStartupCoordinatorTest::orchestrate_core_records_deferred_start_path();
```

```cpp
void PlatformStartupCoordinatorTest::observe_only_records_skipped_start_event()
{
    PlatformLifecycleTraceRecorder recorder;
    recorder.beginSession(PlatformRuntimeMode::ObserveOnly);

    PlatformStartupCoordinator coordinator(
        PlatformRuntimeMode::ObserveOnly,
        [](const QString&) { return true; },
        &recorder);

    QVERIFY(!coordinator.ensureReady(QStringLiteral("org.medicalpro.optical_tracking")));

    const auto events = recorder.lifecycleEvents();
    QVERIFY(!events.isEmpty());
    QCOMPARE(events.last().result, PlatformLifecycleResult::Skipped);
    QCOMPARE(events.last().step, PlatformLifecycleStep::Start);
}
```

- [ ] **Step 2: 运行测试，确认失败**

Run:

```powershell
cmake --build build_x64_noctk --config Release --target platform_startup_coordinator_test
ctest --test-dir build_x64_noctk -C Release -R platform_startup_coordinator_test --output-on-failure
```

Expected:

- 编译失败，提示 `PlatformStartupCoordinator` 构造函数尚未接收 `PlatformLifecycleTraceRecorder*`
- 或断言失败，说明还没有记录 skip/success 事件

- [ ] **Step 3: 实现 recorder 接线、descriptor 初始化与 phase 事件**

```cpp
// Framework/StartupOrchestrator.h
class PlatformLifecycleTraceRecorder;

class FRAMEWORK_EXPORT StartupOrchestrator : public QObject, public SingletonManager<StartupOrchestrator>
{
public:
    void setLifecycleRecorder(PlatformLifecycleTraceRecorder* recorder);
    QVector<PlatformLifecycleEvent> getLifecycleEvents() const;

private:
    PlatformLifecycleTraceRecorder* m_lifecycleRecorder = nullptr;
};
```

```cpp
// Framework/Platform/Kernel/PlatformStartupCoordinator.h
class PlatformLifecycleTraceRecorder;

PlatformStartupCoordinator(
    PlatformRuntimeMode runtimeMode,
    StartPluginFn startPluginFn,
    PlatformLifecycleTraceRecorder* recorder = nullptr);
```

```cpp
// main.cpp
QStringList descriptorErrors;
const auto descriptors = PlatformDescriptorLoader::loadFromDirectory(descriptorDirectoryPath, &descriptorErrors);
if (!descriptorErrors.isEmpty()) {
    throw std::runtime_error(descriptorErrors.join("; ").toStdString());
}

auto mainInterface = new MainInterfaceWidget(nullptr);
mainInterface->platformStateStore()->replaceDescriptors(descriptors);
mainInterface->platformStateStore()->setRuntimeMode(runtimeConfig.runtimeMode);

auto* lifecycleRecorder = mainInterface->lifecycleTraceRecorder();
orchestrator->setLifecycleRecorder(lifecycleRecorder);

PlatformStartupCoordinator startupCoordinator(
    runtimeConfig.runtimeMode,
    [ctkManager](const QString& pluginName) { return ctkManager->startPlugin(pluginName); },
    lifecycleRecorder);
```

- [ ] **Step 4: 重新运行启动侧测试**

Run:

```powershell
cmake --build build_x64_noctk --config Release --target platform_startup_coordinator_test medicalpro
ctest --test-dir build_x64_noctk -C Release -R platform_startup_coordinator_test --output-on-failure
```

Expected:

- `platform_startup_coordinator_test` PASS
- `medicalpro` 编译通过
- `observe_only / facade_mode / orchestrate_core` 的 skip、success 与 deferred 路径事件已可写入 recorder

- [ ] **Step 5: 提交启动链 recorder 接线**

```powershell
git add Framework/StartupOrchestrator.h Framework/StartupOrchestrator.cpp Framework/Platform/Kernel/PlatformStartupCoordinator.h Framework/Platform/Kernel/PlatformStartupCoordinator.cpp Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.h Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.cpp UI/MainInterfaceWidget.h UI/MainInterfaceWidget.cpp main.cpp tests/unit/PlatformStartupCoordinatorTest.cpp
git commit -m "feat: wire lifecycle recorder into startup flow"
```

### Task 3: 落地 CTK install/start 事件桥接与 plugin lifecycle 聚合

**Files:**
- Modify: `Framework/CTKManager.h`
- Modify: `Framework/CTKManager.cpp`
- Modify: `Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.h`
- Modify: `Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.cpp`
- Modify: `Framework/Platform/Kernel/PlatformStateStore.h`
- Modify: `Framework/Platform/Kernel/PlatformStateStore.cpp`
- Create: `Framework/Platform/Diagnostics/PlatformPluginLifecycleAggregator.h`
- Create: `Framework/Platform/Diagnostics/PlatformPluginLifecycleAggregator.cpp`
- Modify: `Framework/Platform/Diagnostics/PlatformDiagnosticsService.h`
- Modify: `Framework/Platform/Diagnostics/PlatformDiagnosticsService.cpp`
- Modify: `tests/unit/CMakeLists.txt`
- Create: `tests/unit/PlatformPluginLifecycleAggregatorTest.cpp`
- Modify: `tests/unit/PlatformDiagnosticsServiceTest.cpp`
- Modify: `tests/unit/PlatformUiBridgeTest.cpp`

- [ ] **Step 1: 先注册聚合测试 target，并写 failing tests 锁定 lifecycle 聚合结果**

```cmake
# tests/unit/CMakeLists.txt
add_executable(platform_plugin_lifecycle_aggregator_test
    PlatformPluginLifecycleAggregatorTest.cpp
)

target_include_directories(platform_plugin_lifecycle_aggregator_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(platform_plugin_lifecycle_aggregator_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME platform_plugin_lifecycle_aggregator_test
    COMMAND platform_plugin_lifecycle_aggregator_test
)
```

```cpp
// tests/unit/PlatformPluginLifecycleAggregatorTest.cpp
void PlatformPluginLifecycleAggregatorTest::aggregates_install_start_service_ready_and_warmup();
void PlatformPluginLifecycleAggregatorTest::identifies_failed_plugin_and_blocking_point();
void PlatformPluginLifecycleAggregatorTest::separates_ready_path_from_warmup_tail();
```

```cpp
// tests/unit/PlatformDiagnosticsServiceTest.cpp
void PlatformDiagnosticsServiceTest::buildSnapshot_identifies_slowest_plugin_and_failure_point();
void PlatformDiagnosticsServiceTest::buildSnapshot_detects_ctk_platform_state_mismatch();
```

- [ ] **Step 2: 运行测试，确认失败**

Run:

```powershell
cmake --build build_x64_noctk --config Release --target platform_plugin_lifecycle_aggregator_test platform_diagnostics_service_test
ctest --test-dir build_x64_noctk -C Release -R "platform_plugin_lifecycle_aggregator_test|platform_diagnostics_service_test" --output-on-failure
```

Expected:

- `PlatformPluginLifecycleAggregator` 尚不存在，编译失败
- `PlatformDiagnosticsServiceTest` 因 `summary / pluginLifecycle / problems` 字段未实现而失败

- [ ] **Step 3: 实现 CTK 信号桥接、runtime observation 扩展与 aggregator**

```cpp
// Framework/CTKManager.h
signals:
    void pluginInstalled(const QString& pluginName, const QString& pluginPath);
    void pluginInstallFailedDetailed(const QString& pluginName, const QString& pluginPath, const QString& reason);
    void pluginStartedDetailed(const QString& pluginName);
    void pluginStartFailedDetailed(const QString& pluginName, const QString& reason);
```

```cpp
// Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.h
struct FRAMEWORK_EXPORT PlatformRuntimeObservation
{
    bool frameworkReady = false;
    QStringList installedPlugins;
    QStringList startedPlugins;
    QStringList loadedPlugins;
    QMap<QString, QString> pluginStates;
    QVector<PlatformLifecycleEvent> lifecycleEvents;
    QVector<PlatformStartupTraceEntry> startupTrace;
};
```

```cpp
// Framework/Platform/Diagnostics/PlatformPluginLifecycleAggregator.h
struct PlatformPluginLifecycleAggregation
{
    PlatformDiagnosticSummary summary;
    QVector<PlatformPluginLifecycleSnapshot> pluginLifecycle;
    QVector<PlatformDiagnosticProblem> problems;
    QStringList recoveryHints;
};

class PlatformPluginLifecycleAggregator
{
public:
    PlatformPluginLifecycleAggregation aggregate(
        const QVector<PlatformLifecycleEvent>& lifecycleEvents,
        const QVector<PlatformPluginDescriptor>& descriptors,
        const PlatformRuntimeObservation& observation) const;
};
```

```cpp
// Framework/Platform/Diagnostics/PlatformDiagnosticsService.cpp
PlatformDiagnosticSnapshot PlatformDiagnosticsService::buildSnapshot(const PlatformRuntimeObservation& observation) const
{
    PlatformDiagnosticSnapshot snapshot;
    snapshot.startupTrace = observation.startupTrace;
    const auto descriptors = m_stateStore ? m_stateStore->descriptors() : QVector<PlatformPluginDescriptor>{};
    const auto aggregation = m_aggregator.aggregate(observation.lifecycleEvents, descriptors, observation);
    snapshot.summary = aggregation.summary;
    snapshot.pluginLifecycle = aggregation.pluginLifecycle;
    snapshot.problems = aggregation.problems;
    snapshot.recoveryHints = aggregation.recoveryHints;
    snapshot.capabilitySnapshot = m_stateStore ? m_stateStore->capabilitySnapshot() : PlatformCapabilitySnapshot{};
    return snapshot;
}
```

- [ ] **Step 4: 运行聚合与 diagnostics 服务测试**

Run:

```powershell
cmake --build build_x64_noctk --config Release --target platform_plugin_lifecycle_aggregator_test platform_diagnostics_service_test platform_ui_bridge_test
ctest --test-dir build_x64_noctk -C Release -R "platform_plugin_lifecycle_aggregator_test|platform_diagnostics_service_test|platform_ui_bridge_test" --output-on-failure
```

Expected:

- `platform_plugin_lifecycle_aggregator_test` PASS
- `platform_diagnostics_service_test` PASS
- `platform_ui_bridge_test` PASS

- [ ] **Step 5: 提交 lifecycle 聚合与 CTK 桥接**

```powershell
git add Framework/CTKManager.h Framework/CTKManager.cpp Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.h Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.cpp Framework/Platform/Kernel/PlatformStateStore.h Framework/Platform/Kernel/PlatformStateStore.cpp Framework/Platform/Diagnostics/PlatformPluginLifecycleAggregator.h Framework/Platform/Diagnostics/PlatformPluginLifecycleAggregator.cpp Framework/Platform/Diagnostics/PlatformDiagnosticsService.h Framework/Platform/Diagnostics/PlatformDiagnosticsService.cpp tests/unit/CMakeLists.txt tests/unit/PlatformPluginLifecycleAggregatorTest.cpp tests/unit/PlatformDiagnosticsServiceTest.cpp tests/unit/PlatformUiBridgeTest.cpp
git commit -m "feat: aggregate plugin lifecycle diagnostics"
```

### Task 4: 扩展 diagnostics page 摘要、问题列表与排序

**Files:**
- Modify: `UI/Forms/PlatformDiagnosticsPage.ui`
- Modify: `UI/NewPages/PlatformDiagnosticsPage.h`
- Modify: `UI/NewPages/PlatformDiagnosticsPage.cpp`
- Modify: `UI/MainInterfaceWidget.cpp`
- Modify: `tests/unit/PlatformDiagnosticsPageTest.cpp`

- [ ] **Step 1: 先写 failing page test，锁定摘要卡、问题列表、时间线字段和排序**

```cpp
// tests/unit/PlatformDiagnosticsPageTest.cpp
void PlatformDiagnosticsPageTest::refreshSnapshot_renders_summary_problem_and_sorted_plugins();
void PlatformDiagnosticsPageTest::refreshSnapshot_renders_timeline_with_blocking_flag();
```

```cpp
void PlatformDiagnosticsPageTest::refreshSnapshot_renders_summary_problem_and_sorted_plugins()
{
    PlatformDiagnosticSnapshot snapshot;
    snapshot.summary.runtimeMode = PlatformRuntimeMode::OrchestrateCore;
    snapshot.summary.frameworkReady = true;
    snapshot.summary.platformReady = false;
    snapshot.summary.startupReadyPathMs = 820;
    snapshot.summary.slowestPluginId = QStringLiteral("org.medicalpro.dicom_viewer");
    snapshot.summary.blockingSpanLabel = QStringLiteral("DicomViewer service ready");

    PlatformPluginLifecycleSnapshot failedPlugin;
    failedPlugin.pluginId = QStringLiteral("org.medicalpro.dicom_viewer");
    failedPlugin.state = PlatformPluginState::Failed;
    failedPlugin.blockingMs = 820;

    PlatformPluginLifecycleSnapshot readyPlugin;
    readyPlugin.pluginId = QStringLiteral("org.medicalpro.user_management");
    readyPlugin.state = PlatformPluginState::Ready;
    readyPlugin.blockingMs = 120;

    snapshot.pluginLifecycle = {readyPlugin, failedPlugin};

    PlatformDiagnosticProblem problem;
    problem.reasonCode = QStringLiteral("service_ready_timeout");
    problem.detail = QStringLiteral("DicomViewerService missing");
    snapshot.problems.append(problem);

    PlatformDiagnosticsPage page(nullptr, [snapshot]() { return snapshot; });
    page.refreshSnapshot();

    QCOMPARE(page.findChild<QLabel*>(QStringLiteral("startupReadyPathValueLabel"))->text(), QStringLiteral("820 ms"));
    QCOMPARE(page.findChild<QTableWidget*>(QStringLiteral("problemTableWidget"))->rowCount(), 1);
    QCOMPARE(page.findChild<QTableWidget*>(QStringLiteral("pluginTableWidget"))->item(0, 0)->text(), QStringLiteral("org.medicalpro.dicom_viewer"));
}
```

- [ ] **Step 2: 运行测试，确认失败**

Run:

```powershell
cmake --build build_x64_noctk --config Release --target platform_diagnostics_page_test
ctest --test-dir build_x64_noctk -C Release -R platform_diagnostics_page_test --output-on-failure
```

Expected:

- 测试因缺少摘要 label、problem table、timeline 字段或插件排序逻辑而失败

- [ ] **Step 3: 更新 UI 和页面渲染逻辑**

```cpp
// UI/NewPages/PlatformDiagnosticsPage.h
void populatePluginTable(const QVector<PlatformPluginLifecycleSnapshot>& plugins);
void populateTraceTable(const QVector<PlatformStartupTraceEntry>& startupTrace);
void populateProblemTable(const QVector<PlatformDiagnosticProblem>& problems);
void applyPluginSorting(QVector<PlatformPluginLifecycleSnapshot>* plugins) const;
```

```cpp
// UI/NewPages/PlatformDiagnosticsPage.cpp
ui->runtimeModeValueLabel->setText(runtimeModeText(snapshot.summary.runtimeMode));
ui->frameworkReadyValueLabel->setText(snapshot.summary.frameworkReady ? QStringLiteral("已就绪") : QStringLiteral("未就绪"));
ui->platformReadyValueLabel->setText(snapshot.summary.platformReady ? QStringLiteral("已就绪") : QStringLiteral("未就绪"));
ui->startupReadyPathValueLabel->setText(QStringLiteral("%1 ms").arg(snapshot.summary.startupReadyPathMs));
ui->blockingPointValueLabel->setText(snapshot.summary.blockingSpanLabel);
populateProblemTable(snapshot.problems);
populateTraceTable(snapshot.startupTrace);
```

```xml
<!-- UI/Forms/PlatformDiagnosticsPage.ui -->
<widget class="QLabel" name="frameworkReadyValueLabel"/>
<widget class="QLabel" name="platformReadyValueLabel"/>
<widget class="QLabel" name="startupReadyPathValueLabel"/>
<widget class="QLabel" name="warmupTailValueLabel"/>
<widget class="QLabel" name="blockingPointValueLabel"/>
<widget class="QLabel" name="failurePointValueLabel"/>
<widget class="QTableWidget" name="problemTableWidget"/>
```

- [ ] **Step 4: 运行页面与解耦验收**

Run:

```powershell
cmake --build build_x64_noctk --config Release --target platform_diagnostics_page_test ui_ctk_decoupling_acceptance_test
ctest --test-dir build_x64_noctk -C Release -R "platform_diagnostics_page_test|ui_ctk_decoupling_acceptance_test" --output-on-failure
rg -n "CTKManager::instance\(|getService<" UI\NewPages UI\MainInterfaceWidget.cpp
```

Expected:

- `platform_diagnostics_page_test` PASS
- `ui_ctk_decoupling_acceptance_test` PASS
- `rg` 无输出

- [ ] **Step 5: 提交 diagnostics page 增强**

```powershell
git add UI/Forms/PlatformDiagnosticsPage.ui UI/NewPages/PlatformDiagnosticsPage.h UI/NewPages/PlatformDiagnosticsPage.cpp UI/MainInterfaceWidget.cpp tests/unit/PlatformDiagnosticsPageTest.cpp
git commit -m "feat: expand diagnostics page for lifecycle analysis"
```

### Task 5: 回写治理文档并完成专项验收

**Files:**
- Modify: `docs/superpowers/specs/2026-04-16-platform-kernel-governance-design.md`
- Modify: `docs/superpowers/tracking/platform-plugin-governance-matrix.md`
- Modify: `docs/superpowers/tracking/platform-migration-decision-log.md`
- Modify: `docs/current_status_and_project_overview.md`

- [ ] **Step 1: 回写治理文档**

```md
## Startup Diagnostics Follow-up

- `StartupTrace` 已从阶段级 trace 扩展为生命周期时间线
- `ready-path` 与 `warmup-tail` 在平台诊断中分离展示
- 新增 `PlatformLifecycleEvent` 账本与 `PlatformPluginLifecycleSnapshot` 聚合输出
```

```md
| Plugin | Required Service | Service Ready Timeout | Warmup Task | Warmup Failure Impact |
| --- | --- | --- | --- | --- |
| UserManagement | `UserManagementService` | 3000 ms | `session_cache` | warning |
| DicomViewer | `DicomViewerService` | 5000 ms | `data_path_precheck` | degraded |
| FourViewDisplay | `FourViewDisplayService` | 5000 ms | `render_backend_warmup` | degraded |
| RegistrationCore | `RegistrationService` | 5000 ms | `core_binary_probe` | degraded |
| OpticalTracking | `InstrumentManagementService` | 5000 ms | `adapter_probe` | degraded |
```

- [ ] **Step 2: 执行专项验收命令**

Run:

```powershell
cmake --build build_x64_noctk --config Release --target medicalpro platform_startup_trace_recorder_test platform_plugin_lifecycle_aggregator_test platform_startup_coordinator_test platform_diagnostics_service_test platform_diagnostics_page_test platform_ui_bridge_test ui_ctk_decoupling_acceptance_test
ctest --test-dir build_x64_noctk -C Release -R "platform_startup_trace_recorder_test|platform_plugin_lifecycle_aggregator_test|platform_startup_coordinator_test|platform_diagnostics_service_test|platform_diagnostics_page_test|platform_ui_bridge_test|ui_ctk_decoupling_acceptance_test" --output-on-failure
rg -n "CTKManager::instance\(|getService<" UI\NewPages UI\MainInterfaceWidget.cpp
```

Expected:

- 所有专项测试 PASS
- `medicalpro` 编译通过
- `rg` 无输出

- [ ] **Step 3: 提交文档与验收结果**

```powershell
git add docs/current_status_and_project_overview.md docs/superpowers/specs/2026-04-16-platform-kernel-governance-design.md docs/superpowers/tracking/platform-plugin-governance-matrix.md docs/superpowers/tracking/platform-migration-decision-log.md
git commit -m "docs: finalize startup lifecycle diagnostics rollout"
```

## Coverage Review

- `StartupTrace` 扩展为可量化时间线：Task 1
- `install / start / service ready / warmup / failed / degraded / skipped_by_mode` 事件模型：Task 1、Task 2、Task 3
- `PlatformDiagnosticsService` 聚合与 `最慢插件 / 阻塞点 / 失败点 / 恢复建议`：Task 3
- `PlatformDiagnosticsPage` 字段、列表、状态、排序：Task 4
- 三种运行模式对齐：Task 2、Task 3
- 测试、验收命令、文档回写：Task 1 到 Task 5

## Review Notes

- 本计划已把新测试 target 注册前置到首轮红灯之前，避免出现“target 不存在”型假失败。
- 红旗词自检已在写计划阶段人工完成，文档内不再嵌入会自匹配的扫描命令。
- 命名统一使用：`PlatformLifecycleTraceRecorder`、`PlatformPluginLifecycleAggregator`、`PlatformLifecycleEvent`、`PlatformPluginLifecycleSnapshot`、`PlatformDiagnosticProblem`、`PlatformDiagnosticSummary`。
