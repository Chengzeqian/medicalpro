# Ankle Navigation Runtime Linkage Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把踝关节导航主链中标定状态、配准结果、tracking 质量、准入状态、评估快照的运行时联动从 `NavigationPage.cpp` 下沉到独立协作者。

**Architecture:** 新增 `navigation_runtime_state` 作为运行时状态载体，新增 `navigation_runtime_coordinator` 作为事件编排与状态刷新入口。`NavigationPage` 保留 UI 渲染与 slot 转发，`RegistrationController`、`NavigationEvaluationController`、`NavigationWorkflowCoordinator` 围绕 runtime coordinator 协作。

**Tech Stack:** C++20, Qt Widgets, Qt Test, existing navigation workflow classes, existing `NavigationEvaluationService`

---

## File Structure

### New Files

- `UI/NewPages/Navigation/navigation_runtime_state.h`
- `UI/NewPages/Navigation/navigation_runtime_state.cpp`
  - 统一承载 calibration、tracking、registration、confidence 运行时快照
- `UI/NewPages/Navigation/navigation_runtime_coordinator.h`
- `UI/NewPages/Navigation/navigation_runtime_coordinator.cpp`
  - 统一处理运行时事件、准入重算、评估快照持久化
- `tests/unit/NavigationRuntimeStateTest.cpp`
- `tests/unit/NavigationRuntimeCoordinatorContractTest.cpp`

### Modified Files

- `CMakeLists.txt`
  - 加入新导航协作者源文件
- `tests/unit/CMakeLists.txt`
  - 加入新测试目标
- `UI/NewPages/NavigationPage.h`
- `UI/NewPages/NavigationPage.cpp`
  - 下沉 `refreshNavigationConfidenceState(...)`、`persistEvaluationReportSnapshot(...)` 主要职责
- `UI/NewPages/Navigation/registration_controller.h`
- `UI/NewPages/Navigation/registration_controller.cpp`
  - 接入 runtime coordinator
- `UI/NewPages/Navigation/navigation_evaluation_controller.h`
- `UI/NewPages/Navigation/navigation_evaluation_controller.cpp`
  - 接入 runtime coordinator
- `UI/NewPages/Navigation/navigation_workflow_coordinator.h`
- `UI/NewPages/Navigation/navigation_workflow_coordinator.cpp`
  - 增加与 runtime coordinator 的协作边界

## Task 1: Introduce A Dedicated Runtime State Object

**Files:**
- Create: `UI/NewPages/Navigation/navigation_runtime_state.h`
- Create: `UI/NewPages/Navigation/navigation_runtime_state.cpp`
- Create: `tests/unit/NavigationRuntimeStateTest.cpp`
- Modify: `tests/unit/CMakeLists.txt`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing unit test**

为 `navigation_runtime_state` 写测试，至少覆盖：

- 初始状态没有 registration/tracking/confidence 快照
- 写入 tracking quality 后可读取 `calibrated`、`calibration_accuracy_mm`
- 写入 registration result 后可读取 `targetRegionTre`、`coverageScore`
- 写入 confidence result 后可读取 `allowNavigation`、`score`

- [ ] **Step 2: Run the test target to verify it fails**

Run: `cmake --build build_x64_v142 --config Release --target navigation_runtime_state_test`

Expected: target 不存在，或测试因新类未定义而失败。

- [ ] **Step 3: Add `navigation_runtime_state` minimal implementation**

实现一个轻量状态对象，至少暴露：

- `setCaseContext(...)`
- `setTrackingQuality(...)`
- `setRegistrationResult(...)`
- `setConfidenceResult(...)`
- `trackingQuality() const`
- `registrationResult() const`
- `confidenceResult() const`
- `hasTrackingQuality() const`
- `hasRegistrationResult() const`
- `hasConfidenceResult() const`

- [ ] **Step 4: Register sources and test target**

把新类和测试目标接入构建系统。

- [ ] **Step 5: Run the unit test to verify it passes**

Run: `ctest --test-dir build_x64_v142 -C Release --output-on-failure -R navigation_runtime_state_test`

Expected: 1/1 passed.

- [ ] **Step 6: Commit**

`git commit -m "feat: add navigation runtime state container"`

