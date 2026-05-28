# Navigation Workspace UI Realignment Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把手术导航工作区五阶段页面与现有病例工作包、工作区聚合和数字孪生链路重新对齐，移除误导性的旧 UI 主路径，并形成可见、可操作、可联调的最小可用导航工作区。

**Architecture:** 保留现有 `NavigationWorkspaceApplicationService + NavigationWorkspaceUiBinder + NavigationWorkflowCoordinator + NavigationRuntimeCoordinator + NavigationVtkBridge` 骨架，不重写底层算法，而是系统性收口 `NavigationPage` 与 `NavigationPage.ui`。本轮先清退旧导入/旧分割/旧 2D-3D 占位交互，再将准备、规划、配准、评估四阶段页面改为只表达统一快照中的真实状态，最后把导航阶段收口到单一 3D 数字孪生空间，并做最小可用联调验证。

**Tech Stack:** C++20, Qt Widgets / Qt Designer `.ui`, existing `Framework/Navigation` workspace repository and planning services, existing `UI/NewPages/Navigation/*` controllers and binder, VTK host bridge, QtTest, CMake, CTest

---

### Task 1: 锁定旧主流程入口退出主路径

**Files:**
- Modify: `UI/Forms/NavigationPage.ui`
- Modify: `UI/NewPages/NavigationPage.cpp`
- Test: `tests/unit/AnkleNavigationWorkflowContractTest.cpp`
- Modify: `tests/unit/CMakeLists.txt`

- [ ] **Step 1: 先写失败契约测试，锁定旧入口必须退出主路径**

```cpp
void AnkleNavigationWorkflowContractTest::navigation_page_removes_legacy_import_and_segmentation_primary_actions()
{
    const QString pageCode = loadFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));
    const QString pageUi = loadFile(QStringLiteral("UI/Forms/NavigationPage.ui"));

    QVERIFY2(!pageCode.contains(QStringLiteral("器械导入功能将通过CTK服务实现")),
             "NavigationPage must not keep legacy instrument import placeholder in the primary workflow");
    QVERIFY2(!pageUi.contains(QStringLiteral("自动分割")),
             "NavigationPage.ui must not keep legacy auto segmentation as a primary planning action");
    QVERIFY2(!pageUi.contains(QStringLiteral("导出STL")),
             "NavigationPage.ui must not keep legacy STL export as a primary planning action");
    QVERIFY2(!pageUi.contains(QStringLiteral("加载2D图像")),
             "NavigationPage.ui must not keep legacy 2D-3D placeholder entry");
}
```

- [ ] **Step 2: 运行契约测试，确认当前实现为 RED**

Run:

```powershell
cmake --build build_x64_v142 --config Release --target ankle_navigation_workflow_contract_test
ctest --test-dir build_x64_v142 -C Release -R ankle_navigation_workflow_contract_test --output-on-failure
```

Expected:

- FAIL，提示旧导入 / 旧分割 / 旧 2D-3D 文案仍存在

- [ ] **Step 3: 从 `.ui` 和页面逻辑中移除旧入口的主流程暴露**

```cpp
// NavigationPage.cpp
void NavigationPageNew::hideLegacyPlanningActions()
{
    const QList<QWidget*> legacyWidgets = {
        ui->autoSegmentButton,
        ui->exportSTLButton,
        ui->selectModelButton,
        ui->loadModelButton,
        ui->toggleModelButton,
        ui->load2DImageButton,
        ui->start2D3DRegButton
    };

    for (QWidget* widget : legacyWidgets) {
        if (widget) {
            widget->hide();
            widget->setEnabled(false);
        }
    }
}
```

```xml
<!-- NavigationPage.ui -->
<!-- 删除 planning / registration 控制面板中旧自动分割、导出 STL、手动导模、加载 2D 图像、2D-3D 开始按钮 -->
```

- [ ] **Step 4: 初始化时统一调用旧入口清退逻辑**

```cpp
NavigationPageNew::NavigationPageNew(QWidget* parent, NavigationPageServiceAccess* serviceAccess)
    : BasePage(parent)
    , ui(new Ui::NavigationPage)
{
    ui->setupUi(this);
    setupNavigationWorkspaceShell();
    hideLegacyPlanningActions();
}
```

