# Cold Start Welcome Shell Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** show `Welcome` quickly through a lightweight startup shell, keep `Enter System` disabled until Phase 1 managed startup reaches `platformReady`, and lazily create `MainInterfaceWidget` only after readiness.

**Architecture:** keep the current governance truth source (`StartupOrchestrator + PlatformStartupCoordinator + PlatformDiagnosticsService`) but move first paint ahead of heavy main-interface construction. Add a shell-facing bootstrap controller and shell snapshot contract, then hand the prepared runtime context into a lazily created main interface after readiness.

**Tech Stack:** Qt 6, Qt Widgets, QtTest, CMake, CTest, PowerShell, existing `Framework/Platform` governance layer, `WelcomePageNew`, `MainInterfaceWidget`

**Progress Update 2026-04-21**

- Task 1 code and verification completed.
- Verified commands:
  - `cmake --build build_x64 --config Release --target welcome_page_bootstrap_state_test welcome_page_runtime_refresh_test`
  - `ctest --test-dir build_x64 -C Release -R "welcome_page_bootstrap_state_test|welcome_page_runtime_refresh_test" --output-on-failure`
- Current checkpoint:
  - `StartupShellSnapshot` contract is added.
  - `WelcomePageNew` can consume shell bootstrap snapshots without being overwritten by its legacy runtime refresh path.
  - Task 2 and later tasks are still pending.

---

## Files And Responsibilities

- Modify: `CMakeLists.txt`
  - Compile the new startup shell and bootstrap controller into the app/framework targets
- Create: `Framework/Platform/Contracts/StartupShellSnapshot.h`
  - Define the shell-facing startup state contract
- Create: `Framework/Platform/Bootstrap/StartupBootstrapController.h`
- Create: `Framework/Platform/Bootstrap/StartupBootstrapController.cpp`
  - Own shell bootstrap session state, readiness gating, failure publication, and retry semantics
- Create: `UI/StartupShell.h`
- Create: `UI/StartupShell.cpp`
  - Host `WelcomePageNew` and expose `Enter System / Retry Startup / View Diagnostics`
- Create: `UI/MainInterfaceFactory.h`
- Create: `UI/MainInterfaceFactory.cpp`
  - Build `MainInterfaceWidget` only after readiness with the prepared runtime bindings
- Modify: `UI/MainInterfaceWidget.h`
- Modify: `UI/MainInterfaceWidget.cpp`
  - Accept handed-off runtime bindings instead of being the cold-start owner of bootstrap truth
- Modify: `UI/NewPages/WelcomePage.h`
- Modify: `UI/NewPages/WelcomePage.cpp`
  - Consume shell snapshot updates for button gating and shell-first status rendering
- Modify: `main.cpp`
  - Replace direct cold-start `MainInterfaceWidget` creation with shell-first startup orchestration
- Modify: `tests/unit/CMakeLists.txt`
  - Register the new shell/controller tests
- Create: `tests/unit/WelcomePageBootstrapStateTest.cpp`
  - Verify `booting / ready / failed` UI behavior on `WelcomePageNew`
- Create: `tests/unit/StartupBootstrapControllerTest.cpp`
  - Verify shell bootstrap state transitions and retry reset behavior
- Modify: `tests/CMakeLists.txt`
  - Register a runtime smoke test for shell-first cold start
- Create: `tests/runtime/verify_cold_start_welcome_shell.ps1`
  - Verify shell-first startup ordering and enter-button enablement markers
- Modify: `docs/current_status_and_project_overview.md`
  - Record cold-start welcome-shell acceptance
- Modify: `docs/superpowers/tracking/platform-migration-decision-log.md`
  - Record the shell-first cold-start decision
- Modify: `docs/superpowers/plans/2026-04-21-cold-start-welcome-shell-implementation.md`
  - Mark implementation status and acceptance

### Task 1: Add The Shell Snapshot Contract And Welcome Gating Tests

**Files:**
- Create: `Framework/Platform/Contracts/StartupShellSnapshot.h`
- Modify: `UI/NewPages/WelcomePage.h`
- Modify: `UI/NewPages/WelcomePage.cpp`
- Modify: `tests/unit/CMakeLists.txt`
- Create: `tests/unit/WelcomePageBootstrapStateTest.cpp`

- [x] **Step 1: Register a red test for shell-driven welcome gating**

```cmake
# tests/unit/CMakeLists.txt
add_executable(welcome_page_bootstrap_state_test
    WelcomePageBootstrapStateTest.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/BasePage.h
    ${CMAKE_SOURCE_DIR}/UI/NewPages/WelcomePage.h
    ${CMAKE_SOURCE_DIR}/UI/NewPages/WelcomePage.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/ThreePagePresentationUtils.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/WelcomeBrandingUtils.cpp
    ${CMAKE_SOURCE_DIR}/UI/Forms/WelcomePage.ui
)

set_target_properties(welcome_page_bootstrap_state_test PROPERTIES
    AUTOUIC_SEARCH_PATHS "${CMAKE_SOURCE_DIR}/UI/Forms"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/MinSizeRel"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/RelWithDebInfo"
)

target_include_directories(welcome_page_bootstrap_state_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/UI/NewPages
    ${CMAKE_SOURCE_DIR}/UI/Forms
)

target_link_libraries(welcome_page_bootstrap_state_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME welcome_page_bootstrap_state_test
    COMMAND welcome_page_bootstrap_state_test
)
```

