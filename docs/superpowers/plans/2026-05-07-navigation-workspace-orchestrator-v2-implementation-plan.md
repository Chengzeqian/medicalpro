# Navigation Workspace Orchestrator V2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把导航页升级为病例驱动的五环节工作区编排器，支持多骨、多器械、分骨配准与单一 3D 虚拟空间导航。

**Architecture:** 这一轮实现沿用当前 `NavigationWorkspaceApplicationService + SnapshotStore + UiBinder + WorkflowCoordinator` 骨架，但把工作区状态模型升级为 V2：活动骨集合、多器械几何绑定和逐个标定、分骨配准结果集、融合导航空间、单窗口导航态、分骨评估摘要。页面仍以 `NavigationPage` 为入口，但业务真源全部收敛到工作区聚合层。

**Tech Stack:** C++20, Qt Core/Gui/Widgets, existing `Framework/Navigation` persistence, existing `NewPagesLib`, QtTest, VTK view hosting

---

### Task 1: 升级工作区状态模型与快照格式

**Files:**
- Modify: `UI/NewPages/Navigation/navigation_workspace_types.h`
- Modify: `UI/NewPages/Navigation/navigation_workspace_snapshot_store.h`
- Modify: `UI/NewPages/Navigation/navigation_workspace_snapshot_store.cpp`
- Modify: `tests/unit/NavigationWorkspaceSnapshotStoreTest.cpp`
- Modify: `tests/unit/AnkleNavigationWorkflowContractTest.cpp`

- [ ] **Step 1: 先写失败测试，锁定多骨、多器械和分骨配准结果快照结构**

```cpp
void NavigationWorkspaceSnapshotStoreTest::store_round_trips_multi_bone_multi_instrument_workspace_snapshot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    NavigationWorkspaceSnapshotStore store(tempDir.path());
    NavigationWorkspaceSnapshot snapshot;
    snapshot.caseId = QStringLiteral("ankle-case-v2-001");
    snapshot.assetState.boundBoneAssets = { QStringLiteral("bone:tibia"), QStringLiteral("bone:talus") };
    snapshot.assetState.activeBoneAssets = snapshot.assetState.boundBoneAssets;
    snapshot.preparationState.instrumentCalibrationStates = {
        NavigationInstrumentCalibrationState { QStringLiteral("instrument:probe-main"), QStringLiteral("geometry:probe-main"), true, 12, 12, true, 0.41 },
        NavigationInstrumentCalibrationState { QStringLiteral("instrument:guide-default"), QStringLiteral("geometry:guide-default"), true, 10, 10, true, 0.52 }
    };
    snapshot.registrationState.perBoneResults = {
        NavigationPerBoneRegistrationState { QStringLiteral("bone:tibia"), QStringLiteral("distal"), 6, true, 0.71, 1.03, 0.92 },
        NavigationPerBoneRegistrationState { QStringLiteral("bone:talus"), QStringLiteral("dome"), 6, true, 0.68, 0.97, 0.95 }
    };
    snapshot.registrationState.fusedNavigationSpaceReady = true;

    QVERIFY(store.persistSnapshot(snapshot));

    const NavigationWorkspaceSnapshot restored = store.loadSnapshot();
    QCOMPARE(restored.assetState.boundBoneAssets.size(), 2);
    QCOMPARE(restored.preparationState.instrumentCalibrationStates.size(), 2);
    QCOMPARE(restored.registrationState.perBoneResults.size(), 2);
    QCOMPARE(restored.registrationState.fusedNavigationSpaceReady, true);
}
```

- [ ] **Step 2: 运行测试确认当前类型不支持**

Run: `ctest --test-dir build_x64_v142 -C Release -R "navigation_workspace_snapshot_store_test|ankle_navigation_workflow_contract_test" --output-on-failure`
Expected: FAIL，提示 `boundBoneAssets`、`preparationState` 或 `perBoneResults` 不存在

- [ ] **Step 3: 在 `navigation_workspace_types.h` 中定义 V2 状态结构**

