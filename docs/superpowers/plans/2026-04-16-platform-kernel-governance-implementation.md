# Platform Kernel Governance Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 `medicalpro` 中完成第一阶段“平台内核治理”落地：建立平台描述文件、依赖图、观察层诊断、三大门面、兼容适配层和最小启动集编排，让核心页面不再直接依赖 CTK 服务，且启动慢问题可以被测量、定位和解释。

**Architecture:** 保留现有 `CTKManager + StartupOrchestrator + NewPagesLib + MainInterfaceWidget` 主体结构，在 `Framework/Platform` 下新增平台治理层，并按 `observe_only -> facade_mode -> orchestrate_core` 三段式推进。先把平台真相、运行时状态和诊断快照建立起来，再用门面与适配器收口 UI/业务流程，最后才切换核心启动链和按需启动逻辑。

**Tech Stack:** Qt 6 / QtTest、Qt Widgets、CTK Plugin Framework、CMake、QJsonDocument、现有 `Framework` 共享库、现有 `tests/unit` / `tests/runtime` 基础设施

---

## Files and Responsibilities

- Modify: `CMakeLists.txt`
  - 把 `Framework/Platform` 新源文件编进 `Framework`，并保证平台相关测试目标可构建。
- Modify: `cmake/PluginMacros.cmake`
  - 为核心插件的 `platform/plugin.json` 增加部署逻辑，把平台描述文件复制到运行时目录。
- Create: `config/platform_runtime.json`
  - 声明平台运行模式、描述文件目录和首轮最小启动集。
- Create: `Framework/Platform/Contracts/PlatformRuntimeTypes.h`
- Create: `Framework/Platform/Contracts/PlatformPluginDescriptor.h`
- Create: `Framework/Platform/Contracts/PlatformSnapshots.h`
- Create: `Framework/Platform/Contracts/PlatformFacadePorts.h`
  - 平台状态枚举、descriptor、快照模型和门面端口。
- Create: `Framework/Platform/Kernel/PlatformDescriptorLoader.h`
- Create: `Framework/Platform/Kernel/PlatformDescriptorLoader.cpp`
- Create: `Framework/Platform/Kernel/PlatformDependencyGraph.h`
- Create: `Framework/Platform/Kernel/PlatformDependencyGraph.cpp`
- Create: `Framework/Platform/Kernel/PlatformStateStore.h`
- Create: `Framework/Platform/Kernel/PlatformStateStore.cpp`
- Create: `Framework/Platform/Kernel/PlatformRuntimeConfig.h`
- Create: `Framework/Platform/Kernel/PlatformRuntimeConfig.cpp`
- Create: `Framework/Platform/Kernel/PlatformStartupCoordinator.h`
- Create: `Framework/Platform/Kernel/PlatformStartupCoordinator.cpp`
  - 平台内核的解析、依赖图、状态存储、运行模式和启动协调。
- Create: `Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.h`
- Create: `Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.cpp`
- Create: `Framework/Platform/Diagnostics/PlatformDiagnosticsService.h`
- Create: `Framework/Platform/Diagnostics/PlatformDiagnosticsService.cpp`
  - 观察层桥接和诊断输出。
- Create: `Framework/Platform/LegacyAdapters/LegacyUserManagementAdapter.h`
- Create: `Framework/Platform/LegacyAdapters/LegacyUserManagementAdapter.cpp`
- Create: `Framework/Platform/LegacyAdapters/LegacyImagingAdapter.h`
- Create: `Framework/Platform/LegacyAdapters/LegacyImagingAdapter.cpp`
- Create: `Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.h`
- Create: `Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.cpp`
- Create: `Framework/Platform/Facades/IdentityAppService.h`
- Create: `Framework/Platform/Facades/IdentityAppService.cpp`
- Create: `Framework/Platform/Facades/ImagingAppService.h`
- Create: `Framework/Platform/Facades/ImagingAppService.cpp`
- Create: `Framework/Platform/Facades/NavigationAppService.h`
- Create: `Framework/Platform/Facades/NavigationAppService.cpp`
  - 三大门面和兼容层适配器。
- Modify: `Framework/StartupOrchestrator.h`
- Modify: `Framework/StartupOrchestrator.cpp`
- Modify: `Framework/CTKManager.h`
- Modify: `Framework/CTKManager.cpp`
- Modify: `main.cpp`
  - 把旧启动链暴露成平台可消费的结构化状态，并在后续切到平台协调器。
- Create: `Plugins/UserManagement/platform/plugin.json`
- Create: `Plugins/DicomViewer/platform/plugin.json`
- Create: `Plugins/FourViewDisplay/platform/plugin.json`
- Create: `Plugins/RegistrationCore/platform/plugin.json`
- Create: `Plugins/OpticalTracking/platform/plugin.json`
  - 5 个核心插件平台描述文件。
- Create: `UI/Forms/PlatformDiagnosticsPage.ui`
- Create: `UI/NewPages/PlatformDiagnosticsPage.h`
- Create: `UI/NewPages/PlatformDiagnosticsPage.cpp`
- Modify: `UI/NewPages/CMakeLists.txt`
- Modify: `UI/NewPages/PageIndex.h`
- Modify: `UI/MainInterfaceWidget.h`
- Modify: `UI/MainInterfaceWidget.cpp`
- Modify: `UI/NewPages/WelcomePage.h`
- Modify: `UI/NewPages/WelcomePage.cpp`
- Modify: `UI/NewPages/ModuleSelectionPage.h`
- Modify: `UI/NewPages/ModuleSelectionPage.cpp`
- Modify: `UI/NewPages/SystemSettingsPage.h`
- Modify: `UI/NewPages/SystemSettingsPage.cpp`
- Modify: `UI/NewPages/ManagementPage.h`
- Modify: `UI/NewPages/ManagementPage.cpp`
- Modify: `UI/NewPages/DashboardPage.h`
- Modify: `UI/NewPages/DashboardPage.cpp`
  - 诊断页接入和核心页面门面迁移。
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/runtime/verify_runtime_artifacts.cmake`
- Modify: `tests/unit/CMakeLists.txt`
- Create: `tests/unit/PlatformDescriptorLoaderTest.cpp`
- Create: `tests/unit/PlatformDependencyGraphTest.cpp`
- Create: `tests/unit/PlatformDiagnosticsServiceTest.cpp`
- Create: `tests/unit/PlatformFacadesTest.cpp`
- Create: `tests/unit/PlatformDiagnosticsPageTest.cpp`
- Create: `tests/unit/PlatformStartupCoordinatorTest.cpp`
  - 平台测试和运行时布局回归。
- Create: `docs/superpowers/tracking/platform-plugin-governance-matrix.md`
- Create: `docs/superpowers/tracking/platform-migration-decision-log.md`
- Modify: `docs/current_status_and_project_overview.md`
  - 治理清单、迁移决策和状态回写。

---

### Task 1: 建立平台 descriptor 解析和基础契约

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `tests/unit/CMakeLists.txt`
- Create: `Framework/Platform/Contracts/PlatformRuntimeTypes.h`
- Create: `Framework/Platform/Contracts/PlatformPluginDescriptor.h`
- Create: `Framework/Platform/Kernel/PlatformDescriptorLoader.h`
- Create: `Framework/Platform/Kernel/PlatformDescriptorLoader.cpp`
- Create: `tests/unit/PlatformDescriptorLoaderTest.cpp`

- [ ] **Step 1: 先写 descriptor loader 的失败测试**

```cpp
// tests/unit/PlatformDescriptorLoaderTest.cpp
#include <QtTest/QtTest>
#include <QFile>
#include <QTemporaryDir>

#include "Framework/Platform/Kernel/PlatformDescriptorLoader.h"

class PlatformDescriptorLoaderTest : public QObject
{
    Q_OBJECT

private slots:
    void loadFromFile_reads_required_fields();
    void loadFromFile_rejects_missing_startup_policy();
};

void PlatformDescriptorLoaderTest::loadFromFile_reads_required_fields()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile file(dir.filePath("plugin.json"));
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
      "provides": {"services": ["imaging.study_query"], "capabilities": ["imaging.data"]},
      "requires": {"services": [], "capabilities": [], "plugins": []},
      "optional": {"services": [], "capabilities": [], "plugins": []},
      "health_checks": ["service_registered", "data_path_accessible"]
    })json");
    file.close();

    QString error;
    const auto descriptor = PlatformDescriptorLoader::loadFromFile(file.fileName(), &error);

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(descriptor.id, QStringLiteral("org.medicalpro.dicom_viewer"));
    QCOMPARE(descriptor.runtime.ctkSymbolicName, QStringLiteral("DicomViewer"));
    QCOMPARE(descriptor.runtime.startupPolicy, PlatformStartupPolicy::Eager);
    QCOMPARE(descriptor.runtime.bootstrapLevel, PlatformBootstrapLevel::Core);
}

void PlatformDescriptorLoaderTest::loadFromFile_rejects_missing_startup_policy()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile file(dir.filePath("plugin.json"));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"json({
      "id": "org.medicalpro.bad_plugin",
      "version": "1.0.0",
      "display_name": "BrokenPlugin",
      "domain": "core",
      "enabled": true,
      "runtime": {
        "ctk_symbolic_name": "BrokenPlugin",
        "bootstrap_level": "core",
        "entry_capability": "broken.capability"
      },
      "provides": {"services": [], "capabilities": []},
      "requires": {"services": [], "capabilities": [], "plugins": []},
      "optional": {"services": [], "capabilities": [], "plugins": []},
      "health_checks": []
    })json");
    file.close();

    QString error;
    const auto descriptor = PlatformDescriptorLoader::loadFromFile(file.fileName(), &error);

    QVERIFY(!error.isEmpty());
    QVERIFY(descriptor.id.isEmpty());
}

QTEST_APPLESS_MAIN(PlatformDescriptorLoaderTest)
#include "PlatformDescriptorLoaderTest.moc"
```

- [ ] **Step 2: 把测试目标挂进现有测试体系，然后确认它先失败**

```cmake
# tests/unit/CMakeLists.txt
add_executable(platform_descriptor_loader_test
    PlatformDescriptorLoaderTest.cpp
    ${CMAKE_SOURCE_DIR}/Framework/Platform/Kernel/PlatformDescriptorLoader.cpp
)