```cpp
// tests/unit/WelcomePageBootstrapStateTest.cpp
#include <QtTest/QtTest>

#include <QLabel>
#include <QPushButton>

#include "Framework/Platform/Contracts/StartupShellSnapshot.h"
#include "UI/NewPages/WelcomePage.h"

class WelcomePageBootstrapStateTest : public QObject
{
    Q_OBJECT

private slots:
    void applyStartupShellSnapshot_disables_enter_while_booting();
    void applyStartupShellSnapshot_enables_enter_when_ready();
    void applyStartupShellSnapshot_surfaces_failure_reason();
};

void WelcomePageBootstrapStateTest::applyStartupShellSnapshot_disables_enter_while_booting()
{
    WelcomePageNew page;
    page.onActivated();

    StartupShellSnapshot snapshot;
    snapshot.state = StartupShellState::Booting;
    snapshot.canEnterSystem = false;
    snapshot.frameworkReady = false;
    snapshot.managedScopeReady = false;
    snapshot.stageLabel = QStringLiteral("CTK framework initialization");
    snapshot.statusText = QStringLiteral("系统初始化中");

    page.applyStartupShellSnapshot(snapshot);

    auto* enterButton = page.findChild<QPushButton*>(QStringLiteral("enterButton"));
    auto* runtimeDecisionBadge = page.findChild<QLabel*>(QStringLiteral("runtimeDecisionBadge"));

    QVERIFY2(enterButton != nullptr, "enterButton not found");
    QVERIFY2(runtimeDecisionBadge != nullptr, "runtimeDecisionBadge not found");
    QVERIFY(!enterButton->isEnabled());
    QCOMPARE(runtimeDecisionBadge->text(), QStringLiteral("系统初始化中"));
}

void WelcomePageBootstrapStateTest::applyStartupShellSnapshot_enables_enter_when_ready()
{
    WelcomePageNew page;
    page.onActivated();

    StartupShellSnapshot snapshot;
    snapshot.state = StartupShellState::Ready;
    snapshot.canEnterSystem = true;
    snapshot.frameworkReady = true;
    snapshot.managedScopeReady = true;
    snapshot.statusText = QStringLiteral("主流程可进入");

    page.applyStartupShellSnapshot(snapshot);

    auto* enterButton = page.findChild<QPushButton*>(QStringLiteral("enterButton"));
    QVERIFY2(enterButton != nullptr, "enterButton not found");
    QVERIFY(enterButton->isEnabled());
}

void WelcomePageBootstrapStateTest::applyStartupShellSnapshot_surfaces_failure_reason()
{
    WelcomePageNew page;
    page.onActivated();

    StartupShellSnapshot snapshot;
    snapshot.state = StartupShellState::Failed;
    snapshot.canEnterSystem = false;
    snapshot.failureReason = QStringLiteral("Critical plugin activation failed");
    snapshot.recoveryHints = { QStringLiteral("Retry startup or inspect diagnostics.") };
    snapshot.statusText = QStringLiteral("初始化失败");

    page.applyStartupShellSnapshot(snapshot);

    auto* runtimeSummaryTextLabel = page.findChild<QLabel*>(QStringLiteral("runtimeSummaryTextLabel"));
    QVERIFY2(runtimeSummaryTextLabel != nullptr, "runtimeSummaryTextLabel not found");
    QVERIFY(runtimeSummaryTextLabel->text().contains(QStringLiteral("Critical plugin activation failed")));
}

QTEST_MAIN(WelcomePageBootstrapStateTest)
#include "WelcomePageBootstrapStateTest.moc"
```

- [x] **Step 2: Run the new test and confirm it fails before implementation**

Run:

```powershell
cmake --build build_x64 --config Release --target welcome_page_bootstrap_state_test
ctest --test-dir build_x64 -C Release -R "welcome_page_bootstrap_state_test" --output-on-failure
```

Expected:

- build fails because `StartupShellSnapshot` and `WelcomePageNew::applyStartupShellSnapshot(...)` do not exist yet
- or the test binary fails because the welcome page does not yet gate `enterButton`

- [x] **Step 3: Add the shell snapshot contract and welcome page shell-state application path**

```cpp
// Framework/Platform/Contracts/StartupShellSnapshot.h
#pragma once

#include <QString>
#include <QStringList>

enum class StartupShellState
{
    Booting,
    Ready,
    Failed
};

struct StartupShellSnapshot
{
    StartupShellState state = StartupShellState::Booting;
    bool canEnterSystem = false;
    bool frameworkReady = false;
    bool managedScopeReady = false;
    bool dataDirectoryReadable = false;
    QString stageKey;
    QString stageLabel;
    QString statusText;
    QString failureReason;
    QStringList recoveryHints;
};
```

```cpp
// UI/NewPages/WelcomePage.h
#include "Framework/Platform/Contracts/StartupShellSnapshot.h"

public:
    void applyStartupShellSnapshot(const StartupShellSnapshot& snapshot);

private:
    void applyShellDecisionState(const StartupShellSnapshot& snapshot);
```

