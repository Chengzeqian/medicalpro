# Ankle Navigation Error-Aware Digital Twin Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在现有踝关节导航系统的实时位姿链、单窗口 VTK 数字孪生和评估落盘能力之上，增量实现误差感知数字孪生状态层、目标区局部关系分析、风险报告、HUD/VTK 风险表达和病例级 twin 指标导出。

**Architecture:** 保持现有 `NavigationRuntimeState + NavigationRuntimeCoordinator + NavigationVtkBridge + Navigation3DViewWidget + NavigationEvaluationService` 主链不变，只在其中插入一个轻量但明确的数字孪生状态构建层。共享类型继续落在 `Framework/Navigation/ankle_navigation_types.h`，数字孪生判定逻辑抽成独立的 `navigation_digital_twin_state_builder.*`，运行时只负责喂数据和持有状态，UI 与 VTK 只消费 twin state，不重复实现决策逻辑。

**Tech Stack:** C++20, Qt Core/Gui/Widgets, QMatrix4x4, QVector3D, QVariantMap, existing VTK widget stack, existing navigation workspace snapshot chain, QtTest, CMake, ctest.

---

## File Structure

- Modify: `Framework/Navigation/ankle_navigation_types.h`
  Responsibility: 扩展数字孪生共享类型，包括 `DigitalTwinTargetRegionDefinition`、`TargetRegionNavigationStatus`、`DigitalTwinRiskReport`、`DigitalTwinState`。

- Modify: `UI/NewPages/Navigation/navigation_workspace_types.h`
  Responsibility: 让 workspace planning state 持有目标区中心、半径和规划轴等真实几何上下文，供 runtime twin 使用。

- Modify: `UI/NewPages/Navigation/navigation_workspace_application_service.cpp`
  Responsibility: 从 `AnklePlanningData` 回填 planning snapshot 中的目标区几何数据。

- Create: `Framework/Navigation/navigation_digital_twin_state_builder.h`
- Create: `Framework/Navigation/navigation_digital_twin_state_builder.cpp`
  Responsibility: 统一计算目标区局部关系、风险来源、twin confidence 和在线建议。

- Modify: `CMakeLists.txt`
  Responsibility: 注册新的 framework 源文件。

- Create: `tests/unit/NavigationDigitalTwinStateBuilderTest.cpp`
- Modify: `tests/unit/CMakeLists.txt`
  Responsibility: 新增数字孪生状态 builder 单测目标并接入现有 Framework 测试集合。

- Modify: `UI/NewPages/Navigation/navigation_runtime_state.h`
- Modify: `UI/NewPages/Navigation/navigation_runtime_state.cpp`
  Responsibility: 在 runtime state 中保存 target region context、target region navigation status、risk report、digital twin state。

- Modify: `UI/NewPages/Navigation/navigation_runtime_coordinator.h`
- Modify: `UI/NewPages/Navigation/navigation_runtime_coordinator.cpp`
  Responsibility: 接收 target region context，统一刷新数字孪生状态，并将 twin 指标写入评估报告。

- Modify: `tests/unit/NavigationRuntimeCoordinatorContractTest.cpp`
  Responsibility: 锁定运行时状态刷新、风险建议、指标落盘行为。

- Modify: `UI/Widgets/Navigation3DViewWidget.h`
- Modify: `UI/Widgets/Navigation3DViewWidget.cpp`
  Responsibility: 增加 target region actor 与风险 tone 表达。

- Modify: `UI/NewPages/Navigation/navigation_vtk_bridge.h`
- Modify: `UI/NewPages/Navigation/navigation_vtk_bridge.cpp`
  Responsibility: 给 VTK bridge 增加目标区定义与风险覆盖层的桥接接口。

- Modify: `tests/unit/NavigationVtkBridgeTest.cpp`
  Responsibility: 锁定 target region overlay 与风险 tone 的 VTK 桥接行为。

- Modify: `UI/NewPages/NavigationPage.h`
- Modify: `UI/NewPages/NavigationPage.cpp`
- Modify: `UI/NewPages/Navigation/navigation_workspace_ui_binder.cpp`
- Modify: `UI/styles/three_pages_theme.qss`
  Responsibility: 扩展数字孪生 HUD，显示 twin confidence、dominant risk、target distance、re-register 建议，并将状态同步到导航工作区摘要。

- Modify: `tests/unit/AnkleNavigationWorkflowContractTest.cpp`
- Modify: `tests/unit/NavigationEvaluationSummaryFormatterTest.cpp`
- Modify: `UI/NewPages/Navigation/navigation_evaluation_summary_formatter.cpp`
  Responsibility: 锁定数字孪生 HUD 与评估摘要的文字契约。

- Modify: `Framework/Navigation/navigation_evaluation_service.cpp`
- Modify: `tests/unit/NavigationEvaluationServiceTest.cpp`
  Responsibility: 将 twin 指标写入 json/csv/summary 快照，服务于病例回放和实验统计。

- Modify: `docs/current_status_and_project_overview.md`
- Create: `docs/superpowers/specs/2026-05-25-ankle-navigation-error-aware-digital-twin-experiment-guide.md`
  Responsibility: 更新项目总览并沉淀数字孪生实验手册。

## Task 1: Extend Shared Types And Planning Geometry Context

**Files:**
- Modify: `Framework/Navigation/ankle_navigation_types.h`
- Modify: `UI/NewPages/Navigation/navigation_workspace_types.h`
- Modify: `UI/NewPages/Navigation/navigation_workspace_application_service.cpp`
- Modify: `tests/unit/NavigationWorkspaceApplicationServiceTest.cpp`

- [ ] **Step 1: 先写失败测试，锁定 planning snapshot 必须保留目标区几何上下文**

在 `tests/unit/NavigationWorkspaceApplicationServiceTest.cpp` 追加：

```cpp
void NavigationWorkspaceApplicationServiceTest::snapshot_preserves_target_region_geometry_for_digital_twin()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    AnkleCaseWorkspaceRepository repository(tempRoot.path());
    NavigationEvaluationService evaluationService(tempRoot.path() + QStringLiteral("/cases"));
    NavigationWorkspaceApplicationService service(repository, evaluationService);

    AnkleCaseManifest manifest;
    manifest.caseId = QStringLiteral("ankle-case-twin-001");
    manifest.patientId = QStringLiteral("patient-001");
    manifest.patientName = QStringLiteral("Patient A");
    manifest.surgeryId = QStringLiteral("surgery-001");
    QVERIFY(repository.createCaseWorkspace(manifest));

    AnklePlanningService planningService(repository);
    AnklePlanningData planning = planningService.createDefaultPlanning(manifest.caseId);
    planning.targetRegionCenter = QVector3D(12.0f, -4.0f, 8.0f);
    planning.targetRegionRadiusMm = 18.0;
    planning.targetOrientation = QQuaternion::fromDirection(
        QVector3D(0.0f, 0.0f, 1.0f),
        QVector3D(0.0f, 1.0f, 0.0f));
    QVERIFY(planningService.savePlanning(manifest.caseId, planning));

    service.openCaseWorkspace(manifest.caseId);
    const NavigationWorkspaceSnapshot snapshot = service.currentSnapshot();

    QVERIFY(snapshot.planningState.targetRegionReady);
    QCOMPARE(snapshot.planningState.targetRegionCenter, QVector3D(12.0f, -4.0f, 8.0f));
    QCOMPARE(snapshot.planningState.targetRegionRadiusMm, 18.0);
    QCOMPARE(snapshot.planningState.targetRegionAxis, QVector3D(0.0f, 0.0f, 1.0f));
}
```

- [ ] **Step 2: 运行应用服务测试，确认当前 planning state 还没有这些字段**

Run:

```bash
cmake --build build_x64_v142 --config Release --target navigation_workspace_application_service_test
ctest --test-dir build_x64_v142 -C Release -R "^navigation_workspace_application_service_test$" --output-on-failure
```

Expected: FAIL，报错集中在 `targetRegionCenter`、`targetRegionRadiusMm`、`targetRegionAxis` 不存在。

- [ ] **Step 3: 扩展共享数字孪生类型**

在 `Framework/Navigation/ankle_navigation_types.h` 增加：

```cpp
struct DigitalTwinTargetRegionDefinition
{
    bool available = false;
    QVector3D centerPatient;
    QVector3D plannedAxisPatient = QVector3D(0.0f, 0.0f, 1.0f);
    double radiusMm = 0.0;
};

struct TargetRegionNavigationStatus
{
    bool targetRegionAvailable = false;
    double distanceToTargetMm = 0.0;
    double angleErrorDeg = 0.0;
    double targetHitProbability = 0.0;
    double localConfidenceScore = 0.0;
};

struct DigitalTwinRiskReport
{
    QString dominantRiskSource;
    QStringList riskReasons;
    QVariantMap rawMetrics;
};

struct DigitalTwinState
{
    bool valid = false;
    QString statusCode;
    QString statusText;
    double twinConfidenceScore = 0.0;
    double localRiskScore = 0.0;
    bool allowNavigation = false;
    bool reRegisterRecommended = false;
    bool trackingDegradationDetected = false;
    QVariantMap evidence;
};
```

