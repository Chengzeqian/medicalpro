# Navigation Page Case Workflow Orchestrator Design

日期：2026-05-07

## 背景

当前 `NavigationPage` 已经能完成部分准备、标定、配准、导航和评估动作，但这些能力仍然分散在页面成员、控制器和运行时状态里，缺少统一的病例工作区真源。

现状问题：

- 病例身份、骨骼资产、器械资产、探针标定、规划、配准、导航、评估分散在不同成员和服务里
- 阶段门禁零散写在按钮和槽函数里
- 页面重开后只能恢复局部 UI 状态，不能恢复完整病例工作流
- 评估页只能看到片段结果，不能看到完整链路摘要

## 目标

把导航页升级成“病例工作流编排器”，围绕一个病例工作区状态聚合体统一管理 7 类数据：

1. 病例身份
2. 骨骼资产
3. 器械资产
4. 探针标定结果
5. 规划结果
6. 配准结果
7. 导航 / 评估结果

UI 保留现有五阶段壳，但每一阶段都明确：

- 输入资产
- 阶段动作
- 完成产物
- 准入条件
- 跨阶段输出

标定不单独新增页签，作为准备与配准之间的受控子流程。

## 设计原则

- 工作流真源集中在 `UI/NewPages/Navigation`
- 底层算法与正式结果继续保留在 `Framework/Navigation` 和既有业务 service
- 页面只做展示和动作转发
- 阶段门禁统一计算，不在按钮槽里散落判断
- 正式结果继续写入现有持久化文件
- 页面快照只保存恢复 UI 所需最小状态
- 页面重开后必须基于真实服务重算门禁，而不是直接信任旧快照

## 模块边界

- `UI/NewPages/Navigation/navigation_workspace_types.h`：工作区状态、门禁、快照结构
- `UI/NewPages/Navigation/navigation_workspace_snapshot_store.h/.cpp`：轻量快照读写
- `UI/NewPages/Navigation/navigation_workspace_application_service.h/.cpp`：工作区聚合、写回、门禁计算、恢复
- `UI/NewPages/Navigation/navigation_workspace_ui_binder.h/.cpp`：快照到控件、门禁到按钮、动作到服务
- `UI/NewPages/NavigationPage.h/.cpp`：页面生命周期、事件转发、局部展示
- `Framework/Navigation/*`：既有病例仓库、规划、评估、运行时能力

## 工作区状态

### 1. 病例身份

包含：

- `caseId`
- `patientId`
- `patientName`
- `surgeryId`
- `currentStage`
- `lastUpdatedAt`

用途：

- 定位当前病例
- 驱动页面标题、摘要和恢复

### 2. 骨骼资产

包含：

- DICOM 是否已就绪
- 骨骼模型是否已就绪
- STL / 模型路径引用
- 骨骼选择列表
- 当前选中的骨骼资产

用途：

- 为规划提供输入
- 为配准提供目标骨骼引用
- 为恢复时重新定位模型来源提供依据

### 3. 器械资产

包含：

- 选中的探针 / 导航器械
- 工具几何文件
- 几何 ID
- 器械管理服务可用性
- 当前工具是否可见

用途：

- 为标定提供 geometry
- 为导航提供工具可见性和门禁依据

### 4. 探针标定结果

包含：

- 是否已开始
- 已采集点数
- 需求点数
- 是否完成
- `tipOffset`
- `accuracy`
- `geometryId`
- 完成时间

用途：

- 为配准和导航提供已标定的探针偏移
- 作为导航准入门禁之一

### 5. 规划结果

包含：

- 是否存在规划
- 目标区是否完成
- 参考骨骼
- 约束区域
- 推荐点序
- 最近保存时间

用途：

- 为配准提供目标和约束
- 为评估提供规划链路上下文

### 6. 配准结果

包含：

- 点数
- 是否配准成功
- `fre`
- `targetTre`
- `coverageScore`
- 变换矩阵
- 完成时间

用途：

- 为导航提供空间配准基准
- 为评估提供配准质量来源

### 7. 导航 / 评估结果

包含：

- 追踪器是否连接
- 导航工具是否可见
- 导航是否运行中
- 当前 confidence
- 是否允许进入导航
- 阻塞原因列表
- 是否存在导航运行记录
- 是否存在评估报告
- 摘要文本
- 导出是否可用

用途：

- 驱动导航页准入与运行状态
- 为评估页和导出提供完整链路摘要
- 为页面恢复时的最终摘要与导出入口提供真源

## 门禁模型

`NavigationStageGate` 需要至少包含：

- `requestedStage`
- `allowed`
- `reasonCode`
- `reasonText`
- `severity`
- `lastComputedAt`

门禁按阶段统一计算，页面按钮和页签只消费计算结果，不自行判定。

## 应用服务

`NavigationWorkspaceApplicationService` 负责：

- 加载病例工作区
- 聚合 manifest / planning / registration / navigation / evaluation / runtime state
- 生成统一快照
- 计算阶段门禁
- 持久化页面恢复快照
- 向 UI 提供统一动作入口

建议对外方法至少包括：

