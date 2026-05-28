# Registration Workspace Single-Algorithm UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rebuild the navigation workspace registration stage into a single-algorithm research UI centered on the real default workflow in the repo: `target_sensitive` point recommendation + `ankle_two_stage_constrained` registration, with guided point collection, registration evidence, and navigation gate readiness.

**Architecture:** Keep the existing registration runtime stack intact: `PointRegistrationService`, `RegistrationWorkflow`, `NavigationWorkspaceApplicationService`, `NavigationWorkspaceUiBinder`, and `RegistrationController` remain the truth sources. The real algorithm path already present in code is: paired landmark coarse registration in `PointRegistrationServiceImpl`, optional constrained surface refine delegated to `RegistrationCore`, then persistence into `PointRegistrationResult`, `NavigationWorkspaceRegistrationState`, and evaluation records. One important constraint from the current codebase: the UI-facing per-bone / fused-space schema exists, but `NavigationPage` does not yet populate `RegistrationController::resolvePerBoneRegistrationResults` or `resolveFusedNavigationSpacePath` in a fully real end-to-end way. This plan must therefore keep the UI language strict: present true registration evidence first, and only present per-bone / fused-space readiness when it comes from real snapshot data.

**Tech Stack:** Qt Widgets, `.ui` XML, QSS, C++17, existing source-contract Qt tests (`QtTest` + `ctest`).

---

## File Structure

- Modify: `UI/Forms/NavigationPage.ui`
  Responsibility: replace the old right-side registration control grouping with the new research-oriented panel skeleton and updated widget names/text.

- Modify: `UI/styles/three_pages_theme.qss`
  Responsibility: style the new registration strategy, guidance, progress, result, and gate-readiness panels consistently with the existing research theme.

- Modify: `UI/NewPages/NavigationPage.h`
  Responsibility: declare helper methods needed to initialize the new registration shell and refresh guided collection state.

- Modify: `UI/NewPages/NavigationPage.cpp`
  Responsibility: build the registration panels, hide legacy registration groups, wire guided point collection state, rewrite registration result presentation, and update calibration visibility semantics for this stage.

- Modify: `UI/NewPages/Navigation/navigation_workspace_ui_binder.cpp`
  Responsibility: reshape registration summary text so the right-side read-only summaries match the new single-algorithm wording and metrics.

- Test: `tests/unit/AnkleNavigationWorkflowContractTest.cpp`
  Responsibility: lock the new registration UI contract and prevent regressions back to the old multi-group shell.

- Test: `tests/unit/NavigationWorkflowCoordinatorTest.cpp`
  Responsibility: keep the per-bone registration result and fused navigation space contract aligned with the new UI language.

- Test: `tests/unit/PointRegistrationRegistrationCoreIntegrationTest.cpp`
  Responsibility: lock the actual registration-method facts used by the UI copy: `single_stage_landmark`, `landmark_plus_global_icp`, `landmark_plus_global_gicp`, and especially the default `ankle_two_stage_constrained`.

## Task 1: Lock The Registration UI Contract First

**Files:**
- Modify: `tests/unit/AnkleNavigationWorkflowContractTest.cpp`
- Test: `build_x64_v142/tests/unit/Release/ankle_navigation_workflow_contract_test.exe`

- [ ] **Step 1: Write the failing contract test for the single-algorithm registration shell**

Add new test declarations near the other registration UI contract slots:

```cpp
    void navigation_page_registration_tab_uses_single_algorithm_research_shell();
    void navigation_page_registration_tab_removes_legacy_parallel_groups();
```

Add a new contract body near the existing stage panel checks:

