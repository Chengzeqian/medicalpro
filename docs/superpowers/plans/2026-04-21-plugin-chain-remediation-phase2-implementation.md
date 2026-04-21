# Plugin Chain Remediation Phase 2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `RegistrationCore` 与 `OpticalTracking` 的按需启动链路收口为 `descriptor-driven on-demand activation + strict ensureReady + governed diagnostics` 的单一真相路径。

**Architecture:** 在 `Framework/Platform/Kernel` 下新增一套独立于 Phase 1 主启动 plan 的 `PlatformOnDemandActivationPlanBuilder` 与 `PlatformOnDemandActivationService`。`PlatformStartupCoordinator` 继续作为统一执行器，扩展出 on-demand `install / start / service_ready / health_check` 路径；`LegacyNavigationAdapter` 改为治理链代理，`MainInterfaceWidget` 不再自行裸建导航适配器，而是复用 `main.cpp` 已建立的治理上下文。

**Tech Stack:** Qt 6、QtTest、Qt Widgets、CTK Plugin Framework、CMake、现有 `Framework/Platform` 治理层、`tests/unit`、`tests/runtime`

---

## Files and Responsibilities

- Modify: `CMakeLists.txt`
  - 把 `PlatformOnDemandActivationPlan*` 与 `PlatformOnDemandActivationService*` 编入 `Framework`
- Create: `Framework/Platform/Kernel/PlatformOnDemandActivationPlan.h`
- Create: `Framework/Platform/Kernel/PlatformOnDemandActivationPlan.cpp`
  - 负责 target plugin descriptor 校验、bundle path 解析、依赖补齐、on-demand activation plan 生成
- Create: `Framework/Platform/Kernel/PlatformOnDemandActivationService.h`
- Create: `Framework/Platform/Kernel/PlatformOnDemandActivationService.cpp`
  - 负责 `ensureReady(plugin_id)` 的正式治理执行入口，串起 plan builder、coordinator、state write-back
- Modify: `Framework/Platform/Kernel/PlatformStartupCoordinator.h`
- Modify: `Framework/Platform/Kernel/PlatformStartupCoordinator.cpp`
  - 新增 on-demand activation 执行入口、`already_ready` 短路、health check 执行与三档 mode 语义
- Modify: `Framework/Platform/Kernel/PlatformStateStore.h`
- Modify: `Framework/Platform/Kernel/PlatformStateStore.cpp`
  - 把 `startup scope` 与 `governed scope` 拆开，避免按需插件污染 Phase 1 `platformReady`
- Modify: `Framework/Platform/Contracts/PlatformSnapshots.h`
  - 为 capability/diagnostics snapshot 增加 `startupScopePluginIds` 与 `governedPluginIds`
- Modify: `Framework/Platform/Diagnostics/PlatformDiagnosticsService.cpp`
  - 使用 governed scope 汇总 excluded/on-demand diagnostics，同时保留 startup scope 的 ready 口径
- Modify: `Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.h`
- Modify: `Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.cpp`
  - 从旧 CTK 直启适配器改为 on-demand activation service 代理
- Modify: `UI/MainInterfaceWidget.h`
- Modify: `UI/MainInterfaceWidget.cpp`
  - 通过构造注入 `INavigationFacadePort`，不再内部 `new LegacyNavigationAdapter()`
- Modify: `main.cpp`
  - 在主治理上下文中组装 `PlatformOnDemandActivationService` 与 `LegacyNavigationAdapter`，并把 governed scope 写入 `PlatformStateStore`
- Modify: `Plugins/RegistrationCore/platform/plugin.json`
- Modify: `Plugins/OpticalTracking/platform/plugin.json`
  - 补齐 `diagnostics.required_services` 与 `diagnostics.service_ready_timeout_ms`
- Modify: `tests/unit/CMakeLists.txt`
  - 注册 `platform_on_demand_activation_plan_test`
- Create: `tests/unit/PlatformOnDemandActivationPlanTest.cpp`
  - 覆盖 on-demand plan 生成、descriptor 契约失败、bundle path 解析失败、依赖补齐
- Modify: `tests/unit/PlatformStartupCoordinatorTest.cpp`
  - 覆盖 `observe_only` skip、`facade_mode`/`orchestrate_core` on-demand activation、`already_ready`、timeout、health check fail
- Modify: `tests/unit/PlatformDependencyGraphTest.cpp`
  - 覆盖 startup scope 不被 governed on-demand capability 污染
- Modify: `tests/unit/PlatformDiagnosticsServiceTest.cpp`
  - 覆盖 governed scope 出现在 diagnostics 且不污染 `platformReady`
- Modify: `tests/unit/PlatformFacadesTest.cpp`
  - 覆盖 `LegacyNavigationAdapter` 不再直接 CTK start，而是代理到 activation service
- Modify: `docs/current_status_and_project_overview.md`
  - 回写 Phase 2 按需治理 acceptance
- Modify: `docs/superpowers/tracking/platform-migration-decision-log.md`
  - 记录 `ensureReady()` 改为严格治理语义
- Modify: `docs/superpowers/specs/2026-04-16-platform-kernel-governance-design.md`
  - 追加 Phase 2 landed note

### Task 1: 新增 on-demand activation plan builder 并补齐 descriptor 契约

**Files:**
- Modify: `CMakeLists.txt`
- Create: `Framework/Platform/Kernel/PlatformOnDemandActivationPlan.h`
- Create: `Framework/Platform/Kernel/PlatformOnDemandActivationPlan.cpp`
- Modify: `Plugins/RegistrationCore/platform/plugin.json`
- Modify: `Plugins/OpticalTracking/platform/plugin.json`
- Modify: `tests/unit/CMakeLists.txt`
- Create: `tests/unit/PlatformOnDemandActivationPlanTest.cpp`

- [x] **Step 1: 先注册 failing test target，并写 RED 用例锁定 on-demand plan 契约**

```cmake
# tests/unit/CMakeLists.txt
add_executable(platform_on_demand_activation_plan_test
    PlatformOnDemandActivationPlanTest.cpp
)

target_include_directories(platform_on_demand_activation_plan_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(platform_on_demand_activation_plan_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME platform_on_demand_activation_plan_test
    COMMAND platform_on_demand_activation_plan_test
)

set_tests_properties(platform_on_demand_activation_plan_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:platform_on_demand_activation_plan_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)
```

