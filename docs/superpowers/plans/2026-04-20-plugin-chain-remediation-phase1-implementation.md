# Plugin Chain Remediation Phase 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 鎶?`medicalpro` 鐨勯鎵瑰彈绠℃彃浠朵富鍚姩閾炬敹鍙ｄ负 `facade_mode + descriptor-driven managed install plan + service ready` 鐨勫崟涓€鐪熺浉閾捐矾锛岃В鍐斥€滄壂鎻忓叏鐩綍瀹夎銆佸钩鍙板彧璁ら儴鍒嗘彃浠躲€佹渶缁堢姸鎬佽В閲婁笉涓€鑷粹€濈殑闂銆?
**Architecture:** 鏂板涓€涓函娌荤悊灞傜殑 `PlatformManagedPluginPlanBuilder`锛岀敱 `platform_runtime.json + descriptor + runtime plugin dir` 鐢熸垚鍙楃瀹夎/鍚姩璁″垝锛屽苟鎶婅璁″垝鍚屾椂浜ょ粰 `PlatformStartupCoordinator`銆乣PlatformStateStore` 鍜?diagnostics service銆俙main.cpp` 鏀规垚鍙礋璐ｇ粍瑁呬緷璧栧拰娉ㄥ唽 phase handler锛屼笉鍐嶇洿鎺ユ壂鎻忔暣涓?`plugins/` 鐩綍鍐冲畾瑁呰皝锛屼篃涓嶅啀鍐呰仈 `service ready` 杞鍜?warmup 缁嗚妭銆?
**Tech Stack:** Qt 6銆丵tTest銆丵t Widgets銆丆TK Plugin Framework銆丆Make銆佺幇鏈?`Framework/Platform` 娌荤悊灞傘€佺幇鏈?`tests/unit` 涓?`tests/runtime`

---

## Files and Responsibilities

- Modify: `CMakeLists.txt`
  - 鎶婃柊鐨勬不鐞嗗眰 helper 婧愭枃浠剁紪杩?`Framework`
- Modify: `config/platform_runtime.json`
  - 鎶婇粯璁よ繍琛屾ā寮忓垏鍒?`facade_mode`
- Create: `Framework/Platform/Kernel/PlatformManagedPluginPlan.h`
- Create: `Framework/Platform/Kernel/PlatformManagedPluginPlan.cpp`
  - 绾不鐞嗗眰 helper锛岃礋璐ｄ粠 runtime config + descriptors + runtime plugin dir 鐢熸垚棣栨壒鍙楃瀹夎/鍚姩璁″垝
- Modify: `Framework/Platform/Kernel/PlatformStateStore.h`
- Modify: `Framework/Platform/Kernel/PlatformStateStore.cpp`
  - 璁板綍鈥滃綋鍓嶅彈绠℃彃浠惰寖鍥粹€濓紝骞惰 capability/platform ready 鍙熀浜庡彈绠℃彃浠惰绠?- Modify: `Framework/Platform/Contracts/PlatformSnapshots.h`
  - 涓?diagnostics snapshot 澧炲姞鍙楃鑼冨洿瀛楁
- Modify: `Framework/Platform/Diagnostics/PlatformDiagnosticsService.cpp`
  - 鎶婂彈绠¤寖鍥翠笌鈥滆鎺掗櫎鍑轰富鍚姩閾锯€濈殑淇℃伅姹囨€昏繘 diagnostics snapshot/problem list
- Modify: `Framework/Platform/Kernel/PlatformStartupCoordinator.h`
- Modify: `Framework/Platform/Kernel/PlatformStartupCoordinator.cpp`
  - 鎺ユ敹鍙楃璁″垝锛岃礋璐ｅ彈绠?install / core start / service ready 鐨勬墽琛屼笌浜嬩欢璁板綍
- Create: `Framework/Platform/Kernel/PlatformWarmupCoordinator.h`
- Create: `Framework/Platform/Kernel/PlatformWarmupCoordinator.cpp`
  - 鎶?warmup 灏炬浠?`main.cpp` 鎶界鎴愮嫭绔?helper锛岀‘淇?Phase 1 涓嶅啀鍦?`main.cpp` 閲岀‖缂栫爜 plugin service 璁块棶
- Modify: `main.cpp`
  - 鎺ュ叆 managed plan builder銆乻tate store managed scope銆乧oordinator 椹卞姩鐨?install/start/service ready锛屼互鍙婄嫭绔?warmup coordinator
- Modify: `tests/unit/CMakeLists.txt`
  - 娉ㄥ唽鏂板崟娴?target
- Create: `tests/unit/PlatformManagedPluginPlanTest.cpp`
  - 瑕嗙洊鍙楃娓呭崟鐢熸垚銆佷緷璧栬ˉ榻愩€佸叧閿?descriptor 绾︽潫鍜?bundle path 瑙ｆ瀽
- Modify: `tests/unit/PlatformDependencyGraphTest.cpp`
  - 琛ヨ鐩栤€滈潪鍙楃 descriptor 涓嶅啀闃绘柇 platform ready鈥?- Modify: `tests/unit/PlatformDiagnosticsServiceTest.cpp`
  - 瑕嗙洊 managed scope 涓?excluded plugin diagnostics 杈撳嚭
- Modify: `tests/unit/PlatformStartupCoordinatorTest.cpp`
  - 瑕嗙洊鍙楃 install / service ready 鏂版柟娉?- Create: `tests/unit/PlatformWarmupCoordinatorTest.cpp`
  - 瑕嗙洊 facade mode skip 涓庢棤 warmup 浠诲姟鏃剁殑闈為樆鏂涓?- Modify: `tests/unit/StartupOrchestratorLifecycleTest.cpp`
  - 瑕嗙洊 facade mode 涓嬪彧璺?managed init/install/core start锛屼笉鍐嶅洖閫€鍒版棫鍏ㄩ噺瀹夎璺緞
- Modify: `docs/current_status_and_project_overview.md`
  - 鍥炲啓鏈疆榛樿妯″紡銆佸彈绠¤寖鍥淬€乺eady 杈圭晫涓庨獙鏀跺懡浠?- Modify: `docs/superpowers/tracking/platform-migration-decision-log.md`
  - 璁板綍鈥淧hase 1 鏀逛负 descriptor-driven managed startup鈥濆喅绛?- Modify: `docs/superpowers/specs/2026-04-16-platform-kernel-governance-design.md`
  - 杩藉姞 2026-04-20 plugin-chain remediation landed note

### Task 1: 鏂板鍙楃瀹夎璁″垝鐢熸垚鍣ㄥ苟閿佸畾 Phase 1 descriptor 濂戠害

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `config/platform_runtime.json`
- Create: `Framework/Platform/Kernel/PlatformManagedPluginPlan.h`
- Create: `Framework/Platform/Kernel/PlatformManagedPluginPlan.cpp`
- Modify: `tests/unit/CMakeLists.txt`
- Create: `tests/unit/PlatformManagedPluginPlanTest.cpp`

- [x] **Step 1: 鍏堟敞鍐屾柊娴嬭瘯 target锛屽苟鍐?failing tests 閿佸畾鈥滃彧鐢熸垚棣栨壒鍙楃璁″垝 + 缂哄瓧娈电洿鎺ュけ璐モ€?*

```cmake
# tests/unit/CMakeLists.txt
add_executable(platform_managed_plugin_plan_test
    PlatformManagedPluginPlanTest.cpp
)

target_include_directories(platform_managed_plugin_plan_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(platform_managed_plugin_plan_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME platform_managed_plugin_plan_test
    COMMAND platform_managed_plugin_plan_test
)
```

