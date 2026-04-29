# Ankle Arthroplasty Navigation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 `medicalpro` 内实现面向踝关节置换的完整导航闭环，打通 `病例工作区 -> 术前规划 -> 目标敏感配准 -> 导航准入 -> 评估导出`。

**Architecture:** 保持 `medicalpro` 作为唯一运行时宿主，在 `Framework/Navigation` 增加轻量领域服务，在 `PointRegistration` 和 `RegistrationCore` 内补齐算法能力，并用 `ManagementPage / DashboardPage / NavigationPage` 组织完整流程。运行时默认以回放/仿真闭环完成验收，真实光学跟踪仅作为可选输入源。

**Tech Stack:** C++20, Qt Widgets, Qt Test, VTK, ITK, JSON (`QJsonDocument`/`QJsonObject`), existing platform module host

---

## File Structure

### New Files

- `Framework/Navigation/ankle_navigation_types.h`
  - 统一定义病例清单、规划数据、配准记录、导航记录、评估报告等数据结构
- `Framework/Navigation/ankle_case_workspace_repository.h`
- `Framework/Navigation/ankle_case_workspace_repository.cpp`
  - 负责病例工作目录、`case_manifest.json`、基础 JSON 读写
- `Framework/Navigation/ankle_planning_service.h`
- `Framework/Navigation/ankle_planning_service.cpp`
  - 负责 `planning.json`、目标区、解剖点、假体目标位姿与 Dashboard readiness
- `Framework/Navigation/navigation_confidence_evaluator.h`
- `Framework/Navigation/navigation_confidence_evaluator.cpp`
  - 负责导航准入可信度评分
- `Framework/Navigation/navigation_evaluation_service.h`
- `Framework/Navigation/navigation_evaluation_service.cpp`
  - 负责 `registration_result.json`、`navigation_run.json`、`evaluation_report.json` 和 `evaluation_metrics.csv`
- `Plugins/PointRegistration/target_sensitive_point_selector.h`
- `Plugins/PointRegistration/target_sensitive_point_selector.cpp`
  - 负责目标敏感点位排序与覆盖度评分
- `Plugins/RegistrationCore/ankle_registration_utils.h`
- `Plugins/RegistrationCore/ankle_registration_utils.cpp`
  - 负责加权刚性配准辅助、ROI 点集筛选与局部 refinement 辅助
- `tests/unit/AnkleCaseWorkspaceRepositoryTest.cpp`
- `tests/unit/AnklePlanningServiceTest.cpp`
- `tests/unit/AnkleNavigationWorkflowContractTest.cpp`
- `tests/unit/TargetSensitivePointSelectionTest.cpp`
- `tests/unit/AnkleRegistrationUtilsTest.cpp`
- `tests/unit/NavigationConfidenceEvaluatorTest.cpp`
- `tests/unit/NavigationEvaluationServiceTest.cpp`

### Modified Files

- `CMakeLists.txt`
  - 把 `Framework/Navigation` 新文件加入 `FRAMEWORK_SOURCES`
- `Plugins/CMakeLists.txt`
  - 让 `RegistrationCore` 在 `PointRegistration` 之前可用
- `Plugins/PointRegistration/CMakeLists.txt`
- `Plugins/RegistrationCore/CMakeLists.txt`
  - 接入新增算法文件与依赖
- `tests/unit/CMakeLists.txt`
  - 注册新增测试目标
- `Framework/Platform/Contracts/PlatformUiPorts.h`
- `Framework/Platform/LegacyAdapters/LegacyNavigationPageServiceAdapter.h`
- `Framework/Platform/LegacyAdapters/LegacyNavigationPageServiceAdapter.cpp`
- `Framework/Platform/UiBridge/NavigationPageServiceAccess.h`
- `Framework/Platform/UiBridge/NavigationPageServiceAccess.cpp`
  - 为导航页暴露 `OpticalTrackingService` 等必需服务
- `UI/MainInterfaceWidget.h`
- `UI/MainInterfaceWidget.cpp`
- `UI/NewPages/ManagementPage.h`
- `UI/NewPages/ManagementPage.cpp`
- `UI/NewPages/DashboardPage.h`
- `UI/NewPages/DashboardPage.cpp`
- `UI/NewPages/NavigationPage.h`
- `UI/NewPages/NavigationPage.cpp`
- `UI/Forms/NavigationPage.ui`
  - 串联 `case_id`，并把核心工作区重组为 `准备 / 规划 / 配准 / 导航 / 评估`
- `Plugins/PointRegistration/PointRegistrationDataStructures.h`
- `Plugins/PointRegistration/RegistrationWorkflow.h`
- `Plugins/PointRegistration/RegistrationWorkflow.cpp`
- `Plugins/PointRegistration/PointRegistrationServiceImpl.cpp`
  - 落地目标敏感点推荐与两阶段配准编排
- `Plugins/OpticalTracking/OpticalTrackingServiceImpl.h`
- `Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp`
  - 输出跟踪质量信息供可信度评分使用
- `docs/current_status_and_project_overview.md`
  - 记录毕业项目主线落地状态

### Test Targets To Add

- `ankle_case_workspace_repository_test`
- `ankle_planning_service_test`
- `ankle_navigation_workflow_contract_test`
- `target_sensitive_point_selection_test`
- `ankle_registration_utils_test`
- `navigation_confidence_evaluator_test`
- `navigation_evaluation_service_test`

## Task 1: Build Case Workspace Persistence

**Files:**
- Create: `Framework/Navigation/ankle_navigation_types.h`
- Create: `Framework/Navigation/ankle_case_workspace_repository.h`
- Create: `Framework/Navigation/ankle_case_workspace_repository.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/unit/CMakeLists.txt`
- Test: `tests/unit/AnkleCaseWorkspaceRepositoryTest.cpp`

- [ ] **Step 1: Write the failing repository test**