```cpp
// tests/unit/PlatformOnDemandActivationPlanTest.cpp
#include <QtTest/QtTest>

#include "Framework/Platform/Kernel/PlatformOnDemandActivationPlan.h"

namespace
{
PlatformPluginDescriptor makeDescriptor(
    const QString& pluginId,
    const QString& displayName,
    const QString& ctkSymbolicName,
    PlatformStartupPolicy startupPolicy = PlatformStartupPolicy::OnDemand,
    PlatformBootstrapLevel bootstrapLevel = PlatformBootstrapLevel::Deferred,
    const QStringList& providedCapabilities = {},
    const QStringList& requiredPlugins = {},
    const QStringList& requiredCapabilities = {})
{
    PlatformPluginDescriptor descriptor;
    descriptor.id = pluginId;
    descriptor.version = QStringLiteral("1.0.0");
    descriptor.displayName = displayName;
    descriptor.domain = QStringLiteral("navigation");
    descriptor.runtime.ctkSymbolicName = ctkSymbolicName;
    descriptor.runtime.startupPolicy = startupPolicy;
    descriptor.runtime.bootstrapLevel = bootstrapLevel;
    descriptor.provides.capabilities = providedCapabilities;
    descriptor.required.plugins = requiredPlugins;
    descriptor.required.capabilities = requiredCapabilities;
    descriptor.diagnostics.requiredServices = QStringList{QStringLiteral("%1.service").arg(pluginId)};
    descriptor.diagnostics.serviceReadyTimeoutMs = 5000;
    descriptor.healthChecks = QStringList{QStringLiteral("service_registered")};
    return descriptor;
}
}

class PlatformOnDemandActivationPlanTest : public QObject
{
    Q_OBJECT

private slots:
    void build_returns_target_activation_entry();
    void build_adds_required_plugin_before_target();
    void build_rejects_missing_diagnostics_contract();
    void build_rejects_missing_bundle_path();
};

void PlatformOnDemandActivationPlanTest::build_returns_target_activation_entry()
{
    QTemporaryDir pluginDir;
    QVERIFY(pluginDir.isValid());
    QVERIFY(QFile(pluginDir.filePath(QStringLiteral("RegistrationCore.dll"))).open(QIODevice::WriteOnly));

    QString error;
    const auto plan = PlatformOnDemandActivationPlanBuilder::build(
        QStringLiteral("org.medicalpro.registration_core"),
        {
            makeDescriptor(
                QStringLiteral("org.medicalpro.registration_core"),
                QStringLiteral("RegistrationCore"),
                QStringLiteral("RegistrationCore"),
                PlatformStartupPolicy::OnDemand,
                PlatformBootstrapLevel::Deferred,
                {QStringLiteral("navigation.registration")},
                {},
                {QStringLiteral("imaging.data")})
        },
        pluginDir.path(),
        &error);

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(plan.targetPluginId, QStringLiteral("org.medicalpro.registration_core"));
    QCOMPARE(plan.activationEntries.size(), 1);
    QVERIFY(plan.activationEntries.constFirst().target);
    QVERIFY(plan.activationEntries.constFirst().bundleFilePath.endsWith(QStringLiteral("RegistrationCore.dll")));
}

void PlatformOnDemandActivationPlanTest::build_adds_required_plugin_before_target()
{
    QTemporaryDir pluginDir;
    QVERIFY(pluginDir.isValid());
    QVERIFY(QFile(pluginDir.filePath(QStringLiteral("NavigationSupport.dll"))).open(QIODevice::WriteOnly));
    QVERIFY(QFile(pluginDir.filePath(QStringLiteral("RegistrationCore.dll"))).open(QIODevice::WriteOnly));

    QString error;
    const auto plan = PlatformOnDemandActivationPlanBuilder::build(
        QStringLiteral("org.medicalpro.registration_core"),
        {
            makeDescriptor(
                QStringLiteral("org.medicalpro.navigation_support"),
                QStringLiteral("NavigationSupport"),
                QStringLiteral("NavigationSupport")),
            makeDescriptor(
                QStringLiteral("org.medicalpro.registration_core"),
                QStringLiteral("RegistrationCore"),
                QStringLiteral("RegistrationCore"),
                PlatformStartupPolicy::OnDemand,
                PlatformBootstrapLevel::Deferred,
                {QStringLiteral("navigation.registration")},
                {QStringLiteral("org.medicalpro.navigation_support")})
        },
        pluginDir.path(),
        &error);

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(plan.activationEntries.size(), 2);
    QCOMPARE(plan.activationEntries.at(0).pluginId, QStringLiteral("org.medicalpro.navigation_support"));
    QCOMPARE(plan.activationEntries.at(1).pluginId, QStringLiteral("org.medicalpro.registration_core"));
    QVERIFY(plan.activationEntries.at(1).target);
}

void PlatformOnDemandActivationPlanTest::build_rejects_missing_diagnostics_contract()
{
    QTemporaryDir pluginDir;
    QVERIFY(pluginDir.isValid());
    QVERIFY(QFile(pluginDir.filePath(QStringLiteral("OpticalTracking.dll"))).open(QIODevice::WriteOnly));

    auto descriptor = makeDescriptor(
        QStringLiteral("org.medicalpro.optical_tracking"),
        QStringLiteral("OpticalTracking"),
        QStringLiteral("OpticalTracking"));
    descriptor.diagnostics.requiredServices.clear();
    descriptor.diagnostics.serviceReadyTimeoutMs = 0;

    QString error;
    const auto plan = PlatformOnDemandActivationPlanBuilder::build(
        QStringLiteral("org.medicalpro.optical_tracking"),
        { descriptor },
        pluginDir.path(),
        &error);

    QVERIFY(plan.activationEntries.isEmpty());
    QVERIFY(error.contains(QStringLiteral("diagnostics")));
}

void PlatformOnDemandActivationPlanTest::build_rejects_missing_bundle_path()
{
    QString error;
    const auto plan = PlatformOnDemandActivationPlanBuilder::build(
        QStringLiteral("org.medicalpro.registration_core"),
        {
            makeDescriptor(
                QStringLiteral("org.medicalpro.registration_core"),
                QStringLiteral("RegistrationCore"),
                QStringLiteral("RegistrationCore"))
        },
        QStringLiteral("C:/missing/plugins"),
        &error);

    QVERIFY(plan.activationEntries.isEmpty());
    QVERIFY(error.contains(QStringLiteral("bundle")));
}

QTEST_APPLESS_MAIN(PlatformOnDemandActivationPlanTest)
#include "PlatformOnDemandActivationPlanTest.moc"
```

- [x] **Step 2: 运行新测试，确认当前先红灯**

Run:

```powershell
cmake --build build_x64 --config Release --target platform_on_demand_activation_plan_test
ctest --test-dir build_x64 -C Release -R platform_on_demand_activation_plan_test --output-on-failure
```

Expected:

- `platform_on_demand_activation_plan_test` 编译失败，因为 `PlatformOnDemandActivationPlanBuilder` 还不存在

- [x] **Step 3: 实现 on-demand plan builder，并接入 Framework 编译**

```cpp
// Framework/Platform/Kernel/PlatformOnDemandActivationPlan.h
#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/PlatformPluginDescriptor.h"

#include <QVector>

struct PlatformOnDemandActivationPlanEntry
{
    QString pluginId;
    QString displayName;
    QString ctkSymbolicName;
    QString bundleFilePath;
    QStringList requiredPlugins;
    QStringList requiredCapabilities;
    QStringList requiredServices;
    QStringList healthChecks;
    int serviceReadyTimeoutMs = 0;
    bool target = false;
};

struct PlatformOnDemandActivationPlan
{
    QString targetPluginId;
    QVector<PlatformOnDemandActivationPlanEntry> activationEntries;
};

class FRAMEWORK_EXPORT PlatformOnDemandActivationPlanBuilder
{
public:
    static PlatformOnDemandActivationPlan build(
        const QString& targetPluginId,
        const QVector<PlatformPluginDescriptor>& descriptors,
        const QString& pluginDirectory,
        QString* error = nullptr);
};
```