```cpp
struct NavigationInstrumentCalibrationState
{
    QString instrumentId;
    QString geometryId;
    bool started = false;
    int collectedPoints = 0;
    int requiredPoints = 0;
    bool completed = false;
    double accuracy = 0.0;
    QDateTime completedAt;
};

struct NavigationPerBoneRegistrationState
{
    QString boneAssetId;
    QString boneRegionId;
    int pointCount = 0;
    bool success = false;
    double fre = 0.0;
    double targetTre = 0.0;
    double coverageScore = 0.0;
    QString transformMatrix;
    QDateTime completedAt;
};
```

```cpp
struct NavigationWorkspacePreparationState
{
    QList<NavigationInstrumentCalibrationState> instrumentCalibrationStates;
    bool allRequiredInstrumentsCalibrated = false;
    QStringList blockingReasons;
};

struct NavigationWorkspaceRegistrationState
{
    QList<NavigationPerBoneRegistrationState> perBoneResults;
    bool fusedNavigationSpaceReady = false;
    QString fusedNavigationSpacePath;
    double fusedCoverageScore = 0.0;
    QStringList fusionBlockingReasons;
    QDateTime completedAt;
};
```

- [ ] **Step 4: 实现新的 JSON 读写**

```cpp
QJsonArray toJsonArray(const QList<NavigationInstrumentCalibrationState>& states)
{
    QJsonArray array;
    for (const NavigationInstrumentCalibrationState& state : states) {
        QJsonObject object;
        object.insert(QStringLiteral("instrument_id"), state.instrumentId);
        object.insert(QStringLiteral("geometry_id"), state.geometryId);
        object.insert(QStringLiteral("started"), state.started);
        object.insert(QStringLiteral("collected_points"), state.collectedPoints);
        object.insert(QStringLiteral("required_points"), state.requiredPoints);
        object.insert(QStringLiteral("completed"), state.completed);
        object.insert(QStringLiteral("accuracy"), state.accuracy);
        array.append(object);
    }
    return array;
}
```

```cpp
snapshotObject.insert(QStringLiteral("preparation_state"), toJson(snapshot.preparationState));
snapshotObject.insert(QStringLiteral("registration_state"), toJson(snapshot.registrationState));
```

- [ ] **Step 5: 运行快照与契约测试**

