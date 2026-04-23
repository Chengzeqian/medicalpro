# Plugin Load Policy Compatibility Residue Minimization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reduce `PluginLoadPolicy / plugin_load_policy.json` to an explicit minimal compatibility shell without changing product startup truth or CTK runtime classification semantics.

**Architecture:** Keep `CTKManager::loadPluginPolicy()` as the last compatibility entry, but shrink `PluginLoadPolicy` into a metadata-only carrier and reduce `plugin_load_policy.json` to a descriptor-governed compatibility projection. Lock that boundary with focused contract tests, then make compatibility artifact deployment explicit in `CMakeLists.txt`, and finally write the new residue-minimization model back into governance docs.

**Tech Stack:** CMake, Qt 6, QtTest, JSON config files, existing `Framework` shared library, governance docs and runtime contracts under `docs/superpowers` and `tests`

---

## Files And Responsibilities

- Modify: `Framework/PluginLoadPolicy.h`
  - Remove dead query-style policy APIs and keep only the minimal compatibility carrier surface.
- Modify: `Framework/PluginLoadPolicy.cpp`
  - Stop parsing legacy runtime strategy fields and only track compatibility projection presence plus config path.
- Modify: `config/plugin_load_policy.json`
  - Reduce the file to a minimal compatibility projection limited to the descriptor-governed CTK plugin set.
- Modify: `config/plugin_load_policy_compatibility.md`
  - Explicitly describe the file as a compatibility projection sourced from runtime config plus descriptors.
- Modify: `CMakeLists.txt`
  - Replace whole-directory `config/` copying with explicit config artifact deployment, including named compatibility artifacts.
- Modify: `tests/unit/CMakeLists.txt`
  - Register a dedicated `plugin_load_policy_compatibility_residue_contract_test` target.
- Create: `tests/unit/PluginLoadPolicyCompatibilityResidueContractTest.cpp`
  - Lock the minimized API surface, the reduced projection contents, and the explicit deployment wiring.
- Modify: `tests/runtime/verify_runtime_artifacts.cmake`
  - Tighten the compatibility runtime contract so deployed compatibility artifacts are verified as the reduced projection shell.
- Modify: `tests/unit/PluginLegacyConsumerGovernanceContractTest.cpp`
  - Update governance source contracts so docs and inventory must describe the minimized residue model.
- Modify: `docs/current_status_and_project_overview.md`
  - Record the 2026-04-23 acceptance summary for residue minimization.
- Modify: `docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md`
  - Reclassify `PluginLoadPolicy` and `plugin_load_policy.json` as shrunk compatibility surfaces.
- Modify: `docs/superpowers/tracking/platform-plugin-governance-matrix.md`
  - Record the minimal carrier / minimal projection boundary and explicit deployment ownership.
- Modify: `docs/superpowers/tracking/platform-migration-decision-log.md`
  - Record the new decisions for compatibility carrier shrinkage, projection shrinkage, and explicit deployment.

### Task 1: Shrink `PluginLoadPolicy` to a minimal compatibility carrier

**Files:**
- Modify: `tests/unit/CMakeLists.txt`
- Create: `tests/unit/PluginLoadPolicyCompatibilityResidueContractTest.cpp`
- Modify: `Framework/PluginLoadPolicy.h`
- Modify: `Framework/PluginLoadPolicy.cpp`

- [ ] **Step 1: Write the failing API-surface contract and register its test target**

```cmake
# tests/unit/CMakeLists.txt
add_executable(plugin_load_policy_compatibility_residue_contract_test
    PluginLoadPolicyCompatibilityResidueContractTest.cpp
)

target_include_directories(plugin_load_policy_compatibility_residue_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(plugin_load_policy_compatibility_residue_contract_test PRIVATE
    MEDICALPRO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(plugin_load_policy_compatibility_residue_contract_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME plugin_load_policy_compatibility_residue_contract_test
    COMMAND plugin_load_policy_compatibility_residue_contract_test
)
```