```cpp
// Framework/Platform/Kernel/PlatformOnDemandActivationPlan.cpp
#include "Framework/Platform/Kernel/PlatformOnDemandActivationPlan.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSet>

namespace
{
bool hasStrictOnDemandContract(const PlatformPluginDescriptor& descriptor, QString* error)
{
    if (descriptor.runtime.ctkSymbolicName.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("descriptor_missing: runtime.ctk_symbolic_name");
        return false;
    }
    if (descriptor.runtime.startupPolicy != PlatformStartupPolicy::OnDemand) {
        if (error) *error = QStringLiteral("diagnostics_contract_missing: startup_policy must be on_demand");
        return false;
    }
    if (descriptor.runtime.bootstrapLevel != PlatformBootstrapLevel::Deferred) {
        if (error) *error = QStringLiteral("diagnostics_contract_missing: bootstrap_level must be deferred");
        return false;
    }
    if (descriptor.diagnostics.requiredServices.isEmpty()) {
        if (error) *error = QStringLiteral("diagnostics_contract_missing: required_services");
        return false;
    }
    if (descriptor.diagnostics.serviceReadyTimeoutMs <= 0) {
        if (error) *error = QStringLiteral("diagnostics_contract_missing: service_ready_timeout_ms");
        return false;
    }
    if (descriptor.healthChecks.isEmpty()) {
        if (error) *error = QStringLiteral("diagnostics_contract_missing: health_checks");
        return false;
    }
    return true;
}
}
```

```diff
# CMakeLists.txt
@@
     Framework/Platform/Kernel/PlatformStartupCoordinator.h
     Framework/Platform/Kernel/PlatformStartupCoordinator.cpp
+    Framework/Platform/Kernel/PlatformOnDemandActivationPlan.h
+    Framework/Platform/Kernel/PlatformOnDemandActivationPlan.cpp
     Framework/Platform/Kernel/PlatformWarmupCoordinator.h
     Framework/Platform/Kernel/PlatformWarmupCoordinator.cpp
```

- [x] **Step 4: 补齐 `RegistrationCore` 与 `OpticalTracking` descriptor 的 diagnostics 契约**

```json
// Plugins/RegistrationCore/platform/plugin.json
{
  "id": "org.medicalpro.registration_core",
  "version": "1.0.0",
  "display_name": "RegistrationCore",
  "domain": "navigation",
  "enabled": true,
  "runtime": {
    "ctk_symbolic_name": "RegistrationCore",
    "startup_policy": "on_demand",
    "bootstrap_level": "deferred",
    "entry_capability": "navigation.registration"
  },
  "diagnostics": {
    "required_services": ["RegistrationService"],
    "service_ready_timeout_ms": 5000,
    "warmup_tasks": [],
    "warmup_timeout_ms": 0,
    "warmup_impacts_ready": false,
    "degrade_on": []
  },
  "provides": {
    "services": ["RegistrationService", "navigation.registration"],
    "capabilities": ["navigation.registration"]
  },
  "requires": {
    "services": [],
    "capabilities": ["imaging.data"],
    "plugins": []
  },
  "optional": {
    "services": [],
    "capabilities": [],
    "plugins": []
  },
  "health_checks": ["service_registered", "core_binary_accessible"]
}
```

```json
// Plugins/OpticalTracking/platform/plugin.json
{
  "id": "org.medicalpro.optical_tracking",
  "version": "1.0.0",
  "display_name": "OpticalTracking",
  "domain": "navigation",
  "enabled": true,
  "runtime": {
    "ctk_symbolic_name": "OpticalTracking",
    "startup_policy": "on_demand",
    "bootstrap_level": "deferred",
    "entry_capability": "navigation.tracking"
  },
  "diagnostics": {
    "required_services": ["OpticalTrackingService", "navigation.tracking"],
    "service_ready_timeout_ms": 5000,
    "warmup_tasks": [],
    "warmup_timeout_ms": 0,
    "warmup_impacts_ready": false,
    "degrade_on": []
  },
  "provides": {
    "services": ["OpticalTrackingService", "navigation.tracking"],
    "capabilities": ["navigation.tracking"]
  },
  "requires": {
    "services": [],
    "capabilities": [],
    "plugins": []
  },
  "optional": {
    "services": [],
    "capabilities": [],
    "plugins": []
  },
  "health_checks": ["service_registered", "tracking_adapter_accessible"]
}
```

- [x] **Step 5: 重新运行 plan builder 测试并提交**

Run:

```powershell
cmake --build build_x64 --config Release --target platform_on_demand_activation_plan_test platform_descriptor_runtime_layout_test
ctest --test-dir build_x64 -C Release -R "platform_on_demand_activation_plan_test|platform_descriptor_runtime_layout_test" --output-on-failure
```

Expected:

- `platform_on_demand_activation_plan_test` PASS
- `platform_descriptor_runtime_layout_test` PASS

```powershell
git add CMakeLists.txt Framework/Platform/Kernel/PlatformOnDemandActivationPlan.h Framework/Platform/Kernel/PlatformOnDemandActivationPlan.cpp Plugins/RegistrationCore/platform/plugin.json Plugins/OpticalTracking/platform/plugin.json tests/unit/CMakeLists.txt tests/unit/PlatformOnDemandActivationPlanTest.cpp
git commit -m "feat: add governed on-demand activation plan"
```

### Task 2: 扩展 `PlatformStartupCoordinator` 执行严格的 on-demand activation

**Files:**
- Modify: `Framework/Platform/Kernel/PlatformStartupCoordinator.h`
- Modify: `Framework/Platform/Kernel/PlatformStartupCoordinator.cpp`
- Modify: `tests/unit/PlatformStartupCoordinatorTest.cpp`

- [x] **Step 1: 先补 RED 测试，锁定三档模式、`already_ready` 和 health check 行为**

```cpp
// tests/unit/PlatformStartupCoordinatorTest.cpp
private slots:
    void observe_only_on_demand_activation_reports_skip();
    void facade_mode_on_demand_activation_runs_install_start_ready_and_health_checks();
    void orchestrate_core_on_demand_activation_reuses_same_governed_path();
    void on_demand_activation_short_circuits_when_target_is_already_ready();
    void on_demand_activation_fails_when_health_check_fails();
```

```cpp
void PlatformStartupCoordinatorTest::observe_only_on_demand_activation_reports_skip()
{
    PlatformLifecycleTraceRecorder recorder;
    recorder.beginSession(PlatformRuntimeMode::ObserveOnly);

    PlatformOnDemandActivationPlan plan;
    plan.targetPluginId = QStringLiteral("org.medicalpro.registration_core");
    plan.activationEntries = {
        {
            QStringLiteral("org.medicalpro.registration_core"),
            QStringLiteral("RegistrationCore"),
            QStringLiteral("RegistrationCore"),
            QStringLiteral("C:/runtime/plugins/RegistrationCore.dll"),
            {},
            {QStringLiteral("imaging.data")},
            {QStringLiteral("RegistrationService")},
            {QStringLiteral("service_registered"), QStringLiteral("core_binary_accessible")},
            5000,
            true
        }
    };

    PlatformStartupCoordinator coordinator(PlatformRuntimeMode::ObserveOnly, {}, {}, &recorder);
    const auto outcome = coordinator.activateOnDemand(
        plan,
        {},
        {
            [](const QString&) { return PlatformPluginState::Discovered; },
            [](const QStringList&) { return QStringList{}; },
            [](const QString&) { return QStringList{}; },
            [](const QString&) { return QStringList{}; },
            [](const QString&, const QStringList&) { return QVector<PlatformHealthCheckResult>{}; }
        });

    QVERIFY(!outcome.success);
    QCOMPARE(outcome.reasonCode, QStringLiteral("skipped_by_mode"));
}

void PlatformStartupCoordinatorTest::on_demand_activation_short_circuits_when_target_is_already_ready()
{
    PlatformOnDemandActivationPlan plan;
    plan.targetPluginId = QStringLiteral("org.medicalpro.registration_core");
    plan.activationEntries = {
        {
            QStringLiteral("org.medicalpro.registration_core"),
            QStringLiteral("RegistrationCore"),
            QStringLiteral("RegistrationCore"),
            QStringLiteral("C:/runtime/plugins/RegistrationCore.dll"),
            {},
            {},
            {QStringLiteral("RegistrationService")},
            {QStringLiteral("service_registered")},
            5000,
            true
        }
    };

    PlatformStartupCoordinator coordinator(PlatformRuntimeMode::FacadeMode, {}, {});
    const auto outcome = coordinator.activateOnDemand(
        plan,
        {},
        {
            [](const QString&) { return PlatformPluginState::Ready; },
            [](const QStringList&) { return QStringList{}; },
            [](const QString&) { return QStringList{}; },
            [](const QString&) { return QStringList{}; },
            [](const QString&, const QStringList&) { return QVector<PlatformHealthCheckResult>{}; }
        });

    QVERIFY(outcome.success);
    QCOMPARE(outcome.reasonCode, QStringLiteral("already_ready"));
}
```