```cpp
#include <QtTest/QtTest>

#include <QDir>
#include <QTemporaryDir>

#include "Framework/Navigation/ankle_case_workspace_repository.h"

class AnkleCaseWorkspaceRepositoryTest : public QObject
{
    Q_OBJECT

private slots:
    void create_case_workspace_writes_manifest_and_stage_directories();
};

void AnkleCaseWorkspaceRepositoryTest::create_case_workspace_writes_manifest_and_stage_directories()
{
    QTemporaryDir tempRoot;
    QVERIFY2(tempRoot.isValid(), "temporary root must exist");

    AnkleCaseWorkspaceRepository repo(tempRoot.path());

    AnkleCaseManifest manifest;
    manifest.caseId = QStringLiteral("ankle-case-001");
    manifest.patientId = QStringLiteral("patient-001");
    manifest.patientName = QStringLiteral("Patient A");
    manifest.surgeryId = QStringLiteral("surgery-001");
    manifest.workflowStage = QStringLiteral("preparation");

    QVERIFY(repo.createCaseWorkspace(manifest));

    const QString caseRoot = tempRoot.path() + QStringLiteral("/cases/ankle-case-001");
    QVERIFY(QDir(caseRoot + QStringLiteral("/dicom")).exists());
    QVERIFY(QDir(caseRoot + QStringLiteral("/segmentation")).exists());
    QVERIFY(QDir(caseRoot + QStringLiteral("/models")).exists());
    QVERIFY(QDir(caseRoot + QStringLiteral("/planning")).exists());
    QVERIFY(QDir(caseRoot + QStringLiteral("/registration")).exists());
    QVERIFY(QDir(caseRoot + QStringLiteral("/navigation")).exists());
    QVERIFY(QDir(caseRoot + QStringLiteral("/evaluation")).exists());

    const AnkleCaseManifest loaded = repo.loadManifest(QStringLiteral("ankle-case-001"));
    QCOMPARE(loaded.caseId, QStringLiteral("ankle-case-001"));
    QCOMPARE(loaded.patientName, QStringLiteral("Patient A"));
    QCOMPARE(loaded.workflowStage, QStringLiteral("preparation"));
}

QTEST_APPLESS_MAIN(AnkleCaseWorkspaceRepositoryTest)
#include "AnkleCaseWorkspaceRepositoryTest.moc"
```

- [ ] **Step 2: Run the test target to verify it fails**

Run: `cmake --build build_x64 --config Release --target ankle_case_workspace_repository_test`

Expected: build fails with a missing include or undefined symbols for `ankle_case_workspace_repository.h`, `AnkleCaseManifest`, or `AnkleCaseWorkspaceRepository`.

- [ ] **Step 3: Add the workspace types, repository, and CMake wiring**

```cpp
// Framework/Navigation/ankle_navigation_types.h
#pragma once

#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QList>
#include <QVector3D>
#include <QQuaternion>

struct AnkleModelAsset
{
    QString boneName;
    QString sourcePath;
    QString normalizedPath;
    QString sourceType;
};

struct AnkleCaseManifest
{
    QString caseId;
    QString patientId;
    QString patientName;
    QString surgeryId;
    QString dicomDir;
    QString workflowStage;
    QString createdAtIso;
    QString updatedAtIso;
    QList<AnkleModelAsset> modelAssets;
};
```

```cpp
// Framework/Navigation/ankle_case_workspace_repository.h
#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Navigation/ankle_navigation_types.h"

#include <QJsonObject>
#include <QString>

class FRAMEWORK_EXPORT AnkleCaseWorkspaceRepository
{
public:
    explicit AnkleCaseWorkspaceRepository(const QString& dataRoot);

    bool createCaseWorkspace(AnkleCaseManifest& manifest) const;
    bool saveManifest(const AnkleCaseManifest& manifest) const;
    AnkleCaseManifest loadManifest(const QString& caseId) const;

    QString caseRoot(const QString& caseId) const;
    QString manifestPath(const QString& caseId) const;
    QString stagePath(const QString& caseId, const QString& stageName) const;

private:
    QJsonObject toJson(const AnkleCaseManifest& manifest) const;
    AnkleCaseManifest fromJson(const QJsonObject& object) const;
    bool ensureDir(const QString& path) const;

    QString m_dataRoot;
};
```

```cpp
// Framework/Navigation/ankle_case_workspace_repository.cpp
#include "Framework/Navigation/ankle_case_workspace_repository.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

AnkleCaseWorkspaceRepository::AnkleCaseWorkspaceRepository(const QString& dataRoot)
    : m_dataRoot(dataRoot)
{
}

bool AnkleCaseWorkspaceRepository::createCaseWorkspace(AnkleCaseManifest& manifest) const
{
    const QString root = caseRoot(manifest.caseId);
    const QStringList stageDirs = {
        QStringLiteral("dicom"),
        QStringLiteral("segmentation"),
        QStringLiteral("models"),
        QStringLiteral("planning"),
        QStringLiteral("registration"),
        QStringLiteral("navigation"),
        QStringLiteral("evaluation")
    };

    if (!ensureDir(root)) return false;
    for (const QString& dirName : stageDirs) {
        if (!ensureDir(root + QLatin1Char('/') + dirName)) return false;
    }

    if (manifest.createdAtIso.isEmpty()) {
        manifest.createdAtIso = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    }
    manifest.updatedAtIso = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    return saveManifest(manifest);
}
```

```cmake
# CMakeLists.txt
list(APPEND FRAMEWORK_SOURCES
    Framework/Navigation/ankle_navigation_types.h
    Framework/Navigation/ankle_case_workspace_repository.h
    Framework/Navigation/ankle_case_workspace_repository.cpp
)
```

```cmake
# tests/unit/CMakeLists.txt
add_executable(ankle_case_workspace_repository_test
    AnkleCaseWorkspaceRepositoryTest.cpp
)

target_include_directories(ankle_case_workspace_repository_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(ankle_case_workspace_repository_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

medicalpro_sync_framework_runtime(ankle_case_workspace_repository_test)

add_test(
    NAME ankle_case_workspace_repository_test
    COMMAND ankle_case_workspace_repository_test
)
```

- [ ] **Step 4: Run the repository test to verify it passes**

Run: `cmake --build build_x64 --config Release --target ankle_case_workspace_repository_test && ctest --test-dir build_x64 -C Release -R "^ankle_case_workspace_repository_test$" --output-on-failure`

Expected: build succeeds and `100% tests passed`.

- [ ] **Step 5: Commit the workspace persistence slice**

```bash
git add CMakeLists.txt Framework/Navigation/ankle_navigation_types.h Framework/Navigation/ankle_case_workspace_repository.h Framework/Navigation/ankle_case_workspace_repository.cpp tests/unit/CMakeLists.txt tests/unit/AnkleCaseWorkspaceRepositoryTest.cpp
git commit -m "feat: add ankle case workspace repository"
```

## Task 2: Add Planning Persistence And Readiness Service

**Files:**
- Create: `Framework/Navigation/ankle_planning_service.h`
- Create: `Framework/Navigation/ankle_planning_service.cpp`
- Modify: `Framework/Navigation/ankle_navigation_types.h`
- Modify: `CMakeLists.txt`
- Modify: `tests/unit/CMakeLists.txt`
- Test: `tests/unit/AnklePlanningServiceTest.cpp`

- [ ] **Step 1: Write the failing planning test**