```cpp
void AnkleNavigationWorkflowContractTest::navigation_page_registration_tab_uses_single_algorithm_research_shell()
{
    const QString pageCode = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));
    const QString pageUi = readFile(QStringLiteral("UI/Forms/NavigationPage.ui"));
    const QString theme = readFile(QStringLiteral("UI/styles/three_pages_theme.qss"));

    QVERIFY2(pageCode.contains(QStringLiteral("registrationStrategyPanel")),
        "registration page must expose a single-algorithm strategy panel");
    QVERIFY2(pageCode.contains(QStringLiteral("registrationGuidancePanel")),
        "registration page must expose a guided point-collection panel");
    QVERIFY2(pageCode.contains(QStringLiteral("registrationPointProgressPanel")),
        "registration page must expose a point-progress panel");
    QVERIFY2(pageCode.contains(QStringLiteral("registrationGateReadinessPanel")),
        "registration page must expose a gate-readiness panel");
    QVERIFY2(pageCode.contains(QStringLiteral("ankle_two_stage_constrained")),
        "registration page must explicitly center the default constrained ankle registration method");
    QVERIFY2(pageCode.contains(QStringLiteral("target_sensitive")),
        "registration page must acknowledge the default target-sensitive point guidance strategy");
    QVERIFY2(theme.contains(QStringLiteral("QFrame#registrationStrategyPanel")),
        "theme must style the new registration strategy panel");
}
```

Add a second contract to reject the old shell:

```cpp
void AnkleNavigationWorkflowContractTest::navigation_page_registration_tab_removes_legacy_parallel_groups()
{
    const QString pageUi = readFile(QStringLiteral("UI/Forms/NavigationPage.ui"));
    const QString pageCode = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));

    QVERIFY2(!pageUi.contains(QStringLiteral("2D-3D配准")),
        "registration tab must not present 2D-3D registration as a primary group");
    QVERIFY2(!pageUi.contains(QStringLiteral("光学配准")),
        "registration tab must not present optical calibration as a parallel registration workflow");
    QVERIFY2(!pageCode.contains(QStringLiteral("ui->opticalRegLayout->addWidget(calibrationStatusLabel)")),
        "registration page must not keep calibration status anchored inside the old optical registration group");
}
```

- [ ] **Step 2: Run the registration workflow contract test to verify it fails**

Run:

```bash
cmake --build build_x64_v142 --config Release --target ankle_navigation_workflow_contract_test
ctest --test-dir build_x64_v142 -C Release -R "^ankle_navigation_workflow_contract_test$" --output-on-failure
```

Expected: FAIL because the new registration panel names and single-algorithm contract do not exist yet.

- [ ] **Step 3: Commit the failing test**

```bash
git add tests/unit/AnkleNavigationWorkflowContractTest.cpp
git commit -m "test: lock single-algorithm registration workspace contract"
```

## Task 2: Rebuild The Registration Tab Shell In The UI And Theme

**Files:**
- Modify: `UI/Forms/NavigationPage.ui`
- Modify: `UI/styles/three_pages_theme.qss`
- Test: `tests/unit/AnkleNavigationWorkflowContractTest.cpp`

- [ ] **Step 1: Replace the old registration side panel structure in the `.ui` file**

In `UI/Forms/NavigationPage.ui`, replace the current `registration2D3DGroup`, `pointRegGroup`, and `opticalRegGroup` stack with a research shell under `registrationControlLayout`.

The replacement structure should look like:

