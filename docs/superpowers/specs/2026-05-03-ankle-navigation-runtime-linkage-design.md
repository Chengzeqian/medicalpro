# Ankle Navigation Runtime Linkage Design

**日期：** 2026-05-03  
**范围：** `medicalpro` 踝关节导航主链中的配准页、导航页、标定状态、准入状态运行时联动  
**目标：** 把当前堆在 `NavigationPage.cpp` 内部的运行时状态拼装、准入刷新和评估快照持久化逻辑下沉成独立协作者，为后续“真实导航闭环”继续落地提供稳定边界。

## 1. 当前结论

这项工作目前**没有完成**。

已经完成的只有页面内收口：

- `UI/NewPages/NavigationPage.h` 已声明 `refreshNavigationConfidenceState(...)`、`persistEvaluationReportSnapshot(...)`
- `UI/NewPages/NavigationPage.cpp:2108` 到 `UI/NewPages/NavigationPage.cpp:2129` 已把 `registrationApplied`、`sessionStateChanged`、`toolStatusChanged`、`calibrationCompleted` 接到页面刷新
- `UI/NewPages/NavigationPage.cpp:1233`、`UI/NewPages/NavigationPage.cpp:1489`、`UI/NewPages/NavigationPage.cpp:2426` 已把部分评估持久化统一走 `persistEvaluationReportSnapshot(...)`

但未完成的关键问题仍然存在：

- `UI/NewPages/NavigationPage.cpp:1720` 仍直接负责构造联合准入输入并执行准入刷新
- `UI/NewPages/NavigationPage.cpp:1775` 仍直接负责评估快照拼装和持久化
- `UI/NewPages/Navigation/registration_controller.h` 目前只有 `computeRegistration`
- `UI/NewPages/Navigation/navigation_evaluation_controller.h` 目前只有 `startNavigation`
- `UI/NewPages/Navigation/navigation_workflow_coordinator.cpp` 目前仍只是阶段切换和动作转发

所以，当前状态只能算“页面内部联动已初步打通”，还不能算“配准页、导航页、标定状态、准入状态形成更强运行时联动”。

## 2. 设计目标

本轮不做新的大重构，只解决真实阻塞点：

- 不再继续往 `NavigationPage.cpp` 增加运行时联动逻辑
- 把“标定状态、配准结果、tracking 质量、准入决策、评估快照”收口到稳定协作者
- 让 `NavigationPage` 退回到 UI 渲染、slot 转发和少量提示文案
- 让后续“标定完成后自动刷新导航准入状态/可信度”和“配准页与分步标定流程串起来”的实现不再依赖页面私有状态拼接

非目标：

- 不重写现有 `NavigationWorkflowContext`
- 不一次性改造全部导航页逻辑
- 不在本轮把 MeshGPU / ProbeCalibration 算法本身改写

## 3. 目标结构

建议新增两个轻量协作者：

### 3.1 `navigation_runtime_state`

建议路径：

- `UI/NewPages/Navigation/navigation_runtime_state.h`
- `UI/NewPages/Navigation/navigation_runtime_state.cpp`

职责：

- 保存当前运行时快照，而不是只依赖页面零散成员变量
- 统一持有以下信息：
  - calibration 状态
  - tracking 质量快照
  - registration 结果快照
  - 最新 gate/confidence 决策
  - evaluation report 持久化所需的最小上下文

建议最小字段：

- `QString caseId`
- `QString trackingSessionId`
- `QString navigationToolId`
- `QVariantMap trackingQuality`
- `PointRegistrationResult registrationResult`
- `NavigationConfidenceResult confidenceResult`
- `bool hasRegistrationResult`
- `bool hasTrackingQuality`
- `bool hasConfidenceResult`

### 3.2 `navigation_runtime_coordinator`

建议路径：

- `UI/NewPages/Navigation/navigation_runtime_coordinator.h`
- `UI/NewPages/Navigation/navigation_runtime_coordinator.cpp`

职责：

- 统一接收运行时事件
- 更新 `navigation_runtime_state`
- 触发准入重算
- 触发评估快照持久化
- 对页面暴露“当前是否允许导航”“当前告警/拒绝原因”“是否需要刷新评估摘要”等稳定结果

它需要接收的事件包括：

- `registrationApplied`
- `sessionStateChanged`
- `toolStatusChanged`
- `calibrationCompleted`
- `finishProbeCalibration()`
- `onRegistrationCompleted(...)`
- `on_pauseNavigationButton_clicked()`

## 4. 职责边界

### 4.1 `NavigationPage`