Run: `ctest --test-dir build_x64_v142 -C Release -R "navigation_workspace_snapshot_store_test|ankle_navigation_workflow_contract_test" --output-on-failure`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add UI/NewPages/Navigation/navigation_workspace_types.h UI/NewPages/Navigation/navigation_workspace_snapshot_store.h UI/NewPages/Navigation/navigation_workspace_snapshot_store.cpp tests/unit/NavigationWorkspaceSnapshotStoreTest.cpp tests/unit/AnkleNavigationWorkflowContractTest.cpp
git commit -m "feat: upgrade navigation workspace snapshot to v2 state model"
```

---

### Task 2: 升级工作区聚合服务与阶段门禁

**Files:**
- Modify: `UI/NewPages/Navigation/navigation_workspace_application_service.h`
- Modify: `UI/NewPages/Navigation/navigation_workspace_application_service.cpp`
- Modify: `UI/NewPages/Navigation/navigation_runtime_state.h`
- Modify: `UI/NewPages/Navigation/navigation_runtime_state.cpp`
- Modify: `Framework/Navigation/navigation_evaluation_service.h`
- Modify: `Framework/Navigation/navigation_evaluation_service.cpp`
- Modify: `tests/unit/NavigationWorkspaceApplicationServiceTest.cpp`
- Modify: `tests/unit/NavigationRuntimeStateTest.cpp`

- [ ] **Step 1: 写失败测试，要求聚合服务同时消费病例绑定、分骨配准和融合结果**

```cpp
void NavigationWorkspaceApplicationServiceTest::service_builds_v2_snapshot_from_case_package_and_per_bone_results()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    seedCaseManifestAndBindings(tempRoot.path(), QStringLiteral("ankle-case-v2-002"));
    seedPerBoneRegistrationResult(tempRoot.path(), QStringLiteral("ankle-case-v2-002"));
    seedFusedNavigationSpace(tempRoot.path(), QStringLiteral("ankle-case-v2-002"));

    NavigationRuntimeState runtimeState;
    runtimeState.setCaseContext(QStringLiteral("ankle-case-v2-002"), QStringLiteral("tracking-002"), QStringLiteral("instrument:probe-main"));
    runtimeState.setTrackedInstrumentVisible(QStringLiteral("instrument:probe-main"), true);

    NavigationWorkspaceApplicationService service(tempRoot.path(), &runtimeState);
    const NavigationWorkspaceSnapshot snapshot =
        service.loadWorkspace(QStringLiteral("ankle-case-v2-002"), QStringLiteral("patient-002"), QStringLiteral("Patient 002"));

    QCOMPARE(snapshot.assetState.boundBoneAssets.size(), 2);
    QCOMPARE(snapshot.preparationState.allRequiredInstrumentsCalibrated, true);
    QCOMPARE(snapshot.registrationState.perBoneResults.size(), 2);
    QCOMPARE(snapshot.registrationState.fusedNavigationSpaceReady, true);
}
```

- [ ] **Step 2: 运行测试确认当前聚合能力不足**

Run: `ctest --test-dir build_x64_v142 -C Release -R "navigation_workspace_application_service_test|navigation_runtime_state_test" --output-on-failure`
Expected: FAIL，提示服务仍基于旧 `assetState/calibrationState/registrationState` 模型

- [ ] **Step 3: 扩展运行时状态和服务接口**

```cpp
class NavigationRuntimeState
{
public:
    void setTrackedInstrumentVisible(const QString& instrumentId, bool visible);
    bool isTrackedInstrumentVisible(const QString& instrumentId) const;
    void setActiveInstrumentPoseSummary(const QString& instrumentId, const QString& summary);
    QString activeInstrumentPoseSummary(const QString& instrumentId) const;
};
```

```cpp
class NavigationWorkspaceApplicationService
{
public:
    void recordPreparationState(const NavigationWorkspacePreparationState& preparationState);
    void recordPlanningState(const NavigationWorkspacePlanningState& planningState);
    void recordRegistrationState(const NavigationWorkspaceRegistrationState& registrationState);
    void recordEvaluationState(const NavigationWorkspaceEvaluationState& evaluationState);
};
```

- [ ] **Step 4: 在实现中改成 V2 聚合与门禁计算**

```cpp
NavigationStageGate NavigationWorkspaceApplicationService::evaluateStageGate(AnkleWorkflowStage stage)
{
    NavigationStageGate gate;
    gate.requestedStage = stage;
    gate.lastComputedAt = QDateTime::currentDateTimeUtc();

    if (stage == AnkleWorkflowStage::Planning) {
        gate.allowed = !m_snapshot.assetState.activeBoneAssets.isEmpty();
        gate.reasonCode = gate.allowed ? QStringLiteral("ok") : QStringLiteral("active_bones_missing");
        gate.reasonText = gate.allowed ? QStringLiteral("规划条件满足") : QStringLiteral("尚未选择参与规划的骨骼");
        return gate;
    }

    if (stage == AnkleWorkflowStage::Registration) {
        gate.allowed = m_snapshot.planningState.completed
            && m_snapshot.preparationState.allRequiredInstrumentsCalibrated;
        gate.reasonCode = gate.allowed ? QStringLiteral("ok") : QStringLiteral("planning_or_calibration_missing");
        gate.reasonText = gate.allowed ? QStringLiteral("配准条件满足") : QStringLiteral("规划结果或器械标定未完成");
        return gate;
    }

    if (stage == AnkleWorkflowStage::Navigation) {
        gate.allowed = m_snapshot.registrationState.fusedNavigationSpaceReady
            && m_snapshot.navigationState.trackerConnected
            && m_snapshot.navigationState.toolVisible
            && m_snapshot.navigationState.allowNavigation;
        gate.reasonCode = gate.allowed ? QStringLiteral("ok") : QStringLiteral("fused_space_or_tracking_missing");
        gate.reasonText = gate.allowed ? QStringLiteral("导航条件满足") : QStringLiteral("融合导航空间或实时跟踪条件未满足");
        return gate;
    }

    return gate;
}
```

- [ ] **Step 5: 运行聚合和运行态测试**

Run: `ctest --test-dir build_x64_v142 -C Release -R "navigation_workspace_application_service_test|navigation_runtime_state_test|navigation_workspace_snapshot_store_test" --output-on-failure`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add UI/NewPages/Navigation/navigation_workspace_application_service.h UI/NewPages/Navigation/navigation_workspace_application_service.cpp UI/NewPages/Navigation/navigation_runtime_state.h UI/NewPages/Navigation/navigation_runtime_state.cpp Framework/Navigation/navigation_evaluation_service.h Framework/Navigation/navigation_evaluation_service.cpp tests/unit/NavigationWorkspaceApplicationServiceTest.cpp tests/unit/NavigationRuntimeStateTest.cpp
git commit -m "feat: aggregate v2 workspace state and stage gates"
```