```cpp
// tests/unit/PluginLoadPolicyCompatibilityResidueContractTest.cpp
#include <QtTest/QtTest>

#include <QFile>
#include <QString>

class PluginLoadPolicyCompatibilityResidueContractTest : public QObject
{
    Q_OBJECT

private slots:
    void plugin_load_policy_surface_is_minimal();

private:
    QString readSource(const QString& relativePath) const;
};

QString PluginLoadPolicyCompatibilityResidueContractTest::readSource(const QString& relativePath) const
{
    QFile sourceFile(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
    if (!sourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTest::qFail(qPrintable(QStringLiteral("failed to read source file: %1").arg(relativePath)), __FILE__, __LINE__);
        return {};
    }
    return QString::fromUtf8(sourceFile.readAll());
}

void PluginLoadPolicyCompatibilityResidueContractTest::plugin_load_policy_surface_is_minimal()
{
    const QString header = readSource(QStringLiteral("Framework/PluginLoadPolicy.h"));
    const QString source = readSource(QStringLiteral("Framework/PluginLoadPolicy.cpp"));

    QVERIFY2(header.contains(QStringLiteral("void loadConfig(const QString& configFilePath);")),
        "PluginLoadPolicy.h no longer exposes loadConfig()");
    QVERIFY2(header.contains(QStringLiteral("QString configPath() const;")),
        "PluginLoadPolicy.h no longer exposes configPath()");
    QVERIFY2(header.contains(QStringLiteral("bool hasValidConfig() const;")),
        "PluginLoadPolicy.h no longer exposes hasValidConfig()");
    QVERIFY2(!header.contains(QStringLiteral("LoadPolicy getLoadPolicy(")),
        "PluginLoadPolicy.h still exposes getLoadPolicy()");
    QVERIFY2(!header.contains(QStringLiteral("QStringList getDependencies(")),
        "PluginLoadPolicy.h still exposes getDependencies()");
    QVERIFY2(!header.contains(QStringLiteral("bool isCriticalPlugin(")),
        "PluginLoadPolicy.h still exposes isCriticalPlugin()");
    QVERIFY2(!header.contains(QStringLiteral("QStringList getPluginsByPolicy(")),
        "PluginLoadPolicy.h still exposes getPluginsByPolicy()");
    QVERIFY2(!header.contains(QStringLiteral("QStringList getCriticalPlugins()")),
        "PluginLoadPolicy.h still exposes getCriticalPlugins()");
    QVERIFY2(!header.contains(QStringLiteral("QStringList getAllConfiguredPlugins()")),
        "PluginLoadPolicy.h still exposes getAllConfiguredPlugins()");
    QVERIFY2(!source.contains(QStringLiteral("LoadPolicy PluginLoadPolicy::getLoadPolicy(")),
        "PluginLoadPolicy.cpp still implements getLoadPolicy()");
    QVERIFY2(!source.contains(QStringLiteral("PluginLoadPolicy::isCriticalPlugin(")),
        "PluginLoadPolicy.cpp still implements isCriticalPlugin()");
    QVERIFY2(source.contains(QStringLiteral("compatibility-only")),
        "PluginLoadPolicy.cpp no longer logs compatibility-only language");
}

QTEST_APPLESS_MAIN(PluginLoadPolicyCompatibilityResidueContractTest)
#include "PluginLoadPolicyCompatibilityResidueContractTest.moc"
```

- [ ] **Step 2: Run the new contract test and confirm the expected RED failure**

Run:

```powershell
cmake --build build_x64 --config Release --target plugin_load_policy_compatibility_residue_contract_test
ctest --test-dir build_x64 -C Release -R "plugin_load_policy_compatibility_residue_contract_test" --output-on-failure
```

Expected:

- Build: PASS
- Test: FAIL with one or more of:
  - `PluginLoadPolicy.h still exposes getLoadPolicy()`
  - `PluginLoadPolicy.h still exposes getDependencies()`
  - `PluginLoadPolicy.cpp still implements getLoadPolicy()`

- [ ] **Step 3: Remove the dead query APIs and simplify `PluginLoadPolicy` implementation**

```cpp
// Framework/PluginLoadPolicy.h
#ifndef PLUGINLOADPOLICY_H
#define PLUGINLOADPOLICY_H

#include "FrameworkExport.h"
#include "ResourceManagement/SingletonManager.h"

#include <QObject>
#include <QMutex>
#include <QString>
#include <QStringList>

/**
 * @brief compatibility-only legacy plugin policy carrier.
 *
 * Product startup truth comes from platform_runtime.json + plugins/descriptors/*.json.
 * This type only tracks whether a compatibility projection was loaded for legacy paths.
 */
class FRAMEWORK_EXPORT PluginLoadPolicy : public QObject, public SingletonManager<PluginLoadPolicy>
{
    Q_OBJECT
    friend class SingletonManager<PluginLoadPolicy>;

public:
    static PluginLoadPolicy* instance() { return &SingletonManager<PluginLoadPolicy>::instance(); }

    void loadConfig(const QString& configFilePath);
    QString configPath() const;
    bool hasValidConfig() const;

signals:
    void policyReloaded();

private:
    PluginLoadPolicy();

    void clearState();

    mutable QMutex m_mutex;
    QStringList m_configuredPluginNames;
    QString m_configPath;
    bool m_hasValidConfig;
};

#endif // PLUGINLOADPOLICY_H
```