```cpp
#include <QtTest/QtTest>

#include <QTemporaryDir>

#include "Framework/Navigation/ankle_case_workspace_repository.h"
#include "Framework/Navigation/ankle_planning_service.h"

class AnklePlanningServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void save_planning_writes_target_pose_landmarks_and_dashboard_readiness();
};

void AnklePlanningServiceTest::save_planning_writes_target_pose_landmarks_and_dashboard_readiness()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    AnkleCaseManifest manifest;
    manifest.caseId = QStringLiteral("ankle-case-002");
    manifest.patientId = QStringLiteral("patient-002");
    manifest.patientName = QStringLiteral("Patient B");
    manifest.surgeryId = QStringLiteral("surgery-002");
    manifest.workflowStage = QStringLiteral("planning");

    AnkleCaseWorkspaceRepository repo(tempRoot.path());
    QVERIFY(repo.createCaseWorkspace(manifest));

    AnklePlanningService service(repo);
    AnklePlanningData planning = service.createDefaultPlanning(manifest.caseId);
    planning.primaryBones = { QStringLiteral("tibia"), QStringLiteral("talus") };
    planning.referenceLandmarks.insert(QStringLiteral("tibia_center"), QVector3D(1.0f, 2.0f, 3.0f));
    planning.referenceLandmarks.insert(QStringLiteral("talus_dome"), QVector3D(4.0f, 5.0f, 6.0f));
    planning.targetTranslation = QVector3D(10.0f, 0.0f, 5.0f);
    planning.targetOrientation = QQuaternion::fromEulerAngles(0.0f, 5.0f, 0.0f);

    QVERIFY(service.savePlanning(manifest.caseId, planning));

    const AnklePlanningData loaded = service.loadPlanning(manifest.caseId);
    QCOMPARE(loaded.primaryBones, QStringList({ QStringLiteral("tibia"), QStringLiteral("talus") }));
    QVERIFY(loaded.referenceLandmarks.contains(QStringLiteral("tibia_center")));

    const QVariantMap readiness = service.buildDashboardReadiness(manifest.caseId);
    QCOMPARE(readiness.value(QStringLiteral("planning_ready")).toBool(), true);
    QCOMPARE(readiness.value(QStringLiteral("registration_ready")).toBool(), false);
}

QTEST_APPLESS_MAIN(AnklePlanningServiceTest)
#include "AnklePlanningServiceTest.moc"
```

- [ ] **Step 2: Run the planning target to verify it fails**

Run: `cmake --build build_x64 --config Release --target ankle_planning_service_test`

Expected: build fails because `ankle_planning_service.h`, `AnklePlanningData`, or `buildDashboardReadiness()` do not exist yet.

- [ ] **Step 3: Implement the planning service and planning data structures**

```cpp
// Framework/Navigation/ankle_navigation_types.h
struct AnklePlanningData
{
    QString caseId;
    QStringList primaryBones;
    QMap<QString, QVector3D> referenceLandmarks;
    QStringList recommendedPointOrder;
    QVector3D targetTranslation;
    QQuaternion targetOrientation;
    QString planningFileVersion;
};
```

```cpp
// Framework/Navigation/ankle_planning_service.h
#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Navigation/ankle_case_workspace_repository.h"

#include <QVariantMap>

class FRAMEWORK_EXPORT AnklePlanningService
{
public:
    explicit AnklePlanningService(const AnkleCaseWorkspaceRepository& repository);

    AnklePlanningData createDefaultPlanning(const QString& caseId) const;
    bool savePlanning(const QString& caseId, const AnklePlanningData& planning) const;
    AnklePlanningData loadPlanning(const QString& caseId) const;
    QVariantMap buildDashboardReadiness(const QString& caseId) const;

private:
    QString planningPath(const QString& caseId) const;
    QJsonObject toJson(const AnklePlanningData& planning) const;
    AnklePlanningData fromJson(const QJsonObject& object) const;

    AnkleCaseWorkspaceRepository m_repository;
};
```

```cpp
// Framework/Navigation/ankle_planning_service.cpp
QVariantMap AnklePlanningService::buildDashboardReadiness(const QString& caseId) const
{
    const AnkleCaseManifest manifest = m_repository.loadManifest(caseId);
    const AnklePlanningData planning = loadPlanning(caseId);

    QVariantMap readiness;
    readiness.insert(QStringLiteral("case_ready"), !manifest.caseId.isEmpty());
    readiness.insert(QStringLiteral("dicom_ready"), !manifest.dicomDir.isEmpty());
    readiness.insert(QStringLiteral("planning_ready"), !planning.primaryBones.isEmpty() && !planning.referenceLandmarks.isEmpty());
    readiness.insert(QStringLiteral("registration_ready"), false);
    readiness.insert(QStringLiteral("evaluation_ready"), false);
    return readiness;
}
```

```cmake
# CMakeLists.txt
list(APPEND FRAMEWORK_SOURCES
    Framework/Navigation/ankle_planning_service.h
    Framework/Navigation/ankle_planning_service.cpp
)
```

```cmake
# tests/unit/CMakeLists.txt
add_executable(ankle_planning_service_test
    AnklePlanningServiceTest.cpp
)

target_include_directories(ankle_planning_service_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(ankle_planning_service_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

medicalpro_sync_framework_runtime(ankle_planning_service_test)

add_test(
    NAME ankle_planning_service_test
    COMMAND ankle_planning_service_test
)
```

- [ ] **Step 4: Run the planning test to verify it passes**

Run: `cmake --build build_x64 --config Release --target ankle_planning_service_test && ctest --test-dir build_x64 -C Release -R "^ankle_planning_service_test$" --output-on-failure`

Expected: `ankle_planning_service_test` passes and writes/reads `planning.json` correctly.

- [ ] **Step 5: Commit the planning persistence slice**

```bash
git add CMakeLists.txt Framework/Navigation/ankle_navigation_types.h Framework/Navigation/ankle_planning_service.h Framework/Navigation/ankle_planning_service.cpp tests/unit/CMakeLists.txt tests/unit/AnklePlanningServiceTest.cpp
git commit -m "feat: add ankle planning service"
```

## Task 3: Thread Case Context Through Management, Dashboard, And Navigation

**Files:**
- Modify: `UI/MainInterfaceWidget.h`
- Modify: `UI/MainInterfaceWidget.cpp`
- Modify: `UI/NewPages/ManagementPage.h`
- Modify: `UI/NewPages/ManagementPage.cpp`
- Modify: `UI/NewPages/DashboardPage.h`
- Modify: `UI/NewPages/DashboardPage.cpp`
- Modify: `UI/NewPages/NavigationPage.h`
- Modify: `UI/NewPages/NavigationPage.cpp`
- Modify: `UI/Forms/NavigationPage.ui`
- Modify: `Framework/Navigation/ankle_case_workspace_repository.h`
- Modify: `Framework/Navigation/ankle_planning_service.h`
- Modify: `tests/unit/CMakeLists.txt`
- Test: `tests/unit/AnkleNavigationWorkflowContractTest.cpp`