```xml
<widget class="QFrame" name="registrationStrategyPanel">
 <layout class="QVBoxLayout" name="registrationStrategyLayout">
  <item>
   <widget class="QLabel" name="registrationStrategySummaryLabel">
    <property name="text">
     <string>策略：地标点粗配准 → 约束区精配准</string>
    </property>
   </widget>
  </item>
  <item>
   <widget class="QLabel" name="registrationMethodSummaryLabel">
    <property name="text">
     <string>算法：ankle_two_stage_constrained / GPU GICP 精配准</string>
    </property>
   </widget>
  </item>
 </layout>
</widget>
<widget class="QFrame" name="registrationGuidancePanel">
 <layout class="QVBoxLayout" name="registrationGuidanceLayout">
  <item>
   <widget class="QLabel" name="registrationGuidanceModeLabel">
    <property name="text">
     <string>模式：target_sensitive 推荐顺序引导</string>
    </property>
   </widget>
  </item>
  <item>
   <widget class="QLabel" name="registrationGuidanceCurrentPointLabel">
    <property name="text">
     <string>当前建议点：等待规划数据</string>
    </property>
   </widget>
  </item>
  <item>
   <layout class="QHBoxLayout" name="registrationGuidanceActionLayout">
    <item><widget class="QPushButton" name="collectPointButton"/></item>
    <item><widget class="QPushButton" name="captureProbePointButton"/></item>
   </layout>
  </item>
 </layout>
</widget>
```

Also keep `registrationPointsTable`, `deletePointButton`, `clearAllPointsButton`, and `computeRegButton`, but nest them under a new `registrationPointProgressPanel` instead of `pointRegGroup`. Keep the existing view-side source-point picking workflow intact; the shell changes presentation, not the underlying point-entry mechanics.

- [ ] **Step 2: Add theme rules for the new registration panels**

Extend `UI/styles/three_pages_theme.qss` with targeted styles:

```css
QWidget#NavigationPage QFrame#registrationStrategyPanel,
QWidget#NavigationPage QFrame#registrationGuidancePanel,
QWidget#NavigationPage QFrame#registrationPointProgressPanel,
QWidget#NavigationPage QFrame#registrationGateReadinessPanel {
    background-color: rgba(12, 24, 32, 0.88);
    border: 1px solid rgba(124, 160, 191, 0.14);
    border-radius: 8px;
}

QWidget#NavigationPage QLabel#registrationStrategySummaryLabel,
QWidget#NavigationPage QLabel#registrationGuidanceModeLabel,
QWidget#NavigationPage QLabel#registrationGateSummaryLabel {
    color: ${TEXT_PRIMARY};
    font-size: 16px;
    font-weight: 700;
}
```

Also add styling for:

- `registrationGuidanceCurrentPointLabel`
- `registrationPointProgressSummaryLabel`
- `captureProbePointButton`
- `registrationRecommendedOrderLabel`

- [ ] **Step 3: Run the contract test again and verify it still fails for missing code wiring, not missing UI objects**

Run:

```bash
cmake --build build_x64_v142 --config Release --target ankle_navigation_workflow_contract_test
ctest --test-dir build_x64_v142 -C Release -R "^ankle_navigation_workflow_contract_test$" --output-on-failure
```

Expected: the earlier “missing panel names” failure should move forward, while code-level assertions still fail because `NavigationPage.cpp` does not yet build or populate the new shell.

- [ ] **Step 4: Commit the UI shell and theme scaffold**

```bash
git add UI/Forms/NavigationPage.ui UI/styles/three_pages_theme.qss
git commit -m "feat: scaffold research-oriented registration workspace shell"
```

## Task 3: Wire The New Registration Panels In NavigationPage

**Files:**
- Modify: `UI/NewPages/NavigationPage.h`
- Modify: `UI/NewPages/NavigationPage.cpp`
- Test: `tests/unit/AnkleNavigationWorkflowContractTest.cpp`

- [ ] **Step 1: Declare helpers for the registration shell in the header**

Add focused helpers to `UI/NewPages/NavigationPage.h`:

```cpp
    void setupRegistrationResearchPanels();
    void refreshRegistrationGuidancePanel();
    void refreshRegistrationGateReadinessPanel();
    QString registrationMethodDisplayText() const;
    QString registrationGuidanceDisplayText() const;
    QString registrationConstraintDisplayText() const;
```

Keep these helpers near the existing registration methods so the write scope stays inside `NavigationPage`.

- [ ] **Step 2: Build the single-algorithm registration shell in `setupRegistrationActionVisibility()`**

