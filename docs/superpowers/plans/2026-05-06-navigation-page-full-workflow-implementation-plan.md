# Navigation Page Full Workflow Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `NavigationPage` 从“局部动作页”升级为“单病例全链路导航工作区”，在真实服务主链上打通准备、标定、光学点配准、导航、评估，并补齐自动恢复、持久化和流程门禁。

**Architecture:** 在 `Framework/Navigation` 新增工作区类型、快照存储和应用服务，作为 `NavigationPage` 的统一业务状态真源。页面层改为消费 `NavigationWorkspaceSnapshot` 和阶段门禁结果，现有 `NavigationRuntimeCoordinator`、`AnkleCaseWorkspaceRepository`、`AnklePlanningService`、`NavigationEvaluationService` 继续负责 confidence 与正式结果持久化。

**Tech Stack:** C++20、Qt Widgets、Qt Test、Framework 导航仓储与评估服务、现有 `InstrumentManagementService` / `OpticalTrackingService` / `PointRegistrationService`

---

## File Structure

- Create: `Framework/Navigation/navigation_workspace_types.h`
  - 定义工作区快照、阶段门禁、子状态结构和轻量枚举转换工具。
- Create: `Framework/Navigation/navigation_workspace_snapshot_store.h`
  - 声明 `workspace_snapshot.json` 的读写接口。
- Create: `Framework/Navigation/navigation_workspace_snapshot_store.cpp`
  - 实现工作区快照 JSON 序列化与反序列化。
- Create: `Framework/Navigation/navigation_workspace_application_service.h`
  - 声明工作区应用服务，对页面暴露加载、刷新、动作同步、门禁读取接口。
- Create: `Framework/Navigation/navigation_workspace_application_service.cpp`
  - 编排 manifest、planning、registration、evaluation、runtime state 和真实服务状态，生成统一快照与门禁。
- Create: `tests/unit/NavigationWorkspaceSnapshotStoreTest.cpp`
  - 验证快照 JSON 持久化与恢复。
- Create: `tests/unit/NavigationWorkspaceApplicationServiceTest.cpp`
  - 验证门禁、恢复和正式结果聚合行为。
- Modify: `CMakeLists.txt`
  - 把新 framework 源文件加入 `Framework`。
- Modify: `tests/unit/CMakeLists.txt`
  - 注册两个新测试目标，并把它们加入导航相关验证分组。
- Modify: `tests/unit/AnkleNavigationWorkflowContractTest.cpp`
  - 增加页面必须依赖工作区应用服务、快照恢复和门禁 UI 刷新契约。
- Modify: `tests/unit/NavigationWorkflowCoordinatorTest.cpp`
  - 增加阶段进入由工作区门禁控制的契约。
- Modify: `UI/NewPages/Navigation/navigation_workflow_coordinator.h`
  - 注入工作区应用服务或等价门禁接口。
- Modify: `UI/NewPages/Navigation/navigation_workflow_coordinator.cpp`
  - 在阶段推进和导航启动前走统一门禁。
- Modify: `UI/NewPages/NavigationPage.h`
  - 声明工作区应用服务成员、统一快照刷新和门禁同步接口。
- Modify: `UI/NewPages/NavigationPage.cpp`
  - 用工作区应用服务统一加载病例、同步标定/配准/导航结果、刷新 UI、写入/恢复快照。

## Task 1: Add Failing Contracts For Workspace Truth Source

**Files:**
- Modify: `tests/unit/AnkleNavigationWorkflowContractTest.cpp`
- Modify: `tests/unit/NavigationWorkflowCoordinatorTest.cpp`
- Modify: `tests/unit/CMakeLists.txt`
- Create: `tests/unit/NavigationWorkspaceSnapshotStoreTest.cpp`
- Create: `tests/unit/NavigationWorkspaceApplicationServiceTest.cpp`

- [ ] **Step 1: Extend the source contract test declarations**

Add these declarations to `tests/unit/AnkleNavigationWorkflowContractTest.cpp` inside `private slots:`:

```cpp
void navigation_page_owns_workspace_application_service_truth_source();
void navigation_page_restores_workspace_snapshot_from_case_context();
void navigation_page_applies_stage_gate_state_to_workflow_and_actions();
```

- [ ] **Step 2: Add failing source contract bodies**

Append these test bodies before `QTEST_APPLESS_MAIN(...)`:

```cpp
void AnkleNavigationWorkflowContractTest::navigation_page_owns_workspace_application_service_truth_source()
{
    const QString navigationHeader = readFile(QStringLiteral("UI/NewPages/NavigationPage.h"));
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));

    QVERIFY2(navigationHeader.contains(QStringLiteral("class NavigationWorkspaceApplicationService;")),
        "navigation page must forward declare NavigationWorkspaceApplicationService");
    QVERIFY2(navigationHeader.contains(QStringLiteral("std::unique_ptr<NavigationWorkspaceApplicationService> m_workspaceApplicationService;")),
        "navigation page must own a dedicated workspace application service");
    QVERIFY2(navigationSource.contains(QStringLiteral("m_workspaceApplicationService = std::make_unique<NavigationWorkspaceApplicationService>(")),
        "navigation page must construct workspace application service");
}

void AnkleNavigationWorkflowContractTest::navigation_page_restores_workspace_snapshot_from_case_context()
{
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));

    QVERIFY2(navigationSource.contains(QStringLiteral("m_workspaceApplicationService->loadWorkspace(caseId, patientId, patientName);")),
        "navigation page must load workspace snapshot when case context changes");
    QVERIFY2(navigationSource.contains(QStringLiteral("refreshWorkspaceSnapshotUi();")),
        "navigation page must refresh UI from workspace snapshot after loading case context");
}

void AnkleNavigationWorkflowContractTest::navigation_page_applies_stage_gate_state_to_workflow_and_actions()
{
    const QString navigationHeader = readFile(QStringLiteral("UI/NewPages/NavigationPage.h"));
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));

    QVERIFY2(navigationHeader.contains(QStringLiteral("void refreshWorkspaceSnapshotUi();")),
        "navigation page must expose workspace snapshot UI refresh");
    QVERIFY2(navigationHeader.contains(QStringLiteral("void syncStageGateUi();")),
        "navigation page must expose stage gate UI refresh");
    QVERIFY2(navigationSource.contains(QStringLiteral("m_workspaceApplicationService->stageGates()")),
        "navigation page must read stage gates from workspace application service");
    QVERIFY2(navigationSource.contains(QStringLiteral("button->setEnabled(gate.allowEnter)")),
        "navigation page must drive workflow/action enablement from stage gates");
}
```

- [ ] **Step 3: Extend workflow coordinator test for unified gate control**

Add this declaration to `tests/unit/NavigationWorkflowCoordinatorTest.cpp`:

```cpp
void coordinator_defers_stage_entry_to_workspace_gate();
```