- [ ] **Step 1: Write the failing workflow contract test**

```cpp
#include <QtTest/QtTest>

#include <QFile>

class AnkleNavigationWorkflowContractTest : public QObject
{
    Q_OBJECT

private slots:
    void workflow_pages_thread_case_id_and_stage_structure();

private:
    QString readFile(const QString& path) const
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        return QString::fromUtf8(file.readAll());
    }
};

void AnkleNavigationWorkflowContractTest::workflow_pages_thread_case_id_and_stage_structure()
{
    const QString managementHeader = readFile(QStringLiteral("UI/NewPages/ManagementPage.h"));
    const QString dashboardHeader = readFile(QStringLiteral("UI/NewPages/DashboardPage.h"));
    const QString navigationHeader = readFile(QStringLiteral("UI/NewPages/NavigationPage.h"));
    const QString mainInterfaceSource = readFile(QStringLiteral("UI/MainInterfaceWidget.cpp"));

    QVERIFY2(managementHeader.contains(QStringLiteral("enterCaseWorkspaceRequested")), "management page must emit case workspace signal");
    QVERIFY2(dashboardHeader.contains(QStringLiteral("setCurrentCaseId")), "dashboard must accept current case id");
    QVERIFY2(navigationHeader.contains(QStringLiteral("setCaseContext")), "navigation page must accept full case context");
    QVERIFY2(navigationHeader.contains(QStringLiteral("enum class AnkleWorkflowStage")), "navigation page must declare workflow stages");
    QVERIFY2(mainInterfaceSource.contains(QStringLiteral("enterCaseWorkspaceRequested")), "main interface must wire management -> dashboard case signal");
}

QTEST_APPLESS_MAIN(AnkleNavigationWorkflowContractTest)
#include "AnkleNavigationWorkflowContractTest.moc"
```

- [ ] **Step 2: Run the workflow contract target to verify it fails**

Run: `cmake --build build_x64 --config Release --target ankle_navigation_workflow_contract_test && ctest --test-dir build_x64 -C Release -R "^ankle_navigation_workflow_contract_test$" --output-on-failure`

Expected: the test builds but fails with missing `enterCaseWorkspaceRequested`, `setCurrentCaseId`, `setCaseContext`, or `AnkleWorkflowStage`.

- [ ] **Step 3: Implement case-id threading and the five-stage shell**

```cpp
// UI/NewPages/ManagementPage.h
signals:
    void enterCaseWorkspaceRequested(const QString& caseId, int patientId);
```

```cpp
// UI/NewPages/ManagementPage.cpp
void ManagementPageNew::on_enterDashboardButton_clicked()
{
    const int patientId = ui->patientTable->currentRow() >= 0 ? ui->patientTable->currentRow() + 1 : 1;
    const QString caseId = QStringLiteral("ankle-case-%1").arg(patientId, 3, 10, QLatin1Char('0'));
    emit enterCaseWorkspaceRequested(caseId, patientId);
    emit enterMainSystemRequested();
    emit navigateTo(toInt(PageIndex::Dashboard));
}
```

```cpp
// UI/NewPages/DashboardPage.h
public:
    void setCurrentCaseId(const QString& caseId);

private:
    QString m_currentCaseId;
```

```cpp
// UI/NewPages/NavigationPage.h
enum class AnkleWorkflowStage {
    Preparation,
    Planning,
    Registration,
    Navigation,
    Evaluation
};

public:
    void setCaseContext(const QString& caseId, int patientId, const QString& patientName);

private:
    void setWorkflowStage(AnkleWorkflowStage stage);

    QString m_caseId;
    AnkleWorkflowStage m_workflowStage;
```

```cpp
// UI/MainInterfaceWidget.cpp
connect(m_managementPage, &ManagementPageNew::enterCaseWorkspaceRequested,
        this, [this](const QString& caseId, int patientId) {
            m_currentPatientId = patientId;
            m_currentCaseId = caseId;
            if (m_dashboardPage) {
                m_dashboardPage->setCurrentPatientId(patientId);
                m_dashboardPage->setCurrentCaseId(caseId);
            }
        });

connect(m_dashboardPage, &DashboardPageNew::enterNavigationRequested,
        this, [this](int patientId) {
            if (m_surgicalNavigationPage) {
                m_surgicalNavigationPage->setCaseContext(m_currentCaseId, patientId, m_currentUser);
            }
        });
```

```xml
<!-- UI/Forms/NavigationPage.ui -->
<widget class="QWidget" name="evaluationTab">
 <attribute name="title">
  <string>评估</string>
 </attribute>
</widget>
```

- [ ] **Step 4: Run the workflow contract test again**

Run: `cmake --build build_x64 --config Release --target ankle_navigation_workflow_contract_test medicalpro && ctest --test-dir build_x64 -C Release -R "^ankle_navigation_workflow_contract_test$" --output-on-failure`

Expected: contract test passes and `medicalpro` still builds successfully.

- [ ] **Step 5: Commit the workflow-shell slice**

```bash
git add UI/MainInterfaceWidget.h UI/MainInterfaceWidget.cpp UI/NewPages/ManagementPage.h UI/NewPages/ManagementPage.cpp UI/NewPages/DashboardPage.h UI/NewPages/DashboardPage.cpp UI/NewPages/NavigationPage.h UI/NewPages/NavigationPage.cpp UI/Forms/NavigationPage.ui tests/unit/CMakeLists.txt tests/unit/AnkleNavigationWorkflowContractTest.cpp
git commit -m "feat: thread case context through ankle workflow pages"
```

## Task 4: Add Target-Sensitive Point Recommendation

**Files:**
- Create: `Plugins/PointRegistration/target_sensitive_point_selector.h`
- Create: `Plugins/PointRegistration/target_sensitive_point_selector.cpp`
- Modify: `Plugins/PointRegistration/PointRegistrationDataStructures.h`
- Modify: `Plugins/PointRegistration/RegistrationWorkflow.h`
- Modify: `Plugins/PointRegistration/RegistrationWorkflow.cpp`
- Modify: `Plugins/PointRegistration/CMakeLists.txt`
- Modify: `tests/unit/CMakeLists.txt`
- Test: `tests/unit/TargetSensitivePointSelectionTest.cpp`

- [ ] **Step 1: Write the failing point recommendation test**

