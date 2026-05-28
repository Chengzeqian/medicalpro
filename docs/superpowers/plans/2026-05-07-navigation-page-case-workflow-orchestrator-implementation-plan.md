# Navigation Page Case Workflow Orchestrator Implementation Plan

日期：2026-05-07

> 目标：把 `NavigationPage` 从“页面级动作集合”升级成“病例工作流编排器”，通过统一的工作区状态聚合体、应用服务和 UI binder，打通准备、规划、标定、配准、导航、评估与恢复闭环。

## Implementation Packages

### Package 1: Workspace Aggregate Model And Snapshot Persistence

**Files**

- Create: `UI/NewPages/Navigation/navigation_workspace_types.h`
- Create: `UI/NewPages/Navigation/navigation_workspace_snapshot_store.h`
- Create: `UI/NewPages/Navigation/navigation_workspace_snapshot_store.cpp`
- Modify: `UI/NewPages/NavigationPage.h`
- Modify: `tests/unit/AnkleNavigationWorkflowContractTest.cpp`
- Modify: `tests/unit/NavigationWorkspaceSnapshotStoreTest.cpp`
- Modify: `tests/unit/CMakeLists.txt`
- Modify: `CMakeLists.txt`

**Goal**

先把病例工作区的统一状态模型和轻量快照持久化打通，为后续应用服务和 UI binder 提供真源。

**Tasks**

- [ ] 定义 7 类工作区子状态与阶段门禁类型
- [ ] 定义 `NavigationWorkspaceSnapshot`
- [ ] 定义快照 JSON 读写接口
- [ ] 实现 `navigation/workspace_snapshot.json` 的读写
- [ ] 为快照做 JSON round-trip 测试
- [ ] 将新源文件加入对应编译目标

**RED Tests**

- `NavigationWorkspaceSnapshotStoreTest::store_persists_latest_workspace_snapshot_for_stage_gate`
- `NavigationWorkspaceSnapshotStoreTest::store_restores_workspace_snapshot_as_single_truth_source`
- `AnkleNavigationWorkflowContractTest::navigation_workspace_snapshot_captures_stage_gate_state`

**Verification**

```powershell
cmake --build build_x64_v142 --config Release --target navigation_workspace_snapshot_store_test
ctest --test-dir build_x64_v142 -C Release -R navigation_workspace_snapshot_store_test --output-on-failure
```

**Commit**

```powershell
git add UI/NewPages/Navigation/navigation_workspace_types.h UI/NewPages/Navigation/navigation_workspace_snapshot_store.h UI/NewPages/Navigation/navigation_workspace_snapshot_store.cpp UI/NewPages/NavigationPage.h tests/unit/AnkleNavigationWorkflowContractTest.cpp tests/unit/NavigationWorkspaceSnapshotStoreTest.cpp tests/unit/CMakeLists.txt CMakeLists.txt
git commit -m "feat: add navigation workspace snapshot model"
```

### Package 2: Workspace Application Service

**Files**

- Create: `UI/NewPages/Navigation/navigation_workspace_application_service.h`
- Create: `UI/NewPages/Navigation/navigation_workspace_application_service.cpp`
- Modify: `UI/NewPages/Navigation/navigation_workflow_context.h`
- Modify: `UI/NewPages/Navigation/navigation_runtime_state.h`
- Modify: `UI/NewPages/Navigation/navigation_runtime_state.cpp`
- Modify: `Framework/Navigation/ankle_case_workspace_repository.*`
- Modify: `Framework/Navigation/ankle_planning_service.*`
- Modify: `Framework/Navigation/navigation_evaluation_service.*`
- Modify: `UI/NewPages/NavigationPage.h`
- Modify: `tests/unit/NavigationWorkspaceApplicationServiceTest.cpp`
- Modify: `tests/unit/AnkleNavigationWorkflowContractTest.cpp`
- Modify: `tests/unit/CMakeLists.txt`
- Modify: `CMakeLists.txt`