```cpp
// Framework/PluginLoadPolicy.cpp
#include "PluginLoadPolicy.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMutexLocker>

#include "Logger.h"

PluginLoadPolicy::PluginLoadPolicy()
    : QObject(nullptr)
    , m_hasValidConfig(false)
{
}

void PluginLoadPolicy::loadConfig(const QString& configFilePath)
{
    QFile file(configFilePath);
    if (!file.exists()) {
        LOG_WARNING(
            "PluginLoadPolicy",
            QString("Compatibility-only projection file not found: %1").arg(configFilePath));
        QMutexLocker locker(&m_mutex);
        clearState();
        return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_ERROR("PluginLoadPolicy", QString("Failed to open config file: %1").arg(configFilePath));
        QMutexLocker locker(&m_mutex);
        clearState();
        return;
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        LOG_ERROR("PluginLoadPolicy", QString("Failed to parse config: %1").arg(parseError.errorString()));
        QMutexLocker locker(&m_mutex);
        clearState();
        return;
    }

    if (!document.isObject()) {
        LOG_ERROR("PluginLoadPolicy", "Invalid config format: root element must be an object");
        QMutexLocker locker(&m_mutex);
        clearState();
        return;
    }

    const QJsonArray pluginsArray = document.object().value(QStringLiteral("plugins")).toArray();
    QStringList pluginNames;
    pluginNames.reserve(pluginsArray.size());

    for (const QJsonValue& entryValue : pluginsArray) {
        if (!entryValue.isObject()) {
            continue;
        }

        const QString pluginName = entryValue.toObject().value(QStringLiteral("name")).toString().trimmed();
        if (!pluginName.isEmpty()) {
            pluginNames.append(pluginName);
        }
    }

    {
        QMutexLocker locker(&m_mutex);
        m_configuredPluginNames = pluginNames;
        m_configPath = configFilePath;
        m_hasValidConfig = !m_configuredPluginNames.isEmpty();
    }

    emit policyReloaded();

    LOG_INFO(
        "PluginLoadPolicy",
        QString("Loaded compatibility-only plugin policy projection from %1 with %2 entries")
            .arg(configFilePath)
            .arg(pluginNames.size()));
}

QString PluginLoadPolicy::configPath() const
{
    QMutexLocker locker(&m_mutex);
    return m_configPath;
}

bool PluginLoadPolicy::hasValidConfig() const
{
    QMutexLocker locker(&m_mutex);
    return m_hasValidConfig;
}

void PluginLoadPolicy::clearState()
{
    m_configuredPluginNames.clear();
    m_configPath.clear();
    m_hasValidConfig = false;
}
```

- [ ] **Step 4: Re-run the residue contract and the truth-source contract**

Run:

```powershell
cmake --build build_x64 --config Release --target plugin_load_policy_compatibility_residue_contract_test plugin_truth_source_governance_contract_test
ctest --test-dir build_x64 -C Release -R "plugin_load_policy_compatibility_residue_contract_test|plugin_truth_source_governance_contract_test" --output-on-failure
```

Expected:

- Build: PASS
- `plugin_load_policy_compatibility_residue_contract_test`: PASS
- `plugin_truth_source_governance_contract_test`: PASS

- [ ] **Step 5: Commit the minimal-carrier refactor**

```powershell
git add tests/unit/CMakeLists.txt tests/unit/PluginLoadPolicyCompatibilityResidueContractTest.cpp Framework/PluginLoadPolicy.h Framework/PluginLoadPolicy.cpp
git commit -m "refactor: shrink plugin load policy compatibility carrier"
```

### Task 2: Reduce `plugin_load_policy.json` to the minimal compatibility projection

**Files:**
- Modify: `tests/unit/PluginLoadPolicyCompatibilityResidueContractTest.cpp`
- Modify: `tests/runtime/verify_runtime_artifacts.cmake`
- Modify: `config/plugin_load_policy.json`
- Modify: `config/plugin_load_policy_compatibility.md`

- [ ] **Step 1: Extend the residue contract with projection-shape assertions**