- [ ] **Step 4: 让 workspace planning state 保留真实目标区几何**

在 `UI/NewPages/Navigation/navigation_workspace_types.h` 的 `NavigationWorkspacePlanningState` 中加入：

```cpp
    QVector3D targetRegionCenter;
    double targetRegionRadiusMm = 0.0;
    QVector3D targetRegionAxis = QVector3D(0.0f, 0.0f, 1.0f);
```

在 `UI/NewPages/Navigation/navigation_workspace_application_service.cpp` 中补齐回填：

```cpp
    state.targetRegionCenter = planning.targetRegionCenter;
    state.targetRegionRadiusMm = planning.targetRegionRadiusMm;
    state.targetRegionAxis = planning.targetOrientation.rotatedVector(QVector3D(0.0f, 0.0f, 1.0f));
```

- [ ] **Step 5: 重新运行 planning snapshot 测试**

Run:

```bash
cmake --build build_x64_v142 --config Release --target navigation_workspace_application_service_test
ctest --test-dir build_x64_v142 -C Release -R "^navigation_workspace_application_service_test$" --output-on-failure
```

Expected: PASS。

- [ ] **Step 6: 提交共享类型与 planning geometry**

```bash
git add Framework/Navigation/ankle_navigation_types.h UI/NewPages/Navigation/navigation_workspace_types.h UI/NewPages/Navigation/navigation_workspace_application_service.cpp tests/unit/NavigationWorkspaceApplicationServiceTest.cpp
git commit -m "feat: extend planning snapshot with digital twin target region context"
```

## Task 2: Add The Digital Twin State Builder Module

**Files:**
- Create: `Framework/Navigation/navigation_digital_twin_state_builder.h`
- Create: `Framework/Navigation/navigation_digital_twin_state_builder.cpp`
- Modify: `CMakeLists.txt`
- Create: `tests/unit/NavigationDigitalTwinStateBuilderTest.cpp`
- Modify: `tests/unit/CMakeLists.txt`

- [ ] **Step 1: 先写失败测试，锁定目标区状态、风险报告和 twin state 的核心行为**

新建 `tests/unit/NavigationDigitalTwinStateBuilderTest.cpp`：

```cpp
#include <QtTest/QtTest>

#include "Framework/Navigation/navigation_digital_twin_state_builder.h"

class NavigationDigitalTwinStateBuilderTest : public QObject
{
    Q_OBJECT

private slots:
    void builder_reports_target_region_distance_angle_and_hit_probability();
    void builder_marks_registration_as_dominant_risk_when_target_tre_is_high();
    void builder_recommends_reregister_when_twin_confidence_drops_below_threshold();
};

void NavigationDigitalTwinStateBuilderTest::builder_reports_target_region_distance_angle_and_hit_probability()
{
    DigitalTwinTargetRegionDefinition targetRegion;
    targetRegion.available = true;
    targetRegion.centerPatient = QVector3D(10.0f, 0.0f, 0.0f);
    targetRegion.plannedAxisPatient = QVector3D(0.0f, 0.0f, 1.0f);
    targetRegion.radiusMm = 5.0;

    NavigationTransformResult transformResult;
    transformResult.valid = true;
    transformResult.vtkToolTransform.translate(12.0f, 0.0f, 0.0f);

    const TargetRegionNavigationStatus status =
        buildTargetRegionNavigationStatus(targetRegion, transformResult);

    QVERIFY(status.targetRegionAvailable);
    QCOMPARE(status.distanceToTargetMm, 2.0);
    QVERIFY(status.targetHitProbability > 0.5);
}

void NavigationDigitalTwinStateBuilderTest::builder_marks_registration_as_dominant_risk_when_target_tre_is_high()
{
    PointRegistrationResult registrationResult;
    registrationResult.success = true;
    registrationResult.targetRegionTre = 2.8;
    registrationResult.coverageScore = 0.74;

    QVariantMap trackingQuality;
    trackingQuality.insert(QStringLiteral("tracking_jitter_mm"), 0.28);
    trackingQuality.insert(QStringLiteral("visible_frame_ratio"), 0.99);
    trackingQuality.insert(QStringLiteral("calibrated"), true);
    trackingQuality.insert(QStringLiteral("calibration_accuracy_mm"), 0.35);

    NavigationConfidenceResult confidenceResult;
    confidenceResult.allowNavigation = false;
    confidenceResult.score = 0.42;

    TargetRegionNavigationStatus targetStatus;
    targetStatus.targetRegionAvailable = true;
    targetStatus.distanceToTargetMm = 1.6;
    targetStatus.localConfidenceScore = 0.48;

    const DigitalTwinRiskReport riskReport = buildDigitalTwinRiskReport(
        registrationResult,
        trackingQuality,
        confidenceResult,
        targetStatus);

    QCOMPARE(riskReport.dominantRiskSource, QStringLiteral("registration"));
    QVERIFY(riskReport.riskReasons.contains(QStringLiteral("target_tre_high")));
}

void NavigationDigitalTwinStateBuilderTest::builder_recommends_reregister_when_twin_confidence_drops_below_threshold()
{
    PointRegistrationResult registrationResult;
    registrationResult.success = true;
    registrationResult.targetRegionTre = 2.6;
    registrationResult.coverageScore = 0.62;

    QVariantMap trackingQuality;
    trackingQuality.insert(QStringLiteral("tracking_jitter_mm"), 0.86);
    trackingQuality.insert(QStringLiteral("visible_frame_ratio"), 0.78);
    trackingQuality.insert(QStringLiteral("calibrated"), true);
    trackingQuality.insert(QStringLiteral("calibration_accuracy_mm"), 0.71);

    NavigationConfidenceResult confidenceResult;
    confidenceResult.allowNavigation = false;
    confidenceResult.score = 0.33;

    TargetRegionNavigationStatus targetStatus;
    targetStatus.targetRegionAvailable = true;
    targetStatus.distanceToTargetMm = 7.2;
    targetStatus.localConfidenceScore = 0.31;

    DigitalTwinRiskReport riskReport;
    riskReport.dominantRiskSource = QStringLiteral("registration");
    riskReport.riskReasons = { QStringLiteral("target_tre_high"), QStringLiteral("coverage_low") };

    const DigitalTwinState twinState = buildDigitalTwinState(
        registrationResult,
        trackingQuality,
        confidenceResult,
        targetStatus,
        riskReport);

    QVERIFY(twinState.valid);
    QVERIFY(twinState.reRegisterRecommended);
    QVERIFY(twinState.trackingDegradationDetected);
    QVERIFY(twinState.twinConfidenceScore < 0.5);
}

QTEST_APPLESS_MAIN(NavigationDigitalTwinStateBuilderTest)
#include "NavigationDigitalTwinStateBuilderTest.moc"
```

- [ ] **Step 2: 运行新测试，确认 builder 模块还不存在**

Run:

```bash
cmake --build build_x64_v142 --config Release --target navigation_digital_twin_state_builder_test
ctest --test-dir build_x64_v142 -C Release -R "^navigation_digital_twin_state_builder_test$" --output-on-failure
```

Expected: FAIL，报错集中在 `buildTargetRegionNavigationStatus`、`buildDigitalTwinRiskReport`、`buildDigitalTwinState` 缺失。

- [ ] **Step 3: 创建 builder 头文件**

`Framework/Navigation/navigation_digital_twin_state_builder.h`：

```cpp
#pragma once

#include "Framework/Navigation/ankle_navigation_types.h"
#include "Framework/Navigation/navigation_transform_graph.h"
#include "Framework/Navigation/navigation_confidence_evaluator.h"
#include "Plugins/PointRegistration/PointRegistrationDataStructures.h"

FRAMEWORK_EXPORT TargetRegionNavigationStatus buildTargetRegionNavigationStatus(
    const DigitalTwinTargetRegionDefinition& targetRegion,
    const NavigationTransformResult& transformResult);

FRAMEWORK_EXPORT DigitalTwinRiskReport buildDigitalTwinRiskReport(
    const PointRegistrationResult& registrationResult,
    const QVariantMap& trackingQuality,
    const NavigationConfidenceResult& confidenceResult,
    const TargetRegionNavigationStatus& targetStatus);

FRAMEWORK_EXPORT DigitalTwinState buildDigitalTwinState(
    const PointRegistrationResult& registrationResult,
    const QVariantMap& trackingQuality,
    const NavigationConfidenceResult& confidenceResult,
    const TargetRegionNavigationStatus& targetStatus,
    const DigitalTwinRiskReport& riskReport);
```