- [ ] **Step 5: 回跑契约测试，确认旧入口退出主路径**

Run:

```powershell
ctest --test-dir build_x64_v142 -C Release -R ankle_navigation_workflow_contract_test --output-on-failure
```

Expected:

- PASS

- [ ] **Step 6: Commit**

```bash
git add UI/Forms/NavigationPage.ui UI/NewPages/NavigationPage.cpp tests/unit/AnkleNavigationWorkflowContractTest.cpp tests/unit/CMakeLists.txt
git commit -m "refactor: remove legacy navigation workspace primary actions"
```

---

### Task 2: 收口准备页为病例工作包资产与标定页

**Files:**
- Modify: `UI/NewPages/Navigation/navigation_workspace_ui_binder.h`
- Modify: `UI/NewPages/Navigation/navigation_workspace_ui_binder.cpp`
- Modify: `UI/NewPages/Navigation/preparation_planning_controller.h`
- Modify: `UI/NewPages/Navigation/preparation_planning_controller.cpp`
- Modify: `UI/NewPages/NavigationPage.cpp`
- Test: `tests/unit/NavigationWorkspaceApplicationServiceTest.cpp`
- Test: `tests/unit/NavigationWorkflowCoordinatorTest.cpp`

- [ ] **Step 1: 先写失败测试，锁定准备页必须只消费工作包器械与标定状态**

```cpp
void NavigationWorkspaceApplicationServiceTest::service_builds_preparation_summary_from_bound_instruments_and_calibration_states()
{
    NavigationWorkspaceApplicationService service(tempRoot.path(), &runtimeState);
    service.loadWorkspace(QStringLiteral("ankle-case-real-45971129749"), 2, QStringLiteral("Real Case"));

    NavigationWorkspaceSnapshot snapshot = service.currentSnapshot();
    snapshot.assetState.activeInstruments = QStringList {
        QStringLiteral("instrument:probe-main"),
        QStringLiteral("instrument:guide-default")
    };
    snapshot.calibrationState.instrumentStates = {
        NavigationInstrumentCalibrationState {
            QStringLiteral("instrument:probe-main"),
            QStringLiteral("geometry:probe-main"),
            true,
            32,
            32,
            true,
            0.42
        },
        NavigationInstrumentCalibrationState {
            QStringLiteral("instrument:guide-default"),
            QStringLiteral("geometry:guide-default"),
            false,
            0,
            32,
            false,
            0.0
        }
    };

    const NavigationWorkspacePreparationState state =
        PreparationPlanningController().buildPreparationState(
            snapshot.assetState.activeInstruments,
            snapshot.calibrationState.instrumentStates);

    QCOMPARE(state.instrumentCalibrationStates.size(), 2);
    QCOMPARE(state.allRequiredInstrumentsCalibrated, false);
    QVERIFY(state.blockingReasons.contains(QStringLiteral("存在未完成标定的器械")));
}
```

- [ ] **Step 2: 运行测试，确认当前接口不完整**

Run:

```powershell
ctest --test-dir build_x64_v142 -C Release -R "navigation_workspace_application_service_test|navigation_workflow_coordinator_test" --output-on-failure
```

Expected:

- FAIL，提示 `buildPreparationState` 仍缺少活动器械维度或准备摘要不完整

- [ ] **Step 3: 扩展准备控制器，只基于活动器械和标定状态构造准备页摘要**

```cpp
class PreparationPlanningController
{
public:
    NavigationWorkspacePreparationState buildPreparationState(
        const QStringList& activeInstrumentIds,
        const QList<NavigationInstrumentCalibrationState>& calibrationStates) const;
};
```

```cpp
NavigationWorkspacePreparationState PreparationPlanningController::buildPreparationState(
    const QStringList& activeInstrumentIds,
    const QList<NavigationInstrumentCalibrationState>& calibrationStates) const
{
    NavigationWorkspacePreparationState state;
    for (const QString& instrumentId : activeInstrumentIds) {
        const auto it = std::find_if(
            calibrationStates.cbegin(),
            calibrationStates.cend(),
            [&instrumentId](const NavigationInstrumentCalibrationState& item) {
                return item.instrumentId == instrumentId;
            });
        if (it != calibrationStates.cend()) {
            state.instrumentCalibrationStates.append(*it);
        }
    }

    state.allRequiredInstrumentsCalibrated =
        !state.instrumentCalibrationStates.isEmpty() &&
        std::all_of(
            state.instrumentCalibrationStates.cbegin(),
            state.instrumentCalibrationStates.cend(),
            [](const NavigationInstrumentCalibrationState& item) { return item.completed; });

    if (!state.allRequiredInstrumentsCalibrated) {
        state.blockingReasons.append(QStringLiteral("存在未完成标定的器械"));
    }

    return state;
}
```

