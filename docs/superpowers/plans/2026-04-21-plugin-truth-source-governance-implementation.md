# Plugin Truth Source Governance Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `medicalpro` 的产品级插件真相源正式锁定为 `config/platform_runtime.json + plugins/descriptors/*.json + PlatformDescriptorLoader`，并把 `plugin_load_policy.json + CTKManager::loadPluginPolicy()/installPluginsFromDirectory()` 明确降级为 compatibility-only。

**Architecture:** 这批不改运行主链语义，只做“边界锁定”。先用源码契约测试和运行时契约测试把主链与兼容链的分界线锁住，再在 `CTKManager` / `PluginLoadPolicy` / `config` 侧补齐 compatibility-only 注释与说明，最后统一回写当前状态、决策日志和治理矩阵，确保代码、测试、文档三者描述同一套事实。

**Tech Stack:** CMake, CTest, Qt 6, QtTest, existing runtime verification scripts under `tests/runtime`, current platform governance code under `Framework/Platform` and `Framework/CTKManager`

---

## Files And Responsibilities

- Modify: `tests/unit/CMakeLists.txt`
  - 注册新的源码契约测试 `plugin_truth_source_governance_contract_test`
- Create: `tests/unit/PluginTruthSourceGovernanceContractTest.cpp`
  - 锁定 `main.cpp` 只走 `platform_runtime.json + PlatformDescriptorLoader`
  - 锁定 `main.cpp` 不调用 `loadPluginPolicy()` / `installPluginsFromDirectory()`
  - 锁定遗留兼容 API 在头文件里被明确标记为 compatibility-only
- Modify: `Framework/CTKManager.h`
  - 给 `loadPluginPolicy()` / `installPluginsFromDirectory()` 增加 compatibility-only 注释，防止误当主链 API
- Modify: `Framework/CTKManager.cpp`
  - 给遗留兼容入口补充明确的 compatibility-only 运行日志
- Modify: `Framework/PluginLoadPolicy.h`
  - 把 `PluginLoadPolicy` 类型注释明确为 legacy compatibility metadata
- Modify: `Framework/PluginLoadPolicy.cpp`
  - 把配置加载日志改成 compatibility-only 语义
- Modify: `tests/CMakeLists.txt`
  - 注册运行时契约测试 `plugin_truth_source_runtime_contract_test`
- Modify: `tests/runtime/verify_runtime_artifacts.cmake`
  - 增加“真相源运行时布局”校验：`platform_runtime.json`、descriptor 目录、兼容说明文件同时存在且语义正确
- Create: `config/plugin_load_policy_compatibility.md`
  - 作为 `plugin_load_policy.json` 的同目录说明文件，明确它只是 compatibility-only 元数据
- Modify: `docs/current_status_and_project_overview.md`
  - 新增这批 acceptance 记录
  - 修正旧状态段里把 `plugin_load_policy.json` 误写成现实主链的漂移文案
- Modify: `docs/superpowers/tracking/platform-migration-decision-log.md`
  - 写入“legacy policy chain 降级为 compatibility-only”的正式决策
- Modify: `docs/superpowers/tracking/platform-plugin-governance-matrix.md`
  - 在治理矩阵里补齐“主链真相源 / compatibility-only”边界说明
- Modify: `docs/superpowers/plans/2026-04-21-plugin-truth-source-governance-implementation.md`
  - 实施完成后回写状态

### Task 1: 用源码契约测试锁定主链真相源边界

**Files:**
- Modify: `tests/unit/CMakeLists.txt`
- Create: `tests/unit/PluginTruthSourceGovernanceContractTest.cpp`
- Modify: `Framework/CTKManager.h`
- Modify: `Framework/CTKManager.cpp`
- Modify: `Framework/PluginLoadPolicy.h`
- Modify: `Framework/PluginLoadPolicy.cpp`

- [x] **Step 1: 先注册新的源码契约测试 target**

```cmake
# tests/unit/CMakeLists.txt
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
```

- [x] **Step 2: 写 RED 契约测试，先把边界锁死**