- [x] **Step 2: 运行 coordinator 测试，确认因新类型和新方法缺失而失败**

Run:

```powershell
cmake --build build_x64 --config Release --target platform_startup_coordinator_test
ctest --test-dir build_x64 -C Release -R platform_startup_coordinator_test --output-on-failure
```

Expected:

- `platform_startup_coordinator_test` 编译失败，因为 `activateOnDemand`、`PlatformOnDemandProbeSet`、`PlatformOnDemandActivationOutcome` 尚不存在

- [x] **Step 3: 实现 on-demand activation outcome、probe set 与显式执行入口**

```cpp
// Framework/Platform/Kernel/PlatformStartupCoordinator.h
struct PlatformOnDemandProbeSet
{
    std::function<PlatformPluginState(const QString&)> currentStateFn;
    std::function<QStringList(const QStringList&)> missingServicesFn;
    std::function<QStringList(const QString&)> missingPluginsFn;
    std::function<QStringList(const QString&)> missingCapabilitiesFn;
    std::function<QVector<PlatformHealthCheckResult>(const QString&, const QStringList&)> runHealthChecksFn;
};

struct PlatformOnDemandActivationOutcome
{
    bool success = false;
    PlatformLifecycleResult result = PlatformLifecycleResult::Failed;
    PlatformPluginState finalState = PlatformPluginState::Failed;
    QString reasonCode;
    QString detail;
    QString targetPluginId;
    QStringList missingServices;
    QStringList missingPlugins;
    QStringList missingCapabilities;
    QVector<PlatformHealthCheckResult> healthCheckResults;
};

PlatformOnDemandActivationOutcome activateOnDemand(
    const PlatformOnDemandActivationPlan& plan,
    const InstallManagedPluginFn& installManagedPluginFn,
    const PlatformOnDemandProbeSet& probes,
    int pollIntervalMs = 50);
```

```cpp
// Framework/Platform/Kernel/PlatformStartupCoordinator.cpp
PlatformOnDemandActivationOutcome PlatformStartupCoordinator::activateOnDemand(
    const PlatformOnDemandActivationPlan& plan,
    const InstallManagedPluginFn& installManagedPluginFn,
    const PlatformOnDemandProbeSet& probes,
    int pollIntervalMs)
{
    PlatformOnDemandActivationOutcome outcome;
    outcome.targetPluginId = plan.targetPluginId;

    if (plan.activationEntries.isEmpty()) {
        outcome.reasonCode = QStringLiteral("descriptor_missing");
        outcome.detail = QStringLiteral("On-demand activation plan is empty");
        return outcome;
    }

    const auto& targetEntry = plan.activationEntries.constLast();
    if (m_runtimeMode == PlatformRuntimeMode::ObserveOnly) {
        if (m_recorder) {
            m_recorder->recordPluginStepStarted(
                targetEntry.pluginId,
                targetEntry.ctkSymbolicName,
                PlatformLifecycleStep::Start,
                false);
            Q_UNUSED(startPluginForPath(
                resolvePlatformPluginTarget(targetEntry.pluginId),
                PluginStartPath::OnDemand));
        }
        outcome.reasonCode = QStringLiteral("skipped_by_mode");
        outcome.result = PlatformLifecycleResult::Skipped;
        outcome.finalState = PlatformPluginState::Discovered;
        outcome.detail = QStringLiteral("On-demand plugin start skipped in observe_only mode");
        return outcome;
    }

    if (probes.currentStateFn && probes.currentStateFn(targetEntry.pluginId) == PlatformPluginState::Ready) {
        outcome.success = true;
        outcome.result = PlatformLifecycleResult::Succeeded;
        outcome.finalState = PlatformPluginState::Ready;
        outcome.reasonCode = QStringLiteral("already_ready");
        outcome.detail = QStringLiteral("Target plugin is already ready");
        return outcome;
    }

    for (const auto& entry : plan.activationEntries) {
        if (!installManagedPluginFn || !installManagedPluginFn(entry)) {
            outcome.reasonCode = QStringLiteral("install_failed");
            outcome.detail = QStringLiteral("On-demand bundle install failed");
            return outcome;
        }
        const auto startOutcome = startPluginForPath(resolvePlatformPluginTarget(entry.pluginId), PluginStartPath::OnDemand);
        if (startOutcome == StartOutcome::Failed) {
            outcome.reasonCode = QStringLiteral("start_failed");
            outcome.detail = QStringLiteral("On-demand plugin start failed");
            return outcome;
        }
        const auto serviceOutcome = waitForServiceReady(
            {
                entry.pluginId,
                entry.displayName,
                entry.ctkSymbolicName,
                entry.bundleFilePath,
                PlatformBootstrapLevel::Deferred,
                PlatformStartupPolicy::OnDemand,
                entry.requiredPlugins,
                entry.requiredCapabilities,
                entry.requiredServices,
                entry.healthChecks,
                entry.serviceReadyTimeoutMs
            },
            {
                probes.missingServicesFn,
                probes.missingPluginsFn,
                probes.missingCapabilitiesFn
            },
            pollIntervalMs);
        if (!serviceOutcome.success) {
            outcome.reasonCode = QStringLiteral("service_ready_timeout");
            outcome.detail = serviceOutcome.detail;
            outcome.missingServices = serviceOutcome.missingServices;
            outcome.missingPlugins = serviceOutcome.missingPlugins;
            outcome.missingCapabilities = serviceOutcome.missingCapabilities;
            return outcome;
        }
    }

    outcome.healthCheckResults = probes.runHealthChecksFn
        ? probes.runHealthChecksFn(targetEntry.pluginId, targetEntry.healthChecks)
        : QVector<PlatformHealthCheckResult>{};
    for (const auto& result : outcome.healthCheckResults) {
        if (result.passed) continue;
        outcome.reasonCode = QStringLiteral("health_check_failed");
        outcome.detail = result.detail;
        outcome.finalState = PlatformPluginState::Failed;
        return outcome;
    }

    outcome.success = true;
    outcome.result = PlatformLifecycleResult::Succeeded;
    outcome.finalState = PlatformPluginState::Ready;
    outcome.reasonCode = QStringLiteral("service_ready");
    outcome.detail = QStringLiteral("On-demand plugin is ready");
    return outcome;
}
```

- [x] **Step 4: 重新运行 coordinator 测试并确认通过**

Run:

```powershell
cmake --build build_x64 --config Release --target platform_startup_coordinator_test
ctest --test-dir build_x64 -C Release -R platform_startup_coordinator_test --output-on-failure
```

Expected:

- `platform_startup_coordinator_test` PASS

- [x] **Step 5: 提交 on-demand execution coordinator**

