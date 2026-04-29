# CTK Runtime Exit Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove `CTK runtime` from the product mainline by first detaching startup and UI from `CTKManager`, then migrating the core plugins to a platform-owned host model, then deleting CTK deployment and compatibility residue.

**Architecture:** Keep the current descriptor/runtime-governed startup truth, but move execution behind explicit platform host ports so `main.cpp`, `StartupOrchestrator`, and `MainWindow` stop knowing what CTK is. Introduce a temporary CTK-backed bridge that implements the new ports, then replace CTK plugin activators with a platform-owned service registry, module context, and event bus. Delete CTK build/runtime artifacts only after the core plugins and product entry no longer require them.

**Tech Stack:** C++, Qt 6, QtTest, CMake, existing `Framework` shared library, descriptor-driven startup flow, governance/runtime contract tests, Windows Release build in `build_x64`

## Execution Status

- Status: completed on `2026-04-27` for the planned scope of `CTK runtime exit from the product mainline`.
- Completed outcome:
  - Default product mainline now configures with `ENABLE_CTK_PLUGIN_FRAMEWORK=OFF`.
  - Product entry, startup orchestration, and `MainWindow` are detached from direct CTK runtime ownership for the default runtime path.
  - Core/UI plugins are migrated to platform module activators and the corresponding legacy CTK activator source files are deleted.
  - Release runtime artifact verification now rejects `EventAdmin`, `CTKPluginFramework.dll`, `CTK*.dll`, and plugin `.manifest` files.
  - Framework-linked unit tests now synchronize the current `Framework.dll` into their output directories so acceptance runs cannot be polluted by stale CTK-linked test-local binaries.
- Residual repository note:
  - Updated on `2026-04-28`: the bridge-only `LegacyCtkRuntimeBridge` and `CTKManager` residue has been deleted from the repository as post-plan cleanup.
  - Updated on `2026-04-28`: runtime-memory startup/diagnostics naming is now platform-neutral (`symbolicName`) across `main.cpp`, `PlatformStartupCoordinator`, runtime snapshots, lifecycle trace recording, diagnostics/UI consumers, descriptor/runtime plans, and `PlatformRuntimeConfig`.
  - Updated on `2026-04-28`: runtime descriptor schema cleanup is complete for the active product path; `PlatformDescriptorLoader` now requires `runtime.symbolic_name`, repository-owned plugin descriptors use `symbolic_name`, and the legacy JSON key `ctk_symbolic_name` is no longer accepted on the mainline path.
  - Updated on `2026-04-28`: diagnostics/runtime observation naming is now platform-neutral at the bridge edge as well; `PlatformRuntimeSnapshotCollector` replaces `CtkRuntimeSnapshotCollector`, and governed mismatch diagnostics now use `runtime_platform_state_mismatch` instead of `ctk_platform_state_mismatch`.
  - Updated on `2026-04-28`: remaining live UI bridge code has moved out of `Framework/Platform/CtkBridge/` into `Framework/Platform/UiBridge/`; `CoreUiRuntimeStatusProvider` and `NavigationPageServiceAccess` no longer keep CTK-specific path naming in the active source tree.
  - Updated on `2026-04-28`: remaining CTK residue after plan completion is now limited to historical docs and compatibility-language artifacts that no longer participate in the runtime mainline.
- Final acceptance commands:
  - `cmake -S . -B build_x64 -DENABLE_CTK_PLUGIN_FRAMEWORK=OFF -DBUILD_TESTING=ON`
  - `cmake --build build_x64 --config Release --target medicalpro platform_runtime_host_ports_contract_test runtime_host_detachment_contract_test platform_plugin_host_core_migration_contract_test platform_plugin_host_ui_migration_contract_test runtime_build_deployment_contract_test platform_plugin_build_governance_contract_test platform_runtime_host_adapter_platform_fallback_test platform_built_in_module_bootstrap_behavior_test runtime_console_log_policy_test`
  - `ctest --test-dir build_x64 -C Release -R "platform_runtime_host_ports_contract_test|runtime_host_detachment_contract_test|platform_plugin_host_core_migration_contract_test|platform_plugin_host_ui_migration_contract_test|runtime_build_deployment_contract_test|platform_plugin_build_governance_contract_test|platform_runtime_host_adapter_platform_fallback_test|platform_built_in_module_bootstrap_behavior_test|runtime_console_log_policy_test|runtime_artifact_layout_test|platform_descriptor_runtime_layout_test|plugin_truth_source_runtime_contract_test" --output-on-failure`

---

## Files And Responsibilities

- Create: `Framework/Platform/Contracts/platform_runtime_host_ports.h`
  - Define the host-side runtime, service-access, and event-bus interfaces consumed by product code.
- Create: `Framework/Platform/Contracts/platform_module_ports.h`
  - Define the platform-owned module activation contract that replaces `ctkPluginActivator`.
- Create: `Framework/Platform/Kernel/platform_service_registry.h`
  - Define a minimal registry for service instances keyed by service id and plugin id.
- Create: `Framework/Platform/Kernel/platform_service_registry.cpp`
  - Implement registration, lookup, and unregister logic used by the platform host.
- Create: `Framework/Platform/Kernel/platform_event_bus.h`
  - Define a platform-owned event bus abstraction and default implementation.
- Create: `Framework/Platform/Kernel/platform_event_bus.cpp`
  - Implement event publication used by migrated plugins that previously depended on `ctkEventAdmin`.
- Create: `Framework/Platform/Kernel/platform_plugin_host.h`
  - Define the host that starts, stops, and tracks platform modules without CTK.
- Create: `Framework/Platform/Kernel/platform_plugin_host.cpp`
  - Implement module registration, activation ordering, and service exposure.
- Create: `Framework/Platform/CtkBridge/ctk_runtime_host_adapter.h`
  - Provide the temporary CTK-backed implementation of the new runtime host ports.
- Create: `Framework/Platform/CtkBridge/ctk_runtime_host_adapter.cpp`
  - Translate new host-port calls to `CTKManager` during the bridge phase.
- Modify: `Framework/Platform/Contracts/PlatformUiPorts.h`
  - Point existing runtime-status and navigation consumers at host-port-based access where needed.
- Modify: `Framework/StartupOrchestrator.h`
  - Replace CTK-specific startup assumptions with platform-host-oriented phase ownership.
- Modify: `Framework/StartupOrchestrator.cpp`
  - Stop treating CTK as the startup contract owner and consume the runtime host port instead.
- Modify: `Framework/CTKManager.h`
  - Demote CTK manager to bridge-only responsibility and remove product-mainline APIs once the bridge is in place.
- Modify: `Framework/CTKManager.cpp`
  - Keep only bridge-facing CTK runtime behavior during migration, then delete remaining runtime-only residue.
- Modify: `main.cpp`
  - Compose the runtime host, stop directly driving `CTKManager`, and feed startup orchestration through the host ports.
- Modify: `mainwindow.h`
  - Remove `ctkPluginContext` and `ctkServiceReference` members in favor of host/service-access ports.
