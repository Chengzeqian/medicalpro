# Ankle Navigation Architecture Remediation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 收口踝关节导航主链的启动链、`NavigationPage` 职责和 VTK 页面宿主边界，为后三个创新点的持续开发提供稳定底座。

**Architecture:** 基于现有 `Framework/Platform/Bootstrap`、`PlatformStartupCoordinator` 和 `StartupOrchestrator` 继续演进，而不是另起一套启动框架。页面层新增轻量协作者，把 `NavigationPage` 降为页面壳，把 VTK 嵌入生命周期统一收口到页面级 host/bridge。

**Tech Stack:** C++20, Qt Widgets, Qt Test, VTK, existing `Framework/Platform` runtime host and startup orchestration

---

## File Structure

### New Files

- `Framework/Platform/Bootstrap/startup_phase_registrar.h`
- `Framework/Platform/Bootstrap/startup_phase_registrar.cpp`
  - 负责把 `PlatformRuntimeInit / PluginInstallation / CorePluginStart / DeferredPluginStart / ServiceWarmup` phase 注册到 `StartupOrchestrator`
- `Framework/Platform/Bootstrap/startup_ui_coordinator.h`
- `Framework/Platform/Bootstrap/startup_ui_coordinator.cpp`
  - 负责 startup shell、主界面和失败兜底提示之间的 UI 协调
- `UI/NewPages/Navigation/navigation_workflow_context.h`
- `UI/NewPages/Navigation/navigation_workflow_context.cpp`
  - 负责导航页病例上下文、阶段状态和路径快照
- `UI/NewPages/Navigation/navigation_service_bundle.h`
- `UI/NewPages/Navigation/navigation_service_bundle.cpp`
  - 负责统一暴露导航页依赖的外部服务
- `UI/NewPages/Navigation/navigation_workflow_coordinator.h`
- `UI/NewPages/Navigation/navigation_workflow_coordinator.cpp`
  - 负责五阶段流程的业务编排
- `UI/NewPages/Navigation/preparation_planning_controller.h`
- `UI/NewPages/Navigation/preparation_planning_controller.cpp`
- `UI/NewPages/Navigation/registration_controller.h`
- `UI/NewPages/Navigation/registration_controller.cpp`
- `UI/NewPages/Navigation/navigation_evaluation_controller.h`
- `UI/NewPages/Navigation/navigation_evaluation_controller.cpp`
  - 分别承载准备/规划、配准、导航/评估三类业务
- `Framework/VTK/embedded_vtk_view_host.h`
- `Framework/VTK/embedded_vtk_view_host.cpp`
  - 统一负责 VTK widget attach/detach/pause/resume/dispose
- `UI/NewPages/Navigation/navigation_vtk_bridge.h`
- `UI/NewPages/Navigation/navigation_vtk_bridge.cpp`
  - 负责导航页内部 `FourView` 与配准 VTK widget 的嵌入
- `tests/unit/StartupPhaseRegistrarContractTest.cpp`
- `tests/unit/NavigationWorkflowContextTest.cpp`
- `tests/unit/NavigationWorkflowCoordinatorTest.cpp`
- `tests/unit/EmbeddedVtkViewHostTest.cpp`

### Modified Files

- `main.cpp`
  - 移除大段内联 phase 注册逻辑，改为调用 bootstrap/registrar
- `CMakeLists.txt`
  - 加入 `Framework/Platform/Bootstrap`、`Framework/VTK` 和 `UI/NewPages/Navigation` 新增源文件
- `tests/unit/CMakeLists.txt`
  - 加入新增测试目标
- `UI/NewPages/NavigationPage.h`
- `UI/NewPages/NavigationPage.cpp`
  - 页面壳只保留 UI 响应、阶段切换和协调器调用
- `Framework/VTKWidgetPool.h`
- `Framework/VTKWidgetPool.cpp`
  - 暴露对新 host 更友好的 acquire/release 接口
- `Framework/Platform/UiBridge/NavigationPageServiceAccess.h`
- `Framework/Platform/UiBridge/NavigationPageServiceAccess.cpp`
  - 为 `NavigationServiceBundle` 提供统一服务接入口
- `docs/current_status_and_project_overview.md`
  - 记录结构治理阶段性完成状态

## Task 1: Extract Startup Phase Registration From `main.cpp`

