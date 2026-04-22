# CTK Descriptor Policy Bridge Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace `CTKManager` internal `PluginLoadPolicy` bucket classification with a descriptor/runtime bridge plus explicit `main.cpp -> CTKManager` context handoff, so CTK runtime semantics follow `platform_runtime.json + plugins/descriptors/*.json`.

**Architecture:** Add a thin `PlatformCtkPolicyBridge` in `Framework/Platform/Kernel` that resolves `PlatformRuntimeConfig + QVector<PlatformPluginDescriptor> + CTK symbolic name` into `load bucket + criticality + diagnostics`. `CTKManager` keeps install/start/deferred/on-demand execution, but stops reading `PluginLoadPolicy::getLoadPolicy()` and `PluginLoadPolicy::isCriticalPlugin()` for runtime classification. `main.cpp` remains the only place that loads runtime config and descriptors, then hands that truth into `CTKManager` before any managed or on-demand install path executes.

**Tech Stack:** CMake, Qt 6, QtTest, existing `Framework` shared library, platform governance docs and source-contract tests under `docs/superpowers` and `tests/unit`

---

## Files And Responsibilities

- Modify: `CMakeLists.txt`
  - Add `PlatformCtkPolicyBridge` to `FRAMEWORK_SOURCES` so both `medicalpro` and `Framework`-linked unit tests can use it.
- Create: `Framework/Platform/Kernel/PlatformCtkPolicyBridge.h`
  - Define the bridge load-bucket enum, resolution-status enum, bridge result struct, and the `resolve(...)` entry point.
- Create: `Framework/Platform/Kernel/PlatformCtkPolicyBridge.cpp`
  - Implement CTK symbolic-name normalization, descriptor lookup, runtime-core criticality mapping, conservative missing-descriptor fallback, and diagnostics metadata.
- Modify: `Framework/CTKManager.h`
  - Add `setDescriptorPolicyContext(...)`, remove the private `policyForPlugin(...)` declaration, remove the `PluginLoadPolicy.h` dependency from the header, and store the handed-off runtime/descriptors context.
- Modify: `Framework/CTKManager.cpp`
  - Resolve runtime bucket classification through `PlatformCtkPolicyBridge`, keep `loadPluginPolicy()` compatibility-only, drive safe mode from runtime core membership, and emit explicit diagnostics when bridge resolution falls back.
- Modify: `main.cpp`
  - Call `CTKManager::setDescriptorPolicyContext(runtimeConfig, descriptors)` immediately after runtime config and descriptors are loaded.
- Modify: `tests/unit/CMakeLists.txt`
  - Register `platform_ctk_policy_bridge_test`.
- Create: `tests/unit/PlatformCtkPolicyBridgeTest.cpp`
  - Lock bridge mapping rules, conservative fallback, and symbolic-name normalization.
- Modify: `tests/unit/PluginLegacyConsumerGovernanceContractTest.cpp`
  - Replace temporary-internal-debt expectations with bridge ownership and explicit context-handoff source contracts.
- Modify: `docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md`
  - Retire the `policyForPlugin()` / `applyPolicyForPlugin()` temporary-debt rows and record the remaining compatibility-only surface.
- Modify: `docs/superpowers/tracking/platform-plugin-governance-matrix.md`
  - Record bridge ownership, explicit context handoff, and safe-mode core-set truth.
- Modify: `docs/superpowers/tracking/platform-migration-decision-log.md`
  - Write the two new architecture decisions for descriptor-driven CTK classification and safe-mode criticality.
- Modify: `docs/current_status_and_project_overview.md`
  - Add the 2026-04-22 acceptance summary for this slice.

### Task 1: Land `PlatformCtkPolicyBridge` with mapping and normalization tests

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `tests/unit/CMakeLists.txt`
- Create: `Framework/Platform/Kernel/PlatformCtkPolicyBridge.h`
- Create: `Framework/Platform/Kernel/PlatformCtkPolicyBridge.cpp`
- Create: `tests/unit/PlatformCtkPolicyBridgeTest.cpp`

- [ ] **Step 1: Write the failing bridge unit test and register its target**

```cmake
# tests/unit/CMakeLists.txt
add_executable(platform_ctk_policy_bridge_test
    PlatformCtkPolicyBridgeTest.cpp
)

target_include_directories(platform_ctk_policy_bridge_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(platform_ctk_policy_bridge_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME platform_ctk_policy_bridge_test
    COMMAND platform_ctk_policy_bridge_test
)

set_tests_properties(platform_ctk_policy_bridge_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:platform_ctk_policy_bridge_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)
```

