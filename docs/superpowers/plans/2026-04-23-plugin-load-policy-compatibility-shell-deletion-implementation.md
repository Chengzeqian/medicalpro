# Plugin Load Policy Compatibility Shell Deletion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Delete the final `plugin_load_policy` compatibility shell and adjacent dead legacy helpers without changing descriptor/runtime startup truth or CTK runtime classification ownership.

**Architecture:** Flip governance contracts from "compatibility shell exists" to "compatibility shell is deleted", then remove source/config/deployment/runtime-contract residue in that order so failures stay readable. Keep `CTKManager` as the CTK runtime executor, keep `PlatformCtkPolicyBridge` and descriptor/runtime startup assembly intact, and avoid touching the unrelated dirty worktree file `medicalpro_zh_CN.ts`.

**Tech Stack:** CMake, Qt 6, QtTest, existing `Framework` shared library, runtime artifact checks in `tests/runtime`, governance docs in `docs/superpowers`, git commits per task

---

## Files And Responsibilities

- Modify: `tests/unit/PluginTruthSourceGovernanceContractTest.cpp`
  - Flip truth-source contract from compatibility retention to shell deletion.
- Modify: `Framework/CTKManager.h`
  - Remove deleted compatibility helper declarations and `m_pluginLoadOrder`.
- Modify: `Framework/CTKManager.cpp`
  - Remove `PluginLoadPolicy` include, `loadPluginPolicy()`, `installPluginsFromDirectory()`, `setPluginLoadOrder()`, and `getRecommendedLoadOrder()`.
- Modify: `CMakeLists.txt`
  - Stop compiling `PluginLoadPolicy`, stop deploying compatibility config artifacts, keep only product config deployment.
- Delete: `Framework/PluginLoadPolicy.h`
  - Remove dead compatibility carrier type.
- Delete: `Framework/PluginLoadPolicy.cpp`
  - Remove dead compatibility carrier implementation.
- Delete: `config/plugin_load_policy.json`
  - Remove dead runtime compatibility projection.
- Delete: `config/plugin_load_policy_compatibility.md`
  - Remove dead sidecar note for the deleted projection.
- Modify: `tests/unit/PluginLegacyConsumerGovernanceContractTest.cpp`
  - Flip legacy consumer governance assertions from retention to deletion and runtime acceptance absence.
- Modify: `tests/CMakeLists.txt`
  - Remove `plugin_legacy_compatibility_runtime_contract_test`.
- Modify: `tests/unit/CMakeLists.txt`
  - Remove `plugin_load_policy_compatibility_residue_contract_test`.
- Modify: `tests/runtime/verify_runtime_artifacts.cmake`
  - Delete the compatibility runtime contract branch.
- Delete: `tests/unit/PluginLoadPolicyCompatibilityResidueContractTest.cpp`
  - Remove unit contract that exists only to keep the deleted shell alive.
- Modify: `docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md`
  - Record deleted shell and forbidden reintroduction.
- Modify: `docs/superpowers/tracking/platform-plugin-governance-matrix.md`
  - Record that compatibility shell deletion is complete.
- Modify: `docs/superpowers/tracking/platform-migration-decision-log.md`
  - Record the deletion decision and its impact.
- Modify: `docs/current_status_and_project_overview.md`
  - Record the 2026-04-23 shell deletion acceptance summary and verification commands.

### Task 1: Delete the source-level compatibility shell and adjacent helper APIs

**Files:**
- Modify: `tests/unit/PluginTruthSourceGovernanceContractTest.cpp`
- Modify: `Framework/CTKManager.h`
- Modify: `Framework/CTKManager.cpp`
- Modify: `CMakeLists.txt`
- Delete: `Framework/PluginLoadPolicy.h`
- Delete: `Framework/PluginLoadPolicy.cpp`
- Delete: `config/plugin_load_policy.json`
- Delete: `config/plugin_load_policy_compatibility.md`

- [ ] **Step 1: Rewrite the truth-source contract to expect shell deletion**