**Files:**
- Create: `Framework/Platform/Bootstrap/startup_phase_registrar.h`
- Create: `Framework/Platform/Bootstrap/startup_phase_registrar.cpp`
- Modify: `main.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/unit/CMakeLists.txt`
- Test: `tests/unit/StartupPhaseRegistrarContractTest.cpp`

- [ ] **Step 1: Write the failing contract test**

```cpp
#include <QtTest/QtTest>

#include <QFile>

class StartupPhaseRegistrarContractTest : public QObject
{
    Q_OBJECT

private slots:
    void main_entry_delegates_phase_registration_to_bootstrap_component();

private:
    QString readSource(const QString& relativePath) const
    {
        QFile file(relativePath);
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(relativePath));
        return QString::fromUtf8(file.readAll());
    }
};

void StartupPhaseRegistrarContractTest::main_entry_delegates_phase_registration_to_bootstrap_component()
{
    const QString mainSource = readSource(QStringLiteral("main.cpp"));

    QVERIFY2(mainSource.contains(QStringLiteral("StartupPhaseRegistrar")),
        "main.cpp must delegate startup phase registration to StartupPhaseRegistrar");
    QVERIFY2(!mainSource.contains(QStringLiteral("registerPhaseHandler(StartupPhase::PlatformRuntimeInit")),
        "main.cpp must no longer inline PlatformRuntimeInit registration");
    QVERIFY2(!mainSource.contains(QStringLiteral("registerPhaseHandler(StartupPhase::PluginInstallation")),
        "main.cpp must no longer inline PluginInstallation registration");
}

QTEST_APPLESS_MAIN(StartupPhaseRegistrarContractTest)
#include "StartupPhaseRegistrarContractTest.moc"
```

- [ ] **Step 2: Run the contract test to verify it fails**

Run: `cmake --build build_x64 --config Release --target startup_phase_registrar_contract_test`

Expected: build fails because `startup_phase_registrar_contract_test` is not defined yet, or test fails because `main.cpp` still inlines `registerPhaseHandler(...)`.

- [ ] **Step 3: Add `StartupPhaseRegistrar` and delegate phase registration**

```cpp
// Framework/Platform/Bootstrap/startup_phase_registrar.h
#pragma once

#include "Framework/FrameworkExport.h"

#include <functional>
#include <memory>

class QApplication;
class StartupOrchestrator;
class PlatformStartupCoordinator;
class IPlatformRuntimeHostPort;

struct StartupPhaseRegistrarContext
{
    QApplication* app = nullptr;
    StartupOrchestrator* orchestrator = nullptr;
    PlatformStartupCoordinator* startupCoordinator = nullptr;
    IPlatformRuntimeHostPort* runtimeHostPort = nullptr;
    std::function<void(const QString&)> publishBootStage;
    std::function<void(const QString&)> publishFailure;
};

class FRAMEWORK_EXPORT StartupPhaseRegistrar
{
public:
    void registerPhases(const StartupPhaseRegistrarContext& context) const;
};
```

```cpp
// Framework/Platform/Bootstrap/startup_phase_registrar.cpp
#include "Framework/Platform/Bootstrap/startup_phase_registrar.h"

#include "Framework/StartupOrchestrator.h"

void StartupPhaseRegistrar::registerPhases(const StartupPhaseRegistrarContext& context) const
{
    auto* orchestrator = context.orchestrator;
    Q_ASSERT(orchestrator);

    orchestrator->clearPhaseHandlers();

    orchestrator->registerPhaseHandler(
        StartupPhase::PlatformRuntimeInit,
        [runtimeHostPort = context.runtimeHostPort, publishBootStage = context.publishBootStage, publishFailure = context.publishFailure](QApplication* app) {
            publishBootStage(QStringLiteral("Platform runtime initialization"));
            if (!runtimeHostPort || !runtimeHostPort->initialize(app) || !runtimeHostPort->startup()) {
                publishFailure(QStringLiteral("Platform runtime initialization failed"));
                return StartupOrchestrator::PhaseExecutionResult::failed(QStringLiteral("Platform runtime initialization failed"));
            }
            return StartupOrchestrator::PhaseExecutionResult::succeeded(QStringLiteral("Platform runtime initialization completed"));
        });
}
```