- [ ] **Step 4: 实现最小但真实可用的 builder 逻辑**

`Framework/Navigation/navigation_digital_twin_state_builder.cpp` 核心实现：

```cpp
TargetRegionNavigationStatus buildTargetRegionNavigationStatus(
    const DigitalTwinTargetRegionDefinition& targetRegion,
    const NavigationTransformResult& transformResult)
{
    TargetRegionNavigationStatus status;
    status.targetRegionAvailable = targetRegion.available && transformResult.valid;
    if (!status.targetRegionAvailable) {
        return status;
    }

    const QVector3D toolTip = transformResult.vtkToolTransform.column(3).toVector3D();
    const QVector3D delta = toolTip - targetRegion.centerPatient;
    status.distanceToTargetMm = delta.length();

    const QVector3D toolAxis = transformResult.vtkToolTransform.column(2).toVector3D().normalized();
    const QVector3D targetAxis = targetRegion.plannedAxisPatient.normalized();
    const float dotValue = qBound(-1.0f, QVector3D::dotProduct(toolAxis, targetAxis), 1.0f);
    status.angleErrorDeg = qRadiansToDegrees(std::acos(dotValue));

    const double normalizedDistance = targetRegion.radiusMm > 0.0
        ? qMin(1.0, status.distanceToTargetMm / targetRegion.radiusMm)
        : 1.0;
    status.targetHitProbability = qMax(0.0, 1.0 - normalizedDistance);
    status.localConfidenceScore =
        qMax(0.0, 1.0 - (status.angleErrorDeg / 45.0)) * status.targetHitProbability;
    return status;
}
```

```cpp
DigitalTwinRiskReport buildDigitalTwinRiskReport(
    const PointRegistrationResult& registrationResult,
    const QVariantMap& trackingQuality,
    const NavigationConfidenceResult& confidenceResult,
    const TargetRegionNavigationStatus& targetStatus)
{
    DigitalTwinRiskReport report;
    report.rawMetrics.insert(QStringLiteral("target_tre_mm"), registrationResult.targetRegionTre);
    report.rawMetrics.insert(QStringLiteral("coverage_score"), registrationResult.coverageScore);
    report.rawMetrics.insert(QStringLiteral("tracking_jitter_mm"), trackingQuality.value(QStringLiteral("tracking_jitter_mm")));
    report.rawMetrics.insert(QStringLiteral("visible_frame_ratio"), trackingQuality.value(QStringLiteral("visible_frame_ratio")));
    report.rawMetrics.insert(QStringLiteral("confidence_score"), confidenceResult.score);
    report.rawMetrics.insert(QStringLiteral("target_region_distance_mm"), targetStatus.distanceToTargetMm);

    if (registrationResult.targetRegionTre > 2.0) {
        report.dominantRiskSource = QStringLiteral("registration");
        report.riskReasons.append(QStringLiteral("target_tre_high"));
    } else if (trackingQuality.value(QStringLiteral("tracking_jitter_mm")).toDouble() > 0.8
               || trackingQuality.value(QStringLiteral("visible_frame_ratio")).toDouble() < 0.85) {
        report.dominantRiskSource = QStringLiteral("tracking");
        report.riskReasons.append(QStringLiteral("tracking_quality_low"));
    } else if (!trackingQuality.value(QStringLiteral("calibrated")).toBool()
               || trackingQuality.value(QStringLiteral("calibration_accuracy_mm")).toDouble() > 0.8) {
        report.dominantRiskSource = QStringLiteral("calibration");
        report.riskReasons.append(QStringLiteral("calibration_quality_low"));
    } else {
        report.dominantRiskSource = QStringLiteral("target_region");
        report.riskReasons.append(QStringLiteral("local_target_risk"));
    }

    if (registrationResult.coverageScore < 0.75) {
        report.riskReasons.append(QStringLiteral("coverage_low"));
    }

    return report;
}
```

```cpp
DigitalTwinState buildDigitalTwinState(
    const PointRegistrationResult& registrationResult,
    const QVariantMap& trackingQuality,
    const NavigationConfidenceResult& confidenceResult,
    const TargetRegionNavigationStatus& targetStatus,
    const DigitalTwinRiskReport& riskReport)
{
    DigitalTwinState state;
    state.valid = registrationResult.success;
    if (!state.valid) {
        state.statusCode = QStringLiteral("registration_missing");
        state.statusText = QStringLiteral("尚未形成可用配准结果");
        return state;
    }

    const double registrationScore = qMax(0.0, 1.0 - registrationResult.targetRegionTre / 3.0);
    const double trackingScore =
        qMax(0.0, 1.0 - trackingQuality.value(QStringLiteral("tracking_jitter_mm")).toDouble() / 1.0)
        * trackingQuality.value(QStringLiteral("visible_frame_ratio")).toDouble();
    const double calibrationScore = trackingQuality.value(QStringLiteral("calibrated")).toBool()
        ? qMax(0.0, 1.0 - trackingQuality.value(QStringLiteral("calibration_accuracy_mm")).toDouble() / 1.0)
        : 0.0;

    state.twinConfidenceScore =
        0.40 * registrationScore
        + 0.25 * trackingScore
        + 0.15 * calibrationScore
        + 0.20 * targetStatus.localConfidenceScore;
    state.localRiskScore = 1.0 - targetStatus.localConfidenceScore;
    state.allowNavigation = confidenceResult.allowNavigation && state.twinConfidenceScore >= 0.5;
    state.reRegisterRecommended =
        registrationResult.targetRegionTre > 2.0 || state.twinConfidenceScore < 0.45;
    state.trackingDegradationDetected =
        trackingQuality.value(QStringLiteral("tracking_jitter_mm")).toDouble() > 0.8
        || trackingQuality.value(QStringLiteral("visible_frame_ratio")).toDouble() < 0.85;
    state.statusCode = state.allowNavigation ? QStringLiteral("twin_ready") : QStringLiteral("twin_risk_high");
    state.statusText = state.allowNavigation
        ? QStringLiteral("数字孪生状态稳定，可继续导航")
        : QStringLiteral("数字孪生检测到高风险，建议复核");

    state.evidence = riskReport.rawMetrics;
    state.evidence.insert(QStringLiteral("dominant_risk_source"), riskReport.dominantRiskSource);
    state.evidence.insert(QStringLiteral("risk_reason_count"), riskReport.riskReasons.size());
    state.evidence.insert(QStringLiteral("target_region_angle_error_deg"), targetStatus.angleErrorDeg);
    state.evidence.insert(QStringLiteral("target_hit_probability"), targetStatus.targetHitProbability);
    return state;
}
```

- [ ] **Step 5: 注册 Framework 与测试构建**

在 `CMakeLists.txt` 中加入：

```cmake
    Framework/Navigation/navigation_digital_twin_state_builder.h
    Framework/Navigation/navigation_digital_twin_state_builder.cpp
```

在 `tests/unit/CMakeLists.txt` 中加入：

```cmake
add_executable(navigation_digital_twin_state_builder_test
    NavigationDigitalTwinStateBuilderTest.cpp
    ${CMAKE_SOURCE_DIR}/Framework/Navigation/navigation_digital_twin_state_builder.cpp
)

target_include_directories(navigation_digital_twin_state_builder_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(navigation_digital_twin_state_builder_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME navigation_digital_twin_state_builder_test
    COMMAND navigation_digital_twin_state_builder_test
)
```

并把它加入 `MEDICALPRO_FRAMEWORK_UNIT_TEST_TARGETS`。

- [ ] **Step 6: 运行 builder 单测**

Run:

```bash
cmake --build build_x64_v142 --config Release --target navigation_digital_twin_state_builder_test
ctest --test-dir build_x64_v142 -C Release -R "^navigation_digital_twin_state_builder_test$" --output-on-failure
```

Expected: PASS。

- [ ] **Step 7: 提交 builder 模块**

```bash
git add CMakeLists.txt Framework/Navigation/navigation_digital_twin_state_builder.h Framework/Navigation/navigation_digital_twin_state_builder.cpp tests/unit/NavigationDigitalTwinStateBuilderTest.cpp tests/unit/CMakeLists.txt
git commit -m "feat: add error-aware digital twin state builder"
```

## Task 3: Integrate Target Region Context And Twin State Into Runtime

**Files:**
- Modify: `UI/NewPages/Navigation/navigation_runtime_state.h`
- Modify: `UI/NewPages/Navigation/navigation_runtime_state.cpp`
- Modify: `UI/NewPages/Navigation/navigation_runtime_coordinator.h`
- Modify: `UI/NewPages/Navigation/navigation_runtime_coordinator.cpp`
- Modify: `tests/unit/NavigationRuntimeCoordinatorContractTest.cpp`