```cpp
// tests/unit/PluginTruthSourceGovernanceContractTest.cpp
#include <QtTest/QtTest>

#include <QFile>
#include <QFileInfo>
#include <QString>

class PluginTruthSourceGovernanceContractTest : public QObject
{
    Q_OBJECT

private slots:
    void main_cpp_uses_runtime_config_and_descriptor_loader_for_product_mainline();
    void main_cpp_does_not_call_legacy_policy_helpers_for_product_mainline();
    void plugin_load_policy_shell_is_deleted();

private:
    QString readSource(const QString& relativePath) const;
};

QString PluginTruthSourceGovernanceContractTest::readSource(const QString& relativePath) const
{
    QFile sourceFile(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
    if (!sourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTest::qFail(qPrintable(QStringLiteral("failed to read source file: %1").arg(relativePath)), __FILE__, __LINE__);
        return {};
    }
    return QString::fromUtf8(sourceFile.readAll());
}

void PluginTruthSourceGovernanceContractTest::main_cpp_uses_runtime_config_and_descriptor_loader_for_product_mainline()
{
    const QString source = readSource(QStringLiteral("main.cpp"));

    QVERIFY2(source.contains(QStringLiteral("config/platform_runtime.json")),
        "main.cpp does not read product runtime config from platform_runtime.json");
    QVERIFY2(source.contains(QStringLiteral("PlatformDescriptorLoader::loadFromDirectory")),
        "main.cpp does not load governed plugin descriptors through PlatformDescriptorLoader");
    QVERIFY2(source.contains(QStringLiteral("PlatformManagedPluginPlanBuilder::build")),
        "main.cpp does not build the managed startup plan from descriptor facts");
}

void PluginTruthSourceGovernanceContractTest::main_cpp_does_not_call_legacy_policy_helpers_for_product_mainline()
{
    const QString source = readSource(QStringLiteral("main.cpp"));

    QVERIFY2(!source.contains(QStringLiteral("loadPluginPolicy(")),
        "main.cpp still calls loadPluginPolicy() in the product startup mainline");
    QVERIFY2(!source.contains(QStringLiteral("installPluginsFromDirectory(")),
        "main.cpp still calls installPluginsFromDirectory() in the product startup mainline");
}

void PluginTruthSourceGovernanceContractTest::plugin_load_policy_shell_is_deleted()
{
    const QString ctkManagerHeader = readSource(QStringLiteral("Framework/CTKManager.h"));
    const QString ctkManagerSource = readSource(QStringLiteral("Framework/CTKManager.cpp"));
    const QString rootCMake = readSource(QStringLiteral("CMakeLists.txt"));

    QVERIFY2(!QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Framework/PluginLoadPolicy.h")).exists(),
        "Framework/PluginLoadPolicy.h still exists");
    QVERIFY2(!QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Framework/PluginLoadPolicy.cpp")).exists(),
        "Framework/PluginLoadPolicy.cpp still exists");
    QVERIFY2(!QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/config/plugin_load_policy.json")).exists(),
        "config/plugin_load_policy.json still exists");
    QVERIFY2(!QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/config/plugin_load_policy_compatibility.md")).exists(),
        "config/plugin_load_policy_compatibility.md still exists");
    QVERIFY2(!ctkManagerHeader.contains(QStringLiteral("loadPluginPolicy(")),
        "CTKManager.h still exposes loadPluginPolicy()");
    QVERIFY2(!ctkManagerHeader.contains(QStringLiteral("installPluginsFromDirectory(")),
        "CTKManager.h still exposes installPluginsFromDirectory()");
    QVERIFY2(!ctkManagerHeader.contains(QStringLiteral("setPluginLoadOrder(")),
        "CTKManager.h still exposes setPluginLoadOrder()");
    QVERIFY2(!ctkManagerHeader.contains(QStringLiteral("getRecommendedLoadOrder(")),
        "CTKManager.h still exposes getRecommendedLoadOrder()");
    QVERIFY2(!ctkManagerHeader.contains(QStringLiteral("m_pluginLoadOrder")),
        "CTKManager.h still stores m_pluginLoadOrder");
    QVERIFY2(!ctkManagerSource.contains(QStringLiteral("#include \"PluginLoadPolicy.h\"")),
        "CTKManager.cpp still includes PluginLoadPolicy.h");
    QVERIFY2(!rootCMake.contains(QStringLiteral("Framework/PluginLoadPolicy.h")),
        "CMakeLists.txt still compiles Framework/PluginLoadPolicy.h");
    QVERIFY2(!rootCMake.contains(QStringLiteral("Framework/PluginLoadPolicy.cpp")),
        "CMakeLists.txt still compiles Framework/PluginLoadPolicy.cpp");
    QVERIFY2(!rootCMake.contains(QStringLiteral("plugin_load_policy.json")),
        "CMakeLists.txt still deploys plugin_load_policy.json");
    QVERIFY2(!rootCMake.contains(QStringLiteral("plugin_load_policy_compatibility.md")),
        "CMakeLists.txt still deploys plugin_load_policy_compatibility.md");
}

QTEST_APPLESS_MAIN(PluginTruthSourceGovernanceContractTest)
#include "PluginTruthSourceGovernanceContractTest.moc"
```

- [ ] **Step 2: Run the truth-source contract and confirm the expected RED failure**

Run:

```powershell
cmake --build build_x64 --config Release --target plugin_truth_source_governance_contract_test
ctest --test-dir build_x64 -C Release -R "plugin_truth_source_governance_contract_test" --output-on-failure
```

Expected:

- Build: PASS
- Test: FAIL with one or more of:
  - `Framework/PluginLoadPolicy.h still exists`
  - `CTKManager.h still exposes loadPluginPolicy()`
  - `CMakeLists.txt still deploys plugin_load_policy.json`

- [ ] **Step 3: Delete the source shell, helper APIs, and compatibility config files**