```cpp
// tests/unit/PlatformManagedPluginPlanTest.cpp
#include <QtTest/QtTest>

#include "Framework/Platform/Kernel/PlatformManagedPluginPlan.h"

namespace
{
PlatformPluginDescriptor makeDescriptor(
    const QString& pluginId,
    const QString& displayName,
    const QString& ctkSymbolicName,
    PlatformBootstrapLevel bootstrapLevel,
    PlatformStartupPolicy startupPolicy,
    const QStringList& providesCapabilities = {},
    const QStringList& requiredCapabilities = {})
{
    PlatformPluginDescriptor descriptor;
    descriptor.id = pluginId;
    descriptor.version = QStringLiteral("1.0.0");
    descriptor.displayName = displayName;
    descriptor.domain = QStringLiteral("test");
    descriptor.runtime.ctkSymbolicName = ctkSymbolicName;
    descriptor.runtime.bootstrapLevel = bootstrapLevel;
    descriptor.runtime.startupPolicy = startupPolicy;
    descriptor.provides.capabilities = providesCapabilities;
    descriptor.required.capabilities = requiredCapabilities;
    descriptor.diagnostics.requiredServices = QStringList{QStringLiteral("%1.service").arg(pluginId)};
    descriptor.diagnostics.serviceReadyTimeoutMs = 5000;
    descriptor.healthChecks = QStringList{QStringLiteral("service_registered")};
    return descriptor;
}
}

class PlatformManagedPluginPlanTest : public QObject
{
    Q_OBJECT

private slots:
    void build_returns_phase1_managed_core_install_plan();
    void build_adds_required_capability_provider_before_dependent_plugin();
    void build_rejects_managed_plugin_missing_phase1_diagnostics_contract();
};

void PlatformManagedPluginPlanTest::build_returns_phase1_managed_core_install_plan()
{
    QTemporaryDir pluginDir;
    QVERIFY(pluginDir.isValid());
    QVERIFY(QFile(pluginDir.filePath(QStringLiteral("UserManagement.dll"))).open(QIODevice::WriteOnly));
    QVERIFY(QFile(pluginDir.filePath(QStringLiteral("DicomViewer.dll"))).open(QIODevice::WriteOnly));
    QVERIFY(QFile(pluginDir.filePath(QStringLiteral("FourViewDisplay.dll"))).open(QIODevice::WriteOnly));

    PlatformRuntimeConfig config;
    config.runtimeMode = PlatformRuntimeMode::FacadeMode;
    config.corePluginIds = {
        QStringLiteral("org.medicalpro.user_management"),
        QStringLiteral("org.medicalpro.dicom_viewer"),
        QStringLiteral("org.medicalpro.four_view_display")
    };

    const auto plan = PlatformManagedPluginPlanBuilder::build(
        config,
        {
            makeDescriptor(QStringLiteral("org.medicalpro.user_management"), QStringLiteral("UserManagement"), QStringLiteral("UserManagement"), PlatformBootstrapLevel::Core, PlatformStartupPolicy::Eager, {QStringLiteral("identity.core")}),
            makeDescriptor(QStringLiteral("org.medicalpro.dicom_viewer"), QStringLiteral("DicomViewer"), QStringLiteral("DicomViewer"), PlatformBootstrapLevel::Core, PlatformStartupPolicy::Eager, {QStringLiteral("imaging.data")}),
            makeDescriptor(QStringLiteral("org.medicalpro.four_view_display"), QStringLiteral("FourViewDisplay"), QStringLiteral("FourViewDisplay"), PlatformBootstrapLevel::Core, PlatformStartupPolicy::Eager, {QStringLiteral("imaging.viewport")}, {QStringLiteral("imaging.data")})
        },
        pluginDir.path());

    QCOMPARE(plan.managedPluginIds, (QStringList{
        QStringLiteral("org.medicalpro.user_management"),
        QStringLiteral("org.medicalpro.dicom_viewer"),
        QStringLiteral("org.medicalpro.four_view_display")
    }));
    QCOMPARE(plan.corePluginIds, plan.managedPluginIds);
    QCOMPARE(plan.installEntries.size(), 3);
    QVERIFY(plan.installEntries.constFirst().bundleFilePath.endsWith(QStringLiteral("UserManagement.dll")));
}

void PlatformManagedPluginPlanTest::build_adds_required_capability_provider_before_dependent_plugin()
{
    QTemporaryDir pluginDir;
    QVERIFY(pluginDir.isValid());
    QVERIFY(QFile(pluginDir.filePath(QStringLiteral("DicomViewer.dll"))).open(QIODevice::WriteOnly));
    QVERIFY(QFile(pluginDir.filePath(QStringLiteral("FourViewDisplay.dll"))).open(QIODevice::WriteOnly));

    PlatformRuntimeConfig config;
    config.runtimeMode = PlatformRuntimeMode::FacadeMode;
    config.corePluginIds = {QStringLiteral("org.medicalpro.four_view_display")};

    const auto plan = PlatformManagedPluginPlanBuilder::build(
        config,
        {
            makeDescriptor(QStringLiteral("org.medicalpro.dicom_viewer"), QStringLiteral("DicomViewer"), QStringLiteral("DicomViewer"), PlatformBootstrapLevel::Core, PlatformStartupPolicy::Eager, {QStringLiteral("imaging.data")}),
            makeDescriptor(QStringLiteral("org.medicalpro.four_view_display"), QStringLiteral("FourViewDisplay"), QStringLiteral("FourViewDisplay"), PlatformBootstrapLevel::Core, PlatformStartupPolicy::Eager, {QStringLiteral("imaging.viewport")}, {QStringLiteral("imaging.data")})
        },
        pluginDir.path());

    QCOMPARE(plan.installEntries.size(), 2);
    QCOMPARE(plan.installEntries.at(0).pluginId, QStringLiteral("org.medicalpro.dicom_viewer"));
    QCOMPARE(plan.installEntries.at(1).pluginId, QStringLiteral("org.medicalpro.four_view_display"));
}

void PlatformManagedPluginPlanTest::build_rejects_managed_plugin_missing_phase1_diagnostics_contract()
{
    QTemporaryDir pluginDir;
    QVERIFY(pluginDir.isValid());
    QVERIFY(QFile(pluginDir.filePath(QStringLiteral("UserManagement.dll"))).open(QIODevice::WriteOnly));

    auto descriptor = makeDescriptor(
        QStringLiteral("org.medicalpro.user_management"),
        QStringLiteral("UserManagement"),
        QStringLiteral("UserManagement"),
        PlatformBootstrapLevel::Core,
        PlatformStartupPolicy::Eager,
        {QStringLiteral("identity.core")});
    descriptor.diagnostics.requiredServices.clear();
    descriptor.healthChecks.clear();

    PlatformRuntimeConfig config;
    config.runtimeMode = PlatformRuntimeMode::FacadeMode;
    config.corePluginIds = {QStringLiteral("org.medicalpro.user_management")};

    QString error;
    const auto plan = PlatformManagedPluginPlanBuilder::build(config, {descriptor}, pluginDir.path(), &error);

    QVERIFY(plan.installEntries.isEmpty());
    QVERIFY(error.contains(QStringLiteral("health_checks")));
}

QTEST_APPLESS_MAIN(PlatformManagedPluginPlanTest)
#include "PlatformManagedPluginPlanTest.moc"
```

- [x] **Step 2: 杩愯鏂版祴璇曪紝纭鐜板湪鍏堢孩鐏?*


Run:

```powershell
cmake --build build_x64 --config Release --target platform_managed_plugin_plan_test
ctest --test-dir build_x64 -C Release -R platform_managed_plugin_plan_test --output-on-failure
```

Expected:

- `platform_managed_plugin_plan_test` 缂栬瘧澶辫触锛屽洜涓?`PlatformManagedPluginPlanBuilder` 杩樹笉瀛樺湪

- [x] **Step 3: 瀹炵幇鍙楃璁″垝 builder锛屽苟鎺ュ叆 Framework 缂栬瘧**

```cpp
// Framework/Platform/Kernel/PlatformManagedPluginPlan.h
#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/PlatformPluginDescriptor.h"
#include "Framework/Platform/Kernel/PlatformRuntimeConfig.h"

#include <QVector>

struct PlatformManagedPluginPlanEntry
{
    QString pluginId;
    QString displayName;
    QString ctkSymbolicName;
    QString bundleFilePath;
    PlatformBootstrapLevel bootstrapLevel = PlatformBootstrapLevel::Deferred;
    PlatformStartupPolicy startupPolicy = PlatformStartupPolicy::Disabled;
    QStringList requiredPlugins;
    QStringList requiredCapabilities;
    QStringList requiredServices;
    QStringList healthChecks;
    int serviceReadyTimeoutMs = 0;
};

struct PlatformManagedPluginPlan
{
    PlatformRuntimeMode runtimeMode = PlatformRuntimeMode::ObserveOnly;
    QVector<PlatformManagedPluginPlanEntry> installEntries;
    QStringList managedPluginIds;
    QStringList corePluginIds;
};

class FRAMEWORK_EXPORT PlatformManagedPluginPlanBuilder
{
public:
    static PlatformManagedPluginPlan build(
        const PlatformRuntimeConfig& runtimeConfig,
        const QVector<PlatformPluginDescriptor>& descriptors,
        const QString& pluginDirectory,
        QString* error = nullptr);
};
```