```cpp
// UI/NewPages/WelcomePage.cpp
void WelcomePageNew::applyStartupShellSnapshot(const StartupShellSnapshot& snapshot)
{
    applyShellDecisionState(snapshot);

    if (ui->enterButton) {
        ui->enterButton->setEnabled(snapshot.canEnterSystem);
    }

    if (!snapshot.failureReason.isEmpty()) {
        QString summary = snapshot.failureReason;
        if (!snapshot.recoveryHints.isEmpty()) {
            summary += QStringLiteral("\n") + snapshot.recoveryHints.join(QStringLiteral("\n"));
        }
        ui->runtimeSummaryTextLabel->setText(summary);
    } else if (!snapshot.statusText.isEmpty()) {
        ui->runtimeSummaryTextLabel->setText(snapshot.statusText);
    }
}

void WelcomePageNew::applyShellDecisionState(const StartupShellSnapshot& snapshot)
{
    QString badgeText = snapshot.statusText;
    QString badgeTone = QStringLiteral("warning");

    switch (snapshot.state) {
    case StartupShellState::Booting:
        if (badgeText.isEmpty()) badgeText = QStringLiteral("系统初始化中");
        badgeTone = QStringLiteral("warning");
        break;
    case StartupShellState::Ready:
        if (badgeText.isEmpty()) badgeText = QStringLiteral("主流程可进入");
        badgeTone = QStringLiteral("ok");
        break;
    case StartupShellState::Failed:
        if (badgeText.isEmpty()) badgeText = QStringLiteral("初始化失败");
        badgeTone = QStringLiteral("danger");
        break;
    }

    ui->runtimeDecisionBadge->setText(badgeText);
    ui->runtimeDecisionBadge->setProperty("statusTone", badgeTone);
    ui->runtimeDecisionBadge->style()->unpolish(ui->runtimeDecisionBadge);
    ui->runtimeDecisionBadge->style()->polish(ui->runtimeDecisionBadge);
    ui->runtimeDecisionBadge->update();
}
```

- [x] **Step 4: Re-run the welcome shell-state test and the existing welcome refresh test**

Run:

```powershell
cmake --build build_x64 --config Release --target welcome_page_bootstrap_state_test welcome_page_runtime_refresh_test
ctest --test-dir build_x64 -C Release -R "welcome_page_bootstrap_state_test|welcome_page_runtime_refresh_test" --output-on-failure
```

Expected:

- `welcome_page_bootstrap_state_test` PASS
- `welcome_page_runtime_refresh_test` PASS

- [ ] **Step 5: Commit the welcome shell-state contract**

```powershell
git add Framework/Platform/Contracts/StartupShellSnapshot.h UI/NewPages/WelcomePage.h UI/NewPages/WelcomePage.cpp tests/unit/CMakeLists.txt tests/unit/WelcomePageBootstrapStateTest.cpp
git commit -m "feat: add welcome shell state contract"
```

### Task 2: Add StartupBootstrapController And StartupShell

**Files:**
- Modify: `CMakeLists.txt`
- Create: `Framework/Platform/Bootstrap/StartupBootstrapController.h`
- Create: `Framework/Platform/Bootstrap/StartupBootstrapController.cpp`
- Create: `UI/StartupShell.h`
- Create: `UI/StartupShell.cpp`
- Modify: `tests/unit/CMakeLists.txt`
- Create: `tests/unit/StartupBootstrapControllerTest.cpp`

- [ ] **Step 1: Add a red controller test for `booting -> ready -> failed -> retry`**

```cpp
// tests/unit/StartupBootstrapControllerTest.cpp
#include <QtTest/QtTest>

#include <QSignalSpy>

#include "Framework/Platform/Bootstrap/StartupBootstrapController.h"

class StartupBootstrapControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void start_marks_shell_booting_before_ready();
    void publishFailure_keeps_enter_locked_and_emits_retry_reset();
};

void StartupBootstrapControllerTest::start_marks_shell_booting_before_ready()
{
    StartupBootstrapController controller;
    QSignalSpy snapshotSpy(&controller, &StartupBootstrapController::snapshotChanged);

    controller.beginBoot(QStringLiteral("CTK framework initialization"));
    QVERIFY(snapshotSpy.count() >= 1);

    const auto booting = controller.snapshot();
    QCOMPARE(booting.state, StartupShellState::Booting);
    QVERIFY(!booting.canEnterSystem);

    controller.markReady();
    const auto ready = controller.snapshot();
    QCOMPARE(ready.state, StartupShellState::Ready);
    QVERIFY(ready.canEnterSystem);
}

void StartupBootstrapControllerTest::publishFailure_keeps_enter_locked_and_emits_retry_reset()
{
    StartupBootstrapController controller;

    controller.beginBoot(QStringLiteral("Critical plugin activation"));
    controller.markFailed(
        QStringLiteral("Critical plugin activation failed"),
        { QStringLiteral("Retry startup or inspect diagnostics.") });

    const auto failed = controller.snapshot();
    QCOMPARE(failed.state, StartupShellState::Failed);
    QVERIFY(!failed.canEnterSystem);
    QVERIFY(failed.failureReason.contains(QStringLiteral("failed")));

    controller.resetForRetry();
    const auto retry = controller.snapshot();
    QCOMPARE(retry.state, StartupShellState::Booting);
    QVERIFY(!retry.canEnterSystem);
}

QTEST_APPLESS_MAIN(StartupBootstrapControllerTest)
#include "StartupBootstrapControllerTest.moc"
```