- Modify: `mainwindow.cpp`
  - Remove direct CTK service queries and use the new service-access port.
- Create: `Plugins/RegistrationCore/registration_core_module.h`
  - Define the platform-owned module entry for `RegistrationCore`.
- Create: `Plugins/RegistrationCore/registration_core_module.cpp`
  - Register `RegistrationServiceImpl` into the platform host instead of CTK.
- Create: `Plugins/Registration2D3D/registration_2d3d_module.h`
  - Define the platform-owned module entry for `Registration2D3D`.
- Create: `Plugins/Registration2D3D/registration_2d3d_module.cpp`
  - Register `Registration2D3DService` into the platform host.
- Create: `Plugins/OpticalTracking/optical_tracking_module.h`
  - Define the platform-owned module entry for `OpticalTracking`.
- Create: `Plugins/OpticalTracking/optical_tracking_module.cpp`
  - Register `OpticalTrackingServiceImpl` through the platform host.
- Create: `Plugins/DicomViewer/dicom_viewer_module.h`
  - Define the platform-owned module entry for `DicomViewer`.
- Create: `Plugins/DicomViewer/dicom_viewer_module.cpp`
  - Register `DicomViewerServiceImpl` through the platform host.
- Create: `Plugins/FourViewDisplay/four_view_display_module.h`
  - Define the platform-owned module entry for `FourViewDisplay`.
- Create: `Plugins/FourViewDisplay/four_view_display_module.cpp`
  - Register `FourViewDisplayServiceImpl` through the platform host.
- Create: `Plugins/UserManagement/user_management_module.h`
  - Define the platform-owned module entry for `UserManagement`.
- Create: `Plugins/UserManagement/user_management_module.cpp`
  - Register `UserManagementServiceImpl` through the platform host.
- Modify: `Plugins/RegistrationCore/RegistrationServiceImpl.h`
  - Replace CTK plugin-context ownership with platform service-registry access.
- Modify: `Plugins/RegistrationCore/RegistrationServiceImpl.cpp`
  - Resolve `Registration2D3DService` through the new registry instead of `ctkPluginContext`.
- Modify: `Plugins/Registration2D3D/Registration2D3DServiceImpl.h`
  - Accept platform host context instead of CTK context if currently needed.
- Modify: `Plugins/Registration2D3D/Registration2D3DServiceImpl.cpp`
  - Remove remaining CTK-only assumptions from the runtime path.
- Modify: `Plugins/OpticalTracking/OpticalTrackingServiceImpl.h`
  - Replace `ctkPluginContext` and `ctkServiceReference` members with host registry/service access.
- Modify: `Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp`
  - Resolve imaging/interaction dependencies through the platform registry.
- Modify: `Plugins/DicomViewer/DicomViewerServiceImpl.h`
  - Remove direct CTK context storage.
- Modify: `Plugins/DicomViewer/DicomViewerServiceImpl.cpp`
  - Resolve any runtime dependencies through the host context.
- Modify: `Plugins/DicomViewer/DicomViewerWidget.h`
  - Replace CTK context/EventAdmin members with host event-bus access.
- Modify: `Plugins/DicomViewer/DicomViewerWidget.cpp`
  - Remove `ctkEventAdmin` lookup and publish through the platform event bus.
- Modify: `Plugins/FourViewDisplay/FourViewDisplayServiceImpl.h`
  - Replace plugin-context storage with host context access.
- Modify: `Plugins/FourViewDisplay/FourViewDisplayServiceImpl.cpp`
  - Remove CTK lookup paths.
- Modify: `Plugins/UserManagement/UserManagementServiceImpl.h`
  - Replace `ctkEventAdmin*` ownership with platform event-bus access.
- Modify: `Plugins/UserManagement/UserManagementServiceImpl.cpp`
  - Publish session/user events through the platform event bus.
- Delete: `Plugins/RegistrationCore/RegistrationActivator.h`
  - Remove CTK activator after the module entry is in place.
- Delete: `Plugins/RegistrationCore/RegistrationActivator.cpp`
  - Remove CTK activator after the module entry is in place.
- Delete: `Plugins/OpticalTracking/OpticalTrackingActivator.h`
  - Remove CTK activator after the module entry is in place.
- Delete: `Plugins/OpticalTracking/OpticalTrackingActivator.cpp`
  - Remove CTK activator after the module entry is in place.
- Delete: `Plugins/DicomViewer/DicomViewerActivator.h`
  - Remove CTK activator after the module entry is in place.
- Delete: `Plugins/DicomViewer/DicomViewerActivator.cpp`
  - Remove CTK activator after the module entry is in place.
- Delete: `Plugins/FourViewDisplay/FourViewDisplayActivator.h`
  - Remove CTK activator after the module entry is in place.
- Delete: `Plugins/FourViewDisplay/FourViewDisplayActivator.cpp`
  - Remove CTK activator after the module entry is in place.
- Delete: `Plugins/UserManagement/UserManagementActivator.h`
  - Remove CTK activator after the module entry is in place.
- Delete: `Plugins/UserManagement/UserManagementActivator.cpp`
  - Remove CTK activator after the module entry is in place.
- Modify: `CMakeLists.txt`
  - Move CTK linkage off the product mainline, then remove CTK runtime deployment and plugin copies.
- Modify: `tests/CMakeLists.txt`
  - Register the new host-detachment, plugin-host-migration, and runtime-artifact contract tests.
- Modify: `tests/unit/CMakeLists.txt`
  - Add the new unit contract targets and remove CTK-only governance targets once obsolete.
- Create: `tests/unit/PlatformRuntimeHostPortsContractTest.cpp`
  - Lock the runtime-host contract shape.
- Create: `tests/unit/CtkRuntimeExitHostDetachmentContractTest.cpp`
  - Lock `main.cpp`, `mainwindow.cpp`, and `StartupOrchestrator` away from direct CTK usage.
- Create: `tests/unit/PlatformPluginHostCoreMigrationContractTest.cpp`
  - Lock `RegistrationCore`, `Registration2D3D`, and `OpticalTracking` away from `ctkPluginActivator`.
- Create: `tests/unit/PlatformPluginHostUiMigrationContractTest.cpp`
  - Lock `DicomViewer`, `FourViewDisplay`, and `UserManagement` away from `ctkPluginActivator` and `ctkEventAdmin`.
- Create: `tests/unit/CtkRuntimeExitArtifactGovernanceContractTest.cpp`
  - Lock build scripts and runtime layout away from `EventAdmin` and `CTKPluginFramework.dll`.
- Modify: `tests/runtime/verify_runtime_artifacts.cmake`
  - Remove CTK runtime artifact checks and assert the platform-host-only layout.
- Modify: `docs/superpowers/tracking/platform-plugin-governance-matrix.md`
  - Record the runtime-host ownership transition and CTK exit completion criteria.
- Modify: `docs/superpowers/tracking/platform-migration-decision-log.md`
  - Record the host model, plugin migration ordering, and CTK build cleanup decision.
- Modify: `docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md`
  - Replace remaining CTK consumer inventory with deletion-based exit status.