## Task 2: Add A Runtime Coordinator Contract Before Refactoring The Page

**Files:**
- Create: `UI/NewPages/Navigation/navigation_runtime_coordinator.h`
- Create: `UI/NewPages/Navigation/navigation_runtime_coordinator.cpp`
- Create: `tests/unit/NavigationRuntimeCoordinatorContractTest.cpp`
- Modify: `tests/unit/CMakeLists.txt`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing contract test**

合同测试至少约束：

- coordinator 必须依赖 `navigation_runtime_state`
- coordinator 必须暴露处理 registration/tracking/calibration 事件的入口
- coordinator 必须暴露评估快照持久化入口
- 页面后续可以把 `refreshNavigationConfidenceState(...)` 和 `persistEvaluationReportSnapshot(...)` 的主要职责委托给 coordinator

- [ ] **Step 2: Run the contract test to verify it fails**

Run: `cmake --build build_x64_v142 --config Release --target navigation_runtime_coordinator_contract_test`

Expected: target 不存在，或因 coordinator 未定义而失败。

- [ ] **Step 3: Add `navigation_runtime_coordinator` skeleton**

最小接口建议包含：

- `handleRegistrationResult(const PointRegistrationResult&)`
- `handleTrackingQuality(const QVariantMap&)`
- `handleCalibrationCompleted(const QVariantMap&)`
- `recomputeConfidence()`
- `persistEvaluationReportSnapshot(bool exportMetricsCsv = false)`

- [ ] **Step 4: Wire build files**

将 coordinator 和测试接入构建。

- [ ] **Step 5: Run the contract test to verify it passes**

Run: `ctest --test-dir build_x64_v142 -C Release --output-on-failure -R navigation_runtime_coordinator_contract_test`

Expected: 1/1 passed.

- [ ] **Step 6: Commit**

`git commit -m "feat: add navigation runtime coordinator contract"`

## Task 3: Move Confidence Recompute Out Of `NavigationPage.cpp`

**Files:**
- Modify: `UI/NewPages/NavigationPage.h`
- Modify: `UI/NewPages/NavigationPage.cpp`
- Modify: `UI/NewPages/Navigation/navigation_runtime_coordinator.h`
- Modify: `UI/NewPages/Navigation/navigation_runtime_coordinator.cpp`
- Modify: `tests/unit/AnkleNavigationWorkflowContractTest.cpp`
- Test: `tests/unit/NavigationRuntimeCoordinatorContractTest.cpp`

- [ ] **Step 1: Extend the failing contract test**

新增断言，要求：

- 页面不再自己承担完整 confidence 输入拼装
- 标定完成、配准完成、tracking 信号变化统一委托到 coordinator 触发重算

- [ ] **Step 2: Run the targeted contract tests to see them fail**

Run: `ctest --test-dir build_x64_v142 -C Release --output-on-failure -R "ankle_navigation_workflow_contract_test|navigation_runtime_coordinator_contract_test"`

Expected: 至少一个断言失败，提示页面尚未委托到 coordinator。

- [ ] **Step 3: Refactor `refreshNavigationConfidenceState(...)` responsibilities**

做法：

- 页面保留展示层入口
- 由 coordinator 负责构造 confidence 输入与产出结果
- 页面从 coordinator 读取当前快照用于渲染

- [ ] **Step 4: Re-run targeted tests**

Run: `ctest --test-dir build_x64_v142 -C Release --output-on-failure -R "ankle_navigation_workflow_contract_test|navigation_runtime_coordinator_contract_test"`

Expected: targeted tests pass.

- [ ] **Step 5: Commit**

`git commit -m "refactor: move navigation confidence recompute into runtime coordinator"`

## Task 4: Move Evaluation Snapshot Persistence Out Of `NavigationPage.cpp`

**Files:**
- Modify: `UI/NewPages/NavigationPage.h`
- Modify: `UI/NewPages/NavigationPage.cpp`
- Modify: `UI/NewPages/Navigation/navigation_runtime_coordinator.h`
- Modify: `UI/NewPages/Navigation/navigation_runtime_coordinator.cpp`
- Modify: `tests/unit/AnkleNavigationWorkflowContractTest.cpp`