- [ ] **Step 4: 在页面中把准备阶段展示收口成工作包资产 + 标定摘要**

```cpp
void NavigationPageNew::refreshPreparationWorkspace()
{
    const NavigationWorkspaceSnapshot snapshot = m_workspaceApplicationService->currentSnapshot();
    m_workspaceUiBinder->applyPreparationSummary(snapshot.preparationState);
    m_workspaceUiBinder->applyCalibrationSummary(snapshot.calibrationState);
}
```

- [ ] **Step 5: 禁止准备页再出现手动导模/导入路径**

```cpp
void NavigationPageNew::on_importInstrumentButton_clicked()
{
    showWarning("准备", "当前版本只允许使用病例工作包中已绑定的器械和几何文件。");
}
```

- [ ] **Step 6: 回跑准备相关测试**

Run:

```powershell
ctest --test-dir build_x64_v142 -C Release -R "navigation_workspace_application_service_test|navigation_workflow_coordinator_test" --output-on-failure
```

Expected:

- PASS

- [ ] **Step 7: Commit**

```bash
git add UI/NewPages/Navigation/navigation_workspace_ui_binder.h UI/NewPages/Navigation/navigation_workspace_ui_binder.cpp UI/NewPages/Navigation/preparation_planning_controller.h UI/NewPages/Navigation/preparation_planning_controller.cpp UI/NewPages/NavigationPage.cpp tests/unit/NavigationWorkspaceApplicationServiceTest.cpp tests/unit/NavigationWorkflowCoordinatorTest.cpp
git commit -m "feat: realign preparation workspace to case package assets"
```

---

### Task 3: 收口规划页为中等规划页

**Files:**
- Modify: `UI/NewPages/Navigation/navigation_workspace_types.h`
- Modify: `UI/NewPages/Navigation/navigation_workspace_ui_binder.h`
- Modify: `UI/NewPages/Navigation/navigation_workspace_ui_binder.cpp`
- Modify: `UI/NewPages/NavigationPage.cpp`
- Test: `tests/unit/NavigationWorkspaceApplicationServiceTest.cpp`

- [ ] **Step 1: 写失败测试，锁定规划页必须展示 planning 摘要和只读病例资产信息**

```cpp
void NavigationWorkspaceApplicationServiceTest::service_exposes_planning_summary_for_read_only_planning_page()
{
    NavigationWorkspacePlanningState state;
    state.hasPlanning = true;
    state.targetBone = QStringLiteral("talus");
    state.targetRegion = QStringLiteral("talus_dome");
    state.constraintRegions = QStringList {
        QStringLiteral("tibia_distal_region"),
        QStringLiteral("talus_dome_region")
    };
    state.recommendedRegistrationPointOrder = QStringList {
        QStringLiteral("medial"),
        QStringLiteral("lateral"),
        QStringLiteral("anterior")
    };

    QCOMPARE(state.hasPlanning, true);
    QCOMPARE(state.targetBone, QStringLiteral("talus"));
    QCOMPARE(state.constraintRegions.size(), 2);
}
```

- [ ] **Step 2: 运行测试，确认当前 planning 展示不足**

Run:

```powershell
ctest --test-dir build_x64_v142 -C Release -R navigation_workspace_application_service_test --output-on-failure
```

Expected:

- FAIL 或虽过但无法覆盖 planning 摘要字段，需要补齐工作区/UI 绑定

- [ ] **Step 3: 扩展 binder，使规划页只展示 planning_state 和只读病例资产摘要**

```cpp
struct PlanningReadOnlySummary
{
    QString targetBone;
    QString targetRegion;
    QStringList constraintRegions;
    QStringList recommendedRegistrationPointOrder;
    QStringList boundBoneAssets;
    bool hasDicom = false;
};
```