```cpp
// main.cpp
#include "Framework/Platform/Bootstrap/startup_phase_registrar.h"

StartupPhaseRegistrar startupPhaseRegistrar;
startupPhaseRegistrar.registerPhases({
    &app,
    orchestrator,
    startupContext->startupCoordinator.get(),
    runtimeHostPort,
    publishBootStage,
    publishFailure
});
```

```cmake
# CMakeLists.txt
list(APPEND FRAMEWORK_SOURCES
    Framework/Platform/Bootstrap/startup_phase_registrar.h
    Framework/Platform/Bootstrap/startup_phase_registrar.cpp
)
```

```cmake
# tests/unit/CMakeLists.txt
add_executable(startup_phase_registrar_contract_test
    StartupPhaseRegistrarContractTest.cpp
)

target_include_directories(startup_phase_registrar_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(startup_phase_registrar_contract_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME startup_phase_registrar_contract_test
    COMMAND startup_phase_registrar_contract_test
)
```

- [ ] **Step 4: Run the contract test and startup lifecycle regression**

Run: `cmake --build build_x64 --config Release --target startup_phase_registrar_contract_test startup_orchestrator_lifecycle_test && ctest --test-dir build_x64 -C Release -R "startup_phase_registrar_contract_test|startup_orchestrator_lifecycle_test" --output-on-failure`

Expected: contract test passes, existing orchestrator lifecycle test still passes.

- [ ] **Step 5: Commit the startup phase registrar slice**

```bash
git add main.cpp CMakeLists.txt tests/unit/CMakeLists.txt tests/unit/StartupPhaseRegistrarContractTest.cpp Framework/Platform/Bootstrap/startup_phase_registrar.h Framework/Platform/Bootstrap/startup_phase_registrar.cpp
git commit -m "refactor: extract startup phase registration"
```

## Task 2: Add Startup UI Coordination Wrapper

**Files:**
- Create: `Framework/Platform/Bootstrap/startup_ui_coordinator.h`
- Create: `Framework/Platform/Bootstrap/startup_ui_coordinator.cpp`
- Modify: `main.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/unit/CMakeLists.txt`
- Test: `tests/unit/StartupBootstrapControllerTest.cpp`

- [ ] **Step 1: Extend the existing bootstrap controller test with UI coordination expectations**

```cpp
void StartupBootstrapControllerTest::ready_snapshot_exposes_enter_state_and_stage_text()
{
    StartupBootstrapController controller;
    QSignalSpy spy(&controller, &StartupBootstrapController::snapshotChanged);

    controller.beginBoot(QStringLiteral("Platform runtime initialization"));
    controller.markReady();

    QVERIFY(spy.count() >= 2);
    const StartupShellSnapshot snapshot = controller.snapshot();
    QCOMPARE(snapshot.state, StartupShellState::Ready);
    QCOMPARE(snapshot.canEnterSystem, true);
    QCOMPARE(snapshot.frameworkReady, true);
}
```

- [ ] **Step 2: Run the bootstrap controller test to verify current behavior is the only covered path**

Run: `ctest --test-dir build_x64 -C Release -R "^startup_bootstrap_controller_test$" --output-on-failure`

Expected: existing test passes, but there is no startup UI coordination class or test coverage for main-window transition orchestration.

- [ ] **Step 3: Add `StartupUiCoordinator` and route success/failure UI transitions through it**

```cpp
// Framework/Platform/Bootstrap/startup_ui_coordinator.h
#pragma once

#include "Framework/FrameworkExport.h"

#include <QPointer>

class QApplication;
class MainInterfaceWidget;
class StartupBootstrapController;

class FRAMEWORK_EXPORT StartupUiCoordinator
{
public:
    StartupUiCoordinator(StartupBootstrapController* bootstrapController,
                         QPointer<MainInterfaceWidget>* mainInterface);

    void bindToStartupCompletion(QApplication* app);
    void showStartupFailureReport(const QString& reportText) const;

private:
    StartupBootstrapController* m_bootstrapController = nullptr;
    QPointer<MainInterfaceWidget>* m_mainInterface = nullptr;
};
```