```powershell
git add Framework/Platform/Kernel/PlatformStartupCoordinator.h Framework/Platform/Kernel/PlatformStartupCoordinator.cpp tests/unit/PlatformStartupCoordinatorTest.cpp
git commit -m "feat: execute governed on-demand activation"
```

### Task 3: 拆分 startup scope 与 governed scope，并更新 diagnostics 口径

**Files:**
- Modify: `Framework/Platform/Contracts/PlatformSnapshots.h`
- Modify: `Framework/Platform/Kernel/PlatformStateStore.h`
- Modify: `Framework/Platform/Kernel/PlatformStateStore.cpp`
- Modify: `Framework/Platform/Diagnostics/PlatformDiagnosticsService.cpp`
- Modify: `tests/unit/PlatformDependencyGraphTest.cpp`
- Modify: `tests/unit/PlatformDiagnosticsServiceTest.cpp`

- [x] **Step 1: 先写 failing tests，锁定 Phase 1 `platformReady` 不受 on-demand 插件污染**

```cpp
// tests/unit/PlatformDependencyGraphTest.cpp
void PlatformDependencyGraphTest::capabilitySnapshot_keeps_platform_ready_bound_to_startup_scope()
{
    PlatformStateStore store;
    store.setRuntimeMode(PlatformRuntimeMode::FacadeMode);
    store.replaceDescriptors({
        makeDescriptor(
            QStringLiteral("org.medicalpro.user_management"),
            QStringLiteral("UserManagement"),
            PlatformBootstrapLevel::Core,
            PlatformStartupPolicy::Eager,
            {QStringLiteral("identity.core")}),
        makeDescriptor(
            QStringLiteral("org.medicalpro.registration_core"),
            QStringLiteral("RegistrationCore"),
            PlatformBootstrapLevel::Deferred,
            PlatformStartupPolicy::OnDemand,
            {QStringLiteral("navigation.registration")})
    });
    store.setStartupScopePluginIds(QStringList{QStringLiteral("org.medicalpro.user_management")});
    store.setGovernedPluginIds(QStringList{
        QStringLiteral("org.medicalpro.user_management"),
        QStringLiteral("org.medicalpro.registration_core")
    });
    store.setPluginState(QStringLiteral("org.medicalpro.user_management"), PlatformPluginState::Ready);

    const auto snapshot = store.capabilitySnapshot();
    QVERIFY(snapshot.platformReady);
    QVERIFY(snapshot.lockedCapabilities.contains(QStringLiteral("navigation.registration")));
    QCOMPARE(snapshot.startupScopePluginIds, (QStringList{QStringLiteral("org.medicalpro.user_management")}));
    QVERIFY(snapshot.governedPluginIds.contains(QStringLiteral("org.medicalpro.registration_core")));
}
```

```cpp
// tests/unit/PlatformDiagnosticsServiceTest.cpp
void PlatformDiagnosticsServiceTest::buildSnapshot_reports_governed_scope_without_polluting_startup_ready()
{
    PlatformStateStore store;
    store.replaceDescriptors({
        makeDescriptor(QStringLiteral("org.medicalpro.user_management"), QStringLiteral("UserManagement")),
        makeDescriptor(
            QStringLiteral("org.medicalpro.registration_core"),
            QStringLiteral("RegistrationCore"),
            PlatformBootstrapLevel::Deferred,
            PlatformStartupPolicy::OnDemand)
    });
    store.setStartupScopePluginIds(QStringList{QStringLiteral("org.medicalpro.user_management")});
    store.setGovernedPluginIds(QStringList{
        QStringLiteral("org.medicalpro.user_management"),
        QStringLiteral("org.medicalpro.registration_core")
    });
    store.setRuntimeMode(PlatformRuntimeMode::FacadeMode);
    store.setPluginState(QStringLiteral("org.medicalpro.user_management"), PlatformPluginState::Ready);
    store.setPluginState(QStringLiteral("org.medicalpro.registration_core"), PlatformPluginState::Failed);

    PlatformDiagnosticsService service(&store);
    const auto snapshot = service.buildSnapshot({});

    QVERIFY(snapshot.summary.platformReady);
    QCOMPARE(snapshot.startupScopePluginIds, (QStringList{QStringLiteral("org.medicalpro.user_management")}));
    QVERIFY(snapshot.governedPluginIds.contains(QStringLiteral("org.medicalpro.registration_core")));
}
```

- [x] **Step 2: 运行 dependency/diagnostics 测试，确认先失败**

Run:

```powershell
cmake --build build_x64 --config Release --target platform_dependency_graph_test platform_diagnostics_service_test
ctest --test-dir build_x64 -C Release -R "platform_dependency_graph_test|platform_diagnostics_service_test" --output-on-failure
```

Expected:

- 现有编译或断言失败，因为 `PlatformStateStore` 还没有 `startupScope`/`governedScope` 概念

- [x] **Step 3: 实现 scope 拆分，并把 diagnostics 改为 governed scope 解释**

```cpp
// Framework/Platform/Contracts/PlatformSnapshots.h
struct PlatformCapabilitySnapshot
{
    PlatformRuntimeMode runtimeMode = PlatformRuntimeMode::ObserveOnly;
    bool platformReady = false;
    QStringList unlockedCapabilities;
    QStringList lockedCapabilities;
    QStringList degradedPlugins;
    QStringList startupScopePluginIds;
    QStringList governedPluginIds;
};

struct PlatformDiagnosticSnapshot
{
    PlatformDiagnosticSummary summary;
    PlatformCapabilitySnapshot capabilitySnapshot;
    QVector<PlatformPluginLifecycleSnapshot> pluginLifecycle;
    QVector<PlatformStartupTraceEntry> startupTrace;
    QVector<PlatformDiagnosticProblem> problems;
    QStringList recoveryHints;
    QStringList startupScopePluginIds;
    QStringList governedPluginIds;
    QStringList excludedPluginIds;
    bool frameworkReady = false;
    PlatformRuntimeMode runtimeMode = PlatformRuntimeMode::ObserveOnly;
    QVector<PlatformPluginRuntimeSnapshot> plugins;
};
```

```diff
# Framework/Platform/Kernel/PlatformStateStore.h
@@
-    void setManagedPluginIds(const QStringList& pluginIds);
-    QStringList managedPluginIds() const;
+    void setStartupScopePluginIds(const QStringList& pluginIds);
+    QStringList startupScopePluginIds() const;
+    void setGovernedPluginIds(const QStringList& pluginIds);
+    QStringList governedPluginIds() const;
+    void setManagedPluginIds(const QStringList& pluginIds) { setStartupScopePluginIds(pluginIds); }
+    QStringList managedPluginIds() const { return startupScopePluginIds(); }
@@
-    QStringList m_managedPluginIds;
+    QStringList m_startupScopePluginIds;
+    QStringList m_governedPluginIds;
```