---

### Task 3: 改造准备/规划/配准控制器，落分骨结果与融合导航空间

**Files:**
- Modify: `UI/NewPages/Navigation/preparation_planning_controller.h`
- Modify: `UI/NewPages/Navigation/preparation_planning_controller.cpp`
- Modify: `UI/NewPages/Navigation/registration_controller.h`
- Modify: `UI/NewPages/Navigation/registration_controller.cpp`
- Modify: `UI/NewPages/Navigation/navigation_workflow_coordinator.h`
- Modify: `UI/NewPages/Navigation/navigation_workflow_coordinator.cpp`
- Modify: `tests/unit/NavigationWorkflowCoordinatorTest.cpp`
- Modify: `tests/unit/AnkleNavigationWorkflowContractTest.cpp`

- [ ] **Step 1: 写失败测试，要求控制器回写 V2 准备态和分骨配准结果**

```cpp
void NavigationWorkflowCoordinatorTest::coordinator_records_per_bone_registration_and_fused_space_into_workspace_service()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    NavigationWorkflowContext context;
    context.setCaseIdentity(QStringLiteral("ankle-case-v2-003"), 3, QStringLiteral("Patient 003"));

    NavigationRuntimeState runtimeState;
    NavigationWorkspaceApplicationService workspaceApplicationService(tempRoot.path(), &runtimeState);
    workspaceApplicationService.loadWorkspace(QStringLiteral("ankle-case-v2-003"), QStringLiteral("patient-003"), QStringLiteral("Patient 003"));

    NavigationWorkflowCoordinator coordinator(
        &context,
        &preparationPlanningController,
        &registrationController,
        &evaluationController,
        &runtimeCoordinator,
        [](AnkleWorkflowStage) {},
        &workspaceApplicationService);

    coordinator.handleComputeRegistration();

    const NavigationWorkspaceSnapshot snapshot = workspaceApplicationService.currentSnapshot();
    QVERIFY(snapshot.registrationState.perBoneResults.size() >= 1);
    QVERIFY(snapshot.registrationState.fusedNavigationSpaceReady);
}
```

- [ ] **Step 2: 运行协调器测试确认失败**

Run: `ctest --test-dir build_x64_v142 -C Release -R "navigation_workflow_coordinator_test|ankle_navigation_workflow_contract_test" --output-on-failure`
Expected: FAIL，提示协调器仍只处理旧单结果配准模型

- [ ] **Step 3: 在准备/规划控制器里显式输出活动骨和器械标定摘要**

```cpp
NavigationWorkspacePreparationState PreparationPlanningController::buildPreparationState(
    const QStringList& activeBones,
    const QList<NavigationInstrumentCalibrationState>& calibrationStates) const
{
    NavigationWorkspacePreparationState state;
    state.instrumentCalibrationStates = calibrationStates;
    state.allRequiredInstrumentsCalibrated = std::all_of(
        calibrationStates.cbegin(),
        calibrationStates.cend(),
        [](const NavigationInstrumentCalibrationState& item) { return item.completed; });
    if (activeBones.isEmpty()) {
        state.blockingReasons.append(QStringLiteral("未选择活动骨集合"));
    }
    return state;
}
```

- [ ] **Step 4: 在配准控制器里构建分骨结果和融合状态**

```cpp
NavigationWorkspaceRegistrationState RegistrationController::buildRegistrationWorkspaceState(
    const QList<PointRegistrationResult>& perBoneResults,
    const QString& fusedSpacePath,
    double fusedCoverageScore) const
{
    NavigationWorkspaceRegistrationState state;
    for (const PointRegistrationResult& result : perBoneResults) {
        state.perBoneResults.append(NavigationPerBoneRegistrationState {
            result.metadata.value(QStringLiteral("bone_asset_id")).toString(),
            result.metadata.value(QStringLiteral("bone_region_id")).toString(),
            result.collectedPoints.size(),
            result.success,
            result.fiducialRegistrationError,
            result.targetRegistrationError,
            result.coverageScore,
            serializeMatrix(result.transformMatrix),
            QDateTime::currentDateTimeUtc()
        });
    }
    state.fusedNavigationSpaceReady = !fusedSpacePath.isEmpty() && !state.perBoneResults.isEmpty();
    state.fusedNavigationSpacePath = fusedSpacePath;
    state.fusedCoverageScore = fusedCoverageScore;
    return state;
}
```