```cpp
// Framework/CTKManager.h
bool initializeFramework(QApplication* app);
bool startFramework();
void stopFramework();
void stopPlugins();
int loadPluginsFromDirectory(const QString& pluginDir);
bool loadPlugin(const QString& pluginPath, bool autoStart = true);
bool installPlugin(const QString& pluginPath, bool autoStart = false, QString* outPluginName = nullptr);
bool startPlugin(const QString& pluginName);
bool startPlugins(const QStringList& pluginNames, bool stopOnFailure = false);
bool startDeferredPlugins(bool stopOnFailure = false);
void setDescriptorPolicyContext(
    const PlatformRuntimeConfig& runtimeConfig,
    const QVector<PlatformPluginDescriptor>& descriptors);
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
bool verifyRequiredServices(const QStringList& serviceNames);
QString getFrameworkDiagnostics() const;
QStringList getMissingServices(const QStringList& required) const;
QMap<QString, QString> getPluginStatus() const;
QString verifyPluginServices();
```

```cpp
// Framework/CTKManager.h private data
bool m_initialized;
bool m_started;
bool m_safeMode;
QStringList m_loadedPlugins;
bool m_descriptorPolicyContextInitialized = false;
PlatformRuntimeConfig m_descriptorPolicyRuntimeConfig;
QVector<PlatformPluginDescriptor> m_descriptorPolicyDescriptors;
```

```cpp
// Framework/CTKManager.cpp
#include "CTKManager.h"
#include "Logger.h"
#include "StartupOrchestrator.h"
#include "ErrorHandler.h"
#include "Framework/Platform/Kernel/PlatformCtkPolicyBridge.h"
```

```cpp
// Framework/CTKManager.cpp
bool CTKManager::isCTKAvailable() const
{
#ifdef CTK_PLUGIN_FRAMEWORK
    return m_initialized && m_started;
#else
    return false;
#endif
}
```

```cmake
# CMakeLists.txt
set(FRAMEWORK_SOURCES
    Framework/CTKManager.h
    Framework/CTKManager.cpp
    Framework/StartupOrchestrator.h
    Framework/StartupOrchestrator.cpp
    Framework/VTKWidgetPool.h
    Framework/VTKWidgetPool.cpp
    Framework/ErrorHandler.h
    Framework/ErrorHandler.cpp
    Framework/ConsoleLogBridge.h
    Framework/ConsoleLogBridge.cpp
    Framework/DatabaseManager.h
    Framework/DatabaseManager.cpp
    Framework/VTKGlobalInitializer.h
    Framework/VTKGlobalInitializer.cpp
    Framework/VTKContextValidator.h
    Framework/VTKContextValidator.cpp
    Framework/VTKWidgetFactory.h
    Framework/VTKWidgetFactory.cpp
    Framework/ImageDataTransfer.h
    Framework/ImageDataTransfer.cpp
    Framework/Core/MedicalDataStructures.h
    Framework/Core/MedicalDataStructures.cpp
    Framework/ResourceManagement/SingletonManager.h
    Framework/DataTypes/Point3D.h
    Framework/DataTypes/ImageSlice.h
    Framework/Segmentation/SegmentationTypes.h
    Framework/Segmentation/SegmentationService.h
    Framework/Registration/RegistrationTypes.h
    Framework/Registration/RegistrationService.h
    Framework/Logger.h
    Framework/Platform/Contracts/PlatformRuntimeTypes.h
    Framework/Platform/Contracts/PlatformPluginDescriptor.h
    Framework/Platform/Contracts/PlatformSnapshots.h
    Framework/Platform/Contracts/StartupShellSnapshot.h
    Framework/Platform/Kernel/PlatformDescriptorLoader.h
    Framework/Platform/Kernel/PlatformDescriptorLoader.cpp
    Framework/Platform/Kernel/PlatformDependencyGraph.h
    Framework/Platform/Kernel/PlatformDependencyGraph.cpp
    Framework/Platform/Kernel/PlatformManagedPluginPlan.h
    Framework/Platform/Kernel/PlatformManagedPluginPlan.cpp
    Framework/Platform/Kernel/PlatformRuntimeConfig.h
    Framework/Platform/Kernel/PlatformRuntimeConfig.cpp
    Framework/Platform/Kernel/PlatformCtkPolicyBridge.h
    Framework/Platform/Kernel/PlatformCtkPolicyBridge.cpp
    Framework/Platform/Kernel/PlatformStartupCoordinator.h
    Framework/Platform/Kernel/PlatformStartupCoordinator.cpp
    Framework/Platform/Kernel/PlatformOnDemandActivationPlan.h
    Framework/Platform/Kernel/PlatformOnDemandActivationPlan.cpp
    Framework/Platform/Kernel/PlatformOnDemandActivationService.h
    Framework/Platform/Kernel/PlatformOnDemandActivationService.cpp
    Framework/Platform/Kernel/PlatformWarmupCoordinator.h
    Framework/Platform/Kernel/PlatformWarmupCoordinator.cpp
    Framework/Platform/Kernel/PlatformStateStore.h
    Framework/Platform/Kernel/PlatformStateStore.cpp
    Framework/Platform/Bootstrap/StartupBootstrapController.h
    Framework/Platform/Bootstrap/StartupBootstrapController.cpp
    Framework/Platform/Bootstrap/PlatformStateStoreHandoff.h
    Framework/Platform/Bootstrap/PlatformStateStoreHandoff.cpp
    Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.h
    Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.cpp
    Framework/Platform/Diagnostics/PlatformDiagnosticsService.h
    Framework/Platform/Diagnostics/PlatformDiagnosticsService.cpp
    Framework/Platform/Diagnostics/PlatformPluginLifecycleAggregator.h
    Framework/Platform/Diagnostics/PlatformPluginLifecycleAggregator.cpp
    Framework/Platform/Diagnostics/PlatformLifecycleTraceRecorder.h
    Framework/Platform/Diagnostics/PlatformLifecycleTraceRecorder.cpp
    Framework/Platform/Contracts/PlatformFacadePorts.h
    Framework/Platform/Contracts/PlatformUiPorts.h
    Framework/Platform/CtkBridge/CoreUiRuntimeStatusProvider.h
    Framework/Platform/CtkBridge/CoreUiRuntimeStatusProvider.cpp
    Framework/Platform/CtkBridge/NavigationPageServiceAccess.h
    Framework/Platform/CtkBridge/NavigationPageServiceAccess.cpp
    Framework/Platform/LegacyAdapters/LegacyUserManagementAdapter.h
    Framework/Platform/LegacyAdapters/LegacyUserManagementAdapter.cpp
    Framework/Platform/LegacyAdapters/LegacyImagingAdapter.h
    Framework/Platform/LegacyAdapters/LegacyImagingAdapter.cpp
    Framework/Platform/LegacyAdapters/LegacyCoreUiRuntimeAdapter.h
    Framework/Platform/LegacyAdapters/LegacyCoreUiRuntimeAdapter.cpp
    Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.h
    Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.cpp
    Framework/Platform/LegacyAdapters/LegacyNavigationPageServiceAdapter.h
    Framework/Platform/LegacyAdapters/LegacyNavigationPageServiceAdapter.cpp
    Framework/Platform/Facades/IdentityAppService.h
    Framework/Platform/Facades/IdentityAppService.cpp
    Framework/Platform/Facades/ImagingAppService.h
    Framework/Platform/Facades/ImagingAppService.cpp
    Framework/Platform/Facades/NavigationAppService.h
    Framework/Platform/Facades/NavigationAppService.cpp
)

set(MEDICALPRO_PRODUCT_CONFIG_FILES
    platform_runtime.json
    segmentation_config.json
)

set(MEDICALPRO_RUNTIME_CONFIG_FILES
    ${MEDICALPRO_PRODUCT_CONFIG_FILES}
)
```