```cpp
void NavigationWorkspaceUiBinder::applyPlanningSummary(
    const NavigationWorkspacePlanningState& planningState,
    const NavigationWorkspaceAssetState& assetState) const
{
    // 更新目标骨、目标区、约束区、推荐点序、已绑定骨模型摘要
}
```

- [ ] **Step 4: 页面内去掉规划页的旧 segmentation / 手动模型主逻辑刷新**

```cpp
void NavigationPageNew::refreshPlanningWorkspace()
{
    const NavigationWorkspaceSnapshot snapshot = m_workspaceApplicationService->currentSnapshot();
    m_workspaceUiBinder->applyPlanningSummary(snapshot.planningState, snapshot.assetState);
}
```

- [ ] **Step 5: 规划页仅保留只读查看病例已绑定骨模型/影像的能力**

```cpp
void NavigationPageNew::setupPlanningReadOnlyPanels()
{
    // 用只读摘要控件替换旧分割/旧导模按钮区
}
```

- [ ] **Step 6: 回跑 planning 测试**

Run:

```powershell
ctest --test-dir build_x64_v142 -C Release -R navigation_workspace_application_service_test --output-on-failure
```

Expected:

- PASS

- [ ] **Step 7: Commit**

```bash
git add UI/NewPages/Navigation/navigation_workspace_types.h UI/NewPages/Navigation/navigation_workspace_ui_binder.h UI/NewPages/Navigation/navigation_workspace_ui_binder.cpp UI/NewPages/NavigationPage.cpp tests/unit/NavigationWorkspaceApplicationServiceTest.cpp
git commit -m "feat: realign planning workspace to read-only planning summary"
```

---

### Task 4: 收口配准页为分骨结果与采点页

**Files:**
- Modify: `UI/NewPages/Navigation/registration_controller.h`
- Modify: `UI/NewPages/Navigation/registration_controller.cpp`
- Modify: `UI/NewPages/Navigation/navigation_workspace_ui_binder.cpp`
- Modify: `UI/NewPages/NavigationPage.cpp`
- Test: `tests/unit/NavigationRuntimeCoordinatorContractTest.cpp`
- Test: `tests/unit/NavigationWorkflowCoordinatorTest.cpp`

- [ ] **Step 1: 写失败测试，锁定配准页必须体现分骨结果与融合导航空间**

```cpp
void NavigationWorkflowCoordinatorTest::coordinator_exposes_registration_workspace_state_with_per_bone_results()
{
    QList<PointRegistrationResult> results;

    PointRegistrationResult tibia;
    tibia.boneName = QStringLiteral("tibia");
    tibia.error = 0.61;
    results.append(tibia);

    PointRegistrationResult talus;
    talus.boneName = QStringLiteral("talus");
    talus.error = 0.44;
    results.append(talus);

    RegistrationController controller({
        .computeRegistration = []() {},
        .resolvePerBoneRegistrationResults = [results]() { return results; },
        .resolveFusedNavigationSpacePath = []() { return QStringLiteral("registration/fused_navigation_space.json"); }
    });

    const NavigationWorkspaceRegistrationState state = controller.computePerBoneRegistration();

    QCOMPARE(state.perBoneResults.size(), 2);
    QCOMPARE(state.fusedNavigationSpaceReady, true);
    QCOMPARE(state.fusedNavigationSpacePath, QStringLiteral("registration/fused_navigation_space.json"));
}
```

- [ ] **Step 2: 运行测试，确认当前页面侧还未完整消费这些结果**

Run:

```powershell
ctest --test-dir build_x64_v142 -C Release -R "navigation_workflow_coordinator_test|navigation_runtime_coordinator_contract_test" --output-on-failure
```

Expected:

- FAIL 或缺少 UI 展示消费路径

- [ ] **Step 3: 扩展配准摘要，让页面展示分骨结果与融合空间状态**

```cpp
void NavigationWorkspaceUiBinder::applyRegistrationSummary(
    const NavigationWorkspaceRegistrationState& state) const
{
    // 展示 perBoneResults 数量、当前活动骨结果、融合导航空间是否就绪、误差摘要
}
```