```cpp
// Framework/Platform/Kernel/PlatformStateStore.cpp
PlatformCapabilitySnapshot PlatformStateStore::capabilitySnapshot() const
{
    QReadLocker locker(&m_lock);
    PlatformCapabilitySnapshot snapshot;
    snapshot.runtimeMode = m_runtimeMode;
    snapshot.startupScopePluginIds = m_startupScopePluginIds;
    snapshot.governedPluginIds = m_governedPluginIds;

    QStringList startupLockedCapabilities;
    QStringList startupDegradedPlugins;

    for (const auto& pluginId : m_descriptorOrder) {
        if (!m_governedPluginIds.contains(pluginId)) continue;

        const auto descriptor = m_descriptors.value(pluginId);
        const auto pluginSnapshot = m_snapshots.value(pluginId);
        const bool ready = pluginSnapshot.state == PlatformPluginState::Ready;

        for (const auto& capability : descriptor.provides.capabilities) {
            if (ready) snapshot.unlockedCapabilities.append(capability);
            else snapshot.lockedCapabilities.append(capability);
        }

        if (!m_startupScopePluginIds.contains(pluginId)) continue;
        if (pluginSnapshot.state == PlatformPluginState::Degraded
            || pluginSnapshot.state == PlatformPluginState::Failed) {
            startupDegradedPlugins.append(pluginId);
            snapshot.degradedPlugins.append(pluginId);
        }

        if (!ready) {
            startupLockedCapabilities.append(descriptor.provides.capabilities);
        }
    }

    snapshot.platformReady = startupLockedCapabilities.isEmpty() && startupDegradedPlugins.isEmpty();
    return snapshot;
}
```

```diff
# Framework/Platform/Diagnostics/PlatformDiagnosticsService.cpp
@@
-    QStringList managedPluginIds;
+    QStringList startupScopePluginIds;
+    QStringList governedPluginIds;
     if (m_stateStore) {
         descriptors = m_stateStore->descriptors();
-        managedPluginIds = m_stateStore->managedPluginIds();
+        startupScopePluginIds = m_stateStore->startupScopePluginIds();
+        governedPluginIds = m_stateStore->governedPluginIds();
         snapshot.plugins = m_stateStore->pluginSnapshots();
         snapshot.capabilitySnapshot = m_stateStore->capabilitySnapshot();
         snapshot.runtimeMode = snapshot.capabilitySnapshot.runtimeMode;
     }
 
-    if (managedPluginIds.isEmpty()) {
+    if (governedPluginIds.isEmpty()) {
         for (const auto& descriptor : descriptors) {
-            managedPluginIds.append(descriptor.id);
+            governedPluginIds.append(descriptor.id);
         }
     }
 
-    snapshot.managedPluginIds = managedPluginIds;
+    snapshot.startupScopePluginIds = startupScopePluginIds;
+    snapshot.governedPluginIds = governedPluginIds;
     for (const auto& descriptor : descriptors) {
-        if (managedPluginIds.contains(descriptor.id)) continue;
+        if (governedPluginIds.contains(descriptor.id)) continue;
 
         snapshot.excludedPluginIds.append(descriptor.id);
 
         PlatformDiagnosticProblem problem;
         problem.severity = PlatformDiagnosticSeverity::Info;
         problem.pluginId = descriptor.id;
-        problem.reasonCode = QStringLiteral("excluded_from_managed_startup");
-        problem.detail = QStringLiteral("Plugin is available in descriptors but excluded from the current managed startup scope");
+        problem.reasonCode = QStringLiteral("excluded_from_governed_scope");
+        problem.detail = QStringLiteral("Plugin is available in descriptors but excluded from the current governed scope");
         snapshot.problems.append(problem);
     }
```

- [x] **Step 4: 重新运行 scope/diagnostics 测试**

Run:

```powershell
cmake --build build_x64 --config Release --target platform_dependency_graph_test platform_diagnostics_service_test
ctest --test-dir build_x64 -C Release -R "platform_dependency_graph_test|platform_diagnostics_service_test" --output-on-failure
```

Expected:

- `platform_dependency_graph_test` PASS
- `platform_diagnostics_service_test` PASS

- [x] **Step 5: 提交 scope split 与 diagnostics 收口**

```powershell
git add Framework/Platform/Contracts/PlatformSnapshots.h Framework/Platform/Kernel/PlatformStateStore.h Framework/Platform/Kernel/PlatformStateStore.cpp Framework/Platform/Diagnostics/PlatformDiagnosticsService.cpp tests/unit/PlatformDependencyGraphTest.cpp tests/unit/PlatformDiagnosticsServiceTest.cpp
git commit -m "feat: split startup and governed plugin scopes"
```

### Task 4: 把 `LegacyNavigationAdapter` 切到治理链，并由 `main.cpp` 注入共享上下文

**Files:**
- Modify: `CMakeLists.txt`
- Create: `Framework/Platform/Kernel/PlatformOnDemandActivationService.h`
- Create: `Framework/Platform/Kernel/PlatformOnDemandActivationService.cpp`
- Modify: `Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.h`
- Modify: `Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.cpp`
- Modify: `UI/MainInterfaceWidget.h`
- Modify: `UI/MainInterfaceWidget.cpp`
- Modify: `main.cpp`
- Modify: `tests/unit/PlatformFacadesTest.cpp`

- [ ] **Step 1: 先补 facade RED 测试，锁定 adapter 不再自己 CTK 直启**

```cpp
// tests/unit/PlatformFacadesTest.cpp
class FakeOnDemandActivationService
{
public:
    bool ensureReady(const QString& pluginId)
    {
        lastPluginId = pluginId;
        return nextResult;
    }

    QString lastPluginId;
    bool nextResult = true;
};

void PlatformFacadesTest::navigationLegacyAdapter_delegates_to_governed_activation_service()
{
    FakeOnDemandActivationService activationService;
    LegacyNavigationAdapter adapter(
        [&activationService](const QString& pluginId) {
            return activationService.ensureReady(pluginId);
        });
    NavigationAppService service(&adapter);

    QVERIFY(service.ensureReady(QStringLiteral("org.medicalpro.registration_core")));
    QCOMPARE(activationService.lastPluginId, QStringLiteral("org.medicalpro.registration_core"));
}

void PlatformFacadesTest::navigationLegacyAdapter_returns_false_when_activation_service_is_missing()
{
    LegacyNavigationAdapter adapter;
    NavigationAppService service(&adapter);

    QVERIFY(!service.ensureReady(QStringLiteral("org.medicalpro.optical_tracking")));
}
```

- [ ] **Step 2: 运行 facade 测试，确认现有 adapter 语义与新 RED 测试冲突**

Run:

```powershell
cmake --build build_x64 --config Release --target platform_facades_test
ctest --test-dir build_x64 -C Release -R platform_facades_test --output-on-failure
```

Expected:

- `platform_facades_test` 编译失败或断言失败，因为 `LegacyNavigationAdapter` 还在用旧的 CTK 直启构造函数

- [ ] **Step 3: 新增 on-demand activation service，并把 adapter 改为治理链代理**

```cpp
// Framework/Platform/Kernel/PlatformOnDemandActivationService.h
#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Kernel/PlatformOnDemandActivationPlan.h"
#include "Framework/Platform/Kernel/PlatformStartupCoordinator.h"

class PlatformStateStore;

class FRAMEWORK_EXPORT PlatformOnDemandActivationService
{
public:
    using InstallPluginFn = std::function<bool(const PlatformOnDemandActivationPlanEntry&)>;

    PlatformOnDemandActivationService(
        QVector<PlatformPluginDescriptor> descriptors,
        QString pluginDirectory,
        PlatformStartupCoordinator* startupCoordinator,
        PlatformStateStore* stateStore,
        InstallPluginFn installPluginFn,
        PlatformOnDemandProbeSet probeSet);

    void setStateStore(PlatformStateStore* stateStore);
    bool ensureReady(const QString& pluginId);

private:
    QVector<PlatformPluginDescriptor> m_descriptors;
    QString m_pluginDirectory;
    PlatformStartupCoordinator* m_startupCoordinator = nullptr;
    PlatformStateStore* m_stateStore = nullptr;
    InstallPluginFn m_installPluginFn;
    PlatformOnDemandProbeSet m_probeSet;
};
```