```cpp
// tests/unit/PluginLoadPolicyCompatibilityResidueContractTest.cpp
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

private slots:
    void plugin_load_policy_surface_is_minimal();
    void plugin_load_policy_projection_contains_only_descriptor_governed_plugins();
    void compatibility_note_describes_projection_boundary();

QJsonDocument readJson(const QString& relativePath) const;

QJsonDocument PluginLoadPolicyCompatibilityResidueContractTest::readJson(const QString& relativePath) const
{
    return QJsonDocument::fromJson(readSource(relativePath).toUtf8());
}

void PluginLoadPolicyCompatibilityResidueContractTest::plugin_load_policy_projection_contains_only_descriptor_governed_plugins()
{
    const QJsonDocument document = readJson(QStringLiteral("config/plugin_load_policy.json"));
    const QJsonObject root = document.object();
    const QJsonArray plugins = root.value(QStringLiteral("plugins")).toArray();
    QStringList actualNames;
    for (const QJsonValue& entry : plugins) {
        actualNames.append(entry.toObject().value(QStringLiteral("name")).toString());
    }

    const QStringList expectedNames{
        QStringLiteral("UserManagement"),
        QStringLiteral("DicomViewer"),
        QStringLiteral("FourViewDisplay"),
        QStringLiteral("RegistrationCore"),
        QStringLiteral("OpticalTracking")
    };

    QCOMPARE(actualNames, expectedNames);
    QCOMPARE(root.value(QStringLiteral("projection_scope")).toString(),
        QStringLiteral("descriptor_governed_ctk_plugin_set"));
    QVERIFY2(!actualNames.contains(QStringLiteral("BoneSegmentation")),
        "plugin_load_policy.json still contains BoneSegmentation");
    QVERIFY2(!actualNames.contains(QStringLiteral("InstrumentManagement")),
        "plugin_load_policy.json still contains InstrumentManagement");
    QVERIFY2(!actualNames.contains(QStringLiteral("Registration2D3D")),
        "plugin_load_policy.json still contains Registration2D3D");
    QVERIFY2(!actualNames.contains(QStringLiteral("PointRegistration")),
        "plugin_load_policy.json still contains PointRegistration");
    QVERIFY2(!actualNames.contains(QStringLiteral("OpticalRegistration")),
        "plugin_load_policy.json still contains OpticalRegistration");
}

void PluginLoadPolicyCompatibilityResidueContractTest::compatibility_note_describes_projection_boundary()
{
    const QString note = readSource(QStringLiteral("config/plugin_load_policy_compatibility.md"));

    QVERIFY2(note.contains(QStringLiteral("compatibility-only runtime projection")),
        "plugin_load_policy_compatibility.md does not describe the file as a projection");
    QVERIFY2(note.contains(QStringLiteral("descriptor-governed CTK plugin set")),
        "plugin_load_policy_compatibility.md does not describe the reduced projection scope");
    QVERIFY2(note.contains(QStringLiteral("must not define the product mainline")),
        "plugin_load_policy_compatibility.md lost the product-mainline boundary");
}
```

- [ ] **Step 2: Run the residue contract and the compatibility runtime contract to get the expected RED failure**

Run:

```powershell
cmake --build build_x64 --config Release --target medicalpro plugin_load_policy_compatibility_residue_contract_test
ctest --test-dir build_x64 -C Release -R "plugin_load_policy_compatibility_residue_contract_test|plugin_legacy_compatibility_runtime_contract_test" --output-on-failure
```

Expected:

- Build: PASS
- `plugin_load_policy_compatibility_residue_contract_test`: FAIL with one or more of:
  - `plugin_load_policy.json still contains BoneSegmentation`
  - `plugin_load_policy_compatibility.md does not describe the file as a projection`
- `plugin_legacy_compatibility_runtime_contract_test`: still PASS at this point, because it does not yet validate the reduced projection shape

- [ ] **Step 3: Rewrite the projection file, rewrite the sidecar note, and tighten the runtime contract**

```json
// config/plugin_load_policy.json
{
  "projection_source": "config/platform_runtime.json + plugins/descriptors/*.json",
  "projection_scope": "descriptor_governed_ctk_plugin_set",
  "plugins": [
    { "name": "UserManagement" },
    { "name": "DicomViewer" },
    { "name": "FourViewDisplay" },
    { "name": "RegistrationCore" },
    { "name": "OpticalTracking" }
  ]
}
```