```cpp
// tests/unit/PlatformCtkPolicyBridgeTest.cpp
#include <QtTest/QtTest>

#include "Framework/Platform/Contracts/PlatformPluginDescriptor.h"
#include "Framework/Platform/Kernel/PlatformCtkPolicyBridge.h"
#include "Framework/Platform/Kernel/PlatformRuntimeConfig.h"

namespace
{
PlatformPluginDescriptor makeDescriptor(
    const QString& pluginId,
    const QString& displayName,
    const QString& ctkSymbolicName,
    PlatformStartupPolicy startupPolicy,
    PlatformBootstrapLevel bootstrapLevel = PlatformBootstrapLevel::Deferred)
{
    PlatformPluginDescriptor descriptor;
    descriptor.id = pluginId;
    descriptor.version = QStringLiteral("1.0.0");
    descriptor.displayName = displayName;
    descriptor.domain = QStringLiteral("test");
    descriptor.runtime.ctkSymbolicName = ctkSymbolicName;
    descriptor.runtime.startupPolicy = startupPolicy;
    descriptor.runtime.bootstrapLevel = bootstrapLevel;
    descriptor.diagnostics.requiredServices = QStringList{QStringLiteral("%1.service").arg(pluginId)};
    descriptor.diagnostics.serviceReadyTimeoutMs = 5000;
    descriptor.healthChecks = QStringList{QStringLiteral("service_registered")};
    return descriptor;
}
}

class PlatformCtkPolicyBridgeTest : public QObject
{
    Q_OBJECT

private slots:
    void resolve_returns_immediate_and_critical_for_core_eager_descriptor();
    void resolve_returns_deferred_and_non_critical_for_non_core_eager_descriptor();
    void resolve_returns_on_demand_and_non_critical_for_registration_core_descriptor();
    void resolve_returns_conservative_fallback_when_descriptor_is_missing();
    void resolve_normalizes_case_and_lib_prefix_before_matching();
};

void PlatformCtkPolicyBridgeTest::resolve_returns_immediate_and_critical_for_core_eager_descriptor()
{
    PlatformRuntimeConfig runtimeConfig;
    runtimeConfig.corePluginIds = QStringList{QStringLiteral("org.medicalpro.user_management")};

    const auto result = PlatformCtkPolicyBridge::resolve(
        runtimeConfig,
        {
            makeDescriptor(
                QStringLiteral("org.medicalpro.user_management"),
                QStringLiteral("UserManagement"),
                QStringLiteral("UserManagement"),
                PlatformStartupPolicy::Eager,
                PlatformBootstrapLevel::Core)
        },
        QStringLiteral("UserManagement"));

    QCOMPARE(result.resolvedPluginId, QStringLiteral("org.medicalpro.user_management"));
    QCOMPARE(result.ctkSymbolicName, QStringLiteral("UserManagement"));
    QCOMPARE(result.loadBucket, PlatformCtkLoadBucket::Immediate);
    QVERIFY(result.isCritical);
    QCOMPARE(result.resolutionStatus, PlatformCtkPolicyResolutionStatus::ResolvedFromDescriptor);
    QVERIFY(result.diagnosticCode.isEmpty());
}

void PlatformCtkPolicyBridgeTest::resolve_returns_deferred_and_non_critical_for_non_core_eager_descriptor()
{
    PlatformRuntimeConfig runtimeConfig;
    runtimeConfig.corePluginIds = QStringList{QStringLiteral("org.medicalpro.user_management")};

    const auto result = PlatformCtkPolicyBridge::resolve(
        runtimeConfig,
        {
            makeDescriptor(
                QStringLiteral("org.medicalpro.navigation_support"),
                QStringLiteral("NavigationSupport"),
                QStringLiteral("NavigationSupport"),
                PlatformStartupPolicy::Eager)
        },
        QStringLiteral("NavigationSupport"));

    QCOMPARE(result.resolvedPluginId, QStringLiteral("org.medicalpro.navigation_support"));
    QCOMPARE(result.loadBucket, PlatformCtkLoadBucket::Deferred);
    QVERIFY(!result.isCritical);
    QCOMPARE(result.resolutionStatus, PlatformCtkPolicyResolutionStatus::ResolvedFromDescriptor);
    QVERIFY(result.diagnosticCode.isEmpty());
}

void PlatformCtkPolicyBridgeTest::resolve_returns_on_demand_and_non_critical_for_registration_core_descriptor()
{
    PlatformRuntimeConfig runtimeConfig;
    runtimeConfig.corePluginIds = QStringList{
        QStringLiteral("org.medicalpro.user_management"),
        QStringLiteral("org.medicalpro.dicom_viewer"),
        QStringLiteral("org.medicalpro.four_view_display")
    };

    const auto result = PlatformCtkPolicyBridge::resolve(
        runtimeConfig,
        {
            makeDescriptor(
                QStringLiteral("org.medicalpro.registration_core"),
                QStringLiteral("RegistrationCore"),
                QStringLiteral("RegistrationCore"),
                PlatformStartupPolicy::OnDemand)
        },
        QStringLiteral("RegistrationCore"));

    QCOMPARE(result.resolvedPluginId, QStringLiteral("org.medicalpro.registration_core"));
    QCOMPARE(result.loadBucket, PlatformCtkLoadBucket::OnDemand);
    QVERIFY(!result.isCritical);
    QCOMPARE(result.resolutionStatus, PlatformCtkPolicyResolutionStatus::ResolvedFromDescriptor);
    QVERIFY(result.diagnosticCode.isEmpty());
}

void PlatformCtkPolicyBridgeTest::resolve_returns_conservative_fallback_when_descriptor_is_missing()
{
    PlatformRuntimeConfig runtimeConfig;
    runtimeConfig.corePluginIds = QStringList{QStringLiteral("org.medicalpro.user_management")};

    const auto result = PlatformCtkPolicyBridge::resolve(
        runtimeConfig,
        {
            makeDescriptor(
                QStringLiteral("org.medicalpro.user_management"),
                QStringLiteral("UserManagement"),
                QStringLiteral("UserManagement"),
                PlatformStartupPolicy::Eager,
                PlatformBootstrapLevel::Core)
        },
        QStringLiteral("LegacyOnlyPlugin"));

    QVERIFY(result.resolvedPluginId.isEmpty());
    QCOMPARE(result.ctkSymbolicName, QStringLiteral("LegacyOnlyPlugin"));
    QCOMPARE(result.loadBucket, PlatformCtkLoadBucket::OnDemand);
    QVERIFY(!result.isCritical);
    QCOMPARE(result.resolutionStatus, PlatformCtkPolicyResolutionStatus::DescriptorMissingFallback);
    QCOMPARE(result.diagnosticCode, QStringLiteral("descriptor_missing_for_ctk_policy_bridge"));
}

void PlatformCtkPolicyBridgeTest::resolve_normalizes_case_and_lib_prefix_before_matching()
{
    PlatformRuntimeConfig runtimeConfig;

    const auto result = PlatformCtkPolicyBridge::resolve(
        runtimeConfig,
        {
            makeDescriptor(
                QStringLiteral("org.medicalpro.registration_core"),
                QStringLiteral("RegistrationCore"),
                QStringLiteral("RegistrationCore"),
                PlatformStartupPolicy::OnDemand)
        },
        QStringLiteral("libregistrationcore"));

    QCOMPARE(result.resolvedPluginId, QStringLiteral("org.medicalpro.registration_core"));
    QCOMPARE(result.ctkSymbolicName, QStringLiteral("RegistrationCore"));
    QCOMPARE(result.loadBucket, PlatformCtkLoadBucket::OnDemand);
    QCOMPARE(result.resolutionStatus, PlatformCtkPolicyResolutionStatus::ResolvedFromDescriptor);
}

QTEST_APPLESS_MAIN(PlatformCtkPolicyBridgeTest)
#include "PlatformCtkPolicyBridgeTest.moc"
```