- Modify: `docs/current_status_and_project_overview.md`
  - Record each acceptance slice and the final CTK runtime exit acceptance commands.

## Phase Map

- Phase 1: `宿主脱钩`
  - Product entry, startup orchestration, and `MainWindow` stop depending on CTK types.
- Phase 2: `核心插件迁宿主`
  - Core plugins stop using `ctkPluginActivator`, `registerService`, `ctkPluginContext`, and `ctkEventAdmin`.
- Phase 3: `构建与运行时清场`
  - CTK deployment, build flags, legacy adapters, and runtime artifact checks are removed.

### Task 1: Introduce runtime-host contracts and lock the target shape

**Files:**
- Create: `Framework/Platform/Contracts/platform_runtime_host_ports.h`
- Create: `tests/unit/PlatformRuntimeHostPortsContractTest.cpp`
- Modify: `tests/unit/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write the failing contract test for the new runtime host ports**

```cpp
// tests/unit/PlatformRuntimeHostPortsContractTest.cpp
#include <QtTest/QtTest>

#include <QFile>

class PlatformRuntimeHostPortsContractTest : public QObject
{
    Q_OBJECT

private slots:
    void runtime_host_ports_define_runtime_service_and_event_bus_boundaries();

private:
    QString readSource(const QString& relativePath) const;
};

QString PlatformRuntimeHostPortsContractTest::readSource(const QString& relativePath) const
{
    QFile file(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QFAIL(qPrintable(QStringLiteral("failed to read %1").arg(relativePath)));
    }
    return QString::fromUtf8(file.readAll());
}

void PlatformRuntimeHostPortsContractTest::runtime_host_ports_define_runtime_service_and_event_bus_boundaries()
{
    const QString source = readSource(QStringLiteral("Framework/Platform/Contracts/platform_runtime_host_ports.h"));

    QVERIFY2(source.contains(QStringLiteral("class FRAMEWORK_EXPORT IPlatformRuntimeHostPort")),
        "missing IPlatformRuntimeHostPort");
    QVERIFY2(source.contains(QStringLiteral("class FRAMEWORK_EXPORT IPlatformServiceAccessPort")),
        "missing IPlatformServiceAccessPort");
    QVERIFY2(source.contains(QStringLiteral("class FRAMEWORK_EXPORT IPlatformEventBusPort")),
        "missing IPlatformEventBusPort");
    QVERIFY2(source.contains(QStringLiteral("virtual bool initialize(QApplication* app) = 0;")),
        "runtime host must own initialization");
    QVERIFY2(source.contains(QStringLiteral("virtual bool start() = 0;")),
        "runtime host must own start");
    QVERIFY2(source.contains(QStringLiteral("virtual bool stop() = 0;")),
        "runtime host must own stop");
    QVERIFY2(source.contains(QStringLiteral("virtual QObject* pluginEventSource() const = 0;")),
        "runtime host must expose an event source for legacy listeners");
    QVERIFY2(source.contains(QStringLiteral("virtual RegistrationService* registrationService() const = 0;")),
        "service access must expose RegistrationService");
    QVERIFY2(source.contains(QStringLiteral("virtual OpticalTrackingService* opticalTrackingService() const = 0;")),
        "service access must expose OpticalTrackingService");
    QVERIFY2(source.contains(QStringLiteral("virtual void publish(")),
        "event bus must expose publish()");
}