```cpp
// Framework/Platform/Kernel/PlatformOnDemandActivationService.cpp
void PlatformOnDemandActivationService::setStateStore(PlatformStateStore* stateStore)
{
    m_stateStore = stateStore;
}

bool PlatformOnDemandActivationService::ensureReady(const QString& pluginId)
{
    QString error;
    const auto plan = PlatformOnDemandActivationPlanBuilder::build(
        pluginId,
        m_descriptors,
        m_pluginDirectory,
        &error);
    if (!error.isEmpty() || !m_startupCoordinator) {
        if (m_stateStore) m_stateStore->setPluginState(pluginId, PlatformPluginState::Failed);
        return false;
    }

    const auto outcome = m_startupCoordinator->activateOnDemand(
        plan,
        m_installPluginFn,
        m_probeSet);

    if (m_stateStore) {
        m_stateStore->setPluginState(pluginId, outcome.finalState);
    }
    return outcome.success;
}
```

```diff
# CMakeLists.txt
@@
     Framework/Platform/Kernel/PlatformOnDemandActivationPlan.h
     Framework/Platform/Kernel/PlatformOnDemandActivationPlan.cpp
+    Framework/Platform/Kernel/PlatformOnDemandActivationService.h
+    Framework/Platform/Kernel/PlatformOnDemandActivationService.cpp
     Framework/Platform/Kernel/PlatformWarmupCoordinator.h
     Framework/Platform/Kernel/PlatformWarmupCoordinator.cpp
```

```cpp
// Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.h
class FRAMEWORK_EXPORT LegacyNavigationAdapter : public INavigationFacadePort
{
public:
    using EnsureReadyFn = std::function<bool(const QString&)>;

    explicit LegacyNavigationAdapter(EnsureReadyFn ensureReadyFn = {});
    bool ensureReady(const QString& pluginId) override;

private:
    EnsureReadyFn m_ensureReadyFn;
};
```

```cpp
// Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.cpp
#include "Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.h"

#include <QDebug>
#include <utility>

LegacyNavigationAdapter::LegacyNavigationAdapter(EnsureReadyFn ensureReadyFn)
    : m_ensureReadyFn(std::move(ensureReadyFn))
{
}

bool LegacyNavigationAdapter::ensureReady(const QString& pluginId)
{
    const auto trimmedPluginId = pluginId.trimmed();
    if (trimmedPluginId.isEmpty() || !m_ensureReadyFn) {
        qWarning() << "[LegacyNavigationAdapter] Missing governed on-demand activation callback for plugin id:" << trimmedPluginId;
        return false;
    }
    return m_ensureReadyFn(trimmedPluginId);
}
```

- [ ] **Step 4: 修改 `MainInterfaceWidget` 与 `main.cpp`，由主治理上下文注入导航 port**

```diff
# UI/MainInterfaceWidget.h
@@
-class LegacyNavigationAdapter;
+class INavigationFacadePort;
@@
-    explicit MainInterfaceWidget(QWidget* parent = nullptr);
+    explicit MainInterfaceWidget(INavigationFacadePort* navigationPort, QWidget* parent = nullptr);
@@
-    LegacyNavigationAdapter* m_navigationAdapter;
     IdentityAppService* m_identityAppService;
     ImagingAppService* m_imagingAppService;
     NavigationAppService* m_navigationAppService;
+    INavigationFacadePort* m_navigationPort = nullptr;
```

```diff
# UI/MainInterfaceWidget.cpp
@@
-MainInterfaceWidget::MainInterfaceWidget(QWidget* parent)
+MainInterfaceWidget::MainInterfaceWidget(INavigationFacadePort* navigationPort, QWidget* parent)
@@
-    , m_navigationAdapter(nullptr)
     , m_identityAppService(nullptr)
     , m_imagingAppService(nullptr)
     , m_navigationAppService(nullptr)
+    , m_navigationPort(navigationPort)
     , m_currentPatientId(-1)
     , m_isLoggedIn(false)
 {
+    Q_ASSERT(m_navigationPort);
     qDebug() << "[MainInterfaceWidget] create";
@@
-    delete m_navigationAdapter;
     delete m_imagingAdapter;
     delete m_identityAdapter;
@@
-    if (!m_navigationAdapter) m_navigationAdapter = new LegacyNavigationAdapter();
     if (!m_coreUiRuntimeAdapter) m_coreUiRuntimeAdapter = new LegacyCoreUiRuntimeAdapter();
@@
-    if (!m_navigationAppService) m_navigationAppService = new NavigationAppService(m_navigationAdapter);
+    if (!m_navigationAppService) m_navigationAppService = new NavigationAppService(m_navigationPort);
}
```

```diff
# main.cpp
@@
 #include "Framework/Platform/Kernel/PlatformStartupCoordinator.h"
+#include "Framework/Platform/Kernel/PlatformOnDemandActivationService.h"
 #include "Framework/Platform/Kernel/PlatformWarmupCoordinator.h"
+#include "Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.h"
+#include "Framework/Registration/RegistrationService.h"
+#include "Plugins/OpticalTracking/OpticalTrackingService.h"
@@ struct StartupRuntimeContext
     std::unique_ptr<PlatformWarmupCoordinator> warmupCoordinator;
     PlatformStateStore* stateStore = nullptr;
+    std::shared_ptr<PlatformOnDemandActivationService> onDemandActivationService;
+    std::unique_ptr<LegacyNavigationAdapter> navigationAdapter;
```

```cpp
// main.cpp
startupContext->onDemandActivationService = std::make_shared<PlatformOnDemandActivationService>(
    descriptors,
    pluginsPath,
    &startupContext->startupCoordinator,
    nullptr,
    [ctkManager](const PlatformOnDemandActivationPlanEntry& entry) {
        return ctkManager->installPlugin(entry.bundleFilePath, false, nullptr);
    },
    {
        [startupContext](const QString& pluginId) {
            if (!startupContext->stateStore) return PlatformPluginState::Discovered;
            for (const auto& snapshot : startupContext->stateStore->pluginSnapshots()) {
                if (snapshot.pluginId == pluginId) return snapshot.state;
            }
            return PlatformPluginState::Discovered;
        },
        [ctkManager](const QStringList& requiredServices) {
            return ctkManager->getMissingServices(requiredServices);
        },
        [startupContext](const QString& pluginId) {
            return startupContext->missingRequiredPlugins(pluginId);
        },
        [startupContext](const QString& pluginId) {
            return startupContext->missingRequiredCapabilities(pluginId);
        },
        [ctkManager](const QString&, const QStringList& healthChecks) {
            QVector<PlatformHealthCheckResult> results;
            for (const auto& healthCheck : healthChecks) {
                PlatformHealthCheckResult result;
                result.name = healthCheck;
                if (healthCheck == QStringLiteral("service_registered")) {
                    result.passed = true;
                    result.detail = QStringLiteral("Required services were registered");
                } else if (healthCheck == QStringLiteral("core_binary_accessible")) {
                    result.passed = ctkManager->getService<RegistrationService>() != nullptr;
                    result.detail = result.passed
                        ? QStringLiteral("RegistrationService is available")
                        : QStringLiteral("RegistrationService is not available");
                } else if (healthCheck == QStringLiteral("tracking_adapter_accessible")) {
                    result.passed = ctkManager->getService<OpticalTrackingService>() != nullptr;
                    result.detail = result.passed
                        ? QStringLiteral("OpticalTrackingService is available")
                        : QStringLiteral("OpticalTrackingService is not available");
                } else {
                    result.passed = false;
                    result.detail = QStringLiteral("Unknown health check");
                }
                results.append(result);
            }
            return results;
        }
    });

startupContext->navigationAdapter = std::make_unique<LegacyNavigationAdapter>(
    [service = startupContext->onDemandActivationService](const QString& pluginId) {
        return service->ensureReady(pluginId);
    });

QPointer<MainInterfaceWidget> mainInterface = new MainInterfaceWidget(startupContext->navigationAdapter.get(), nullptr);
startupContext->stateStore = mainInterface->platformStateStore();
startupContext->stateStore->setStartupScopePluginIds(startupContext->managedPlan.managedPluginIds);
startupContext->stateStore->setGovernedPluginIds(QStringList{
    QStringLiteral("org.medicalpro.user_management"),
    QStringLiteral("org.medicalpro.dicom_viewer"),
    QStringLiteral("org.medicalpro.four_view_display"),
    QStringLiteral("org.medicalpro.registration_core"),
    QStringLiteral("org.medicalpro.optical_tracking")
});
startupContext->onDemandActivationService->setStateStore(startupContext->stateStore);
```