- [ ] **Step 2: Run the new test target and confirm the expected RED failure**

Run:

```powershell
cmake --build build_x64 --config Release --target platform_ctk_policy_bridge_test
ctest --test-dir build_x64 -C Release -R platform_ctk_policy_bridge_test --output-on-failure
```

Expected:

- `platform_ctk_policy_bridge_test` build FAILS because `Framework/Platform/Kernel/PlatformCtkPolicyBridge.h` and `PlatformCtkPolicyBridge.cpp` do not exist yet.
- No existing runtime or governance code is changed in this step.

- [ ] **Step 3: Implement the bridge and add it to the Framework target**

```cmake
# CMakeLists.txt
    Framework/Platform/Kernel/PlatformRuntimeConfig.h
    Framework/Platform/Kernel/PlatformRuntimeConfig.cpp
    Framework/Platform/Kernel/PlatformCtkPolicyBridge.h
    Framework/Platform/Kernel/PlatformCtkPolicyBridge.cpp
    Framework/Platform/Kernel/PlatformStartupCoordinator.h
    Framework/Platform/Kernel/PlatformStartupCoordinator.cpp
```

```cpp
// Framework/Platform/Kernel/PlatformCtkPolicyBridge.h
#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/PlatformPluginDescriptor.h"
#include "Framework/Platform/Kernel/PlatformRuntimeConfig.h"

#include <QString>
#include <QVector>

enum class PlatformCtkLoadBucket
{
    Immediate,
    Deferred,
    OnDemand
};

enum class PlatformCtkPolicyResolutionStatus
{
    ResolvedFromDescriptor,
    DescriptorMissingFallback
};

struct FRAMEWORK_EXPORT PlatformCtkPolicyBridgeResult
{
    QString resolvedPluginId;
    QString ctkSymbolicName;
    PlatformCtkLoadBucket loadBucket = PlatformCtkLoadBucket::OnDemand;
    bool isCritical = false;
    PlatformCtkPolicyResolutionStatus resolutionStatus = PlatformCtkPolicyResolutionStatus::DescriptorMissingFallback;
    QString diagnosticCode;
};

class FRAMEWORK_EXPORT PlatformCtkPolicyBridge
{
public:
    static PlatformCtkPolicyBridgeResult resolve(
        const PlatformRuntimeConfig& runtimeConfig,
        const QVector<PlatformPluginDescriptor>& descriptors,
        const QString& ctkSymbolicName);
};
```

```cpp
// Framework/Platform/Kernel/PlatformCtkPolicyBridge.cpp
#include "Framework/Platform/Kernel/PlatformCtkPolicyBridge.h"

namespace
{
QString trimAndLower(const QString& value)
{
    return value.trimmed().toLower();
}

QString normalizedLookupKey(const QString& value)
{
    QString normalized = trimAndLower(value);
    if (normalized.startsWith(QStringLiteral("lib"))) {
        normalized.remove(0, 3);
    }
    return normalized;
}

const PlatformPluginDescriptor* findDescriptorBySymbolicName(
    const QVector<PlatformPluginDescriptor>& descriptors,
    const QString& ctkSymbolicName)
{
    const QString trimmed = ctkSymbolicName.trimmed();

    for (const auto& descriptor : descriptors) {
        if (descriptor.runtime.ctkSymbolicName.trimmed() == trimmed) {
            return &descriptor;
        }
    }

    for (const auto& descriptor : descriptors) {
        if (descriptor.runtime.ctkSymbolicName.trimmed().compare(trimmed, Qt::CaseInsensitive) == 0) {
            return &descriptor;
        }
    }

    const QString normalized = normalizedLookupKey(trimmed);
    for (const auto& descriptor : descriptors) {
        if (normalizedLookupKey(descriptor.runtime.ctkSymbolicName) == normalized) {
            return &descriptor;
        }
    }

    return nullptr;
}

PlatformCtkLoadBucket resolveLoadBucket(
    const PlatformRuntimeConfig& runtimeConfig,
    const PlatformPluginDescriptor& descriptor)
{
    if (descriptor.runtime.startupPolicy == PlatformStartupPolicy::OnDemand) {
        return PlatformCtkLoadBucket::OnDemand;
    }

    if (runtimeConfig.corePluginIds.contains(descriptor.id)) {
        return PlatformCtkLoadBucket::Immediate;
    }

    return PlatformCtkLoadBucket::Deferred;
}
}

PlatformCtkPolicyBridgeResult PlatformCtkPolicyBridge::resolve(
    const PlatformRuntimeConfig& runtimeConfig,
    const QVector<PlatformPluginDescriptor>& descriptors,
    const QString& ctkSymbolicName)
{
    PlatformCtkPolicyBridgeResult result;
    result.ctkSymbolicName = ctkSymbolicName.trimmed();

    const auto* descriptor = findDescriptorBySymbolicName(descriptors, ctkSymbolicName);
    if (!descriptor) {
        result.diagnosticCode = QStringLiteral("descriptor_missing_for_ctk_policy_bridge");
        return result;
    }

    result.resolvedPluginId = descriptor->id;
    result.ctkSymbolicName = descriptor->runtime.ctkSymbolicName.trimmed();
    result.loadBucket = resolveLoadBucket(runtimeConfig, *descriptor);
    result.isCritical = runtimeConfig.corePluginIds.contains(descriptor->id);
    result.resolutionStatus = PlatformCtkPolicyResolutionStatus::ResolvedFromDescriptor;
    return result;
}
```