- [ ] **Step 2: Register the new controller test and run it red**

```cmake
# tests/unit/CMakeLists.txt
add_executable(startup_bootstrap_controller_test
    StartupBootstrapControllerTest.cpp
)

target_include_directories(startup_bootstrap_controller_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(startup_bootstrap_controller_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME startup_bootstrap_controller_test
    COMMAND startup_bootstrap_controller_test
)
```

Run:

```powershell
cmake --build build_x64 --config Release --target startup_bootstrap_controller_test
ctest --test-dir build_x64 -C Release -R "startup_bootstrap_controller_test" --output-on-failure
```

Expected:

- build fails because `StartupBootstrapController` does not exist yet

- [ ] **Step 3: Implement the controller state machine and lightweight shell window**

```cpp
// Framework/Platform/Bootstrap/StartupBootstrapController.h
#pragma once

#include <QObject>

#include "Framework/Platform/Contracts/StartupShellSnapshot.h"

class FRAMEWORK_EXPORT StartupBootstrapController : public QObject
{
    Q_OBJECT

public:
    explicit StartupBootstrapController(QObject* parent = nullptr);

    const StartupShellSnapshot& snapshot() const;

    void beginBoot(const QString& stageLabel, const QString& statusText = QStringLiteral("系统初始化中"));
    void updateBootStage(const QString& stageLabel, const QString& statusText);
    void markReady();
    void markFailed(const QString& failureReason, const QStringList& recoveryHints);
    void resetForRetry();

signals:
    void snapshotChanged(const StartupShellSnapshot& snapshot);
    void readyToEnter();

private:
    void publishSnapshot();

    StartupShellSnapshot m_snapshot;
};
```

```cpp
// Framework/Platform/Bootstrap/StartupBootstrapController.cpp
#include "Framework/Platform/Bootstrap/StartupBootstrapController.h"

StartupBootstrapController::StartupBootstrapController(QObject* parent)
    : QObject(parent)
{
    resetForRetry();
}

const StartupShellSnapshot& StartupBootstrapController::snapshot() const
{
    return m_snapshot;
}

void StartupBootstrapController::beginBoot(const QString& stageLabel, const QString& statusText)
{
    m_snapshot.state = StartupShellState::Booting;
    m_snapshot.canEnterSystem = false;
    m_snapshot.stageLabel = stageLabel;
    m_snapshot.statusText = statusText;
    m_snapshot.failureReason.clear();
    m_snapshot.recoveryHints.clear();
    publishSnapshot();
}

void StartupBootstrapController::updateBootStage(const QString& stageLabel, const QString& statusText)
{
    m_snapshot.stageLabel = stageLabel;
    m_snapshot.statusText = statusText;
    publishSnapshot();
}

void StartupBootstrapController::markReady()
{
    m_snapshot.state = StartupShellState::Ready;
    m_snapshot.canEnterSystem = true;
    m_snapshot.statusText = QStringLiteral("主流程可进入");
    publishSnapshot();
    emit readyToEnter();
}

void StartupBootstrapController::markFailed(const QString& failureReason, const QStringList& recoveryHints)
{
    m_snapshot.state = StartupShellState::Failed;
    m_snapshot.canEnterSystem = false;
    m_snapshot.statusText = QStringLiteral("初始化失败");
    m_snapshot.failureReason = failureReason;
    m_snapshot.recoveryHints = recoveryHints;
    publishSnapshot();
}

void StartupBootstrapController::resetForRetry()
{
    m_snapshot = StartupShellSnapshot {};
    m_snapshot.state = StartupShellState::Booting;
    m_snapshot.statusText = QStringLiteral("系统初始化中");
    m_snapshot.canEnterSystem = false;
    publishSnapshot();
}

void StartupBootstrapController::publishSnapshot()
{
    emit snapshotChanged(m_snapshot);
}
```

```cpp
// UI/StartupShell.h
#pragma once

#include <QWidget>

#include "Framework/Platform/Contracts/StartupShellSnapshot.h"

class QLabel;
class QPushButton;
class WelcomePageNew;

class StartupShell : public QWidget
{
    Q_OBJECT

public:
    explicit StartupShell(QWidget* parent = nullptr);

    void applySnapshot(const StartupShellSnapshot& snapshot);

signals:
    void enterSystemRequested();
    void retryStartupRequested();
    void viewDiagnosticsRequested();
    void exitRequested();

private:
    WelcomePageNew* m_welcomePage = nullptr;
    QLabel* m_failureLabel = nullptr;
    QPushButton* m_retryButton = nullptr;
    QPushButton* m_viewDiagnosticsButton = nullptr;
};
```