- [ ] **Step 1: Extend the failing contract test**

新增断言，要求：

- 页面不再自己拼完整 `AnkleEvaluationReport`
- 标定完成、导航暂停、配准完成三处都走 runtime coordinator 的统一持久化入口

- [ ] **Step 2: Run the contract test to verify it fails**

Run: `ctest --test-dir build_x64_v142 -C Release --output-on-failure -R ankle_navigation_workflow_contract_test`

Expected: 失败，直到页面委托完成。

- [ ] **Step 3: Refactor persistence path**

做法：

- coordinator 负责读取 `NavigationEvaluationService`
- coordinator 负责合并 registration/tracking/gate/evaluation snapshot
- 页面仅触发 “persist now” 和 “是否导出 CSV” 选项

- [ ] **Step 4: Re-run the contract test**

Run: `ctest --test-dir build_x64_v142 -C Release --output-on-failure -R ankle_navigation_workflow_contract_test`

Expected: passed.

- [ ] **Step 5: Commit**

`git commit -m "refactor: move navigation evaluation snapshot persistence into runtime coordinator"`

## Task 5: Route Controllers Through The Runtime Coordinator

**Files:**
- Modify: `UI/NewPages/Navigation/registration_controller.h`
- Modify: `UI/NewPages/Navigation/registration_controller.cpp`
- Modify: `UI/NewPages/Navigation/navigation_evaluation_controller.h`
- Modify: `UI/NewPages/Navigation/navigation_evaluation_controller.cpp`
- Modify: `UI/NewPages/Navigation/navigation_workflow_coordinator.h`
- Modify: `UI/NewPages/Navigation/navigation_workflow_coordinator.cpp`
- Modify: `UI/NewPages/NavigationPage.cpp`
- Test: `tests/unit/NavigationRuntimeCoordinatorContractTest.cpp`

- [ ] **Step 1: Write or extend failing tests**

约束：

- `RegistrationController` 不再只做 `computeRegistration`
- `NavigationEvaluationController` 不再只做 `startNavigation`
- `NavigationWorkflowCoordinator` 会与 runtime coordinator 协作而不是仅做阶段转发

- [ ] **Step 2: Run the targeted tests to verify they fail**

Run: `ctest --test-dir build_x64_v142 -C Release --output-on-failure -R "navigation_workflow_coordinator_test|navigation_runtime_coordinator_contract_test"`

Expected: 失败，直到控制器协作边界落地。

- [ ] **Step 3: Implement minimal controller integration**

做法：

- `RegistrationController` 将配准完成结果提交给 runtime coordinator
- `NavigationEvaluationController` 从 runtime coordinator 读取准入状态
- `NavigationWorkflowCoordinator` 在关键阶段切换时驱动 runtime 同步

- [ ] **Step 4: Re-run the targeted tests**

Run: `ctest --test-dir build_x64_v142 -C Release --output-on-failure -R "navigation_workflow_coordinator_test|navigation_runtime_coordinator_contract_test"`

Expected: passed.

- [ ] **Step 5: Commit**

`git commit -m "refactor: route navigation controllers through runtime coordinator"`

## Task 6: Final Verification

**Files:**
- Verify only

- [ ] **Step 1: Build the relevant targets**

Run: `cmake --build build_x64_v142 --config Release --target medicalpro ankle_navigation_workflow_contract_test`

Expected: build succeeds.

- [ ] **Step 2: Run focused regression tests**

Run: `ctest --test-dir build_x64_v142 -C Release --output-on-failure -R "navigation_runtime_state_test|navigation_runtime_coordinator_contract_test|navigation_workflow_coordinator_test|navigation_confidence_evaluator_test|navigation_evaluation_service_test|navigation_evaluation_summary_formatter_test|ankle_navigation_workflow_contract_test"`

Expected: all selected tests pass.

- [ ] **Step 3: Review the spec coverage**

确认实现满足：

- 页面不再继续堆 runtime 联动主逻辑
- 标定/配准/tracking/gate/evaluation 快照联动统一下沉
- 为后续“真实导航闭环”的最后一段预留稳定落点

- [ ] **Step 4: Commit**

`git commit -m "test: verify navigation runtime linkage refactor"`