- [ ] **Step 1: 先写失败测试，锁定 runtime coordinator 会持续刷新 twin state**

在 `tests/unit/NavigationRuntimeCoordinatorContractTest.cpp` 追加：

```cpp
void NavigationRuntimeCoordinatorContractTest::coordinator_refreshes_digital_twin_state_from_registration_tracking_and_target_region()
{
    NavigationRuntimeState runtimeState;
    runtimeState.setCaseContext(QStringLiteral("case-twin-rt-001"), QStringLiteral("tracking-001"), QStringLiteral("instrument:probe-main"));

    NavigationRuntimeCoordinator coordinator(&runtimeState);

    DigitalTwinTargetRegionDefinition targetRegion;
    targetRegion.available = true;
    targetRegion.centerPatient = QVector3D(10.0f, 0.0f, 7.0f);
    targetRegion.plannedAxisPatient = QVector3D(0.0f, 0.0f, 1.0f);
    targetRegion.radiusMm = 5.0;
    coordinator.setTargetRegionDefinition(targetRegion);

    QVariantMap trackingQuality;
    trackingQuality.insert(QStringLiteral("tracking_jitter_mm"), 0.22);
    trackingQuality.insert(QStringLiteral("visible_frame_ratio"), 0.98);
    trackingQuality.insert(QStringLiteral("calibrated"), true);
    trackingQuality.insert(QStringLiteral("calibration_accuracy_mm"), 0.32);
    coordinator.handleTrackingQuality(trackingQuality);

    PointRegistrationResult registrationResult;
    registrationResult.success = true;
    registrationResult.targetRegionTre = 1.1;
    registrationResult.coverageScore = 0.88;
    coordinator.handleRegistrationResult(registrationResult);

    NavigationConfidenceResult confidenceResult;
    confidenceResult.allowNavigation = true;
    confidenceResult.score = 0.83;
    runtimeState.setConfidenceResult(confidenceResult);

    QMatrix4x4 markerToTool;
    markerToTool.translate(0.0f, 5.0f, 0.0f);
    coordinator.handleCalibrationTransform(markerToTool);

    QMatrix4x4 patientToVtkWorld;
    patientToVtkWorld.translate(0.0f, 0.0f, 7.0f);
    coordinator.handleRegistrationTransform(patientToVtkWorld);

    NavigationPoseFrame frame;
    frame.sourceId = QStringLiteral("simulator");
    frame.toolId = QStringLiteral("instrument:probe-main");
    frame.timestamp = QDateTime::currentDateTimeUtc();
    frame.trackingVisible = true;
    frame.trackingToMarker.translate(10.0f, 0.0f, 0.0f);
    coordinator.handlePoseFrame(frame);

    QVERIFY(runtimeState.hasDigitalTwinState());
    QVERIFY(runtimeState.hasTargetRegionNavigationStatus());
    QVERIFY(runtimeState.hasDigitalTwinRiskReport());
    QVERIFY(runtimeState.digitalTwinState().valid);
    QVERIFY(runtimeState.digitalTwinState().twinConfidenceScore > 0.5);
    QCOMPARE(runtimeState.targetRegionNavigationStatus().distanceToTargetMm, 5.0);
}
```

- [ ] **Step 2: 运行 contract test，确认 runtime state/coordinator 还没有 twin 状态接口**

Run:

```bash
cmake --build build_x64_v142 --config Release --target navigation_runtime_coordinator_contract_test
ctest --test-dir build_x64_v142 -C Release -R "^navigation_runtime_coordinator_contract_test$" --output-on-failure
```

Expected: FAIL，报错集中在 `setTargetRegionDefinition`、`hasDigitalTwinState`、`digitalTwinState` 等接口不存在。

- [ ] **Step 3: 扩展 runtime state，保存 target region 与 twin 状态**

在 `navigation_runtime_state.h` 中加入：

```cpp
    void setTargetRegionDefinition(const DigitalTwinTargetRegionDefinition& definition);
    void clearTargetRegionDefinition();
    bool hasTargetRegionDefinition() const;
    const DigitalTwinTargetRegionDefinition& targetRegionDefinition() const;

    void setTargetRegionNavigationStatus(const TargetRegionNavigationStatus& status);
    void clearTargetRegionNavigationStatus();
    bool hasTargetRegionNavigationStatus() const;
    const TargetRegionNavigationStatus& targetRegionNavigationStatus() const;

    void setDigitalTwinRiskReport(const DigitalTwinRiskReport& report);
    void clearDigitalTwinRiskReport();
    bool hasDigitalTwinRiskReport() const;
    const DigitalTwinRiskReport& digitalTwinRiskReport() const;

    void setDigitalTwinState(const DigitalTwinState& state);
    void clearDigitalTwinState();
    bool hasDigitalTwinState() const;
    const DigitalTwinState& digitalTwinState() const;
```

并补充成员：

```cpp
    DigitalTwinTargetRegionDefinition m_targetRegionDefinition;
    TargetRegionNavigationStatus m_targetRegionNavigationStatus;
    DigitalTwinRiskReport m_digitalTwinRiskReport;
    DigitalTwinState m_digitalTwinState;
    bool m_hasTargetRegionDefinition = false;
    bool m_hasTargetRegionNavigationStatus = false;
    bool m_hasDigitalTwinRiskReport = false;
    bool m_hasDigitalTwinState = false;
```

- [ ] **Step 4: 在 runtime coordinator 中统一刷新 twin state**

在 `navigation_runtime_coordinator.h` 中加入：

```cpp
    void setTargetRegionDefinition(const DigitalTwinTargetRegionDefinition& definition);
    void clearTargetRegionDefinition();

private:
    void refreshDigitalTwinState();
```

在 `navigation_runtime_coordinator.cpp` 中实现：

```cpp
void NavigationRuntimeCoordinator::setTargetRegionDefinition(const DigitalTwinTargetRegionDefinition& definition)
{
    if (!m_runtimeState) {
        return;
    }

    m_runtimeState->setTargetRegionDefinition(definition);
    refreshDigitalTwinState();
}

void NavigationRuntimeCoordinator::refreshDigitalTwinState()
{
    if (!m_runtimeState
        || !m_runtimeState->hasRegistrationResult()
        || !m_runtimeState->hasTrackingQuality()
        || !m_runtimeState->hasConfidenceResult()
        || !m_runtimeState->hasLatestTransformResult()
        || !m_runtimeState->hasTargetRegionDefinition()) {
        return;
    }

    const TargetRegionNavigationStatus targetStatus = buildTargetRegionNavigationStatus(
        m_runtimeState->targetRegionDefinition(),
        m_runtimeState->latestTransformResult());
    const DigitalTwinRiskReport riskReport = buildDigitalTwinRiskReport(
        m_runtimeState->registrationResult(),
        m_runtimeState->trackingQuality(),
        m_runtimeState->confidenceResult(),
        targetStatus);
    const DigitalTwinState twinState = buildDigitalTwinState(
        m_runtimeState->registrationResult(),
        m_runtimeState->trackingQuality(),
        m_runtimeState->confidenceResult(),
        targetStatus,
        riskReport);

    m_runtimeState->setTargetRegionNavigationStatus(targetStatus);
    m_runtimeState->setDigitalTwinRiskReport(riskReport);
    m_runtimeState->setDigitalTwinState(twinState);
}
```

并在以下入口尾部调用 `refreshDigitalTwinState();`

```cpp
handleRegistrationResult(...)
handleTrackingQuality(...)
handleCalibrationCompleted(...)
handlePoseFrame(...)
recomputeConfidence()
```

- [ ] **Step 5: 增加 twin 指标进入 evaluation report 的 contract 测试**

在同一测试文件追加：