```cpp
// UI/StartupShell.cpp
#include "UI/StartupShell.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "UI/NewPages/WelcomePage.h"

StartupShell::StartupShell(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_welcomePage = new WelcomePageNew(this);
    m_failureLabel = new QLabel(this);
    m_retryButton = new QPushButton(QStringLiteral("重试启动"), this);
    m_viewDiagnosticsButton = new QPushButton(QStringLiteral("查看诊断"), this);

    layout->addWidget(m_welcomePage);
    layout->addWidget(m_failureLabel);
    layout->addWidget(m_retryButton);
    layout->addWidget(m_viewDiagnosticsButton);

    connect(m_welcomePage, &WelcomePageNew::enterSystemRequested, this, &StartupShell::enterSystemRequested);
    connect(m_welcomePage, &WelcomePageNew::exitRequested, this, &StartupShell::exitRequested);
    connect(m_retryButton, &QPushButton::clicked, this, &StartupShell::retryStartupRequested);
    connect(m_viewDiagnosticsButton, &QPushButton::clicked, this, &StartupShell::viewDiagnosticsRequested);
}

void StartupShell::applySnapshot(const StartupShellSnapshot& snapshot)
{
    m_welcomePage->applyStartupShellSnapshot(snapshot);
    m_failureLabel->setVisible(snapshot.state == StartupShellState::Failed);
    m_failureLabel->setText(snapshot.failureReason);
    m_retryButton->setVisible(snapshot.state == StartupShellState::Failed);
    m_viewDiagnosticsButton->setVisible(snapshot.state == StartupShellState::Failed || snapshot.state == StartupShellState::Booting);
}
```

- [ ] **Step 4: Add the new files to the build and turn the controller test green**

```cmake
# CMakeLists.txt
list(APPEND FRAMEWORK_SOURCES
    Framework/Platform/Contracts/StartupShellSnapshot.h
    Framework/Platform/Bootstrap/StartupBootstrapController.h
    Framework/Platform/Bootstrap/StartupBootstrapController.cpp
)

list(APPEND PROJECT_SOURCES
    UI/StartupShell.h
    UI/StartupShell.cpp
)
```

Run:

```powershell
cmake --build build_x64 --config Release --target startup_bootstrap_controller_test medicalpro
ctest --test-dir build_x64 -C Release -R "startup_bootstrap_controller_test|welcome_page_bootstrap_state_test" --output-on-failure
```

Expected:

- `startup_bootstrap_controller_test` PASS
- `welcome_page_bootstrap_state_test` PASS

- [ ] **Step 5: Commit the shell controller layer**

```powershell
git add CMakeLists.txt Framework/Platform/Bootstrap/StartupBootstrapController.h Framework/Platform/Bootstrap/StartupBootstrapController.cpp UI/StartupShell.h UI/StartupShell.cpp tests/unit/CMakeLists.txt tests/unit/StartupBootstrapControllerTest.cpp
git commit -m "feat: add startup welcome shell controller"
```

### Task 3: Move Cold Start To The Shell And Delay MainInterfaceWidget Creation

**Files:**
- Create: `UI/MainInterfaceFactory.h`
- Create: `UI/MainInterfaceFactory.cpp`
- Modify: `UI/MainInterfaceWidget.h`
- Modify: `UI/MainInterfaceWidget.cpp`
- Modify: `main.cpp`

- [ ] **Step 1: Add a red lifecycle test for deferred main-interface creation**

```cpp
// tests/unit/StartupOrchestratorLifecycleTest.cpp
void StartupOrchestratorLifecycleTest::waits_for_background_startup_before_releasing_context()
{
    auto* orchestrator = StartupOrchestrator::instance();
    bool shellShown = false;
    bool mainInterfaceCreated = false;

    auto context = std::make_shared<int>(42);
    orchestrator->clearPhaseHandlers();
    orchestrator->registerPhaseHandler(
        StartupPhase::SplashScreen,
        [&shellShown](QApplication*) -> StartupOrchestrator::PhaseExecutionResult {
            shellShown = true;
            return {
                true,
                PlatformLifecycleResult::Succeeded,
                QStringLiteral("startup_shell_shown"),
                QStringLiteral("startup shell shown")
            };
        });
    orchestrator->registerPhaseHandler(
        StartupPhase::MainUICreation,
        [&mainInterfaceCreated](QApplication*) -> StartupOrchestrator::PhaseExecutionResult {
            mainInterfaceCreated = true;
            return StartupOrchestrator::PhaseExecutionResult::skipped(
                QStringLiteral("main interface deferred until enter"),
                QStringLiteral("main_interface_deferred"));
        });

    QSignalSpy completedSpy(orchestrator, &StartupOrchestrator::startupCompleted);
    orchestrator->start(qApp);

    QTRY_COMPARE(completedSpy.count(), 1);
    QVERIFY(shellShown);
    QVERIFY(mainInterfaceCreated);
}
```

- [ ] **Step 2: Run the lifecycle test red**

Run:

```powershell
cmake --build build_x64 --config Release --target startup_orchestrator_lifecycle_test
ctest --test-dir build_x64 -C Release -R "startup_orchestrator_lifecycle_test" --output-on-failure
```

Expected:

- the test fails because `main.cpp` still creates `MainInterfaceWidget` eagerly and does not defer the main-interface phase

- [ ] **Step 3: Introduce a main-interface factory and external runtime bindings**