Replace the current “summaryGroup + metricLabel” logic with creation of:

```cpp
    auto* strategyLabel = createStageTextLabel(
        QStringLiteral("registrationStrategySummaryLabel"),
        QStringLiteral("策略：地标点粗配准 -> 约束区精配准\n算法：ankle_two_stage_constrained / GPU GICP"));
    auto* strategyPanel = createResearchPanel(
        QStringLiteral("registrationStrategyPanel"),
        QStringLiteral("配准策略"),
        strategyLabel);
```

Add equivalent blocks for:

- `registrationGuidancePanel`
- `registrationPointProgressPanel`
- `registrationResultPanel`
- `registrationGateReadinessPanel`

Inside the strategy/guidance panels, make room for these read-only labels:

- `registrationMethodSummaryLabel`
- `registrationConstraintSummaryLabel`
- `registrationGuidanceModeLabel`
- `registrationGuidanceCurrentPointLabel`
- `registrationRecommendedOrderLabel`

Rename button text during this step:

```cpp
    if (ui->collectPointButton) {
        ui->collectPointButton->setText(QStringLiteral("添加 CT 点"));
    }
    if (auto* captureProbePointButton = findChild<QPushButton*>(QStringLiteral("captureProbePointButton"))) {
        captureProbePointButton->setText(QStringLiteral("采集对应探针点"));
    }
    if (ui->computeRegButton) {
        ui->computeRegButton->setText(QStringLiteral("计算患者空间配准"));
    }
```

- [ ] **Step 3: Change point collection behavior so the two-step semantics match the UI**

Keep the existing VTK selection behavior, but split the actions:

```cpp
void NavigationPageNew::on_collectPointButton_clicked()
{
    if (!m_registrationWorkflow) {
        setupRegistration();
    }
    embedRegistrationVTKWidget();
    showInfo("采点引导", QStringLiteral("请先在配准视图中选择当前推荐的 CT 点。"));
}

void NavigationPageNew::captureRegistrationProbePoint()
{
    if (!m_registrationWorkflow) {
        return;
    }

    m_registrationWorkflow->setProbeSource(ProbePointSource::Simulated);
    const int generatedCount = m_registrationWorkflow->generateSimulatedProbePoints(0.5);
    if (generatedCount > 0) {
        updateRegistrationPointsList();
        refreshRegistrationGuidancePanel();
    }
}
```

This keeps the current simulated collection capability while aligning the UI with the actual source-point/target-point workflow. Do not invent a new capture backend in this task; the UI wording should be updated around the current `generateSimulatedProbePoints(0.5)` behavior until a per-point live capture path is implemented separately.

- [ ] **Step 4: Rewrite registration result and gate-readiness summaries around the real algorithm outputs**

Update `updateRegistrationResultDisplay()` and related refresh methods so they drive:

```cpp
    ui->regErrorLabel->setText(QStringLiteral("FRE：%1 mm").arg(result.rmsError, 0, 'f', 2));
```

Also populate labels for:

- `Target TRE`
- `coverageScore`
- `fused navigation space`
- calibration read-only state
- `constraint_refine_used`
- `constraint_region_count`

If `fusedNavigationSpaceReady` is still false in the real snapshot, the UI must explicitly present it as “未生成 / 未完成联通” rather than implying a finished multi-bone fused result.

Add a read-only gate summary block:

```cpp
void NavigationPageNew::refreshRegistrationGateReadinessPanel()
{
    auto* gateLabel = findChild<QLabel*>(QStringLiteral("registrationGateSummaryLabel"));
    if (!gateLabel || !m_workspaceApplicationService) {
        return;
    }

    const auto snapshot = m_workspaceApplicationService->currentSnapshot();
    gateLabel->setText(QStringLiteral(
        "患者空间：%1\n融合导航空间：%2\n探针校准：%3\n导航准入：%4")
        .arg(snapshot.registrationState.success ? QStringLiteral("已完成") : QStringLiteral("未完成"))
        .arg(snapshot.registrationState.fusedNavigationSpaceReady ? QStringLiteral("已生成") : QStringLiteral("未生成"))
        .arg(snapshot.calibrationState.completed ? QStringLiteral("已完成") : QStringLiteral("请回准备页完成"))
        .arg(snapshot.stageGate.allowed ? QStringLiteral("已满足") : snapshot.stageGate.reasonText));
}
```