target_include_directories(platform_descriptor_loader_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(platform_descriptor_loader_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME platform_descriptor_loader_test
    COMMAND platform_descriptor_loader_test
)
```

Run:

```powershell
cmake --build build_x64 --config Release --target platform_descriptor_loader_test
```

Expected:
- 编译失败，报 `PlatformDescriptorLoader.h` 或相关平台类型未定义

- [ ] **Step 3: 实现平台基础类型和 descriptor loader 的最小版本**

```cpp
// Framework/Platform/Contracts/PlatformRuntimeTypes.h
#pragma once

#include <QString>

enum class PlatformStartupPolicy { Eager, OnDemand, Disabled };
enum class PlatformBootstrapLevel { Core, Deferred };
enum class PlatformPluginState { Discovered, Installed, Starting, Ready, Degraded, Failed };
enum class PlatformRuntimeMode { ObserveOnly, FacadeMode, OrchestrateCore };

struct PlatformHealthCheckResult
{
    QString name;
    bool passed = false;
    QString detail;
};
```

```cpp
// Framework/Platform/Contracts/PlatformPluginDescriptor.h
#pragma once

#include "Framework/Platform/Contracts/PlatformRuntimeTypes.h"

#include <QString>
#include <QStringList>

struct PlatformServiceSet
{
    QStringList services;
    QStringList capabilities;
    QStringList plugins;
};

struct PlatformRuntimeDescriptor
{
    QString ctkSymbolicName;
    PlatformStartupPolicy startupPolicy = PlatformStartupPolicy::Disabled;
    PlatformBootstrapLevel bootstrapLevel = PlatformBootstrapLevel::Deferred;
    QString entryCapability;
};

struct PlatformPluginDescriptor
{
    QString id;
    QString version;
    QString displayName;
    QString domain;
    bool enabled = true;
    PlatformRuntimeDescriptor runtime;
    PlatformServiceSet provides;
    PlatformServiceSet requires;
    PlatformServiceSet optional;
    QStringList healthChecks;
};
```

```cpp
// Framework/Platform/Kernel/PlatformDescriptorLoader.h
#pragma once

#include "Framework/Platform/Contracts/PlatformPluginDescriptor.h"

#include <QVector>

class PlatformDescriptorLoader
{
public:
    static PlatformPluginDescriptor loadFromFile(const QString& filePath, QString* error = nullptr);
    static QVector<PlatformPluginDescriptor> loadFromDirectory(const QString& directoryPath, QStringList* errors = nullptr);
};
```

```cpp
// Framework/Platform/Kernel/PlatformDescriptorLoader.cpp
#include "Framework/Platform/Kernel/PlatformDescriptorLoader.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace
{
QStringList toStringList(const QJsonValue& value)
{
    QStringList output;
    for (const auto& item : value.toArray()) output.append(item.toString());
    return output;
}
}

PlatformPluginDescriptor PlatformDescriptorLoader::loadFromFile(const QString& filePath, QString* error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Cannot open descriptor: %1").arg(filePath);
        return {};
    }

    const auto root = QJsonDocument::fromJson(file.readAll()).object();
    PlatformPluginDescriptor descriptor;
    descriptor.id = root.value(QStringLiteral("id")).toString();
    descriptor.version = root.value(QStringLiteral("version")).toString();
    descriptor.displayName = root.value(QStringLiteral("display_name")).toString();
    descriptor.domain = root.value(QStringLiteral("domain")).toString();
    descriptor.enabled = root.value(QStringLiteral("enabled")).toBool(true);

    const auto runtimeObject = root.value(QStringLiteral("runtime")).toObject();
    descriptor.runtime.ctkSymbolicName = runtimeObject.value(QStringLiteral("ctk_symbolic_name")).toString();
    descriptor.runtime.entryCapability = runtimeObject.value(QStringLiteral("entry_capability")).toString();
    const QString startupPolicy = runtimeObject.value(QStringLiteral("startup_policy")).toString();
    const QString bootstrapLevel = runtimeObject.value(QStringLiteral("bootstrap_level")).toString();

    if (startupPolicy == QStringLiteral("eager")) descriptor.runtime.startupPolicy = PlatformStartupPolicy::Eager;
    else if (startupPolicy == QStringLiteral("on_demand")) descriptor.runtime.startupPolicy = PlatformStartupPolicy::OnDemand;
    else if (startupPolicy == QStringLiteral("disabled")) descriptor.runtime.startupPolicy = PlatformStartupPolicy::Disabled;
    else {
        if (error) *error = QStringLiteral("Unsupported startup_policy: %1").arg(startupPolicy);
        return {};
    }

    if (bootstrapLevel == QStringLiteral("core")) descriptor.runtime.bootstrapLevel = PlatformBootstrapLevel::Core;
    else if (bootstrapLevel == QStringLiteral("deferred")) descriptor.runtime.bootstrapLevel = PlatformBootstrapLevel::Deferred;
    else {
        if (error) *error = QStringLiteral("Unsupported bootstrap_level: %1").arg(bootstrapLevel);
        return {};
    }

    const auto providesObject = root.value(QStringLiteral("provides")).toObject();
    descriptor.provides.services = toStringList(providesObject.value(QStringLiteral("services")));
    descriptor.provides.capabilities = toStringList(providesObject.value(QStringLiteral("capabilities")));
    const auto requiresObject = root.value(QStringLiteral("requires")).toObject();
    descriptor.requires.services = toStringList(requiresObject.value(QStringLiteral("services")));
    descriptor.requires.capabilities = toStringList(requiresObject.value(QStringLiteral("capabilities")));
    descriptor.requires.plugins = toStringList(requiresObject.value(QStringLiteral("plugins")));
    const auto optionalObject = root.value(QStringLiteral("optional")).toObject();
    descriptor.optional.services = toStringList(optionalObject.value(QStringLiteral("services")));
    descriptor.optional.capabilities = toStringList(optionalObject.value(QStringLiteral("capabilities")));
    descriptor.optional.plugins = toStringList(optionalObject.value(QStringLiteral("plugins")));
    descriptor.healthChecks = toStringList(root.value(QStringLiteral("health_checks")));

    if (descriptor.id.isEmpty() || descriptor.version.isEmpty() || descriptor.displayName.isEmpty() || descriptor.domain.isEmpty()) {
        if (error) *error = QStringLiteral("Descriptor missing required fields: %1").arg(filePath);
        return {};
    }

    if (error) error->clear();
    return descriptor;
}
```

同时把这些新文件加进 `Framework`：

```cmake
# CMakeLists.txt
list(APPEND FRAMEWORK_SOURCES
    Framework/Platform/Contracts/PlatformRuntimeTypes.h
    Framework/Platform/Contracts/PlatformPluginDescriptor.h
    Framework/Platform/Kernel/PlatformDescriptorLoader.h
    Framework/Platform/Kernel/PlatformDescriptorLoader.cpp
)
```

- [ ] **Step 4: 运行测试，确认 loader 和类型已经打通**

Run:

```powershell
cmake --build build_x64 --config Release --target platform_descriptor_loader_test
ctest --test-dir build_x64 -C Release -R platform_descriptor_loader_test --output-on-failure
```

Expected:
- `platform_descriptor_loader_test` 构建通过
- 2 个测试断言通过

- [ ] **Step 5: 提交这一轮平台描述基础**

```powershell
git add CMakeLists.txt tests/unit/CMakeLists.txt tests/unit/PlatformDescriptorLoaderTest.cpp Framework/Platform/Contracts/PlatformRuntimeTypes.h Framework/Platform/Contracts/PlatformPluginDescriptor.h Framework/Platform/Kernel/PlatformDescriptorLoader.h Framework/Platform/Kernel/PlatformDescriptorLoader.cpp
git commit -m "feat: add platform descriptor loader foundation"
```

---

### Task 2: 为核心插件补 `plugin.json` 并把 descriptor 部署到运行时

**Files:**
- Modify: `cmake/PluginMacros.cmake`
- Modify: `tests/runtime/verify_runtime_artifacts.cmake`
- Modify: `tests/CMakeLists.txt`
- Create: `Plugins/UserManagement/platform/plugin.json`
- Create: `Plugins/DicomViewer/platform/plugin.json`
- Create: `Plugins/FourViewDisplay/platform/plugin.json`
- Create: `Plugins/RegistrationCore/platform/plugin.json`
- Create: `Plugins/OpticalTracking/platform/plugin.json`

- [ ] **Step 1: 先让运行时测试明确检查 descriptor，目前应当失败**

```cmake
# tests/CMakeLists.txt
add_test(
    NAME platform_descriptor_runtime_layout_test
    COMMAND ${CMAKE_COMMAND}
        -Druntime_dir=$<TARGET_FILE_DIR:medicalpro>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/runtime/verify_runtime_artifacts.cmake
)
```

```cmake
# tests/runtime/verify_runtime_artifacts.cmake
set(platform_descriptor_files
    "${runtime_dir}/plugins/descriptors/UserManagement.json"
    "${runtime_dir}/plugins/descriptors/DicomViewer.json"
    "${runtime_dir}/plugins/descriptors/FourViewDisplay.json"
    "${runtime_dir}/plugins/descriptors/RegistrationCore.json"
    "${runtime_dir}/plugins/descriptors/OpticalTracking.json"
)

foreach(descriptor_file IN LISTS platform_descriptor_files)
    if(NOT EXISTS "${descriptor_file}")
        list(APPEND missing_artifacts "${descriptor_file}")
    endif()