```cpp
// UI/MainInterfaceFactory.h
#pragma once

#include <memory>

class MainInterfaceWidget;
class INavigationFacadePort;
class PlatformStateStore;
class PlatformLifecycleTraceRecorder;

struct MainInterfaceRuntimeBindings
{
    INavigationFacadePort* navigationPort = nullptr;
    PlatformStateStore* stateStore = nullptr;
    PlatformLifecycleTraceRecorder* lifecycleRecorder = nullptr;
};

std::unique_ptr<MainInterfaceWidget> createMainInterface(
    const MainInterfaceRuntimeBindings& bindings);
```

```cpp
// UI/MainInterfaceWidget.h
struct MainInterfaceRuntimeBindings;

class MainInterfaceWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MainInterfaceWidget(const MainInterfaceRuntimeBindings& bindings, QWidget* parent = nullptr);
    PlatformStateStore* platformStateStore();
    PlatformLifecycleTraceRecorder* lifecycleTraceRecorder();

private:
    PlatformStateStore* m_platformStateStore = nullptr;
    PlatformLifecycleTraceRecorder* m_lifecycleTraceRecorder = nullptr;
    std::unique_ptr<PlatformDiagnosticsService> m_platformDiagnosticsService;
};
```

```cpp
// UI/MainInterfaceWidget.cpp
MainInterfaceWidget::MainInterfaceWidget(const MainInterfaceRuntimeBindings& bindings, QWidget* parent)
    : QWidget(parent)
    , m_platformStateStore(bindings.stateStore)
    , m_lifecycleTraceRecorder(bindings.lifecycleRecorder)
{
    Q_ASSERT(bindings.navigationPort);
    Q_ASSERT(m_platformStateStore);
    Q_ASSERT(m_lifecycleTraceRecorder);

    m_navigationPort = bindings.navigationPort;
    m_platformDiagnosticsService = std::make_unique<PlatformDiagnosticsService>(m_platformStateStore);
    setupUI();
    setupConnections();
    navigateToPage(PAGE_MODULE_SELECTION);
}

PlatformStateStore* MainInterfaceWidget::platformStateStore()
{
    return m_platformStateStore;
}

PlatformLifecycleTraceRecorder* MainInterfaceWidget::lifecycleTraceRecorder()
{
    return m_lifecycleTraceRecorder;
}
```

```cpp
// UI/MainInterfaceFactory.cpp
#include "UI/MainInterfaceFactory.h"
#include "UI/MainInterfaceWidget.h"

std::unique_ptr<MainInterfaceWidget> createMainInterface(const MainInterfaceRuntimeBindings& bindings)
{
    return std::make_unique<MainInterfaceWidget>(bindings, nullptr);
}
```

- [ ] **Step 4: Replace eager main-interface startup in `main.cpp` with shell-first orchestration**

```cpp
// main.cpp
auto startupShell = std::make_unique<StartupShell>();
auto bootstrapController = std::make_unique<StartupBootstrapController>();
startupShell->show();
bootstrapController->beginBoot(QStringLiteral("Startup shell shown"), QStringLiteral("系统初始化中"));

PlatformStateStore startupStateStore;
startupStateStore.replaceDescriptors(descriptors);
startupStateStore.setRuntimeMode(runtimeConfig.runtimeMode);
startupStateStore.setStartupScopePluginIds(managedPlan.managedPluginIds);
startupStateStore.setGovernedPluginIds(governedPluginIds);
startupContext->stateStore = &startupStateStore;

QObject::connect(
    bootstrapController.get(),
    &StartupBootstrapController::snapshotChanged,
    startupShell.get(),
    [shell = startupShell.get()](const StartupShellSnapshot& snapshot) {
        shell->applySnapshot(snapshot);
    });

QObject::connect(
    startupShell.get(),
    &StartupShell::enterSystemRequested,
    &app,
    [startupShell = startupShell.get(), startupContext]() mutable {
        auto mainInterface = createMainInterface({
            startupContext->navigationAdapter.get(),
            startupContext->stateStore,
            &startupContext->lifecycleRecorder
        });
        mainInterface->setAttribute(Qt::WA_DeleteOnClose, true);
        mainInterface->showFullScreen();
        startupShell->hide();
        mainInterface.release();
    });

QObject::connect(
    startupShell.get(),
    &StartupShell::retryStartupRequested,
    &app,
    [bootstrapController = bootstrapController.get(), ctkManager]() {
        ctkManager->stopFramework();
        bootstrapController->resetForRetry();
        bootstrapController->beginBoot(QStringLiteral("CTK framework initialization"));
    });

orchestrator->registerPhaseHandler(
    StartupPhase::MainUICreation,
    [bootstrapController = bootstrapController.get()](QApplication*) -> StartupOrchestrator::PhaseExecutionResult {
        bootstrapController->updateBootStage(
            QStringLiteral("Main interface deferred"),
            QStringLiteral("首屏已显示，主界面延迟到进入系统时创建"));
        return StartupOrchestrator::PhaseExecutionResult::skipped(
            QStringLiteral("Main interface deferred until enter"),
            QStringLiteral("main_interface_deferred"));
    });

orchestrator->registerPhaseHandler(
    StartupPhase::CriticalPluginStart,
    [startupContext, bootstrapController = bootstrapController.get(), ctkManager, applyPluginState](QApplication*) -> StartupOrchestrator::PhaseExecutionResult {
        bootstrapController->updateBootStage(
            QStringLiteral("Critical plugin activation"),
            QStringLiteral("正在准备主流程插件"));

        for (const auto& entry : startupContext->managedPlan.installEntries) {
            if (!startupContext->startupCoordinator.startCorePlugin(entry.pluginId)) {
                bootstrapController->markFailed(
                    QStringLiteral("Critical plugin activation failed"),
                    { QStringLiteral("Retry startup or inspect diagnostics.") });
                applyPluginState(startupContext->resolveByPlatformPluginId(entry.pluginId), PlatformPluginState::Failed);
                return {
                    false,
                    PlatformLifecycleResult::Failed,
                    QStringLiteral("critical_plugin_start_failed"),
                    QStringLiteral("Critical plugin activation failed")
                };
            }
            const auto outcome = startupContext->startupCoordinator.waitForServiceReady(
                entry,
                [ctkManager](const QStringList& requiredServices) {
                    return ctkManager->getMissingServices(requiredServices);
                },
                [startupContext, ctkManager](const QString& pluginId) {
                    return startupContext->missingRequiredPlugins(pluginId, ctkManager);
                },
                [startupContext, ctkManager](const QString& pluginId) {
                    return startupContext->missingRequiredCapabilities(pluginId, ctkManager);
                });
            if (!outcome.success) {
                bootstrapController->markFailed(outcome.detail, { QStringLiteral("Retry startup or inspect diagnostics.") });
                return {
                    false,
                    PlatformLifecycleResult::Failed,
                    outcome.reasonCode,
                    outcome.detail
                };
            }
        }

        bootstrapController->markReady();
        return {
            true,
            PlatformLifecycleResult::Succeeded,
            QStringLiteral("phase1_platform_ready"),
            QStringLiteral("Managed core plugins are ready")
        };
    });
```