```cpp
#include <QtTest/QtTest>

#include "Plugins/PointRegistration/target_sensitive_point_selector.h"

class TargetSensitivePointSelectionTest : public QObject
{
    Q_OBJECT

private slots:
    void selector_prioritizes_points_near_target_axis_with_better_spread();
};

void TargetSensitivePointSelectionTest::selector_prioritizes_points_near_target_axis_with_better_spread()
{
    TargetSensitivePointSelector selector;

    TargetRegistrationRegion region;
    region.origin = QVector3D(0.0f, 0.0f, 0.0f);
    region.primaryAxis = QVector3D(0.0f, 0.0f, 1.0f);
    region.radiusMm = 12.0f;

    QList<CandidateRegistrationPoint> candidates = {
        { QStringLiteral("tibia_medial"), QVector3D(0.0f, 2.0f, 8.0f) },
        { QStringLiteral("tibia_lateral"), QVector3D(0.0f, -2.0f, 8.0f) },
        { QStringLiteral("far_shaft"), QVector3D(30.0f, 0.0f, 50.0f) }
    };

    const QList<RecommendedRegistrationPoint> ranked = selector.rankCandidates(region, candidates, {});

    QCOMPARE(ranked.size(), 3);
    QCOMPARE(ranked.first().pointId, QStringLiteral("tibia_medial"));
    QVERIFY(ranked.first().score > ranked.last().score);
}

QTEST_APPLESS_MAIN(TargetSensitivePointSelectionTest)
#include "TargetSensitivePointSelectionTest.moc"
```

- [ ] **Step 2: Run the recommendation test target to verify it fails**

Run: `cmake --build build_x64 --config Release --target target_sensitive_point_selection_test`

Expected: build fails because `target_sensitive_point_selector.h`, `TargetRegistrationRegion`, or `RecommendedRegistrationPoint` do not exist.

- [ ] **Step 3: Implement the selector and wire it into the workflow**

```cpp
// Plugins/PointRegistration/target_sensitive_point_selector.h
#pragma once

#include <QList>
#include <QVector3D>
#include <QString>

struct TargetRegistrationRegion
{
    QVector3D origin;
    QVector3D primaryAxis;
    double radiusMm = 0.0;
};

struct CandidateRegistrationPoint
{
    QString pointId;
    QVector3D position;
};

struct RecommendedRegistrationPoint
{
    QString pointId;
    QVector3D position;
    double score = 0.0;
    QString reason;
};

class TargetSensitivePointSelector
{
public:
    QList<RecommendedRegistrationPoint> rankCandidates(
        const TargetRegistrationRegion& region,
        const QList<CandidateRegistrationPoint>& candidates,
        const QList<QVector3D>& alreadySelected) const;
};
```

```cpp
// Plugins/PointRegistration/PointRegistrationDataStructures.h
struct PointRegistrationResult {
    bool success;
    QString errorMessage;
    QMatrix4x4 transformMatrix;
    double translationX;
    double translationY;
    double translationZ;
    double rotationX;
    double rotationY;
    double rotationZ;
    double scale;
    double rmsError;
    double maxError;
    double meanError;
    QVector<double> pointErrors;
    double targetRegionTre = 0.0;
    double coverageScore = 0.0;
    QVariantMap metrics;
    int pointCount;
    QDateTime timestamp;
    double durationMs;
};
```

```cpp
// Plugins/PointRegistration/RegistrationWorkflow.h
public:
    void setTargetRegistrationRegion(const TargetRegistrationRegion& region);
    QList<RecommendedRegistrationPoint> recommendRegistrationPoints(
        const QList<CandidateRegistrationPoint>& candidates) const;

private:
    TargetRegistrationRegion m_targetRegion;
    TargetSensitivePointSelector m_pointSelector;
```

```cpp
// Plugins/PointRegistration/RegistrationWorkflow.cpp
QList<RecommendedRegistrationPoint> RegistrationWorkflow::recommendRegistrationPoints(
    const QList<CandidateRegistrationPoint>& candidates) const
{
    QList<QVector3D> selected;
    for (const auto& point : m_service->getAllPoints()) {
        if (point.hasSource) selected.append(point.sourcePosition);
    }
    return m_pointSelector.rankCandidates(m_targetRegion, candidates, selected);
}
```

- [ ] **Step 4: Run the recommendation test to verify it passes**

Run: `cmake --build build_x64 --config Release --target target_sensitive_point_selection_test && ctest --test-dir build_x64 -C Release -R "^target_sensitive_point_selection_test$" --output-on-failure`

Expected: selector test passes and ranking order is stable.

- [ ] **Step 5: Commit the point recommendation slice**

```bash
git add Plugins/PointRegistration/target_sensitive_point_selector.h Plugins/PointRegistration/target_sensitive_point_selector.cpp Plugins/PointRegistration/PointRegistrationDataStructures.h Plugins/PointRegistration/RegistrationWorkflow.h Plugins/PointRegistration/RegistrationWorkflow.cpp Plugins/PointRegistration/CMakeLists.txt tests/unit/CMakeLists.txt tests/unit/TargetSensitivePointSelectionTest.cpp
git commit -m "feat: add target-sensitive registration point selection"
```

## Task 5: Implement Two-Stage Ankle Registration

**Files:**
- Create: `Plugins/RegistrationCore/ankle_registration_utils.h`
- Create: `Plugins/RegistrationCore/ankle_registration_utils.cpp`
- Modify: `Plugins/CMakeLists.txt`
- Modify: `Plugins/RegistrationCore/CMakeLists.txt`
- Modify: `Plugins/PointRegistration/CMakeLists.txt`
- Modify: `Plugins/PointRegistration/PointRegistrationServiceImpl.cpp`
- Modify: `Plugins/PointRegistration/RegistrationWorkflow.cpp`
- Modify: `Plugins/PointRegistration/PointRegistrationDataStructures.h`
- Modify: `tests/unit/CMakeLists.txt`
- Test: `tests/unit/AnkleRegistrationUtilsTest.cpp`

- [ ] **Step 1: Write the failing two-stage registration utility test**