- [ ] **Step 4: Re-run the bridge test and adjacent kernel regressions until they are green**

Run:

```powershell
cmake --build build_x64 --config Release --target platform_ctk_policy_bridge_test platform_managed_plugin_plan_test platform_on_demand_activation_plan_test
ctest --test-dir build_x64 -C Release -R "platform_ctk_policy_bridge_test|platform_managed_plugin_plan_test|platform_on_demand_activation_plan_test" --output-on-failure
```

Expected:

- `platform_ctk_policy_bridge_test` PASS
- `platform_managed_plugin_plan_test` PASS
- `platform_on_demand_activation_plan_test` PASS

- [ ] **Step 5: Commit the bridge-model landing batch**

```powershell
git add CMakeLists.txt tests/unit/CMakeLists.txt tests/unit/PlatformCtkPolicyBridgeTest.cpp Framework/Platform/Kernel/PlatformCtkPolicyBridge.h Framework/Platform/Kernel/PlatformCtkPolicyBridge.cpp
git commit -m "test: add ctk descriptor policy bridge coverage"
```

### Task 2: Switch `CTKManager` runtime classification to explicit descriptor policy context

**Files:**
- Modify: `Framework/CTKManager.h`
- Modify: `Framework/CTKManager.cpp`
- Modify: `main.cpp`
- Modify: `tests/unit/PluginLegacyConsumerGovernanceContractTest.cpp`

- [ ] **Step 1: Rewrite the source contract test to encode the desired post-state before touching runtime code**

```cpp
// tests/unit/PluginLegacyConsumerGovernanceContractTest.cpp
class PluginLegacyConsumerGovernanceContractTest : public QObject
{
    Q_OBJECT

private slots:
    void main_cpp_remains_forbidden_product_mainline_consumer();
    void legacy_consumer_inventory_classifies_current_consumers();
    void ctk_manager_uses_descriptor_policy_bridge_for_runtime_classification();
    void main_cpp_hands_descriptor_policy_context_into_ctk_manager();
    void runtime_acceptance_wiring_separates_product_and_compatibility_artifacts();

private:
    QString readSource(const QString& relativePath) const;
};
```

```cpp
void PluginLegacyConsumerGovernanceContractTest::ctk_manager_uses_descriptor_policy_bridge_for_runtime_classification()
{
    const QString source = readSource(QStringLiteral("Framework/CTKManager.cpp"));
    const QString header = readSource(QStringLiteral("Framework/CTKManager.h"));

    QVERIFY2(source.contains(QStringLiteral("PlatformCtkPolicyBridge::resolve")),
        "CTKManager.cpp does not resolve runtime classification through PlatformCtkPolicyBridge");
    QVERIFY2(!source.contains(QStringLiteral("PluginLoadPolicy::instance()->isCriticalPlugin(")),
        "CTKManager.cpp still queries PluginLoadPolicy::isCriticalPlugin() for runtime classification");
    QVERIFY2(!source.contains(QStringLiteral("getLoadPolicy(")),
        "CTKManager.cpp still queries PluginLoadPolicy::getLoadPolicy() for runtime classification");
    QVERIFY2(!header.contains(QStringLiteral("policyForPlugin(")),
        "CTKManager.h still declares the legacy policyForPlugin() helper");
    QVERIFY2(header.contains(QStringLiteral("setDescriptorPolicyContext(")),
        "CTKManager.h does not expose the explicit descriptor policy context handoff");
}

void PluginLegacyConsumerGovernanceContractTest::main_cpp_hands_descriptor_policy_context_into_ctk_manager()
{
    const QString source = readSource(QStringLiteral("main.cpp"));

    QVERIFY2(source.contains(QStringLiteral("setDescriptorPolicyContext(runtimeConfig, descriptors)")),
        "main.cpp does not hand the loaded runtime config and descriptors into CTKManager");
}
```

- [ ] **Step 2: Run the source-contract suite and confirm the expected RED failure**

Run:

```powershell
cmake --build build_x64 --config Release --target plugin_legacy_consumer_governance_contract_test
ctest --test-dir build_x64 -C Release -R plugin_legacy_consumer_governance_contract_test --output-on-failure
```

Expected:

- `plugin_legacy_consumer_governance_contract_test` FAIL
- Failure text includes at least one of:
  - `CTKManager.cpp still queries PluginLoadPolicy::isCriticalPlugin() for runtime classification`
  - `CTKManager.h still declares the legacy policyForPlugin() helper`
  - `main.cpp does not hand the loaded runtime config and descriptors into CTKManager`

- [ ] **Step 3: Implement explicit context handoff and bridge-driven classification in `CTKManager`**