- [ ] **Step 5: Re-run lifecycle and platform-governance tests**

Run:

```powershell
cmake --build build_x64 --config Release --target medicalpro startup_orchestrator_lifecycle_test platform_startup_coordinator_test platform_diagnostics_service_test
ctest --test-dir build_x64 -C Release -R "startup_orchestrator_lifecycle_test|platform_startup_coordinator_test|platform_diagnostics_service_test|welcome_page_bootstrap_state_test|startup_bootstrap_controller_test" --output-on-failure
```

Expected:

- all listed tests PASS
- `StartupOrchestrator` still reports governed phase completion, but `Main interface creation` is now deferred instead of eagerly building `MainInterfaceWidget`

- [ ] **Step 6: Commit shell-first cold-start integration**

```powershell
git add UI/MainInterfaceFactory.h UI/MainInterfaceFactory.cpp UI/MainInterfaceWidget.h UI/MainInterfaceWidget.cpp main.cpp tests/unit/StartupOrchestratorLifecycleTest.cpp
git commit -m "feat: defer main interface until shell startup is ready"
```

### Task 4: Add Runtime Smoke Coverage And Write Back Acceptance

**Files:**
- Modify: `tests/CMakeLists.txt`
- Create: `tests/runtime/verify_cold_start_welcome_shell.ps1`
- Modify: `docs/current_status_and_project_overview.md`
- Modify: `docs/superpowers/tracking/platform-migration-decision-log.md`
- Modify: `docs/superpowers/plans/2026-04-21-cold-start-welcome-shell-implementation.md`

- [ ] **Step 1: Register a runtime smoke test for shell-first cold start**

```cmake
# tests/CMakeLists.txt
if(TARGET medicalpro)
    if(WIN32)
        find_program(POWERSHELL_EXECUTABLE NAMES pwsh powershell REQUIRED)
        add_test(
            NAME cold_start_welcome_shell_smoke_test
            COMMAND ${POWERSHELL_EXECUTABLE}
                -NoProfile
                -ExecutionPolicy Bypass
                -File ${CMAKE_CURRENT_SOURCE_DIR}/runtime/verify_cold_start_welcome_shell.ps1
                -ExePath $<TARGET_FILE:medicalpro>
                -WorkingDirectory $<TARGET_FILE_DIR:medicalpro>
        )
    endif()
endif()
```

```powershell
# tests/runtime/verify_cold_start_welcome_shell.ps1
param(
    [Parameter(Mandatory = $true)]
    [string]$ExePath,

    [Parameter(Mandatory = $true)]
    [string]$WorkingDirectory,

    [int]$TimeoutSeconds = 20
)

$stdoutPath = Join-Path $WorkingDirectory 'cold_start_welcome_shell_stdout.log'
$stderrPath = Join-Path $WorkingDirectory 'cold_start_welcome_shell_stderr.log'
Remove-Item $stdoutPath, $stderrPath -Force -ErrorAction SilentlyContinue

function Read-LogText {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path $Path)) { return '' }

    $stream = [System.IO.File]::Open(
        $Path,
        [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read,
        [System.IO.FileShare]::ReadWrite)
    try {
        $reader = New-Object System.IO.StreamReader($stream)
        try { return $reader.ReadToEnd() } finally { $reader.Dispose() }
    } finally {
        $stream.Dispose()
    }
}

$process = Start-Process `
    -FilePath $ExePath `
    -WorkingDirectory $WorkingDirectory `
    -RedirectStandardOutput $stdoutPath `
    -RedirectStandardError $stderrPath `
    -PassThru