endforeach()
```

Run:

```powershell
cmake --build build_x64 --config Release --target medicalpro
ctest --test-dir build_x64 -C Release -R platform_descriptor_runtime_layout_test --output-on-failure
```

Expected:
- `platform_descriptor_runtime_layout_test` 失败
- 错误输出里出现缺失的 `plugins/descriptors/*.json`

- [ ] **Step 2: 为 5 个核心插件写出平台描述文件**

```json
// Plugins/UserManagement/platform/plugin.json
{
  "id": "org.medicalpro.user_management",
  "version": "1.0.0",
  "display_name": "UserManagement",
  "domain": "identity",
  "enabled": true,
  "runtime": {
    "ctk_symbolic_name": "UserManagement",
    "startup_policy": "eager",
    "bootstrap_level": "core",
    "entry_capability": "identity.core"
  },
  "provides": {
    "services": ["identity.user_query", "identity.session"],
    "capabilities": ["identity.core"]
  },
  "requires": {"services": [], "capabilities": [], "plugins": []},
  "optional": {"services": [], "capabilities": [], "plugins": []},
  "health_checks": ["service_registered", "database_accessible"]
}
```

```json
// Plugins/DicomViewer/platform/plugin.json
{
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
  "provides": {
    "services": ["imaging.study_query", "imaging.study_reader"],
    "capabilities": ["imaging.data"]
  },
  "requires": {"services": [], "capabilities": [], "plugins": []},
  "optional": {"services": [], "capabilities": [], "plugins": []},
  "health_checks": ["service_registered", "data_path_accessible"]
}
```

```json
// Plugins/FourViewDisplay/platform/plugin.json
{
  "id": "org.medicalpro.four_view_display",
  "version": "1.0.0",
  "display_name": "FourViewDisplay",
  "domain": "imaging",
  "enabled": true,
  "runtime": {
    "ctk_symbolic_name": "FourViewDisplay",
    "startup_policy": "eager",
    "bootstrap_level": "core",
    "entry_capability": "imaging.viewport"
  },
  "provides": {
    "services": ["imaging.viewport", "imaging.viewport_state"],
    "capabilities": ["imaging.viewport"]
  },
  "requires": {
    "services": [],
    "capabilities": ["imaging.data"],
    "plugins": []
  },
  "optional": {"services": [], "capabilities": [], "plugins": []},
  "health_checks": ["service_registered", "render_backend_accessible"]
}
```

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
  "provides": {
    "services": ["navigation.registration"],
    "capabilities": ["navigation.registration"]
  },
  "requires": {
    "services": [],
    "capabilities": ["imaging.data"],
    "plugins": []
  },
  "optional": {"services": [], "capabilities": [], "plugins": []},
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
  "provides": {
    "services": ["navigation.tracking"],
    "capabilities": ["navigation.tracking"]
  },
  "requires": {"services": [], "capabilities": [], "plugins": []},
  "optional": {"services": [], "capabilities": [], "plugins": []},
  "health_checks": ["service_registered", "tracking_adapter_accessible"]
}
```

- [ ] **Step 3: 修改插件宏，把 descriptor 复制到运行时 `plugins/descriptors`**

```cmake
# cmake/PluginMacros.cmake
function(copy_plugin_to_exe_dir PLUGIN_NAME)
    add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/$<CONFIG>/plugins"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${PLUGIN_NAME}>"
            "${CMAKE_BINARY_DIR}/$<CONFIG>/plugins/$<TARGET_FILE_NAME:${PLUGIN_NAME}>"
        COMMENT "Copying ${PLUGIN_NAME} plugin to output plugins directory"
        VERBATIM
    )

    set(_descriptor_source "${CMAKE_CURRENT_SOURCE_DIR}/platform/plugin.json")
    if(EXISTS "${_descriptor_source}")
        add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/$<CONFIG>/plugins/descriptors"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_descriptor_source}"
                "${CMAKE_BINARY_DIR}/$<CONFIG>/plugins/descriptors/${PLUGIN_NAME}.json"
            COMMENT "Copying ${PLUGIN_NAME} platform descriptor to runtime directory"
            VERBATIM
        )
    endif()
endfunction()
```

- [ ] **Step 4: 重跑主程序构建和运行时布局测试**

Run:

```powershell
cmake --build build_x64 --config Release --target medicalpro
ctest --test-dir build_x64 -C Release -R "runtime_artifact_layout_test|platform_descriptor_runtime_layout_test" --output-on-failure
```

Expected:
- 原有 `runtime_artifact_layout_test` 继续通过
- 新的 `platform_descriptor_runtime_layout_test` 通过

- [ ] **Step 5: 提交核心插件 descriptor 和部署链**

```powershell
git add cmake/PluginMacros.cmake tests/CMakeLists.txt tests/runtime/verify_runtime_artifacts.cmake Plugins/UserManagement/platform/plugin.json Plugins/DicomViewer/platform/plugin.json Plugins/FourViewDisplay/platform/plugin.json Plugins/RegistrationCore/platform/plugin.json Plugins/OpticalTracking/platform/plugin.json
git commit -m "feat: add core plugin platform descriptors"
```

---

### Task 3: 建立依赖图、状态存储和 capability 计算

**Files:**
- Create: `Framework/Platform/Contracts/PlatformSnapshots.h`
- Create: `Framework/Platform/Kernel/PlatformDependencyGraph.h`
- Create: `Framework/Platform/Kernel/PlatformDependencyGraph.cpp`
- Create: `Framework/Platform/Kernel/PlatformStateStore.h`
- Create: `Framework/Platform/Kernel/PlatformStateStore.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/unit/CMakeLists.txt`
- Create: `tests/unit/PlatformDependencyGraphTest.cpp`

- [ ] **Step 1: 先写依赖图失败用例，锁定 core 启动顺序和 on_demand 依赖边界**

```cpp
// tests/unit/PlatformDependencyGraphTest.cpp
#include <QtTest/QtTest>

#include "Framework/Platform/Contracts/PlatformPluginDescriptor.h"
#include "Framework/Platform/Kernel/PlatformDependencyGraph.h"

namespace
{
PlatformPluginDescriptor makeDescriptor(
    const QString& id,
    const QString& symbolicName,
    PlatformBootstrapLevel bootstrapLevel,
    PlatformStartupPolicy startupPolicy,
    const QStringList& providesCapabilities,
    const QStringList& requiresCapabilities = {})
{
    PlatformPluginDescriptor descriptor;
    descriptor.id = id;
    descriptor.version = QStringLiteral("1.0.0");
    descriptor.displayName = symbolicName;
    descriptor.domain = QStringLiteral("test");
    descriptor.runtime.ctkSymbolicName = symbolicName;
    descriptor.runtime.bootstrapLevel = bootstrapLevel;
    descriptor.runtime.startupPolicy = startupPolicy;
    descriptor.provides.capabilities = providesCapabilities;
    descriptor.requires.capabilities = requiresCapabilities;
    return descriptor;
}
}

class PlatformDependencyGraphTest : public QObject
{
    Q_OBJECT

private slots:
    void build_rejects_core_dependency_on_on_demand_plugin();
    void build_returns_topological_core_order();
};
```

```cpp
void PlatformDependencyGraphTest::build_rejects_core_dependency_on_on_demand_plugin()
{
    const auto coreDisplay = makeDescriptor(
        QStringLiteral("org.medicalpro.four_view_display"),
        QStringLiteral("FourViewDisplay"),
        PlatformBootstrapLevel::Core,
        PlatformStartupPolicy::Eager,
        {QStringLiteral("imaging.viewport")},
        {QStringLiteral("navigation.tracking")});

    const auto tracking = makeDescriptor(
        QStringLiteral("org.medicalpro.optical_tracking"),
        QStringLiteral("OpticalTracking"),
        PlatformBootstrapLevel::Deferred,
        PlatformStartupPolicy::OnDemand,
        {QStringLiteral("navigation.tracking")});

    const auto result = PlatformDependencyGraph::build({coreDisplay, tracking});
    QVERIFY(!result.errors.isEmpty());
    QVERIFY(result.errors.join(QStringLiteral(" | ")).contains(QStringLiteral("on_demand")));
}

void PlatformDependencyGraphTest::build_returns_topological_core_order()
{
    const auto user = makeDescriptor(
        QStringLiteral("org.medicalpro.user_management"),
        QStringLiteral("UserManagement"),
        PlatformBootstrapLevel::Core,
        PlatformStartupPolicy::Eager,
        {QStringLiteral("identity.core")});
    const auto dicom = makeDescriptor(
        QStringLiteral("org.medicalpro.dicom_viewer"),
        QStringLiteral("DicomViewer"),
        PlatformBootstrapLevel::Core,
        PlatformStartupPolicy::Eager,
        {QStringLiteral("imaging.data")});
    const auto display = makeDescriptor(
        QStringLiteral("org.medicalpro.four_view_display"),
        QStringLiteral("FourViewDisplay"),
        PlatformBootstrapLevel::Core,
        PlatformStartupPolicy::Eager,
        {QStringLiteral("imaging.viewport")},
        {QStringLiteral("imaging.data")});

    const auto result = PlatformDependencyGraph::build({user, dicom, display});
    QVERIFY(result.errors.isEmpty());
    QCOMPARE(result.coreStartupOrder, (QStringList{
        QStringLiteral("org.medicalpro.user_management"),
        QStringLiteral("org.medicalpro.dicom_viewer"),
        QStringLiteral("org.medicalpro.four_view_display")
    }));
}

QTEST_APPLESS_MAIN(PlatformDependencyGraphTest)
#include "PlatformDependencyGraphTest.moc"
```

- [ ] **Step 2: 把测试目标挂进单测体系，先确认依赖图类尚未实现而失败**

```cmake
# tests/unit/CMakeLists.txt
add_executable(platform_dependency_graph_test
    PlatformDependencyGraphTest.cpp
    ${CMAKE_SOURCE_DIR}/Framework/Platform/Kernel/PlatformDependencyGraph.cpp
    ${CMAKE_SOURCE_DIR}/Framework/Platform/Kernel/PlatformStateStore.cpp
)

target_include_directories(platform_dependency_graph_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(platform_dependency_graph_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME platform_dependency_graph_test
    COMMAND platform_dependency_graph_test
)
```

Run:

```powershell
cmake --build build_x64 --config Release --target platform_dependency_graph_test
```

Expected:
- 编译失败，提示 `PlatformDependencyGraph`、`PlatformStateStore` 或 `PlatformSnapshots` 未定义

- [ ] **Step 3: 实现快照模型、依赖图校验和 capability 状态存储**

```cpp
// Framework/Platform/Contracts/PlatformSnapshots.h
#pragma once

#include "Framework/Platform/Contracts/PlatformRuntimeTypes.h"

#include <QString>
#include <QStringList>
#include <QVector>

struct PlatformStartupTraceEntry
{
    QString phaseKey;
    QString phaseLabel;
    bool success = false;
    qint64 elapsedMs = 0;
    QString detail;
};

struct PlatformPluginRuntimeSnapshot
{
    QString pluginId;
    QString ctkSymbolicName;
    PlatformPluginState state = PlatformPluginState::Discovered;
    QStringList missingRequiredServices;
    QStringList missingRequiredCapabilities;
    QStringList missingRequiredPlugins;
};

struct PlatformCapabilitySnapshot
{
    PlatformRuntimeMode runtimeMode = PlatformRuntimeMode::ObserveOnly;
    bool platformReady = false;
    QStringList unlockedCapabilities;
    QStringList lockedCapabilities;
    QStringList degradedPlugins;
};

struct PlatformDiagnosticSnapshot
{
    bool frameworkReady = false;
    PlatformRuntimeMode runtimeMode = PlatformRuntimeMode::ObserveOnly;
    QVector<PlatformPluginRuntimeSnapshot> plugins;
    QVector<PlatformStartupTraceEntry> startupTrace;
    PlatformCapabilitySnapshot capabilitySnapshot;
    QStringList recoveryHints;
};
```

```cpp
// Framework/Platform/Kernel/PlatformDependencyGraph.h
#pragma once

#include "Framework/Platform/Contracts/PlatformPluginDescriptor.h"

#include <QHash>
#include <QStringList>

struct PlatformDependencyGraphResult
{
    QHash<QString, QStringList> outgoingEdges;
    QStringList coreStartupOrder;
    QStringList errors;
};

class PlatformDependencyGraph
{
public:
    static PlatformDependencyGraphResult build(const QVector<PlatformPluginDescriptor>& descriptors);
};
```

```cpp
// Framework/Platform/Kernel/PlatformStateStore.h
#pragma once

#include "Framework/Platform/Contracts/PlatformPluginDescriptor.h"
#include "Framework/Platform/Contracts/PlatformSnapshots.h"

#include <QHash>

class PlatformStateStore
{
public:
    void replaceDescriptors(const QVector<PlatformPluginDescriptor>& descriptors);
    void setRuntimeMode(PlatformRuntimeMode runtimeMode);
    void setPluginState(const QString& pluginId, PlatformPluginState state);
    PlatformCapabilitySnapshot capabilitySnapshot() const;
    QVector<PlatformPluginRuntimeSnapshot> pluginSnapshots() const;

private:
    PlatformRuntimeMode m_runtimeMode = PlatformRuntimeMode::ObserveOnly;
    QHash<QString, PlatformPluginDescriptor> m_descriptors;
    QHash<QString, PlatformPluginRuntimeSnapshot> m_snapshots;
};
```

```cpp
// Framework/Platform/Kernel/PlatformDependencyGraph.cpp
#include "Framework/Platform/Kernel/PlatformDependencyGraph.h"

#include <QHash>
#include <QQueue>

PlatformDependencyGraphResult PlatformDependencyGraph::build(const QVector<PlatformPluginDescriptor>& descriptors)
{
    PlatformDependencyGraphResult result;
    QHash<QString, QString> capabilityProviders;
    QHash<QString, int> indegree;
    QHash<QString, PlatformPluginDescriptor> byId;

    for (const auto& descriptor : descriptors) {
        byId.insert(descriptor.id, descriptor);
        indegree.insert(descriptor.id, 0);
        result.outgoingEdges.insert(descriptor.id, {});
        for (const auto& capability : descriptor.provides.capabilities) capabilityProviders.insert(capability, descriptor.id);
    }

    for (const auto& descriptor : descriptors) {
        for (const auto& capability : descriptor.requires.capabilities) {
            const QString providerId = capabilityProviders.value(capability);
            if (providerId.isEmpty()) result.errors.append(QStringLiteral("Missing capability provider: %1").arg(capability));
            else {
                if (descriptor.runtime.bootstrapLevel == PlatformBootstrapLevel::Core
                    && byId.value(providerId).runtime.startupPolicy == PlatformStartupPolicy::OnDemand) {
                    result.errors.append(QStringLiteral("core plugin %1 cannot require on_demand capability %2").arg(descriptor.id, capability));
                }
                result.outgoingEdges[providerId].append(descriptor.id);
                indegree[descriptor.id] = indegree.value(descriptor.id) + 1;
            }
        }
    }

    QQueue<QString> queue;
    for (auto it = indegree.cbegin(); it != indegree.cend(); ++it) {
        if (it.value() == 0 && byId.value(it.key()).runtime.bootstrapLevel == PlatformBootstrapLevel::Core) queue.enqueue(it.key());
    }

    while (!queue.isEmpty()) {
        const QString current = queue.dequeue();
        result.coreStartupOrder.append(current);
        for (const auto& next : result.outgoingEdges.value(current)) {
            indegree[next] = indegree.value(next) - 1;
            if (indegree.value(next) == 0 && byId.value(next).runtime.bootstrapLevel == PlatformBootstrapLevel::Core) queue.enqueue(next);
        }
    }

    return result;
}
```

```cpp
// Framework/Platform/Kernel/PlatformStateStore.cpp
#include "Framework/Platform/Kernel/PlatformStateStore.h"

void PlatformStateStore::replaceDescriptors(const QVector<PlatformPluginDescriptor>& descriptors)
{
    m_descriptors.clear();
    m_snapshots.clear();
    for (const auto& descriptor : descriptors) {
        m_descriptors.insert(descriptor.id, descriptor);
        PlatformPluginRuntimeSnapshot snapshot;
        snapshot.pluginId = descriptor.id;
        snapshot.ctkSymbolicName = descriptor.runtime.ctkSymbolicName;
        m_snapshots.insert(descriptor.id, snapshot);
    }
}

void PlatformStateStore::setRuntimeMode(PlatformRuntimeMode runtimeMode)
{
    m_runtimeMode = runtimeMode;
}

void PlatformStateStore::setPluginState(const QString& pluginId, PlatformPluginState state)
{
    auto snapshot = m_snapshots.value(pluginId);
    snapshot.state = state;
    m_snapshots.insert(pluginId, snapshot);
}

PlatformCapabilitySnapshot PlatformStateStore::capabilitySnapshot() const
{
    PlatformCapabilitySnapshot snapshot;
    snapshot.runtimeMode = m_runtimeMode;
    for (auto it = m_descriptors.cbegin(); it != m_descriptors.cend(); ++it) {
        const bool ready = m_snapshots.value(it.key()).state == PlatformPluginState::Ready;
        for (const auto& capability : it.value().provides.capabilities) {
            if (ready) snapshot.unlockedCapabilities.append(capability);
            else snapshot.lockedCapabilities.append(capability);
        }
    }
    snapshot.platformReady = snapshot.lockedCapabilities.isEmpty();
    return snapshot;
}

QVector<PlatformPluginRuntimeSnapshot> PlatformStateStore::pluginSnapshots() const
{
    return m_snapshots.values().toVector();
}
```

```cmake
# CMakeLists.txt
list(APPEND FRAMEWORK_SOURCES
    Framework/Platform/Contracts/PlatformSnapshots.h
    Framework/Platform/Kernel/PlatformDependencyGraph.h
    Framework/Platform/Kernel/PlatformDependencyGraph.cpp
    Framework/Platform/Kernel/PlatformStateStore.h
    Framework/Platform/Kernel/PlatformStateStore.cpp
)
```

- [ ] **Step 4: 运行依赖图测试，确认 core 顺序和 capability 锁定逻辑都生效**

Run:

```powershell
cmake --build build_x64 --config Release --target platform_dependency_graph_test
ctest --test-dir build_x64 -C Release -R platform_dependency_graph_test --output-on-failure
```

Expected:
- `platform_dependency_graph_test` 构建通过
- 2 个断言全部通过

- [ ] **Step 5: 提交平台依赖图与状态存储基础**

```powershell
git add CMakeLists.txt tests/unit/CMakeLists.txt tests/unit/PlatformDependencyGraphTest.cpp Framework/Platform/Contracts/PlatformSnapshots.h Framework/Platform/Kernel/PlatformDependencyGraph.h Framework/Platform/Kernel/PlatformDependencyGraph.cpp Framework/Platform/Kernel/PlatformStateStore.h Framework/Platform/Kernel/PlatformStateStore.cpp
git commit -m "feat: add platform dependency graph and state store"
```

---

### Task 4: 建立 observe_only 观察层、结构化启动 trace 和诊断服务

**Files:**
- Create: `Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.h`
- Create: `Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.cpp`
- Create: `Framework/Platform/Diagnostics/PlatformDiagnosticsService.h`
- Create: `Framework/Platform/Diagnostics/PlatformDiagnosticsService.cpp`
- Modify: `Framework/StartupOrchestrator.h`
- Modify: `Framework/StartupOrchestrator.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/unit/CMakeLists.txt`
- Create: `tests/unit/PlatformDiagnosticsServiceTest.cpp`

- [ ] **Step 1: 先写诊断聚合失败用例，锁定 observe_only 的快照结构**

```cpp
// tests/unit/PlatformDiagnosticsServiceTest.cpp
#include <QtTest/QtTest>

#include "Framework/Platform/Diagnostics/PlatformDiagnosticsService.h"
#include "Framework/Platform/Kernel/PlatformStateStore.h"

class PlatformDiagnosticsServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void buildSnapshot_includes_mode_trace_and_recovery_hint();
};

void PlatformDiagnosticsServiceTest::buildSnapshot_includes_mode_trace_and_recovery_hint()
{
    PlatformPluginDescriptor descriptor;
    descriptor.id = QStringLiteral("org.medicalpro.dicom_viewer");
    descriptor.version = QStringLiteral("1.0.0");
    descriptor.displayName = QStringLiteral("DicomViewer");
    descriptor.domain = QStringLiteral("imaging");
    descriptor.runtime.ctkSymbolicName = QStringLiteral("DicomViewer");
    descriptor.provides.capabilities = {QStringLiteral("imaging.data")};

    PlatformStateStore store;
    store.replaceDescriptors({descriptor});
    store.setRuntimeMode(PlatformRuntimeMode::ObserveOnly);

    PlatformRuntimeObservation observation;
    observation.frameworkReady = true;
    observation.installedPlugins = {QStringLiteral("DicomViewer")};
    observation.startupTrace = {
        {QStringLiteral("plugin_install"), QStringLiteral("Install plugins"), true, 210, QStringLiteral("installed 5 plugins")},
        {QStringLiteral("critical_start"), QStringLiteral("Start core plugins"), false, 820, QStringLiteral("DicomViewerService missing")}
    };

    PlatformDiagnosticsService service(&store);
    const auto snapshot = service.buildSnapshot(observation);

    QCOMPARE(snapshot.runtimeMode, PlatformRuntimeMode::ObserveOnly);
    QVERIFY(snapshot.frameworkReady);
    QCOMPARE(snapshot.startupTrace.size(), 2);
    QVERIFY(snapshot.recoveryHints.join(QStringLiteral(" | ")).contains(QStringLiteral("DicomViewerService")));
}

QTEST_APPLESS_MAIN(PlatformDiagnosticsServiceTest)
#include "PlatformDiagnosticsServiceTest.moc"
```

- [ ] **Step 2: 挂接测试目标，先确认 collector / service 尚未存在**

```cmake
# tests/unit/CMakeLists.txt
add_executable(platform_diagnostics_service_test
    PlatformDiagnosticsServiceTest.cpp
    ${CMAKE_SOURCE_DIR}/Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.cpp
    ${CMAKE_SOURCE_DIR}/Framework/Platform/Diagnostics/PlatformDiagnosticsService.cpp
    ${CMAKE_SOURCE_DIR}/Framework/Platform/Kernel/PlatformStateStore.cpp
)

target_include_directories(platform_diagnostics_service_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(platform_diagnostics_service_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME platform_diagnostics_service_test
    COMMAND platform_diagnostics_service_test
)
```

Run:

```powershell
cmake --build build_x64 --config Release --target platform_diagnostics_service_test
```

Expected:
- 编译失败，提示 `PlatformRuntimeObservation`、`PlatformDiagnosticsService` 或 `CtkRuntimeSnapshotCollector` 未定义

- [ ] **Step 3: 实现观测采集器与诊断服务，并给 StartupOrchestrator 暴露结构化 trace**

```cpp
// Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.h
#pragma once

#include "Framework/Platform/Contracts/PlatformSnapshots.h"

struct PlatformRuntimeObservation
{
    bool frameworkReady = false;
    QStringList installedPlugins;
    QStringList startedPlugins;
    QVector<PlatformStartupTraceEntry> startupTrace;
};

class CtkRuntimeSnapshotCollector
{
public:
    PlatformRuntimeObservation collect() const;
};
```

```cpp
// Framework/Platform/Diagnostics/PlatformDiagnosticsService.h
#pragma once

#include "Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.h"
#include "Framework/Platform/Contracts/PlatformSnapshots.h"

class PlatformStateStore;

class PlatformDiagnosticsService
{
public:
    explicit PlatformDiagnosticsService(PlatformStateStore* stateStore);
    PlatformDiagnosticSnapshot buildSnapshot(const PlatformRuntimeObservation& observation) const;

private:
    PlatformStateStore* m_stateStore;
};
```

```cpp
// Framework/StartupOrchestrator.h
#include "Framework/Platform/Contracts/PlatformSnapshots.h"

public:
    QVector<PlatformStartupTraceEntry> getStartupTraceEntries() const;

private:
    QVector<PlatformStartupTraceEntry> m_startupTraceEntries;
```

```cpp
// Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.cpp
#include "Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.h"

#include "Framework/CTKManager.h"
#include "Framework/StartupOrchestrator.h"

PlatformRuntimeObservation CtkRuntimeSnapshotCollector::collect() const
{
    PlatformRuntimeObservation observation;
    auto* ctkManager = CTKManager::instance();
    observation.frameworkReady = ctkManager && ctkManager->isCTKAvailable();
    observation.installedPlugins = ctkManager ? ctkManager->getInstalledPlugins() : QStringList{};
    observation.startedPlugins = ctkManager ? ctkManager->getStartedPlugins() : QStringList{};
    observation.startupTrace = StartupOrchestrator::instance()->getStartupTraceEntries();
    return observation;
}
```

```cpp
// Framework/Platform/Diagnostics/PlatformDiagnosticsService.cpp
#include "Framework/Platform/Diagnostics/PlatformDiagnosticsService.h"

#include "Framework/Platform/Kernel/PlatformStateStore.h"

PlatformDiagnosticsService::PlatformDiagnosticsService(PlatformStateStore* stateStore)
    : m_stateStore(stateStore)
{
}

PlatformDiagnosticSnapshot PlatformDiagnosticsService::buildSnapshot(const PlatformRuntimeObservation& observation) const
{
    PlatformDiagnosticSnapshot snapshot;
    snapshot.frameworkReady = observation.frameworkReady;
    snapshot.startupTrace = observation.startupTrace;
    snapshot.plugins = m_stateStore ? m_stateStore->pluginSnapshots() : QVector<PlatformPluginRuntimeSnapshot>{};
    snapshot.capabilitySnapshot = m_stateStore ? m_stateStore->capabilitySnapshot() : PlatformCapabilitySnapshot{};
    snapshot.runtimeMode = snapshot.capabilitySnapshot.runtimeMode;

    if (!observation.startupTrace.isEmpty() && !observation.startupTrace.constLast().success) {
        snapshot.recoveryHints.append(observation.startupTrace.constLast().detail);
    }
    for (const auto& plugin : snapshot.plugins) {
        if (!plugin.missingRequiredServices.isEmpty()) {
            snapshot.recoveryHints.append(
                QStringLiteral("%1 缺少服务：%2").arg(plugin.ctkSymbolicName, plugin.missingRequiredServices.join(QStringLiteral(", "))));
        }
    }
    return snapshot;
}
```

```cpp
// Framework/StartupOrchestrator.cpp
QVector<PlatformStartupTraceEntry> StartupOrchestrator::getStartupTraceEntries() const
{
    return m_startupTraceEntries;
}

bool StartupOrchestrator::executePhase(const PhaseInfo& info, QApplication* app, qint64& elapsedMs)
{
    QElapsedTimer timer;
    timer.start();
    const bool success = m_phaseHandlers.contains(info.phase) ? m_phaseHandlers.value(info.phase)(app) : true;

    PlatformStartupTraceEntry entry;
    entry.phaseKey = info.name;
    entry.phaseLabel = info.label;
    entry.success = success;
    entry.elapsedMs = timer.elapsed();
    entry.detail = success ? QStringLiteral("completed") : QStringLiteral("failed");
    m_startupTraceEntries.append(entry);

    elapsedMs += entry.elapsedMs;
    return success;
}
```

```cmake
# CMakeLists.txt
list(APPEND FRAMEWORK_SOURCES
    Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.h
    Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.cpp
    Framework/Platform/Diagnostics/PlatformDiagnosticsService.h
    Framework/Platform/Diagnostics/PlatformDiagnosticsService.cpp
)
```

- [ ] **Step 4: 运行诊断服务测试，确认 observe_only 快照可以稳定生成**

Run:

```powershell
cmake --build build_x64 --config Release --target platform_diagnostics_service_test
ctest --test-dir build_x64 -C Release -R platform_diagnostics_service_test --output-on-failure
```

Expected:
- `platform_diagnostics_service_test` 构建通过
- 诊断快照包含 `observe_only`、2 条 trace 和恢复建议

- [ ] **Step 5: 提交 observe_only 诊断层与结构化 trace**

```powershell
git add CMakeLists.txt tests/unit/CMakeLists.txt tests/unit/PlatformDiagnosticsServiceTest.cpp Framework/StartupOrchestrator.h Framework/StartupOrchestrator.cpp Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.h Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.cpp Framework/Platform/Diagnostics/PlatformDiagnosticsService.h Framework/Platform/Diagnostics/PlatformDiagnosticsService.cpp
git commit -m "feat: add observe-only platform diagnostics"
```

---

### Task 5: 落地诊断页并接入主程序外壳

**Files:**
- Create: `UI/Forms/PlatformDiagnosticsPage.ui`
- Create: `UI/NewPages/PlatformDiagnosticsPage.h`
- Create: `UI/NewPages/PlatformDiagnosticsPage.cpp`
- Modify: `UI/NewPages/CMakeLists.txt`
- Modify: `UI/NewPages/PageIndex.h`
- Modify: `UI/MainInterfaceWidget.h`
- Modify: `UI/MainInterfaceWidget.cpp`
- Modify: `UI/NewPages/SystemSettingsPage.h`
- Modify: `UI/NewPages/SystemSettingsPage.cpp`
- Modify: `tests/unit/CMakeLists.txt`
- Create: `tests/unit/PlatformDiagnosticsPageTest.cpp`

- [ ] **Step 1: 先写页面测试，锁定 provider 注入和渲染结果**

```cpp
// tests/unit/PlatformDiagnosticsPageTest.cpp
#include <QtTest/QtTest>
#include <QLabel>
#include <QTableWidget>

#include "UI/NewPages/PlatformDiagnosticsPage.h"

class PlatformDiagnosticsPageTest : public QObject
{
    Q_OBJECT

private slots:
    void refreshSnapshot_renders_mode_plugin_and_trace();
};

void PlatformDiagnosticsPageTest::refreshSnapshot_renders_mode_plugin_and_trace()
{
    PlatformDiagnosticSnapshot snapshot;
    snapshot.runtimeMode = PlatformRuntimeMode::ObserveOnly;
    snapshot.frameworkReady = true;
    snapshot.startupTrace = {
        {QStringLiteral("critical_start"), QStringLiteral("Start core plugins"), true, 540, QStringLiteral("ok")}
    };

    PlatformPluginRuntimeSnapshot pluginSnapshot;
    pluginSnapshot.pluginId = QStringLiteral("org.medicalpro.dicom_viewer");
    pluginSnapshot.ctkSymbolicName = QStringLiteral("DicomViewer");
    pluginSnapshot.state = PlatformPluginState::Ready;
    snapshot.plugins.append(pluginSnapshot);

    PlatformDiagnosticsPage page(nullptr, [snapshot]() { return snapshot; });
    page.refreshSnapshot();

    QCOMPARE(page.findChild<QLabel*>(QStringLiteral("runtimeModeValueLabel"))->text(), QStringLiteral("observe_only"));
    QCOMPARE(page.findChild<QTableWidget*>(QStringLiteral("pluginTableWidget"))->rowCount(), 1);
    QCOMPARE(page.findChild<QTableWidget*>(QStringLiteral("traceTableWidget"))->rowCount(), 1);
}

QTEST_MAIN(PlatformDiagnosticsPageTest)
#include "PlatformDiagnosticsPageTest.moc"
```

- [ ] **Step 2: 挂接页面测试目标，先确认页面类和 ui 尚未存在**

```cmake
# tests/unit/CMakeLists.txt
add_executable(platform_diagnostics_page_test
    PlatformDiagnosticsPageTest.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/PlatformDiagnosticsPage.cpp
    ${CMAKE_SOURCE_DIR}/UI/Forms/PlatformDiagnosticsPage.ui
)

set_target_properties(platform_diagnostics_page_test PROPERTIES
    AUTOUIC_SEARCH_PATHS "${CMAKE_SOURCE_DIR}/UI/Forms"
)

target_include_directories(platform_diagnostics_page_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/UI/NewPages
    ${CMAKE_SOURCE_DIR}/UI/Forms
)

target_link_libraries(platform_diagnostics_page_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME platform_diagnostics_page_test
    COMMAND platform_diagnostics_page_test
)
```

Run:

```powershell
cmake --build build_x64 --config Release --target platform_diagnostics_page_test
```

Expected:
- 编译失败，提示 `PlatformDiagnosticsPage` 或 `PlatformDiagnosticsPage.ui` 不存在

- [ ] **Step 3: 实现诊断页、PageIndex 扩展和主壳跳转入口**

```cpp
// UI/NewPages/PlatformDiagnosticsPage.h
#pragma once

#include "BasePage.h"
#include "Framework/Platform/Contracts/PlatformSnapshots.h"

#include <functional>

namespace Ui { class PlatformDiagnosticsPage; }

class PlatformDiagnosticsPage : public BasePage
{
    Q_OBJECT

public:
    using SnapshotProvider = std::function<PlatformDiagnosticSnapshot()>;

    explicit PlatformDiagnosticsPage(QWidget* parent = nullptr, SnapshotProvider snapshotProvider = {});
    ~PlatformDiagnosticsPage() override;
    void onActivated() override;
    void refreshSnapshot();

signals:
    void backRequested();

private slots:
    void on_backButton_clicked();

private:
    QString runtimeModeText(PlatformRuntimeMode runtimeMode) const;

    Ui::PlatformDiagnosticsPage* ui;
    SnapshotProvider m_snapshotProvider;
};
```

```cpp
// UI/NewPages/PlatformDiagnosticsPage.cpp
#include "PlatformDiagnosticsPage.h"
#include "ui_PlatformDiagnosticsPage.h"

PlatformDiagnosticsPage::PlatformDiagnosticsPage(QWidget* parent, SnapshotProvider snapshotProvider)
    : BasePage(parent)
    , ui(new Ui::PlatformDiagnosticsPage)
    , m_snapshotProvider(std::move(snapshotProvider))
{
    ui->setupUi(this);
}

PlatformDiagnosticsPage::~PlatformDiagnosticsPage()
{
    delete ui;
}

void PlatformDiagnosticsPage::onActivated()
{
    BasePage::onActivated();
    refreshSnapshot();
}

void PlatformDiagnosticsPage::refreshSnapshot()
{
    const auto snapshot = m_snapshotProvider ? m_snapshotProvider() : PlatformDiagnosticSnapshot{};
    ui->runtimeModeValueLabel->setText(runtimeModeText(snapshot.runtimeMode));
    ui->pluginTableWidget->setRowCount(snapshot.plugins.size());
    ui->traceTableWidget->setRowCount(snapshot.startupTrace.size());
}

QString PlatformDiagnosticsPage::runtimeModeText(PlatformRuntimeMode runtimeMode) const
{
    switch (runtimeMode) {
    case PlatformRuntimeMode::ObserveOnly: return QStringLiteral("observe_only");
    case PlatformRuntimeMode::FacadeMode: return QStringLiteral("facade_mode");
    case PlatformRuntimeMode::OrchestrateCore: return QStringLiteral("orchestrate_core");
    }
    return QStringLiteral("unknown");
}

void PlatformDiagnosticsPage::on_backButton_clicked()
{
    emit backRequested();
}
```

```xml
<!-- UI/Forms/PlatformDiagnosticsPage.ui -->
<ui version="4.0">
 <class>PlatformDiagnosticsPage</class>
 <widget class="QWidget" name="PlatformDiagnosticsPage">
  <layout class="QVBoxLayout" name="mainLayout">
   <item><widget class="QPushButton" name="backButton"/></item>
   <item><widget class="QLabel" name="runtimeModeValueLabel"/></item>
   <item><widget class="QTableWidget" name="pluginTableWidget"/></item>
   <item><widget class="QTableWidget" name="traceTableWidget"/></item>
  </layout>
 </widget>
</ui>
```

```cpp
// UI/NewPages/PageIndex.h
enum class PageIndex {
    Welcome = 0,
    Login = 1,
    ModuleSelection = 2,
    SystemSettings = 3,
    Management = 4,
    Dashboard = 5,
    Navigation = 6,
    Diagnostics = 7
};
```

```cpp
// UI/NewPages/SystemSettingsPage.h
signals:
    void backRequested();
    void diagnosticsRequested();
```

```cpp
// UI/NewPages/SystemSettingsPage.cpp
auto* diagnosticsButton = new QPushButton(QStringLiteral("运行诊断"), ui->systemSettingsHeaderFrame);
ui->headerLayout->insertWidget(2, diagnosticsButton);
connect(diagnosticsButton, &QPushButton::clicked, this, [this]() {
    emit diagnosticsRequested();
    emit navigateTo(toInt(PageIndex::Diagnostics));
});
```

```cpp
// UI/MainInterfaceWidget.h
class PlatformDiagnosticsPage;

private slots:
    void onSystemSettingsDiagnostics();
    void onDiagnosticsBack();

private:
    PlatformDiagnosticsPage* m_platformDiagnosticsPage;
```

```cpp
// UI/MainInterfaceWidget.cpp
m_platformDiagnosticsPage = new PlatformDiagnosticsPage(this, [this]() {
    return m_platformDiagnosticsService->buildSnapshot(m_runtimeCollector.collect());
});
m_stackedWidget->addWidget(m_platformDiagnosticsPage);

connect(m_systemSettingsPage, &SystemSettingsPageNew::diagnosticsRequested,
        this, &MainInterfaceWidget::onSystemSettingsDiagnostics);
connect(m_platformDiagnosticsPage, &PlatformDiagnosticsPage::backRequested,
        this, &MainInterfaceWidget::onDiagnosticsBack);
```

```cmake
# UI/NewPages/CMakeLists.txt
list(APPEND NEWPAGES_HEADERS PlatformDiagnosticsPage.h)
list(APPEND NEWPAGES_SOURCES PlatformDiagnosticsPage.cpp)
list(APPEND NEWPAGES_UI_FILES ${UI_FORMS_DIR}/PlatformDiagnosticsPage.ui)
```

- [ ] **Step 4: 运行页面测试和 NewPagesLib 构建，确认诊断页能在主壳访问**

Run:

```powershell
cmake --build build_x64 --config Release --target NewPagesLib platform_diagnostics_page_test
ctest --test-dir build_x64 -C Release -R platform_diagnostics_page_test --output-on-failure
```

Expected:
- `NewPagesLib` 构建通过
- `platform_diagnostics_page_test` 通过
- System Settings 中出现“运行诊断”入口

- [ ] **Step 5: 提交诊断页接入与 UI 收口入口**

```powershell
git add UI/Forms/PlatformDiagnosticsPage.ui UI/NewPages/CMakeLists.txt UI/NewPages/PageIndex.h UI/NewPages/PlatformDiagnosticsPage.h UI/NewPages/PlatformDiagnosticsPage.cpp UI/NewPages/SystemSettingsPage.h UI/NewPages/SystemSettingsPage.cpp UI/MainInterfaceWidget.h UI/MainInterfaceWidget.cpp tests/unit/CMakeLists.txt tests/unit/PlatformDiagnosticsPageTest.cpp
git commit -m "feat: add platform diagnostics page"
```

---

### Task 6: 落地三大门面和 legacy adapter

**Files:**
- Create: `Framework/Platform/Contracts/PlatformFacadePorts.h`
- Create: `Framework/Platform/LegacyAdapters/LegacyUserManagementAdapter.h`
- Create: `Framework/Platform/LegacyAdapters/LegacyUserManagementAdapter.cpp`
- Create: `Framework/Platform/LegacyAdapters/LegacyImagingAdapter.h`
- Create: `Framework/Platform/LegacyAdapters/LegacyImagingAdapter.cpp`
- Create: `Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.h`
- Create: `Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.cpp`
- Create: `Framework/Platform/Facades/IdentityAppService.h`
- Create: `Framework/Platform/Facades/IdentityAppService.cpp`
- Create: `Framework/Platform/Facades/ImagingAppService.h`
- Create: `Framework/Platform/Facades/ImagingAppService.cpp`
- Create: `Framework/Platform/Facades/NavigationAppService.h`
- Create: `Framework/Platform/Facades/NavigationAppService.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/unit/CMakeLists.txt`
- Create: `tests/unit/PlatformFacadesTest.cpp`

- [ ] **Step 1: 先写门面测试，锁定门面只依赖 port、不直接依赖 CTK**

```cpp
// tests/unit/PlatformFacadesTest.cpp
#include <QtTest/QtTest>

#include "Framework/Platform/Facades/IdentityAppService.h"
#include "Framework/Platform/Facades/ImagingAppService.h"
#include "Framework/Platform/Facades/NavigationAppService.h"

class FakeIdentityPort : public IIdentityFacadePort
{
public:
    QString currentUserName() const override { return QStringLiteral("admin"); }
    bool hasActiveSession() const override { return true; }
};

class FakeImagingPort : public IImagingFacadePort
{
public:
    QString currentPatientName() const override { return QStringLiteral("patient-a"); }
    bool hasReadableStudy() const override { return true; }
};

class FakeNavigationPort : public INavigationFacadePort
{
public:
    bool ensureReady(const QString& pluginId) override
    {
        lastPluginId = pluginId;
        return true;
    }

    QString lastPluginId;
};

class PlatformFacadesTest : public QObject
{
    Q_OBJECT

private slots:
    void identityFacade_reads_current_session_from_port();
    void navigationFacade_forwards_ensure_ready_to_port();
};
```

```cpp
void PlatformFacadesTest::identityFacade_reads_current_session_from_port()
{
    FakeIdentityPort port;
    IdentityAppService service(&port);
    QCOMPARE(service.currentUserName(), QStringLiteral("admin"));
    QVERIFY(service.hasActiveSession());
}

void PlatformFacadesTest::navigationFacade_forwards_ensure_ready_to_port()
{
    FakeNavigationPort port;
    NavigationAppService service(&port);
    QVERIFY(service.ensureReady(QStringLiteral("org.medicalpro.registration_core")));
    QCOMPARE(port.lastPluginId, QStringLiteral("org.medicalpro.registration_core"));
}

QTEST_APPLESS_MAIN(PlatformFacadesTest)
#include "PlatformFacadesTest.moc"
```

- [ ] **Step 2: 挂接门面测试目标，先确认 port / facade / adapter 还不存在**

```cmake
# tests/unit/CMakeLists.txt
add_executable(platform_facades_test
    PlatformFacadesTest.cpp
    ${CMAKE_SOURCE_DIR}/Framework/Platform/Facades/IdentityAppService.cpp
    ${CMAKE_SOURCE_DIR}/Framework/Platform/Facades/ImagingAppService.cpp
    ${CMAKE_SOURCE_DIR}/Framework/Platform/Facades/NavigationAppService.cpp
)

target_include_directories(platform_facades_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(platform_facades_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME platform_facades_test
    COMMAND platform_facades_test
)
```

Run:

```powershell
cmake --build build_x64 --config Release --target platform_facades_test
```

Expected:
- 编译失败，提示 facade port 或 app service 未定义

- [ ] **Step 3: 定义 facade port、legacy adapter 和三个 app service**

```cpp
// Framework/Platform/Contracts/PlatformFacadePorts.h
#pragma once

#include <QString>

class IIdentityFacadePort
{
public:
    virtual ~IIdentityFacadePort() = default;
    virtual QString currentUserName() const = 0;
    virtual bool hasActiveSession() const = 0;
};

class IImagingFacadePort
{
public:
    virtual ~IImagingFacadePort() = default;
    virtual QString currentPatientName() const = 0;
    virtual bool hasReadableStudy() const = 0;
};

class INavigationFacadePort
{
public:
    virtual ~INavigationFacadePort() = default;
    virtual bool ensureReady(const QString& pluginId) = 0;
};
```

```cpp
// Framework/Platform/Facades/IdentityAppService.h
#pragma once

#include "Framework/Platform/Contracts/PlatformFacadePorts.h"

class IdentityAppService
{
public:
    explicit IdentityAppService(IIdentityFacadePort* port);
    QString currentUserName() const;
    bool hasActiveSession() const;

private:
    IIdentityFacadePort* m_port;
};
```

```cpp
// Framework/Platform/Facades/IdentityAppService.cpp
#include "Framework/Platform/Facades/IdentityAppService.h"

IdentityAppService::IdentityAppService(IIdentityFacadePort* port)
    : m_port(port)
{
}

QString IdentityAppService::currentUserName() const
{
    return m_port ? m_port->currentUserName() : QString{};
}

bool IdentityAppService::hasActiveSession() const
{
    return m_port && m_port->hasActiveSession();
}
```

```cpp
// Framework/Platform/Facades/ImagingAppService.cpp
#include "Framework/Platform/Facades/ImagingAppService.h"

ImagingAppService::ImagingAppService(IImagingFacadePort* port)
    : m_port(port)
{
}

QString ImagingAppService::currentPatientName() const
{
    return m_port ? m_port->currentPatientName() : QString{};
}

bool ImagingAppService::hasReadableStudy() const
{
    return m_port && m_port->hasReadableStudy();
}
```

```cpp
// Framework/Platform/Facades/NavigationAppService.cpp
#include "Framework/Platform/Facades/NavigationAppService.h"

NavigationAppService::NavigationAppService(INavigationFacadePort* port)
    : m_port(port)
{
}

bool NavigationAppService::ensureReady(const QString& pluginId)
{
    return m_port && m_port->ensureReady(pluginId);
}
```

```cpp
// Framework/Platform/LegacyAdapters/LegacyUserManagementAdapter.cpp
#include "Framework/Platform/LegacyAdapters/LegacyUserManagementAdapter.h"

#include "Framework/CTKManager.h"
#include "Plugins/UserManagement/UserManagementService.h"

QString LegacyUserManagementAdapter::currentUserName() const
{
    auto* service = CTKManager::instance()->getService<UserManagementService>();
    return service ? service->getCurrentUser().username : QString{};
}

bool LegacyUserManagementAdapter::hasActiveSession() const
{
    auto* service = CTKManager::instance()->getService<UserManagementService>();
    return service && service->getCurrentUser().isValid();
}
```

```cpp
// Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.cpp
#include "Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.h"

#include "Framework/CTKManager.h"

bool LegacyNavigationAdapter::ensureReady(const QString& pluginId)
{
    return CTKManager::instance()->startPlugin(pluginId);
}
```

```cmake
# CMakeLists.txt
list(APPEND FRAMEWORK_SOURCES
    Framework/Platform/Contracts/PlatformFacadePorts.h
    Framework/Platform/LegacyAdapters/LegacyUserManagementAdapter.h
    Framework/Platform/LegacyAdapters/LegacyUserManagementAdapter.cpp
    Framework/Platform/LegacyAdapters/LegacyImagingAdapter.h
    Framework/Platform/LegacyAdapters/LegacyImagingAdapter.cpp
    Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.h
    Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.cpp
    Framework/Platform/Facades/IdentityAppService.h
    Framework/Platform/Facades/IdentityAppService.cpp
    Framework/Platform/Facades/ImagingAppService.h
    Framework/Platform/Facades/ImagingAppService.cpp
    Framework/Platform/Facades/NavigationAppService.h
    Framework/Platform/Facades/NavigationAppService.cpp
)
```

- [ ] **Step 4: 运行门面测试，确认门面边界与 legacy adapter 稳定**

Run:

```powershell
cmake --build build_x64 --config Release --target platform_facades_test
ctest --test-dir build_x64 -C Release -R platform_facades_test --output-on-failure
```

Expected:
- `platform_facades_test` 构建通过
- 2 个门面断言通过

- [ ] **Step 5: 提交三大门面和兼容层**

```powershell
git add CMakeLists.txt tests/unit/CMakeLists.txt tests/unit/PlatformFacadesTest.cpp Framework/Platform/Contracts/PlatformFacadePorts.h Framework/Platform/LegacyAdapters/LegacyUserManagementAdapter.h Framework/Platform/LegacyAdapters/LegacyUserManagementAdapter.cpp Framework/Platform/LegacyAdapters/LegacyImagingAdapter.h Framework/Platform/LegacyAdapters/LegacyImagingAdapter.cpp Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.h Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.cpp Framework/Platform/Facades/IdentityAppService.h Framework/Platform/Facades/IdentityAppService.cpp Framework/Platform/Facades/ImagingAppService.h Framework/Platform/Facades/ImagingAppService.cpp Framework/Platform/Facades/NavigationAppService.h Framework/Platform/Facades/NavigationAppService.cpp
git commit -m "feat: add platform facades and legacy adapters"
```

---

### Task 7: 落地 PlatformStartupCoordinator、运行模式和按需启动能力

**Files:**
- Create: `config/platform_runtime.json`
- Create: `Framework/Platform/Kernel/PlatformRuntimeConfig.h`
- Create: `Framework/Platform/Kernel/PlatformRuntimeConfig.cpp`
- Create: `Framework/Platform/Kernel/PlatformStartupCoordinator.h`
- Create: `Framework/Platform/Kernel/PlatformStartupCoordinator.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/unit/CMakeLists.txt`
- Create: `tests/unit/PlatformStartupCoordinatorTest.cpp`

- [ ] **Step 1: 先写配置与 coordinator 失败用例，锁定 runtime mode 和 ensureReady 行为**

```cpp
// tests/unit/PlatformStartupCoordinatorTest.cpp
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>

#include "Framework/Platform/Kernel/PlatformRuntimeConfig.h"
#include "Framework/Platform/Kernel/PlatformStartupCoordinator.h"

class PlatformStartupCoordinatorTest : public QObject
{
    Q_OBJECT

private slots:
    void loadFromFile_reads_runtime_mode_and_core_plugin_ids();
    void ensureReady_starts_target_plugin_once();
};
```

```cpp
void PlatformStartupCoordinatorTest::loadFromFile_reads_runtime_mode_and_core_plugin_ids()
{
    QTemporaryDir dir;
    QFile file(dir.filePath(QStringLiteral("platform_runtime.json")));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"json({
      "runtime_mode": "facade_mode",
      "descriptor_directory": "plugins/descriptors",
      "core_plugin_ids": [
        "org.medicalpro.user_management",
        "org.medicalpro.dicom_viewer",
        "org.medicalpro.four_view_display"
      ]
    })json");
    file.close();

    QString error;
    const auto config = PlatformRuntimeConfig::loadFromFile(file.fileName(), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(config.runtimeMode, PlatformRuntimeMode::FacadeMode);
    QCOMPARE(config.corePluginIds.size(), 3);
}

void PlatformStartupCoordinatorTest::ensureReady_starts_target_plugin_once()
{
    QStringList startedPlugins;
    PlatformStartupCoordinator coordinator(
        PlatformRuntimeMode::FacadeMode,
        [&startedPlugins](const QString& pluginId) {
            startedPlugins.append(pluginId);
            return true;
        });

    QVERIFY(coordinator.ensureReady(QStringLiteral("org.medicalpro.registration_core")));
    QVERIFY(coordinator.ensureReady(QStringLiteral("org.medicalpro.registration_core")));
    QCOMPARE(startedPlugins, (QStringList{QStringLiteral("org.medicalpro.registration_core")}));
}

QTEST_APPLESS_MAIN(PlatformStartupCoordinatorTest)
#include "PlatformStartupCoordinatorTest.moc"
```

- [ ] **Step 2: 挂接 coordinator 测试目标，先确认配置与 coordinator 尚未存在**

```cmake
# tests/unit/CMakeLists.txt
add_executable(platform_startup_coordinator_test
    PlatformStartupCoordinatorTest.cpp
    ${CMAKE_SOURCE_DIR}/Framework/Platform/Kernel/PlatformRuntimeConfig.cpp
    ${CMAKE_SOURCE_DIR}/Framework/Platform/Kernel/PlatformStartupCoordinator.cpp
)

target_include_directories(platform_startup_coordinator_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(platform_startup_coordinator_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME platform_startup_coordinator_test
    COMMAND platform_startup_coordinator_test
)
```

Run:

```powershell
cmake --build build_x64 --config Release --target platform_startup_coordinator_test
```

Expected:
- 编译失败，提示 `PlatformRuntimeConfig` 或 `PlatformStartupCoordinator` 未定义

- [ ] **Step 3: 实现运行模式配置和最小启动编排器**

```json
// config/platform_runtime.json
{
  "runtime_mode": "observe_only",
  "descriptor_directory": "plugins/descriptors",
  "core_plugin_ids": [
    "org.medicalpro.user_management",
    "org.medicalpro.dicom_viewer",
    "org.medicalpro.four_view_display"
  ]
}
```

```cpp
// Framework/Platform/Kernel/PlatformRuntimeConfig.h
#pragma once

#include "Framework/Platform/Contracts/PlatformRuntimeTypes.h"

#include <QString>
#include <QStringList>

struct PlatformRuntimeConfig
{
    PlatformRuntimeMode runtimeMode = PlatformRuntimeMode::ObserveOnly;
    QString descriptorDirectory;
    QStringList corePluginIds;

    static PlatformRuntimeConfig loadFromFile(const QString& filePath, QString* error = nullptr);
};
```

```cpp
// Framework/Platform/Kernel/PlatformStartupCoordinator.h
#pragma once

#include "Framework/Platform/Contracts/PlatformRuntimeTypes.h"

#include <functional>
#include <QSet>
#include <QString>

class PlatformStartupCoordinator
{
public:
    using StartPluginFn = std::function<bool(const QString&)>;

    PlatformStartupCoordinator(PlatformRuntimeMode runtimeMode, StartPluginFn startPluginFn);
    bool ensureReady(const QString& pluginId);
    PlatformRuntimeMode runtimeMode() const;

private:
    PlatformRuntimeMode m_runtimeMode;
    StartPluginFn m_startPluginFn;
    QSet<QString> m_startedOnDemandPlugins;
};
```

```cpp
// Framework/Platform/Kernel/PlatformRuntimeConfig.cpp
#include "Framework/Platform/Kernel/PlatformRuntimeConfig.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

PlatformRuntimeConfig PlatformRuntimeConfig::loadFromFile(const QString& filePath, QString* error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Cannot open runtime config: %1").arg(filePath);
        return {};
    }

    const auto root = QJsonDocument::fromJson(file.readAll()).object();
    PlatformRuntimeConfig config;
    const QString runtimeMode = root.value(QStringLiteral("runtime_mode")).toString();
    if (runtimeMode == QStringLiteral("observe_only")) config.runtimeMode = PlatformRuntimeMode::ObserveOnly;
    else if (runtimeMode == QStringLiteral("facade_mode")) config.runtimeMode = PlatformRuntimeMode::FacadeMode;
    else if (runtimeMode == QStringLiteral("orchestrate_core")) config.runtimeMode = PlatformRuntimeMode::OrchestrateCore;
    else if (error) *error = QStringLiteral("Unsupported runtime mode: %1").arg(runtimeMode);

    config.descriptorDirectory = root.value(QStringLiteral("descriptor_directory")).toString();
    for (const auto& item : root.value(QStringLiteral("core_plugin_ids")).toArray()) config.corePluginIds.append(item.toString());
    return config;
}
```

```cpp
// Framework/Platform/Kernel/PlatformStartupCoordinator.cpp
#include "Framework/Platform/Kernel/PlatformStartupCoordinator.h"

PlatformStartupCoordinator::PlatformStartupCoordinator(PlatformRuntimeMode runtimeMode, StartPluginFn startPluginFn)
    : m_runtimeMode(runtimeMode)
    , m_startPluginFn(std::move(startPluginFn))
{
}

bool PlatformStartupCoordinator::ensureReady(const QString& pluginId)
{
    if (m_runtimeMode == PlatformRuntimeMode::ObserveOnly) return false;
    if (m_startedOnDemandPlugins.contains(pluginId)) return true;
    if (!m_startPluginFn || !m_startPluginFn(pluginId)) return false;
    m_startedOnDemandPlugins.insert(pluginId);
    return true;
}

PlatformRuntimeMode PlatformStartupCoordinator::runtimeMode() const
{
    return m_runtimeMode;
}
```

```cmake
# CMakeLists.txt
list(APPEND FRAMEWORK_SOURCES
    Framework/Platform/Kernel/PlatformRuntimeConfig.h
    Framework/Platform/Kernel/PlatformRuntimeConfig.cpp
    Framework/Platform/Kernel/PlatformStartupCoordinator.h
    Framework/Platform/Kernel/PlatformStartupCoordinator.cpp
)
```

- [ ] **Step 4: 运行 coordinator 测试，确认 runtime mode 和 ensureReady 行为固定**

Run:

```powershell
cmake --build build_x64 --config Release --target platform_startup_coordinator_test medicalpro
ctest --test-dir build_x64 -C Release -R platform_startup_coordinator_test --output-on-failure
```

Expected:
- `platform_startup_coordinator_test` 构建通过
- `platform_runtime.json` 会随现有 `config` 目录一起复制到运行目录
- `ensureReady` 对同一插件只触发一次实际启动

- [ ] **Step 5: 提交运行模式和启动编排器**

```powershell
git add CMakeLists.txt tests/unit/CMakeLists.txt tests/unit/PlatformStartupCoordinatorTest.cpp config/platform_runtime.json Framework/Platform/Kernel/PlatformRuntimeConfig.h Framework/Platform/Kernel/PlatformRuntimeConfig.cpp Framework/Platform/Kernel/PlatformStartupCoordinator.h Framework/Platform/Kernel/PlatformStartupCoordinator.cpp
git commit -m "feat: add platform startup coordinator"
```

---

### Task 8: 把核心页面迁移到平台 provider / facade

**Files:**
- Modify: `UI/MainInterfaceWidget.h`
- Modify: `UI/MainInterfaceWidget.cpp`
- Modify: `UI/NewPages/WelcomePage.h`
- Modify: `UI/NewPages/WelcomePage.cpp`
- Modify: `UI/NewPages/ModuleSelectionPage.h`
- Modify: `UI/NewPages/ModuleSelectionPage.cpp`
- Modify: `UI/NewPages/SystemSettingsPage.h`
- Modify: `UI/NewPages/SystemSettingsPage.cpp`
- Modify: `UI/NewPages/ManagementPage.h`
- Modify: `UI/NewPages/ManagementPage.cpp`
- Modify: `UI/NewPages/DashboardPage.h`
- Modify: `UI/NewPages/DashboardPage.cpp`
- Modify: `tests/unit/CMakeLists.txt`
- Create: `tests/unit/CorePagesPlatformProvidersTest.cpp`

- [ ] **Step 1: 先写页面 provider 测试，锁定 ModuleSelection / SystemSettings 不再直连 CTK**

```cpp
// tests/unit/CorePagesPlatformProvidersTest.cpp
#include <QtTest/QtTest>

#include "UI/NewPages/ModuleSelectionPage.h"
#include "UI/NewPages/SystemSettingsPage.h"

class CorePagesPlatformProvidersTest : public QObject
{
    Q_OBJECT

private slots:
    void moduleSelection_uses_runtime_status_provider();
    void systemSettings_uses_runtime_status_provider();
};

void CorePagesPlatformProvidersTest::moduleSelection_uses_runtime_status_provider()
{
    ModuleSelectionPageNew page(nullptr, []() {
        ModuleSelectionPageNew::ModuleRuntimeStatus status;
        status.frameworkReady = true;
        status.workflowReady = true;
        status.readyServices = 3;
        status.totalServices = 3;
        return status;
    });

    page.onActivated();
    QVERIFY(page.findChild<QLabel*>(QStringLiteral("workflowStatusLabel")) != nullptr);
}

void CorePagesPlatformProvidersTest::systemSettings_uses_runtime_status_provider()
{
    SystemSettingsPageNew page(nullptr, []() {
        SystemSettingsPageNew::RuntimeStatusSnapshot status;
        status.frameworkReady = true;
        status.pluginCount = 3;
        status.readyServices = 3;
        status.totalServices = 3;
        status.dataDirectoryReadable = true;
        status.dicomDirectoryReadable = true;
        return status;
    });

    page.onActivated();
    QVERIFY(page.findChild<QLabel*>(QStringLiteral("systemRecommendationLabel")) != nullptr);
}

QTEST_MAIN(CorePagesPlatformProvidersTest)
#include "CorePagesPlatformProvidersTest.moc"
```

- [ ] **Step 2: 挂接页面 provider 测试，先确认构造签名还未支持注入**

```cmake
# tests/unit/CMakeLists.txt
add_executable(core_pages_platform_providers_test
    CorePagesPlatformProvidersTest.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/ModuleSelectionPage.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/SystemSettingsPage.cpp
    ${CMAKE_SOURCE_DIR}/UI/Forms/ModuleSelectionPage.ui
    ${CMAKE_SOURCE_DIR}/UI/Forms/SystemSettingsPage.ui
)

set_target_properties(core_pages_platform_providers_test PROPERTIES
    AUTOUIC_SEARCH_PATHS "${CMAKE_SOURCE_DIR}/UI/Forms"
)

target_include_directories(core_pages_platform_providers_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/UI/NewPages
    ${CMAKE_SOURCE_DIR}/UI/Forms
)

target_link_libraries(core_pages_platform_providers_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME core_pages_platform_providers_test
    COMMAND core_pages_platform_providers_test
)
```

Run:

```powershell
cmake --build build_x64 --config Release --target core_pages_platform_providers_test
```

Expected:
- 编译失败，提示 `ModuleSelectionPageNew` 或 `SystemSettingsPageNew` 还不支持 provider 构造

- [ ] **Step 3: 把页面状态读取改成 provider / facade 注入，移除页面内的 CTK 直连**

```cpp
// UI/NewPages/ModuleSelectionPage.h
public:
    using RuntimeStatusProvider = std::function<ModuleRuntimeStatus()>;

    explicit ModuleSelectionPageNew(QWidget* parent = nullptr, RuntimeStatusProvider runtimeStatusProvider = {});

private:
    RuntimeStatusProvider m_runtimeStatusProvider;
```

```cpp
// UI/NewPages/SystemSettingsPage.h
public:
    using RuntimeStatusProvider = std::function<RuntimeStatusSnapshot()>;

    explicit SystemSettingsPageNew(QWidget* parent = nullptr, RuntimeStatusProvider runtimeStatusProvider = {});

private:
    RuntimeStatusProvider m_runtimeStatusProvider;
```

```cpp
// UI/NewPages/ModuleSelectionPage.cpp
ModuleSelectionPageNew::ModuleSelectionPageNew(QWidget* parent, RuntimeStatusProvider runtimeStatusProvider)
    : BasePage(parent)
    , ui(new Ui::ModuleSelectionPage)
    , m_runtimeStatusProvider(std::move(runtimeStatusProvider))
{
    ui->setupUi(this);
}

ModuleSelectionPageNew::ModuleRuntimeStatus ModuleSelectionPageNew::collectRuntimeStatus() const
{
    if (m_runtimeStatusProvider) return m_runtimeStatusProvider();
    return {};
}
```

```cpp
// UI/NewPages/SystemSettingsPage.cpp
SystemSettingsPageNew::SystemSettingsPageNew(QWidget* parent, RuntimeStatusProvider runtimeStatusProvider)
    : BasePage(parent)
    , ui(new Ui::SystemSettingsPage)
    , m_runtimeStatusProvider(std::move(runtimeStatusProvider))
{
    ui->setupUi(this);
}

SystemSettingsPageNew::RuntimeStatusSnapshot SystemSettingsPageNew::collectRuntimeStatus() const
{
    if (m_runtimeStatusProvider) return m_runtimeStatusProvider();
    return {};
}
```

```cpp
// UI/NewPages/ManagementPage.h
class IdentityAppService;

public:
    explicit ManagementPageNew(QWidget* parent = nullptr, IdentityAppService* identityAppService = nullptr);

private:
    IdentityAppService* m_identityAppService;
```

```cpp
// UI/NewPages/DashboardPage.h
class ImagingAppService;
class NavigationAppService;

public:
    explicit DashboardPageNew(
        QWidget* parent = nullptr,
        ImagingAppService* imagingAppService = nullptr,
        NavigationAppService* navigationAppService = nullptr);

private:
    ImagingAppService* m_imagingAppService;
    NavigationAppService* m_navigationAppService;
```

```cpp
// UI/NewPages/DashboardPage.cpp
void DashboardPageNew::loadPatients()
{
    if (!m_imagingAppService || !m_imagingAppService->hasReadableStudy()) {
        clearPatientDetails();
        return;
    }

    setOverviewPatientSummary(m_imagingAppService->currentPatientName());
}

void DashboardPageNew::on_enterNavigationButton_clicked()
{
    if (!m_navigationAppService || !m_navigationAppService->ensureReady(QStringLiteral("org.medicalpro.registration_core"))) {
        showWarning(QStringLiteral("导航准备"), QStringLiteral("导航核心插件尚未就绪。"));
        return;
    }

    emit enterNavigationRequested(m_currentPatientId);
}
```

```cpp
// UI/MainInterfaceWidget.cpp
auto* identityAdapter = new LegacyUserManagementAdapter();
auto* imagingAdapter = new LegacyImagingAdapter();
auto* navigationAdapter = new LegacyNavigationAdapter();

auto* identityAppService = new IdentityAppService(identityAdapter);
auto* imagingAppService = new ImagingAppService(imagingAdapter);
auto* navigationAppService = new NavigationAppService(navigationAdapter);

m_moduleSelectionPage = new ModuleSelectionPageNew(this, [this]() { return buildModuleSelectionRuntimeStatus(); });
m_systemSettingsPage = new SystemSettingsPageNew(this, [this]() { return buildSystemSettingsRuntimeStatus(); });
m_managementPage = new ManagementPageNew(this, identityAppService);
m_dashboardPage = new DashboardPageNew(this, imagingAppService, navigationAppService);
```

- [ ] **Step 4: 运行页面 provider 测试，并做一次 CTK 直连扫描**

Run:

```powershell
cmake --build build_x64 --config Release --target core_pages_platform_providers_test NewPagesLib
ctest --test-dir build_x64 -C Release -R core_pages_platform_providers_test --output-on-failure
rg -n "CTKManager::instance\\(|getService<" UI\\NewPages\\WelcomePage.cpp UI\\NewPages\\ModuleSelectionPage.cpp UI\\NewPages\\SystemSettingsPage.cpp UI\\NewPages\\ManagementPage.cpp UI\\NewPages\\DashboardPage.cpp
```

Expected:
- `core_pages_platform_providers_test` 通过
- `WelcomePage.cpp` 保留 provider 模式
- `ModuleSelectionPage.cpp`、`SystemSettingsPage.cpp` 不再出现 `CTKManager::instance()`
- `ManagementPage.cpp`、`DashboardPage.cpp` 不再直接做 `getService<...>()`

- [ ] **Step 5: 提交核心页面迁移**

```powershell
git add UI/MainInterfaceWidget.h UI/MainInterfaceWidget.cpp UI/NewPages/WelcomePage.h UI/NewPages/WelcomePage.cpp UI/NewPages/ModuleSelectionPage.h UI/NewPages/ModuleSelectionPage.cpp UI/NewPages/SystemSettingsPage.h UI/NewPages/SystemSettingsPage.cpp UI/NewPages/ManagementPage.h UI/NewPages/ManagementPage.cpp UI/NewPages/DashboardPage.h UI/NewPages/DashboardPage.cpp tests/unit/CMakeLists.txt tests/unit/CorePagesPlatformProvidersTest.cpp
git commit -m "refactor: migrate core pages to platform providers"
```

---

### Task 9: 把主启动链切到 facade_mode / orchestrate_core

**Files:**
- Modify: `Framework/StartupOrchestrator.h`
- Modify: `Framework/StartupOrchestrator.cpp`
- Modify: `Framework/CTKManager.h`
- Modify: `Framework/CTKManager.cpp`
- Modify: `main.cpp`
- Modify: `tests/unit/PlatformStartupCoordinatorTest.cpp`

- [ ] **Step 1: 先扩展 coordinator 测试，锁定 observe_only / facade_mode / orchestrate_core 三档行为**

```cpp
// tests/unit/PlatformStartupCoordinatorTest.cpp
void PlatformStartupCoordinatorTest::ensureReady_starts_target_plugin_once();
void PlatformStartupCoordinatorTest::observe_only_does_not_start_any_plugin();

void PlatformStartupCoordinatorTest::observe_only_does_not_start_any_plugin()
{
    QStringList startedPlugins;
    PlatformStartupCoordinator coordinator(
        PlatformRuntimeMode::ObserveOnly,
        [&startedPlugins](const QString& pluginId) {
            startedPlugins.append(pluginId);
            return true;
        });

    QVERIFY(!coordinator.ensureReady(QStringLiteral("org.medicalpro.optical_tracking")));
    QVERIFY(startedPlugins.isEmpty());
}
```

- [ ] **Step 2: 先跑测试，确认主启动链切换能力尚未落实**

Run:

```powershell
cmake --build build_x64 --config Release --target platform_startup_coordinator_test
ctest --test-dir build_x64 -C Release -R platform_startup_coordinator_test --output-on-failure
```

Expected:
- 新增断言先失败，说明 `observe_only` 仍会被错误触发启动或行为未被编码

- [ ] **Step 3: 在主程序接入 runtime config + coordinator，并按模式切换启动链**

```cpp
// main.cpp
const auto runtimeConfig = PlatformRuntimeConfig::loadFromFile(
    QCoreApplication::applicationDirPath() + "/config/platform_runtime.json");

PlatformStartupCoordinator startupCoordinator(
    runtimeConfig.runtimeMode,
    [ctkManager](const QString& pluginId) {
        return ctkManager->startPlugin(pluginId);
    });
```

```cpp
// main.cpp
orchestrator->registerPhaseHandler(StartupPhase::CriticalPluginStart, [ctkManager, runtimeConfig](QApplication*) {
    if (runtimeConfig.runtimeMode == PlatformRuntimeMode::ObserveOnly) {
        return true;
    }

    for (const auto& pluginId : runtimeConfig.corePluginIds) {
        if (!ctkManager->startPlugin(pluginId)) return false;
    }
    return true;
});

orchestrator->registerPhaseHandler(StartupPhase::DeferredPluginStart, [runtimeConfig](QApplication*) {
    return runtimeConfig.runtimeMode == PlatformRuntimeMode::ObserveOnly;
});
```

```cpp
// Framework/StartupOrchestrator.h
public:
    void clearPhaseHandler(StartupPhase phase);
```

```cpp
// Framework/StartupOrchestrator.cpp
void StartupOrchestrator::clearPhaseHandler(StartupPhase phase)
{
    m_phaseHandlers.remove(phase);
}
```

```cpp
// Framework/CTKManager.h
public:
    bool hasInstalledPlugin(const QString& pluginName) const;
```

```cpp
// Framework/CTKManager.cpp
bool CTKManager::hasInstalledPlugin(const QString& pluginName) const
{
    return m_installedPluginNames.contains(pluginName);
}
```

- [ ] **Step 4: 构建主程序并验证模式切换链路**

Run:

```powershell
cmake --build build_x64 --config Release --target medicalpro platform_startup_coordinator_test
ctest --test-dir build_x64 -C Release -R platform_startup_coordinator_test --output-on-failure
```

Expected:
- `platform_startup_coordinator_test` 通过
- `medicalpro` 构建通过
- `observe_only` 模式下应用可启动但不主动接管核心启动
- 切到 `facade_mode` / `orchestrate_core` 后，核心启动集只按 `platform_runtime.json` 执行

- [ ] **Step 5: 提交主启动链切换**

```powershell
git add Framework/StartupOrchestrator.h Framework/StartupOrchestrator.cpp Framework/CTKManager.h Framework/CTKManager.cpp main.cpp tests/unit/PlatformStartupCoordinatorTest.cpp
git commit -m "refactor: route startup through platform runtime modes"
```

---

### Task 10: 更新治理文档、决策日志并完成最终验收

**Files:**
- Create: `docs/superpowers/tracking/platform-plugin-governance-matrix.md`
- Create: `docs/superpowers/tracking/platform-migration-decision-log.md`
- Modify: `docs/current_status_and_project_overview.md`
- Modify: `docs/superpowers/specs/2026-04-16-platform-kernel-governance-design.md`
- Modify: `docs/superpowers/plans/2026-04-16-platform-kernel-governance-implementation.md`

- [ ] **Step 1: 先写治理文档骨架，固定插件治理矩阵和迁移决策格式**

```md
<!-- docs/superpowers/tracking/platform-plugin-governance-matrix.md -->
# Platform Plugin Governance Matrix

| Plugin | Descriptor | Bootstrap | Startup Policy | Facade Owner | Legacy Adapter | UI Entry |
| --- | --- | --- | --- | --- | --- | --- |
| UserManagement | yes | core | eager | IdentityAppService | LegacyUserManagementAdapter | Welcome / Management |
| DicomViewer | yes | core | eager | ImagingAppService | LegacyImagingAdapter | Welcome / Dashboard |
| FourViewDisplay | yes | core | eager | ImagingAppService | LegacyImagingAdapter | Dashboard |
| RegistrationCore | yes | deferred | on_demand | NavigationAppService | LegacyNavigationAdapter | Navigation |
| OpticalTracking | yes | deferred | on_demand | NavigationAppService | LegacyNavigationAdapter | Navigation |
```

```md
<!-- docs/superpowers/tracking/platform-migration-decision-log.md -->
# Platform Migration Decision Log

## 2026-04-16
- 决策：第一阶段保留 CTK，仅新增平台治理层
- 原因：先解决初始化链路混乱、诊断缺失、页面直连 CTK 问题
- 影响：observe_only -> facade_mode -> orchestrate_core 三段推进
```

- [ ] **Step 2: 回写当前状态文档和设计稿中的实施结果链接**

```md
<!-- docs/current_status_and_project_overview.md -->
## Platform Kernel Governance

- 设计文档：`docs/superpowers/specs/2026-04-16-platform-kernel-governance-design.md`
- 实施计划：`docs/superpowers/plans/2026-04-16-platform-kernel-governance-implementation.md`
- 治理矩阵：`docs/superpowers/tracking/platform-plugin-governance-matrix.md`
- 决策日志：`docs/superpowers/tracking/platform-migration-decision-log.md`
```

```md
<!-- docs/superpowers/specs/2026-04-16-platform-kernel-governance-design.md -->
## Implementation Link

- 实施计划：`docs/superpowers/plans/2026-04-16-platform-kernel-governance-implementation.md`
- 治理矩阵：`docs/superpowers/tracking/platform-plugin-governance-matrix.md`
- 决策日志：`docs/superpowers/tracking/platform-migration-decision-log.md`
```

- [ ] **Step 3: 执行最终验收命令，锁定单测、运行时布局和 CTK 直连清理结果**

Run:

```powershell
cmake --build build_x64 --config Release --target medicalpro NewPagesLib
ctest --test-dir build_x64 -C Release --output-on-failure
rg -n "CTKManager::instance\\(|getService<" UI\\NewPages UI\\MainInterfaceWidget.cpp
ctest --test-dir build_x64 -C Release -R "runtime_artifact_layout_test|platform_descriptor_runtime_layout_test" --output-on-failure
```

Expected:
- 计划内新增单测全部通过
- 运行时 `plugins/descriptors/*.json` 和 `config/platform_runtime.json` 布局正确
- `UI/NewPages` 与 `UI/MainInterfaceWidget.cpp` 不再保留违规 CTK 直连

- [ ] **Step 4: 提交治理文档与最终验收结果**

```powershell
git add docs/current_status_and_project_overview.md docs/superpowers/specs/2026-04-16-platform-kernel-governance-design.md docs/superpowers/plans/2026-04-16-platform-kernel-governance-implementation.md docs/superpowers/tracking/platform-plugin-governance-matrix.md docs/superpowers/tracking/platform-migration-decision-log.md
git commit -m "docs: finalize platform governance rollout"
```

---

## Spec Coverage Check

- `设计背景 / 第一阶段范围与边界`：Task 2、Task 7、Task 9 固定核心插件范围、最小启动集和三档运行模式。
- `架构目标与核心原则 / 目标分层与目录结构`：Task 1、Task 3、Task 4、Task 6 完成 `Contracts / Kernel / Diagnostics / Facades / LegacyAdapters / CtkBridge` 分层落地。
- `生命周期、启动编排与状态模型`：Task 3、Task 4、Task 7、Task 9 固定 `discovered -> installed -> starting -> ready -> degraded -> failed` 语义和 trace 输出。
- `三大门面与上下文模型`：Task 6、Task 8 把 `IdentityAppService / ImagingAppService / NavigationAppService` 作为页面唯一正式入口。
- `插件描述文件与依赖图规则`：Task 1、Task 2、Task 3 负责 descriptor 契约、部署规则和依赖图校验。
- `兼容层策略与禁用边界`：Task 6、Task 8、Task 9 把 CTK 直连收敛到 `LegacyAdapters / CtkBridge`，并在 UI 层做扫描验收。
- `诊断输出与页面收口`：Task 4、Task 5 负责 `StartupTrace / PlatformDiagnosticSnapshot / PlatformCapabilitySnapshot` 和 `PlatformDiagnosticsPage`。
- `测试策略与验收标准`：Task 1 到 Task 10 每项都包含 failing test、build / ctest 命令和提交点，Task 10 负责最终验收矩阵。
- `迁移阶段划分与推荐实施顺序`：Task 4 对应 `observe_only`，Task 6 到 Task 8 对应 `facade_mode`，Task 7 与 Task 9 对应 `orchestrate_core`。
- `风险、回滚与文档机制`：Task 7、Task 9 固定按运行模式整段回退，Task 10 固定治理矩阵、决策日志和状态回写。

## Placeholder Scan

- [ ] 运行占位词扫描

Run:

```powershell
$planLines = Get-Content docs\superpowers\plans\2026-04-16-platform-kernel-governance-implementation.md
$cutLine = (($planLines | Select-String -Pattern "^## Placeholder Scan$").LineNumber | Select-Object -First 1) - 1
$planLines[0..($cutLine - 1)] | rg -n "TODO|TBD|待定|占位|后续补|类似 Task"
```

Expected:
- 无输出

- [ ] 如果命中任何占位词，立即在当前计划文件内补全，不允许把缺口留到执行阶段

## Type Consistency Check

- 运行模式统一使用：`PlatformRuntimeMode::ObserveOnly / FacadeMode / OrchestrateCore`
- 启动编排器统一使用：`PlatformStartupCoordinator`
- 诊断快照统一使用：`PlatformDiagnosticSnapshot`
- 能力快照统一使用：`PlatformCapabilitySnapshot`
- 结构化启动跟踪统一使用：`PlatformStartupTraceEntry`
- 页面接入统一使用：`SnapshotProvider`、`RuntimeStatusProvider`
- 门面端口统一使用：`IIdentityFacadePort / IImagingFacadePort / INavigationFacadePort`
- 页面禁止直连 CTK 的扫描目标统一使用：`CTKManager::instance()` 与 `getService<`