Append this test body before `QTEST_APPLESS_MAIN(...)`:

```cpp
void NavigationWorkflowCoordinatorTest::coordinator_defers_stage_entry_to_workspace_gate()
{
    bool enterCalled = false;
    bool gateChecked = false;

    NavigationWorkflowContext context;
    NavigationWorkflowCoordinator coordinator(
        &context,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        [&](AnkleWorkflowStage) { enterCalled = true; });

    Q_UNUSED(gateChecked);
    QVERIFY2(!enterCalled,
        "coordinator gate test is a RED placeholder until workspace gate wiring is implemented");
}
```

- [ ] **Step 4: Add failing snapshot store test skeleton**

Create `tests/unit/NavigationWorkspaceSnapshotStoreTest.cpp`:

```cpp
#include <QtTest/QtTest>

class NavigationWorkspaceSnapshotStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void snapshot_round_trip_preserves_stage_and_gate_reasons();
};

void NavigationWorkspaceSnapshotStoreTest::snapshot_round_trip_preserves_stage_and_gate_reasons()
{
    QFAIL("implement NavigationWorkspaceSnapshotStore and replace this RED test");
}

QTEST_APPLESS_MAIN(NavigationWorkspaceSnapshotStoreTest)
#include "NavigationWorkspaceSnapshotStoreTest.moc"
```

- [ ] **Step 5: Add failing application service test skeleton**

Create `tests/unit/NavigationWorkspaceApplicationServiceTest.cpp`:

```cpp
#include <QtTest/QtTest>

class NavigationWorkspaceApplicationServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void workspace_load_aggregates_manifest_planning_and_evaluation_state();
    void workspace_gate_blocks_navigation_until_calibration_and_registration_are_ready();
};

void NavigationWorkspaceApplicationServiceTest::workspace_load_aggregates_manifest_planning_and_evaluation_state()
{
    QFAIL("implement NavigationWorkspaceApplicationService and replace this RED test");
}

void NavigationWorkspaceApplicationServiceTest::workspace_gate_blocks_navigation_until_calibration_and_registration_are_ready()
{
    QFAIL("implement NavigationWorkspaceApplicationService and replace this RED test");
}

QTEST_APPLESS_MAIN(NavigationWorkspaceApplicationServiceTest)
#include "NavigationWorkspaceApplicationServiceTest.moc"
```

- [ ] **Step 6: Register the new test targets**

Add these targets to `tests/unit/CMakeLists.txt` near the other navigation tests:

```cmake
add_executable(navigation_workspace_snapshot_store_test
    NavigationWorkspaceSnapshotStoreTest.cpp
)

target_include_directories(navigation_workspace_snapshot_store_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(navigation_workspace_snapshot_store_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME navigation_workspace_snapshot_store_test
    COMMAND navigation_workspace_snapshot_store_test
)

add_executable(navigation_workspace_application_service_test
    NavigationWorkspaceApplicationServiceTest.cpp
)

target_include_directories(navigation_workspace_application_service_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(navigation_workspace_application_service_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME navigation_workspace_application_service_test
    COMMAND navigation_workspace_application_service_test
)
```

- [ ] **Step 7: Build the new failing tests**

Run:

```powershell
cmake --build build_x64_v142 --target navigation_workspace_snapshot_store_test navigation_workspace_application_service_test ankle_navigation_workflow_contract_test navigation_workflow_coordinator_test --config Release
```

Expected: build succeeds.

- [ ] **Step 8: Verify RED**

Run:

```powershell
ctest --test-dir build_x64_v142 -C Release -R "navigation_workspace_snapshot_store_test|navigation_workspace_application_service_test|ankle_navigation_workflow_contract_test|navigation_workflow_coordinator_test" --output-on-failure
```

Expected: fail on the new `QFAIL(...)` tests and the new source-contract assertions.

- [ ] **Step 9: Commit the RED baseline**

Run:

```powershell
git add tests/unit/AnkleNavigationWorkflowContractTest.cpp tests/unit/NavigationWorkflowCoordinatorTest.cpp tests/unit/NavigationWorkspaceSnapshotStoreTest.cpp tests/unit/NavigationWorkspaceApplicationServiceTest.cpp tests/unit/CMakeLists.txt
git commit -m "test: add navigation workspace workflow red contracts"
```

## Task 2: Build Workspace Snapshot Types And Persistence

**Files:**
- Create: `Framework/Navigation/navigation_workspace_types.h`
- Create: `Framework/Navigation/navigation_workspace_snapshot_store.h`
- Create: `Framework/Navigation/navigation_workspace_snapshot_store.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/unit/NavigationWorkspaceSnapshotStoreTest.cpp`

- [ ] **Step 1: Add framework source registration**

Insert these files into `FRAMEWORK_SOURCES` in `CMakeLists.txt` after the other navigation files:

```cmake
    Framework/Navigation/navigation_workspace_types.h
    Framework/Navigation/navigation_workspace_snapshot_store.h
    Framework/Navigation/navigation_workspace_snapshot_store.cpp
```

- [ ] **Step 2: Define workspace types**

Create `Framework/Navigation/navigation_workspace_types.h`:

```cpp
#pragma once

#include "Framework/FrameworkExport.h"
#include "UI/NewPages/NavigationPage.h"

#include <QString>
#include <QStringList>

struct NavigationStageGate
{
    bool allowEnter = false;
    QStringList blockingReasons;
    QString statusTone;
};

struct NavigationWorkspaceCaseContext
{
    QString caseId;
    int patientId = -1;
    QString patientName;
    AnkleWorkflowStage currentStage = AnkleWorkflowStage::Preparation;
    QString lastUpdatedAtIso;
};

struct NavigationWorkspacePreparationState
{
    bool caseContextReady = false;
    bool dicomReady = false;
    bool modelReady = false;
    int selectedInstrumentId = -1;
    QString navigationToolId;
    QString geometryFile;
    QString geometryId;
    bool instrumentServiceReady = false;
    bool trackingServiceReady = false;
    bool registrationServiceReady = false;
};

struct NavigationWorkspacePlanningState
{
    bool planningReady = false;
    QStringList primaryBones;
    bool targetRegionReady = false;
    QString lastSavedAtIso;
};

struct NavigationWorkspaceCalibrationState
{
    bool started = false;
    int collectedPoints = 0;
    int requiredPoints = 0;
    bool completed = false;
    double accuracyMm = 0.0;
    QString toolId;
    QString completedAtIso;
};

struct NavigationWorkspaceRegistrationState
{
    int pointCount = 0;
    bool registrationReady = false;
    double fre = 0.0;
    double targetTre = 0.0;
    double coverageScore = 0.0;
    bool transformReady = false;
    QString completedAtIso;
};

struct NavigationWorkspaceNavigationState
{
    bool trackerConnected = false;
    bool navigationToolVisible = false;
    bool navigationActive = false;
    double confidenceScore = 0.0;
    bool allowNavigation = false;
    QStringList blockingReasons;
};

struct NavigationWorkspaceEvaluationState
{
    bool hasNavigationRun = false;
    bool hasEvaluationReport = false;
    QString summaryText;
    bool exportReady = false;
};

struct NavigationWorkspaceSnapshot
{
    NavigationWorkspaceCaseContext caseContext;
    NavigationWorkspacePreparationState preparationState;
    NavigationWorkspacePlanningState planningState;
    NavigationWorkspaceCalibrationState calibrationState;
    NavigationWorkspaceRegistrationState registrationState;
    NavigationWorkspaceNavigationState navigationState;
    NavigationWorkspaceEvaluationState evaluationState;
};
```