```cpp
void NavigationRuntimeCoordinatorContractTest::coordinator_persists_digital_twin_metrics_into_evaluation_report()
{
    NavigationRuntimeState runtimeState;
    runtimeState.setCaseContext(QStringLiteral("case-twin-export-001"), QStringLiteral("tracking-001"), QStringLiteral("instrument:probe-main"));

    QVariantMap trackingQuality;
    trackingQuality.insert(QStringLiteral("tracking_jitter_mm"), 0.86);
    trackingQuality.insert(QStringLiteral("visible_frame_ratio"), 0.80);
    trackingQuality.insert(QStringLiteral("calibrated"), true);
    trackingQuality.insert(QStringLiteral("calibration_accuracy_mm"), 0.74);
    runtimeState.setTrackingQuality(trackingQuality);

    PointRegistrationResult registrationResult;
    registrationResult.success = true;
    registrationResult.targetRegionTre = 2.4;
    registrationResult.coverageScore = 0.66;
    runtimeState.setRegistrationResult(registrationResult);

    NavigationConfidenceResult confidenceResult;
    confidenceResult.allowNavigation = false;
    confidenceResult.score = 0.39;
    runtimeState.setConfidenceResult(confidenceResult);

    DigitalTwinTargetRegionDefinition targetRegion;
    targetRegion.available = true;
    targetRegion.centerPatient = QVector3D(0.0f, 0.0f, 0.0f);
    targetRegion.radiusMm = 5.0;

    AnkleEvaluationReport savedReport;
    NavigationRuntimeCoordinator::PersistenceActions persistenceActions;
    persistenceActions.loadEvaluationSnapshot = [](const QString& caseId) {
        AnkleEvaluationSnapshot snapshot;
        snapshot.caseId = caseId;
        return snapshot;
    };
    persistenceActions.saveEvaluationReport = [&savedReport](const AnkleEvaluationReport& report) {
        savedReport = report;
        return true;
    };
    persistenceActions.exportMetricsCsv = [](const QString&) { return true; };
    persistenceActions.exportCaseSummary = [](const QString&) { return true; };

    NavigationRuntimeCoordinator coordinator(&runtimeState, persistenceActions);
    coordinator.setTargetRegionDefinition(targetRegion);

    QMatrix4x4 markerToTool;
    coordinator.handleCalibrationTransform(markerToTool);
    QMatrix4x4 patientToVtkWorld;
    coordinator.handleRegistrationTransform(patientToVtkWorld);

    NavigationPoseFrame frame;
    frame.toolId = QStringLiteral("instrument:probe-main");
    frame.trackingVisible = true;
    frame.timestamp = QDateTime::currentDateTimeUtc();
    frame.trackingToMarker.translate(9.0f, 0.0f, 0.0f);
    coordinator.handlePoseFrame(frame);

    coordinator.persistEvaluationReportSnapshot();

    QVERIFY(savedReport.metrics.contains(QStringLiteral("twin_confidence_score")));
    QVERIFY(savedReport.metrics.contains(QStringLiteral("target_region_distance_mm")));
    QVERIFY(savedReport.metrics.contains(QStringLiteral("local_risk_score")));
    QVERIFY(savedReport.metrics.contains(QStringLiteral("dominant_risk_source")));
    QVERIFY(savedReport.metrics.contains(QStringLiteral("re_register_recommended")));
}
```

- [ ] **Step 6: 运行 runtime 相关测试**

Run:

```bash
cmake --build build_x64_v142 --config Release --target navigation_runtime_coordinator_contract_test navigation_runtime_state_test
ctest --test-dir build_x64_v142 -C Release -R "^(navigation_runtime_coordinator_contract_test|navigation_runtime_state_test)$" --output-on-failure
```

Expected: PASS。

- [ ] **Step 7: 提交 runtime twin 集成**

```bash
git add UI/NewPages/Navigation/navigation_runtime_state.h UI/NewPages/Navigation/navigation_runtime_state.cpp UI/NewPages/Navigation/navigation_runtime_coordinator.h UI/NewPages/Navigation/navigation_runtime_coordinator.cpp tests/unit/NavigationRuntimeCoordinatorContractTest.cpp
git commit -m "feat: integrate error-aware digital twin state into runtime coordinator"
```

## Task 4: Add Target Region Overlay And Risk Tone To The VTK Twin

**Files:**
- Modify: `UI/Widgets/Navigation3DViewWidget.h`
- Modify: `UI/Widgets/Navigation3DViewWidget.cpp`
- Modify: `UI/NewPages/Navigation/navigation_vtk_bridge.h`
- Modify: `UI/NewPages/Navigation/navigation_vtk_bridge.cpp`
- Modify: `tests/unit/NavigationVtkBridgeTest.cpp`

- [ ] **Step 1: 先写失败测试，锁定 target region overlay 和 risk tone 桥接**

在 `tests/unit/NavigationVtkBridgeTest.cpp` 中追加：

```cpp
void NavigationVtkBridgeTest::bridge_updates_target_region_overlay_and_risk_tone()
{
    NavigationVtkBridge bridge(
        nullptr,
        nullptr,
        nullptr,
        []() { return nullptr; },
        []() { return nullptr; });

    Navigation3DViewWidget navigationView;
    bridge.setNavigationViewWidget(&navigationView);

    DigitalTwinTargetRegionDefinition targetRegion;
    targetRegion.available = true;
    targetRegion.centerPatient = QVector3D(12.0f, -4.0f, 8.0f);
    targetRegion.radiusMm = 18.0;

    bridge.setTargetRegionDefinition(targetRegion);
    bridge.setTargetRegionRiskTone(QStringLiteral("warning"));

    QVERIFY(bridge.hasTargetRegionDefinition());
    QCOMPARE(bridge.targetRegionDefinition().centerPatient, QVector3D(12.0f, -4.0f, 8.0f));
    QCOMPARE(bridge.targetRegionRiskTone(), QStringLiteral("warning"));
    QVERIFY(navigationView.hasTargetRegionActor());
}
```

- [ ] **Step 2: 运行 VTK bridge 测试，确认当前还不支持目标区 overlay**

Run:

```bash
cmake --build build_x64_v142 --config Release --target navigation_vtk_bridge_test
ctest --test-dir build_x64_v142 -C Release -R "^navigation_vtk_bridge_test$" --output-on-failure
```

Expected: FAIL，报错集中在 `setTargetRegionDefinition`、`targetRegionRiskTone`、`hasTargetRegionActor` 缺失。

- [ ] **Step 3: 在 3D widget 中补 target region actor 接口**

在 `UI/Widgets/Navigation3DViewWidget.h` 中加入：

```cpp
    void setTargetRegionMarker(const QVector3D& center, double radiusMm);
    void clearTargetRegionMarker();
    bool hasTargetRegionActor() const;
    void setTargetRegionRiskTone(const QString& tone);
    QString targetRegionRiskTone() const;
```

并新增成员：

```cpp
    vtkSmartPointer<vtkActor> m_targetRegionActor;
    QString m_targetRegionRiskTone = QStringLiteral("ok");
```

- [ ] **Step 4: 用最小球体 actor 表达目标区，并用 tone 驱动颜色**

在 `Navigation3DViewWidget.cpp` 中实现：

```cpp
void Navigation3DViewWidget::setTargetRegionMarker(const QVector3D& center, double radiusMm)
{
    if (!m_targetRegionActor) {
        auto sphere = vtkSmartPointer<vtkSphereSource>::New();
        sphere->SetPhiResolution(24);
        sphere->SetThetaResolution(24);
        sphere->SetCenter(center.x(), center.y(), center.z());
        sphere->SetRadius(radiusMm);

        auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputConnection(sphere->GetOutputPort());

        m_targetRegionActor = vtkSmartPointer<vtkActor>::New();
        m_targetRegionActor->SetMapper(mapper);
        m_targetRegionActor->GetProperty()->SetOpacity(0.18);
        m_renderer->AddActor(m_targetRegionActor);
    }

    setTargetRegionRiskTone(m_targetRegionRiskTone);
    render();
}
```

```cpp
void Navigation3DViewWidget::setTargetRegionRiskTone(const QString& tone)
{
    m_targetRegionRiskTone = tone;
    if (!m_targetRegionActor) {
        return;
    }

    if (tone == QStringLiteral("warning")) {
        m_targetRegionActor->GetProperty()->SetColor(0.95, 0.67, 0.12);
    } else if (tone == QStringLiteral("danger")) {
        m_targetRegionActor->GetProperty()->SetColor(0.85, 0.23, 0.18);
    } else {
        m_targetRegionActor->GetProperty()->SetColor(0.16, 0.68, 0.45);
    }
}
```

- [ ] **Step 5: 在 VTK bridge 中增加 target region 与 risk tone 桥接**

在 `navigation_vtk_bridge.h` 中加入：

```cpp
    void setTargetRegionDefinition(const DigitalTwinTargetRegionDefinition& definition);
    void clearTargetRegionDefinition();
    bool hasTargetRegionDefinition() const;
    DigitalTwinTargetRegionDefinition targetRegionDefinition() const;
    void setTargetRegionRiskTone(const QString& tone);
    QString targetRegionRiskTone() const;
```

在 `navigation_vtk_bridge.cpp` 中实现：

```cpp
void NavigationVtkBridge::setTargetRegionDefinition(const DigitalTwinTargetRegionDefinition& definition)
{
    m_targetRegionDefinition = definition;
    m_hasTargetRegionDefinition = definition.available;
    if (m_navigationViewWidget && definition.available) {
        m_navigationViewWidget->setTargetRegionMarker(definition.centerPatient, definition.radiusMm);
    }
}

void NavigationVtkBridge::setTargetRegionRiskTone(const QString& tone)
{
    m_targetRegionRiskTone = tone;
    if (m_navigationViewWidget) {
        m_navigationViewWidget->setTargetRegionRiskTone(tone);
    }
}
```