**Goal**

建立病例工作区应用服务，把 manifest / planning / registration / navigation / evaluation / runtime state 聚合成统一快照，并计算阶段门禁。

**Tasks**

- [ ] 实现 `loadWorkspace(caseId, patientId, patientName)`
- [ ] 实现正式结果读取与统一快照聚合
- [ ] 实现 `currentSnapshot()`
- [ ] 实现 `evaluateStageGate(AnkleWorkflowStage stage)`
- [ ] 实现 `persistSnapshot()` / `restoreSnapshot()`
- [ ] 实现准备 / 规划 / 标定 / 配准 / 导航 / 评估的状态写入口
- [ ] 为门禁阻塞原因补齐明确文本
- [ ] 为应用服务补单测

**RED Tests**

- `NavigationWorkspaceApplicationServiceTest::service_builds_workspace_snapshot_from_runtime_inputs`
- `NavigationWorkspaceApplicationServiceTest::service_evaluates_stage_gate_from_workspace_snapshot`
- `AnkleNavigationWorkflowContractTest::navigation_workspace_application_service_becomes_workspace_truth_source`

**Verification**

```powershell
cmake --build build_x64_v142 --config Release --target navigation_workspace_application_service_test
ctest --test-dir build_x64_v142 -C Release -R "navigation_workspace_snapshot_store_test|navigation_workspace_application_service_test" --output-on-failure
```

**Commit**

```powershell
git add UI/NewPages/Navigation/navigation_workspace_application_service.h UI/NewPages/Navigation/navigation_workspace_application_service.cpp UI/NewPages/Navigation/navigation_workflow_context.h UI/NewPages/Navigation/navigation_runtime_state.h UI/NewPages/Navigation/navigation_runtime_state.cpp UI/NewPages/NavigationPage.h Framework/Navigation/ankle_case_workspace_repository.cpp Framework/Navigation/ankle_planning_service.cpp Framework/Navigation/navigation_evaluation_service.cpp tests/unit/NavigationWorkspaceApplicationServiceTest.cpp tests/unit/AnkleNavigationWorkflowContractTest.cpp tests/unit/CMakeLists.txt CMakeLists.txt
git commit -m "feat: add navigation workspace application service"
```

### Package 3: Workflow Gate Wiring And UI Binder

**Files**

- Create: `UI/NewPages/Navigation/navigation_workspace_ui_binder.h`
- Create: `UI/NewPages/Navigation/navigation_workspace_ui_binder.cpp`
- Modify: `UI/NewPages/Navigation/navigation_workflow_coordinator.h`
- Modify: `UI/NewPages/Navigation/navigation_workflow_coordinator.cpp`
- Modify: `UI/NewPages/NavigationPage.h`
- Modify: `UI/NewPages/NavigationPage.cpp`
- Modify: `tests/unit/NavigationWorkflowCoordinatorTest.cpp`
- Modify: `tests/unit/AnkleNavigationWorkflowContractTest.cpp`
- Modify: `tests/unit/CMakeLists.txt`

**Goal**

让阶段推进和 UI 刷新都经过统一的工作区门禁与快照真源，页面本体只保留动作转发和展示。

**Tasks**

- [ ] 给 `NavigationWorkflowCoordinator` 增加 gate-aware 的 stage entry
- [ ] 让 prepare / planning / registration / navigation / evaluation 的入口先问 `NavigationWorkspaceApplicationService`
- [ ] 新增 UI binder，统一刷新阶段按钮、状态栏、阻塞原因和恢复摘要
- [ ] 在 `NavigationPage` 中只保留页面动作和 binder 调用
- [ ] 在 `setCaseContext(...)` 时加载工作区并刷新快照
- [ ] 把标定、配准、导航、评估的关键事件写回工作区快照
- [ ] 把工具/阶段按钮启用状态改成由门禁驱动

**RED Tests**