```cpp
// Framework/Platform/Bootstrap/startup_ui_coordinator.cpp
#include "Framework/Platform/Bootstrap/startup_ui_coordinator.h"

#include "Framework/Platform/Bootstrap/StartupBootstrapController.h"
#include "Framework/StartupOrchestrator.h"
#include "UI/MainInterfaceWidget.h"

void StartupUiCoordinator::bindToStartupCompletion(QApplication* app)
{
    QObject::connect(StartupOrchestrator::instance(), &StartupOrchestrator::startupCompleted, app,
        [this](bool success) {
            if (!m_mainInterface || !*m_mainInterface) return;
            if (success) {
                m_bootstrapController->markReady();
                return;
            }
            showStartupFailureReport(StartupOrchestrator::instance()->getDiagnosticReport());
        });
}
```

```cpp
// main.cpp
#include "Framework/Platform/Bootstrap/startup_ui_coordinator.h"

StartupUiCoordinator startupUiCoordinator(startupBootstrapController.get(), &mainInterface);
startupUiCoordinator.bindToStartupCompletion(&app);
```

- [ ] **Step 4: Rebuild and run the startup-focused test batch**

Run: `cmake --build build_x64 --config Release --target startup_bootstrap_controller_test startup_phase_registrar_contract_test medicalpro && ctest --test-dir build_x64 -C Release -R "startup_bootstrap_controller_test|startup_phase_registrar_contract_test" --output-on-failure`

Expected: startup controller tests still pass, new startup coordinator compiles into `medicalpro`.

- [ ] **Step 5: Commit the startup UI coordination slice**

```bash
git add main.cpp CMakeLists.txt Framework/Platform/Bootstrap/startup_ui_coordinator.h Framework/Platform/Bootstrap/startup_ui_coordinator.cpp
git commit -m "refactor: centralize startup ui coordination"
```

## Task 3: Split Navigation Page State And Service Access

**Files:**
- Create: `UI/NewPages/Navigation/navigation_workflow_context.h`
- Create: `UI/NewPages/Navigation/navigation_workflow_context.cpp`
- Create: `UI/NewPages/Navigation/navigation_service_bundle.h`
- Create: `UI/NewPages/Navigation/navigation_service_bundle.cpp`
- Modify: `UI/NewPages/NavigationPage.h`
- Modify: `UI/NewPages/NavigationPage.cpp`
- Modify: `Framework/Platform/UiBridge/NavigationPageServiceAccess.h`
- Modify: `Framework/Platform/UiBridge/NavigationPageServiceAccess.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/unit/CMakeLists.txt`
- Test: `tests/unit/NavigationWorkflowContextTest.cpp`

- [ ] **Step 1: Write the failing workflow context test**

```cpp
#include <QtTest/QtTest>

#include "UI/NewPages/Navigation/navigation_workflow_context.h"

class NavigationWorkflowContextTest : public QObject
{
    Q_OBJECT

private slots:
    void context_tracks_case_identity_stage_and_workspace_paths();
};

void NavigationWorkflowContextTest::context_tracks_case_identity_stage_and_workspace_paths()
{
    NavigationWorkflowContext context;
    context.setCaseIdentity(QStringLiteral("ankle-case-101"), 12, QStringLiteral("Patient Z"));
    context.setCurrentStage(AnkleWorkflowStage::Registration);
    context.setCasesRoot(QStringLiteral("D:/Qtproject/medicalpro/data/cases"));

    QCOMPARE(context.caseId(), QStringLiteral("ankle-case-101"));
    QCOMPARE(context.patientId(), 12);
    QCOMPARE(context.patientName(), QStringLiteral("Patient Z"));
    QCOMPARE(context.currentStage(), AnkleWorkflowStage::Registration);
    QCOMPARE(context.caseRoot(), QStringLiteral("D:/Qtproject/medicalpro/data/cases/ankle-case-101"));
}

QTEST_APPLESS_MAIN(NavigationWorkflowContextTest)
#include "NavigationWorkflowContextTest.moc"
```

- [ ] **Step 2: Run the new test target to verify it fails**

Run: `cmake --build build_x64 --config Release --target navigation_workflow_context_test`

Expected: build fails because `navigation_workflow_context.h` or `navigation_workflow_context_test` does not exist yet.

- [ ] **Step 3: Add workflow context and service bundle, then wire them into `NavigationPage`**