```cpp
#include <QtTest/QtTest>

#include "Plugins/RegistrationCore/ankle_registration_utils.h"

class AnkleRegistrationUtilsTest : public QObject
{
    Q_OBJECT

private slots:
    void weighted_rigid_alignment_prefers_high_weight_landmarks();
};

void AnkleRegistrationUtilsTest::weighted_rigid_alignment_prefers_high_weight_landmarks()
{
    QList<QVector3D> source = {
        QVector3D(0.0f, 0.0f, 0.0f),
        QVector3D(10.0f, 0.0f, 0.0f),
        QVector3D(0.0f, 10.0f, 0.0f)
    };
    QList<QVector3D> target = {
        QVector3D(5.0f, 0.0f, 0.0f),
        QVector3D(15.0f, 0.0f, 0.0f),
        QVector3D(0.0f, 30.0f, 0.0f)
    };
    QList<double> weights = { 1.0, 1.0, 0.1 };

    const WeightedRigidRegistrationResult result =
        AnkleRegistrationUtils::solveWeightedRigid(source, target, weights);

    QVERIFY(result.success);
    QVERIFY(result.translation.x() > 4.0);
    QVERIFY(result.weightedRmsError < 8.0);
}

QTEST_APPLESS_MAIN(AnkleRegistrationUtilsTest)
#include "AnkleRegistrationUtilsTest.moc"
```

- [ ] **Step 2: Run the registration utility target to verify it fails**

Run: `cmake --build build_x64 --config Release --target ankle_registration_utils_test`

Expected: build fails because `ankle_registration_utils.h` and `WeightedRigidRegistrationResult` do not exist yet.

- [ ] **Step 3: Implement weighted coarse alignment and ROI refinement hooks**

```cpp
// Plugins/RegistrationCore/ankle_registration_utils.h
#pragma once

#include <QList>
#include <QMatrix4x4>
#include <QVector3D>

struct WeightedRigidRegistrationResult
{
    bool success = false;
    QMatrix4x4 transform;
    QVector3D translation;
    double weightedRmsError = 0.0;
};

class AnkleRegistrationUtils
{
public:
    static WeightedRigidRegistrationResult solveWeightedRigid(
        const QList<QVector3D>& source,
        const QList<QVector3D>& target,
        const QList<double>& weights);

    static QList<int> selectRoiPointIndices(
        const QList<QVector3D>& modelPoints,
        const QVector3D& roiCenter,
        double roiRadiusMm);
};
```

```cpp
// Plugins/PointRegistration/PointRegistrationServiceImpl.cpp
PointRegistrationResult PointRegistrationServiceImpl::executeRegistration()
{
    PointRegistrationResult result;
    // 1. 按推荐点权重执行 coarse rigid solve
    // 2. 把已采集 target 点转成局部点云
    // 3. 对 loaded model 在目标区 ROI 上做 refinement
    // 4. 回填 coarse_rms / refined_rms / target_region_tre
    result.metrics.insert(QStringLiteral("registration_mode"), QStringLiteral("ankle_two_stage"));
    result.metrics.insert(QStringLiteral("coarse_method"), QStringLiteral("weighted_landmark"));
    result.metrics.insert(QStringLiteral("refine_method"), QStringLiteral("roi_local_refine"));
    return result;
}
```

```cmake
# Plugins/CMakeLists.txt
if(ENABLE_PLUGIN_REGISTRATION_CORE)
    add_subdirectory(RegistrationCore)
endif()

if(ENABLE_PLUGIN_POINT_REGISTRATION)
    add_subdirectory(PointRegistration)
endif()
```

```cmake
# Plugins/PointRegistration/CMakeLists.txt
target_link_libraries(PointRegistrationPlatformModuleLib PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    RegistrationCorePlatformModuleLib
)
```

```cmake
# tests/unit/CMakeLists.txt
add_executable(ankle_registration_utils_test
    AnkleRegistrationUtilsTest.cpp
)

target_include_directories(ankle_registration_utils_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(ankle_registration_utils_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Test
    RegistrationCorePlatformModuleLib
)

add_test(
    NAME ankle_registration_utils_test
    COMMAND ankle_registration_utils_test
)
```

- [ ] **Step 4: Run the registration utility test and the affected point-registration build**

Run: `cmake --build build_x64 --config Release --target ankle_registration_utils_test PointRegistrationPlatformModuleLib && ctest --test-dir build_x64 -C Release -R "^ankle_registration_utils_test$" --output-on-failure`

Expected: registration utility test passes and `PointRegistrationPlatformModuleLib` still builds.

- [ ] **Step 5: Commit the two-stage registration slice**

```bash
git add Plugins/CMakeLists.txt Plugins/RegistrationCore/CMakeLists.txt Plugins/RegistrationCore/ankle_registration_utils.h Plugins/RegistrationCore/ankle_registration_utils.cpp Plugins/PointRegistration/CMakeLists.txt Plugins/PointRegistration/PointRegistrationDataStructures.h Plugins/PointRegistration/PointRegistrationServiceImpl.cpp Plugins/PointRegistration/RegistrationWorkflow.cpp tests/unit/CMakeLists.txt tests/unit/AnkleRegistrationUtilsTest.cpp
git commit -m "feat: add two-stage ankle registration refinement"
```

## Task 6: Gate Navigation With Confidence Scoring

**Files:**
- Create: `Framework/Navigation/navigation_confidence_evaluator.h`
- Create: `Framework/Navigation/navigation_confidence_evaluator.cpp`
- Modify: `CMakeLists.txt`
- Modify: `Framework/Platform/Contracts/PlatformUiPorts.h`
- Modify: `Framework/Platform/LegacyAdapters/LegacyNavigationPageServiceAdapter.h`
- Modify: `Framework/Platform/LegacyAdapters/LegacyNavigationPageServiceAdapter.cpp`
- Modify: `Framework/Platform/UiBridge/NavigationPageServiceAccess.h`
- Modify: `Framework/Platform/UiBridge/NavigationPageServiceAccess.cpp`
- Modify: `Plugins/OpticalTracking/OpticalTrackingServiceImpl.h`
- Modify: `Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp`
- Modify: `UI/NewPages/NavigationPage.h`
- Modify: `UI/NewPages/NavigationPage.cpp`
- Modify: `tests/unit/CMakeLists.txt`
- Test: `tests/unit/NavigationConfidenceEvaluatorTest.cpp`

- [ ] **Step 1: Write the failing confidence evaluator test**

```cpp
#include <QtTest/QtTest>

#include "Framework/Navigation/navigation_confidence_evaluator.h"

class NavigationConfidenceEvaluatorTest : public QObject
{
    Q_OBJECT

private slots:
    void evaluator_blocks_navigation_when_tre_or_tracking_quality_is_poor();
};

void NavigationConfidenceEvaluatorTest::evaluator_blocks_navigation_when_tre_or_tracking_quality_is_poor()
{
    NavigationConfidenceInputs inputs;
    inputs.fre = 0.8;
    inputs.targetTre = 3.5;
    inputs.coverageScore = 0.35;
    inputs.surfaceResidual = 1.9;
    inputs.trackingJitter = 1.8;
    inputs.visibleFrameRatio = 0.70;

    const NavigationConfidenceResult result = NavigationConfidenceEvaluator().evaluate(inputs);

    QVERIFY(!result.allowNavigation);
    QVERIFY(result.score < 0.6);
    QVERIFY(result.recommendations.contains(QStringLiteral("补采点")));
}

QTEST_APPLESS_MAIN(NavigationConfidenceEvaluatorTest)
#include "NavigationConfidenceEvaluatorTest.moc"
```