```md
<!-- config/plugin_load_policy_compatibility.md -->
# plugin_load_policy compatibility note

- `config/plugin_load_policy.json` is a compatibility-only runtime projection.
- Projection source comes from `config/platform_runtime.json` and `plugins/descriptors/*.json`.
- The projection is limited to the descriptor-governed CTK plugin set: `UserManagement`, `DicomViewer`, `FourViewDisplay`, `RegistrationCore`, and `OpticalTracking`.
- `CTKManager::loadPluginPolicy()` may load this projection for compatibility paths, but it must not define the product mainline or CTK runtime classification semantics.
```

```cmake
# tests/runtime/verify_runtime_artifacts.cmake
if(verify_plugin_legacy_compatibility_runtime_contract)
    set(plugin_policy_file "${runtime_dir}/config/plugin_load_policy.json")
    set(plugin_policy_note_file "${runtime_dir}/config/plugin_load_policy_compatibility.md")

    append_missing_artifact("${plugin_policy_file}")
    append_missing_artifact("${plugin_policy_note_file}")

    if(missing_artifacts)
        string(JOIN "\n - " missing_report ${missing_artifacts})
        message(FATAL_ERROR "plugin_legacy_compatibility_runtime_layout_mismatch:\n - ${missing_report}")
    endif()

    file(READ "${plugin_policy_file}" plugin_policy_text)
    if(NOT plugin_policy_text MATCHES "\"projection_scope\"[ \t\r\n]*:[ \t\r\n]*\"descriptor_governed_ctk_plugin_set\"")
        message(FATAL_ERROR "plugin_policy_projection_scope_mismatch: ${plugin_policy_file}")
    endif()

    set(expected_projection_plugins
        UserManagement
        DicomViewer
        FourViewDisplay
        RegistrationCore
        OpticalTracking
    )

    foreach(expected_name IN LISTS expected_projection_plugins)
        if(NOT plugin_policy_text MATCHES "\"name\"[ \t\r\n]*:[ \t\r\n]*\"${expected_name}\"")
            message(FATAL_ERROR "plugin_policy_missing_projection_entry: ${expected_name}")
        endif()
    endforeach()

    set(forbidden_projection_plugins
        BoneSegmentation
        InstrumentManagement
        Registration2D3D
        PointRegistration
        OpticalRegistration
    )

    foreach(forbidden_name IN LISTS forbidden_projection_plugins)
        if(plugin_policy_text MATCHES "\"name\"[ \t\r\n]*:[ \t\r\n]*\"${forbidden_name}\"")
            message(FATAL_ERROR "plugin_policy_contains_legacy_entry: ${forbidden_name}")
        endif()
    endforeach()

    file(READ "${plugin_policy_note_file}" plugin_policy_note_text)
    if(NOT plugin_policy_note_text MATCHES "compatibility-only runtime projection")
        message(FATAL_ERROR "plugin_policy_note_missing_projection_boundary: ${plugin_policy_note_file}")
    endif()

    if(NOT plugin_policy_note_text MATCHES "descriptor-governed CTK plugin set")
        message(FATAL_ERROR "plugin_policy_note_missing_projection_scope: ${plugin_policy_note_file}")
    endif()

    if(NOT plugin_policy_note_text MATCHES "must not define the product mainline")
        message(FATAL_ERROR "plugin_policy_note_missing_mainline_boundary: ${plugin_policy_note_file}")
    endif()
endif()
```

- [ ] **Step 4: Rebuild the runtime output and verify both the source contract and runtime contract**

Run:

```powershell
cmake --build build_x64 --config Release --target medicalpro plugin_load_policy_compatibility_residue_contract_test
ctest --test-dir build_x64 -C Release -R "plugin_load_policy_compatibility_residue_contract_test|plugin_legacy_compatibility_runtime_contract_test" --output-on-failure
```

Expected:

- Build: PASS
- `plugin_load_policy_compatibility_residue_contract_test`: PASS
- `plugin_legacy_compatibility_runtime_contract_test`: PASS

- [ ] **Step 5: Commit the projection reduction**

```powershell
git add tests/unit/PluginLoadPolicyCompatibilityResidueContractTest.cpp tests/runtime/verify_runtime_artifacts.cmake config/plugin_load_policy.json config/plugin_load_policy_compatibility.md
git commit -m "refactor: reduce plugin load policy compatibility projection"
```

### Task 3: Explicitly deploy compatibility config artifacts from CMake

**Files:**
- Modify: `tests/unit/PluginLoadPolicyCompatibilityResidueContractTest.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Extend the residue contract so deployment must be explicit**

```cpp
// tests/unit/PluginLoadPolicyCompatibilityResidueContractTest.cpp
private slots:
    void plugin_load_policy_surface_is_minimal();
    void plugin_load_policy_projection_contains_only_descriptor_governed_plugins();
    void compatibility_note_describes_projection_boundary();
    void compatibility_artifacts_are_explicitly_deployed();

void PluginLoadPolicyCompatibilityResidueContractTest::compatibility_artifacts_are_explicitly_deployed()
{
    const QString rootCMake = readSource(QStringLiteral("CMakeLists.txt"));

    QVERIFY2(rootCMake.contains(QStringLiteral("MEDICALPRO_COMPATIBILITY_CONFIG_FILES")),
        "CMakeLists.txt does not define explicit compatibility config artifacts");
    QVERIFY2(rootCMake.contains(QStringLiteral("plugin_load_policy.json")),
        "CMakeLists.txt does not explicitly name plugin_load_policy.json");
    QVERIFY2(rootCMake.contains(QStringLiteral("plugin_load_policy_compatibility.md")),
        "CMakeLists.txt does not explicitly name plugin_load_policy_compatibility.md");
    QVERIFY2(!rootCMake.contains(
                 QStringLiteral("COMMAND ${CMAKE_COMMAND} -E copy_directory\n            \"${CMAKE_SOURCE_DIR}/config\"")),
        "CMakeLists.txt still copies the whole config directory");
}
```

- [ ] **Step 2: Run the contract and confirm the expected RED failure**

Run:

```powershell
cmake --build build_x64 --config Release --target plugin_load_policy_compatibility_residue_contract_test
ctest --test-dir build_x64 -C Release -R "plugin_load_policy_compatibility_residue_contract_test" --output-on-failure
```

Expected:

- Build: PASS
- Test: FAIL with one or both of:
  - `CMakeLists.txt does not define explicit compatibility config artifacts`
  - `CMakeLists.txt still copies the whole config directory`

- [ ] **Step 3: Replace whole-directory config copying with explicit file deployment**

```cmake
# CMakeLists.txt
set(MEDICALPRO_PRODUCT_CONFIG_FILES
    platform_runtime.json
    segmentation_config.json
)

set(MEDICALPRO_COMPATIBILITY_CONFIG_FILES
    plugin_load_policy.json
    plugin_load_policy_compatibility.md
)

set(MEDICALPRO_RUNTIME_CONFIG_FILES
    ${MEDICALPRO_PRODUCT_CONFIG_FILES}
    ${MEDICALPRO_COMPATIBILITY_CONFIG_FILES}
)

function(copy_runtime_config_file target_name config_file)
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>/config"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/config/${config_file}"
            "$<TARGET_FILE_DIR:${target_name}>/config/${config_file}"
        COMMENT "Copying config/${config_file} to ${target_name} runtime directory"
    )
endfunction()

if(EXISTS "${CMAKE_SOURCE_DIR}/config")
    foreach(_config_file ${MEDICALPRO_RUNTIME_CONFIG_FILES})
        if(EXISTS "${CMAKE_SOURCE_DIR}/config/${_config_file}")
            copy_runtime_config_file(medicalpro "${_config_file}")

            if(TARGET medicalpro_newui)
                copy_runtime_config_file(medicalpro_newui "${_config_file}")
            endif()
        endif()
    endforeach()

    message(STATUS "Runtime config files will be copied to build output directory")
endif()
```

- [ ] **Step 4: Reconfigure, rebuild, and verify both source and runtime contracts**

Run:

```powershell
cmake -S . -B build_x64
cmake --build build_x64 --config Release --target medicalpro plugin_load_policy_compatibility_residue_contract_test
ctest --test-dir build_x64 -C Release -R "plugin_load_policy_compatibility_residue_contract_test|plugin_legacy_compatibility_runtime_contract_test|runtime_artifact_layout_test|platform_descriptor_runtime_layout_test" --output-on-failure
```

Expected:

- Configure: PASS
- Build: PASS
- `plugin_load_policy_compatibility_residue_contract_test`: PASS
- `plugin_legacy_compatibility_runtime_contract_test`: PASS
- `runtime_artifact_layout_test`: PASS
- `platform_descriptor_runtime_layout_test`: PASS

- [ ] **Step 5: Commit the explicit deployment change**

```powershell
git add tests/unit/PluginLoadPolicyCompatibilityResidueContractTest.cpp CMakeLists.txt
git commit -m "build: explicitly deploy plugin policy compatibility artifacts"
```

### Task 4: Write back governance docs and acceptance

**Files:**
- Modify: `tests/unit/PluginLegacyConsumerGovernanceContractTest.cpp`
- Modify: `docs/current_status_and_project_overview.md`
- Modify: `docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md`
- Modify: `docs/superpowers/tracking/platform-plugin-governance-matrix.md`
- Modify: `docs/superpowers/tracking/platform-migration-decision-log.md`

- [ ] **Step 1: Strengthen the governance contract so docs must describe the minimized residue model**

```cpp
// tests/unit/PluginLegacyConsumerGovernanceContractTest.cpp
void PluginLegacyConsumerGovernanceContractTest::legacy_consumer_inventory_classifies_current_consumers()
{
    const QString inventory = readSource(QStringLiteral("docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md"));

    QVERIFY2(inventory.contains(QStringLiteral("`PluginLoadPolicy` | `allowed_compatibility_surface` | `shrunk`")),
        "legacy consumer inventory does not classify PluginLoadPolicy as a shrunk compatibility surface");
    QVERIFY2(inventory.contains(QStringLiteral("minimal compatibility carrier")),
        "legacy consumer inventory does not describe PluginLoadPolicy as a minimal compatibility carrier");
    QVERIFY2(inventory.contains(QStringLiteral("minimal compatibility projection")),
        "legacy consumer inventory does not describe plugin_load_policy.json as a minimal compatibility projection");
}

void PluginLegacyConsumerGovernanceContractTest::governance_docs_record_descriptor_policy_bridge_ownership()
{
    const QString governanceMatrix =
        readSource(QStringLiteral("docs/superpowers/tracking/platform-plugin-governance-matrix.md"));
    const QString decisionLog =
        readSource(QStringLiteral("docs/superpowers/tracking/platform-migration-decision-log.md"));
    const QString currentStatus = readSource(QStringLiteral("docs/current_status_and_project_overview.md"));

    QVERIFY2(governanceMatrix.contains(QStringLiteral("minimal compatibility carrier")),
        "governance matrix does not record PluginLoadPolicy as a minimal compatibility carrier");
    QVERIFY2(governanceMatrix.contains(QStringLiteral("minimal compatibility projection")),
        "governance matrix does not record plugin_load_policy.json as a minimal compatibility projection");
    QVERIFY2(decisionLog.contains(QStringLiteral("reduce `PluginLoadPolicy` to a minimal compatibility metadata carrier")),
        "decision log does not record the minimal compatibility carrier decision");
    QVERIFY2(decisionLog.contains(QStringLiteral("minimal compatibility projection limited to descriptor-governed CTK plugins")),
        "decision log does not record the reduced projection decision");
    QVERIFY2(currentStatus.contains(QStringLiteral("plugin_load_policy.json is now a minimal compatibility projection")),
        "current status does not record the reduced compatibility projection acceptance");
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
  - `legacy consumer inventory does not classify PluginLoadPolicy as a shrunk compatibility surface`
  - `governance matrix does not record plugin_load_policy.json as a minimal compatibility projection`
  - `current status does not record the reduced compatibility projection acceptance`

- [ ] **Step 3: Update the inventory, governance docs, and acceptance summary**

```md
<!-- docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md -->
| `PluginLoadPolicy` | `allowed_compatibility_surface` | `shrunk` | Minimal compatibility carrier used only by `CTKManager::loadPluginPolicy()`. | Must not expose query-style runtime policy APIs. |
| `config/plugin_load_policy.json` | `allowed_compatibility_surface` | `shrunk` | Minimal compatibility projection limited to the descriptor-governed CTK plugin set. | Must remain aligned with `platform_runtime.json + plugins/descriptors/*.json`. |
| `config/plugin_load_policy_compatibility.md` | `allowed_compatibility_surface` | `retained` | Sidecar note that explains the projection boundary. | Covered by dedicated compatibility runtime contract. |
| `CTKManager::loadPluginPolicy()` | `allowed_compatibility_surface` | `retained` | Final compatibility entry for loading the reduced projection. | Must not be called by product mainline. |
```

```md
<!-- docs/superpowers/tracking/platform-plugin-governance-matrix.md -->
- `PluginLoadPolicy` is now a minimal compatibility carrier and no longer exposes query-style runtime policy APIs.
- `plugin_load_policy.json` is now a minimal compatibility projection limited to the descriptor-governed CTK plugin set.
- Compatibility config artifacts are explicitly deployed from `CMakeLists.txt` instead of surviving only through whole-directory config copying.
```

```md
<!-- docs/superpowers/tracking/platform-migration-decision-log.md -->
## 2026-04-23

- Decision: reduce `PluginLoadPolicy` to a minimal compatibility metadata carrier.
- Rationale: current product code no longer reads legacy policy facts for runtime behavior, so the remaining type must stop looking like a strategy query surface.
- Impact: dead query APIs are removed, `CTKManager::loadPluginPolicy()` remains as the final compatibility entry, and compatibility metadata no longer implies runtime authority.

- Decision: treat `plugin_load_policy.json` as a minimal compatibility projection limited to descriptor-governed CTK plugins.
- Rationale: the file must stop presenting historical non-governed plugin entries as if it were still a general plugin policy table.
- Impact: the projection now contains only `UserManagement`, `DicomViewer`, `FourViewDisplay`, `RegistrationCore`, and `OpticalTracking`, and it is documented as sourced from runtime config plus descriptors.

- Decision: explicitly deploy compatibility config artifacts rather than relying on whole-directory config copying.
- Rationale: compatibility artifact survival must be intentional, test-owned, and discoverable in the build graph.
- Impact: `plugin_load_policy.json` and `plugin_load_policy_compatibility.md` are now named compatibility artifacts in `CMakeLists.txt`.
```

```md
<!-- docs/current_status_and_project_overview.md -->
### 2026-04-23 Plugin Load Policy Compatibility Residue Minimization Acceptance

- `PluginLoadPolicy` is now a minimal compatibility carrier and no longer exposes query-style policy APIs.
- `config/plugin_load_policy.json` is now a minimal compatibility projection limited to the descriptor-governed CTK plugin set.
- Compatibility config artifacts are now explicitly deployed from `CMakeLists.txt` instead of surviving only through whole-directory config copying.
- Executed command (configure):
  - `cmake -S . -B build_x64`
- Executed command (build):
  - `cmake --build build_x64 --config Release --target medicalpro plugin_load_policy_compatibility_residue_contract_test plugin_truth_source_governance_contract_test plugin_legacy_consumer_governance_contract_test`
- Executed command (ctest):
  - `ctest --test-dir build_x64 -C Release -R "plugin_load_policy_compatibility_residue_contract_test|plugin_truth_source_governance_contract_test|plugin_legacy_consumer_governance_contract_test|plugin_legacy_compatibility_runtime_contract_test|plugin_truth_source_runtime_contract_test|runtime_artifact_layout_test|platform_descriptor_runtime_layout_test" --output-on-failure`
- Expected outcomes alignment and actual results:
  - Build target chain: PASS.
  - Compatibility residue contract suite: PASS.
  - Governance/runtime contract suite: PASS.
```

- [ ] **Step 4: Run the full residue-minimization acceptance suite**

Run:

```powershell
cmake --build build_x64 --config Release --target medicalpro plugin_load_policy_compatibility_residue_contract_test plugin_truth_source_governance_contract_test plugin_legacy_consumer_governance_contract_test
ctest --test-dir build_x64 -C Release -R "plugin_load_policy_compatibility_residue_contract_test|plugin_truth_source_governance_contract_test|plugin_legacy_consumer_governance_contract_test|plugin_legacy_compatibility_runtime_contract_test|plugin_truth_source_runtime_contract_test|runtime_artifact_layout_test|platform_descriptor_runtime_layout_test" --output-on-failure
```

Expected:

- Build: PASS
- `plugin_load_policy_compatibility_residue_contract_test`: PASS
- `plugin_truth_source_governance_contract_test`: PASS
- `plugin_legacy_consumer_governance_contract_test`: PASS
- `plugin_legacy_compatibility_runtime_contract_test`: PASS
- `plugin_truth_source_runtime_contract_test`: PASS
- `runtime_artifact_layout_test`: PASS
- `platform_descriptor_runtime_layout_test`: PASS

- [ ] **Step 5: Commit the governance write-back**

```powershell
git add tests/unit/PluginLegacyConsumerGovernanceContractTest.cpp docs/current_status_and_project_overview.md docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md docs/superpowers/tracking/platform-plugin-governance-matrix.md docs/superpowers/tracking/platform-migration-decision-log.md
git commit -m "docs: record plugin load policy residue minimization acceptance"
```