```cpp
// UI/NewPages/Navigation/navigation_workflow_context.h
#pragma once

#include "UI/NewPages/NavigationPage.h"

#include <QString>

class NavigationWorkflowContext
{
public:
    void setCaseIdentity(const QString& caseId, int patientId, const QString& patientName);
    void setCurrentStage(AnkleWorkflowStage stage);
    void setCasesRoot(const QString& root);

    QString caseId() const;
    int patientId() const;
    QString patientName() const;
    AnkleWorkflowStage currentStage() const;
    QString caseRoot() const;

private:
    QString m_caseId;
    int m_patientId = -1;
    QString m_patientName;
    QString m_casesRoot;
    AnkleWorkflowStage m_currentStage = AnkleWorkflowStage::Preparation;
};
```

```cpp
// UI/NewPages/Navigation/navigation_service_bundle.h
#pragma once

#include <QObject>

class NavigationPageServiceAccess;
class InstrumentManagementService;
class BoneSegmentationService;
class FourViewDisplayService;
class OpticalTrackingService;
class PointRegistrationService;

class NavigationServiceBundle
{
public:
    explicit NavigationServiceBundle(NavigationPageServiceAccess* serviceAccess);

    InstrumentManagementService* instrumentService() const;
    BoneSegmentationService* segmentationService() const;
    FourViewDisplayService* fourViewService() const;
    OpticalTrackingService* trackingService() const;
    PointRegistrationService* pointRegistrationService(bool tryStartPlugin) const;

private:
    NavigationPageServiceAccess* m_serviceAccess = nullptr;
};
```

```cpp
// UI/NewPages/NavigationPage.h
#include "UI/NewPages/Navigation/navigation_service_bundle.h"
#include "UI/NewPages/Navigation/navigation_workflow_context.h"

std::unique_ptr<NavigationWorkflowContext> m_workflowContext;
std::unique_ptr<NavigationServiceBundle> m_serviceBundle;
```

```cpp
// UI/NewPages/NavigationPage.cpp
m_workflowContext = std::make_unique<NavigationWorkflowContext>();
m_serviceBundle = std::make_unique<NavigationServiceBundle>(m_serviceAccess);

void NavigationPageNew::setCaseContext(const QString& caseId, int patientId, const QString& patientName)
{
    m_workflowContext->setCaseIdentity(caseId, patientId, patientName);
}
```

- [ ] **Step 4: Run the workflow context test and UI bridge regression**

Run: `cmake --build build_x64 --config Release --target navigation_workflow_context_test platform_ui_bridge_test medicalpro && ctest --test-dir build_x64 -C Release -R "navigation_workflow_context_test|platform_ui_bridge_test" --output-on-failure`

Expected: workflow context test passes, existing `PlatformUiBridgeTest` still passes.

- [ ] **Step 5: Commit the navigation state/service split**

```bash
git add CMakeLists.txt tests/unit/CMakeLists.txt tests/unit/NavigationWorkflowContextTest.cpp UI/NewPages/NavigationPage.h UI/NewPages/NavigationPage.cpp UI/NewPages/Navigation/navigation_workflow_context.h UI/NewPages/Navigation/navigation_workflow_context.cpp UI/NewPages/Navigation/navigation_service_bundle.h UI/NewPages/Navigation/navigation_service_bundle.cpp Framework/Platform/UiBridge/NavigationPageServiceAccess.h Framework/Platform/UiBridge/NavigationPageServiceAccess.cpp
git commit -m "refactor: split navigation workflow state and services"
```

## Task 4: Move Navigation Business Flow Into Coordinator And Controllers

**Files:**
- Create: `UI/NewPages/Navigation/navigation_workflow_coordinator.h`
- Create: `UI/NewPages/Navigation/navigation_workflow_coordinator.cpp`
- Create: `UI/NewPages/Navigation/preparation_planning_controller.h`
- Create: `UI/NewPages/Navigation/preparation_planning_controller.cpp`
- Create: `UI/NewPages/Navigation/registration_controller.h`
- Create: `UI/NewPages/Navigation/registration_controller.cpp`
- Create: `UI/NewPages/Navigation/navigation_evaluation_controller.h`
- Create: `UI/NewPages/Navigation/navigation_evaluation_controller.cpp`
- Modify: `UI/NewPages/NavigationPage.h`
- Modify: `UI/NewPages/NavigationPage.cpp`
- Modify: `tests/unit/CMakeLists.txt`
- Test: `tests/unit/NavigationWorkflowCoordinatorTest.cpp`