```cpp
// Framework/Platform/Kernel/PlatformManagedPluginPlan.cpp
#include "Framework/Platform/Kernel/PlatformManagedPluginPlan.h"

#include "Framework/Platform/Kernel/PlatformDependencyGraph.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>

namespace
{
QString resolveBundlePath(const QString& pluginDirectory, const QString& ctkSymbolicName)
{
    const QDir dir(pluginDirectory);
    for (const auto& fileName : dir.entryList(QDir::Files)) {
        const QFileInfo fileInfo(dir.filePath(fileName));
        const auto baseName = fileInfo.completeBaseName();
        if (baseName.compare(ctkSymbolicName, Qt::CaseInsensitive) == 0) return fileInfo.absoluteFilePath();
        if (baseName.compare(QStringLiteral("lib%1").arg(ctkSymbolicName), Qt::CaseInsensitive) == 0) {
            return fileInfo.absoluteFilePath();
        }
    }
    return {};
}

void appendManagedDescriptorRecursively(
    const QString& pluginId,
    const QHash<QString, PlatformPluginDescriptor>& descriptorsById,
    QHash<QString, QString>& capabilityProviders,
    QSet<QString>& visited,
    QVector<PlatformPluginDescriptor>& managedDescriptors,
    QString* error)
{
    if (visited.contains(pluginId)) return;
    if (!descriptorsById.contains(pluginId)) {
        if (error) *error = QStringLiteral("missing descriptor for managed dependency: %1").arg(pluginId);
        return;
    }

    const auto descriptor = descriptorsById.value(pluginId);
    visited.insert(pluginId);

    for (const auto& requiredPluginId : descriptor.required.plugins) {
        appendManagedDescriptorRecursively(
            requiredPluginId,
            descriptorsById,
            capabilityProviders,
            visited,
            managedDescriptors,
            error);
        if (error && !error->isEmpty()) return;
    }

    for (const auto& requiredCapability : descriptor.required.capabilities) {
        if (!capabilityProviders.contains(requiredCapability)) {
            if (error) *error = QStringLiteral("missing provider for managed capability: %1").arg(requiredCapability);
            return;
        }

        appendManagedDescriptorRecursively(
            capabilityProviders.value(requiredCapability),
            descriptorsById,
            capabilityProviders,
            visited,
            managedDescriptors,
            error);
        if (error && !error->isEmpty()) return;
    }

    managedDescriptors.append(descriptor);
}

bool validateManagedDescriptor(const PlatformPluginDescriptor& descriptor, QString* error)
{
    if (descriptor.runtime.ctkSymbolicName.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("descriptor missing runtime.ctk_symbolic_name: %1").arg(descriptor.id);
        return false;
    }
    if (descriptor.diagnostics.requiredServices.isEmpty()) {
        if (error) *error = QStringLiteral("descriptor missing diagnostics.required_services: %1").arg(descriptor.id);
        return false;
    }
    if (descriptor.diagnostics.serviceReadyTimeoutMs <= 0) {
        if (error) *error = QStringLiteral("descriptor missing diagnostics.service_ready_timeout_ms: %1").arg(descriptor.id);
        return false;
    }
    if (descriptor.healthChecks.isEmpty()) {
        if (error) *error = QStringLiteral("descriptor missing health_checks: %1").arg(descriptor.id);
        return false;
    }
    return true;
}
}

PlatformManagedPluginPlan PlatformManagedPluginPlanBuilder::build(
    const PlatformRuntimeConfig& runtimeConfig,
    const QVector<PlatformPluginDescriptor>& descriptors,
    const QString& pluginDirectory,
    QString* error)
{
    if (error) error->clear();

    QHash<QString, PlatformPluginDescriptor> descriptorsById;
    QHash<QString, QString> capabilityProviders;
    descriptorsById.reserve(descriptors.size());
    capabilityProviders.reserve(descriptors.size());
    for (const auto& descriptor : descriptors) {
        descriptorsById.insert(descriptor.id, descriptor);
        for (const auto& capability : descriptor.provides.capabilities) {
            if (!capabilityProviders.contains(capability)) {
                capabilityProviders.insert(capability, descriptor.id);
            }
        }
    }

    QVector<PlatformPluginDescriptor> managedDescriptors;
    QSet<QString> visited;
    for (const auto& pluginId : runtimeConfig.corePluginIds) {
        appendManagedDescriptorRecursively(
            pluginId,
            descriptorsById,
            capabilityProviders,
            visited,
            managedDescriptors,
            error);
        if (error && !error->isEmpty()) return {};
    }

    const auto dependencyGraph = PlatformDependencyGraph::build(managedDescriptors);
    if (!dependencyGraph.errors.isEmpty()) {
        if (error) *error = dependencyGraph.errors.join(QStringLiteral("; "));
        return {};
    }

    PlatformManagedPluginPlan plan;
    plan.runtimeMode = runtimeConfig.runtimeMode;
    plan.managedPluginIds = dependencyGraph.coreStartupOrder;
    plan.corePluginIds = dependencyGraph.coreStartupOrder;

    for (const auto& pluginId : dependencyGraph.coreStartupOrder) {
        const auto descriptor = descriptorsById.value(pluginId);
        if (!validateManagedDescriptor(descriptor, error)) return {};

        PlatformManagedPluginPlanEntry entry;
        entry.pluginId = descriptor.id;
        entry.displayName = descriptor.displayName;
        entry.ctkSymbolicName = descriptor.runtime.ctkSymbolicName.trimmed();
        entry.bootstrapLevel = descriptor.runtime.bootstrapLevel;
        entry.startupPolicy = descriptor.runtime.startupPolicy;
        entry.requiredPlugins = descriptor.required.plugins;
        entry.requiredCapabilities = descriptor.required.capabilities;
        entry.requiredServices = descriptor.diagnostics.requiredServices;
        entry.healthChecks = descriptor.healthChecks;
        entry.serviceReadyTimeoutMs = descriptor.diagnostics.serviceReadyTimeoutMs;
        entry.bundleFilePath = resolveBundlePath(pluginDirectory, entry.ctkSymbolicName);
        if (entry.bundleFilePath.isEmpty()) {
            if (error) *error = QStringLiteral("missing runtime bundle for managed plugin: %1").arg(pluginId);
            return {};
        }
        plan.installEntries.append(entry);
    }

    return plan;
}
```

```cmake
# CMakeLists.txt
list(APPEND FRAMEWORK_SOURCES
    Framework/Platform/Kernel/PlatformManagedPluginPlan.cpp
)
```

- [x] **Step 4: 鎶婇粯璁よ繍琛屾ā寮忓垏鍒?`facade_mode`锛屽苟琛ラ綈棣栨壒 descriptor 鐨?diagnostics 鍧?*

```json
// config/platform_runtime.json
{
  "runtime_mode": "facade_mode",
  "descriptor_directory": "plugins/descriptors",
  "core_plugin_ids": [
    "org.medicalpro.user_management",
    "org.medicalpro.dicom_viewer",
    "org.medicalpro.four_view_display"
  ]
}
```

```json
// Plugins/UserManagement/platform/plugin.json
"diagnostics": {
  "required_services": ["identity.user_query", "identity.session"],
  "service_ready_timeout_ms": 5000,
  "warmup_tasks": [],
  "warmup_timeout_ms": 0,
  "warmup_impacts_ready": false,
  "degrade_on": []
},
"health_checks": ["service_registered", "database_accessible"]
```

```json
// Plugins/DicomViewer/platform/plugin.json
"diagnostics": {
  "required_services": ["imaging.study_query", "imaging.study_reader"],
  "service_ready_timeout_ms": 8000,
  "warmup_tasks": [],
  "warmup_timeout_ms": 0,
  "warmup_impacts_ready": false,
  "degrade_on": []
},
"health_checks": ["service_registered", "data_path_accessible"]
```

```json
// Plugins/FourViewDisplay/platform/plugin.json
"diagnostics": {
  "required_services": ["imaging.viewport", "imaging.viewport_state"],
  "service_ready_timeout_ms": 8000,
  "warmup_tasks": [],
  "warmup_timeout_ms": 0,
  "warmup_impacts_ready": false,
  "degrade_on": []
},
"health_checks": ["service_registered", "render_backend_accessible"]
```