- [ ] **Step 5: Run the registration workflow contract test and make sure it passes**

Run:

```bash
cmake --build build_x64_v142 --config Release --target ankle_navigation_workflow_contract_test
ctest --test-dir build_x64_v142 -C Release -R "^ankle_navigation_workflow_contract_test$" --output-on-failure
```

Expected: PASS, including the new single-algorithm registration shell assertions.

- [ ] **Step 6: Commit the `NavigationPage` wiring**

```bash
git add UI/NewPages/NavigationPage.h UI/NewPages/NavigationPage.cpp tests/unit/AnkleNavigationWorkflowContractTest.cpp
git commit -m "feat: rebuild registration stage around constrained ankle workflow"
```

## Task 4: Align Binder Summaries With The New Registration Semantics

**Files:**
- Modify: `UI/NewPages/Navigation/navigation_workspace_ui_binder.cpp`
- Modify: `UI/NewPages/Navigation/navigation_workspace_ui_binder.h`
- Modify: `tests/unit/NavigationWorkflowCoordinatorTest.cpp`
- Test: `tests/unit/AnkleNavigationWorkflowContractTest.cpp`

- [ ] **Step 1: Expand binder summary wording for the new registration stage**

Adjust `applyRegistrationSummary()` so the summary text reads like a research workspace instead of a generic result dump:

```cpp
    lines.append(QStringLiteral("策略：地标点粗配准 -> 约束区精配准"));
    lines.append(QStringLiteral("默认采点策略：target_sensitive"));
    lines.append(QStringLiteral("融合导航空间：%1")
                     .arg(registrationState.fusedNavigationSpaceReady
                              ? QStringLiteral("已生成")
                              : QStringLiteral("未生成")));
```

Update the metrics block to include the new framing:

```cpp
    metricLines.append(QStringLiteral("完整点对：%1").arg(registrationState.pointCount));
    metricLines.append(QStringLiteral("FRE：%1 mm").arg(registrationState.fre, 0, 'f', 2));
    metricLines.append(QStringLiteral("Target TRE：%1 mm").arg(registrationState.targetTre, 0, 'f', 2));
    metricLines.append(QStringLiteral("覆盖率：%1").arg(registrationState.coverageScore, 0, 'f', 2));
```

- [ ] **Step 2: Keep the binder API stable while clarifying ownership**

Do not add a new binder method unless necessary. If helper extraction is needed, keep it private inside `navigation_workspace_ui_binder.cpp`, for example:

```cpp
namespace
{
QString fusedSpaceSummaryText(const NavigationWorkspaceRegistrationState& state)
{
    return state.fusedNavigationSpaceReady
        ? QStringLiteral("已生成")
        : QStringLiteral("未生成");
}
}
```

This preserves the current call site in `refreshRegistrationWorkspace()`.

- [ ] **Step 3: Keep the registration state contract aligned in workflow tests**

In `tests/unit/NavigationWorkflowCoordinatorTest.cpp`, extend the existing `registration_controller_exposes_per_bone_results_and_fused_navigation_space_ready()` assertion block so the UI-facing state contract is explicit:

```cpp
    QCOMPARE(state.success, true);
    QCOMPARE(state.pointCount, 11);
    QCOMPARE(state.fusedCoverageScore, 0.82);
    QVERIFY(state.fre > 0.0);
```

- [ ] **Step 4: Run the registration contract plus the binder-dependent navigation regression set**