```cpp
// tests/unit/PluginTruthSourceGovernanceContractTest.cpp
#include <QtTest/QtTest>

#include <QFile>
#include <QString>

class PluginTruthSourceGovernanceContractTest : public QObject
{
    Q_OBJECT

private slots:
    void main_cpp_uses_runtime_config_and_descriptor_loader_for_product_mainline();
    void main_cpp_does_not_call_legacy_policy_helpers_for_product_mainline();
    void legacy_policy_surface_is_marked_as_compatibility_only();

private:
    QString readSource(const QString& relativePath) const;
};

QString PluginTruthSourceGovernanceContractTest::readSource(const QString& relativePath) const
{
    QFile sourceFile(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
    if (!sourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTest::qFail(qPrintable(QStringLiteral("无法读取源文件: %1").arg(relativePath)), __FILE__, __LINE__);
        return {};
    }
    return QString::fromUtf8(sourceFile.readAll());
}

void PluginTruthSourceGovernanceContractTest::main_cpp_uses_runtime_config_and_descriptor_loader_for_product_mainline()
{
    const QString source = readSource(QStringLiteral("main.cpp"));

    QVERIFY2(source.contains(QStringLiteral("config/platform_runtime.json")),
        "main.cpp 没有从 platform_runtime.json 读取产品级运行时配置");
    QVERIFY2(source.contains(QStringLiteral("PlatformDescriptorLoader::loadFromDirectory")),
        "main.cpp 没有通过 PlatformDescriptorLoader 加载 descriptor 目录");
    QVERIFY2(source.contains(QStringLiteral("PlatformManagedPluginPlanBuilder::build")),
        "main.cpp 没有从 descriptor 事实构建 managed startup plan");
}

void PluginTruthSourceGovernanceContractTest::main_cpp_does_not_call_legacy_policy_helpers_for_product_mainline()
{
    const QString source = readSource(QStringLiteral("main.cpp"));

    QVERIFY2(!source.contains(QStringLiteral("loadPluginPolicy(")),
        "main.cpp 仍在产品主链中调用 loadPluginPolicy()");
    QVERIFY2(!source.contains(QStringLiteral("installPluginsFromDirectory(")),
        "main.cpp 仍在产品主链中调用 installPluginsFromDirectory()");
}

void PluginTruthSourceGovernanceContractTest::legacy_policy_surface_is_marked_as_compatibility_only()
{
    const QString ctkManagerHeader = readSource(QStringLiteral("Framework/CTKManager.h"));
    const QString pluginLoadPolicyHeader = readSource(QStringLiteral("Framework/PluginLoadPolicy.h"));
    const QString pluginLoadPolicySource = readSource(QStringLiteral("Framework/PluginLoadPolicy.cpp"));

    QVERIFY2(ctkManagerHeader.contains(QStringLiteral("compatibility-only")),
        "CTKManager.h 尚未把遗留 load-policy API 标记为 compatibility-only");
    QVERIFY2(pluginLoadPolicyHeader.contains(QStringLiteral("compatibility-only")),
        "PluginLoadPolicy.h 尚未把该类型标记为 compatibility-only");
    QVERIFY2(pluginLoadPolicySource.contains(QStringLiteral("compatibility-only")),
        "PluginLoadPolicy.cpp 尚未输出 compatibility-only 语义日志");
}

QTEST_APPLESS_MAIN(PluginTruthSourceGovernanceContractTest)
#include "PluginTruthSourceGovernanceContractTest.moc"
```

- [x] **Step 3: 先运行 RED，用失败结果证明这批边界还没被完整锁住**

Run:

```powershell
cmake --build build_x64 --config Release --target plugin_truth_source_governance_contract_test
ctest --test-dir build_x64 -C Release -R plugin_truth_source_governance_contract_test --output-on-failure
```

Expected:

- `plugin_truth_source_governance_contract_test` FAIL
- 失败点应至少包含一条 compatibility-only 标记缺失：
  - `CTKManager.h 尚未把遗留 load-policy API 标记为 compatibility-only`
  - 或 `PluginLoadPolicy.h 尚未把该类型标记为 compatibility-only`
  - 或 `PluginLoadPolicy.cpp 尚未输出 compatibility-only 语义日志`

- [x] **Step 4: 给遗留兼容面补齐最小注释和日志，让契约测试转绿**

```cpp
// Framework/CTKManager.h
    /**
     * @brief Compatibility-only helper that bulk-installs plugins from a directory.
     * Product startup mainline must use platform_runtime.json + descriptors instead.
     */
    int installPluginsFromDirectory(const QString& pluginDir);

    /**
     * @brief Compatibility-only helper that loads legacy plugin policy metadata.
     * Product startup mainline must not use this API as a truth source.
     */
    void loadPluginPolicy(const QString& configPath);
```

```cpp
// Framework/PluginLoadPolicy.h
/**
 * @brief Compatibility-only legacy plugin policy store.
 *
 * Product startup truth comes from platform_runtime.json + plugins/descriptors/*.json.
 * This type only preserves the old CTK load-policy metadata for compatibility paths.
 */
class FRAMEWORK_EXPORT PluginLoadPolicy : public QObject, public SingletonManager<PluginLoadPolicy>
```