- [ ] **Step 6: 重跑 VTK bridge 测试**

Run:

```bash
cmake --build build_x64_v142 --config Release --target navigation_vtk_bridge_test
ctest --test-dir build_x64_v142 -C Release -R "^navigation_vtk_bridge_test$" --output-on-failure
```

Expected: PASS。

- [ ] **Step 7: 提交 VTK twin overlay**

```bash
git add UI/Widgets/Navigation3DViewWidget.h UI/Widgets/Navigation3DViewWidget.cpp UI/NewPages/Navigation/navigation_vtk_bridge.h UI/NewPages/Navigation/navigation_vtk_bridge.cpp tests/unit/NavigationVtkBridgeTest.cpp
git commit -m "feat: add target region overlay to digital twin view"
```

## Task 5: Upgrade HUD, Workspace Summary, And Navigation Page Wiring

**Files:**
- Modify: `UI/NewPages/NavigationPage.h`
- Modify: `UI/NewPages/NavigationPage.cpp`
- Modify: `UI/NewPages/Navigation/navigation_workspace_ui_binder.cpp`
- Modify: `UI/NewPages/Navigation/navigation_evaluation_summary_formatter.cpp`
- Modify: `UI/styles/three_pages_theme.qss`
- Modify: `tests/unit/AnkleNavigationWorkflowContractTest.cpp`
- Modify: `tests/unit/NavigationEvaluationSummaryFormatterTest.cpp`

- [ ] **Step 1: 先写失败 contract test，锁定导航页必须暴露 error-aware digital twin HUD**

在 `tests/unit/AnkleNavigationWorkflowContractTest.cpp` 中追加：

```cpp
void AnkleNavigationWorkflowContractTest::navigation_page_exposes_error_aware_digital_twin_hud()
{
    const QString pageCode = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));
    const QString theme = readFile(QStringLiteral("UI/styles/three_pages_theme.qss"));

    QVERIFY2(pageCode.contains(QStringLiteral("navigationHudRiskLabel")),
        "navigation page must expose digital twin dominant risk text");
    QVERIFY2(pageCode.contains(QStringLiteral("navigationHudTargetLabel")),
        "navigation page must expose target region navigation status text");
    QVERIFY2(pageCode.contains(QStringLiteral("setTargetRegionDefinition(")),
        "navigation page must push target region context into runtime coordinator or vtk bridge");
    QVERIFY2(theme.contains(QStringLiteral("QFrame#navigationHudFrame")),
        "theme must style the digital twin hud frame");
}
```

- [ ] **Step 2: 扩展失败的 summary formatter 测试，要求输出 twin 指标**

在 `tests/unit/NavigationEvaluationSummaryFormatterTest.cpp` 中追加：

```cpp
void NavigationEvaluationSummaryFormatterTest::formatter_includes_digital_twin_confidence_risk_and_target_distance()
{
    AnkleEvaluationSnapshot snapshot;
    snapshot.caseId = QStringLiteral("ankle-case-twin-summary-001");
    snapshot.hasEvaluationReport = true;
    snapshot.allowNavigation = false;
    snapshot.evaluationConfidenceScore = 0.41;
    snapshot.evaluationMetrics.insert(QStringLiteral("twin_confidence_score"), 0.37);
    snapshot.evaluationMetrics.insert(QStringLiteral("local_risk_score"), 0.72);
    snapshot.evaluationMetrics.insert(QStringLiteral("target_region_distance_mm"), 6.8);
    snapshot.evaluationMetrics.insert(QStringLiteral("dominant_risk_source"), QStringLiteral("registration"));
    snapshot.evaluationMetrics.insert(QStringLiteral("re_register_recommended"), true);

    const NavigationEvaluationSummary summary = buildNavigationEvaluationSummary(snapshot);

    QVERIFY(summary.gateText.contains(QStringLiteral("0.37")));
    QVERIFY(summary.gateText.contains(QStringLiteral("0.72")));
    QVERIFY(summary.gateText.contains(QStringLiteral("6.80")));
    QVERIFY(summary.gateText.contains(QStringLiteral("registration")));
}
```

- [ ] **Step 3: 运行 contract + summary formatter 测试，确认 UI 还没有 twin 风险层**

Run:

```bash
cmake --build build_x64_v142 --config Release --target ankle_navigation_workflow_contract_test navigation_evaluation_summary_formatter_test
ctest --test-dir build_x64_v142 -C Release -R "^(ankle_navigation_workflow_contract_test|navigation_evaluation_summary_formatter_test)$" --output-on-failure
```

Expected: FAIL。

- [ ] **Step 4: 在 NavigationPage 中扩展 HUD，并把 planning snapshot 转成 target region context**

在 `NavigationPage.h` 中加入：

```cpp
    QLabel* m_navigationHudRiskLabel = nullptr;
    QLabel* m_navigationHudTargetLabel = nullptr;
    void refreshDigitalTwinHudState();
    DigitalTwinTargetRegionDefinition currentTargetRegionDefinition() const;
```

在 `NavigationPage.cpp` 的 `setupNavigationHud()` 中补充：

```cpp
    m_navigationHudRiskLabel = createStageTextLabel(
        QStringLiteral("navigationHudRiskLabel"),
        QStringLiteral("主风险：待评估"));
    m_navigationHudTargetLabel = createStageTextLabel(
        QStringLiteral("navigationHudTargetLabel"),
        QStringLiteral("目标区：待规划"));

    m_navigationHudRiskLabel->setParent(m_navigationHudFrame);
    m_navigationHudTargetLabel->setParent(m_navigationHudFrame);
    layout->addWidget(m_navigationHudRiskLabel);
    layout->addWidget(m_navigationHudTargetLabel);
```

实现 `currentTargetRegionDefinition()`：

```cpp
DigitalTwinTargetRegionDefinition NavigationPageNew::currentTargetRegionDefinition() const
{
    DigitalTwinTargetRegionDefinition definition;
    if (!m_workspaceApplicationService) {
        return definition;
    }

    const NavigationWorkspacePlanningState planningState =
        m_workspaceApplicationService->currentSnapshot().planningState;
    definition.available = planningState.targetRegionReady && planningState.targetRegionRadiusMm > 0.0;
    definition.centerPatient = planningState.targetRegionCenter;
    definition.plannedAxisPatient = planningState.targetRegionAxis;
    definition.radiusMm = planningState.targetRegionRadiusMm;
    return definition;
}
```

- [ ] **Step 5: 在 HUD 刷新与 VTK twin 刷新时同步 twin state**

在 `refreshNavigationHudFromDisplayState(...)` 附近加入：

```cpp
    if (m_runtimeCoordinator) {
        const DigitalTwinTargetRegionDefinition targetRegion = currentTargetRegionDefinition();
        m_runtimeCoordinator->setTargetRegionDefinition(targetRegion);
        m_navigationVtkBridge->setTargetRegionDefinition(targetRegion);
    }

    if (m_runtimeCoordinator && m_runtimeCoordinator->runtimeState()->hasDigitalTwinState()) {
        const DigitalTwinState twinState = m_runtimeCoordinator->runtimeState()->digitalTwinState();
        const TargetRegionNavigationStatus targetStatus =
            m_runtimeCoordinator->runtimeState()->targetRegionNavigationStatus();
        const DigitalTwinRiskReport riskReport =
            m_runtimeCoordinator->runtimeState()->digitalTwinRiskReport();

        m_navigationHudRiskLabel->setText(QStringLiteral("主风险：%1 | 建议重配准：%2")
            .arg(riskReport.dominantRiskSource.isEmpty() ? QStringLiteral("无") : riskReport.dominantRiskSource)
            .arg(twinState.reRegisterRecommended ? QStringLiteral("是") : QStringLiteral("否")));
        m_navigationHudTargetLabel->setText(QStringLiteral("目标距离：%1 mm | 角度误差：%2 deg | twin：%3")
            .arg(targetStatus.distanceToTargetMm, 0, 'f', 2)
            .arg(targetStatus.angleErrorDeg, 0, 'f', 2)
            .arg(twinState.twinConfidenceScore, 0, 'f', 2));

        m_navigationVtkBridge->setTargetRegionRiskTone(
            twinState.localRiskScore > 0.7 ? QStringLiteral("danger")
            : twinState.localRiskScore > 0.4 ? QStringLiteral("warning")
                                             : QStringLiteral("ok"));
    }
```

- [ ] **Step 6: 扩展 workspace binder 与评估 summary formatter**

在 `navigation_workspace_ui_binder.cpp` 中加入 twin 摘要：