- [ ] **Step 5: 閲嶆柊杩愯娴嬭瘯锛岀‘璁?plan builder 涓庨厤缃垏鎹㈤€氳繃锛岀劧鍚庢彁浜ょ涓€涓壒娆?*

Run:

```powershell
cmake --build build_x64 --config Release --target platform_managed_plugin_plan_test platform_descriptor_loader_test platform_startup_coordinator_test
ctest --test-dir build_x64 -C Release -R "platform_managed_plugin_plan_test|platform_descriptor_loader_test|platform_startup_coordinator_test" --output-on-failure
```

Expected:

- `platform_managed_plugin_plan_test` PASS
- `platform_descriptor_loader_test` PASS
- `platform_startup_coordinator_test` PASS

```powershell
git add CMakeLists.txt config/platform_runtime.json Framework/Platform/Kernel/PlatformManagedPluginPlan.h Framework/Platform/Kernel/PlatformManagedPluginPlan.cpp Plugins/UserManagement/platform/plugin.json Plugins/DicomViewer/platform/plugin.json Plugins/FourViewDisplay/platform/plugin.json tests/unit/CMakeLists.txt tests/unit/PlatformManagedPluginPlanTest.cpp
git commit -m "feat: add managed startup plan builder"
```

### Task 2: 璁?state store 鍜?diagnostics 鍙寜鍙楃鑼冨洿璁＄畻涓婚摼缁撹

**Files:**
- Modify: `Framework/Platform/Contracts/PlatformSnapshots.h`
- Modify: `Framework/Platform/Kernel/PlatformStateStore.h`
- Modify: `Framework/Platform/Kernel/PlatformStateStore.cpp`
- Modify: `Framework/Platform/Diagnostics/PlatformDiagnosticsService.cpp`
- Modify: `tests/unit/PlatformDependencyGraphTest.cpp`
- Modify: `tests/unit/PlatformDiagnosticsServiceTest.cpp`

- [x] **Step 1: 鍏堝啓 failing tests锛岄攣瀹氣€滈潪鍙楃 descriptor 涓嶅啀闃绘柇 platform ready锛宒iagnostics 鑳借鏄庤皝琚帓闄も€?*

```cpp
// tests/unit/PlatformDependencyGraphTest.cpp
void capabilitySnapshot_ignores_unmanaged_descriptors_for_platform_ready();

void PlatformDependencyGraphTest::capabilitySnapshot_ignores_unmanaged_descriptors_for_platform_ready()
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
            {QStringLiteral("registration.core")})
    });
    store.setManagedPluginIds(QStringList{QStringLiteral("org.medicalpro.user_management")});
    store.setPluginState(QStringLiteral("org.medicalpro.user_management"), PlatformPluginState::Ready);

    const auto snapshot = store.capabilitySnapshot();
    QVERIFY(snapshot.platformReady);
    QCOMPARE(snapshot.unlockedCapabilities, (QStringList{QStringLiteral("identity.core")}));
    QVERIFY(!snapshot.unlockedCapabilities.contains(QStringLiteral("registration.core")));
}
```

```cpp
// tests/unit/PlatformDiagnosticsServiceTest.cpp
void buildSnapshot_reports_managed_scope_and_excluded_plugins();

void PlatformDiagnosticsServiceTest::buildSnapshot_reports_managed_scope_and_excluded_plugins()
{
    PlatformStateStore store;
    store.replaceDescriptors({
        makeDescriptor(QStringLiteral("org.medicalpro.user_management"), QStringLiteral("UserManagement")),
        makeDescriptor(QStringLiteral("org.medicalpro.registration_core"), QStringLiteral("RegistrationCore"), PlatformBootstrapLevel::Deferred, PlatformStartupPolicy::OnDemand)
    });
    store.setManagedPluginIds(QStringList{QStringLiteral("org.medicalpro.user_management")});
    store.setRuntimeMode(PlatformRuntimeMode::FacadeMode);
    store.setPluginState(QStringLiteral("org.medicalpro.user_management"), PlatformPluginState::Ready);

    PlatformDiagnosticsService service(&store);
    const auto snapshot = service.buildSnapshot({});

    QCOMPARE(snapshot.managedPluginIds, (QStringList{QStringLiteral("org.medicalpro.user_management")}));
    QCOMPARE(snapshot.excludedPluginIds, (QStringList{QStringLiteral("org.medicalpro.registration_core")}));
    QVERIFY(std::any_of(snapshot.problems.begin(), snapshot.problems.end(), [](const PlatformDiagnosticProblem& problem) {
        return problem.reasonCode == QStringLiteral("excluded_from_managed_startup");
    }));
}
```

- [x] **Step 2: 杩愯鐩稿叧娴嬭瘯锛岀‘璁ゅ綋鍓嶅厛澶辫触**

Run:

```powershell
cmake --build build_x64 --config Release --target platform_dependency_graph_test platform_diagnostics_service_test
ctest --test-dir build_x64 -C Release -R "platform_dependency_graph_test|platform_diagnostics_service_test" --output-on-failure
```

Expected:

- 缂栬瘧澶辫触锛屽洜涓?`PlatformStateStore::setManagedPluginIds`銆乣managedPluginIds`銆乣excludedPluginIds` 杩樹笉瀛樺湪

- [x] **Step 3: 鍦?snapshots銆乻tate store 鍜?diagnostics service 涓帴鍏?managed scope**

```cpp
// Framework/Platform/Contracts/PlatformSnapshots.h
struct PlatformDiagnosticSnapshot
{
    PlatformDiagnosticSummary summary;
    PlatformCapabilitySnapshot capabilitySnapshot;
    QVector<PlatformPluginLifecycleSnapshot> pluginLifecycle;
    QVector<PlatformStartupTraceEntry> startupTrace;
    QVector<PlatformDiagnosticProblem> problems;
    QStringList recoveryHints;
    QStringList managedPluginIds;
    QStringList excludedPluginIds;

    bool frameworkReady = false;
    PlatformRuntimeMode runtimeMode = PlatformRuntimeMode::ObserveOnly;
    QVector<PlatformPluginRuntimeSnapshot> plugins;
};
```

```cpp
// Framework/Platform/Kernel/PlatformStateStore.h
class FRAMEWORK_EXPORT PlatformStateStore
{
public:
    void replaceDescriptors(const QVector<PlatformPluginDescriptor>& descriptors);
    void setRuntimeMode(PlatformRuntimeMode runtimeMode);
    void setManagedPluginIds(const QStringList& pluginIds);
    QStringList managedPluginIds() const;
    void setPluginState(const QString& pluginId, PlatformPluginState state);
    QVector<PlatformPluginDescriptor> descriptors() const;
    PlatformCapabilitySnapshot capabilitySnapshot() const;
    QVector<PlatformPluginRuntimeSnapshot> pluginSnapshots() const;

private:
    bool isManagedPlugin(const QString& pluginId) const;
    bool isCapabilityUnlocked(const QString& capability) const;
    bool isPluginReady(const QString& pluginId) const;
    void refreshSnapshots();
    void refreshSnapshot(const QString& pluginId);

    PlatformRuntimeMode m_runtimeMode = PlatformRuntimeMode::ObserveOnly;
    QStringList m_descriptorOrder;
    QStringList m_managedPluginIds;
    QHash<QString, PlatformPluginDescriptor> m_descriptors;
    QHash<QString, PlatformPluginRuntimeSnapshot> m_snapshots;
    mutable QReadWriteLock m_lock;
};
```

```cpp
// Framework/Platform/Kernel/PlatformStateStore.cpp
void PlatformStateStore::setManagedPluginIds(const QStringList& pluginIds)
{
    QWriteLocker locker(&m_lock);
    m_managedPluginIds = pluginIds;
    refreshSnapshots();
}

QStringList PlatformStateStore::managedPluginIds() const
{
    QReadLocker locker(&m_lock);
    return m_managedPluginIds;
}

bool PlatformStateStore::isManagedPlugin(const QString& pluginId) const
{
    return m_managedPluginIds.isEmpty() || m_managedPluginIds.contains(pluginId);
}

PlatformCapabilitySnapshot PlatformStateStore::capabilitySnapshot() const
{
    QReadLocker locker(&m_lock);
    PlatformCapabilitySnapshot snapshot;
    snapshot.runtimeMode = m_runtimeMode;

    for (const auto& pluginId : m_descriptorOrder) {
        if (!isManagedPlugin(pluginId)) continue;
        const auto descriptor = m_descriptors.value(pluginId);
        const auto pluginSnapshot = m_snapshots.value(pluginId);
        const bool ready = pluginSnapshot.state == PlatformPluginState::Ready;

        if (pluginSnapshot.state == PlatformPluginState::Degraded
            || pluginSnapshot.state == PlatformPluginState::Failed) {
            snapshot.degradedPlugins.append(pluginId);
        }

        for (const auto& capability : descriptor.provides.capabilities) {
            if (ready) snapshot.unlockedCapabilities.append(capability);
            else snapshot.lockedCapabilities.append(capability);
        }
    }

    snapshot.platformReady = snapshot.lockedCapabilities.isEmpty() && snapshot.degradedPlugins.isEmpty();
    return snapshot;
}
```