保留职责：

- 用户交互
- UI 控件刷新
- 把当前页面可见状态绑定到 coordinator 输出
- 弹出提示信息

移出职责：

- 不再自己拼完整准入输入
- 不再自己拼完整评估报告快照
- 不再自己决定何时因为外部运行时事件触发快照持久化

### 4.2 `RegistrationController`

从当前“只有 `computeRegistration` 的薄壳”提升为：

- 负责触发配准计算
- 负责把配准完成结果交给 runtime coordinator
- 不直接依赖页面内部字段决定后续准入/评估刷新

### 4.3 `NavigationEvaluationController`

从当前“只有 `startNavigation` 的薄壳”提升为：

- 负责启动/暂停导航阶段相关动作
- 通过 runtime coordinator 获取当前准入状态，而不是让页面重复拼接

### 4.4 `NavigationWorkflowCoordinator`

继续负责阶段切换，但新增职责：

- 与 `navigation_runtime_coordinator` 协作
- 在阶段进入时触发必要的运行时同步
- 统一把“当前阶段是否允许进入下一步”建立在 runtime state 上

## 5. 事件流设计

### 5.1 标定完成

事件源：

- `finishProbeCalibration()`
- `OpticalTrackingService::calibrationCompleted`

目标流：

1. coordinator 更新 `trackingQuality.calibrated`
2. coordinator 更新 `trackingQuality.calibration_accuracy_mm`
3. coordinator 重新计算 navigation confidence
4. coordinator 持久化 evaluation report snapshot
5. 页面只刷新显示

### 5.2 配准完成

事件源：

- `onRegistrationCompleted(const PointRegistrationResult&)`

目标流：

1. coordinator 更新 registration result
2. coordinator 重新计算 navigation confidence
3. coordinator 持久化 evaluation report snapshot
4. 页面刷新准入状态和评估摘要

### 5.3 tracking 状态变化

事件源：

- `sessionStateChanged`
- `toolStatusChanged`

目标流：

1. coordinator 拉取最新 tracking quality
2. coordinator 重新计算 confidence/gate
3. 页面仅渲染新的允许状态、分数和原因

### 5.4 导航暂停或结束

事件源：

- `on_pauseNavigationButton_clicked()`

目标流：

1. controller 保存 navigation run
2. coordinator 统一持久化 evaluation report snapshot
3. coordinator 选择性导出 metrics CSV
4. 页面刷新 evaluation summary

## 6. 渐进式迁移范围

本轮只迁移四类逻辑：

- `refreshNavigationConfidenceState(...)`
- `persistEvaluationReportSnapshot(...)`
- 与 runtime signal connect 强相关的刷新分发
- 标定完成、配准完成、导航暂停三个关键持久化触发点

不在本轮迁移：

- 全部按钮点击逻辑
- 全部病例工作区逻辑
- 全部 VTK 视图逻辑

## 7. 验收标准

完成本轮后，至少满足：

1. `NavigationPage.cpp` 不再拥有完整的 confidence 输入拼装逻辑
2. `NavigationPage.cpp` 不再拥有完整的 evaluation report 快照拼装逻辑
3. 标定完成、配准完成、tracking 变化三类事件都经由 runtime coordinator 驱动状态刷新
4. 页面仍能在同样的时机更新准入显示和评估摘要
5. 现有导航契约测试可扩展为对 runtime 协作者边界的检查，而不是继续依赖页面源码字符串搜索

## 8. 风险与控制

### 8.1 风险：状态重复

如果 `NavigationPage` 成员和 `navigation_runtime_state` 同时保存同一份状态，后续容易漂移。

控制：

- 迁移后以 `navigation_runtime_state` 为准
- 页面只保留 UI 所必需的本地字段

### 8.2 风险：事件顺序不稳定

tracking 信号和配��完成信号可能交错，导致准入结果短暂抖动。

控制：

- coordinator 中按“更新 state -> 重算 confidence -> 选择性持久化”的固定顺序执行
- 页面只消费最终快照

### 8.3 风险：继续往 `NavigationPage.cpp` 回填逻辑

这是当前最现实的回退风险。

控制：

- 新需求优先落在 runtime coordinator
- 通过测试约束页面不再持有完整持久化/准入实现

## 9. 结论

这项工作当前还没完成，接下来不该继续在 `NavigationPage.cpp` 堆行为。  
正确下一步是先按这个边界补上 `navigation_runtime_state` 和 `navigation_runtime_coordinator`，再把“配准页与分步标定流程串起来”的最后一段建立在这两个协作者之上。