```cpp
    lines.append(QStringLiteral("数字孪生：%1")
                     .arg(snapshot.navigationState.summaryText.isEmpty()
                              ? QStringLiteral("待刷新")
                              : snapshot.navigationState.summaryText));
```

在 `navigation_evaluation_summary_formatter.cpp` 中补充：

```cpp
    summary.gateText.append(QStringLiteral(
        "\nTwin 可信度：%1\n局部风险：%2\n目标距离：%3 mm\n主风险：%4\n建议重配准：%5")
        .arg(snapshot.evaluationMetrics.value(QStringLiteral("twin_confidence_score")).toDouble(), 0, 'f', 2)
        .arg(snapshot.evaluationMetrics.value(QStringLiteral("local_risk_score")).toDouble(), 0, 'f', 2)
        .arg(snapshot.evaluationMetrics.value(QStringLiteral("target_region_distance_mm")).toDouble(), 0, 'f', 2)
        .arg(valueOrDash(snapshot.evaluationMetrics, QStringLiteral("dominant_risk_source")))
        .arg(snapshot.evaluationMetrics.value(QStringLiteral("re_register_recommended")).toBool()
                 ? QStringLiteral("是")
                 : QStringLiteral("否")));
```

并在 `three_pages_theme.qss` 中增加：

```css
QWidget#NavigationPage QFrame#navigationHudFrame {
    background-color: rgba(12, 24, 32, 0.88);
    border: 1px solid rgba(124, 160, 191, 0.16);
    border-radius: 10px;
}

QWidget#NavigationPage QLabel#navigationHudRiskLabel {
    color: #f4b63a;
    font-size: 14px;
    font-weight: 700;
}

QWidget#NavigationPage QLabel#navigationHudTargetLabel {
    color: #d9e4ec;
    font-size: 14px;
}
```

- [ ] **Step 7: 重跑导航 contract 和 summary formatter 测试**

Run:

```bash
cmake --build build_x64_v142 --config Release --target ankle_navigation_workflow_contract_test navigation_evaluation_summary_formatter_test
ctest --test-dir build_x64_v142 -C Release -R "^(ankle_navigation_workflow_contract_test|navigation_evaluation_summary_formatter_test)$" --output-on-failure
```

Expected: PASS。

- [ ] **Step 8: 提交 HUD 与摘要层**

```bash
git add UI/NewPages/NavigationPage.h UI/NewPages/NavigationPage.cpp UI/NewPages/Navigation/navigation_workspace_ui_binder.cpp UI/NewPages/Navigation/navigation_evaluation_summary_formatter.cpp UI/styles/three_pages_theme.qss tests/unit/AnkleNavigationWorkflowContractTest.cpp tests/unit/NavigationEvaluationSummaryFormatterTest.cpp
git commit -m "feat: surface error-aware digital twin state in navigation hud"
```

## Task 6: Export Twin Metrics Through NavigationEvaluationService

**Files:**
- Modify: `Framework/Navigation/navigation_evaluation_service.cpp`
- Modify: `tests/unit/NavigationEvaluationServiceTest.cpp`

- [ ] **Step 1: 先写失败测试，锁定 json/csv/case summary 必须保留 twin 指标**

在 `tests/unit/NavigationEvaluationServiceTest.cpp` 中追加：

```cpp
void NavigationEvaluationServiceTest::service_round_trips_digital_twin_metrics_and_case_summary_fields()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    NavigationEvaluationService service(tempRoot.path());

    AnkleRegistrationRecord registration;
    registration.caseId = QStringLiteral("ankle-case-twin-export-001");
    registration.registrationMode = QStringLiteral("ankle_two_stage_constrained");
    registration.fre = 0.82;
    registration.targetTre = 1.36;
    registration.coverageScore = 0.91;

    AnkleNavigationRunRecord run;
    run.caseId = registration.caseId;
    run.navigationMode = QStringLiteral("live_tracking");
    run.confidenceScore = 0.87;

    AnkleEvaluationReport report;
    report.caseId = registration.caseId;
    report.allowNavigation = false;
    report.confidenceScore = 0.41;
    report.metrics.insert(QStringLiteral("twin_confidence_score"), 0.37);
    report.metrics.insert(QStringLiteral("local_risk_score"), 0.72);
    report.metrics.insert(QStringLiteral("target_region_distance_mm"), 6.8);
    report.metrics.insert(QStringLiteral("target_region_angle_error_deg"), 12.5);
    report.metrics.insert(QStringLiteral("dominant_risk_source"), QStringLiteral("registration"));
    report.metrics.insert(QStringLiteral("re_register_recommended"), true);
    report.metrics.insert(QStringLiteral("tracking_degradation_detected"), true);

    QVERIFY(service.saveRegistrationRecord(registration));
    QVERIFY(service.saveNavigationRun(run));
    QVERIFY(service.saveEvaluationReport(report));
    QVERIFY(service.exportMetricsCsv(report.caseId));
    QVERIFY(service.exportCaseSummary(report.caseId));

    const AnkleEvaluationSnapshot snapshot = service.loadEvaluationSnapshot(report.caseId);
    QCOMPARE(snapshot.evaluationMetrics.value(QStringLiteral("twin_confidence_score")).toDouble(), 0.37);
    QCOMPARE(snapshot.evaluationMetrics.value(QStringLiteral("local_risk_score")).toDouble(), 0.72);
    QCOMPARE(snapshot.evaluationMetrics.value(QStringLiteral("target_region_distance_mm")).toDouble(), 6.8);
}
```

- [ ] **Step 2: 运行评估服务测试，确认 twin 指标还没有显式进入 summary 输出**

Run:

```bash
cmake --build build_x64_v142 --config Release --target navigation_evaluation_service_test
ctest --test-dir build_x64_v142 -C Release -R "^navigation_evaluation_service_test$" --output-on-failure
```

Expected: FAIL，表现为 summary json/csv 中缺少显式 twin 字段。

- [ ] **Step 3: 在 snapshot json 和 case summary 中加入 twin 指标主字段**

在 `navigation_evaluation_service.cpp` 的 `toJson(const AnkleEvaluationSnapshot& snapshot)` 中加入：

```cpp
    object.insert(QStringLiteral("twin_confidence_score"),
                  snapshot.evaluationMetrics.value(QStringLiteral("twin_confidence_score")).toDouble());
    object.insert(QStringLiteral("local_risk_score"),
                  snapshot.evaluationMetrics.value(QStringLiteral("local_risk_score")).toDouble());
    object.insert(QStringLiteral("target_region_distance_mm"),
                  snapshot.evaluationMetrics.value(QStringLiteral("target_region_distance_mm")).toDouble());
    object.insert(QStringLiteral("target_region_angle_error_deg"),
                  snapshot.evaluationMetrics.value(QStringLiteral("target_region_angle_error_deg")).toDouble());
    object.insert(QStringLiteral("dominant_risk_source"),
                  snapshot.evaluationMetrics.value(QStringLiteral("dominant_risk_source")).toString());
    object.insert(QStringLiteral("re_register_recommended"),
                  snapshot.evaluationMetrics.value(QStringLiteral("re_register_recommended")).toBool());
```

- [ ] **Step 4: 在 metrics csv 中显式导出 twin 关键行**

在 `exportMetricsCsv(...)` 组装 `lines` 时补充：

```cpp
        QStringLiteral("twin_confidence_score,%1").arg(evaluationMetrics.value(QStringLiteral("twin_confidence_score")).toDouble(), 0, 'f', 4),
        QStringLiteral("local_risk_score,%1").arg(evaluationMetrics.value(QStringLiteral("local_risk_score")).toDouble(), 0, 'f', 4),
        QStringLiteral("target_region_distance_mm,%1").arg(evaluationMetrics.value(QStringLiteral("target_region_distance_mm")).toDouble(), 0, 'f', 4),
        QStringLiteral("target_region_angle_error_deg,%1").arg(evaluationMetrics.value(QStringLiteral("target_region_angle_error_deg")).toDouble(), 0, 'f', 4),
        QStringLiteral("dominant_risk_source,%1").arg(evaluationMetrics.value(QStringLiteral("dominant_risk_source")).toString()),
        QStringLiteral("re_register_recommended,%1").arg(evaluationMetrics.value(QStringLiteral("re_register_recommended")).toBool() ? QStringLiteral("true") : QStringLiteral("false")),
        QStringLiteral("tracking_degradation_detected,%1").arg(evaluationMetrics.value(QStringLiteral("tracking_degradation_detected")).toBool() ? QStringLiteral("true") : QStringLiteral("false")),
```

- [ ] **Step 5: 更新 case summary csv 行**

在 `exportBatchSummaryCsv(...)` 的 header 与 row 中补 twin 列：