QTEST_APPLESS_MAIN(PlatformRuntimeHostPortsContractTest)
#include "PlatformRuntimeHostPortsContractTest.moc"
```

- [ ] **Step 2: Register the new test target**

```cmake
# tests/unit/CMakeLists.txt
add_executable(platform_runtime_host_ports_contract_test
    PlatformRuntimeHostPortsContractTest.cpp
)
target_link_libraries(platform_runtime_host_ports_contract_test PRIVATE Qt6::Test)
target_compile_definitions(platform_runtime_host_ports_contract_test PRIVATE
    MEDICALPRO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
add_test(NAME platform_runtime_host_ports_contract_test COMMAND platform_runtime_host_ports_contract_test)
```

- [ ] **Step 3: Run the new contract test and confirm the expected RED failure**

Run:

```bash
cmake --build build_x64 --config Release --target platform_runtime_host_ports_contract_test
ctest --test-dir build_x64 -C Release -R "platform_runtime_host_ports_contract_test" --output-on-failure
```

Expected: FAIL because `platform_runtime_host_ports.h` does not exist yet.

- [ ] **Step 4: Add the runtime-host contract header**

```cpp
// Framework/Platform/Contracts/platform_runtime_host_ports.h
#pragma once

#include "Framework/FrameworkExport.h"

#include <QApplication>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class PatientDatabaseService;
class MedicalImageCoreService;
class ImageInteractionService;
class MedicalViewerService;
class MedicalProcessingService;
class RegistrationService;
class OpticalTrackingService;

class FRAMEWORK_EXPORT IPlatformRuntimeHostPort
{
public:
    virtual ~IPlatformRuntimeHostPort() = default;
    virtual bool initialize(QApplication* app) = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool activatePlugin(const QString& pluginId) = 0;
    virtual bool isPluginStarted(const QString& pluginId) const = 0;
    virtual QString pluginState(const QString& pluginId) const = 0;
    virtual QStringList missingServices(const QStringList& requiredServices) const = 0;
    virtual QObject* pluginEventSource() const = 0;
};

class FRAMEWORK_EXPORT IPlatformServiceAccessPort
{
public:
    virtual ~IPlatformServiceAccessPort() = default;
    virtual PatientDatabaseService* patientDatabaseService() const = 0;
    virtual MedicalImageCoreService* medicalImageCoreService() const = 0;
    virtual ImageInteractionService* imageInteractionService() const = 0;
    virtual MedicalViewerService* medicalViewerService() const = 0;
    virtual MedicalProcessingService* medicalProcessingService() const = 0;
    virtual RegistrationService* registrationService() const = 0;
    virtual OpticalTrackingService* opticalTrackingService() const = 0;
};

class FRAMEWORK_EXPORT IPlatformEventBusPort
{
public:
    virtual ~IPlatformEventBusPort() = default;
    virtual void publish(const QString& topic, const QVariantMap& payload) = 0;
};
```

- [ ] **Step 5: Re-run the contract test and confirm it passes**

Run:

```bash
cmake --build build_x64 --config Release --target platform_runtime_host_ports_contract_test
ctest --test-dir build_x64 -C Release -R "platform_runtime_host_ports_contract_test" --output-on-failure
```

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add Framework/Platform/Contracts/platform_runtime_host_ports.h tests/unit/PlatformRuntimeHostPortsContractTest.cpp tests/unit/CMakeLists.txt tests/CMakeLists.txt
git commit -m "plan: add runtime host port contracts"
```

### Task 2: Complete Phase 1 host detachment for startup and `MainWindow`

**Files:**
- Create: `Framework/Platform/CtkBridge/ctk_runtime_host_adapter.h`
- Create: `Framework/Platform/CtkBridge/ctk_runtime_host_adapter.cpp`
- Modify: `Framework/StartupOrchestrator.h`
- Modify: `Framework/StartupOrchestrator.cpp`
- Modify: `Framework/Platform/Contracts/PlatformUiPorts.h`
- Modify: `main.cpp`
- Modify: `mainwindow.h`
- Modify: `mainwindow.cpp`
- Create: `tests/unit/CtkRuntimeExitHostDetachmentContractTest.cpp`
- Modify: `tests/unit/CMakeLists.txt`

- [ ] **Step 1: Write the failing host-detachment contract**

```cpp
// tests/unit/CtkRuntimeExitHostDetachmentContractTest.cpp
#include <QtTest/QtTest>

#include <QFile>

class CtkRuntimeExitHostDetachmentContractTest : public QObject
{
    Q_OBJECT

private slots:
    void product_entry_and_mainwindow_do_not_depend_on_ctk_types();

private:
    QString readSource(const QString& relativePath) const;
};

QString CtkRuntimeExitHostDetachmentContractTest::readSource(const QString& relativePath) const
{
    QFile file(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QFAIL(qPrintable(QStringLiteral("failed to read %1").arg(relativePath)));
    }
    return QString::fromUtf8(file.readAll());
}

void CtkRuntimeExitHostDetachmentContractTest::product_entry_and_mainwindow_do_not_depend_on_ctk_types()
{
    const QString mainSource = readSource(QStringLiteral("main.cpp"));
    const QString mainWindowHeader = readSource(QStringLiteral("mainwindow.h"));
    const QString mainWindowSource = readSource(QStringLiteral("mainwindow.cpp"));
    const QString orchestratorSource = readSource(QStringLiteral("Framework/StartupOrchestrator.cpp"));

    QVERIFY2(mainSource.contains(QStringLiteral("ctk_runtime_host_adapter.h")),
        "main.cpp must compose the runtime through ctk_runtime_host_adapter during Phase 1");
    QVERIFY2(!mainSource.contains(QStringLiteral("CTKManager::instance()")),
        "main.cpp still directly pulls CTKManager");
    QVERIFY2(!mainWindowHeader.contains(QStringLiteral("ctkPluginContext")),
        "mainwindow.h still stores ctkPluginContext");
    QVERIFY2(!mainWindowHeader.contains(QStringLiteral("ctkServiceReference")),
        "mainwindow.h still stores ctkServiceReference");
    QVERIFY2(!mainWindowSource.contains(QStringLiteral("getServiceReference")),
        "mainwindow.cpp still queries services through CTK");
    QVERIFY2(!mainWindowSource.contains(QStringLiteral("getService<")),
        "mainwindow.cpp still resolves services through CTK");
    QVERIFY2(!orchestratorSource.contains(QStringLiteral("CTK framework initialization")),
        "StartupOrchestrator still treats CTK as a first-class startup phase");
}

QTEST_APPLESS_MAIN(CtkRuntimeExitHostDetachmentContractTest)
#include "CtkRuntimeExitHostDetachmentContractTest.moc"
```

- [ ] **Step 2: Run the host-detachment contract and confirm the expected RED failure**

Run:

```bash
cmake --build build_x64 --config Release --target runtime_host_detachment_contract_test
ctest --test-dir build_x64 -C Release -R "runtime_host_detachment_contract_test" --output-on-failure
```

Expected: FAIL because `main.cpp`, `mainwindow.*`, and `StartupOrchestrator.cpp` still directly mention CTK.

- [ ] **Step 3: Implement the temporary CTK-backed runtime host adapter**

```cpp
// Framework/Platform/CtkBridge/ctk_runtime_host_adapter.h
#pragma once

#include "Framework/Platform/Contracts/platform_runtime_host_ports.h"

class CTKManager;

class PlatformRuntimeHostAdapter final
    : public IPlatformRuntimeHostPort
    , public IPlatformServiceAccessPort
    , public IPlatformEventBusPort
{
public:
    explicit PlatformRuntimeHostAdapter(CTKManager* manager);

    bool initialize(QApplication* app) override;
    bool start() override;
    bool stop() override;
    bool activatePlugin(const QString& pluginId) override;
    bool isPluginStarted(const QString& pluginId) const override;
    QString pluginState(const QString& pluginId) const override;
    QStringList missingServices(const QStringList& requiredServices) const override;
    QObject* pluginEventSource() const override;

    PatientDatabaseService* patientDatabaseService() const override;
    MedicalImageCoreService* medicalImageCoreService() const override;
    ImageInteractionService* imageInteractionService() const override;
    MedicalViewerService* medicalViewerService() const override;
    MedicalProcessingService* medicalProcessingService() const override;
    RegistrationService* registrationService() const override;
    OpticalTrackingService* opticalTrackingService() const override;

    void publish(const QString& topic, const QVariantMap& payload) override;

private:
    CTKManager* m_manager = nullptr;
};
```

- [ ] **Step 4: Refactor product entry and `MainWindow` to depend on the host ports**

```cpp
// main.cpp
#include "Framework/Platform/CtkBridge/ctk_runtime_host_adapter.h"

auto ctkManager = CTKManager::instance();
auto runtimeHost = std::make_shared<PlatformRuntimeHostAdapter>(ctkManager);

if (!runtimeHost->initialize(&app)) {
    throw std::runtime_error("Failed to initialize platform runtime host");
}

startupContext->runtimeHost = runtimeHost;
startupContext->serviceAccess = runtimeHost;
startupContext->eventBus = runtimeHost;
```

```cpp
// mainwindow.h
class IPlatformServiceAccessPort;

IPlatformServiceAccessPort* m_serviceAccess = nullptr;
```

```cpp
// mainwindow.cpp
void MainWindow::initializeServices()
{
    m_patientService = m_serviceAccess ? m_serviceAccess->patientDatabaseService() : nullptr;
    m_imageService = m_serviceAccess ? m_serviceAccess->medicalImageCoreService() : nullptr;
    m_imageInteractionService = m_serviceAccess ? m_serviceAccess->imageInteractionService() : nullptr;
    m_medicalViewerService = m_serviceAccess ? m_serviceAccess->medicalViewerService() : nullptr;
    m_medicalProcessingService = m_serviceAccess ? m_serviceAccess->medicalProcessingService() : nullptr;
    m_trackingService = m_serviceAccess ? m_serviceAccess->opticalTrackingService() : nullptr;
}
```

- [ ] **Step 5: Remove CTK-specific startup labels from `StartupOrchestrator`**

```cpp
// Framework/StartupOrchestrator.cpp
m_phases = {
    {StartupPhase::VTKInit, "VTK initialization", "Initialize the global VTK runtime", true, 500},
    {StartupPhase::QApplicationInit, "QApplication creation", "Create the Qt application instance", true, 100},
    {StartupPhase::SplashScreen, "Startup surface", "Prepare the startup surface", false, 50},
    {StartupPhase::MainUICreation, "Main interface creation", "Create and show the main interface", true, 300},
    {StartupPhase::CTKFrameworkInit, "Platform runtime initialization", "Initialize the active platform runtime host", true, 200},
    {StartupPhase::PluginInstallation, "Managed plugin preparation", "Prepare governed plugins through the active host", false, 500},
    {StartupPhase::CriticalPluginStart, "Core service activation", "Start required core services through the active host", true, 300},
    {StartupPhase::DeferredPluginStart, "Deferred service activation", "Start deferred services through the active host", false, 500},
    {StartupPhase::ServiceWarmup, "Service warmup", "Pre-initialize Python runtime and VTK components", false, 2000},
    {StartupPhase::Completed, "Startup complete", "All startup phases completed", true, 50}
};
```

- [ ] **Step 6: Re-run the host-detachment contract**

Run:

```bash
cmake --build build_x64 --config Release --target runtime_host_detachment_contract_test medicalpro
ctest --test-dir build_x64 -C Release -R "runtime_host_detachment_contract_test" --output-on-failure
```

Expected: PASS.

- [ ] **Step 7: Smoke-test startup without changing runtime behavior**

Run:

```bash
build_x64/Release/medicalpro.exe
```

Expected: startup reaches `Executing phase: Startup complete`, and the visible entry stays unchanged.

- [ ] **Step 8: Commit**

```bash
git add Framework/Platform/CtkBridge/ctk_runtime_host_adapter.h Framework/Platform/CtkBridge/ctk_runtime_host_adapter.cpp Framework/StartupOrchestrator.h Framework/StartupOrchestrator.cpp Framework/Platform/Contracts/PlatformUiPorts.h main.cpp mainwindow.h mainwindow.cpp tests/unit/CtkRuntimeExitHostDetachmentContractTest.cpp tests/unit/CMakeLists.txt
git commit -m "refactor: detach startup and mainwindow from direct ctk usage"
```

### Task 3: Build the platform plugin host and migrate `RegistrationCore`, `Registration2D3D`, and `OpticalTracking`

**Files:**
- Create: `Framework/Platform/Contracts/platform_module_ports.h`
- Create: `Framework/Platform/Kernel/platform_service_registry.h`
- Create: `Framework/Platform/Kernel/platform_service_registry.cpp`
- Create: `Framework/Platform/Kernel/platform_event_bus.h`
- Create: `Framework/Platform/Kernel/platform_event_bus.cpp`
- Create: `Framework/Platform/Kernel/platform_plugin_host.h`
- Create: `Framework/Platform/Kernel/platform_plugin_host.cpp`
- Create: `Plugins/RegistrationCore/registration_core_module.h`
- Create: `Plugins/RegistrationCore/registration_core_module.cpp`
- Create: `Plugins/Registration2D3D/registration_2d3d_module.h`
- Create: `Plugins/Registration2D3D/registration_2d3d_module.cpp`
- Create: `Plugins/OpticalTracking/optical_tracking_module.h`
- Create: `Plugins/OpticalTracking/optical_tracking_module.cpp`
- Modify: `Plugins/RegistrationCore/RegistrationServiceImpl.h`
- Modify: `Plugins/RegistrationCore/RegistrationServiceImpl.cpp`
- Modify: `Plugins/Registration2D3D/Registration2D3DServiceImpl.h`
- Modify: `Plugins/Registration2D3D/Registration2D3DServiceImpl.cpp`
- Modify: `Plugins/OpticalTracking/OpticalTrackingServiceImpl.h`
- Modify: `Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp`
- Delete: `Plugins/RegistrationCore/RegistrationActivator.h`
- Delete: `Plugins/RegistrationCore/RegistrationActivator.cpp`
- Delete: `Plugins/OpticalTracking/OpticalTrackingActivator.h`
- Delete: `Plugins/OpticalTracking/OpticalTrackingActivator.cpp`
- Create: `tests/unit/PlatformPluginHostCoreMigrationContractTest.cpp`
- Modify: `tests/unit/CMakeLists.txt`

- [ ] **Step 1: Write the failing core-migration contract**

```cpp
// tests/unit/PlatformPluginHostCoreMigrationContractTest.cpp
#include <QtTest/QtTest>

#include <QFile>
#include <QFileInfo>

class PlatformPluginHostCoreMigrationContractTest : public QObject
{
    Q_OBJECT

private slots:
    void core_plugins_no_longer_use_ctk_activators_or_plugin_context();

private:
    QString readSource(const QString& relativePath) const;
};

QString PlatformPluginHostCoreMigrationContractTest::readSource(const QString& relativePath) const
{
    QFile file(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QFAIL(qPrintable(QStringLiteral("failed to read %1").arg(relativePath)));
    }
    return QString::fromUtf8(file.readAll());
}

void PlatformPluginHostCoreMigrationContractTest::core_plugins_no_longer_use_ctk_activators_or_plugin_context()
{
    QVERIFY2(QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Plugins/RegistrationCore/registration_core_module.cpp")).exists(),
        "registration_core_module.cpp must exist");
    QVERIFY2(QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Plugins/Registration2D3D/registration_2d3d_module.cpp")).exists(),
        "registration_2d3d_module.cpp must exist");
    QVERIFY2(QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Plugins/OpticalTracking/optical_tracking_module.cpp")).exists(),
        "optical_tracking_module.cpp must exist");

    const QString registrationSource = readSource(QStringLiteral("Plugins/RegistrationCore/RegistrationServiceImpl.cpp"));
    const QString trackingHeader = readSource(QStringLiteral("Plugins/OpticalTracking/OpticalTrackingServiceImpl.h"));

    QVERIFY2(!QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Plugins/RegistrationCore/RegistrationActivator.cpp")).exists(),
        "RegistrationActivator.cpp still exists");
    QVERIFY2(!QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Plugins/OpticalTracking/OpticalTrackingActivator.cpp")).exists(),
        "OpticalTrackingActivator.cpp still exists");
    QVERIFY2(!registrationSource.contains(QStringLiteral("ctkServiceReference")),
        "RegistrationServiceImpl.cpp still uses ctkServiceReference");
    QVERIFY2(!trackingHeader.contains(QStringLiteral("ctkPluginContext")),
        "OpticalTrackingServiceImpl.h still stores ctkPluginContext");
}

QTEST_APPLESS_MAIN(PlatformPluginHostCoreMigrationContractTest)
#include "PlatformPluginHostCoreMigrationContractTest.moc"
```

- [ ] **Step 2: Run the core-migration contract and confirm the expected RED failure**

Run:

```bash
cmake --build build_x64 --config Release --target platform_plugin_host_core_migration_contract_test
ctest --test-dir build_x64 -C Release -R "platform_plugin_host_core_migration_contract_test" --output-on-failure
```

Expected: FAIL because the CTK activators and plugin-context-based implementations still exist.

- [ ] **Step 3: Add the platform-owned module and registry primitives**

```cpp
// Framework/Platform/Contracts/platform_module_ports.h
#pragma once

#include "Framework/FrameworkExport.h"

#include <QObject>
#include <QString>

class PlatformServiceRegistry;
class IPlatformEventBusPort;

struct PlatformModuleContext
{
    PlatformServiceRegistry* serviceRegistry = nullptr;
    IPlatformEventBusPort* eventBus = nullptr;
};

class FRAMEWORK_EXPORT IPlatformModuleActivator
{
public:
    virtual ~IPlatformModuleActivator() = default;
    virtual QString pluginId() const = 0;
    virtual bool start(PlatformModuleContext& context) = 0;
    virtual void stop(PlatformModuleContext& context) = 0;
};
```

```cpp
// Framework/Platform/Kernel/platform_service_registry.h
class FRAMEWORK_EXPORT PlatformServiceRegistry
{
public:
    void registerService(const QString& pluginId, const QString& serviceId, QObject* service);
    QObject* service(const QString& serviceId) const;
    void unregisterPlugin(const QString& pluginId);
};
```

- [ ] **Step 4: Migrate `RegistrationCore` and `Registration2D3D` to the platform host**

```cpp
// Plugins/RegistrationCore/registration_core_module.cpp
bool RegistrationCoreModule::start(PlatformModuleContext& context)
{
    m_service.reset(new RegistrationServiceImpl());
    m_service->setServiceRegistry(context.serviceRegistry);
    context.serviceRegistry->registerService(pluginId(), QStringLiteral("RegistrationService"), m_service.get());
    return true;
}
```

```cpp
// Plugins/RegistrationCore/RegistrationServiceImpl.cpp
Registration2D3DService* RegistrationServiceImpl::getRegistration2D3DService()
{
    if (!m_serviceRegistry) return nullptr;
    return qobject_cast<Registration2D3DService*>(m_serviceRegistry->service(QStringLiteral("Registration2D3DService")));
}
```

- [ ] **Step 5: Migrate `OpticalTracking` to the platform host**

```cpp
// Plugins/OpticalTracking/optical_tracking_module.cpp
bool OpticalTrackingModule::start(PlatformModuleContext& context)
{
    m_service.reset(new OpticalTrackingServiceImpl());
    m_service->setServiceRegistry(context.serviceRegistry);
    m_service->startService();
    context.serviceRegistry->registerService(pluginId(), QStringLiteral("OpticalTrackingService"), m_service.get());
    return true;
}
```

```cpp
// Plugins/OpticalTracking/OpticalTrackingServiceImpl.h
PlatformServiceRegistry* m_serviceRegistry = nullptr;
```

- [ ] **Step 6: Re-run the core-migration contract**

Run:

```bash
cmake --build build_x64 --config Release --target platform_plugin_host_core_migration_contract_test medicalpro
ctest --test-dir build_x64 -C Release -R "platform_plugin_host_core_migration_contract_test" --output-on-failure
```

Expected: PASS.

- [ ] **Step 7: Run focused runtime verification for registration and tracking**

Run:

```bash
ctest --test-dir build_x64 -C Release -R "platform_plugin_host_core_migration_contract_test|startup_welcome_entry_source_contract_test" --output-on-failure
```

Expected: PASS, and startup still reaches the in-app welcome entry.

- [ ] **Step 8: Commit**

```bash
git add Framework/Platform/Contracts/platform_module_ports.h Framework/Platform/Kernel/platform_service_registry.h Framework/Platform/Kernel/platform_service_registry.cpp Framework/Platform/Kernel/platform_event_bus.h Framework/Platform/Kernel/platform_event_bus.cpp Framework/Platform/Kernel/platform_plugin_host.h Framework/Platform/Kernel/platform_plugin_host.cpp Plugins/RegistrationCore/registration_core_module.h Plugins/RegistrationCore/registration_core_module.cpp Plugins/Registration2D3D/registration_2d3d_module.h Plugins/Registration2D3D/registration_2d3d_module.cpp Plugins/OpticalTracking/optical_tracking_module.h Plugins/OpticalTracking/optical_tracking_module.cpp Plugins/RegistrationCore/RegistrationServiceImpl.h Plugins/RegistrationCore/RegistrationServiceImpl.cpp Plugins/Registration2D3D/Registration2D3DServiceImpl.h Plugins/Registration2D3D/Registration2D3DServiceImpl.cpp Plugins/OpticalTracking/OpticalTrackingServiceImpl.h Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp tests/unit/PlatformPluginHostCoreMigrationContractTest.cpp tests/unit/CMakeLists.txt
git commit -m "refactor: migrate registration and tracking plugins to platform host"
```

### Task 4: Complete Phase 2 by migrating `DicomViewer`, `FourViewDisplay`, and `UserManagement`

**Files:**
- Create: `Plugins/DicomViewer/dicom_viewer_module.h`
- Create: `Plugins/DicomViewer/dicom_viewer_module.cpp`
- Create: `Plugins/FourViewDisplay/four_view_display_module.h`
- Create: `Plugins/FourViewDisplay/four_view_display_module.cpp`
- Create: `Plugins/UserManagement/user_management_module.h`
- Create: `Plugins/UserManagement/user_management_module.cpp`
- Modify: `Plugins/DicomViewer/DicomViewerServiceImpl.h`
- Modify: `Plugins/DicomViewer/DicomViewerServiceImpl.cpp`
- Modify: `Plugins/DicomViewer/DicomViewerWidget.h`
- Modify: `Plugins/DicomViewer/DicomViewerWidget.cpp`
- Modify: `Plugins/FourViewDisplay/FourViewDisplayServiceImpl.h`
- Modify: `Plugins/FourViewDisplay/FourViewDisplayServiceImpl.cpp`
- Modify: `Plugins/UserManagement/UserManagementServiceImpl.h`
- Modify: `Plugins/UserManagement/UserManagementServiceImpl.cpp`
- Delete: `Plugins/DicomViewer/DicomViewerActivator.h`
- Delete: `Plugins/DicomViewer/DicomViewerActivator.cpp`
- Delete: `Plugins/FourViewDisplay/FourViewDisplayActivator.h`
- Delete: `Plugins/FourViewDisplay/FourViewDisplayActivator.cpp`
- Delete: `Plugins/UserManagement/UserManagementActivator.h`
- Delete: `Plugins/UserManagement/UserManagementActivator.cpp`
- Create: `tests/unit/PlatformPluginHostUiMigrationContractTest.cpp`
- Modify: `tests/unit/CMakeLists.txt`

- [ ] **Step 1: Write the failing UI-plugin migration contract**

```cpp
// tests/unit/PlatformPluginHostUiMigrationContractTest.cpp
#include <QtTest/QtTest>

#include <QFile>
#include <QFileInfo>

class PlatformPluginHostUiMigrationContractTest : public QObject
{
    Q_OBJECT

private slots:
    void ui_plugins_no_longer_use_ctk_activator_or_event_admin();

private:
    QString readSource(const QString& relativePath) const;
};

QString PlatformPluginHostUiMigrationContractTest::readSource(const QString& relativePath) const
{
    QFile file(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QFAIL(qPrintable(QStringLiteral("failed to read %1").arg(relativePath)));
    }
    return QString::fromUtf8(file.readAll());
}

void PlatformPluginHostUiMigrationContractTest::ui_plugins_no_longer_use_ctk_activator_or_event_admin()
{
    const QString dicomWidgetSource = readSource(QStringLiteral("Plugins/DicomViewer/DicomViewerWidget.cpp"));
    const QString userServiceHeader = readSource(QStringLiteral("Plugins/UserManagement/UserManagementServiceImpl.h"));

    QVERIFY2(!QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Plugins/DicomViewer/DicomViewerActivator.cpp")).exists(),
        "DicomViewerActivator.cpp still exists");
    QVERIFY2(!QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Plugins/FourViewDisplay/FourViewDisplayActivator.cpp")).exists(),
        "FourViewDisplayActivator.cpp still exists");
    QVERIFY2(!QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Plugins/UserManagement/UserManagementActivator.cpp")).exists(),
        "UserManagementActivator.cpp still exists");
    QVERIFY2(!dicomWidgetSource.contains(QStringLiteral("ctkEventAdmin")),
        "DicomViewerWidget.cpp still uses ctkEventAdmin");
    QVERIFY2(!userServiceHeader.contains(QStringLiteral("ctkEventAdmin")),
        "UserManagementServiceImpl.h still stores ctkEventAdmin");
}

QTEST_APPLESS_MAIN(PlatformPluginHostUiMigrationContractTest)
#include "PlatformPluginHostUiMigrationContractTest.moc"
```

- [ ] **Step 2: Run the UI-plugin migration contract and confirm the expected RED failure**

Run:

```bash
cmake --build build_x64 --config Release --target platform_plugin_host_ui_migration_contract_test
ctest --test-dir build_x64 -C Release -R "platform_plugin_host_ui_migration_contract_test" --output-on-failure
```

Expected: FAIL because the activators and `ctkEventAdmin` usage still exist.

- [ ] **Step 3: Migrate `DicomViewer` and `FourViewDisplay` to the platform host**

```cpp
// Plugins/DicomViewer/dicom_viewer_module.cpp
bool DicomViewerModule::start(PlatformModuleContext& context)
{
    m_service.reset(new DicomViewerServiceImpl());
    m_service->setServiceRegistry(context.serviceRegistry);
    m_service->setEventBus(context.eventBus);
    if (!m_service->initialize()) return false;
    context.serviceRegistry->registerService(pluginId(), QStringLiteral("DicomViewerService"), m_service.get());
    return true;
}
```

```cpp
// Plugins/FourViewDisplay/four_view_display_module.cpp
bool FourViewDisplayModule::start(PlatformModuleContext& context)
{
    m_service.reset(new FourViewDisplayServiceImpl(nullptr));
    m_service->setServiceRegistry(context.serviceRegistry);
    context.serviceRegistry->registerService(pluginId(), QStringLiteral("FourViewDisplayService"), m_service.get());
    return true;
}
```

- [ ] **Step 4: Migrate `UserManagement` off `ctkEventAdmin`**

```cpp
// Plugins/UserManagement/UserManagementServiceImpl.h
IPlatformEventBusPort* m_eventBus = nullptr;
```

```cpp
// Plugins/UserManagement/UserManagementServiceImpl.cpp
void UserManagementServiceImpl::publishSessionEvent(const QString& topic, const QVariantMap& payload)
{
    if (!m_eventBus) return;
    m_eventBus->publish(topic, payload);
}
```

- [ ] **Step 5: Re-run the UI-plugin migration contract**

Run:

```bash
cmake --build build_x64 --config Release --target platform_plugin_host_ui_migration_contract_test medicalpro
ctest --test-dir build_x64 -C Release -R "platform_plugin_host_ui_migration_contract_test" --output-on-failure
```

Expected: PASS.

- [ ] **Step 6: Run a focused startup smoke for UI/service access**

Run:

```bash
ctest --test-dir build_x64 -C Release -R "platform_plugin_host_ui_migration_contract_test|startup_welcome_entry_source_contract_test|welcome_page_bootstrap_state_test" --output-on-failure
```

Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add Plugins/DicomViewer/dicom_viewer_module.h Plugins/DicomViewer/dicom_viewer_module.cpp Plugins/FourViewDisplay/four_view_display_module.h Plugins/FourViewDisplay/four_view_display_module.cpp Plugins/UserManagement/user_management_module.h Plugins/UserManagement/user_management_module.cpp Plugins/DicomViewer/DicomViewerServiceImpl.h Plugins/DicomViewer/DicomViewerServiceImpl.cpp Plugins/DicomViewer/DicomViewerWidget.h Plugins/DicomViewer/DicomViewerWidget.cpp Plugins/FourViewDisplay/FourViewDisplayServiceImpl.h Plugins/FourViewDisplay/FourViewDisplayServiceImpl.cpp Plugins/UserManagement/UserManagementServiceImpl.h Plugins/UserManagement/UserManagementServiceImpl.cpp tests/unit/PlatformPluginHostUiMigrationContractTest.cpp tests/unit/CMakeLists.txt
git commit -m "refactor: migrate ui and identity plugins to platform host"
```

### Task 5: Complete Phase 3 by removing CTK build deployment and runtime residue

**Files:**
- Modify: `Framework/CTKManager.h`
- Modify: `Framework/CTKManager.cpp`
- Modify: `Framework/Platform/CtkBridge/CoreUiRuntimeStatusProvider.h`
- Modify: `Framework/Platform/CtkBridge/CoreUiRuntimeStatusProvider.cpp`
- Delete: `Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.h`
- Delete: `Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.cpp`
- Delete: `Framework/Platform/LegacyAdapters/LegacyCoreUiRuntimeAdapter.h`
- Delete: `Framework/Platform/LegacyAdapters/LegacyCoreUiRuntimeAdapter.cpp`
- Delete: `Framework/Platform/LegacyAdapters/LegacyNavigationPageServiceAdapter.h`
- Delete: `Framework/Platform/LegacyAdapters/LegacyNavigationPageServiceAdapter.cpp`
- Modify: `CMakeLists.txt`
- Create: `tests/unit/CtkRuntimeExitArtifactGovernanceContractTest.cpp`
- Modify: `tests/runtime/verify_runtime_artifacts.cmake`
- Modify: `tests/unit/CMakeLists.txt`

- [ ] **Step 1: Write the failing runtime-artifact governance contract**

```cpp
// tests/unit/CtkRuntimeExitArtifactGovernanceContractTest.cpp
#include <QtTest/QtTest>

#include <QFile>

class CtkRuntimeExitArtifactGovernanceContractTest : public QObject
{
    Q_OBJECT

private slots:
    void build_and_runtime_layout_do_not_deploy_ctk_runtime_artifacts();

private:
    QString readSource(const QString& relativePath) const;
};

QString CtkRuntimeExitArtifactGovernanceContractTest::readSource(const QString& relativePath) const
{
    QFile file(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QFAIL(qPrintable(QStringLiteral("failed to read %1").arg(relativePath)));
    }
    return QString::fromUtf8(file.readAll());
}

void CtkRuntimeExitArtifactGovernanceContractTest::build_and_runtime_layout_do_not_deploy_ctk_runtime_artifacts()
{
    const QString rootCMake = readSource(QStringLiteral("CMakeLists.txt"));
    const QString runtimeVerify = readSource(QStringLiteral("tests/runtime/verify_runtime_artifacts.cmake"));

    QVERIFY2(!rootCMake.contains(QStringLiteral("liborg_commontk_eventadmin.dll")),
        "CMakeLists.txt still deploys EventAdmin");
    QVERIFY2(!rootCMake.contains(QStringLiteral("CTKPluginFramework.dll")),
        "CMakeLists.txt still deploys CTKPluginFramework.dll");
    QVERIFY2(!rootCMake.contains(QStringLiteral("CTK*.dll")),
        "CMakeLists.txt still deploys CTK runtime DLLs");
    QVERIFY2(!runtimeVerify.contains(QStringLiteral("CTKPluginFramework.dll")),
        "runtime artifact verification still expects CTKPluginFramework.dll");
}

QTEST_APPLESS_MAIN(CtkRuntimeExitArtifactGovernanceContractTest)
#include "CtkRuntimeExitArtifactGovernanceContractTest.moc"
```

- [ ] **Step 2: Run the runtime-artifact contract and confirm the expected RED failure**

Run:

```bash
cmake --build build_x64 --config Release --target ctk_runtime_exit_artifact_governance_contract_test
ctest --test-dir build_x64 -C Release -R "ctk_runtime_exit_artifact_governance_contract_test" --output-on-failure
```

Expected: FAIL because CTK deployment and runtime checks still exist.

- [ ] **Step 3: Remove CTK deployment from the build and runtime verification**

```cmake
# CMakeLists.txt
target_compile_definitions(medicalpro PRIVATE
    MEDICALPRO_MAIN_APPLICATION
)

# Delete the post-build copies for:
# - liborg_commontk_eventadmin.dll
# - CTKPluginFramework.dll
# - CTK*.dll runtime copies
```

```cmake
# tests/runtime/verify_runtime_artifacts.cmake
set(expected_runtime_files
    "medicalpro.exe"
    "plugins/descriptors/RegistrationCore.json"
    "plugins/descriptors/OpticalTracking.json"
)
```

- [ ] **Step 4: Delete CTK-only bridge and legacy adapter residue**

```cpp
// Keep CoreUiRuntimeStatusProvider, but make it consume the platform runtime host.
observation.frameworkReady = runtimeHost->start();
observation.loadedPlugins = platformStateStore->governedPluginIds();
```

- [ ] **Step 5: Re-run the runtime-artifact contract**

Run:

```bash
cmake --build build_x64 --config Release --target ctk_runtime_exit_artifact_governance_contract_test medicalpro
ctest --test-dir build_x64 -C Release -R "ctk_runtime_exit_artifact_governance_contract_test" --output-on-failure
```

Expected: PASS.

- [ ] **Step 6: Run runtime artifact verification**

Run:

```bash
ctest --test-dir build_x64 -C Release -R "ctk_runtime_exit_artifact_governance_contract_test|runtime_artifact_layout_test|platform_descriptor_runtime_layout_test" --output-on-failure
```

Expected: PASS, and the Release layout no longer contains CTK runtime artifacts.

- [ ] **Step 7: Commit**

```bash
git add Framework/CTKManager.h Framework/CTKManager.cpp Framework/Platform/CtkBridge/CoreUiRuntimeStatusProvider.h Framework/Platform/CtkBridge/CoreUiRuntimeStatusProvider.cpp CMakeLists.txt tests/runtime/verify_runtime_artifacts.cmake tests/unit/CtkRuntimeExitArtifactGovernanceContractTest.cpp tests/unit/CMakeLists.txt
git commit -m "build: remove ctk runtime deployment and legacy bridge residue"
```

### Task 6: Update governance docs and run the full CTK-exit acceptance suite

**Files:**
- Modify: `docs/superpowers/tracking/platform-plugin-governance-matrix.md`
- Modify: `docs/superpowers/tracking/platform-migration-decision-log.md`
- Modify: `docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md`
- Modify: `docs/current_status_and_project_overview.md`

- [ ] **Step 1: Record the completed runtime-host ownership transition**

```md
### 2026-04-23 CTK Runtime Exit Acceptance

- Product startup and `MainWindow` no longer directly depend on CTK runtime types.
- `RegistrationCore`, `Registration2D3D`, `OpticalTracking`, `DicomViewer`, `FourViewDisplay`, and `UserManagement` now start through the platform host instead of `ctkPluginActivator`.
- Build and runtime deployment no longer ship `EventAdmin`, `CTKPluginFramework.dll`, or CTK runtime DLLs.
```

- [ ] **Step 2: Run the full acceptance build**

Run:

```bash
cmake -S . -B build_x64 -DENABLE_PLUGIN_REGISTRATION_CORE=ON -DBUILD_TESTING=ON
cmake --build build_x64 --config Release --target medicalpro platform_runtime_host_ports_contract_test runtime_host_detachment_contract_test platform_plugin_host_core_migration_contract_test platform_plugin_host_ui_migration_contract_test ctk_runtime_exit_artifact_governance_contract_test
```

Expected: PASS.

- [ ] **Step 3: Run the full acceptance suite**

Run:

```bash
ctest --test-dir build_x64 -C Release -R "platform_runtime_host_ports_contract_test|runtime_host_detachment_contract_test|platform_plugin_host_core_migration_contract_test|platform_plugin_host_ui_migration_contract_test|ctk_runtime_exit_artifact_governance_contract_test|startup_welcome_entry_source_contract_test|welcome_page_bootstrap_state_test|runtime_artifact_layout_test|platform_descriptor_runtime_layout_test" --output-on-failure
```

Expected: PASS.

- [ ] **Step 4: Run one manual startup smoke**

Run:

```bash
build_x64/Release/medicalpro.exe
```

Expected: the app reaches `Executing phase: Startup complete`, the welcome entry renders, and no CTK runtime DLL is required beside the executable.

- [ ] **Step 5: Commit**

```bash
git add docs/superpowers/tracking/platform-plugin-governance-matrix.md docs/superpowers/tracking/platform-migration-decision-log.md docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md docs/current_status_and_project_overview.md
git commit -m "docs: record ctk runtime exit acceptance"
```

## Exit Criteria

- `main.cpp`, `mainwindow.cpp`, and `mainwindow.h` no longer mention `ctkPluginContext`, `ctkServiceReference`, `getServiceReference`, or direct `CTKManager::instance()` product-mainline ownership.
- `RegistrationCore`, `Registration2D3D`, `OpticalTracking`, `DicomViewer`, `FourViewDisplay`, and `UserManagement` no longer contain `ctkPluginActivator`, `registerService`, `ctkPluginContext`, or `ctkEventAdmin` in their active runtime path.
- `CMakeLists.txt` no longer deploys `liborg_commontk_eventadmin.dll`, `CTKPluginFramework.dll`, or `CTK*.dll`.
- Runtime artifact verification passes without CTK runtime files in the Release layout.
- Governance docs and current-status docs record `CTK runtime exit` as complete.