```cpp
// Framework/CTKManager.cpp
int CTKManager::installPluginsFromDirectory(const QString& pluginDir)
{
    if (!m_started) {
        LOG_ERROR("CTKManager", "CTK Framework not started - cannot install plugins");
        return 0;
    }

    LOG_INFO("CTKManager",
        QString("Installing plugins from compatibility-only directory scan: %1").arg(pluginDir));

#ifdef CTK_PLUGIN_FRAMEWORK
    // existing implementation unchanged
```

```cpp
// Framework/CTKManager.cpp
void CTKManager::loadPluginPolicy(const QString& configPath)
{
    m_pluginPolicyPath = configPath;
    if (configPath.isEmpty()) {
        return;
    }

    LOG_INFO("CTKManager",
        QString("Loading compatibility-only plugin policy metadata from %1").arg(configPath));
    PluginLoadPolicy::instance()->loadConfig(configPath);
}
```

```cpp
// Framework/PluginLoadPolicy.cpp
void PluginLoadPolicy::loadConfig(const QString& configFilePath)
{
    QFile file(configFilePath);
    if (!file.exists()) {
        LOG_WARNING("PluginLoadPolicy", QString("Compatibility-only config file not found: %1").arg(configFilePath));
        QMutexLocker locker(&m_mutex);
        clearPolicies();
        m_configPath.clear();
        m_hasValidConfig = false;
        return;
    }

    // existing parse logic unchanged

    LOG_INFO("PluginLoadPolicy",
        QString("Loaded compatibility-only plugin policy configuration from %1").arg(configFilePath));
}
```

- [x] **Step 5: 重新运行源码契约测试，并顺手回归 descriptor loader 现有测试**

Run:

```powershell
cmake --build build_x64 --config Release --target plugin_truth_source_governance_contract_test platform_descriptor_loader_test
ctest --test-dir build_x64 -C Release -R "plugin_truth_source_governance_contract_test|platform_descriptor_loader_test" --output-on-failure
```

Expected:

- `plugin_truth_source_governance_contract_test` PASS
- `platform_descriptor_loader_test` PASS

- [x] **Step 6: 提交源码边界锁定批次**

```powershell
git add tests/unit/CMakeLists.txt tests/unit/PluginTruthSourceGovernanceContractTest.cpp Framework/CTKManager.h Framework/CTKManager.cpp Framework/PluginLoadPolicy.h Framework/PluginLoadPolicy.cpp
git commit -m "test: lock plugin truth source governance boundary"
```

### Task 2: 增加运行时契约测试并给 legacy policy 文件补 sidecar 说明

**Files:**
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/runtime/verify_runtime_artifacts.cmake`
- Create: `config/plugin_load_policy_compatibility.md`

- [x] **Step 1: 注册新的运行时契约测试 `plugin_truth_source_runtime_contract_test`**

```cmake
# tests/CMakeLists.txt
if(TARGET medicalpro)
    add_test(
        NAME plugin_truth_source_runtime_contract_test
        COMMAND ${CMAKE_COMMAND}
            -Druntime_dir=$<TARGET_FILE_DIR:medicalpro>
            -Dverify_plugin_truth_source_runtime_contract=ON
            -P ${CMAKE_CURRENT_SOURCE_DIR}/runtime/verify_runtime_artifacts.cmake
    )
endif()
```

- [x] **Step 2: 扩展运行时校验脚本，把“主链真相源 + compatibility sidecar”都纳入验收**

```cmake
# tests/runtime/verify_runtime_artifacts.cmake
if(verify_plugin_truth_source_runtime_contract)
    set(platform_runtime_file "${runtime_dir}/config/platform_runtime.json")
    set(plugin_policy_file "${runtime_dir}/config/plugin_load_policy.json")
    set(plugin_policy_note_file "${runtime_dir}/config/plugin_load_policy_compatibility.md")

    append_missing_artifact("${platform_runtime_file}")
    append_missing_artifact("${plugin_policy_file}")
    append_missing_artifact("${plugin_policy_note_file}")

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

    file(READ "${plugin_policy_note_file}" plugin_policy_note_text)
    if(NOT plugin_policy_note_text MATCHES "compatibility-only")
        message(FATAL_ERROR "plugin_policy_note_missing_compatibility_only: ${plugin_policy_note_file}")
    endif()

    if(NOT plugin_policy_note_text MATCHES "platform_runtime.json")
        message(FATAL_ERROR "plugin_policy_note_missing_runtime_truth_reference: ${plugin_policy_note_file}")
    endif()