- `loadWorkspace(caseId, patientId, patientName)`
- `currentSnapshot() const`
- `evaluateStageGate(AnkleWorkflowStage stage) const`
- `persistSnapshot()`
- `restoreSnapshot()`
- `recordPlanningResult(...)`
- `recordCalibrationState(...)`
- `recordRegistrationResult(...)`
- `recordNavigationRuntime(...)`
- `recordEvaluationSummary(...)`

## UI Binder

`NavigationWorkspaceUiBinder` 负责：

- 把统一快照映射到页面控件
- 把阶段门禁映射到页签和按钮可用性
- 把用户动作转发给应用服务
- 把页面局部文本改为基于快照刷新

建议对外方法至少包括：

- `bind(...)`
- `refreshFromSnapshot()`
- `applyStageGate(...)`
- `applyWorkspaceSummary(...)`
- `applyRestoreState(...)`

## 五阶段壳

页面保留五个阶段页签：

1. 准备
2. 规划
3. 配准
4. 导航
5. 评估

标定作为准备 / 配准之间的受控子流程，不单独新增页签。

## 阶段流转

### 准备

输入：

- 病例身份
- 骨骼资产
- 器械资产
- DICOM / 模型资源

动作：

- 加载病例上下文
- 选择骨骼资产
- 选择器械与 geometry
- 触发标定入口

产物：

- `case_manifest.json`
- 骨骼资产选择
- 器械资产选择
- 标定前准备状态

门禁：

- 只有已进入病例工作台的病例可进入

### 规划

输入：

- 骨骼资产
- 器械资产

动作：

- 构建规划
- 保存规划结果

产物：

- `planning/planning.json`
- 目标区
- 约束区域
- 推荐点序

门禁：

- 病例上下文有效
- 骨骼资产可用
- 器械资产可用

### 标定

输入：

- 追踪器连接状态
- 探针 geometry
- 物理设备状态

动作：

- 收集 pose sample
- 执行标定
- 记录 tipOffset / accuracy / geometryId

产物：

- 标定结果
- 标定质量统计

门禁：

- 追踪器在线
- 探针 geometry 已明确
- 物理设备模式可用

### 配准

输入：

- 规划结果
- 标定结果
- 配准点集

动作：

- 执行点配准
- 保存配准结果

产物：

- `registration/registration_result.json`
- 配准变换矩阵
- `fre` / `targetTre` / `coverageScore`

门禁：

- 规划已就绪
- 标定已完成
- 点数达到要求

### 导航

输入：

- 配准结果
- 追踪器状态
- 导航工具状态
- 标定结果

动作：

- 启动导航
- 持续更新 confidence
- 记录导航运行数据

产物：

- `navigation/navigation_run.json`
- 导航 metrics
- 阻塞原因列表

门禁：

- 追踪器在线
- 导航工具可见
- 标定完成
- 配准成功
- confidence 达标

### 评估

输入：

- 导航运行记录
- 配准结果
- 标定结果

动作：

- 生成评估报告
- 导出 case summary / metrics

产物：

- `evaluation/evaluation_report.json`
- `evaluation/evaluation_metrics.csv`
- `evaluation/case_evaluation_summary.json`

门禁：

- 已有导航运行记录或评估报告

## 持久化

### 正式结果

继续沿用现有正式结果文件：

- `case_manifest.json`
- `planning/planning.json`
- `registration/registration_result.json`
- `navigation/navigation_run.json`
- `evaluation/evaluation_report.json`

### 页面快照

新增轻量 `navigation/workspace_snapshot.json`，只保存页面恢复所需信息：

- 当前阶段
- 选中的骨骼 / 器械 / geometry 引用
- 最近一次标定 / 配准 / 导航摘要
- 最近门禁阻塞原因
- 最近刷新时间
- 最近一次恢复成功的阶段摘要

快照不替代正式结果。

## 恢复策略

页面重开时按以下顺序恢复：

1. 加载病例身份和工作区目录
2. 读取正式结果文件
3. 构建统一工作区快照
4. 重新计算全部阶段门禁
5. 刷新页面壳、阶段按钮和右侧状态栏
6. 如果真实服务在线，再用实时状态覆盖运行态字段

恢复时不能直接复用旧快照中的 allow/deny 结论。
如果任何正式结果缺失，则按已有文件重新聚合，快照只补 UI 恢复信息，不补业务真值。

## 计划边界

本版纳入：

- 五阶段壳升级
- 7 类工作区状态聚合
- 统一门禁
- 自动恢复
- 标定 / 配准 / 导航闭环
- 评估摘要和导出

本版不纳入：

- 新病例创建 / 病例选择 UI 重构
- 2D-3D 配准链路重做
- 算法本体重写
- 其他页面的整体重构

## 成功标准

完成后应满足：

- 只能从已有病例工作台进入导航页
- 页面能展示完整病例工作流摘要
- 各阶段按钮和动作都由统一门禁控制
- 标定、规划、配准、导航、评估的结果都能聚合到一个工作区状态
- 页面重开后能恢复上次工作区状态，并基于真实服务重新判定门禁
- 评估页可输出完整链路摘要和导出结果