```cpp
// Framework/Platform/Diagnostics/PlatformDiagnosticsService.cpp
PlatformDiagnosticSnapshot PlatformDiagnosticsService::buildSnapshot(const PlatformRuntimeObservation& observation) const
{
    PlatformDiagnosticSnapshot snapshot;
    snapshot.startupTrace = observation.startupTrace;
    snapshot.frameworkReady = observation.frameworkReady;

    QVector<PlatformPluginDescriptor> descriptors;
    QStringList managedPluginIds;
    if (m_stateStore) {
        descriptors = m_stateStore->descriptors();
        managedPluginIds = m_stateStore->managedPluginIds();
        snapshot.plugins = m_stateStore->pluginSnapshots();
        snapshot.capabilitySnapshot = m_stateStore->capabilitySnapshot();
        snapshot.runtimeMode = snapshot.capabilitySnapshot.runtimeMode;
    }

    snapshot.managedPluginIds = managedPluginIds;
    for (const auto& descriptor : descriptors) {
        if (managedPluginIds.contains(descriptor.id)) continue;
        snapshot.excludedPluginIds.append(descriptor.id);

        PlatformDiagnosticProblem problem;
        problem.severity = PlatformDiagnosticSeverity::Info;
        problem.pluginId = descriptor.id;
        problem.reasonCode = QStringLiteral("excluded_from_managed_startup");
        problem.detail = QStringLiteral("Plugin is available in descriptors but excluded from the current managed startup scope");
        snapshot.problems.append(problem);
    }

    // existing aggregation follows...
```

- [x] **Step 4: 閲嶆柊杩愯娴嬭瘯锛岀‘璁ゅ彈绠?scope 宸茬敓鏁?*

Run:

```powershell
cmake --build build_x64 --config Release --target platform_dependency_graph_test platform_diagnostics_service_test
ctest --test-dir build_x64 -C Release -R "platform_dependency_graph_test|platform_diagnostics_service_test" --output-on-failure
```

Expected:

- `platform_dependency_graph_test` PASS
- `platform_diagnostics_service_test` PASS

- [ ] **Step 5: 鎻愪氦 managed scope 涓?diagnostics 鏀跺彛**

```powershell
git add Framework/Platform/Contracts/PlatformSnapshots.h Framework/Platform/Kernel/PlatformStateStore.h Framework/Platform/Kernel/PlatformStateStore.cpp Framework/Platform/Diagnostics/PlatformDiagnosticsService.cpp tests/unit/PlatformDependencyGraphTest.cpp tests/unit/PlatformDiagnosticsServiceTest.cpp
git commit -m "feat: scope platform readiness to managed plugins"
```

### Task 3: 鎶?install / core start / service ready 鐪熸浜ょ粰 coordinator 鎵ц

**Files:**
- Modify: `Framework/Platform/Kernel/PlatformStartupCoordinator.h`
- Modify: `Framework/Platform/Kernel/PlatformStartupCoordinator.cpp`
- Modify: `main.cpp`
- Modify: `tests/unit/PlatformStartupCoordinatorTest.cpp`
- Modify: `tests/unit/StartupOrchestratorLifecycleTest.cpp`

- [ ] **Step 1: 鍏堣ˉ failing tests锛岄攣瀹氣€滃彧瀹夎鍙楃娓呭崟 + service ready 閫昏緫涓嶅啀鍐呰仈鍦?main.cpp鈥?*

```cpp
// tests/unit/PlatformStartupCoordinatorTest.cpp
void installManagedPlugins_installs_only_planned_entries();
void waitForServiceReady_returns_timeout_with_missing_dependencies();

void PlatformStartupCoordinatorTest::installManagedPlugins_installs_only_planned_entries()
{
    QStringList installedBundles;
    PlatformStartupCoordinator coordinator(PlatformRuntimeMode::FacadeMode, {});

    PlatformManagedPluginPlan plan;
    PlatformManagedPluginPlanEntry user;
    user.pluginId = QStringLiteral("org.medicalpro.user_management");
    user.ctkSymbolicName = QStringLiteral("UserManagement");
    user.bundleFilePath = QStringLiteral("C:/runtime/plugins/UserManagement.dll");
    PlatformManagedPluginPlanEntry dicom;
    dicom.pluginId = QStringLiteral("org.medicalpro.dicom_viewer");
    dicom.ctkSymbolicName = QStringLiteral("DicomViewer");
    dicom.bundleFilePath = QStringLiteral("C:/runtime/plugins/DicomViewer.dll");
    plan.installEntries = {user, dicom};

    QVERIFY(coordinator.installManagedPlugins(plan, [&installedBundles](const PlatformManagedPluginPlanEntry& entry) {
        installedBundles.append(entry.bundleFilePath);
        return true;
    }));

    QCOMPARE(installedBundles, (QStringList{
        QStringLiteral("C:/runtime/plugins/UserManagement.dll"),
        QStringLiteral("C:/runtime/plugins/DicomViewer.dll")
    }));
}

void PlatformStartupCoordinatorTest::waitForServiceReady_returns_timeout_with_missing_dependencies()
{
    PlatformLifecycleTraceRecorder recorder;
    recorder.beginSession(PlatformRuntimeMode::FacadeMode);

    PlatformStartupCoordinator coordinator(
        PlatformRuntimeMode::FacadeMode,
        [](const QString&) { return true; },
        {},
        &recorder);

    PlatformManagedPluginPlanEntry entry;
    entry.pluginId = QStringLiteral("org.medicalpro.four_view_display");
    entry.ctkSymbolicName = QStringLiteral("FourViewDisplay");
    entry.requiredServices = QStringList{QStringLiteral("imaging.viewport")};
    entry.serviceReadyTimeoutMs = 100;

    const auto outcome = coordinator.waitForServiceReady(
        entry,
        {
            [](const QStringList&) { return QStringList{QStringLiteral("imaging.viewport")}; },
            [](const QString&) { return QStringList{}; },
            [](const QString&) { return QStringList{}; }
        },
        10);

    QVERIFY(!outcome.success);
    QCOMPARE(outcome.reasonCode, QStringLiteral("service_ready_timeout"));
}
```

```cpp
// tests/unit/StartupOrchestratorLifecycleTest.cpp
void facade_mode_managed_startup_skips_deferred_phase_after_managed_core_success();
```

- [ ] **Step 2: 杩愯鐩稿叧娴嬭瘯锛岀‘璁ょ洰鍓嶅厛澶辫触**

Run:

```powershell
cmake --build build_x64_noctk --config Release --target platform_startup_coordinator_test startup_orchestrator_lifecycle_test
ctest --test-dir build_x64_noctk -C Release -R "platform_startup_coordinator_test|startup_orchestrator_lifecycle_test" --output-on-failure
```

Expected:

- 缂栬瘧澶辫触锛屽洜涓?`installManagedPlugins` 鍜?`waitForServiceReady` 杩樹笉瀛樺湪

- [ ] **Step 3: 鎵╁睍 coordinator锛岃瀹冩帴鏀?managed plan entry 骞惰礋璐?install / service ready**

```cpp
// Framework/Platform/Kernel/PlatformStartupCoordinator.h
struct PlatformServiceReadyProbeSet
{
    std::function<QStringList(const QStringList&)> missingServicesFn;
    std::function<QStringList(const QString&)> missingPluginsFn;
    std::function<QStringList(const QString&)> missingCapabilitiesFn;
};

struct PlatformServiceReadyOutcome
{
    bool success = true;
    PlatformPluginState finalState = PlatformPluginState::Ready;
    QString reasonCode;
    QString detail;
    QStringList missingServices;
    QStringList missingPlugins;
    QStringList missingCapabilities;
};

class FRAMEWORK_EXPORT PlatformStartupCoordinator
{
public:
    using InstallManagedPluginFn = std::function<bool(const PlatformManagedPluginPlanEntry&)>;

    bool installManagedPlugins(
        const PlatformManagedPluginPlan& plan,
        const InstallManagedPluginFn& installManagedPluginFn);

    PlatformServiceReadyOutcome waitForServiceReady(
        const PlatformManagedPluginPlanEntry& entry,
        const PlatformServiceReadyProbeSet& probes,
        int pollIntervalMs = 50) const;
```