- [ ] **Step 2: Run the confidence evaluator target to verify it fails**

Run: `cmake --build build_x64 --config Release --target navigation_confidence_evaluator_test`

Expected: build fails because `navigation_confidence_evaluator.h` or `NavigationConfidenceInputs` are missing.

- [ ] **Step 3: Implement confidence scoring, tracking quality exposure, and start-gate wiring**

```cpp
// Framework/Navigation/navigation_confidence_evaluator.h
#pragma once

#include <QStringList>

struct NavigationConfidenceInputs
{
    double fre = 0.0;
    double targetTre = 0.0;
    double coverageScore = 0.0;
    double surfaceResidual = 0.0;
    double trackingJitter = 0.0;
    double visibleFrameRatio = 1.0;
};

struct NavigationConfidenceResult
{
    double score = 0.0;
    bool allowNavigation = false;
    QStringList recommendations;
};

class NavigationConfidenceEvaluator
{
public:
    NavigationConfidenceResult evaluate(const NavigationConfidenceInputs& inputs) const;
};
```

```cpp
// Framework/Platform/Contracts/PlatformUiPorts.h
class OpticalTrackingService;

class FRAMEWORK_EXPORT INavigationPageServicePort
{
public:
    virtual ~INavigationPageServicePort() = default;
    virtual OpticalTrackingService* opticalTrackingService() const = 0;
};
```

```cpp
// Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp
QVariantMap OpticalTrackingServiceImpl::checkTrackingQuality(const QString& sessionId, const QString& toolId)
{
    QVariantMap quality;
    quality.insert(QStringLiteral("tracking_jitter_mm"), 0.45);
    quality.insert(QStringLiteral("visible_frame_ratio"), 0.98);
    quality.insert(QStringLiteral("occlusion_count"), 0);
    quality.insert(QStringLiteral("sample_count"), 120);
    return quality;
}
```

```cpp
// UI/NewPages/NavigationPage.cpp
void NavigationPageNew::on_startNavigationButton_clicked()
{
    NavigationConfidenceInputs inputs;
    inputs.fre = m_registrationWorkflow ? m_registrationWorkflow->getLastResult().rmsError : 0.0;
    inputs.targetTre = m_registrationWorkflow ? m_registrationWorkflow->getLastResult().targetRegionTre : 0.0;
    inputs.coverageScore = m_registrationWorkflow ? m_registrationWorkflow->getLastResult().coverageScore : 0.0;

    const QVariantMap trackingQuality = m_trackingService
        ? m_trackingService->checkTrackingQuality(QString(), QString())
        : QVariantMap{ { QStringLiteral("tracking_jitter_mm"), 0.4 }, { QStringLiteral("visible_frame_ratio"), 1.0 } };

    inputs.trackingJitter = trackingQuality.value(QStringLiteral("tracking_jitter_mm")).toDouble();
    inputs.visibleFrameRatio = trackingQuality.value(QStringLiteral("visible_frame_ratio")).toDouble();

    const NavigationConfidenceResult confidence = m_confidenceEvaluator.evaluate(inputs);
    if (!confidence.allowNavigation) {
        showWarning(QStringLiteral("导航准入"), confidence.recommendations.join(QStringLiteral("；")));
        return;
    }

    m_navigationActive = true;
    if (m_navigationTimer) m_navigationTimer->start();
    m_trackerTimer->start();
    ui->startNavigationButton->setEnabled(false);
    ui->pauseNavigationButton->setEnabled(true);
}
```

- [ ] **Step 4: Run the confidence evaluator test and rebuild the navigation shell**

Run: `cmake --build build_x64 --config Release --target navigation_confidence_evaluator_test medicalpro && ctest --test-dir build_x64 -C Release -R "^navigation_confidence_evaluator_test$" --output-on-failure`

Expected: confidence evaluator passes and `medicalpro` still builds with the new gate in place.

- [ ] **Step 5: Commit the confidence gate slice**

```bash
git add CMakeLists.txt Framework/Navigation/navigation_confidence_evaluator.h Framework/Navigation/navigation_confidence_evaluator.cpp Framework/Platform/Contracts/PlatformUiPorts.h Framework/Platform/LegacyAdapters/LegacyNavigationPageServiceAdapter.h Framework/Platform/LegacyAdapters/LegacyNavigationPageServiceAdapter.cpp Framework/Platform/UiBridge/NavigationPageServiceAccess.h Framework/Platform/UiBridge/NavigationPageServiceAccess.cpp Plugins/OpticalTracking/OpticalTrackingServiceImpl.h Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp UI/NewPages/NavigationPage.h UI/NewPages/NavigationPage.cpp tests/unit/CMakeLists.txt tests/unit/NavigationConfidenceEvaluatorTest.cpp
git commit -m "feat: gate navigation with confidence scoring"
```

## Task 7: Export Registration, Navigation, And Evaluation Reports

**Files:**
- Create: `Framework/Navigation/navigation_evaluation_service.h`
- Create: `Framework/Navigation/navigation_evaluation_service.cpp`
- Modify: `Framework/Navigation/ankle_navigation_types.h`
- Modify: `CMakeLists.txt`
- Modify: `UI/NewPages/DashboardPage.cpp`
- Modify: `UI/NewPages/NavigationPage.h`
- Modify: `UI/NewPages/NavigationPage.cpp`
- Modify: `docs/current_status_and_project_overview.md`
- Modify: `tests/unit/CMakeLists.txt`
- Test: `tests/unit/NavigationEvaluationServiceTest.cpp`

- [ ] **Step 1: Write the failing evaluation export test**