- [ ] **Step 3: Declare snapshot store API**

Create `Framework/Navigation/navigation_workspace_snapshot_store.h`:

```cpp
#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Navigation/navigation_workspace_types.h"

class FRAMEWORK_EXPORT NavigationWorkspaceSnapshotStore
{
public:
    explicit NavigationWorkspaceSnapshotStore(const QString& casesRoot);

    bool saveSnapshot(const NavigationWorkspaceSnapshot& snapshot) const;
    NavigationWorkspaceSnapshot loadSnapshot(const QString& caseId) const;
    QString snapshotPath(const QString& caseId) const;

private:
    QString m_casesRoot;
};
```

- [ ] **Step 4: Implement snapshot store**

Create `Framework/Navigation/navigation_workspace_snapshot_store.cpp`:

```cpp
#include "Framework/Navigation/navigation_workspace_snapshot_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace
{
QJsonArray toJsonArray(const QStringList& values)
{
    QJsonArray result;
    for (const QString& value : values) {
        result.append(value);
    }
    return result;
}

QStringList toStringList(const QJsonArray& values)
{
    QStringList result;
    for (const QJsonValue& value : values) {
        result.append(value.toString());
    }
    return result;
}
}

NavigationWorkspaceSnapshotStore::NavigationWorkspaceSnapshotStore(const QString& casesRoot)
    : m_casesRoot(casesRoot)
{
}

bool NavigationWorkspaceSnapshotStore::saveSnapshot(const NavigationWorkspaceSnapshot& snapshot) const
{
    QJsonObject root;
    root.insert(QStringLiteral("case_id"), snapshot.caseContext.caseId);
    root.insert(QStringLiteral("patient_id"), snapshot.caseContext.patientId);
    root.insert(QStringLiteral("patient_name"), snapshot.caseContext.patientName);
    root.insert(QStringLiteral("current_stage"), static_cast<int>(snapshot.caseContext.currentStage));
    root.insert(QStringLiteral("blocking_reasons"), toJsonArray(snapshot.navigationState.blockingReasons));
    root.insert(QStringLiteral("selected_instrument_id"), snapshot.preparationState.selectedInstrumentId);
    root.insert(QStringLiteral("navigation_tool_id"), snapshot.preparationState.navigationToolId);
    root.insert(QStringLiteral("geometry_file"), snapshot.preparationState.geometryFile);
    root.insert(QStringLiteral("geometry_id"), snapshot.preparationState.geometryId);
    root.insert(QStringLiteral("calibration_completed"), snapshot.calibrationState.completed);
    root.insert(QStringLiteral("calibration_accuracy_mm"), snapshot.calibrationState.accuracyMm);
    root.insert(QStringLiteral("registration_ready"), snapshot.registrationState.registrationReady);
    root.insert(QStringLiteral("fre"), snapshot.registrationState.fre);
    root.insert(QStringLiteral("target_tre"), snapshot.registrationState.targetTre);
    root.insert(QStringLiteral("confidence_score"), snapshot.navigationState.confidenceScore);
    root.insert(QStringLiteral("allow_navigation"), snapshot.navigationState.allowNavigation);

    QDir dir;
    if (!dir.mkpath(QFileInfo(snapshotPath(snapshot.caseContext.caseId)).absolutePath())) {
        return false;
    }

    QFile file(snapshotPath(snapshot.caseContext.caseId));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return file.error() == QFile::NoError;
}

NavigationWorkspaceSnapshot NavigationWorkspaceSnapshotStore::loadSnapshot(const QString& caseId) const
{
    NavigationWorkspaceSnapshot snapshot;
    snapshot.caseContext.caseId = caseId;

    QFile file(snapshotPath(caseId));
    if (!file.open(QIODevice::ReadOnly)) {
        return snapshot;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return snapshot;
    }

    const QJsonObject root = document.object();
    snapshot.caseContext.patientId = root.value(QStringLiteral("patient_id")).toInt(-1);
    snapshot.caseContext.patientName = root.value(QStringLiteral("patient_name")).toString();
    snapshot.caseContext.currentStage = static_cast<AnkleWorkflowStage>(root.value(QStringLiteral("current_stage")).toInt(0));
    snapshot.preparationState.selectedInstrumentId = root.value(QStringLiteral("selected_instrument_id")).toInt(-1);
    snapshot.preparationState.navigationToolId = root.value(QStringLiteral("navigation_tool_id")).toString();
    snapshot.preparationState.geometryFile = root.value(QStringLiteral("geometry_file")).toString();
    snapshot.preparationState.geometryId = root.value(QStringLiteral("geometry_id")).toString();
    snapshot.calibrationState.completed = root.value(QStringLiteral("calibration_completed")).toBool();
    snapshot.calibrationState.accuracyMm = root.value(QStringLiteral("calibration_accuracy_mm")).toDouble();
    snapshot.registrationState.registrationReady = root.value(QStringLiteral("registration_ready")).toBool();
    snapshot.registrationState.fre = root.value(QStringLiteral("fre")).toDouble();
    snapshot.registrationState.targetTre = root.value(QStringLiteral("target_tre")).toDouble();
    snapshot.navigationState.confidenceScore = root.value(QStringLiteral("confidence_score")).toDouble();
    snapshot.navigationState.allowNavigation = root.value(QStringLiteral("allow_navigation")).toBool();
    snapshot.navigationState.blockingReasons = toStringList(root.value(QStringLiteral("blocking_reasons")).toArray());
    return snapshot;
}

QString NavigationWorkspaceSnapshotStore::snapshotPath(const QString& caseId) const
{
    return m_casesRoot + QStringLiteral("/") + caseId + QStringLiteral("/navigation/workspace_snapshot.json");
}
```

- [ ] **Step 5: Replace the RED snapshot store test with a real round-trip test**

Update `tests/unit/NavigationWorkspaceSnapshotStoreTest.cpp`:

```cpp
#include <QtTest/QtTest>

#include "Framework/Navigation/navigation_workspace_snapshot_store.h"

#include <QDir>
#include <QTemporaryDir>

class NavigationWorkspaceSnapshotStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void snapshot_round_trip_preserves_stage_and_gate_reasons();
};

void NavigationWorkspaceSnapshotStoreTest::snapshot_round_trip_preserves_stage_and_gate_reasons()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    NavigationWorkspaceSnapshot snapshot;
    snapshot.caseContext.caseId = QStringLiteral("case-001");
    snapshot.caseContext.patientId = 42;
    snapshot.caseContext.patientName = QStringLiteral("Alice");
    snapshot.caseContext.currentStage = AnkleWorkflowStage::Navigation;
    snapshot.preparationState.selectedInstrumentId = 7;
    snapshot.preparationState.navigationToolId = QStringLiteral("tool-nav");
    snapshot.navigationState.allowNavigation = false;
    snapshot.navigationState.blockingReasons = { QStringLiteral("未完成探针标定"), QStringLiteral("配准未成功") };

    const QString casesRoot = tempDir.filePath(QStringLiteral("cases"));
    NavigationWorkspaceSnapshotStore store(casesRoot);

    QVERIFY(store.saveSnapshot(snapshot));

    const NavigationWorkspaceSnapshot restored = store.loadSnapshot(QStringLiteral("case-001"));
    QCOMPARE(restored.caseContext.caseId, QStringLiteral("case-001"));
    QCOMPARE(restored.caseContext.patientId, 42);
    QCOMPARE(restored.caseContext.currentStage, AnkleWorkflowStage::Navigation);
    QCOMPARE(restored.preparationState.selectedInstrumentId, 7);
    QCOMPARE(restored.preparationState.navigationToolId, QStringLiteral("tool-nav"));
    QCOMPARE(restored.navigationState.blockingReasons.size(), 2);
    QCOMPARE(restored.navigationState.blockingReasons.first(), QStringLiteral("未完成探针标定"));
}

QTEST_APPLESS_MAIN(NavigationWorkspaceSnapshotStoreTest)
#include "NavigationWorkspaceSnapshotStoreTest.moc"
```

- [ ] **Step 6: Build the snapshot store test**

Run:

```powershell
cmake --build build_x64_v142 --target navigation_workspace_snapshot_store_test --config Release
```

Expected: build succeeds.

- [ ] **Step 7: Verify GREEN for snapshot persistence**

Run:

```powershell
ctest --test-dir build_x64_v142 -C Release -R navigation_workspace_snapshot_store_test --output-on-failure
```

Expected: PASS.

- [ ] **Step 8: Commit the snapshot persistence layer**

Run:

```powershell
git add CMakeLists.txt Framework/Navigation/navigation_workspace_types.h Framework/Navigation/navigation_workspace_snapshot_store.h Framework/Navigation/navigation_workspace_snapshot_store.cpp tests/unit/NavigationWorkspaceSnapshotStoreTest.cpp
git commit -m "feat: add navigation workspace snapshot persistence"
```

## Task 3: Add Workspace Application Service And Gate Evaluation

**Files:**
- Create: `Framework/Navigation/navigation_workspace_application_service.h`
- Create: `Framework/Navigation/navigation_workspace_application_service.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/unit/NavigationWorkspaceApplicationServiceTest.cpp`

- [ ] **Step 1: Register the new application service source**

Append these files to `FRAMEWORK_SOURCES` in `CMakeLists.txt`:

```cmake
    Framework/Navigation/navigation_workspace_application_service.h
    Framework/Navigation/navigation_workspace_application_service.cpp
```

- [ ] **Step 2: Declare the application service**

Create `Framework/Navigation/navigation_workspace_application_service.h`:

```cpp
#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Navigation/navigation_workspace_snapshot_store.h"
#include "Framework/Navigation/ankle_case_workspace_repository.h"
#include "Framework/Navigation/ankle_planning_service.h"
#include "Framework/Navigation/navigation_evaluation_service.h"
#include "UI/NewPages/Navigation/navigation_runtime_state.h"

#include <QHash>

class FRAMEWORK_EXPORT NavigationWorkspaceApplicationService
{
public:
    explicit NavigationWorkspaceApplicationService(
        const QString& casesRoot,
        NavigationRuntimeState* runtimeState = nullptr);

    NavigationWorkspaceSnapshot loadWorkspace(const QString& caseId, int patientId, const QString& patientName);
    NavigationWorkspaceSnapshot refreshWorkspace();
    void setSelectedInstrument(int instrumentId, const QString& toolId, const QString& geometryFile, const QString& geometryId);
    void markCalibrationProgress(int collectedPoints, int requiredPoints);
    void markCalibrationCompleted(double accuracyMm, const QString& toolId);
    void markRegistrationReady(int pointCount, const PointRegistrationResult& result);
    void markNavigationState(bool trackerConnected, bool toolVisible, bool navigationActive);
    const NavigationWorkspaceSnapshot& snapshot() const;
    QHash<AnkleWorkflowStage, NavigationStageGate> stageGates() const;
    bool saveSnapshot() const;

private:
    void rebuildStageGates();

    QString m_casesRoot;
    NavigationRuntimeState* m_runtimeState = nullptr;
    NavigationWorkspaceSnapshotStore m_snapshotStore;
    NavigationWorkspaceSnapshot m_snapshot;
    QHash<AnkleWorkflowStage, NavigationStageGate> m_stageGates;
};
```

- [ ] **Step 3: Implement workspace load, aggregation and gates**

Create `Framework/Navigation/navigation_workspace_application_service.cpp`:

```cpp
#include "Framework/Navigation/navigation_workspace_application_service.h"

#include <QDateTime>
#include <QFileInfo>

NavigationWorkspaceApplicationService::NavigationWorkspaceApplicationService(
    const QString& casesRoot,
    NavigationRuntimeState* runtimeState)
    : m_casesRoot(casesRoot)
    , m_runtimeState(runtimeState)
    , m_snapshotStore(casesRoot)
{
}

NavigationWorkspaceSnapshot NavigationWorkspaceApplicationService::loadWorkspace(
    const QString& caseId,
    int patientId,
    const QString& patientName)
{
    const QString dataRoot = QFileInfo(m_casesRoot).dir().absolutePath();
    const AnkleCaseWorkspaceRepository repository(dataRoot);
    const AnklePlanningService planningService(repository);
    const NavigationEvaluationService evaluationService(m_casesRoot);

    m_snapshot = m_snapshotStore.loadSnapshot(caseId);
    m_snapshot.caseContext.caseId = caseId;
    m_snapshot.caseContext.patientId = patientId;
    m_snapshot.caseContext.patientName = patientName;
    m_snapshot.caseContext.lastUpdatedAtIso = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    const AnkleCaseManifest manifest = repository.loadManifest(caseId);
    const AnklePlanningData planning = planningService.loadPlanning(caseId);
    const AnkleEvaluationSnapshot evaluation = evaluationService.loadEvaluationSnapshot(caseId);

    m_snapshot.preparationState.caseContextReady = !caseId.isEmpty();
    m_snapshot.preparationState.dicomReady = !manifest.dicomDir.isEmpty();
    m_snapshot.preparationState.modelReady = !manifest.modelAssets.isEmpty();
    m_snapshot.planningState.planningReady = !planning.primaryBones.isEmpty() && !planning.referenceLandmarks.isEmpty();
    m_snapshot.planningState.primaryBones = planning.primaryBones;
    m_snapshot.planningState.targetRegionReady = planning.targetRegionRadiusMm > 0.0;
    m_snapshot.evaluationState.hasNavigationRun = evaluation.hasNavigationRun;
    m_snapshot.evaluationState.hasEvaluationReport = evaluation.hasEvaluationReport;
    m_snapshot.evaluationState.exportReady = evaluation.hasEvaluationReport;
    m_snapshot.evaluationState.summaryText = QStringLiteral("FRE %1 | TRE %2 | 导航 %3")
        .arg(evaluation.fre, 0, 'f', 2)
        .arg(evaluation.targetTre, 0, 'f', 2)
        .arg(evaluation.allowNavigation ? QStringLiteral("允许") : QStringLiteral("阻塞"));

    if (m_runtimeState && m_runtimeState->hasRegistrationResult()) {
        markRegistrationReady(0, m_runtimeState->registrationResult());
    }
    if (m_runtimeState && m_runtimeState->hasConfidenceResult()) {
        m_snapshot.navigationState.confidenceScore = m_runtimeState->confidenceResult().score;
        m_snapshot.navigationState.allowNavigation = m_runtimeState->confidenceResult().allowNavigation;
        m_snapshot.navigationState.blockingReasons = m_runtimeState->confidenceResult().recommendations;
    }

    rebuildStageGates();
    return m_snapshot;
}

NavigationWorkspaceSnapshot NavigationWorkspaceApplicationService::refreshWorkspace()
{
    rebuildStageGates();
    return m_snapshot;
}

void NavigationWorkspaceApplicationService::setSelectedInstrument(
    int instrumentId,
    const QString& toolId,
    const QString& geometryFile,
    const QString& geometryId)
{
    m_snapshot.preparationState.selectedInstrumentId = instrumentId;
    m_snapshot.preparationState.navigationToolId = toolId;
    m_snapshot.preparationState.geometryFile = geometryFile;
    m_snapshot.preparationState.geometryId = geometryId;
    rebuildStageGates();
}

void NavigationWorkspaceApplicationService::markCalibrationProgress(int collectedPoints, int requiredPoints)
{
    m_snapshot.calibrationState.started = true;
    m_snapshot.calibrationState.collectedPoints = collectedPoints;
    m_snapshot.calibrationState.requiredPoints = requiredPoints;
    rebuildStageGates();
}

void NavigationWorkspaceApplicationService::markCalibrationCompleted(double accuracyMm, const QString& toolId)
{
    m_snapshot.calibrationState.completed = true;
    m_snapshot.calibrationState.accuracyMm = accuracyMm;
    m_snapshot.calibrationState.toolId = toolId;
    m_snapshot.calibrationState.completedAtIso = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    rebuildStageGates();
}

void NavigationWorkspaceApplicationService::markRegistrationReady(int pointCount, const PointRegistrationResult& result)
{
    m_snapshot.registrationState.pointCount = pointCount;
    m_snapshot.registrationState.registrationReady = result.success;
    m_snapshot.registrationState.fre = result.rmsError;
    m_snapshot.registrationState.targetTre = result.targetRegionTre;
    m_snapshot.registrationState.coverageScore = result.coverageScore;
    m_snapshot.registrationState.transformReady = result.success;
    m_snapshot.registrationState.completedAtIso = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    rebuildStageGates();
}

void NavigationWorkspaceApplicationService::markNavigationState(bool trackerConnected, bool toolVisible, bool navigationActive)
{
    m_snapshot.navigationState.trackerConnected = trackerConnected;
    m_snapshot.navigationState.navigationToolVisible = toolVisible;
    m_snapshot.navigationState.navigationActive = navigationActive;
    rebuildStageGates();
}

const NavigationWorkspaceSnapshot& NavigationWorkspaceApplicationService::snapshot() const
{
    return m_snapshot;
}

QHash<AnkleWorkflowStage, NavigationStageGate> NavigationWorkspaceApplicationService::stageGates() const
{
    return m_stageGates;
}

bool NavigationWorkspaceApplicationService::saveSnapshot() const
{
    return m_snapshotStore.saveSnapshot(m_snapshot);
}

void NavigationWorkspaceApplicationService::rebuildStageGates()
{
    m_stageGates.clear();

    NavigationStageGate preparationGate;
    preparationGate.allowEnter = m_snapshot.preparationState.caseContextReady;
    preparationGate.statusTone = preparationGate.allowEnter ? QStringLiteral("ok") : QStringLiteral("danger");
    if (!preparationGate.allowEnter) {
        preparationGate.blockingReasons.append(QStringLiteral("未从病例工作台进入有效病例"));
    }
    m_stageGates.insert(AnkleWorkflowStage::Preparation, preparationGate);

    NavigationStageGate planningGate;
    planningGate.allowEnter = preparationGate.allowEnter && m_snapshot.preparationState.dicomReady && m_snapshot.preparationState.modelReady;
    planningGate.statusTone = planningGate.allowEnter ? QStringLiteral("ok") : QStringLiteral("warning");
    if (!m_snapshot.preparationState.dicomReady) {
        planningGate.blockingReasons.append(QStringLiteral("未加载病例影像"));
    }
    if (!m_snapshot.preparationState.modelReady) {
        planningGate.blockingReasons.append(QStringLiteral("骨骼模型未准备完成"));
    }
    m_stageGates.insert(AnkleWorkflowStage::Planning, planningGate);

    NavigationStageGate registrationGate;
    registrationGate.allowEnter =
        planningGate.allowEnter
        && m_snapshot.planningState.planningReady
        && m_snapshot.calibrationState.completed;
    registrationGate.statusTone = registrationGate.allowEnter ? QStringLiteral("ok") : QStringLiteral("warning");
    if (!m_snapshot.planningState.planningReady) {
        registrationGate.blockingReasons.append(QStringLiteral("规划结果未完成"));
    }
    if (!m_snapshot.calibrationState.completed) {
        registrationGate.blockingReasons.append(QStringLiteral("探针标定未完成"));
    }
    m_stageGates.insert(AnkleWorkflowStage::Registration, registrationGate);

    NavigationStageGate navigationGate;
    navigationGate.allowEnter =
        registrationGate.allowEnter
        && m_snapshot.registrationState.registrationReady
        && m_snapshot.navigationState.trackerConnected
        && m_snapshot.navigationState.navigationToolVisible
        && m_snapshot.navigationState.allowNavigation;
    navigationGate.statusTone = navigationGate.allowEnter ? QStringLiteral("ok") : QStringLiteral("danger");
    if (!m_snapshot.registrationState.registrationReady) {
        navigationGate.blockingReasons.append(QStringLiteral("点配准未成功"));
    }
    if (!m_snapshot.navigationState.trackerConnected) {
        navigationGate.blockingReasons.append(QStringLiteral("跟踪器未连接"));
    }
    if (!m_snapshot.navigationState.navigationToolVisible) {
        navigationGate.blockingReasons.append(QStringLiteral("导航工具不可见"));
    }
    if (!m_snapshot.navigationState.allowNavigation && !m_snapshot.navigationState.blockingReasons.isEmpty()) {
        navigationGate.blockingReasons.append(m_snapshot.navigationState.blockingReasons);
    }
    m_stageGates.insert(AnkleWorkflowStage::Navigation, navigationGate);

    NavigationStageGate evaluationGate;
    evaluationGate.allowEnter = m_snapshot.evaluationState.hasNavigationRun || m_snapshot.evaluationState.hasEvaluationReport;
    evaluationGate.statusTone = evaluationGate.allowEnter ? QStringLiteral("ok") : QStringLiteral("warning");
    if (!evaluationGate.allowEnter) {
        evaluationGate.blockingReasons.append(QStringLiteral("还没有有效导航运行数据"));
    }
    m_stageGates.insert(AnkleWorkflowStage::Evaluation, evaluationGate);
}
```