```cpp
// Framework/Platform/Kernel/PlatformStartupCoordinator.cpp
bool PlatformStartupCoordinator::installManagedPlugins(
    const PlatformManagedPluginPlan& plan,
    const InstallManagedPluginFn& installManagedPluginFn)
{
    if (!shouldInstallPlugins()) return true;

    for (const auto& entry : plan.installEntries) {
        if (m_recorder) {
            m_recorder->recordPluginStepStarted(
                entry.pluginId,
                entry.ctkSymbolicName,
                PlatformLifecycleStep::Install,
                false);
        }
        const bool installed = installManagedPluginFn && installManagedPluginFn(entry);
        if (!installed) {
            if (m_recorder) {
                m_recorder->recordPluginStepFinished(
                    entry.pluginId,
                    entry.ctkSymbolicName,
                    PlatformLifecycleStep::Install,
                    PlatformLifecycleResult::Failed,
                    QStringLiteral("install_failed"),
                    QStringLiteral("Managed bundle install failed"));
            }
            return false;
        }
        if (m_recorder) {
            m_recorder->recordPluginStepFinished(
                entry.pluginId,
                entry.ctkSymbolicName,
                PlatformLifecycleStep::Install,
                PlatformLifecycleResult::Succeeded,
                QStringLiteral("install_succeeded"),
                QStringLiteral("Managed bundle install succeeded"));
        }
    }

    return true;
}

PlatformServiceReadyOutcome PlatformStartupCoordinator::waitForServiceReady(
    const PlatformManagedPluginPlanEntry& entry,
    const PlatformServiceReadyProbeSet& probes,
    int pollIntervalMs) const
{
    PlatformServiceReadyOutcome outcome;
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < entry.serviceReadyTimeoutMs) {
        outcome.missingServices = probes.missingServicesFn ? probes.missingServicesFn(entry.requiredServices) : QStringList{};
        outcome.missingPlugins = probes.missingPluginsFn ? probes.missingPluginsFn(entry.pluginId) : QStringList{};
        outcome.missingCapabilities = probes.missingCapabilitiesFn ? probes.missingCapabilitiesFn(entry.pluginId) : QStringList{};

        if (outcome.missingServices.isEmpty()
            && outcome.missingPlugins.isEmpty()
            && outcome.missingCapabilities.isEmpty()) {
            outcome.finalState = PlatformPluginState::Ready;
            outcome.reasonCode = QStringLiteral("service_ready");
            outcome.detail = QStringLiteral("Required services and dependencies are ready");
            return outcome;
        }

        QThread::msleep(pollIntervalMs);
    }

    outcome.success = false;
    outcome.finalState = PlatformPluginState::Failed;
    outcome.reasonCode = QStringLiteral("service_ready_timeout");
    outcome.detail = QStringLiteral("Timed out while waiting for managed plugin service readiness");
    return outcome;
}
```

- [ ] **Step 4: 鐢?managed plan 鏀瑰啓 `main.cpp` 鐨?plugin install/core start/service ready 璺緞**

Status update 2026-04-20:

- Completed. `main.cpp` now builds the managed startup plan, writes managed scope into `PlatformStateStore`, routes plugin installation through `installManagedPlugins`, and routes core start plus `service ready` polling through `PlatformStartupCoordinator`.

```cpp
// main.cpp
#include "Framework/Platform/Kernel/PlatformManagedPluginPlan.h"

struct StartupRuntimeContext
{
    PlatformManagedPluginPlan managedPlan;
    std::unique_ptr<PlatformWarmupCoordinator> warmupCoordinator;
    // existing fields stay here...
};

const QString pluginsPath = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("plugins"));
QString managedPlanError;
const auto managedPlan = PlatformManagedPluginPlanBuilder::build(
    runtimeConfig,
    descriptors,
    pluginsPath,
    &managedPlanError);
if (!managedPlanError.isEmpty()) {
    throw std::runtime_error(
        QStringLiteral("Failed to build managed startup plan: %1").arg(managedPlanError).toStdString());
}

mainInterface->platformStateStore()->replaceDescriptors(descriptors);
mainInterface->platformStateStore()->setRuntimeMode(runtimeConfig.runtimeMode);
mainInterface->platformStateStore()->setManagedPluginIds(managedPlan.managedPluginIds);
startupContext->managedPlan = managedPlan;
```

```cpp
// main.cpp - StartupPhase::PluginInstallation
orchestrator->registerPhaseHandler(
    StartupPhase::PluginInstallation,
    [ctkManager, startupContext](QApplication*) -> StartupOrchestrator::PhaseExecutionResult {
        if (!startupContext->startupCoordinator.shouldInstallPlugins()) {
            return StartupOrchestrator::PhaseExecutionResult::skipped(
                QStringLiteral("Managed plugin install skipped in observe_only mode"));
        }

        const bool installed = startupContext->startupCoordinator.installManagedPlugins(
            startupContext->managedPlan,
            [ctkManager](const PlatformManagedPluginPlanEntry& entry) {
                return ctkManager->installPlugin(entry.bundleFilePath, false, nullptr);
            });

        return installed;
    });
```

```cpp
// main.cpp - StartupPhase::CriticalPluginStart
for (const auto& entry : startupContext->managedPlan.installEntries) {
    if (!startupContext->startupCoordinator.startCorePlugin(entry.pluginId)) {
        return false;
    }

    const auto outcome = startupContext->startupCoordinator.waitForServiceReady(
        entry,
        {
            [ctkManager](const QStringList& requiredServices) {
                return ctkManager->getMissingServices(requiredServices);
            },
            [startupContext, ctkManager](const QString& pluginId) {
                return startupContext->missingRequiredPlugins(pluginId, ctkManager);
            },
            [startupContext, ctkManager](const QString& pluginId) {
                return startupContext->missingRequiredCapabilities(pluginId, ctkManager);
            }
        });

    const auto identity = startupContext->resolveByPlatformPluginId(entry.pluginId);
    applyPluginState(identity, outcome.finalState);
    startupContext->lifecycleRecorder.recordPluginStepStarted(
        identity.pluginId,
        identity.ctkSymbolicName,
        PlatformLifecycleStep::ServiceReady,
        true);
    startupContext->lifecycleRecorder.recordPluginStepFinished(
        identity.pluginId,
        identity.ctkSymbolicName,
        PlatformLifecycleStep::ServiceReady,
        outcome.success ? PlatformLifecycleResult::Succeeded : PlatformLifecycleResult::Timeout,
        outcome.reasonCode,
        outcome.detail);

    if (!outcome.success) return false;
}
```

- [ ] **Step 5: 璺戦€?coordinator/orchestrator 娴嬭瘯骞舵彁浜や富鍚姩閾炬敹鍙?*

2026-04-20 update:

- Acceptance rerun passed on `build_x64`, but this step remains open because the commit batch has not been created yet.
- A pre-existing `NewPagesLib` baseline issue blocked `medicalpro` link at first: `UI/NewPages/CMakeLists.txt` appended `RegistrationWorkflow` / `PointRegistrationService` after `add_library(NewPagesLib ...)`, so those sources were not compiled into the target and `AUTOMOC` did not generate the required meta-object code.
- The blocker was resolved by switching that wiring to `target_sources(NewPagesLib ...)`, after which `medicalpro` and the Task 3 acceptance tests passed.

Run:

```powershell
cmake --build build_x64 --config Release --target medicalpro platform_managed_plugin_plan_test platform_startup_coordinator_test startup_orchestrator_lifecycle_test
ctest --test-dir build_x64 -C Release -R "platform_managed_plugin_plan_test|platform_startup_coordinator_test|startup_orchestrator_lifecycle_test" --output-on-failure
```

Expected:

- `platform_startup_coordinator_test` PASS
- `startup_orchestrator_lifecycle_test` PASS
- `medicalpro` build target PASS