```text
Delete files:
- Framework/PluginLoadPolicy.h
- Framework/PluginLoadPolicy.cpp
- config/plugin_load_policy.json
- config/plugin_load_policy_compatibility.md
```

- [ ] **Step 4: Re-run the truth-source contract and descriptor/runtime acceptance**

Run:

```powershell
cmake -S . -B build_x64
cmake --build build_x64 --config Release --target medicalpro plugin_truth_source_governance_contract_test
ctest --test-dir build_x64 -C Release -R "plugin_truth_source_governance_contract_test|plugin_truth_source_runtime_contract_test|platform_descriptor_runtime_layout_test|runtime_artifact_layout_test" --output-on-failure
```

Expected:

- Configure: PASS
- Build: PASS
- `plugin_truth_source_governance_contract_test`: PASS
- `plugin_truth_source_runtime_contract_test`: PASS
- `platform_descriptor_runtime_layout_test`: PASS
- `runtime_artifact_layout_test`: PASS

- [ ] **Step 5: Commit the source-shell deletion**

```powershell
git add tests/unit/PluginTruthSourceGovernanceContractTest.cpp Framework/CTKManager.h Framework/CTKManager.cpp CMakeLists.txt
git add -u Framework/PluginLoadPolicy.h Framework/PluginLoadPolicy.cpp config/plugin_load_policy.json config/plugin_load_policy_compatibility.md
git commit -m "refactor: delete plugin load policy shell"
```

### Task 2: Delete compatibility runtime and unit contract wiring

**Files:**
- Modify: `tests/unit/PluginLegacyConsumerGovernanceContractTest.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/unit/CMakeLists.txt`
- Modify: `tests/runtime/verify_runtime_artifacts.cmake`
- Delete: `tests/unit/PluginLoadPolicyCompatibilityResidueContractTest.cpp`

- [ ] **Step 1: Flip legacy-consumer contract to require compatibility test absence**