- [ ] **Step 5: 跑 facade/build 验收并提交 adapter 注入链**

Run:

```powershell
cmake --build build_x64 --config Release --target medicalpro platform_facades_test
ctest --test-dir build_x64 -C Release -R platform_facades_test --output-on-failure
rg -n "new LegacyNavigationAdapter\\(|CTKManager::instance\\(|startPlugin\\(" UI/MainInterfaceWidget.cpp Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.cpp
```

Expected:

- `medicalpro` build target PASS
- `platform_facades_test` PASS
- `UI/MainInterfaceWidget.cpp` 中无 `new LegacyNavigationAdapter(`
- `Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.cpp` 中无 `CTKManager::instance(` 或 `startPlugin(`

```powershell
git add CMakeLists.txt Framework/Platform/Kernel/PlatformOnDemandActivationService.h Framework/Platform/Kernel/PlatformOnDemandActivationService.cpp Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.h Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.cpp UI/MainInterfaceWidget.h UI/MainInterfaceWidget.cpp main.cpp tests/unit/PlatformFacadesTest.cpp
git commit -m "refactor: route navigation ensureReady through platform governance"
```

### Task 5: 回写文档并执行 Phase 2 全量验收

**Files:**
- Modify: `docs/current_status_and_project_overview.md`
- Modify: `docs/superpowers/tracking/platform-migration-decision-log.md`
- Modify: `docs/superpowers/specs/2026-04-16-platform-kernel-governance-design.md`
- Modify: `docs/superpowers/plans/2026-04-21-plugin-chain-remediation-phase2-implementation.md`

- [ ] **Step 1: 回写文档，明确 Phase 2 landed scope 与严格 `ensureReady()` 口径**

```md
<!-- docs/current_status_and_project_overview.md -->
### 2026-04-21 Plugin Chain Remediation Phase 2 Acceptance

- `RegistrationCore` and `OpticalTracking` now enter through governed on-demand activation instead of direct CTK start shortcuts.
- `ensureReady(plugin_id)` now uses strict `descriptor -> install/start -> service_ready -> health_check` semantics.
- `platformReady` remains bound to the Phase 1 startup scope, while on-demand plugins now belong to governed scope and diagnostics.
- `observe_only`, `facade_mode`, and `orchestrate_core` now report consistent on-demand activation semantics.
```

```md
<!-- docs/superpowers/tracking/platform-migration-decision-log.md -->
## 2026-04-21

- Decision: move `ensureReady()` onto the governed on-demand activation path for `RegistrationCore` and `OpticalTracking`.
- Rationale: the old `plugin id -> CTK symbolic name -> direct start` path bypassed descriptor validation, service-ready gating, and diagnostics.
- Impact: on-demand activation is now descriptor-driven and diagnosable, but Phase 1 startup readiness remains scoped to the cold-start core set.
```

```md
<!-- docs/superpowers/specs/2026-04-16-platform-kernel-governance-design.md -->
## Plugin Chain Remediation Phase 2 Follow-up (2026-04-21)

- `RegistrationCore` and `OpticalTracking` now enter through governed on-demand activation.
- `ensureReady()` now follows strict `descriptor -> install/start -> service_ready -> health_check` semantics.
- Startup scope and governed scope are now separated, so on-demand failures no longer rewrite Phase 1 startup readiness.
```

- [ ] **Step 2: 运行完整验收命令**

Run:

```powershell
cmake --build build_x64 --config Release --target medicalpro platform_on_demand_activation_plan_test platform_startup_coordinator_test platform_dependency_graph_test platform_diagnostics_service_test platform_facades_test
ctest --test-dir build_x64 -C Release -R "platform_on_demand_activation_plan_test|platform_startup_coordinator_test|platform_dependency_graph_test|platform_diagnostics_service_test|platform_facades_test|platform_descriptor_runtime_layout_test" --output-on-failure
rg -n "new LegacyNavigationAdapter\\(|CTKManager::instance\\(|startPlugin\\(" UI/MainInterfaceWidget.cpp Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.cpp
```

Expected:

- `medicalpro` build target PASS
- `platform_on_demand_activation_plan_test` PASS
- `platform_startup_coordinator_test` PASS
- `platform_dependency_graph_test` PASS
- `platform_diagnostics_service_test` PASS
- `platform_facades_test` PASS
- `platform_descriptor_runtime_layout_test` PASS
- `rg` 对目标文件无输出

- [ ] **Step 3: 回写实施计划状态并提交文档收口**

```md
<!-- docs/superpowers/plans/2026-04-21-plugin-chain-remediation-phase2-implementation.md -->
Status update 2026-04-21:

- Completed. Phase 2 now routes `RegistrationCore` and `OpticalTracking` through governed on-demand activation.
- Acceptance rerun passed on `build_x64` for build, unit tests, runtime descriptor layout, and legacy-entry scan.
- `platformReady` remains limited to Phase 1 startup scope, while on-demand plugins now appear inside governed diagnostics.
```

```powershell
git add docs/current_status_and_project_overview.md docs/superpowers/tracking/platform-migration-decision-log.md docs/superpowers/specs/2026-04-16-platform-kernel-governance-design.md docs/superpowers/plans/2026-04-21-plugin-chain-remediation-phase2-implementation.md
git commit -m "docs: record plugin chain remediation phase 2 acceptance"
```

## Self-Review

- Spec coverage:
  - 严格 `ensureReady()` 语义：Task 1 / Task 2 / Task 4 覆盖
  - 三档运行模式一致：Task 2 覆盖
  - `RegistrationCore / OpticalTracking` descriptor diagnostics 契约：Task 1 覆盖
  - startup scope 与 governed scope 拆分：Task 3 覆盖
  - adapter 不再 CTK 直启、widget 不再自建导航适配器：Task 4 覆盖
  - diagnostics 与文档回写：Task 3 / Task 5 覆盖
- Placeholder scan:
  - 已避免占位词与“稍后补实现/稍后补测试”式描述
  - 所有任务都包含明确文件、代码片段、命令和提交信息
- Type consistency:
  - 统一使用 `PlatformOnDemandActivationPlanEntry`、`PlatformOnDemandActivationPlan`、`PlatformOnDemandProbeSet`、`PlatformOnDemandActivationOutcome`、`PlatformOnDemandActivationService`
  - `startupScopePluginIds` 与 `governedPluginIds` 的命名在 state store、snapshots、diagnostics 中保持一致