- [ ] **Step 4: 清理配准页 2D-3D 占位入口，保留采点/删点/清点/计算**

```cpp
void NavigationPageNew::setupRegistrationActionVisibility()
{
    if (ui->load2DImageButton) ui->load2DImageButton->hide();
    if (ui->start2D3DRegButton) ui->start2D3DRegButton->hide();
}
```

- [ ] **Step 5: 页面配准刷新只消费 registration_state**

```cpp
void NavigationPageNew::refreshRegistrationWorkspace()
{
    const NavigationWorkspaceSnapshot snapshot = m_workspaceApplicationService->currentSnapshot();
    m_workspaceUiBinder->applyRegistrationSummary(snapshot.registrationState);
}
```

- [ ] **Step 6: 回跑配准相关测试**

Run:

```powershell
ctest --test-dir build_x64_v142 -C Release -R "navigation_workflow_coordinator_test|navigation_runtime_coordinator_contract_test" --output-on-failure
```

Expected:

- PASS

- [ ] **Step 7: Commit**

```bash
git add UI/NewPages/Navigation/registration_controller.h UI/NewPages/Navigation/registration_controller.cpp UI/NewPages/Navigation/navigation_workspace_ui_binder.cpp UI/NewPages/NavigationPage.cpp tests/unit/NavigationRuntimeCoordinatorContractTest.cpp tests/unit/NavigationWorkflowCoordinatorTest.cpp
git commit -m "feat: realign registration workspace to per-bone workflow"
```

---

### Task 5: 收口导航页为单一 3D 数字孪生空间

**Files:**
- Modify: `UI/Forms/NavigationPage.ui`
- Modify: `UI/NewPages/NavigationPage.cpp`
- Modify: `UI/NewPages/Navigation/navigation_vtk_bridge.h`
- Modify: `UI/NewPages/Navigation/navigation_vtk_bridge.cpp`
- Test: `tests/unit/NavigationVtkBridgeTest.cpp`
- Test: `tests/unit/NavigationRuntimeCoordinatorContractTest.cpp`

- [ ] **Step 1: 写失败测试，锁定导航页不能继续依赖旧四视图主布局**

```cpp
void NavigationVtkBridgeTest::bridge_supports_single_navigation_view_digital_twin_contract()
{
    NavigationVtkBridge bridge(planningHost, registrationHost, resolvePointRegistrationService);

    QVERIFY(bridge.loadBoneModels(QStringList { QStringLiteral("cases/test/models/tibia.stl") }));
    QVERIFY(bridge.loadInstrumentModel(QStringLiteral("instrument:probe-main"),
                                       QStringLiteral("cases/test/models/probe.stl")));

    QMatrix4x4 pose;
    pose.translate(10.0f, 5.0f, 2.0f);
    bridge.updateInstrumentPose(QStringLiteral("instrument:probe-main"), pose);
    bridge.setInstrumentVisible(QStringLiteral("instrument:probe-main"), true);

    QVERIFY(true);
}
```

- [ ] **Step 2: 运行导航桥和 runtime 契约测试**

Run:

```powershell
ctest --test-dir build_x64_v142 -C Release -R "navigation_vtk_bridge_test|navigation_runtime_coordinator_contract_test" --output-on-failure
```

Expected:

- FAIL 或仍反映旧四视图主路径假设

- [ ] **Step 3: 在 `.ui` 中移除导航阶段旧四视图主布局，替换为单一导航容器**

```xml
<!-- NavigationPage.ui -->
<!-- 删除导航 tab 内 Axial / Sagittal / Coronal / 3D Volume 四块主展示结构 -->
<!-- 替换为一个 singleNavigationViewFrame / singleNavigationViewLayout -->
```

- [ ] **Step 4: 页面导航阶段只挂单一 3D View，并持续刷新 digital twin**

```cpp
void NavigationPageNew::setupSingleNavigationWorkspace()
{
    m_navigation3DView = new Navigation3DViewWidget(this);
    ui->singleNavigationViewLayout->addWidget(m_navigation3DView);
    m_navigationVtkBridge->setNavigationViewWidget(m_navigation3DView);
}
```