```powershell
git add Framework/Platform/Kernel/PlatformStartupCoordinator.h Framework/Platform/Kernel/PlatformStartupCoordinator.cpp main.cpp tests/unit/PlatformStartupCoordinatorTest.cpp tests/unit/StartupOrchestratorLifecycleTest.cpp
git commit -m "feat: route startup through managed plugin plan"
```

### Task 4: 鎶?warmup 灏炬浠?`main.cpp` 鎶芥垚鐙珛 coordinator锛屽苟淇濇寔 Phase 1 闈為樆鏂?
**Files:**
- Modify: `CMakeLists.txt`
- Create: `Framework/Platform/Kernel/PlatformWarmupCoordinator.h`
- Create: `Framework/Platform/Kernel/PlatformWarmupCoordinator.cpp`
- Modify: `main.cpp`
- Modify: `tests/unit/CMakeLists.txt`
- Create: `tests/unit/PlatformWarmupCoordinatorTest.cpp`

- [ ] **Step 1: 鍏堝啓 failing tests锛岄攣瀹氣€渇acade mode 蹇呴』 skip锛宮ain.cpp 涓嶅啀鐩存帴璁块棶鍏蜂綋 plugin service 绫诲瀷鈥?*

```cmake
# tests/unit/CMakeLists.txt
add_executable(platform_warmup_coordinator_test
    PlatformWarmupCoordinatorTest.cpp
)

target_include_directories(platform_warmup_coordinator_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(platform_warmup_coordinator_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME platform_warmup_coordinator_test
    COMMAND platform_warmup_coordinator_test
)
```

```cpp
// tests/unit/PlatformWarmupCoordinatorTest.cpp
#include <QtTest/QtTest>

#include "Framework/Platform/Diagnostics/PlatformLifecycleTraceRecorder.h"
#include "Framework/Platform/Kernel/PlatformWarmupCoordinator.h"

class PlatformWarmupCoordinatorTest : public QObject
{
    Q_OBJECT

private slots:
    void run_skips_all_warmup_entries_outside_orchestrate_core();
    void run_with_empty_phase1_plan_returns_non_blocking_success();
};

void PlatformWarmupCoordinatorTest::run_skips_all_warmup_entries_outside_orchestrate_core()
{
    PlatformLifecycleTraceRecorder recorder;
    recorder.beginSession(PlatformRuntimeMode::FacadeMode);

    PlatformManagedPluginPlan plan;
    PlatformManagedPluginPlanEntry entry;
    entry.pluginId = QStringLiteral("org.medicalpro.dicom_viewer");
    entry.ctkSymbolicName = QStringLiteral("DicomViewer");
    plan.installEntries = {entry};

    PlatformWarmupCoordinator coordinator(&recorder);
    const auto outcome = coordinator.run(
        plan,
        PlatformRuntimeMode::FacadeMode,
        [](const PlatformManagedPluginPlanEntry&) {
            QFAIL("warmup step should not run in facade_mode");
            return PlatformLifecycleResult::Succeeded;
        });

    QVERIFY(outcome.success);
    QCOMPARE(outcome.result, PlatformLifecycleResult::Skipped);
}

void PlatformWarmupCoordinatorTest::run_with_empty_phase1_plan_returns_non_blocking_success()
{
    PlatformLifecycleTraceRecorder recorder;
    recorder.beginSession(PlatformRuntimeMode::OrchestrateCore);

    PlatformWarmupCoordinator coordinator(&recorder);
    const auto outcome = coordinator.run({}, PlatformRuntimeMode::OrchestrateCore, {});

    QVERIFY(outcome.success);
    QCOMPARE(outcome.result, PlatformLifecycleResult::Succeeded);
}

QTEST_APPLESS_MAIN(PlatformWarmupCoordinatorTest)
#include "PlatformWarmupCoordinatorTest.moc"
```

Status update 2026-04-20:

- Completed. Added `platform_warmup_coordinator_test` and verified the RED step first: the target initially failed because `PlatformWarmupCoordinator` did not exist.

- [ ] **Step 2: 杩愯 warmup 鏂版祴璇曪紝纭鍏堝け璐?*

Run:

```powershell
cmake --build build_x64 --config Release --target platform_warmup_coordinator_test
ctest --test-dir build_x64 -C Release -R platform_warmup_coordinator_test --output-on-failure
```

Expected:

- 缂栬瘧澶辫触锛屽洜涓?`PlatformWarmupCoordinator` 杩樹笉瀛樺湪

- [ ] **Step 3: 鏂板缓 warmup coordinator锛屽苟鎶?`main.cpp` 閲岀殑纭紪鐮?warmup lambda 鏇挎崲鎴?helper 璋冪敤**

```cpp
// Framework/Platform/Kernel/PlatformWarmupCoordinator.h
#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/PlatformRuntimeTypes.h"
#include "Framework/Platform/Kernel/PlatformManagedPluginPlan.h"

#include <functional>

class PlatformLifecycleTraceRecorder;

struct PlatformWarmupOutcome
{
    bool success = true;
    PlatformLifecycleResult result = PlatformLifecycleResult::Succeeded;
    QString reasonCode;
    QString detail;
};

class FRAMEWORK_EXPORT PlatformWarmupCoordinator
{
public:
    using WarmupStepFn = std::function<PlatformLifecycleResult(const PlatformManagedPluginPlanEntry&)>;

    explicit PlatformWarmupCoordinator(PlatformLifecycleTraceRecorder* recorder = nullptr);
    PlatformWarmupOutcome run(
        const PlatformManagedPluginPlan& plan,
        PlatformRuntimeMode runtimeMode,
        const WarmupStepFn& warmupStepFn) const;

private:
    PlatformLifecycleTraceRecorder* m_recorder = nullptr;
};
```

```cpp
// Framework/Platform/Kernel/PlatformWarmupCoordinator.cpp
#include "Framework/Platform/Kernel/PlatformWarmupCoordinator.h"

#include "Framework/Platform/Diagnostics/PlatformLifecycleTraceRecorder.h"

PlatformWarmupCoordinator::PlatformWarmupCoordinator(PlatformLifecycleTraceRecorder* recorder)
    : m_recorder(recorder)
{
}

PlatformWarmupOutcome PlatformWarmupCoordinator::run(
    const PlatformManagedPluginPlan& plan,
    PlatformRuntimeMode runtimeMode,
    const WarmupStepFn& warmupStepFn) const
{
    if (runtimeMode != PlatformRuntimeMode::OrchestrateCore) {
        for (const auto& entry : plan.installEntries) {
            if (!m_recorder) continue;
            m_recorder->recordPluginStepStarted(entry.pluginId, entry.ctkSymbolicName, PlatformLifecycleStep::Warmup, false);
            m_recorder->recordPluginStepFinished(
                entry.pluginId,
                entry.ctkSymbolicName,
                PlatformLifecycleStep::Warmup,
                PlatformLifecycleResult::Skipped,
                QStringLiteral("skipped_by_mode"),
                QStringLiteral("Warmup skipped outside orchestrate_core"));
        }
        return {true, PlatformLifecycleResult::Skipped, QStringLiteral("skipped_by_mode"), QStringLiteral("Warmup skipped outside orchestrate_core")};
    }

    if (plan.installEntries.isEmpty() || !warmupStepFn) {
        return {true, PlatformLifecycleResult::Succeeded, QStringLiteral("no_warmup_tasks"), QStringLiteral("No managed warmup tasks configured")};
    }

    for (const auto& entry : plan.installEntries) {
        const auto result = warmupStepFn(entry);
        if (result == PlatformLifecycleResult::Failed) {
            return {false, PlatformLifecycleResult::Failed, QStringLiteral("warmup_failed"), QStringLiteral("Managed warmup step failed")};
        }
    }

    return {true, PlatformLifecycleResult::Succeeded, QStringLiteral("warmup_ready"), QStringLiteral("Managed warmup completed")};
}
```

```cpp
// main.cpp
#include "Framework/Platform/Kernel/PlatformWarmupCoordinator.h"
```

```cpp
// main.cpp
startupContext->warmupCoordinator = std::make_unique<PlatformWarmupCoordinator>(&startupContext->lifecycleRecorder);

orchestrator->registerPhaseHandler(
    StartupPhase::ServiceWarmup,
    [startupContext](QApplication*) -> StartupOrchestrator::PhaseExecutionResult {
        const auto outcome = startupContext->warmupCoordinator->run(
            startupContext->managedPlan,
            startupContext->runtimeConfig.runtimeMode,
            {});

        if (!outcome.success) {
            return StartupOrchestrator::PhaseExecutionResult {
                false,
                outcome.result,
                outcome.reasonCode,
                outcome.detail
            };
        }

        if (outcome.result == PlatformLifecycleResult::Skipped) {
            return StartupOrchestrator::PhaseExecutionResult::skipped(outcome.detail, outcome.reasonCode);
        }

        return true;
    });
```