- [ ] **Step 1: Write the failing coordinator test**

```cpp
#include <QtTest/QtTest>

#include "UI/NewPages/Navigation/navigation_workflow_coordinator.h"

class NavigationWorkflowCoordinatorTest : public QObject
{
    Q_OBJECT

private slots:
    void coordinator_updates_stage_and_routes_registration_start();
};

void NavigationWorkflowCoordinatorTest::coordinator_updates_stage_and_routes_registration_start()
{
    NavigationWorkflowContext context;
    NavigationWorkflowCoordinator coordinator(&context, nullptr, nullptr, nullptr);

    coordinator.enterStage(AnkleWorkflowStage::Registration);

    QCOMPARE(context.currentStage(), AnkleWorkflowStage::Registration);
}

QTEST_APPLESS_MAIN(NavigationWorkflowCoordinatorTest)
#include "NavigationWorkflowCoordinatorTest.moc"
```

- [ ] **Step 2: Run the coordinator target to verify it fails**

Run: `cmake --build build_x64 --config Release --target navigation_workflow_coordinator_test`

Expected: build fails because `navigation_workflow_coordinator.h` or target wiring does not exist yet.

- [ ] **Step 3: Add coordinator/controllers and delegate slot handlers**

```cpp
// UI/NewPages/Navigation/navigation_workflow_coordinator.h
#pragma once

#include "UI/NewPages/Navigation/navigation_workflow_context.h"

class PreparationPlanningController;
class RegistrationController;
class NavigationEvaluationController;

class NavigationWorkflowCoordinator
{
public:
    NavigationWorkflowCoordinator(
        NavigationWorkflowContext* context,
        PreparationPlanningController* preparationPlanningController,
        RegistrationController* registrationController,
        NavigationEvaluationController* navigationEvaluationController);

    void enterStage(AnkleWorkflowStage stage);
    void handleLoadDicom();
    void handleComputeRegistration();
    void handleStartNavigation();

private:
    NavigationWorkflowContext* m_context = nullptr;
    PreparationPlanningController* m_preparationPlanningController = nullptr;
    RegistrationController* m_registrationController = nullptr;
    NavigationEvaluationController* m_navigationEvaluationController = nullptr;
};
```

```cpp
// UI/NewPages/NavigationPage.cpp
void NavigationPageNew::on_loadDicomButton_clicked()
{
    m_workflowCoordinator->handleLoadDicom();
}

void NavigationPageNew::on_computeRegButton_clicked()
{
    m_workflowCoordinator->handleComputeRegistration();
}

void NavigationPageNew::on_startNavigationButton_clicked()
{
    m_workflowCoordinator->handleStartNavigation();
}
```

```cpp
// UI/NewPages/Navigation/preparation_planning_controller.h
class PreparationPlanningController
{
public:
    PreparationPlanningController(NavigationWorkflowContext* context,
                                  NavigationServiceBundle* services,
                                  Ui::NavigationPage* ui);
    void loadDicom();
    void startSegmentation();
};
```

- [ ] **Step 4: Run the coordinator test and navigation contract regression**

Run: `cmake --build build_x64 --config Release --target navigation_workflow_coordinator_test ankle_navigation_workflow_contract_test medicalpro && ctest --test-dir build_x64 -C Release -R "navigation_workflow_coordinator_test|ankle_navigation_workflow_contract_test" --output-on-failure`

Expected: coordinator test passes, existing workflow contract test still passes after the split.

- [ ] **Step 5: Commit the navigation workflow coordination slice**

```bash
git add tests/unit/CMakeLists.txt tests/unit/NavigationWorkflowCoordinatorTest.cpp UI/NewPages/NavigationPage.h UI/NewPages/NavigationPage.cpp UI/NewPages/Navigation/navigation_workflow_coordinator.h UI/NewPages/Navigation/navigation_workflow_coordinator.cpp UI/NewPages/Navigation/preparation_planning_controller.h UI/NewPages/Navigation/preparation_planning_controller.cpp UI/NewPages/Navigation/registration_controller.h UI/NewPages/Navigation/registration_controller.cpp UI/NewPages/Navigation/navigation_evaluation_controller.h UI/NewPages/Navigation/navigation_evaluation_controller.cpp
git commit -m "refactor: move navigation flow into coordinator"
```