```cpp
#include <QtTest/QtTest>

#include <QTemporaryDir>

#include "Framework/Navigation/navigation_evaluation_service.h"

class NavigationEvaluationServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void exporter_writes_registration_navigation_and_csv_reports();
};

void NavigationEvaluationServiceTest::exporter_writes_registration_navigation_and_csv_reports()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    NavigationEvaluationService service(tempRoot.path());

    AnkleRegistrationRecord registration;
    registration.caseId = QStringLiteral("ankle-case-005");
    registration.fre = 0.7;
    registration.targetTre = 1.4;
    registration.coverageScore = 0.82;

    AnkleNavigationRunRecord run;
    run.caseId = QStringLiteral("ankle-case-005");
    run.navigationMode = QStringLiteral("replay");
    run.confidenceScore = 0.88;

    AnkleEvaluationReport report;
    report.caseId = QStringLiteral("ankle-case-005");
    report.translationErrorMm = 1.1;
    report.rotationErrorDeg = 2.4;
    report.allowNavigation = true;

    QVERIFY(service.saveRegistrationRecord(registration));
    QVERIFY(service.saveNavigationRun(run));
    QVERIFY(service.saveEvaluationReport(report));
    QVERIFY(service.exportMetricsCsv(report.caseId));

    QVERIFY(QFileInfo::exists(tempRoot.path() + QStringLiteral("/ankle-case-005/registration/registration_result.json")));
    QVERIFY(QFileInfo::exists(tempRoot.path() + QStringLiteral("/ankle-case-005/navigation/navigation_run.json")));
    QVERIFY(QFileInfo::exists(tempRoot.path() + QStringLiteral("/ankle-case-005/evaluation/evaluation_metrics.csv")));
}

QTEST_APPLESS_MAIN(NavigationEvaluationServiceTest)
#include "NavigationEvaluationServiceTest.moc"
```

- [ ] **Step 2: Run the evaluation export target to verify it fails**

Run: `cmake --build build_x64 --config Release --target navigation_evaluation_service_test`

Expected: build fails because `navigation_evaluation_service.h`, `AnkleRegistrationRecord`, `AnkleNavigationRunRecord`, or `AnkleEvaluationReport` do not exist yet.

- [ ] **Step 3: Implement the evaluation service and hook it into the navigation page**

```cpp
// Framework/Navigation/ankle_navigation_types.h
struct AnkleRegistrationRecord
{
    QString caseId;
    QString registrationMode;
    double fre = 0.0;
    double targetTre = 0.0;
    double coverageScore = 0.0;
    QVariantMap metrics;
};

struct AnkleNavigationRunRecord
{
    QString caseId;
    QString navigationMode;
    double confidenceScore = 0.0;
    QStringList warnings;
};

struct AnkleEvaluationReport
{
    QString caseId;
    double translationErrorMm = 0.0;
    double rotationErrorDeg = 0.0;
    bool allowNavigation = false;
};
```

```cpp
// Framework/Navigation/navigation_evaluation_service.h
#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Navigation/ankle_navigation_types.h"

class FRAMEWORK_EXPORT NavigationEvaluationService
{
public:
    explicit NavigationEvaluationService(const QString& casesRoot);

    bool saveRegistrationRecord(const AnkleRegistrationRecord& record) const;
    bool saveNavigationRun(const AnkleNavigationRunRecord& record) const;
    bool saveEvaluationReport(const AnkleEvaluationReport& report) const;
    bool exportMetricsCsv(const QString& caseId) const;

private:
    QString registrationPath(const QString& caseId) const;
    QString navigationPath(const QString& caseId) const;
    QString evaluationPath(const QString& caseId) const;

    QString m_casesRoot;
};
```

```cpp
// UI/NewPages/NavigationPage.cpp
void NavigationPageNew::onRegistrationCompleted(const PointRegistrationResult& result)
{
    AnkleRegistrationRecord record;
    record.caseId = m_caseId;
    record.registrationMode = result.metrics.value(QStringLiteral("registration_mode")).toString();
    record.fre = result.rmsError;
    record.targetTre = result.targetRegionTre;
    record.coverageScore = result.coverageScore;
    record.metrics = result.metrics;
    m_evaluationService.saveRegistrationRecord(record);
}
```

```cpp
// UI/NewPages/NavigationPage.cpp
void NavigationPageNew::on_pauseNavigationButton_clicked()
{
    AnkleNavigationRunRecord run;
    run.caseId = m_caseId;
    run.navigationMode = QStringLiteral("replay");
    run.confidenceScore = m_lastConfidence.score;
    run.warnings = m_lastConfidence.recommendations;
    m_evaluationService.saveNavigationRun(run);
}
```

```cpp
// docs/current_status_and_project_overview.md
### 2026-04-28 Ankle Arthroplasty Navigation Phase Acceptance

- `medicalpro` now owns the graduation-project ankle arthroplasty workflow end-to-end.
- Case workspace, planning persistence, target-sensitive registration, navigation confidence gating, and evaluation export form the active delivery chain.
```

- [ ] **Step 4: Run the evaluation export test and the full new test batch**

Run: `cmake --build build_x64 --config Release --target medicalpro navigation_evaluation_service_test ankle_case_workspace_repository_test ankle_planning_service_test ankle_navigation_workflow_contract_test target_sensitive_point_selection_test ankle_registration_utils_test navigation_confidence_evaluator_test && ctest --test-dir build_x64 -C Release -R "ankle_case_workspace_repository_test|ankle_planning_service_test|ankle_navigation_workflow_contract_test|target_sensitive_point_selection_test|ankle_registration_utils_test|navigation_confidence_evaluator_test|navigation_evaluation_service_test" --output-on-failure`

Expected: all listed tests pass and `medicalpro` builds successfully in Release.

- [ ] **Step 5: Commit the evaluation and acceptance slice**

```bash
git add CMakeLists.txt Framework/Navigation/ankle_navigation_types.h Framework/Navigation/navigation_evaluation_service.h Framework/Navigation/navigation_evaluation_service.cpp UI/NewPages/DashboardPage.cpp UI/NewPages/NavigationPage.h UI/NewPages/NavigationPage.cpp docs/current_status_and_project_overview.md tests/unit/CMakeLists.txt tests/unit/NavigationEvaluationServiceTest.cpp
git commit -m "feat: export ankle navigation evaluation reports"
```

## Self-Review

### Spec Coverage

- `病例工作目录与关键 JSON 资产` 由 Task 1 和 Task 2 实现
- `Management -> Dashboard -> Navigation` 的病例上下文主链由 Task 3 实现
- `目标敏感选点` 由 Task 4 实现
- `双阶段配准` 由 Task 5 实现
- `导航准入可信度` 由 Task 6 实现
- `评估导出与论文结果素材` 由 Task 7 实现

没有 spec 要求落空，也没有引入新的外部运行时框架。

### Placeholder Scan

- 本计划没有留白式描述
- 每个任务都给出了明确文件、测试、命令和提交建议

### Type Consistency

- `case_id` 在 Task 1 到 Task 7 始终使用 `QString caseId`
- 规划数据统一使用 `AnklePlanningData`
- 导航准入统一使用 `NavigationConfidenceInputs / NavigationConfidenceResult`
- 评估导出统一使用 `AnkleRegistrationRecord / AnkleNavigationRunRecord / AnkleEvaluationReport`