Status update 2026-04-20:

- Completed. `PlatformWarmupCoordinator` is now compiled into `Framework`, `main.cpp` no longer directly includes `Registration2D3DService` / `FourViewDisplayService`, and the `ServiceWarmup` phase is routed through the coordinator.
- Phase 1 remains non-blocking for warmup: `facade_mode` records skip events for managed entries, and the current managed plan runs with `no_warmup_tasks` instead of hard-coded plugin service access.

- [ ] **Step 4: 鍒犻櫎 `main.cpp` 閲岀洿鎺ュ寘鍚拰璋冪敤鐨勫叿浣?warmup service 浠ｇ爜锛屽啀璺戞祴璇?*

```cpp
// main.cpp
// 鍒犻櫎锛?// #include "Plugins/Registration2D3D/Registration2D3DService.h"
// #include "Plugins/FourViewDisplay/FourViewDisplayService.h"
// 浠ュ強鏁存鐩存帴璁块棶 reg2D3DService / fourViewService 鐨?warmup lambda
```

Run:

```powershell
cmake --build build_x64 --config Release --target medicalpro platform_warmup_coordinator_test startup_orchestrator_lifecycle_test
ctest --test-dir build_x64 -C Release -R "platform_warmup_coordinator_test|startup_orchestrator_lifecycle_test" --output-on-failure
```

Expected:

- `platform_warmup_coordinator_test` PASS
- `startup_orchestrator_lifecycle_test` PASS
- `medicalpro` build target PASS

2026-04-20 update:

- Acceptance passed on `build_x64`.
- This step remains open only because the warmup extraction batch has not been committed yet.

- [ ] **Step 5: 鎻愪氦 main.cpp 娓呯悊涓?warmup 鎶界**

```powershell
git add CMakeLists.txt Framework/Platform/Kernel/PlatformWarmupCoordinator.h Framework/Platform/Kernel/PlatformWarmupCoordinator.cpp main.cpp tests/unit/CMakeLists.txt tests/unit/PlatformWarmupCoordinatorTest.cpp
git commit -m "refactor: extract managed warmup coordinator"
```

### Task 5: 鍥炲啓鏂囨。銆佸埛鏂伴獙鏀讹紝骞舵妸 Phase 1 鏀跺彛鎴愬彲鍥炴函鎵规

**Files:**
- Modify: `docs/current_status_and_project_overview.md`
- Modify: `docs/superpowers/tracking/platform-migration-decision-log.md`
- Modify: `docs/superpowers/specs/2026-04-16-platform-kernel-governance-design.md`

- [ ] **Step 1: 鍏堝啓鏂囨。鍥炲啓锛屾槑纭?Phase 1 宸茶惤鍦扮殑榛樿妯″紡銆佸彈绠¤寖鍥村拰杈圭晫**

```md
<!-- docs/current_status_and_project_overview.md -->
## 2026-04-20 Plugin Chain Remediation Phase 1 Acceptance

- Default runtime mode is now `facade_mode`.
- The managed startup scope is now limited to `UserManagement`, `DicomViewer`, and `FourViewDisplay`.
- Startup install and core activation now run from a descriptor-driven managed plan instead of scanning the entire `plugins/` directory as the main-source-of-truth.
- `platform ready` now evaluates only managed plugins.
- `ready` remains limited to `service registration + required plugin/capability satisfaction + lightweight health checks`.
- Warmup is no longer part of the blocking ready path in Phase 1.
```

```md
<!-- docs/superpowers/tracking/platform-migration-decision-log.md -->
## 2026-04-20

- Decision: switch the product default runtime mode to `facade_mode` for plugin-chain remediation Phase 1.
- Rationale: `observe_only` explains but does not stabilize the plugin chain; Phase 1 must actually govern framework init, managed install, core start, and service ready.
- Impact: the startup truth source is now `platform_runtime.json + descriptor-driven managed startup plan`, while `plugin_load_policy.json` is compatibility-only.

- Decision: treat `UserManagement`, `DicomViewer`, and `FourViewDisplay` as the only Phase 1 managed startup scope.
- Rationale: these three plugins define the minimum stable core path without forcing deferred/on-demand registration workflows into the first remediation slice.
- Impact: unmanaged plugins no longer block `platform ready`.
```

```md
<!-- docs/superpowers/specs/2026-04-16-platform-kernel-governance-design.md -->
## Plugin Chain Remediation Follow-up (2026-04-20)

- The default runtime mode is now `facade_mode` for the product path.
- The governed startup path is now generated from a descriptor-driven managed plan rather than from whole-directory plugin installation.
- Phase 1 managed scope is limited to `UserManagement`, `DicomViewer`, and `FourViewDisplay`.
- `platform ready` now evaluates only the managed Phase 1 scope.
```

- [ ] **Step 2: 杩愯瀹屾暣楠屾敹鍛戒护锛岀‘璁ゆ瀯寤恒€佸崟娴嬨€佽繍琛屾椂甯冨眬鍜屾簮鐮佹壂鎻忛兘閫氳繃**

Run:

```powershell
cmake --build build_x64_noctk --config Release --target medicalpro platform_descriptor_loader_test platform_managed_plugin_plan_test platform_dependency_graph_test platform_startup_coordinator_test platform_diagnostics_service_test platform_warmup_coordinator_test startup_orchestrator_lifecycle_test
ctest --test-dir build_x64_noctk -C Release -R "platform_descriptor_loader_test|platform_managed_plugin_plan_test|platform_dependency_graph_test|platform_startup_coordinator_test|platform_diagnostics_service_test|platform_warmup_coordinator_test|startup_orchestrator_lifecycle_test|platform_descriptor_runtime_layout_test" --output-on-failure
rg -n "installPluginsFromDirectory\\(|loadPluginPolicy\\(" main.cpp
```

Expected:

- `medicalpro` build target PASS
- 鎵€鏈?Phase 1 鍗曟祴 PASS
- `platform_descriptor_runtime_layout_test` PASS
- `rg` 瀵?`main.cpp` 鏃犺緭鍑猴紝璇存槑浜у搧涓婚摼涓嶅啀鐩存帴璋冪敤鏃х殑鍏ㄧ洰褰曞畨瑁呭叆鍙ｅ拰鏃х瓥鐣ュ姞杞藉叆鍙?
- [ ] **Step 3: 鎻愪氦鏂囨。涓庨獙鏀跺洖鍐?*

```powershell
git add docs/current_status_and_project_overview.md docs/superpowers/tracking/platform-migration-decision-log.md docs/superpowers/specs/2026-04-16-platform-kernel-governance-design.md
git commit -m "docs: record plugin chain remediation phase 1 acceptance"
```

## Self-Review

- Spec coverage:
  - `facade_mode` 榛樿妯″紡锛歍ask 1 / Task 3 / Task 5 瑕嗙洊
  - 棣栨壒涓夋彃浠跺彈绠¤寖鍥达細Task 1 / Task 5 瑕嗙洊
  - descriptor 椹卞姩 install source锛歍ask 1 / Task 3 瑕嗙洊
  - `ready` 涓?`warmup` 杈圭晫锛歍ask 1 / Task 3 / Task 4 瑕嗙洊
  - `CTKManager / StartupOrchestrator / PlatformStartupCoordinator / main.cpp / UI` 杈圭晫锛歍ask 2 / Task 3 / Task 4 瑕嗙洊
  - diagnostics / docs 鍥炲啓锛歍ask 2 / Task 5 瑕嗙洊
- Placeholder scan:
  - 宸查伩鍏嶅崰浣嶈瘝鍜屸€滅◢鍚庤ˉ涓娾€濆紡鎻忚堪
  - 鎵€鏈夋祴璇曞悕銆佹枃浠惰矾寰勩€佸懡浠ゅ拰鎻愪氦淇℃伅閮藉凡鍏蜂綋鍖?- Type consistency:
  - 鏂板缁熶竴浣跨敤 `PlatformManagedPluginPlanEntry`銆乣PlatformManagedPluginPlanBuilder`銆乣PlatformServiceReadyProbeSet`銆乣PlatformWarmupCoordinator`