```cpp
"case_id,registration_mode,navigation_mode,allow_navigation,fre,target_tre,coverage_score,"
"tracking_jitter_mm,visible_frame_ratio,tracking_confidence_score,gate_reason_count,"
"calibration_accuracy_mm,twin_confidence_score,local_risk_score,target_region_distance_mm,"
"target_region_angle_error_deg,dominant_risk_source,re_register_recommended\n"
```

- [ ] **Step 6: 重跑评估服务测试**

Run:

```bash
cmake --build build_x64_v142 --config Release --target navigation_evaluation_service_test
ctest --test-dir build_x64_v142 -C Release -R "^navigation_evaluation_service_test$" --output-on-failure
```

Expected: PASS。

- [ ] **Step 7: 提交 twin 指标导出**

```bash
git add Framework/Navigation/navigation_evaluation_service.cpp tests/unit/NavigationEvaluationServiceTest.cpp
git commit -m "feat: export digital twin metrics in evaluation reports"
```

## Task 7: Update Project Docs And Experiment Guide

**Files:**
- Modify: `docs/current_status_and_project_overview.md`
- Create: `docs/superpowers/specs/2026-05-25-ankle-navigation-error-aware-digital-twin-experiment-guide.md`

- [x] **Step 1: 更新项目总览里的数字孪生现状**

在 `docs/current_status_and_project_overview.md` 增补：

```md
## 数字孪生现状补充

- 当前导航系统已具备实时位姿链、单窗口 3D twin 和病例级评估落盘。
- 本轮增强后，数字孪生从“显示型”升级为“误差感知型”，可统一表达配准、追踪、标定和目标区风险。
- 新增 twin 指标包括：
  - `twin_confidence_score`
  - `local_risk_score`
  - `target_region_distance_mm`
  - `target_region_angle_error_deg`
  - `dominant_risk_source`
  - `re_register_recommended`
```

- [x] **Step 2: 新建实验手册，固定 twin 章节实验口径**

`docs/superpowers/specs/2026-05-25-ankle-navigation-error-aware-digital-twin-experiment-guide.md` 写成：

```md
# Ankle Navigation Error-Aware Digital Twin Experiment Guide

## Core Outputs

- `twin_confidence_score`
- `local_risk_score`
- `target_region_distance_mm`
- `target_region_angle_error_deg`
- `dominant_risk_source`
- `re_register_recommended`

## Comparison Groups

1. Display-only digital twin
2. Evidence-aware digital twin
3. Error-aware decision digital twin

## Recommended Thresholds

- `target_tre_mm > 2.0` => registration risk high
- `tracking_jitter_mm > 0.8` => tracking degradation
- `visible_frame_ratio < 0.85` => tracking degradation
- `twin_confidence_score < 0.45` => recommend re-register
```

- [x] **Step 3: 自查文档术语一致性**

Run:

```bash
rg -n "twin_confidence_score|target_region_distance_mm|dominant_risk_source|re_register_recommended" docs/current_status_and_project_overview.md docs/superpowers/specs/2026-05-25-ankle-navigation-error-aware-digital-twin-experiment-guide.md
```

Expected: 两份文档都能命中相同指标名，不出现旧命名冲突。

- [x] **Step 4: 归档文档更新状态（未执行 commit）**

```bash
git add docs/current_status_and_project_overview.md docs/superpowers/specs/2026-05-25-ankle-navigation-error-aware-digital-twin-experiment-guide.md
git commit -m "docs: add error-aware digital twin experiment guide"
```

Status:

- 文档内容已落到工作区：
  - `docs/current_status_and_project_overview.md`
  - `docs/superpowers/specs/2026-05-25-ankle-navigation-error-aware-digital-twin-experiment-guide.md`
- 当前会话未收到显式提交指令，因此保留为未 commit 状态，不再把该项作为实施阻塞。

## Task 8: Final Verification

**Files:**
- Modify: none

- [x] **Step 1: 运行数字孪生核心测试集合**

Run:

```bash
cmake --build build_x64_v142 --config Release --target navigation_digital_twin_state_builder_test navigation_runtime_coordinator_contract_test navigation_vtk_bridge_test navigation_evaluation_service_test navigation_evaluation_summary_formatter_test navigation_workspace_application_service_test ankle_navigation_workflow_contract_test medicalpro
ctest --test-dir build_x64_v142 -C Release -R "^(navigation_digital_twin_state_builder_test|navigation_runtime_coordinator_contract_test|navigation_vtk_bridge_test|navigation_evaluation_service_test|navigation_evaluation_summary_formatter_test|navigation_workspace_application_service_test|ankle_navigation_workflow_contract_test)$" --output-on-failure
```

Expected:

- 所有数字孪生新增测试 PASS
- `medicalpro` Release 构建成功
- 导航页 contract 仍保持真实工作流表达

Actual result:

- `navigation_digital_twin_state_builder_test`
- `navigation_runtime_coordinator_contract_test`
- `navigation_vtk_bridge_test`
- `navigation_evaluation_service_test`
- `navigation_evaluation_summary_formatter_test`
- `navigation_workspace_application_service_test`
- `ankle_navigation_workflow_contract_test`
- `7/7 PASS`
- `medicalpro` Release 构建成功

- [x] **Step 2: 运行已有实时导航相关回归集合**

Run:

```bash
ctest --test-dir build_x64_v142 -C Release -R "^(navigation_pose_stream_test|navigation_transform_graph_test|navigation_runtime_state_test|navigation_confidence_evaluator_test|navigation_gate_strategy_test)$" --output-on-failure
```

Expected: PASS，说明误差感知 twin 没有破坏现有位姿链与门禁链。

Actual result:

- 先补构建缺失目标：
  - `navigation_pose_stream_test`
  - `navigation_transform_graph_test`
  - `navigation_gate_strategy_test`
  - `navigation_confidence_evaluator_test`
- 回归集合最终 `5/5 PASS`

- [ ] **Step 3: 手工 smoke-check 导航页 HUD（仍待人工界面确认）**

Run:

```bash
build_x64_v142\\Release\\medicalpro.exe
```

Current status:

- 已完成非交互启动烟测：`medicalpro.exe` 启动后 8 秒内持续运行，说明程序可正常拉起。
- 仍需在真实界面中人工进入导航页，目视确认 HUD 文本、target region overlay 和 risk tone 变化是否符合预期。

Manual checklist:

- 导航页顶部能看到 `数字孪生 HUD`
- HUD 至少显示：
  - `骨 STL`
  - `器械 STL`
  - `准入`
  - `可信度`
  - `主风险`
  - `目标距离`
- 当 target region 可用时，3D 视图中出现 target region overlay
- twin 风险升高时，overlay tone 从 `ok` 变成 `warning` 或 `danger`
- twin 建议与 evaluation summary 中的 `re_register_recommended` 一致

- [x] **Step 4: 记录最终验证 checkpoint 状态（未执行 commit）**

```bash
git add docs/superpowers/specs/2026-05-23-ankle-navigation-error-aware-digital-twin-design.md docs/superpowers/plans/2026-05-25-ankle-navigation-error-aware-digital-twin-implementation-plan.md
git commit -m "docs: add implementation plan for error-aware digital twin"
```

Status:

- 自动化验证已完成，核心测试和回归测试均已有通过记录。
- 非交互启动烟测已完成，程序可正常启动。
- 当前会话未执行 `git commit`，以避免在没有明确要求时提交工作树。

## Self-Review

- Spec coverage: 已覆盖误差感知数字孪生的四个核心部分：状态类型、目标区局部关系、运行时 twin 刷新、VTK/HUD/评估导出与实验文档。
- Existing-code fit: 计划围绕现有 `NavigationRuntimeCoordinator`、`NavigationRuntimeState`、`NavigationVtkBridge`、`Navigation3DViewWidget`、`NavigationEvaluationService` 和 `NavigationPage` 做增量扩展，没有另起新系统。
- Placeholder scan: 计划中没有 `TBD`、`TODO`、`implement later`、`类似 Task N` 之类的占位语。
- Type consistency: 全文统一使用 `DigitalTwinTargetRegionDefinition`、`TargetRegionNavigationStatus`、`DigitalTwinRiskReport`、`DigitalTwinState` 作为类型名，统一使用 `twin_confidence_score`、`local_risk_score`、`target_region_distance_mm`、`dominant_risk_source`、`re_register_recommended` 作为指标名。

## Current Execution Status

- 该实施计划对应的代码、测试、实验文档和工作区摘要接入已经落地。
- 自动化验证已完成：
  - 数字孪生核心集合 `7/7 PASS`
  - 实时导航相关回归集合 `5/5 PASS`
  - `medicalpro.exe` 非交互启动烟测通过
- 当前唯一未闭环项是人工导航页 HUD 目视验收。
- 如果后续需要完全收口，可在真实导航页按本计划的 Manual checklist 做一轮人工确认，再决定是否提交 commit。