```cpp
// Framework/CTKManager.h
#include "Framework/Platform/Contracts/PlatformPluginDescriptor.h"
#include "Framework/Platform/Kernel/PlatformRuntimeConfig.h"
#include "FrameworkExport.h"
#include "ResourceManagement/SingletonManager.h"

class FRAMEWORK_EXPORT CTKManager : public QObject, public SingletonManager<CTKManager>
{
    Q_OBJECT
    friend class SingletonManager<CTKManager>;

public:
    static CTKManager* instance() { return &SingletonManager<CTKManager>::instance(); }

    void setDescriptorPolicyContext(
        const PlatformRuntimeConfig& runtimeConfig,
        const QVector<PlatformPluginDescriptor>& descriptors);

    bool initializeFramework(QApplication* app);
    bool startFramework();
    void stopFramework();
    void stopPlugins();
    int loadPluginsFromDirectory(const QString& pluginDir);
    bool loadPlugin(const QString& pluginPath, bool autoStart = true);
    int installPluginsFromDirectory(const QString& pluginDir);
    bool installPlugin(const QString& pluginPath, bool autoStart = false, QString* outPluginName = nullptr);
    bool startPlugin(const QString& pluginName);
    bool startPlugins(const QStringList& pluginNames, bool stopOnFailure = false);
    bool startDeferredPlugins(bool stopOnFailure = false);
    void loadPluginPolicy(const QString& configPath);
    bool isCTKAvailable() const;
    void setSafeMode(bool enabled);
    bool isSafeMode() const;
    QStringList getLoadedPlugins() const;
    QStringList getInstalledPlugins() const;
    QStringList getDeferredPlugins() const;
    QStringList getOnDemandPlugins() const;
    QStringList getStartedPlugins() const;
    bool isPluginStarted(const QString& pluginName) const;
    QString getPluginState(const QString& pluginName) const;
    void setPluginLoadOrder(const QStringList& order);
    QStringList getRecommendedLoadOrder() const;
    bool verifyRequiredServices(const QStringList& serviceNames);
    QString getFrameworkDiagnostics() const;
    QStringList getMissingServices(const QStringList& required) const;
    QMap<QString, QString> getPluginStatus() const;
    QString verifyPluginServices();

private:
    CTKManager();
    ~CTKManager() override;

    PlatformRuntimeConfig m_descriptorPolicyRuntimeConfig;
    QVector<PlatformPluginDescriptor> m_descriptorPolicyDescriptors;

#ifdef CTK_PLUGIN_FRAMEWORK
    QSharedPointer<ctkPluginFramework> m_framework;
    ctkPluginContext* m_pluginContext;
    ctkPluginFrameworkFactory* m_frameworkFactory;
    ctkEventAdmin* m_eventAdmin;
    QMap<QString, QSharedPointer<ctkPlugin>> m_installedPluginHandles;
    QSet<QString> m_installedPluginNames;
    QSet<QString> m_startedPluginNames;
    QSet<QString> m_deferredPlugins;
    QSet<QString> m_onDemandPlugins;
    QHash<QString, QString> m_pluginSourceMap;
    bool startPluginInternal(const QString& pluginName, QSet<QString>& visiting);
    bool activatePlugin(const QString& pluginName);
    bool stopPluginInternal(const QString& pluginName);
    QStringList manifestDependenciesForPlugin(const QString& pluginName) const;
    QString locateManifestForPlugin(const QString& pluginName) const;
    QStringList parseManifestDependencies(const QString& manifestPath) const;
    bool applyPolicyForPlugin(const QString& pluginName, bool allowStart, bool forceStart = false);
#endif

    bool m_initialized;
    bool m_started;
    bool m_safeMode;
    QStringList m_loadedPlugins;
    QStringList m_pluginLoadOrder;

    void logMessage(const QString& message);
    QString getPluginStateString(int state) const;
};
```

```cpp
// Framework/CTKManager.cpp
#include "CTKManager.h"
#include "Logger.h"
#include "PluginLoadPolicy.h"
#include "StartupOrchestrator.h"
#include "ErrorHandler.h"
#include "Framework/Platform/Kernel/PlatformCtkPolicyBridge.h"
```

```cpp
namespace
{
QString resolutionStatusToString(PlatformCtkPolicyResolutionStatus status)
{
    switch (status) {
    case PlatformCtkPolicyResolutionStatus::ResolvedFromDescriptor:
        return QStringLiteral("resolved_from_descriptor");
    case PlatformCtkPolicyResolutionStatus::DescriptorMissingFallback:
    default:
        return QStringLiteral("descriptor_missing_fallback");
    }
}

QString loadBucketToString(PlatformCtkLoadBucket bucket)
{
    switch (bucket) {
    case PlatformCtkLoadBucket::Immediate:
        return QStringLiteral("immediate");
    case PlatformCtkLoadBucket::Deferred:
        return QStringLiteral("deferred");
    case PlatformCtkLoadBucket::OnDemand:
    default:
        return QStringLiteral("on_demand");
    }
}
}

void CTKManager::setDescriptorPolicyContext(
    const PlatformRuntimeConfig& runtimeConfig,
    const QVector<PlatformPluginDescriptor>& descriptors)
{
    m_descriptorPolicyRuntimeConfig = runtimeConfig;
    m_descriptorPolicyDescriptors = descriptors;
    LOG_INFO_F(
        "CTKManager",
        "Descriptor policy context updated with %1 descriptors",
        descriptors.size());
}

void CTKManager::loadPluginPolicy(const QString& configPath)
{
    LOG_INFO(
        "CTKManager",
        QString("Loading compatibility-only plugin policy metadata from: %1").arg(configPath));
    PluginLoadPolicy::instance()->loadConfig(configPath);
}

bool CTKManager::applyPolicyForPlugin(const QString& pluginName, bool allowStart, bool forceStart)
{
    m_deferredPlugins.remove(pluginName);
    m_onDemandPlugins.remove(pluginName);

    const auto resolved = PlatformCtkPolicyBridge::resolve(
        m_descriptorPolicyRuntimeConfig,
        m_descriptorPolicyDescriptors,
        pluginName);

    if (resolved.resolutionStatus != PlatformCtkPolicyResolutionStatus::ResolvedFromDescriptor) {
        QVariantMap context;
        context.insert(QStringLiteral("plugin"), pluginName);
        context.insert(QStringLiteral("diagnostic_code"), resolved.diagnosticCode);
        context.insert(QStringLiteral("resolution_status"), resolutionStatusToString(resolved.resolutionStatus));
        context.insert(QStringLiteral("load_bucket"), loadBucketToString(resolved.loadBucket));
        StartupOrchestrator::instance()->logDiagnostic(
            ErrorHandler::ErrorLevel::Warning,
            QStringLiteral("Descriptor policy bridge fallback for plugin: %1").arg(pluginName),
            context);
    }

    if (m_safeMode && !resolved.isCritical) {
        LOG_INFO_F("CTKManager", "Safe mode active, skipping non-core plugin: %1", pluginName);
        QVariantMap context;
        context.insert(QStringLiteral("plugin"), pluginName);
        context.insert(QStringLiteral("load_bucket"), loadBucketToString(resolved.loadBucket));
        context.insert(QStringLiteral("resolution_status"), resolutionStatusToString(resolved.resolutionStatus));
        StartupOrchestrator::instance()->logDiagnostic(
            ErrorHandler::ErrorLevel::Info,
            QStringLiteral("Safe mode skipped non-core plugin: %1").arg(pluginName),
            context);
        return true;
    }

    switch (resolved.loadBucket) {
    case PlatformCtkLoadBucket::Immediate: {
        if (!m_startedPluginNames.contains(pluginName)) {
            if (forceStart) {
                allowStart = true;
            }
            if (allowStart) {
                QSet<QString> visiting;
                if (!startPluginInternal(pluginName, visiting)) {
                    return false;
                }
            }
        }
        return true;
    }
    case PlatformCtkLoadBucket::Deferred:
        if (!m_startedPluginNames.contains(pluginName)) {
            m_deferredPlugins.insert(pluginName);
        }
        return true;
    case PlatformCtkLoadBucket::OnDemand:
    default:
        if (!m_startedPluginNames.contains(pluginName)) {
            m_onDemandPlugins.insert(pluginName);
        }
        return true;
    }
}
```