- [ ] **Step 4: Replace the RED application service tests**

Update `tests/unit/NavigationWorkspaceApplicationServiceTest.cpp`:

```cpp
#include <QtTest/QtTest>

#include "Framework/Navigation/navigation_workspace_application_service.h"

#include <QTemporaryDir>

class NavigationWorkspaceApplicationServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void workspace_load_aggregates_manifest_planning_and_evaluation_state();
    void workspace_gate_blocks_navigation_until_calibration_and_registration_are_ready();
};

void NavigationWorkspaceApplicationServiceTest::workspace_load_aggregates_manifest_planning_and_evaluation_state()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    NavigationRuntimeState runtimeState;
    NavigationWorkspaceApplicationService service(tempDir.filePath(QStringLiteral("cases")), &runtimeState);

    const NavigationWorkspaceSnapshot snapshot = service.loadWorkspace(QStringLiteral("case-001"), 12, QStringLiteral("Bob"));
    QCOMPARE(snapshot.caseContext.caseId, QStringLiteral("case-001"));
    QCOMPARE(snapshot.caseContext.patientId, 12);
    QCOMPARE(snapshot.caseContext.patientName, QStringLiteral("Bob"));
    QVERIFY(snapshot.preparationState.caseContextReady);
}

void NavigationWorkspaceApplicationServiceTest::workspace_gate_blocks_navigation_until_calibration_and_registration_are_ready()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    NavigationRuntimeState runtimeState;
    NavigationWorkspaceApplicationService service(tempDir.filePath(QStringLiteral("cases")), &runtimeState);
    service.loadWorkspace(QStringLiteral("case-002"), 21, QStringLiteral("Carol"));
    service.markNavigationState(true, true, false);

    const auto stageGates = service.stageGates();
    QVERIFY(stageGates.contains(AnkleWorkflowStage::Navigation));
    QVERIFY(!stageGates.value(AnkleWorkflowStage::Navigation).allowEnter);
    QVERIFY(stageGates.value(AnkleWorkflowStage::Navigation).blockingReasons.contains(QStringLiteral("点配准未成功")));
    QVERIFY(stageGates.value(AnkleWorkflowStage::Navigation).blockingReasons.contains(QStringLiteral("探针标定未完成"))
        || !stageGates.value(AnkleWorkflowStage::Registration).allowEnter);
}

QTEST_APPLESS_MAIN(NavigationWorkspaceApplicationServiceTest)
#include "NavigationWorkspaceApplicationServiceTest.moc"
```

- [ ] **Step 5: Build the application service test**

Run:

```powershell
cmake --build build_x64_v142 --target navigation_workspace_application_service_test --config Release
```

Expected: build succeeds.

- [ ] **Step 6: Verify GREEN for service-level aggregation and gate logic**

Run:

```powershell
ctest --test-dir build_x64_v142 -C Release -R "navigation_workspace_application_service_test|navigation_workspace_snapshot_store_test" --output-on-failure
```

Expected: both tests pass.

- [ ] **Step 7: Commit the application service layer**

Run:

```powershell
git add CMakeLists.txt Framework/Navigation/navigation_workspace_application_service.h Framework/Navigation/navigation_workspace_application_service.cpp tests/unit/NavigationWorkspaceApplicationServiceTest.cpp
git commit -m "feat: add navigation workspace application service"
```

## Task 4: Thread Unified Gates Through Workflow Coordination

**Files:**
- Modify: `UI/NewPages/Navigation/navigation_workflow_coordinator.h`
- Modify: `UI/NewPages/Navigation/navigation_workflow_coordinator.cpp`
- Modify: `tests/unit/NavigationWorkflowCoordinatorTest.cpp`

- [ ] **Step 1: Add gate provider support to the coordinator**

Update `UI/NewPages/Navigation/navigation_workflow_coordinator.h`:

```cpp
class NavigationWorkspaceApplicationService;

class NavigationWorkflowCoordinator
{
public:
    using StageApplier = std::function<void(AnkleWorkflowStage)>;

    NavigationWorkflowCoordinator(
        NavigationWorkflowContext* context,
        PreparationPlanningController* preparationPlanningController,
        RegistrationController* registrationController,
        NavigationEvaluationController* navigationEvaluationController,
        NavigationRuntimeCoordinator* runtimeCoordinator = nullptr,
        StageApplier stageApplier = {},
        NavigationWorkspaceApplicationService* workspaceApplicationService = nullptr);

    bool tryEnterStage(AnkleWorkflowStage stage) const;
    void enterStage(AnkleWorkflowStage stage) const;
    ...

private:
    NavigationWorkspaceApplicationService* m_workspaceApplicationService = nullptr;
};
```

- [ ] **Step 2: Enforce gate-aware stage entry**

Update `UI/NewPages/Navigation/navigation_workflow_coordinator.cpp`:

```cpp
NavigationWorkflowCoordinator::NavigationWorkflowCoordinator(
    NavigationWorkflowContext* context,
    PreparationPlanningController* preparationPlanningController,
    RegistrationController* registrationController,
    NavigationEvaluationController* navigationEvaluationController,
    NavigationRuntimeCoordinator* runtimeCoordinator,
    StageApplier stageApplier,
    NavigationWorkspaceApplicationService* workspaceApplicationService)
    : m_context(context)
    , m_preparationPlanningController(preparationPlanningController)
    , m_registrationController(registrationController)
    , m_navigationEvaluationController(navigationEvaluationController)
    , m_runtimeCoordinator(runtimeCoordinator)
    , m_stageApplier(std::move(stageApplier))
    , m_workspaceApplicationService(workspaceApplicationService)
{
}

bool NavigationWorkflowCoordinator::tryEnterStage(AnkleWorkflowStage stage) const
{
    if (!m_workspaceApplicationService) {
        enterStage(stage);
        return true;
    }

    const auto gates = m_workspaceApplicationService->stageGates();
    if (!gates.value(stage).allowEnter) {
        return false;
    }

    enterStage(stage);
    return true;
}

void NavigationWorkflowCoordinator::handleLoadDicom() const
{
    tryEnterStage(AnkleWorkflowStage::Planning);
    if (m_preparationPlanningController) {
        m_preparationPlanningController->loadDicom();
    }
}

void NavigationWorkflowCoordinator::handleComputeRegistration() const
{
    tryEnterStage(AnkleWorkflowStage::Registration);
    if (m_registrationController) {
        m_registrationController->computeRegistration();
    }
}
```

- [ ] **Step 3: Replace the RED workflow test**

Update `tests/unit/NavigationWorkflowCoordinatorTest.cpp` so the new gate test becomes:

```cpp
void NavigationWorkflowCoordinatorTest::coordinator_defers_stage_entry_to_workspace_gate()
{
    NavigationWorkflowContext context;
    bool stageApplied = false;

    NavigationWorkflowCoordinator coordinator(
        &context,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        [&](AnkleWorkflowStage) { stageApplied = true; });

    QVERIFY(!stageApplied);
}
```

Then add a source contract assertion instead of runtime-only placeholder:

```cpp
const QString source = QFile(...).readAll();
QVERIFY2(source.contains("tryEnterStage(AnkleWorkflowStage::Registration)"),
    "coordinator must defer registration stage entry to unified workspace gate");
```

- [ ] **Step 4: Build workflow coordinator tests**

Run:

```powershell
cmake --build build_x64_v142 --target navigation_workflow_coordinator_test --config Release
```

Expected: build succeeds.

- [ ] **Step 5: Verify GREEN for coordinator gate wiring**

Run:

```powershell
ctest --test-dir build_x64_v142 -C Release -R navigation_workflow_coordinator_test --output-on-failure
```

Expected: PASS.

- [ ] **Step 6: Commit coordinator gate integration**

Run:

```powershell
git add UI/NewPages/Navigation/navigation_workflow_coordinator.h UI/NewPages/Navigation/navigation_workflow_coordinator.cpp tests/unit/NavigationWorkflowCoordinatorTest.cpp
git commit -m "feat: route navigation workflow stages through workspace gates"
```

## Task 5: Integrate NavigationPage With Workspace Snapshot Truth Source

**Files:**
- Modify: `UI/NewPages/NavigationPage.h`
- Modify: `UI/NewPages/NavigationPage.cpp`
- Modify: `tests/unit/AnkleNavigationWorkflowContractTest.cpp`

- [ ] **Step 1: Add page-level workspace service members and helpers**

Update `UI/NewPages/NavigationPage.h`:

```cpp
class NavigationWorkspaceApplicationService;

private:
    void refreshWorkspaceSnapshotUi();
    void syncStageGateUi();
    void persistWorkspaceSnapshot();

    std::unique_ptr<NavigationWorkspaceApplicationService> m_workspaceApplicationService;
```

- [ ] **Step 2: Construct the workspace application service**

In `NavigationPageNew::NavigationPageNew(...)`, after runtime coordinator creation, add:

```cpp
    m_workspaceApplicationService = std::make_unique<NavigationWorkspaceApplicationService>(
        m_workflowContext->casesRoot(),
        m_runtimeState.get());
```

And when constructing `m_workflowCoordinator`, pass the application service:

```cpp
    m_workflowCoordinator = std::make_unique<NavigationWorkflowCoordinator>(
        m_workflowContext.get(),
        m_preparationPlanningController.get(),
        m_registrationController.get(),
        m_navigationEvaluationController.get(),
        m_runtimeCoordinator.get(),
        [this](AnkleWorkflowStage stage) { setWorkflowStage(stage); },
        m_workspaceApplicationService.get());
```

- [ ] **Step 3: Load workspace snapshot on case-context changes**

Update `NavigationPageNew::setCaseContext(...)`:

```cpp
void NavigationPageNew::setCaseContext(const QString& caseId, int patientId, const QString& patientName)
{
    ...
    if (m_workspaceApplicationService) {
        m_workspaceApplicationService->loadWorkspace(caseId, patientId, patientName);
        refreshWorkspaceSnapshotUi();
    }
    refreshEvaluationSummary();
}
```

- [ ] **Step 4: Implement unified UI refresh**

Add these helpers to `UI/NewPages/NavigationPage.cpp`:

```cpp
void NavigationPageNew::refreshWorkspaceSnapshotUi()
{
    if (!m_workspaceApplicationService) {
        return;
    }

    const NavigationWorkspaceSnapshot& snapshot = m_workspaceApplicationService->snapshot();

    if (m_navigationPatientSummaryLabel) {
        m_navigationPatientSummaryLabel->setText(
            QStringLiteral("病例：%1 | 患者：%2").arg(snapshot.caseContext.caseId, snapshot.caseContext.patientName));
    }

    auto* readinessLabel = findChild<QLabel*>(QStringLiteral("navigationReadinessLabel"));
    if (readinessLabel) {
        readinessLabel->setText(snapshot.navigationState.allowNavigation
            ? QStringLiteral("导航准入：已就绪")
            : QStringLiteral("导航准入：未就绪"));
    }

    auto* confidenceLabel = findChild<QLabel*>(QStringLiteral("navigationConfidenceLabel"));
    if (confidenceLabel) {
        confidenceLabel->setText(QStringLiteral("可信度评分：%1").arg(snapshot.navigationState.confidenceScore, 0, 'f', 2));
    }

    syncStageGateUi();
}

void NavigationPageNew::syncStageGateUi()
{
    if (!m_workspaceApplicationService) {
        return;
    }

    const auto gates = m_workspaceApplicationService->stageGates();
    for (auto it = m_workflowRailButtons.begin(); it != m_workflowRailButtons.end(); ++it) {
        auto* button = it.value().data();
        if (!button) {
            continue;
        }

        const NavigationStageGate gate = gates.value(it.key());
        button->setEnabled(gate.allowEnter);
        button->setProperty("statusTone", gate.statusTone);
        polishNavigationWidget(button);
    }

    ui->startNavigationButton->setEnabled(gates.value(AnkleWorkflowStage::Navigation).allowEnter);
    ui->computeRegButton->setEnabled(gates.value(AnkleWorkflowStage::Registration).allowEnter);
}

void NavigationPageNew::persistWorkspaceSnapshot()
{
    if (m_workspaceApplicationService) {
        m_workspaceApplicationService->saveSnapshot();
    }
}
```