$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
$shellShown = $false
$enterEnabled = $false
$startupCompleted = $false

try {
    while ([DateTime]::UtcNow -lt $deadline) {
        $combinedOutput = Read-LogText -Path $stdoutPath
        $stderrText = Read-LogText -Path $stderrPath
        if ($stderrText.Length -gt 0) {
            $combinedOutput += [Environment]::NewLine + $stderrText
        }

        if ($combinedOutput -match '\[StartupShell\] shown') { $shellShown = $true }
        if ($combinedOutput -match '\[StartupShell\] enter enabled') { $enterEnabled = $true }
        if ($combinedOutput -match 'Executing phase: Startup complete') { $startupCompleted = $true }

        if ($shellShown -and $enterEnabled -and $startupCompleted) { break }
        if ($process.HasExited) { break }

        Start-Sleep -Milliseconds 250
    }

    if (-not $shellShown) {
        throw 'cold_start_welcome_shell_smoke_failed: Startup shell was not shown'
    }
    if (-not $startupCompleted) {
        throw 'cold_start_welcome_shell_smoke_failed: Startup complete marker not observed'
    }
    if (-not $enterEnabled) {
        throw 'cold_start_welcome_shell_smoke_failed: Enter System was not enabled after readiness'
    }

    $shellIndex = $combinedOutput.IndexOf('[StartupShell] shown')
    $startupCompleteIndex = $combinedOutput.IndexOf('Executing phase: Startup complete')
    if ($shellIndex -gt $startupCompleteIndex) {
        throw 'cold_start_welcome_shell_smoke_failed: Startup shell appeared after Startup complete'
    }

    Write-Host 'Cold start welcome shell smoke passed'
}
finally {
    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
        Wait-Process -Id $process.Id -ErrorAction SilentlyContinue
    }
}
```

- [ ] **Step 2: Run the new runtime smoke and full focused acceptance**

Run:

```powershell
cmake --build build_x64 --config Release --target medicalpro
ctest --test-dir build_x64 -C Release -R "welcome_page_bootstrap_state_test|startup_bootstrap_controller_test|startup_orchestrator_lifecycle_test|cold_start_welcome_shell_smoke_test|platform_startup_coordinator_test|platform_diagnostics_service_test" --output-on-failure
```

Expected:

- all listed tests PASS
- the runtime smoke proves the shell appears before `Startup complete`

- [ ] **Step 3: Write back current status, decision log, and plan completion state**

```md
<!-- docs/current_status_and_project_overview.md -->
### 2026-04-21 Cold Start Welcome Shell Acceptance

- `Welcome` now renders through a lightweight startup shell before the heavy main interface is created.
- `Enter System` stays disabled until the existing Phase 1 managed startup scope reaches `platformReady`.
- `MainInterfaceWidget` is now created lazily after readiness and only when the user chooses to enter the system.
- Shell failure handling now keeps `Welcome` visible and exposes retry plus diagnostics.
```

```md
<!-- docs/superpowers/tracking/platform-migration-decision-log.md -->
## 2026-04-21

- Decision: optimize cold-start UX through a shell-first welcome host instead of further stretching `MainInterfaceWidget` as the cold-start root.
- Rationale: first paint was still coupled to heavy main-interface construction even though governance truth was already stable enough to bootstrap in the background.
- Impact: `Welcome` can now appear before full startup completion, while Phase 1 `platformReady` remains the only enter gate.
```

```md
<!-- docs/superpowers/plans/2026-04-21-cold-start-welcome-shell-implementation.md -->
Status update 2026-04-21:

- Completed. Cold start now shows `Welcome` before `Startup complete`.
- `Enter System` remains locked until Phase 1 managed readiness is achieved.
- Runtime smoke and focused governance acceptance passed.
```

- [ ] **Step 4: Commit runtime smoke and governance write-back**

```powershell
git add tests/CMakeLists.txt tests/runtime/verify_cold_start_welcome_shell.ps1 docs/current_status_and_project_overview.md docs/superpowers/tracking/platform-migration-decision-log.md docs/superpowers/plans/2026-04-21-cold-start-welcome-shell-implementation.md
git commit -m "test: verify cold start welcome shell behavior"
```

## Self-Review

- Spec coverage:
  - shell-first welcome rendering: Task 2 and Task 3
  - `Enter System` locked until Phase 1 `platformReady`: Task 1, Task 2, and Task 3
  - failure reason, retry, and diagnostics on the shell: Task 2 and Task 4
  - lazy `MainInterfaceWidget` creation after readiness: Task 3
  - runtime smoke and governance write-back: Task 4
- Placeholder scan:
  - no open markers or vague “handle this” steps remain
  - every task includes exact files, commands, and concrete code snippets
- Type consistency:
  - shell types stay aligned around `StartupShellState`, `StartupShellSnapshot`, and `StartupBootstrapController`
  - the enter gate remains `platformReady` for the existing Phase 1 managed scope
  - the runtime smoke test name is consistent: `cold_start_welcome_shell_smoke_test`