```cpp
// main.cpp
        const auto descriptors = PlatformDescriptorLoader::loadFromDirectory(descriptorDirectoryPath, &descriptorErrors);
        if (!descriptorErrors.isEmpty()) {
            throw std::runtime_error(
                QStringLiteral("Failed to load platform descriptors: %1")
                    .arg(descriptorErrors.join(QStringLiteral("; ")))
                    .toStdString());
        }

        ctkManager->setDescriptorPolicyContext(runtimeConfig, descriptors);

        QHash<QString, QString> platformPluginIdToCtkSymbolicName;
        platformPluginIdToCtkSymbolicName.reserve(descriptors.size());
```

- [ ] **Step 4: Run build, source-contract, and kernel regression verification**

Run:

```powershell
cmake --build build_x64 --config Release --target medicalpro platform_ctk_policy_bridge_test plugin_legacy_consumer_governance_contract_test plugin_truth_source_governance_contract_test
ctest --test-dir build_x64 -C Release -R "platform_ctk_policy_bridge_test|plugin_legacy_consumer_governance_contract_test|plugin_truth_source_governance_contract_test|platform_managed_plugin_plan_test|platform_on_demand_activation_plan_test" --output-on-failure
rg -n "PluginLoadPolicy::instance\\(\\)->isCriticalPlugin\\(|getLoadPolicy\\(" Framework/CTKManager.cpp
rg -n "setDescriptorPolicyContext\\(runtimeConfig, descriptors\\)|PlatformCtkPolicyBridge::resolve" main.cpp Framework/CTKManager.cpp
```

Expected:

- `medicalpro` build target PASS
- `platform_ctk_policy_bridge_test` PASS
- `plugin_legacy_consumer_governance_contract_test` PASS
- `plugin_truth_source_governance_contract_test` PASS
- `platform_managed_plugin_plan_test` PASS
- `platform_on_demand_activation_plan_test` PASS
- The first `rg` command prints no matches
- The second `rg` command prints both the `main.cpp` context handoff and the `CTKManager.cpp` bridge-resolution call

- [ ] **Step 5: Commit the runtime truth-source switch batch**

```powershell
git add Framework/CTKManager.h Framework/CTKManager.cpp main.cpp tests/unit/PluginLegacyConsumerGovernanceContractTest.cpp
git commit -m "feat: bridge ctk runtime policy to descriptors"
```

### Task 3: Rewrite governance contracts and docs around the retired internal policy debt

**Files:**
- Modify: `tests/unit/PluginLegacyConsumerGovernanceContractTest.cpp`
- Modify: `docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md`
- Modify: `docs/superpowers/tracking/platform-plugin-governance-matrix.md`
- Modify: `docs/superpowers/tracking/platform-migration-decision-log.md`
- Modify: `docs/current_status_and_project_overview.md`

- [ ] **Step 1: Extend the governance contract test to require the new documentation state before editing docs**

```cpp
// tests/unit/PluginLegacyConsumerGovernanceContractTest.cpp
private slots:
    void legacy_consumer_inventory_retires_internal_policy_debt();
    void governance_docs_record_descriptor_policy_bridge_ownership();
```