- [ ] **Step 5: 让工作流协调器把控制器产物全部写回工作区服务**

```cpp
void NavigationWorkflowCoordinator::handleComputeRegistration() const
{
    if (!m_workspaceApplicationService || !tryEnterStage(AnkleWorkflowStage::Registration)) {
        return;
    }

    const NavigationWorkspaceRegistrationState registrationState =
        m_registrationController->computePerBoneRegistration();
    m_workspaceApplicationService->recordRegistrationState(registrationState);
    m_workspaceApplicationService->persistSnapshot();
}
```

- [ ] **Step 6: 运行协调器与契约测试**

Run: `ctest --test-dir build_x64_v142 -C Release -R "navigation_workflow_coordinator_test|ankle_navigation_workflow_contract_test|navigation_workspace_application_service_test" --output-on-failure`
Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add UI/NewPages/Navigation/preparation_planning_controller.h UI/NewPages/Navigation/preparation_planning_controller.cpp UI/NewPages/Navigation/registration_controller.h UI/NewPages/Navigation/registration_controller.cpp UI/NewPages/Navigation/navigation_workflow_coordinator.h UI/NewPages/Navigation/navigation_workflow_coordinator.cpp tests/unit/NavigationWorkflowCoordinatorTest.cpp tests/unit/AnkleNavigationWorkflowContractTest.cpp
git commit -m "feat: record preparation and per-bone registration states"
```

---

### Task 4: 把导航页收口为单一 3D 虚拟空间并交给 binder 驱动

**Files:**
- Modify: `UI/NewPages/NavigationPage.h`
- Modify: `UI/NewPages/NavigationPage.cpp`
- Modify: `UI/NewPages/Navigation/navigation_workspace_ui_binder.h`
- Modify: `UI/NewPages/Navigation/navigation_workspace_ui_binder.cpp`
- Modify: `UI/NewPages/Navigation/navigation_vtk_bridge.h`
- Modify: `UI/NewPages/Navigation/navigation_vtk_bridge.cpp`
- Modify: `tests/unit/AnkleNavigationWorkflowContractTest.cpp`

- [ ] **Step 1: 写失败测试，要求导航页不再依赖四视图作为主导航视图**

```cpp
void AnkleNavigationWorkflowContractTest::navigation_page_uses_single_virtual_space_for_navigation_stage()
{
    QFile source(QStringLiteral("UI/NewPages/NavigationPage.cpp"));
    QVERIFY(source.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString body = QString::fromUtf8(source.readAll());

    QVERIFY(body.contains(QStringLiteral("singleVirtualNavigationSpace")));
    QVERIFY(!body.contains(QStringLiteral("embedFourViewWidget()")));
    QVERIFY(body.contains(QStringLiteral("骨骼 STL")));
}
```

- [ ] **Step 2: 运行契约测试确认当前仍保留旧依赖**

Run: `ctest --test-dir build_x64_v142 -C Release -R ankle_navigation_workflow_contract_test --output-on-failure`
Expected: FAIL，提示主导航视图仍围绕 four-view 或多个窗口路径组织

- [ ] **Step 3: 在 binder 中增加准备态、导航态和阻塞摘要映射**

```cpp
void NavigationWorkspaceUiBinder::applyPreparationSummary(
    const NavigationWorkspacePreparationState& state) const
{
    if (!m_bindings.calibrationStatusLabel) {
        return;
    }

    if (state.allRequiredInstrumentsCalibrated) {
        m_bindings.calibrationStatusLabel->setText(QStringLiteral("所有导航器械均已标定"));
        return;
    }

    m_bindings.calibrationStatusLabel->setText(
        state.blockingReasons.join(QStringLiteral("；")));
}
```

- [ ] **Step 4: 在 `NavigationPage` 中把导航阶段主视图收敛为单一 3D 容器**

```cpp
void NavigationPageNew::setupNavigationWorkspaceShell()
{
    m_navigationVtkHost = std::make_unique<EmbeddedVtkViewHost>(
        ui->fourViewLayout ? ui->fourViewLayout->parentWidget() : nullptr,
        ui->fourViewLayout,
        nullptr,
        EmbeddedVtkViewHostOptions {
            .hideExistingWidgets = true,
            .gridRow = 0,
            .gridColumn = 0,
            .gridRowSpan = 1,
            .gridColumnSpan = 1
        });

    ui->view3DLabel->setText(QStringLiteral("singleVirtualNavigationSpace"));
}
```

- [ ] **Step 5: 刷新运行态时只同步骨骼 STL、当前工具 STL 和位姿摘要**

```cpp
void NavigationPageNew::restoreNavigationSnapshotState()
{
    const NavigationWorkspaceSnapshot snapshot = m_workspaceApplicationService->currentSnapshot();
    m_workspaceUiBinder->applyNavigationConfidence(snapshot.navigationState, snapshot.stageGate);
    m_navigationVtkBridge->showSingleNavigationSpace(
        snapshot.assetState.activeBoneAssets,
        snapshot.navigationState.activeToolId,
        snapshot.navigationState.latestPoseSummary);
}
```

- [ ] **Step 6: 运行导航页契约测试**

Run: `ctest --test-dir build_x64_v142 -C Release -R "ankle_navigation_workflow_contract_test|navigation_workflow_coordinator_test" --output-on-failure`
Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add UI/NewPages/NavigationPage.h UI/NewPages/NavigationPage.cpp UI/NewPages/Navigation/navigation_workspace_ui_binder.h UI/NewPages/Navigation/navigation_workspace_ui_binder.cpp UI/NewPages/Navigation/navigation_vtk_bridge.h UI/NewPages/Navigation/navigation_vtk_bridge.cpp tests/unit/AnkleNavigationWorkflowContractTest.cpp
git commit -m "feat: switch navigation stage to single virtual 3d space"
```

---

### Task 5: 扩展评估摘要、恢复逻辑与最终验证

**Files:**
- Modify: `UI/NewPages/Navigation/navigation_evaluation_controller.h`
- Modify: `UI/NewPages/Navigation/navigation_evaluation_controller.cpp`
- Modify: `UI/NewPages/Navigation/navigation_evaluation_summary_formatter.h`
- Modify: `UI/NewPages/Navigation/navigation_evaluation_summary_formatter.cpp`
- Modify: `UI/NewPages/NavigationPage.cpp`
- Modify: `tests/unit/NavigationEvaluationSummaryFormatterTest.cpp`
- Modify: `tests/unit/NavigationWorkspaceApplicationServiceTest.cpp`
- Modify: `tests/unit/NavigationEvaluationServiceTest.cpp`

- [ ] **Step 1: 写失败测试，要求评估摘要包含分骨质量和导航摘要**

```cpp
void NavigationEvaluationSummaryFormatterTest::formatter_includes_per_bone_quality_and_navigation_process_summary()
{
    NavigationWorkspaceSnapshot snapshot;
    snapshot.caseId = QStringLiteral("ankle-case-v2-004");
    snapshot.registrationState.perBoneResults = {
        NavigationPerBoneRegistrationState { QStringLiteral("bone:tibia"), QStringLiteral("distal"), 6, true, 0.72, 1.01, 0.93 },
        NavigationPerBoneRegistrationState { QStringLiteral("bone:talus"), QStringLiteral("dome"), 6, true, 0.69, 0.98, 0.95 }
    };
    snapshot.evaluationState.navigationProcessSummary = QStringLiteral("完成 2 段骨配准后进入导航，导航持续 180 秒");

    const QString summary = buildNavigationEvaluationSummary(snapshot);
    QVERIFY(summary.contains(QStringLiteral("bone:tibia")));
    QVERIFY(summary.contains(QStringLiteral("180 秒")));
}
```

- [ ] **Step 2: 运行评估格式化测试确认失败**

Run: `ctest --test-dir build_x64_v142 -C Release -R "navigation_evaluation_service_test|navigation_workspace_application_service_test|NavigationEvaluationSummaryFormatterTest" --output-on-failure`
Expected: FAIL，提示评估摘要仍只输出旧的单结果信息

- [ ] **Step 3: 扩展评估状态与格式化器**

```cpp
struct NavigationWorkspaceEvaluationState
{
    QVariantMap errorMetrics;
    QStringList perBoneQualitySummary;
    QString navigationProcessSummary;
    bool reportReady = false;
    QStringList exportableArtifacts;
    QDateTime lastUpdatedAt;
};
```

```cpp
QString buildNavigationEvaluationSummary(const NavigationWorkspaceSnapshot& snapshot)
{
    QStringList lines;
    lines.append(QStringLiteral("病例：%1").arg(snapshot.caseId));
    for (const NavigationPerBoneRegistrationState& item : snapshot.registrationState.perBoneResults) {
        lines.append(QStringLiteral("%1/%2 FRE=%3 TRE=%4 Coverage=%5")
            .arg(item.boneAssetId)
            .arg(item.boneRegionId)
            .arg(item.fre, 0, 'f', 2)
            .arg(item.targetTre, 0, 'f', 2)
            .arg(item.coverageScore, 0, 'f', 2));
    }
    lines.append(snapshot.evaluationState.navigationProcessSummary);
    return lines.join(QStringLiteral("\n"));
}
```

- [ ] **Step 4: 调整恢复流程，优先恢复正式结果再覆盖 UI 快照**

```cpp
NavigationWorkspaceSnapshot NavigationWorkspaceApplicationService::loadWorkspace(
    const QString& caseId,
    const QString& patientId,
    const QString& patientName)
{
    m_snapshot = buildSnapshot(caseId, patientId, patientName);
    const NavigationWorkspaceSnapshot restoredSnapshot = restoreSnapshot(caseId);
    mergeUiRestoreFacts(restoredSnapshot);
    m_snapshot.stageGate = evaluateStageGate(m_snapshot.caseContext.currentStage);
    return m_snapshot;
}
```

- [ ] **Step 5: 运行完整导航相关测试集**

Run: `ctest --test-dir build_x64_v142 -C Release -R "navigation_workspace_snapshot_store_test|navigation_workspace_application_service_test|navigation_runtime_state_test|navigation_workflow_coordinator_test|ankle_navigation_workflow_contract_test|navigation_evaluation_service_test" --output-on-failure`
Expected: PASS

- [ ] **Step 6: 构建主程序并做最终 smoke**

Run: `cmake --build build_x64_v142 --config Release --target medicalpro`
Expected: BUILD SUCCESS

- [ ] **Step 7: Commit**

```bash
git add UI/NewPages/Navigation/navigation_evaluation_controller.h UI/NewPages/Navigation/navigation_evaluation_controller.cpp UI/NewPages/Navigation/navigation_evaluation_summary_formatter.h UI/NewPages/Navigation/navigation_evaluation_summary_formatter.cpp UI/NewPages/NavigationPage.cpp tests/unit/NavigationEvaluationSummaryFormatterTest.cpp tests/unit/NavigationWorkspaceApplicationServiceTest.cpp tests/unit/NavigationEvaluationServiceTest.cpp
git commit -m "feat: restore and evaluate navigation workspace v2"
```

---

## Self-Review

### Spec coverage

- V2 工作区状态模型：Task 1
- 多骨、多器械、逐个标定：Task 1, Task 2, Task 3
- 分骨配准与融合导航空间：Task 2, Task 3
- 单一 3D 虚拟空间导航页：Task 4
- 评估摘要、导出与恢复：Task 5
- 统一门禁：Task 2, Task 4

### Placeholder scan

- 所有任务包含明确文件、命令、代码片段和测试名
- 未使用 `TODO`、`TBD`、`later`

### Type consistency

- `NavigationWorkspacePreparationState`、`NavigationPerBoneRegistrationState`、`NavigationWorkspaceEvaluationState` 命名在所有任务中一致
- `fusedNavigationSpaceReady` 与融合导航空间路径命名一致
- 单一导航空间统一使用 `singleVirtualNavigationSpace` 作为契约标识

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-05-07-navigation-workspace-orchestrator-v2-implementation-plan.md`. Two execution options:

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

Which approach?