endif()
```

- [x] **Step 3: 先运行 RED，确认当前 runtime layout 还缺 sidecar 说明**

Run:

```powershell
cmake --build build_x64 --config Release --target medicalpro
ctest --test-dir build_x64 -C Release -R plugin_truth_source_runtime_contract_test --output-on-failure
```

Expected:

- `plugin_truth_source_runtime_contract_test` FAIL
- 失败信息应为 `plugin_truth_source_runtime_layout_mismatch`
- 缺失项中应包含 `build_x64/Release/config/plugin_load_policy_compatibility.md`

- [x] **Step 4: 增加 sidecar 说明文件，把 legacy policy 的 compatibility-only 身份写死**

```md
<!-- config/plugin_load_policy_compatibility.md -->
# plugin_load_policy compatibility note

- `config/plugin_load_policy.json` is a compatibility-only runtime artifact.
- Product startup truth comes from `config/platform_runtime.json` and `plugins/descriptors/*.json`.
- `CTKManager::loadPluginPolicy()` and `CTKManager::installPluginsFromDirectory()` are retained only for legacy compatibility paths and must not define the product mainline.
```

- [x] **Step 5: 重新运行运行时契约测试，并顺手回归 runtime descriptor layout**

Run:

```powershell
cmake --build build_x64 --config Release --target medicalpro
ctest --test-dir build_x64 -C Release -R "plugin_truth_source_runtime_contract_test|platform_descriptor_runtime_layout_test" --output-on-failure
```

Expected:

- `plugin_truth_source_runtime_contract_test` PASS
- `platform_descriptor_runtime_layout_test` PASS

- [x] **Step 6: 提交 runtime contract + sidecar 批次**

```powershell
git add tests/CMakeLists.txt tests/runtime/verify_runtime_artifacts.cmake config/plugin_load_policy_compatibility.md
git commit -m "test: add plugin truth source runtime contract"
```

### Task 3: 回写当前状态、决策日志和治理矩阵，纠正旧漂移文案

**Files:**
- Modify: `docs/current_status_and_project_overview.md`
- Modify: `docs/superpowers/tracking/platform-migration-decision-log.md`
- Modify: `docs/superpowers/tracking/platform-plugin-governance-matrix.md`
- Modify: `docs/superpowers/plans/2026-04-21-plugin-truth-source-governance-implementation.md`

- [x] **Step 1: 在 current status 顶部增加这批 acceptance，并修正旧段落里的误导文案**

```md
<!-- docs/current_status_and_project_overview.md -->
### 2026-04-21 Plugin Truth Source Governance Acceptance

- Product startup truth is explicitly `config/platform_runtime.json + plugins/descriptors/*.json + PlatformDescriptorLoader`.
- `config/plugin_load_policy.json` continues to ship only as compatibility-only metadata for legacy CTK helper paths.
- `main.cpp` does not call `CTKManager::loadPluginPolicy()` or `CTKManager::installPluginsFromDirectory()` in the product startup path.
- Runtime acceptance now includes `plugin_truth_source_governance_contract_test` and `plugin_truth_source_runtime_contract_test`.
```

```md
<!-- replace the stale bullet in section 1.4 with the following two lines -->
- `config/plugin_load_policy.json` 现仅保留为兼容层策略元数据，不再作为产品启动主链真相源。
- `RegistrationCore` 与 `OpticalTracking` 的产品级识别和治理均由 `platform_runtime.json + plugins/descriptors/*.json` 驱动。
```

- [x] **Step 2: 给 decision log 和 governance matrix 写正式边界说明**

```md
<!-- docs/superpowers/tracking/platform-migration-decision-log.md -->
## 2026-04-21

- Decision: treat `plugin_load_policy.json` and CTK load-policy helpers as compatibility-only metadata rather than product startup truth.
- Rationale: the product mainline already boots from `platform_runtime.json` plus descriptors, so leaving the old policy chain unnamed creates a false second truth source.
- Impact: future mainline code must not call `loadPluginPolicy()` or `installPluginsFromDirectory()` to decide startup content, while the legacy helpers remain available for compatibility scenarios.
```

```md
<!-- docs/superpowers/tracking/platform-plugin-governance-matrix.md -->
## Current Implementation Notes