```cpp
void PluginLegacyConsumerGovernanceContractTest::legacy_consumer_inventory_retires_internal_policy_debt()
{
    const QString inventory = readSource(QStringLiteral("docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md"));

    QVERIFY2(inventory.contains(QStringLiteral("`CTKManager::loadPluginPolicy()` | `allowed_compatibility_surface`")),
        "legacy consumer inventory no longer records CTKManager::loadPluginPolicy() as compatibility surface");
    QVERIFY2(inventory.contains(QStringLiteral("`CTKManager::installPluginsFromDirectory()` | `allowed_compatibility_surface`")),
        "legacy consumer inventory no longer records CTKManager::installPluginsFromDirectory() as compatibility surface");
    QVERIFY2(!inventory.contains(QStringLiteral("`CTKManager::policyForPlugin()` | `temporary_internal_compatibility_debt`")),
        "legacy consumer inventory still lists CTKManager::policyForPlugin() as temporary internal debt");
    QVERIFY2(!inventory.contains(QStringLiteral("`CTKManager::applyPolicyForPlugin()` | `temporary_internal_compatibility_debt`")),
        "legacy consumer inventory still lists CTKManager::applyPolicyForPlugin() as temporary internal debt");
    QVERIFY2(inventory.contains(QStringLiteral("`PlatformCtkPolicyBridge`")),
        "legacy consumer inventory does not record PlatformCtkPolicyBridge as the replacement ownership note");
}

void PluginLegacyConsumerGovernanceContractTest::governance_docs_record_descriptor_policy_bridge_ownership()
{
    const QString governanceMatrix = readSource(QStringLiteral("docs/superpowers/tracking/platform-plugin-governance-matrix.md"));
    const QString decisionLog = readSource(QStringLiteral("docs/superpowers/tracking/platform-migration-decision-log.md"));
    const QString currentStatus = readSource(QStringLiteral("docs/current_status_and_project_overview.md"));

    QVERIFY2(governanceMatrix.contains(QStringLiteral("PlatformCtkPolicyBridge")),
        "governance matrix does not mention PlatformCtkPolicyBridge");
    QVERIFY2(governanceMatrix.contains(QStringLiteral("setDescriptorPolicyContext()")),
        "governance matrix does not mention the explicit CTKManager context handoff");
    QVERIFY2(decisionLog.contains(QStringLiteral("safe mode criticality now follows `platform_runtime.json.core_plugin_ids`")),
        "decision log does not record the safe-mode criticality decision");
    QVERIFY2(currentStatus.contains(QStringLiteral("CTKManager runtime bucket classification now resolves through `PlatformCtkPolicyBridge`")),
        "current status does not record the CTK descriptor policy bridge acceptance");
}
```

- [ ] **Step 2: Run the governance contract suite and confirm the expected RED failure**

Run:

```powershell
cmake --build build_x64 --config Release --target plugin_legacy_consumer_governance_contract_test
ctest --test-dir build_x64 -C Release -R plugin_legacy_consumer_governance_contract_test --output-on-failure
```

Expected:

- `plugin_legacy_consumer_governance_contract_test` FAIL
- Failure text includes at least one of:
  - `legacy consumer inventory still lists CTKManager::policyForPlugin() as temporary internal debt`
  - `governance matrix does not mention PlatformCtkPolicyBridge`
  - `current status does not record the CTK descriptor policy bridge acceptance`

- [ ] **Step 3: Update inventory, governance matrix, decision log, and current status**

```md
<!-- docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md -->
# Platform Plugin Legacy Consumer Inventory

Updated: 2026-04-22

## Current Consumers

| Consumer | Bucket | Status | Notes | Next Step |
| --- | --- | --- | --- | --- |
| `main.cpp` | `forbidden_product_mainline` | enforced | Product startup remains descriptor-driven and must not call legacy load-policy helpers. | Protected by source contract tests. |
| `config/plugin_load_policy.json` | `allowed_compatibility_surface` | retained | Compatibility-only runtime metadata. | Covered by dedicated compatibility runtime contract. |
| `config/plugin_load_policy_compatibility.md` | `allowed_compatibility_surface` | retained | Sidecar note that explains the compatibility-only boundary. | Covered by dedicated compatibility runtime contract. |
| `CTKManager::loadPluginPolicy()` | `allowed_compatibility_surface` | retained | Compatibility-only metadata loader. | Must not influence runtime bucket classification. |
| `CTKManager::installPluginsFromDirectory()` | `allowed_compatibility_surface` | retained | Compatibility directory-scan helper. | Product mainline must not call it. |
| `runtime_artifact_layout_test` | `forbidden_product_mainline` | enforced | Default runtime artifact acceptance must validate product artifacts only. | Must not require `plugin_load_policy.json`. |
| `plugin_legacy_compatibility_runtime_contract_test` | `allowed_compatibility_surface` | retained | Dedicated runtime acceptance for compatibility-only artifacts. | Keep it separate from product runtime layout acceptance. |

## Retired Internal Debt

- `CTKManager::policyForPlugin()` is removed from runtime classification; `PlatformCtkPolicyBridge` now resolves bucket and criticality facts from descriptor/runtime truth.
- `CTKManager::applyPolicyForPlugin()` remains as the execution helper, but it no longer reads `PluginLoadPolicy` and no longer acts as a legacy truth-source consumer.

## Forbidden New Usage

- No new product-mainline code may call `loadPluginPolicy()` or `installPluginsFromDirectory()`.
- No new product-mainline code may read `plugin_load_policy.json` to decide startup content.
- Any newly discovered legacy consumer must be added to this inventory before it can be considered acceptable.
```