- `NavigationWorkflowCoordinatorTest::coordinator_defers_stage_entry_to_workspace_gate`
- `AnkleNavigationWorkflowContractTest::navigation_page_stage_gate_ui_reads_workspace_snapshot_truth_source`

**Verification**

```powershell
cmake --build build_x64_v142 --config Release --target navigation_workflow_coordinator_test ankle_navigation_workflow_contract_test
ctest --test-dir build_x64_v142 -C Release -R "navigation_workflow_coordinator_test|ankle_navigation_workflow_contract_test" --output-on-failure
```

**Commit**

```powershell
git add UI/NewPages/Navigation/navigation_workspace_ui_binder.h UI/NewPages/Navigation/navigation_workspace_ui_binder.cpp UI/NewPages/Navigation/navigation_workflow_coordinator.h UI/NewPages/Navigation/navigation_workflow_coordinator.cpp UI/NewPages/NavigationPage.h UI/NewPages/NavigationPage.cpp tests/unit/NavigationWorkflowCoordinatorTest.cpp tests/unit/AnkleNavigationWorkflowContractTest.cpp tests/unit/CMakeLists.txt
git commit -m "feat: route navigation page through workspace gates"
```

### Package 4: Restore, Evaluation, And Final Verification

**Files**

- Modify: `UI/NewPages/NavigationPage.cpp`
- Modify: `UI/NewPages/Navigation/navigation_evaluation_controller.h`
- Modify: `UI/NewPages/Navigation/navigation_evaluation_controller.cpp`
- Modify: `Framework/Navigation/navigation_evaluation_service.*`
- Modify: `tests/unit/NavigationEvaluationServiceTest.cpp`
- Modify: `tests/unit/NavigationWorkspaceApplicationServiceTest.cpp`
- Modify: `tests/unit/AnkleNavigationWorkflowContractTest.cpp`

**Goal**

补齐完整恢复路径、评估摘要和最终验证，确保页面重开时基于真源恢复，并且评估页能展示完整链路结果。

**Tasks**

- [ ] 恢复时读取正式结果文件并重算门禁
- [ ] 把最近一次标定、配准、导航结果汇总到统一摘要
- [ ] 评估页显示完整链路摘要与导出能力
- [ ] 保存 / 导出导航运行记录与 case summary
- [ ] 验证页面重开后能恢复上次工作区状态
- [ ] 清理页面中的旧式局部真源字段

**Verification**

```powershell
ctest --test-dir build_x64_v142 -C Release -R "navigation_workspace_snapshot_store_test|navigation_workspace_application_service_test|navigation_workflow_coordinator_test|navigation_runtime_coordinator_contract_test|navigation_runtime_state_test|ankle_navigation_workflow_contract_test|ankle_case_workspace_repository_test|ankle_planning_service_test|navigation_evaluation_service_test" --output-on-failure
cmake --build build_x64_v142 --target medicalpro --config Release
```

**Manual Smoke**

1. 仅从已有病例工作台进入导航页
2. 页面顶部和右侧展示完整病例链路摘要
3. 未满足门禁时，阶段按钮不可用并显示阻塞原因
4. 标定完成后，配准 / 导航门禁自动更新
5. 导航运行后，评估页显示运行记录与摘要
6. 重新进入同一病例后，页面从工作区快照恢复

**Commit**

```powershell
git add .
git commit -m "feat: restore and gate full navigation workflow"
```

## Recommended Order

1. Package 1: state model + snapshot persistence
2. Package 2: application service aggregation
3. Package 3: workflow gate wiring + UI binder
4. Package 4: restore + evaluation + final verification

## Final Acceptance Criteria

- `NavigationPage` 变成病例工作流编排器，而不是阶段按钮堆叠页
- 7 类工作区状态有统一真源
- 五阶段壳保留，但门禁统一
- 标定、规划、配准、导航、评估都从统一工作区状态读写
- 页面重开能恢复，并基于真服务重新计算门禁
- 评估页能展示完整病例链路摘要并导出