```cpp
// tests/unit/PluginLegacyConsumerGovernanceContractTest.cpp
#include <QtTest/QtTest>

#include <QFile>
#include <QFileInfo>
#include <QString>

void PluginLegacyConsumerGovernanceContractTest::runtime_acceptance_wiring_separates_product_and_compatibility_artifacts()
{
    const QString testsCMake = readSource(QStringLiteral("tests/CMakeLists.txt"));
    const QString unitTestsCMake = readSource(QStringLiteral("tests/unit/CMakeLists.txt"));
    const QString runtimeScript = readSource(QStringLiteral("tests/runtime/verify_runtime_artifacts.cmake"));

    QVERIFY2(!testsCMake.contains(QStringLiteral("NAME plugin_legacy_compatibility_runtime_contract_test")),
        "tests/CMakeLists.txt still registers plugin_legacy_compatibility_runtime_contract_test");
    QVERIFY2(!runtimeScript.contains(QStringLiteral("verify_plugin_legacy_compatibility_runtime_contract")),
        "verify_runtime_artifacts.cmake still exposes verify_plugin_legacy_compatibility_runtime_contract");
    QVERIFY2(!unitTestsCMake.contains(QStringLiteral("plugin_load_policy_compatibility_residue_contract_test")),
        "tests/unit/CMakeLists.txt still registers plugin_load_policy_compatibility_residue_contract_test");
    QVERIFY2(!QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/tests/unit/PluginLoadPolicyCompatibilityResidueContractTest.cpp")).exists(),
        "tests/unit/PluginLoadPolicyCompatibilityResidueContractTest.cpp still exists");
}
```

- [ ] **Step 2: Run the governance contract and confirm the expected RED failure**

Run:

```powershell
cmake --build build_x64 --config Release --target plugin_legacy_consumer_governance_contract_test
ctest --test-dir build_x64 -C Release -R "plugin_legacy_consumer_governance_contract_test" --output-on-failure
```

Expected:

- Build: PASS
- Test: FAIL with one or more of:
  - `tests/CMakeLists.txt still registers plugin_legacy_compatibility_runtime_contract_test`
  - `verify_runtime_artifacts.cmake still exposes verify_plugin_legacy_compatibility_runtime_contract`
  - `tests/unit/PluginLoadPolicyCompatibilityResidueContractTest.cpp still exists`

- [ ] **Step 3: Remove compatibility runtime and unit contract wiring**

```cmake
// tests/CMakeLists.txt
if(TARGET medicalpro)
    add_test(
        NAME platform_descriptor_runtime_layout_test
        COMMAND ${CMAKE_COMMAND}
            -Druntime_dir=$<TARGET_FILE_DIR:medicalpro>
            -Drequire_platform_descriptors=ON
            -P ${CMAKE_CURRENT_SOURCE_DIR}/runtime/verify_runtime_artifacts.cmake
    )

    add_test(
        NAME plugin_truth_source_runtime_contract_test
        COMMAND ${CMAKE_COMMAND}
            -Druntime_dir=$<TARGET_FILE_DIR:medicalpro>
            -Dverify_plugin_truth_source_runtime_contract=ON
            -P ${CMAKE_CURRENT_SOURCE_DIR}/runtime/verify_runtime_artifacts.cmake
    )
```

```cmake
// tests/unit/CMakeLists.txt
add_executable(plugin_truth_source_governance_contract_test
    PluginTruthSourceGovernanceContractTest.cpp
)

target_include_directories(plugin_truth_source_governance_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(plugin_truth_source_governance_contract_test PRIVATE
    MEDICALPRO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(plugin_truth_source_governance_contract_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME plugin_truth_source_governance_contract_test
    COMMAND plugin_truth_source_governance_contract_test
)

add_executable(plugin_legacy_consumer_governance_contract_test
    PluginLegacyConsumerGovernanceContractTest.cpp
)

target_include_directories(plugin_legacy_consumer_governance_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(plugin_legacy_consumer_governance_contract_test PRIVATE
    MEDICALPRO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(plugin_legacy_consumer_governance_contract_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME plugin_legacy_consumer_governance_contract_test
    COMMAND plugin_legacy_consumer_governance_contract_test
)
```

```cmake
// tests/runtime/verify_runtime_artifacts.cmake
if(verify_plugin_truth_source_runtime_contract)
    set(platform_runtime_file "${runtime_dir}/config/platform_runtime.json")

    append_missing_artifact("${platform_runtime_file}")

    set(platform_descriptor_files
        "${runtime_dir}/plugins/descriptors/UserManagement.json"
        "${runtime_dir}/plugins/descriptors/DicomViewer.json"
        "${runtime_dir}/plugins/descriptors/FourViewDisplay.json"
        "${runtime_dir}/plugins/descriptors/RegistrationCore.json"
        "${runtime_dir}/plugins/descriptors/OpticalTracking.json"
    )

    foreach(descriptor_file IN LISTS platform_descriptor_files)
        append_missing_artifact("${descriptor_file}")
    endforeach()

    if(missing_artifacts)
        string(JOIN "\n - " missing_report ${missing_artifacts})
        message(FATAL_ERROR "plugin_truth_source_runtime_layout_mismatch:\n - ${missing_report}")
    endif()

    require_json_field_value(
        "${platform_runtime_file}"
        "descriptor_directory"
        "plugins/descriptors"
        "platform_descriptor_directory_mismatch"
    )
endif()
```

```text
Delete file:
- tests/unit/PluginLoadPolicyCompatibilityResidueContractTest.cpp
```

- [ ] **Step 4: Re-run governance and runtime acceptance after cleanup**

Run:

```powershell
cmake -S . -B build_x64
cmake --build build_x64 --config Release --target medicalpro plugin_legacy_consumer_governance_contract_test plugin_truth_source_governance_contract_test
ctest --test-dir build_x64 -C Release -R "plugin_legacy_consumer_governance_contract_test|plugin_truth_source_governance_contract_test|plugin_truth_source_runtime_contract_test|platform_descriptor_runtime_layout_test|runtime_artifact_layout_test" --output-on-failure
```

Expected:

- Configure: PASS
- Build: PASS
- `plugin_legacy_consumer_governance_contract_test`: PASS
- `plugin_truth_source_governance_contract_test`: PASS
- `plugin_truth_source_runtime_contract_test`: PASS
- `platform_descriptor_runtime_layout_test`: PASS
- `runtime_artifact_layout_test`: PASS

- [ ] **Step 5: Commit the contract wiring cleanup**

```powershell
git add tests/unit/PluginLegacyConsumerGovernanceContractTest.cpp tests/CMakeLists.txt tests/unit/CMakeLists.txt tests/runtime/verify_runtime_artifacts.cmake
git add -u tests/unit/PluginLoadPolicyCompatibilityResidueContractTest.cpp
git commit -m "test: remove plugin load policy compatibility contracts"
```

### Task 3: Rewrite governance tracking to record deleted shell semantics

**Files:**
- Modify: `tests/unit/PluginLegacyConsumerGovernanceContractTest.cpp`
- Modify: `docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md`
- Modify: `docs/superpowers/tracking/platform-plugin-governance-matrix.md`
- Modify: `docs/superpowers/tracking/platform-migration-decision-log.md`

- [ ] **Step 1: Rewrite governance contract expectations from retention to deletion**

```cpp
// tests/unit/PluginLegacyConsumerGovernanceContractTest.cpp
void PluginLegacyConsumerGovernanceContractTest::legacy_consumer_inventory_classifies_current_consumers()
{
    const QString inventory = readSource(QStringLiteral("docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md"));

    QVERIFY2(inventory.contains(QStringLiteral("`main.cpp` | `forbidden_product_mainline`")),
        "legacy consumer inventory does not classify main.cpp as forbidden_product_mainline");
    QVERIFY2(inventory.contains(QStringLiteral("## Deleted Compatibility Shell")),
        "legacy consumer inventory does not record the deleted compatibility shell section");
    QVERIFY2(inventory.contains(QStringLiteral("`PluginLoadPolicy`")),
        "legacy consumer inventory does not record deleted PluginLoadPolicy");
    QVERIFY2(inventory.contains(QStringLiteral("forbidden_reintroduction")),
        "legacy consumer inventory does not mark shell reintroduction as forbidden");
    QVERIFY2(!inventory.contains(QStringLiteral("`config/plugin_load_policy.json` | `allowed_compatibility_surface`")),
        "legacy consumer inventory still classifies plugin_load_policy.json as allowed_compatibility_surface");
    QVERIFY2(!inventory.contains(QStringLiteral("`CTKManager::loadPluginPolicy()` | `allowed_compatibility_surface`")),
        "legacy consumer inventory still classifies CTKManager::loadPluginPolicy() as allowed_compatibility_surface");
}

void PluginLegacyConsumerGovernanceContractTest::governance_docs_record_descriptor_policy_bridge_ownership()
{
    const QString governanceMatrix =
        readSource(QStringLiteral("docs/superpowers/tracking/platform-plugin-governance-matrix.md"));
    const QString decisionLog =
        readSource(QStringLiteral("docs/superpowers/tracking/platform-migration-decision-log.md"));

    QVERIFY2(governanceMatrix.contains(QStringLiteral("compatibility shell deletion is complete"), Qt::CaseInsensitive),
        "governance matrix does not record completed compatibility shell deletion");
    QVERIFY2(governanceMatrix.contains(QStringLiteral("no repository-recognized legacy load-policy entry point"), Qt::CaseInsensitive),
        "governance matrix does not record absence of legacy load-policy entry points");
    QVERIFY2(decisionLog.contains(QStringLiteral("delete the final `plugin_load_policy` compatibility shell")),
        "decision log does not record the shell deletion decision");
    QVERIFY2(decisionLog.contains(QStringLiteral("delete adjacent dead legacy helpers")),
        "decision log does not record adjacent helper deletion");
    QVERIFY2(!decisionLog.contains(QStringLiteral("keep `plugin_load_policy.json` as a minimal compatibility projection")),
        "decision log still records plugin_load_policy.json retention");
}
```

- [ ] **Step 2: Run the governance contract and confirm the expected RED failure**

Run:

```powershell
cmake --build build_x64 --config Release --target plugin_legacy_consumer_governance_contract_test
ctest --test-dir build_x64 -C Release -R "plugin_legacy_consumer_governance_contract_test" --output-on-failure
```

Expected:

- Build: PASS
- Test: FAIL with one or more of:
  - `legacy consumer inventory does not record the deleted compatibility shell section`
  - `governance matrix does not record completed compatibility shell deletion`
  - `decision log does not record the shell deletion decision`

- [ ] **Step 3: Rewrite the inventory, governance matrix, and decision log**