```md
<!-- docs/superpowers/tracking/platform-plugin-governance-matrix.md -->
## Current Implementation Notes

- `config/platform_runtime.json` stores platform descriptor ids, not CTK symbolic names.
- Product startup truth is explicitly `platform_runtime.json + plugins/descriptors/*.json + PlatformDescriptorLoader`.
- `main.cpp` now hands the already-loaded `runtimeConfig` and `descriptors` into `CTKManager::setDescriptorPolicyContext()`.
- `PlatformCtkPolicyBridge` is now the only runtime classification source for CTK deferred/on-demand/immediate bucket resolution.
- Safe mode criticality now follows `platform_runtime.json.core_plugin_ids` rather than `PluginLoadPolicy::isCriticalPlugin()`.
- `plugin_load_policy.json` and `PluginLoadPolicy` remain compatibility-only metadata for legacy surfaces.
- `CTKManager::loadPluginPolicy()` and `CTKManager::installPluginsFromDirectory()` remain available but are not part of `main.cpp` product assembly.
- `runtime_artifact_layout_test` now covers product-mainline runtime artifacts only.
- `plugin_legacy_compatibility_runtime_contract_test` owns `plugin_load_policy.json` and `plugin_load_policy_compatibility.md` shipping verification.
- The authoritative human-readable inventory for remaining legacy consumers is `docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md`.
```

```md
<!-- docs/superpowers/tracking/platform-migration-decision-log.md -->
## 2026-04-22

- Decision: drive `CTKManager` runtime bucket classification through `PlatformCtkPolicyBridge` instead of `PluginLoadPolicy`.
- Rationale: the product mainline already loads runtime config and descriptors as the single truth source, so CTK internal bucket classification must converge on that same authority instead of retaining `plugin_load_policy.json` as a hidden semantic input.
- Impact: `CTKManager` now classifies installed plugins from explicit `runtimeConfig + descriptors` handoff, `PluginLoadPolicy::getLoadPolicy()` no longer influences runtime bucket semantics, and missing descriptor context now falls back to `on_demand + non-critical` with explicit diagnostics.

- Decision: safe mode criticality now follows `platform_runtime.json.core_plugin_ids`.
- Rationale: safe mode should skip non-core plugins based on the current runtime truth, not on the old `PluginLoadPolicy::isCriticalPlugin()` flag set.
- Impact: core plugins stay startable in safe mode according to runtime config, while non-core deferred and on-demand plugins can be skipped consistently with the governed descriptor/runtime model.
```

```md
<!-- docs/current_status_and_project_overview.md -->
### 2026-04-22 CTK Descriptor Policy Bridge Acceptance

- `CTKManager` runtime bucket classification now resolves through `PlatformCtkPolicyBridge`.
- `main.cpp` explicitly hands `runtimeConfig + descriptors` into `CTKManager::setDescriptorPolicyContext()`.
- Safe mode criticality now follows `platform_runtime.json.core_plugin_ids` rather than `PluginLoadPolicy::isCriticalPlugin()`.
- `PluginLoadPolicy` remains shipped only as a compatibility-only metadata surface and no longer drives CTK runtime classification semantics.
- Executed command (build):
  - `cmake --build build_x64 --config Release --target medicalpro platform_ctk_policy_bridge_test plugin_legacy_consumer_governance_contract_test plugin_truth_source_governance_contract_test`
- Executed command (ctest):
  - `ctest --test-dir build_x64 -C Release -R "platform_ctk_policy_bridge_test|plugin_legacy_consumer_governance_contract_test|plugin_truth_source_governance_contract_test|runtime_artifact_layout_test|plugin_legacy_compatibility_runtime_contract_test|plugin_truth_source_runtime_contract_test|platform_descriptor_runtime_layout_test" --output-on-failure`
- Executed command (source scan):
  - `rg -n "PluginLoadPolicy::instance\\(\\)->isCriticalPlugin\\(|getLoadPolicy\\(" Framework/CTKManager.cpp`
- Expected outcomes alignment and actual results:
  - Descriptor policy bridge suite: PASS.
  - Product/compatibility/runtime governance suite: PASS.
  - Legacy runtime-classification scan: no output.
```

- [ ] **Step 4: Run the full governance acceptance suite and source scans**

Run:

```powershell
cmake --build build_x64 --config Release --target medicalpro platform_ctk_policy_bridge_test plugin_legacy_consumer_governance_contract_test plugin_truth_source_governance_contract_test
ctest --test-dir build_x64 -C Release -R "platform_ctk_policy_bridge_test|plugin_legacy_consumer_governance_contract_test|plugin_truth_source_governance_contract_test|runtime_artifact_layout_test|plugin_legacy_compatibility_runtime_contract_test|plugin_truth_source_runtime_contract_test|platform_descriptor_runtime_layout_test" --output-on-failure
rg -n "temporary_internal_compatibility_debt|PlatformCtkPolicyBridge|setDescriptorPolicyContext\\(|core_plugin_ids" docs/current_status_and_project_overview.md docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md docs/superpowers/tracking/platform-plugin-governance-matrix.md docs/superpowers/tracking/platform-migration-decision-log.md
```

Expected:

- `medicalpro` build target PASS
- `platform_ctk_policy_bridge_test` PASS
- `plugin_legacy_consumer_governance_contract_test` PASS
- `plugin_truth_source_governance_contract_test` PASS
- `runtime_artifact_layout_test` PASS
- `plugin_legacy_compatibility_runtime_contract_test` PASS
- `plugin_truth_source_runtime_contract_test` PASS
- `platform_descriptor_runtime_layout_test` PASS
- The `rg` command finds `PlatformCtkPolicyBridge`, `setDescriptorPolicyContext(`, and `core_plugin_ids`
- The `rg` command no longer finds `temporary_internal_compatibility_debt` in the updated governance docs for this slice

- [ ] **Step 5: Commit the governance write-back batch**

```powershell
git add tests/unit/PluginLegacyConsumerGovernanceContractTest.cpp docs/current_status_and_project_overview.md docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md docs/superpowers/tracking/platform-plugin-governance-matrix.md docs/superpowers/tracking/platform-migration-decision-log.md
git commit -m "docs: record ctk descriptor policy bridge acceptance"
```

## Self-Review

- Spec coverage:
  - Bridge model and mapping rules: Task 1 covers the new component, normalization order, conservative fallback, and direct unit tests.
  - CTKManager truth-source switch and explicit context handoff: Task 2 covers the setter, `main.cpp` handoff, bridge-based bucket classification, and safe-mode criticality convergence.
  - Governance write-back and source contracts: Task 3 covers inventory, governance matrix, decision log, current status, and the contract suite that protects the new architecture boundary.
- Placeholder scan:
  - No `TBD`, `TODO`, `implement later`, `fill in later`, or `similar to Task N` placeholders remain.
  - Every code-changing step includes the exact code block to add or change.
  - Every verification step includes concrete commands plus expected outcomes.
- Type consistency:
  - `PlatformCtkLoadBucket`, `PlatformCtkPolicyResolutionStatus`, and `PlatformCtkPolicyBridgeResult` are introduced in Task 1 and reused consistently in Task 2.
  - The explicit handoff API is always named `setDescriptorPolicyContext(...)`.
  - Governance language consistently distinguishes `forbidden_product_mainline`, `allowed_compatibility_surface`, and retired internal debt instead of reusing the old `temporary_internal_compatibility_debt` state for this slice.