- `config/platform_runtime.json` stores platform descriptor ids, not CTK symbolic names.
- Product startup truth is explicitly `platform_runtime.json + plugins/descriptors/*.json + PlatformDescriptorLoader`.
- `plugin_load_policy.json` and `PluginLoadPolicy` are compatibility-only metadata for legacy CTK helper paths.
- `CTKManager::loadPluginPolicy()` and `CTKManager::installPluginsFromDirectory()` remain available but are not part of `main.cpp` product assembly.
```

- [x] **Step 3: 在本计划文件底部回写完成状态模板**

```md
## Status Update 2026-04-21

- Completed. Product startup truth is now explicitly locked to `platform_runtime.json + descriptors + PlatformDescriptorLoader`.
- `plugin_load_policy.json` now ships with a sidecar compatibility note, and legacy CTK load-policy helpers are documented as compatibility-only.
- Source-contract and runtime-contract acceptance now protect the boundary from drifting back to a multi-truth startup model.
```

- [x] **Step 4: 运行整批 acceptance 命令**

Run:

```powershell
cmake --build build_x64 --config Release --target medicalpro plugin_truth_source_governance_contract_test platform_descriptor_loader_test
ctest --test-dir build_x64 -C Release -R "plugin_truth_source_governance_contract_test|plugin_truth_source_runtime_contract_test|platform_descriptor_loader_test|platform_descriptor_runtime_layout_test" --output-on-failure
rg -n "loadPluginPolicy\\(|installPluginsFromDirectory\\(" main.cpp
rg -n "compatibility-only|platform_runtime.json|PlatformDescriptorLoader" Framework/CTKManager.h Framework/PluginLoadPolicy.h Framework/PluginLoadPolicy.cpp config/plugin_load_policy_compatibility.md docs/current_status_and_project_overview.md docs/superpowers/tracking/platform-migration-decision-log.md docs/superpowers/tracking/platform-plugin-governance-matrix.md
```

Expected:

- `medicalpro` build target PASS
- `plugin_truth_source_governance_contract_test` PASS
- `plugin_truth_source_runtime_contract_test` PASS
- `platform_descriptor_loader_test` PASS
- `platform_descriptor_runtime_layout_test` PASS
- `rg -n "loadPluginPolicy\\(|installPluginsFromDirectory\\(" main.cpp` 无输出
- 第二条 `rg` 命令能在预期文件中搜到 `compatibility-only` / `platform_runtime.json` / `PlatformDescriptorLoader` 边界说明

- [x] **Step 5: 提交文档回写和 acceptance 状态**

```powershell
git add docs/current_status_and_project_overview.md docs/superpowers/tracking/platform-migration-decision-log.md docs/superpowers/tracking/platform-plugin-governance-matrix.md docs/superpowers/plans/2026-04-21-plugin-truth-source-governance-implementation.md
git commit -m "docs: record plugin truth source governance acceptance"
```

## Self-Review

- Spec coverage:
  - 单一产品真相源定义：Task 1 / Task 2 / Task 3 覆盖
  - `plugin_load_policy.json` compatibility-only 降级：Task 1 / Task 2 / Task 3 覆盖
  - 主链不得调用 `loadPluginPolicy()` / `installPluginsFromDirectory()`：Task 1 / Task 3 覆盖
  - 运行时布局与 descriptor 主链事实：Task 2 覆盖
  - current status / decision log / governance matrix 一致回写：Task 3 覆盖
- Placeholder scan:
  - 未出现占位词或“以后再补”的延后实现表述
  - 每个代码步骤都给了具体代码块
  - 每个验收步骤都给了明确命令和预期输出
- Type consistency:
  - 测试名统一为 `plugin_truth_source_governance_contract_test` 与 `plugin_truth_source_runtime_contract_test`
  - 说明文件名统一为 `config/plugin_load_policy_compatibility.md`
  - 文档中的主链术语统一为 `platform_runtime.json + plugins/descriptors/*.json + PlatformDescriptorLoader`

## Status Update 2026-04-21

- Completed Task 1 with commit `4a2e9b0` (`test: lock plugin truth source governance boundary`).
- Completed Task 2 with commit `0ac23d7` (`test: add plugin truth source runtime contract`).
- Completed Task 3 by aligning current status, decision log, governance matrix, and this implementation plan to the single truth-source boundary.
- Final acceptance passed with:
  - `cmake --build build_x64 --config Release --target medicalpro plugin_truth_source_governance_contract_test platform_descriptor_loader_test`
  - `ctest --test-dir build_x64 -C Release -R "plugin_truth_source_governance_contract_test|plugin_truth_source_runtime_contract_test|platform_descriptor_loader_test|platform_descriptor_runtime_layout_test" --output-on-failure`
  - `rg -n "loadPluginPolicy\\(|installPluginsFromDirectory\\(" main.cpp` produced no output.