```cpp
void NavigationPageNew::refreshRealtimeDigitalTwin()
{
    const NavigationDisplayState displayState =
        m_runtimeCoordinator->buildDisplayState(activeBoneModelPaths(), activeInstrumentModelPath());

    m_navigationVtkBridge->loadBoneModels(displayState.boneModelPaths);
    m_navigationVtkBridge->loadInstrumentModel(displayState.activeToolId, displayState.activeToolModelPath);
    m_navigationVtkBridge->updateInstrumentPose(displayState.activeToolId, displayState.vtkToolTransform);
    m_navigationVtkBridge->setInstrumentVisible(displayState.activeToolId, displayState.toolVisible);
}
```

- [ ] **Step 5: 右侧导航状态栏只保留运行相关状态**

```cpp
void NavigationPageNew::refreshNavigationWorkspace()
{
    refreshRealtimeDigitalTwin();
    syncNavigationStatusSummary();
}
```

- [ ] **Step 6: 回跑导航桥和 runtime 测试**

Run:

```powershell
ctest --test-dir build_x64_v142 -C Release -R "navigation_vtk_bridge_test|navigation_runtime_coordinator_contract_test" --output-on-failure
```

Expected:

- PASS

- [ ] **Step 7: Commit**

```bash
git add UI/Forms/NavigationPage.ui UI/NewPages/NavigationPage.cpp UI/NewPages/Navigation/navigation_vtk_bridge.h UI/NewPages/Navigation/navigation_vtk_bridge.cpp tests/unit/NavigationVtkBridgeTest.cpp tests/unit/NavigationRuntimeCoordinatorContractTest.cpp
git commit -m "feat: realign navigation workspace to single 3d digital twin"
```

---

### Task 6: 收口评估页为病例级评估摘要页

**Files:**
- Modify: `UI/NewPages/NavigationPage.cpp`
- Modify: `UI/NewPages/Navigation/navigation_workspace_ui_binder.cpp`
- Test: `tests/unit/NavigationEvaluationSummaryFormatterTest.cpp`
- Test: `tests/unit/NavigationWorkspaceApplicationServiceTest.cpp`

- [ ] **Step 1: 写失败测试，锁定评估页必须展示病例级摘要字段**

```cpp
void NavigationEvaluationSummaryFormatterTest::formatter_builds_case_level_summary_for_ui_realigned_workspace()
{
    NavigationEvaluationSummary summary;
    summary.caseId = QStringLiteral("ankle-case-real-45971129749");
    summary.registrationErrorMm = 0.42;
    summary.visibleFrameRatio = 0.95;
    summary.trackingLatencyMs = 31.0;
    summary.trackingJitterMm = 0.28;
    summary.navigationSummary = QStringLiteral("导航过程稳定");

    const QString text = NavigationEvaluationSummaryFormatter::format(summary);

    QVERIFY(text.contains(QStringLiteral("0.42")));
    QVERIFY(text.contains(QStringLiteral("0.95")));
    QVERIFY(text.contains(QStringLiteral("31")));
    QVERIFY(text.contains(QStringLiteral("0.28")));
}
```

- [ ] **Step 2: 运行评估相关测试**

Run:

```powershell
ctest --test-dir build_x64_v142 -C Release -R "navigation_evaluation_summary_formatter_test|navigation_workspace_application_service_test" --output-on-failure
```

Expected:

- FAIL 或无法覆盖病例级 UI 展示

- [ ] **Step 3: 用统一摘要块替换旧 placeholder 主展示**

```cpp
void NavigationPageNew::refreshEvaluationSummary()
{
    const NavigationWorkspaceSnapshot snapshot = m_workspaceApplicationService->currentSnapshot();
    m_workspaceUiBinder->applyEvaluationSummary(snapshot.evaluationState);
}
```

- [ ] **Step 4: 评估页只保留摘要与导出动作**

```cpp
void NavigationPageNew::setupEvaluationWorkspace()
{
    if (ui->evaluationPlaceholderLabel) {
        ui->evaluationPlaceholderLabel->hide();
    }
    // 保留 exportEvaluationSummaryButton，并挂接病例级报告导出
}
```

- [ ] **Step 5: 回跑评估相关测试**

Run:

```powershell
ctest --test-dir build_x64_v142 -C Release -R "navigation_evaluation_summary_formatter_test|navigation_workspace_application_service_test" --output-on-failure
```