```md
<!-- docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md -->
# Platform Plugin Legacy Consumer Inventory

Updated: 2026-04-23

## Current Consumers

| Consumer | Bucket | Status | Notes | Next Step |
| --- | --- | --- | --- | --- |
| `main.cpp` | `forbidden_product_mainline` | enforced | Product startup must remain descriptor-driven and must not call deleted legacy load-policy helpers. | Protected by source contract tests. |
| `runtime_artifact_layout_test` | `forbidden_product_mainline` | enforced | Default runtime artifact acceptance must validate product artifacts only. | Must not require deleted compatibility artifacts. |

## Deleted Compatibility Shell

| Surface | Status | Notes |
| --- | --- | --- |
| `PluginLoadPolicy` | deleted | Former compatibility carrier is removed from source. |
| `config/plugin_load_policy.json` | deleted | Former compatibility projection is no longer shipped. |
| `config/plugin_load_policy_compatibility.md` | deleted | Former sidecar note is removed with the projection. |
| `CTKManager::loadPluginPolicy()` | deleted | Repository no longer recognizes a legacy load-policy entry point. |
| `CTKManager::installPluginsFromDirectory()` | deleted | Repository no longer recognizes a compatibility directory-scan startup side path. |
| `CTKManager::setPluginLoadOrder()` | deleted | Manual legacy load-order override is removed. |
| `CTKManager::getRecommendedLoadOrder()` | deleted | Manual legacy load-order recommendation is removed. |

## Retained Runtime Ownership

- `CTKManager::applyPolicyForPlugin()` remains an execution helper, but runtime bucket and criticality classification continue to resolve through `PlatformCtkPolicyBridge` with explicit `setDescriptorPolicyContext(...)` handoff.
- Product startup truth remains `platform_runtime.json + plugins/descriptors/*.json + PlatformDescriptorLoader`.

## Forbidden New Usage

- `plugin_load_policy` compatibility shell reintroduction is `forbidden_reintroduction`.
- No new product-mainline code may introduce a second plugin-loading truth source.
- Any future legacy loading surface must be treated as new technical debt, not as restoration of approved compatibility residue.
```

```md
<!-- docs/superpowers/tracking/platform-plugin-governance-matrix.md -->
## Current Implementation Notes

- `config/platform_runtime.json` stores platform descriptor ids, not CTK symbolic names.
- Product startup truth is explicitly `platform_runtime.json + plugins/descriptors/*.json + PlatformDescriptorLoader`.
- `main.cpp` performs explicit descriptor policy handoff through `ctkManager->setDescriptorPolicyContext(runtimeConfig, descriptors)` before plugin install/start orchestration.
- `CTKManager::setDescriptorPolicyContext()` is the governed descriptor policy handoff boundary between startup assembly and CTK runtime classification.
- `CTKManager` runtime bucket classification now resolves through `PlatformCtkPolicyBridge` instead of legacy load-policy lookup.
- Safe mode criticality now follows `platform_runtime.json.core_plugin_ids`.
- Plugin load policy compatibility shell deletion is complete.
- There is no repository-recognized legacy load-policy entry point or directory-scan startup side path.
- `runtime_artifact_layout_test` covers product-mainline runtime artifacts only.
- The authoritative human-readable inventory for remaining legacy consumers is `docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md`.
```

```md
<!-- docs/superpowers/tracking/platform-migration-decision-log.md -->
## 2026-04-23

- Decision: delete the final `plugin_load_policy` compatibility shell.
- Rationale: repository-local scans show that the remaining shell is kept alive only by deployment, contracts, and documentation rather than live runtime consumers.
- Impact: `PluginLoadPolicy`, `plugin_load_policy.json`, `plugin_load_policy_compatibility.md`, and `CTKManager::loadPluginPolicy()` are removed from the repository.

- Decision: delete adjacent dead legacy helpers from the same compatibility-era loading path.
- Rationale: keeping directory-scan and manual load-order helpers would preserve a half-dead legacy loading side path after shell deletion.
- Impact: `CTKManager::installPluginsFromDirectory()`, `CTKManager::setPluginLoadOrder()`, and `CTKManager::getRecommendedLoadOrder()` are removed alongside the shell.

- Decision: remove compatibility runtime acceptance for the deleted shell.
- Rationale: repository contracts must protect shell absence rather than continue validating deleted compatibility artifacts.
- Impact: compatibility runtime and residue contracts are deleted, while descriptor/runtime truth contracts remain in place.
```

- [ ] **Step 4: Re-run governance contracts after doc rewrite**

Run:

```powershell
cmake --build build_x64 --config Release --target plugin_legacy_consumer_governance_contract_test plugin_truth_source_governance_contract_test
ctest --test-dir build_x64 -C Release -R "plugin_legacy_consumer_governance_contract_test|plugin_truth_source_governance_contract_test" --output-on-failure
```

Expected:

- Build: PASS
- `plugin_legacy_consumer_governance_contract_test`: PASS
- `plugin_truth_source_governance_contract_test`: PASS

- [ ] **Step 5: Commit the governance tracking rewrite**

```powershell
git add tests/unit/PluginLegacyConsumerGovernanceContractTest.cpp docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md docs/superpowers/tracking/platform-plugin-governance-matrix.md docs/superpowers/tracking/platform-migration-decision-log.md
git commit -m "docs: record plugin load policy shell deletion governance"
```

### Task 4: Record acceptance and run the final shell-deletion verification suite

**Files:**
- Modify: `tests/unit/PluginLegacyConsumerGovernanceContractTest.cpp`
- Modify: `docs/current_status_and_project_overview.md`

- [ ] **Step 1: Extend governance contract so current status must record shell deletion acceptance**

```cpp
// tests/unit/PluginLegacyConsumerGovernanceContractTest.cpp
void PluginLegacyConsumerGovernanceContractTest::governance_docs_record_descriptor_policy_bridge_ownership()
{
    const QString governanceMatrix =
        readSource(QStringLiteral("docs/superpowers/tracking/platform-plugin-governance-matrix.md"));
    const QString decisionLog =
        readSource(QStringLiteral("docs/superpowers/tracking/platform-migration-decision-log.md"));
    const QString currentStatus = readSource(QStringLiteral("docs/current_status_and_project_overview.md"));

    QVERIFY2(governanceMatrix.contains(QStringLiteral("PlatformCtkPolicyBridge")),
        "governance matrix does not record PlatformCtkPolicyBridge ownership");
    QVERIFY2(governanceMatrix.contains(QStringLiteral("compatibility shell deletion is complete"), Qt::CaseInsensitive),
        "governance matrix does not record completed compatibility shell deletion");
    QVERIFY2(decisionLog.contains(QStringLiteral("delete the final `plugin_load_policy` compatibility shell")),
        "decision log does not record the shell deletion decision");
    QVERIFY2(currentStatus.contains(QStringLiteral("Plugin Load Policy Compatibility Shell Deletion Acceptance")),
        "current status does not record shell deletion acceptance");
    QVERIFY2(currentStatus.contains(QStringLiteral("compatibility shell deletion is complete"), Qt::CaseInsensitive),
        "current status does not describe completed compatibility shell deletion");
}
```

- [ ] **Step 2: Run the governance contract and confirm the expected RED failure**

Run:

```powershell
cmake --build build_x64 --config Release --target plugin_legacy_consumer_governance_contract_test
ctest --test-dir build_x64 -C Release -R "plugin_legacy_consumer_governance_contract_test" --output-on-failure
```

Expected:

- Build: PASS
- Test: FAIL with:
  - `current status does not record shell deletion acceptance`

- [ ] **Step 3: Add the acceptance note to current status**

```md
<!-- docs/current_status_and_project_overview.md -->
### 2026-04-23 Plugin Load Policy Compatibility Shell Deletion Acceptance

- `PluginLoadPolicy`, `config/plugin_load_policy.json`, and `config/plugin_load_policy_compatibility.md` are deleted from the repository and runtime layout.
- `CTKManager::loadPluginPolicy()`, `CTKManager::installPluginsFromDirectory()`, `setPluginLoadOrder()`, and `getRecommendedLoadOrder()` are removed.
- Compatibility runtime and residue contracts are removed; descriptor/runtime truth contracts remain the acceptance owners.
- Compatibility shell deletion is complete, while CTK runtime execution remains intact.
- Executed command (configure):
  - `cmake -S . -B build_x64`
- Executed command (build):
  - `cmake --build build_x64 --config Release --target medicalpro plugin_truth_source_governance_contract_test plugin_legacy_consumer_governance_contract_test`
- Executed command (ctest):
  - `ctest --test-dir build_x64 -C Release -R "plugin_truth_source_governance_contract_test|plugin_legacy_consumer_governance_contract_test|plugin_truth_source_runtime_contract_test|platform_descriptor_runtime_layout_test|runtime_artifact_layout_test" --output-on-failure`
- Expected outcomes alignment and actual results:
  - Build target chain: PASS.
  - Governance contract suite: PASS.
  - Runtime truth-source suite: PASS.
```

- [ ] **Step 4: Run the final shell-deletion acceptance suite**

Run:

```powershell
cmake -S . -B build_x64
cmake --build build_x64 --config Release --target medicalpro plugin_truth_source_governance_contract_test plugin_legacy_consumer_governance_contract_test
ctest --test-dir build_x64 -C Release -R "plugin_truth_source_governance_contract_test|plugin_legacy_consumer_governance_contract_test|plugin_truth_source_runtime_contract_test|platform_descriptor_runtime_layout_test|runtime_artifact_layout_test" --output-on-failure
```

Expected:

- Configure: PASS
- Build: PASS
- `plugin_truth_source_governance_contract_test`: PASS
- `plugin_legacy_consumer_governance_contract_test`: PASS
- `plugin_truth_source_runtime_contract_test`: PASS
- `platform_descriptor_runtime_layout_test`: PASS
- `runtime_artifact_layout_test`: PASS

- [ ] **Step 5: Commit the acceptance write-back**

```powershell
git add tests/unit/PluginLegacyConsumerGovernanceContractTest.cpp docs/current_status_and_project_overview.md
git commit -m "docs: record plugin load policy shell deletion acceptance"
```