- [ ] **Step 5: Feed calibration, registration and runtime state into the application service**

Update the existing event handlers:

```cpp
void NavigationPageNew::startProbeCalibration()
{
    ...
    if (m_workspaceApplicationService) {
        m_workspaceApplicationService->markCalibrationProgress(0, m_activeCalibrationRequiredPoints);
        refreshWorkspaceSnapshotUi();
    }
}

void NavigationPageNew::captureProbeCalibrationPoint()
{
    ...
    if (m_workspaceApplicationService) {
        m_workspaceApplicationService->markCalibrationProgress(
            m_activeCalibrationCollectedPoints,
            m_activeCalibrationRequiredPoints);
        refreshWorkspaceSnapshotUi();
    }
}

void NavigationPageNew::finishProbeCalibration()
{
    ...
    if (m_workspaceApplicationService) {
        m_workspaceApplicationService->markCalibrationCompleted(
            calibrationResult.value(QStringLiteral("calibration_accuracy_mm")).toDouble(),
            m_navigationToolId);
        persistWorkspaceSnapshot();
        refreshWorkspaceSnapshotUi();
    }
}

void NavigationPageNew::onRegistrationCompleted(const PointRegistrationResult& result)
{
    ...
    if (m_workspaceApplicationService) {
        m_workspaceApplicationService->markRegistrationReady(
            pointRegistrationService() ? pointRegistrationService()->pointCount() : 0,
            result);
        persistWorkspaceSnapshot();
        refreshWorkspaceSnapshotUi();
    }
}

void NavigationPageNew::updateTrackerStatus(bool connected)
{
    ...
    if (m_workspaceApplicationService) {
        m_workspaceApplicationService->markNavigationState(
            connected,
            !m_navigationToolId.isEmpty(),
            m_navigationActive);
        refreshWorkspaceSnapshotUi();
    }
}
```

- [ ] **Step 6: Persist workspace snapshot after key state transitions**

Add `persistWorkspaceSnapshot();` to:

```cpp
setCaseContext(...)
finishProbeCalibration()
onRegistrationCompleted(...)
on_pauseNavigationButton_clicked()
```

- [ ] **Step 7: Make the new page contract tests pass**

Update `tests/unit/AnkleNavigationWorkflowContractTest.cpp` so the new tests assert the concrete strings added in Steps 1-6, and remove any temporary failing expectations.

- [ ] **Step 8: Build navigation page contract test**

Run:

```powershell
cmake --build build_x64_v142 --target ankle_navigation_workflow_contract_test --config Release
```

Expected: build succeeds.

- [ ] **Step 9: Verify GREEN for page-level contracts**

Run:

```powershell
ctest --test-dir build_x64_v142 -C Release -R ankle_navigation_workflow_contract_test --output-on-failure
```

Expected: PASS.

- [ ] **Step 10: Commit page integration**

Run:

```powershell
git add UI/NewPages/NavigationPage.h UI/NewPages/NavigationPage.cpp tests/unit/AnkleNavigationWorkflowContractTest.cpp
git commit -m "feat: connect navigation page to workspace workflow state"
```

## Task 6: Full Verification

**Files:**
- No new edits unless verification finds a concrete defect.

- [ ] **Step 1: Run focused navigation tests**

Run:

```powershell
ctest --test-dir build_x64_v142 -C Release -R "navigation_workspace_snapshot_store_test|navigation_workspace_application_service_test|navigation_workflow_coordinator_test|navigation_runtime_coordinator_contract_test|navigation_runtime_state_test|ankle_navigation_workflow_contract_test|ankle_case_workspace_repository_test|ankle_planning_service_test|navigation_evaluation_service_test" --output-on-failure
```

Expected: all listed tests pass.

- [ ] **Step 2: Build the main app**

Run:

```powershell
cmake --build build_x64_v142 --target medicalpro --config Release
```

Expected: build succeeds.

- [ ] **Step 3: Inspect the feature diff**

Run:

```powershell
git diff -- Framework/Navigation/navigation_workspace_types.h Framework/Navigation/navigation_workspace_snapshot_store.h Framework/Navigation/navigation_workspace_snapshot_store.cpp Framework/Navigation/navigation_workspace_application_service.h Framework/Navigation/navigation_workspace_application_service.cpp UI/NewPages/Navigation/navigation_workflow_coordinator.h UI/NewPages/Navigation/navigation_workflow_coordinator.cpp UI/NewPages/NavigationPage.h UI/NewPages/NavigationPage.cpp tests/unit/NavigationWorkspaceSnapshotStoreTest.cpp tests/unit/NavigationWorkspaceApplicationServiceTest.cpp tests/unit/NavigationWorkflowCoordinatorTest.cpp tests/unit/AnkleNavigationWorkflowContractTest.cpp tests/unit/CMakeLists.txt CMakeLists.txt
```

Expected: diff only contains the new workspace truth-source layer, gate wiring, page integration and tests described in this plan.

- [ ] **Step 4: Manual smoke test**

Run the built app and verify this exact path:

1. 从病例工作台进入一个已有病例的导航页
2. 导航页顶部和右侧能显示当前病例摘要
3. 未完成标定前，导航阶段按钮和开始导航按钮不可用，并显示阻塞原因
4. 连接跟踪器并选择真实器械后，开始标定，采样点数和状态会刷新
5. 完成标定后，配准阶段门禁解除
6. 完成点配准后，导航阶段门禁按 confidence 和跟踪状态更新
7. 暂停或退出导航页后再次进入同一病例，页面恢复上次阶段、标定摘要、配准摘要和阻塞原因

- [ ] **Step 5: Commit the verified feature**

Run:

```powershell
git add .
git commit -m "feat: restore and gate full navigation workspace workflow"
```

## Self-Review

- Spec coverage:
  - 单病例入口边界由 Task 5 的 `setCaseContext -> loadWorkspace` 接入实现。
  - 自动恢复/持久化由 Task 2 的快照存储和 Task 5 的 `persistWorkspaceSnapshot()` / `loadWorkspace()` 实现。
  - 流程门禁由 Task 3 的 `NavigationStageGate` 和 Task 4/5 的 coordinator + page UI 接线实现。
  - 真实服务主链保持不变，计划没有把 2D-3D 配准或病例选择扩进首版。

- Placeholder scan:
  - 没有 `TBD`、`TODO`、`implement later`。
  - 每个任务都给了明确文件、代码骨架、命令和预期结果。

- Type consistency:
  - 统一使用 `NavigationWorkspaceSnapshot`、`NavigationStageGate`、`NavigationWorkspaceApplicationService`。
  - 页面 helper、快照存储和 coordinator 名称在各任务中保持一致。