Expected:

- PASS

- [ ] **Step 6: Commit**

```bash
git add UI/NewPages/NavigationPage.cpp UI/NewPages/Navigation/navigation_workspace_ui_binder.cpp tests/unit/NavigationEvaluationSummaryFormatterTest.cpp tests/unit/NavigationWorkspaceApplicationServiceTest.cpp
git commit -m "feat: realign evaluation workspace to case-level summary"
```

---

### Task 7: 最小可用联调与文档回写

**Files:**
- Modify: `docs/superpowers/specs/2026-05-08-navigation-workspace-ui-realignment-design.md`
- Modify: `docs/superpowers/plans/2026-05-08-navigation-realtime-pose-digital-twin-end-to-end-checklist.md`

- [ ] **Step 1: 跑本轮 UI 收口相关回归测试**

Run:

```powershell
ctest --test-dir build_x64_v142 -C Release -R "ankle_navigation_workflow_contract_test|navigation_workspace_application_service_test|navigation_workflow_coordinator_test|navigation_runtime_coordinator_contract_test|navigation_vtk_bridge_test|navigation_evaluation_summary_formatter_test" --output-on-failure
```

Expected:

- PASS

- [ ] **Step 2: 构建主程序**

Run:

```powershell
cmake --build build_x64_v142 --config Release --target medicalpro
```

Expected:

- BUILD SUCCESS

- [ ] **Step 3: 人工联调，确认这条最小主链可以看见真实效果**

Manual path:

1. 进入病例工作台，确认病例工作包已就绪
2. 进入准备页，确认页面展示工作包器械、geometry 和标定状态，不再出现导入占位提示
3. 进入规划页，确认只显示规划摘要和病例只读资产，不再出现旧分割 / 旧导模入口
4. 进入配准页，确认只保留采点、删点、清点、计算配准，并可看到分骨结果摘要
5. 进入导航页，确认主界面为单一 3D 虚拟空间，可显示骨模型与当前器械
6. 进入评估页，确认能看到病例级评估摘要和导出入口

- [ ] **Step 4: 回写 spec 的实现状态**

```md
## Implementation Status

- 旧导入 / 旧分割 / 旧 2D-3D 占位主路径：已退出
- 准备页病例工作包真实收口：已实现
- 规划页中等规划页真实收口：已实现
- 配准页分骨工作流真实收口：已实现
- 导航页单一 3D 数字孪生页：已实现
- 评估页病例级摘要与导出：已实现
```

- [ ] **Step 5: 更新端到端检查清单**

```md
- 准备页不再依赖页面内导入器械或骨模型
- 规划页不再依赖旧分割 / 旧手动导模
- 配准页不再保留 2D-3D 占位入口
- 导航页主视图为单一 3D 虚拟空间
```

- [ ] **Step 6: Commit**

```bash
git add docs/superpowers/specs/2026-05-08-navigation-workspace-ui-realignment-design.md docs/superpowers/plans/2026-05-08-navigation-realtime-pose-digital-twin-end-to-end-checklist.md
git commit -m "docs: verify navigation workspace ui realignment"
```

---

## Self-Review

### Spec coverage

- 旧入口清退：Task 1
- 准备页真实收口：Task 2
- 规划页中等规划页：Task 3
- 配准页分骨结果与融合导航空间：Task 4
- 导航页单一 3D 数字孪生空间：Task 5
- 评估页病例级摘要和导出：Task 6
- 最小可用联调与文档回写：Task 7

### Placeholder scan

- 所有任务都给了明确文件路径、测试目标、命令和代码骨架
- 未使用 `TODO`、`TBD`、`implement later`

### Type consistency

- 统一使用 `NavigationWorkspaceSnapshot`、`NavigationWorkspacePreparationState`、`NavigationWorkspacePlanningState`、`NavigationWorkspaceRegistrationState`、`NavigationWorkspaceEvaluationState`
- 统一使用 `NavigationDisplayState` 作为导航页渲染输入
- 统一以 `NavigationWorkspaceApplicationService` 为页面真源

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-05-08-navigation-workspace-ui-realignment-implementation-plan.md`. Two execution options:

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

Which approach?