## Task 5: Centralize VTK View Embedding For Navigation Page

**Files:**
- Create: `Framework/VTK/embedded_vtk_view_host.h`
- Create: `Framework/VTK/embedded_vtk_view_host.cpp`
- Create: `UI/NewPages/Navigation/navigation_vtk_bridge.h`
- Create: `UI/NewPages/Navigation/navigation_vtk_bridge.cpp`
- Modify: `Framework/VTKWidgetPool.h`
- Modify: `Framework/VTKWidgetPool.cpp`
- Modify: `UI/NewPages/NavigationPage.h`
- Modify: `UI/NewPages/NavigationPage.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/unit/CMakeLists.txt`
- Test: `tests/unit/EmbeddedVtkViewHostTest.cpp`

- [ ] **Step 1: Write the failing VTK host test**

```cpp
#include <QtTest/QtTest>

#include <QFrame>
#include <QVBoxLayout>

#include "Framework/VTK/embedded_vtk_view_host.h"

class EmbeddedVtkViewHostTest : public QObject
{
    Q_OBJECT

private slots:
    void host_swaps_placeholder_and_attaches_widget_once();
};

void EmbeddedVtkViewHostTest::host_swaps_placeholder_and_attaches_widget_once()
{
    QWidget parent;
    auto* frame = new QFrame(&parent);
    auto* layout = new QVBoxLayout(frame);
    auto* placeholder = new QLabel(QStringLiteral("placeholder"), frame);
    layout->addWidget(placeholder);

    EmbeddedVtkViewHost host(frame, layout, placeholder);
    QWidget widget;

    host.attach(&widget);

    QCOMPARE(layout->indexOf(&widget) >= 0, true);
    QCOMPARE(placeholder->isHidden(), true);
}

QTEST_MAIN(EmbeddedVtkViewHostTest)
#include "EmbeddedVtkViewHostTest.moc"
```

- [ ] **Step 2: Run the VTK host test to verify it fails**

Run: `cmake --build build_x64 --config Release --target embedded_vtk_view_host_test`

Expected: build fails because `embedded_vtk_view_host.h` and target wiring do not exist yet.

- [ ] **Step 3: Add page-level VTK host/bridge and route navigation page embedding through them**

```cpp
// Framework/VTK/embedded_vtk_view_host.h
#pragma once

#include "Framework/FrameworkExport.h"

#include <QPointer>

class QLabel;
class QLayout;
class QWidget;

class FRAMEWORK_EXPORT EmbeddedVtkViewHost
{
public:
    EmbeddedVtkViewHost(QWidget* frame, QLayout* layout, QLabel* placeholder);

    void attach(QWidget* widget);
    void detach();
    void setPaused(bool paused);

private:
    QPointer<QWidget> m_frame;
    QPointer<QLayout> m_layout;
    QPointer<QLabel> m_placeholder;
    QPointer<QWidget> m_widget;
};
```

```cpp
// UI/NewPages/Navigation/navigation_vtk_bridge.h
#pragma once

#include "Framework/VTK/embedded_vtk_view_host.h"

class FourViewDisplayService;
class PointRegistrationService;

class NavigationVtkBridge
{
public:
    NavigationVtkBridge(EmbeddedVtkViewHost* planningHost,
                        EmbeddedVtkViewHost* registrationHost,
                        FourViewDisplayService* fourViewService,
                        PointRegistrationService* pointRegistrationService);

    QWidget* ensureFourViewWidget(QWidget* parent);
    QWidget* ensureRegistrationWidget(QWidget* parent);

private:
    EmbeddedVtkViewHost* m_planningHost = nullptr;
    EmbeddedVtkViewHost* m_registrationHost = nullptr;
    FourViewDisplayService* m_fourViewService = nullptr;
    PointRegistrationService* m_pointRegistrationService = nullptr;
};
```

```cpp
// UI/NewPages/NavigationPage.cpp
void NavigationPageNew::setupVTKViews()
{
    m_navigationVtkBridge->ensureFourViewWidget(this);
}

void NavigationPageNew::embedRegistrationVTKWidget()
{
    m_navigationVtkBridge->ensureRegistrationWidget(this);
}
```

