# Plugin Legacy Consumer Governance Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 legacy CTK load-policy consumer 的剩余使用面正式盘点、分类并锁定，同时把产品 runtime artifact 验收和 compatibility runtime artifact 验收拆开。

**Architecture:** 这批不改产品启动主链，也不直接删除 `PluginLoadPolicy`。先用源码契约测试把 consumer inventory、internal debt 标记和测试边界锁住，再把 runtime acceptance 拆成 `product` 与 `compatibility` 两条独立契约，最后统一回写 current status、decision log、governance matrix 和本计划状态。

**Tech Stack:** CMake, CTest, Qt 6, QtTest, existing runtime verification script under `tests/runtime`, current platform governance docs under `docs/superpowers`

---

## Files And Responsibilities

- Modify: `tests/unit/CMakeLists.txt`
  - 注册新的源码契约测试 `plugin_legacy_consumer_governance_contract_test`
- Create: `tests/unit/PluginLegacyConsumerGovernanceContractTest.cpp`
  - 锁定 legacy consumer inventory、`main.cpp` 禁止使用 legacy helper、`CTKManager` internal debt 标记，以及 runtime acceptance wiring 分界
- Create: `docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md`
  - 记录当前 legacy consumer 的 `allowed_compatibility_surface` / `temporary_internal_compatibility_debt` / `forbidden_product_mainline` 分类
- Modify: `Framework/CTKManager.cpp`
  - 在 `policyForPlugin()` / `applyPolicyForPlugin()` 邻近位置补充 `temporary_internal_compatibility_debt` 注释，明确这是剩余 internal debt
- Modify: `tests/CMakeLists.txt`
  - 把默认 `runtime_artifact_layout_test` 与新的 compatibility runtime contract 分开注册
- Modify: `tests/runtime/verify_runtime_artifacts.cmake`
  - 默认 runtime artifact 校验只覆盖产品主链制品；新增 `plugin_legacy_compatibility_runtime_contract` 分支专门校验 `plugin_load_policy.json + sidecar`
- Modify: `docs/current_status_and_project_overview.md`
  - 回写这批 acceptance，并说明默认 runtime artifact 验收不再把 compatibility artifact 当成通用必需项
- Modify: `docs/superpowers/tracking/platform-migration-decision-log.md`
  - 写入“product runtime artifact acceptance 与 compatibility runtime artifact acceptance 分离”的正式决策
- Modify: `docs/superpowers/tracking/platform-plugin-governance-matrix.md`
  - 补齐 runtime acceptance ownership 与 `temporary_internal_compatibility_debt` 说明
- Modify: `docs/superpowers/plans/2026-04-21-plugin-legacy-consumer-governance-implementation.md`
  - 实施完成后回写状态

### Task 1: 锁定 legacy consumer inventory 和 internal debt 边界

**Files:**
- Modify: `tests/unit/CMakeLists.txt`
- Create: `tests/unit/PluginLegacyConsumerGovernanceContractTest.cpp`
- Create: `docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md`
- Modify: `Framework/CTKManager.cpp`

- [x] **Step 1: 先注册新的源码契约测试 target**

```cmake
# tests/unit/CMakeLists.txt
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

- [x] **Step 2: 写 RED 源码契约测试，把 inventory 和 internal debt 边界先锁死**

```cpp
// tests/unit/PluginLegacyConsumerGovernanceContractTest.cpp
#include <QtTest/QtTest>

#include <QFile>
#include <QString>

class PluginLegacyConsumerGovernanceContractTest : public QObject
{
    Q_OBJECT

private slots:
    void main_cpp_remains_forbidden_product_mainline_consumer();
    void legacy_consumer_inventory_classifies_current_consumers();
    void ctk_manager_internal_policy_helpers_are_marked_as_temporary_internal_compatibility_debt();

private:
    QString readSource(const QString& relativePath) const;
};