Run:

```bash
cmake --build build_x64_v142 --config Release --target ankle_navigation_workflow_contract_test navigation_workspace_application_service_test navigation_workflow_coordinator_test
ctest --test-dir build_x64_v142 -C Release -R "^(ankle_navigation_workflow_contract_test|navigation_workspace_application_service_test|navigation_workflow_coordinator_test)$" --output-on-failure
```

Expected: PASS with no binder summary regressions.

- [ ] **Step 5: Commit the binder summary alignment**

```bash
git add UI/NewPages/Navigation/navigation_workspace_ui_binder.cpp UI/NewPages/Navigation/navigation_workspace_ui_binder.h tests/unit/NavigationWorkflowCoordinatorTest.cpp
git commit -m "feat: align registration summaries with constrained workflow UI"
```

## Task 5: Final Verification And Runtime Build

**Files:**
- Modify: none
- Test: `build_x64_v142/Release/medicalpro.exe`

- [ ] **Step 1: Run the focused full verification set**

Run:

```bash
cmake --build build_x64_v142 --config Release --target medicalpro
cmake --build build_x64_v142 --config Release --target point_registration_registration_core_integration_test navigation_workflow_coordinator_test
ctest --test-dir build_x64_v142 -C Release -R "^(platform_ui_bridge_test|ankle_navigation_workflow_contract_test|navigation_workspace_application_service_test|navigation_evaluation_summary_formatter_test|navigation_vtk_bridge_test|navigation_runtime_coordinator_contract_test|navigation_workflow_coordinator_test|point_registration_registration_core_integration_test)$" --output-on-failure
```

Expected:

- `medicalpro.exe` builds successfully
- `point_registration_registration_core_integration_test` confirms the repo still defaults to `ankle_two_stage_constrained`
- all listed tests passed, 0 failed
- if `fusedNavigationSpaceReady` is still false in the real run, the UI copy remains honest and does not claim a completed fused multi-bone result

- [ ] **Step 2: Smoke-check the registration UI manually**

Launch:

```bash
build_x64_v142\\Release\\medicalpro.exe
```

Manual checklist:

- Registration page no longer shows `2D-3D配准` or `光学配准` as primary groups
- Right-side shell shows `配准策略 / 采点引导 / 点集进度 / 配准结果 / 导航准入状态`
- Default copy reflects `ankle_two_stage_constrained`
- Guidance copy reflects the default `target_sensitive` recommendation strategy
- `添加 CT 点` and `采集对应探针点` read clearly as two separate steps
- Calibration is shown as read-only guidance, with wording that points back to the preparation stage

- [ ] **Step 3: Commit the verification checkpoint**

```bash
git add docs/superpowers/specs/2026-05-11-registration-workspace-single-algorithm-ui-design.md \
        docs/superpowers/plans/2026-05-11-registration-workspace-single-algorithm-ui-implementation-plan.md
git commit -m "docs: add registration workspace implementation plan"
```

## Self-Review

- Spec coverage: covered the single-algorithm shell, guided collection, free collection fallback, read-only calibration state, registration metrics, fused navigation space, navigation gate readability, and removal of legacy parallel groups.
- Algorithm coverage: the plan now explicitly verifies the repo facts for `target_sensitive`, `ankle_two_stage_constrained`, and constrained GPU refine through `PointRegistrationRegistrationCoreIntegrationTest`.
- Placeholder scan: no `TBD`, `TODO`, or “implement later” instructions remain in the plan steps.
- Type consistency: the plan uses existing project types and names such as `RegistrationWorkflow`, `PointRegistrationResult`, `NavigationWorkspaceRegistrationState`, `RegistrationController`, `registrationSummaryLabel`, and `registrationMetricSummaryLabel`.

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-05-11-registration-workspace-single-algorithm-ui-implementation-plan.md`. Two execution options:

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

**Which approach?**