- [ ] **Step 4: Run the VTK host test and rebuild the navigation shell**

Run: `cmake --build build_x64 --config Release --target embedded_vtk_view_host_test medicalpro && ctest --test-dir build_x64 -C Release -R "^embedded_vtk_view_host_test$" --output-on-failure`

Expected: VTK host test passes and `medicalpro` still builds with the new page-level VTK bridge.

- [ ] **Step 5: Commit the VTK host slice**

```bash
git add CMakeLists.txt tests/unit/CMakeLists.txt tests/unit/EmbeddedVtkViewHostTest.cpp Framework/VTK/embedded_vtk_view_host.h Framework/VTK/embedded_vtk_view_host.cpp Framework/VTKWidgetPool.h Framework/VTKWidgetPool.cpp UI/NewPages/NavigationPage.h UI/NewPages/NavigationPage.cpp UI/NewPages/Navigation/navigation_vtk_bridge.h UI/NewPages/Navigation/navigation_vtk_bridge.cpp
git commit -m "refactor: centralize navigation vtk embedding"
```

## Task 6: Update Docs And Run Final Architecture Regression

**Files:**
- Modify: `docs/current_status_and_project_overview.md`
- Modify: `tests/unit/CMakeLists.txt`
- Test: `tests/unit/RuntimeHostDetachmentContractTest.cpp`
- Test: `tests/unit/PlatformUiBridgeTest.cpp`
- Test: `tests/unit/StartupOrchestratorLifecycleTest.cpp`
- Test: `tests/unit/AnkleNavigationWorkflowContractTest.cpp`

- [ ] **Step 1: Update the status document for the new architecture boundaries**

```md
### 2026-04-29 Ankle Navigation Architecture Remediation

- `main.cpp` no longer owns inline startup phase registration.
- `NavigationPage` now delegates workflow orchestration to dedicated collaborators.
- Navigation-page VTK embedding is managed through page-level host/bridge components.
```

- [ ] **Step 2: Run the full architecture-focused test batch**

Run: `cmake --build build_x64 --config Release --target medicalpro startup_phase_registrar_contract_test navigation_workflow_context_test navigation_workflow_coordinator_test embedded_vtk_view_host_test && ctest --test-dir build_x64 -C Release -R "startup_phase_registrar_contract_test|startup_orchestrator_lifecycle_test|platform_ui_bridge_test|runtime_host_detachment_contract_test|ankle_navigation_workflow_contract_test|navigation_workflow_context_test|navigation_workflow_coordinator_test|embedded_vtk_view_host_test" --output-on-failure`

Expected: all listed tests pass and `medicalpro` builds successfully.

- [ ] **Step 3: Check that `main.cpp` and `NavigationPage.cpp` are materially smaller**

```powershell
$mainLines = (Get-Content 'main.cpp').Count
$navigationLines = (Get-Content 'UI/NewPages/NavigationPage.cpp').Count
Write-Output "main.cpp=$mainLines"
Write-Output "NavigationPage.cpp=$navigationLines"
```

Expected: `main.cpp` and `NavigationPage.cpp` are both measurably smaller than the starting counts `1005` and `1718`.

- [ ] **Step 4: Commit the architecture remediation acceptance slice**

```bash
git add docs/current_status_and_project_overview.md
git commit -m "docs: record ankle navigation architecture remediation"
```

- [ ] **Step 5: Tag the handoff point**

```bash
git status --short
```

Expected: working tree is clean, or only contains intentionally staged follow-up work for the innovation/baseline plan.

## Self-Review

### Spec Coverage

- 启动链收口由 Task 1 和 Task 2 实现
- `NavigationPage` 职责下沉由 Task 3 实现
- VTK 页面级宿主收口由 Task 5 实现
- 回归与验收由 Task 6 实现

### Placeholder Scan

- 本计划没有保留 `TODO`、`TBD` 或“后续补充”的占位描述
- 每个任务都给出了明确文件、测试、命令和提交动作

### Type Consistency

- 页面阶段统一使用 `AnkleWorkflowStage`
- 页面共享状态统一使用 `NavigationWorkflowContext`
- 页面服务统一通过 `NavigationServiceBundle`
- VTK 页面级生命周期统一通过 `EmbeddedVtkViewHost` 和 `NavigationVtkBridge`