QString PluginLegacyConsumerGovernanceContractTest::readSource(const QString& relativePath) const
{
    QFile sourceFile(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
    if (!sourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTest::qFail(qPrintable(QStringLiteral("failed to read source file: %1").arg(relativePath)), __FILE__, __LINE__);
        return {};
    }
    return QString::fromUtf8(sourceFile.readAll());
}

void PluginLegacyConsumerGovernanceContractTest::main_cpp_remains_forbidden_product_mainline_consumer()
{
    const QString source = readSource(QStringLiteral("main.cpp"));

    QVERIFY2(!source.contains(QStringLiteral("loadPluginPolicy(")),
        "main.cpp still calls loadPluginPolicy() in the product startup mainline");
    QVERIFY2(!source.contains(QStringLiteral("installPluginsFromDirectory(")),
        "main.cpp still calls installPluginsFromDirectory() in the product startup mainline");
}

void PluginLegacyConsumerGovernanceContractTest::legacy_consumer_inventory_classifies_current_consumers()
{
    const QString inventory = readSource(QStringLiteral("docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md"));

    QVERIFY2(inventory.contains(QStringLiteral("`main.cpp` | `forbidden_product_mainline`")),
        "legacy consumer inventory does not classify main.cpp as forbidden_product_mainline");
    QVERIFY2(inventory.contains(QStringLiteral("`config/plugin_load_policy.json` | `allowed_compatibility_surface`")),
        "legacy consumer inventory does not classify plugin_load_policy.json as allowed_compatibility_surface");
    QVERIFY2(inventory.contains(QStringLiteral("`CTKManager::policyForPlugin()` | `temporary_internal_compatibility_debt`")),
        "legacy consumer inventory does not classify CTKManager::policyForPlugin() as temporary_internal_compatibility_debt");
    QVERIFY2(inventory.contains(QStringLiteral("`CTKManager::applyPolicyForPlugin()` | `temporary_internal_compatibility_debt`")),
        "legacy consumer inventory does not classify CTKManager::applyPolicyForPlugin() as temporary_internal_compatibility_debt");
}

void PluginLegacyConsumerGovernanceContractTest::ctk_manager_internal_policy_helpers_are_marked_as_temporary_internal_compatibility_debt()
{
    const QString source = readSource(QStringLiteral("Framework/CTKManager.cpp"));

    QVERIFY2(source.contains(QStringLiteral("temporary_internal_compatibility_debt")),
        "CTKManager.cpp does not mark legacy policy internals as temporary_internal_compatibility_debt");
    QVERIFY2(source.contains(QStringLiteral("Product startup truth remains descriptor-driven")),
        "CTKManager.cpp does not explain that descriptor-driven startup remains the product truth source");
}

QTEST_APPLESS_MAIN(PluginLegacyConsumerGovernanceContractTest)
#include "PluginLegacyConsumerGovernanceContractTest.moc"
```

- [x] **Step 3: 先运行 RED，证明 inventory 和 internal debt 说明还没被完整写实**

Run:

```powershell
cmake --build build_x64 --config Release --target plugin_legacy_consumer_governance_contract_test
ctest --test-dir build_x64 -C Release -R plugin_legacy_consumer_governance_contract_test --output-on-failure
```

Expected:

- `plugin_legacy_consumer_governance_contract_test` FAIL
- 失败点应至少包含以下之一：
  - `failed to read source file: docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md`
  - `CTKManager.cpp does not mark legacy policy internals as temporary_internal_compatibility_debt`

- [x] **Step 4: 补 inventory 文档和 CTKManager internal debt 注释，让契约测试转绿**

```md
<!-- docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md -->
# Platform Plugin Legacy Consumer Inventory

Updated: 2026-04-21

## Current Consumers

| Consumer | Bucket | Status | Notes | Next Step |
| --- | --- | --- | --- | --- |
| `main.cpp` | `forbidden_product_mainline` | enforced | Product startup must remain descriptor-driven and must not call legacy load-policy helpers. | Protected by source contract tests. |
| `config/plugin_load_policy.json` | `allowed_compatibility_surface` | retained | Compatibility-only runtime metadata. | Covered by dedicated compatibility runtime contract. |
| `config/plugin_load_policy_compatibility.md` | `allowed_compatibility_surface` | retained | Sidecar note that explains the compatibility-only boundary. | Covered by dedicated compatibility runtime contract. |
| `CTKManager::loadPluginPolicy()` | `allowed_compatibility_surface` | retained | Compatibility entry for legacy policy metadata loading. | Must not be called by product mainline. |
| `CTKManager::installPluginsFromDirectory()` | `allowed_compatibility_surface` | retained | Compatibility directory-scan helper. | Must not be called by product mainline. |
| `CTKManager::policyForPlugin()` | `temporary_internal_compatibility_debt` | retained | Internal legacy policy lookup still feeds current deferred/on-demand compatibility behavior. | Future cleanup slice should replace it with descriptor/runtime facts. |
| `CTKManager::applyPolicyForPlugin()` | `temporary_internal_compatibility_debt` | retained | Internal legacy policy application still populates deferred/on-demand buckets and safe-mode branching. | Future cleanup slice should replace it with descriptor/runtime facts. |

## Forbidden New Usage

- No new product-mainline code may call `loadPluginPolicy()` or `installPluginsFromDirectory()`.
- No new product-mainline code may read `plugin_load_policy.json` to decide startup content.
- Any newly discovered legacy consumer must be added to this inventory before it can be considered acceptable.
```

```cpp
// Framework/CTKManager.cpp
// temporary_internal_compatibility_debt:
// These helpers still read legacy load-policy metadata to preserve existing
// deferred/on-demand and safe-mode behavior inside CTKManager.
// Product startup truth remains descriptor-driven and must not route through this path.
LoadPolicy CTKManager::policyForPlugin(const QString& pluginName)
{
    PluginLoadPolicy* policyMgr = PluginLoadPolicy::instance();
    if (!m_pluginPolicyPath.isEmpty()) {
        if (policyMgr->configPath() != m_pluginPolicyPath || !policyMgr->hasValidConfig()) {
            policyMgr->loadConfig(m_pluginPolicyPath);
        }
    }

    if (!policyMgr->hasValidConfig()) {
        return LoadPolicy::OnDemand;
    }

    return policyMgr->getLoadPolicy(pluginName);
}
```

- [x] **Step 5: 重新运行源码契约测试，并顺手回归现有 truth-source contract**

Run:

```powershell
cmake --build build_x64 --config Release --target plugin_legacy_consumer_governance_contract_test plugin_truth_source_governance_contract_test
ctest --test-dir build_x64 -C Release -R "plugin_legacy_consumer_governance_contract_test|plugin_truth_source_governance_contract_test" --output-on-failure
```

Expected:

- `plugin_legacy_consumer_governance_contract_test` PASS
- `plugin_truth_source_governance_contract_test` PASS

- [x] **Step 6: 提交 inventory + internal debt 边界锁定批次**

```powershell
git add tests/unit/CMakeLists.txt tests/unit/PluginLegacyConsumerGovernanceContractTest.cpp docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md Framework/CTKManager.cpp
git commit -m "test: lock plugin legacy consumer inventory boundary"
```

### Task 2: 拆分 product runtime artifact 与 compatibility runtime artifact 验收

**Files:**
- Modify: `tests/unit/PluginLegacyConsumerGovernanceContractTest.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/runtime/verify_runtime_artifacts.cmake`
- Modify: `docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md`

- [x] **Step 1: 扩展源码契约测试，先把 runtime acceptance wiring 的目标状态写成 RED**

```cpp
// tests/unit/PluginLegacyConsumerGovernanceContractTest.cpp
private slots:
    void runtime_acceptance_wiring_separates_product_and_compatibility_artifacts();
```

```cpp
void PluginLegacyConsumerGovernanceContractTest::runtime_acceptance_wiring_separates_product_and_compatibility_artifacts()
{
    const QString testsCMake = readSource(QStringLiteral("tests/CMakeLists.txt"));
    const QString runtimeScript = readSource(QStringLiteral("tests/runtime/verify_runtime_artifacts.cmake"));

    QVERIFY2(!testsCMake.contains(QStringLiteral("-Dplugin_policy_file=$<TARGET_FILE_DIR:medicalpro>/config/plugin_load_policy.json")),
        "tests/CMakeLists.txt still wires plugin_policy_file into the default product runtime layout test");
    QVERIFY2(testsCMake.contains(QStringLiteral("NAME plugin_legacy_compatibility_runtime_contract_test")),
        "tests/CMakeLists.txt has not registered plugin_legacy_compatibility_runtime_contract_test");
    QVERIFY2(runtimeScript.contains(QStringLiteral("verify_plugin_legacy_compatibility_runtime_contract")),
        "verify_runtime_artifacts.cmake has no dedicated compatibility runtime contract mode");
}
```

- [x] **Step 2: 先运行 RED，确认默认 runtime 验收与 compatibility artifact 仍然混在一起**

Run:

```powershell
cmake --build build_x64 --config Release --target plugin_legacy_consumer_governance_contract_test
ctest --test-dir build_x64 -C Release -R plugin_legacy_consumer_governance_contract_test --output-on-failure
```

Expected:

- `plugin_legacy_consumer_governance_contract_test` FAIL
- 失败信息应至少包含：
  - `tests/CMakeLists.txt still wires plugin_policy_file into the default product runtime layout test`
  - 或 `tests/CMakeLists.txt has not registered plugin_legacy_compatibility_runtime_contract_test`

- [x] **Step 3: 更新测试注册，让默认 runtime artifact 验收不再夹带 compatibility artifact**

```cmake
# tests/CMakeLists.txt
if(TARGET medicalpro AND TARGET UserManagement AND TARGET DicomViewer AND TARGET FourViewDisplay)
    add_test(
        NAME runtime_artifact_layout_test
        COMMAND ${CMAKE_COMMAND}
            -Druntime_dir=$<TARGET_FILE_DIR:medicalpro>
            -Duser_management_plugin=$<TARGET_FILE:UserManagement>
            -Ddicom_viewer_plugin=$<TARGET_FILE:DicomViewer>
            -Dfour_view_display_plugin=$<TARGET_FILE:FourViewDisplay>
            -Dmeshgpu_runtime_dll=$<TARGET_FILE_DIR:medicalpro>/MeshGPULib.dll
            -Ddata_dir=$<TARGET_FILE_DIR:medicalpro>/data
            -P ${CMAKE_CURRENT_SOURCE_DIR}/runtime/verify_runtime_artifacts.cmake
    )
endif()

if(TARGET medicalpro)
    add_test(
        NAME plugin_legacy_compatibility_runtime_contract_test
        COMMAND ${CMAKE_COMMAND}
            -Druntime_dir=$<TARGET_FILE_DIR:medicalpro>
            -Dverify_plugin_legacy_compatibility_runtime_contract=ON
            -P ${CMAKE_CURRENT_SOURCE_DIR}/runtime/verify_runtime_artifacts.cmake
    )
endif()
```

```md
<!-- docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md -->
| `runtime_artifact_layout_test` | `forbidden_product_mainline` | enforced | Default runtime artifact acceptance must validate product artifacts only. | Must not require `plugin_load_policy.json`. |
| `plugin_legacy_compatibility_runtime_contract_test` | `allowed_compatibility_surface` | retained | Dedicated runtime acceptance for compatibility-only artifacts. | Keep it separate from product runtime layout acceptance. |
```

- [x] **Step 4: 更新 runtime 校验脚本，显式拆开 product / compatibility / truth-source 三种语义**

```cmake
# tests/runtime/verify_runtime_artifacts.cmake
set(required_files
    "${user_management_plugin}"
    "${dicom_viewer_plugin}"
    "${four_view_display_plugin}"
    "${meshgpu_runtime_dll}"
)
```

```cmake
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

```cmake
if(verify_plugin_legacy_compatibility_runtime_contract)
    set(plugin_policy_file "${runtime_dir}/config/plugin_load_policy.json")
    set(plugin_policy_note_file "${runtime_dir}/config/plugin_load_policy_compatibility.md")

    append_missing_artifact("${plugin_policy_file}")
    append_missing_artifact("${plugin_policy_note_file}")

    if(missing_artifacts)
        string(JOIN "\n - " missing_report ${missing_artifacts})
        message(FATAL_ERROR "plugin_legacy_compatibility_runtime_layout_mismatch:\n - ${missing_report}")
    endif()

    file(READ "${plugin_policy_note_file}" plugin_policy_note_text)
    if(NOT plugin_policy_note_text MATCHES "compatibility-only")
        message(FATAL_ERROR "plugin_policy_note_missing_compatibility_only: ${plugin_policy_note_file}")
    endif()

    if(NOT plugin_policy_note_text MATCHES "must not define the product mainline")
        message(FATAL_ERROR "plugin_policy_note_missing_mainline_boundary: ${plugin_policy_note_file}")
    endif()
endif()
```

- [x] **Step 5: 重新运行源码契约测试和 runtime acceptance，确认三条语义边界都转绿**

Run:

```powershell
cmake --build build_x64 --config Release --target medicalpro plugin_legacy_consumer_governance_contract_test
ctest --test-dir build_x64 -C Release -R "plugin_legacy_consumer_governance_contract_test|runtime_artifact_layout_test|plugin_legacy_compatibility_runtime_contract_test|plugin_truth_source_runtime_contract_test|platform_descriptor_runtime_layout_test" --output-on-failure
```

Expected:

- `plugin_legacy_consumer_governance_contract_test` PASS
- `runtime_artifact_layout_test` PASS
- `plugin_legacy_compatibility_runtime_contract_test` PASS
- `plugin_truth_source_runtime_contract_test` PASS
- `platform_descriptor_runtime_layout_test` PASS

- [x] **Step 6: 提交 runtime acceptance separation 批次**

```powershell
git add tests/unit/PluginLegacyConsumerGovernanceContractTest.cpp tests/CMakeLists.txt tests/runtime/verify_runtime_artifacts.cmake docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md
git commit -m "test: separate plugin legacy compatibility runtime contract"
```

### Task 3: 回写 current status、decision log、governance matrix 和计划状态

**Files:**
- Modify: `docs/current_status_and_project_overview.md`
- Modify: `docs/superpowers/tracking/platform-migration-decision-log.md`
- Modify: `docs/superpowers/tracking/platform-plugin-governance-matrix.md`
- Modify: `docs/superpowers/plans/2026-04-21-plugin-legacy-consumer-governance-implementation.md`

- [x] **Step 1: 在 current status 顶部增加 acceptance，并修正 runtime artifact 文案**

```md
<!-- docs/current_status_and_project_overview.md -->
### 2026-04-21 Plugin Legacy Consumer Governance Acceptance

- Default product runtime artifact acceptance no longer requires `config/plugin_load_policy.json`.
- Compatibility runtime artifact acceptance is now owned by `plugin_legacy_compatibility_runtime_contract_test`.
- The remaining legacy-policy consumers are now inventoried in `docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md`.
- `CTKManager::policyForPlugin()` and `CTKManager::applyPolicyForPlugin()` are now documented as `temporary_internal_compatibility_debt`, not product-mainline truth.
- Executed command (build):
  - `cmake --build build_x64 --config Release --target medicalpro plugin_legacy_consumer_governance_contract_test`
- Executed command (ctest):
  - `ctest --test-dir build_x64 -C Release -R "plugin_legacy_consumer_governance_contract_test|runtime_artifact_layout_test|plugin_legacy_compatibility_runtime_contract_test|plugin_truth_source_runtime_contract_test|platform_descriptor_runtime_layout_test" --output-on-failure`
- Expected outcomes alignment and actual results:
  - Runtime artifact boundary suite: PASS.
  - Product/compatibility acceptance split: PASS.
```

```md
<!-- replace the stale wording in the Plugin Truth Source Governance Acceptance section -->
- Runtime acceptance now includes `plugin_truth_source_governance_contract_test`, `plugin_truth_source_runtime_contract_test`, and the separated compatibility check `plugin_legacy_compatibility_runtime_contract_test`.
```

- [x] **Step 2: 给 decision log 和 governance matrix 写正式 consumer-boundary 说明**

```md
<!-- docs/superpowers/tracking/platform-migration-decision-log.md -->
## 2026-04-21

- Decision: split default product runtime artifact acceptance from compatibility runtime artifact acceptance.
- Rationale: `plugin_load_policy.json` is still a shipped compatibility artifact, but default product runtime verification must not keep implying that it is a universal runtime requirement.
- Impact: `runtime_artifact_layout_test` now verifies product-mainline runtime artifacts only, while `plugin_legacy_compatibility_runtime_contract_test` owns legacy policy artifact shipping.

- Decision: classify `CTKManager::policyForPlugin()` and `CTKManager::applyPolicyForPlugin()` as temporary internal compatibility debt.
- Rationale: those helpers still preserve current internal deferred/on-demand behavior, but they are not product-mainline truth and should not be treated as approved long-term architecture.
- Impact: the remaining legacy consumer set is now documented and test-protected without changing runtime behavior in this slice.
```

```md
<!-- docs/superpowers/tracking/platform-plugin-governance-matrix.md -->
## Current Implementation Notes

- `runtime_artifact_layout_test` now covers product-mainline runtime artifacts only.
- `plugin_legacy_compatibility_runtime_contract_test` owns `plugin_load_policy.json` and `plugin_load_policy_compatibility.md` shipping verification.
- `CTKManager::policyForPlugin()` and `CTKManager::applyPolicyForPlugin()` remain `temporary_internal_compatibility_debt` until a later descriptor-driven cleanup slice replaces them.
- The authoritative human-readable inventory for remaining legacy consumers is `docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md`.
```

- [x] **Step 3: 在本计划文件底部回写完成状态模板**

```md
## Status Update 2026-04-21

- Completed. Remaining legacy-policy consumers are now explicitly inventoried and classified.
- Default product runtime artifact acceptance and compatibility runtime artifact acceptance are now separated.
- `CTKManager` internal legacy policy usage is now documented as temporary internal compatibility debt rather than implicit product truth.
- Source-contract, product-runtime, compatibility-runtime, and truth-source-runtime acceptance now protect the consumer-boundary model from drifting.
```

- [x] **Step 4: 运行整批 acceptance 命令**

Run:

```powershell
cmake --build build_x64 --config Release --target medicalpro plugin_legacy_consumer_governance_contract_test plugin_truth_source_governance_contract_test
ctest --test-dir build_x64 -C Release -R "plugin_legacy_consumer_governance_contract_test|plugin_truth_source_governance_contract_test|runtime_artifact_layout_test|plugin_legacy_compatibility_runtime_contract_test|plugin_truth_source_runtime_contract_test|platform_descriptor_runtime_layout_test" --output-on-failure
rg -n "loadPluginPolicy\\(|installPluginsFromDirectory\\(" main.cpp
rg -n "temporary_internal_compatibility_debt|plugin_legacy_compatibility_runtime_contract_test|platform-plugin-legacy-consumer-inventory" Framework/CTKManager.cpp docs/current_status_and_project_overview.md docs/superpowers/tracking/platform-migration-decision-log.md docs/superpowers/tracking/platform-plugin-governance-matrix.md docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md
```

Expected:

- `medicalpro` build target PASS
- `plugin_legacy_consumer_governance_contract_test` PASS
- `plugin_truth_source_governance_contract_test` PASS
- `runtime_artifact_layout_test` PASS
- `plugin_legacy_compatibility_runtime_contract_test` PASS
- `plugin_truth_source_runtime_contract_test` PASS
- `platform_descriptor_runtime_layout_test` PASS
- `rg -n "loadPluginPolicy\\(|installPluginsFromDirectory\\(" main.cpp` 无输出
- 第二条 `rg` 命令能在预期文件中搜到 `temporary_internal_compatibility_debt`、`plugin_legacy_compatibility_runtime_contract_test` 和 inventory 文案

- [x] **Step 5: 提交文档回写和 acceptance 状态**

```powershell
git add docs/current_status_and_project_overview.md docs/superpowers/tracking/platform-migration-decision-log.md docs/superpowers/tracking/platform-plugin-governance-matrix.md docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md docs/superpowers/plans/2026-04-21-plugin-legacy-consumer-governance-implementation.md
git commit -m "docs: record plugin legacy consumer governance acceptance"
```

## Self-Review

- Spec coverage:
  - legacy consumer inventory：Task 1 覆盖
  - product/compatibility runtime acceptance separation：Task 2 覆盖
  - internal legacy debt 明示化：Task 1 / Task 3 覆盖
  - current status / decision log / governance matrix 回写：Task 3 覆盖
- Placeholder scan:
  - 未出现占位词或“以后再补”的延后实现表述
  - 每个代码步骤都给了具体代码块
  - 每个验收步骤都给了明确命令和预期输出
- Type consistency:
  - inventory bucket 统一为 `forbidden_product_mainline`、`allowed_compatibility_surface`、`temporary_internal_compatibility_debt`
  - runtime 测试名统一为 `runtime_artifact_layout_test`、`plugin_legacy_compatibility_runtime_contract_test`、`plugin_truth_source_runtime_contract_test`
  - inventory 文档路径统一为 `docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md`

## Status Update 2026-04-21

- Completed Task 1 with commit `6b6f7c5` (`test: lock plugin legacy consumer inventory boundary`).
- Completed Task 2 with commit `4aebe50` (`test: separate plugin legacy compatibility runtime contract`).
- Completed Task 3 by aligning current status, decision log, governance matrix, and this implementation plan to the consumer-boundary model.
- Final acceptance passed with:
  - `cmake --build build_x64 --config Release --target medicalpro plugin_legacy_consumer_governance_contract_test plugin_truth_source_governance_contract_test`
  - `ctest --test-dir build_x64 -C Release -R "^(plugin_legacy_consumer_governance_contract_test|plugin_truth_source_governance_contract_test|runtime_artifact_layout_test|plugin_legacy_compatibility_runtime_contract_test|plugin_truth_source_runtime_contract_test|platform_descriptor_runtime_layout_test)$" --output-on-failure`
  - `rg -n "loadPluginPolicy\\(|installPluginsFromDirectory\\(" main.cpp` produced no output.
