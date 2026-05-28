# Navigation Workspace Orchestrator V2 Design

日期：2026-05-07

## 背景

当前 `NavigationPage` 及相关模块已经完成一轮“页面壳层 + 工作区聚合”演进，代码中已经存在：

- `NavigationWorkspaceApplicationService`
- `NavigationWorkspaceSnapshotStore`
- `NavigationWorkspaceUiBinder`
- `NavigationWorkflowCoordinator`
- `NavigationRuntimeCoordinator`
- `NavigationRuntimeState`

但现有设计仍主要围绕“导航页工作区壳层”展开，对以下几个关键点定义不够完整：

1. 准备阶段还没有被定义为“多器械几何绑定与逐个标定”的正式阶段
2. 规划阶段仍偏向单骨、单结果假设
3. 配准阶段仍需要从“单结果配准”演进为“分骨/分部位配准 + 融合导航空间”
4. 导航阶段还保留旧四视图痕迹，而目标页面是单一 3D 虚拟空间
5. 评估阶段还没有把分骨质量和导航摘要作为正式输出

因此需要在现有导航工作区聚合基础上，定义 V2 编排规格。

## 目标

V2 编排规格需要达到以下结果：

1. `NavigationPage` 真正成为手术导航工作区，而不是页面动作集合
2. `NavigationWorkspaceApplicationService` 成为导航工作区唯一业务聚合入口
3. 五环节页面全部服从统一门禁与统一恢复模型
4. 配准结果模型升级为分骨/分部位结果集与融合导航空间
5. 导航页面升级为单一 3D 虚拟空间
6. 评估页面输出完整链路摘要和导出能力

## 非目标

本次不纳入以下范围：

- 重写服务配准算法内部实现
- 重写底层光学跟踪算法
- 把现有 `NavigationPage.cpp` 一次性拆成完全独立的所有子页面
- 对数据中台和病例工作台进行大规模 UI 重构

## 方案比较

### 方案 A：继续在 `NavigationPage` 局部补逻辑

优点：

- 修改面最小

缺点：

- 会继续把准备、规划、配准、导航、评估逻辑散落在页面类中
- 很难承载多骨、多部位、融合导航空间

### 方案 B：保留现有工作区聚合骨架，升级为 V2 编排层

优点：

- 能复用现有 `NavigationWorkspaceApplicationService`、`SnapshotStore`、`UiBinder`
- 可以逐步把旧逻辑收束到聚合层
- 风险和收益平衡最好

缺点：

- 需要补足新的状态模型和控制器边界

### 方案 C：改造成后端流程引擎 + 纯前端壳

优点：

- 长期结构最强

缺点：

- 明显超出当前迭代

### 结论

采用方案 B。

## 总体模块边界

### 1. `NavigationPage`

职责：

- 页面生命周期
- 用户交互事件转发
- 3D 视图容器承载
- 局部显示刷新

约束：

- 不再作为工作流真源
- 不自行维护全链路门禁
- 不自行拼装跨阶段聚合状态

### 2. `NavigationWorkspaceApplicationService`

职责：

- 加载当前病例工作包
- 聚合准备、规划、配准、导航、评估状态
- 输出统一工作区快照
- 统一计算阶段门禁
- 协调正式结果持久化与快照恢复

它是导航工作区唯一业务聚合入口。

### 3. `NavigationWorkspaceSnapshotStore`

职责：

- 读写 `workspace_snapshot.json`

约束：

- 不处理正式结果文件
- 不计算门禁

### 4. `NavigationWorkspaceUiBinder`

职责：

- 把工作区快照映射到页面控件
- 把阶段门禁映射到按钮、页签、状态栏
- 把阻塞原因和状态摘要映射到结果区

### 5. `PreparationPlanningController`

职责：

- 编排准备与规划阶段动作

约束：

- 不维护全局工作区真源
- 结果要回写到 `NavigationWorkspaceApplicationService`

### 6. `RegistrationController`

职责：

- 编排分骨/分部位配准
- 调用服务配准黑盒
- 生成分骨结果和融合结果

### 7. `NavigationEvaluationController`

职责：

- 编排导航运行、评估汇总和导出

### 8. `NavigationRuntimeCoordinator`

职责：

- 接收实时跟踪数据
- 计算 confidence 和运行态摘要
- 输出运行态快照给工作区聚合层

## 工作区状态模型 V2

现有 `NavigationWorkspaceSnapshot` 需要从“单阶段摘要”升级为“病例驱动的五环节聚合模型”。

### 1. `case_context`

字段建议：

- `case_id`
- `patient_id`
- `patient_name`
- `surgery_id`
- `current_stage`
- `last_updated_at`

### 2. `asset_state`

字段建议：

- `bound_bone_assets[]`
- `active_bone_assets[]`
- `bound_instruments[]`
- `active_instruments[]`
- `instrument_geometry_bindings[]`
- `bone_models_ready`
- `geometry_ready`

### 3. `preparation_state`

字段建议：

- `instrument_calibration_states[]`
- `all_required_instruments_calibrated`
- `blocking_reasons[]`

其中单器械标定状态至少包含：

- `instrument_id`
- `geometry_asset_id`
- `started`
- `collected_points`
- `required_points`
- `completed`
- `accuracy`
- `completed_at`

### 4. `planning_state`

字段建议：

- `target_bone`
- `target_region`
- `constraint_regions[]`
- `recommended_registration_point_order[]`
- `completed`
- `saved_at`

### 5. `registration_state`

字段建议：

- `per_bone_registration_results[]`
- `fused_navigation_space`
- `fusion_quality_metrics`
- `all_required_bones_registered`
- `completed_at`

单个分骨结果至少包含：

- `bone_asset_id`
- `bone_region_id`
- `point_count`
- `success`
- `fre`
- `target_tre`
- `coverage_score`
- `transform_matrix`
- `completed_at`

### 6. `navigation_state`

字段建议：

- `tracker_connected`
- `active_tool_id`
- `tool_visible`
- `running`
- `confidence`
- `allow_navigation`
- `blocking_reasons[]`
- `latest_pose_summary`
- `has_run_record`

### 7. `evaluation_state`

字段建议：

- `error_metrics`
- `per_bone_quality_summary[]`
- `navigation_process_summary`
- `report_ready`
- `exportable_artifacts[]`

## 五环节页面规则

### 1. 通用规则

- 所有页签可进入
- 页签分为“查看态”和“可执行态”
- 前置条件不满足时：
  - 动作按钮禁用
  - 页面结果区显示未就绪原因
  - 右侧状态栏显示当前阻塞摘要

### 2. 准备页

目标：

- 确定活动骨和活动器械
- 完成探针和所有参与导航器械的几何绑定与标定

最低交互要求：

- 选择活动骨集合
- 选择参与导航器械
- 为每个器械查看或确认几何文件
- 为每个器械逐个启动标定、采样、完成标定

完成条件：

- 所有必需器械都有几何文件绑定
- 所有必需器械标定完成

### 3. 规划页

目标：

- 输出导航准备型规划结果

最低交互要求：

- 选择目标骨
- 定义目标区域
- 定义解剖约束区
- 生成或确认推荐配准点顺序

完成条件：

- 目标骨存在
- 目标区域存在
- 推荐配准点顺序存在

### 4. 配准页

目标：

- 逐骨/逐部位完成配准并融合

最低交互要求：

- 选择当前配准目标骨或目标部位
- 执行采点和配准
- 查看单骨结果
- 执行融合
- 查看融合质量指标

完成条件：

- 所有要求的骨或骨部位都已成功配准
- 融合导航空间生成成功

### 5. 导航页

目标：

- 在单一 3D 虚拟空间中执行实时导航

页面形式：

- 一个 3D 虚拟空间
- 不要求四视图

最低显示要求：

- 骨骼 STL
- 当前探针/器械 STL
- 实时位姿变化

最低交互要求：

- 启动导航
- 暂停或停止导航
- 查看当前工具和跟踪状态

### 6. 评估页

目标：

- 形成完整链路评估结果

最低显示要求：

- 误差指标
- 分骨/分部位配准质量
- 导航过程摘要
- 报告导出入口

## 阶段门禁 V2

### 1. 准备

允许浏览条件：

- 当前病例已进入导航工作区

允许执行条件：

- 病例工作包有效
- 已绑定至少一个骨骼资产
- 已绑定至少一个参与导航的探针/器械

### 2. 规划

允许执行条件：

- 活动骨集合存在
- 相关骨模型可用

### 3. 配准

允许执行条件：

- 规划完成
- 参与配准的探针/器械均已标定

### 4. 导航

允许执行条件：

- 融合导航空间可用
- 跟踪设备在线
- 当前工具可见
- `confidence` 达标

### 5. 评估

允许执行条件：

- 存在导航运行记录，或已有可评估结果

## 服务配准黑盒边界

V2 继续将服务配准按黑盒能力接入。

工作区编排层只关心：

- 输入骨或骨部位上下文
- 输入规划结果与采点结果
- 输出单骨配准结果
- 输出融合导航空间和融合质量指标
- 输出失败原因

V2 不要求：

- 暴露算法内部步骤
- 支持多算法对比
- 支持实验参数面板

## 持久化建议

### 正式结果

- `preparation/preparation_state.json`
- `planning/planning.json`
- `registration/per_bone_registration_results.json`
- `registration/fused_navigation_space.json`
- `navigation/navigation_run.json`
- `evaluation/evaluation_report.json`

### 页面快照

- `navigation/workspace_snapshot.json`

快照中只保留：

- 当前页签
- 当前活动骨
- 当前活动器械
- 最近一次标定摘要
- 最近一次配准摘要
- 最近一次导航摘要
- 最近一次门禁阻塞原因

## 恢复策略

工作区恢复顺序：

1. 加载病例工作包
2. 加载正式阶段结果
3. 加载页面快照
4. 重新从实时服务读取跟踪状态
5. 统一重算阶段门禁
6. 刷新 UI

约束：

- 不复用历史快照中的旧门禁结论
- 不因为页面恢复而假定当前仍可导航

## 与现有代码的映射

### 需要保留并升级

- `NavigationWorkspaceApplicationService`
- `NavigationWorkspaceSnapshotStore`
- `NavigationWorkspaceUiBinder`
- `NavigationWorkflowCoordinator`
- `NavigationRuntimeCoordinator`
- `NavigationEvaluationController`

### 需要重点扩展

- `navigation_workspace_types.h`
- `registration_controller.*`
- `preparation_planning_controller.*`
- `NavigationPage.*`

### 需要重点收缩

- `NavigationPage` 中直接拼装全链路状态的旧逻辑
- 导航页中对多视图的默认依赖
- 单结果配准假设

## 验收标准

V2 完成后必须满足：

1. `NavigationWorkspaceApplicationService` 成为导航工作区唯一业务聚合入口
2. `NavigationPage` 不再维护分散的全链路真源
3. 准备页支持多器械几何绑定和逐个标定
4. 规划页输出目标骨、目标区域、约束区和推荐配准点顺序
5. 配准页输出分骨/分部位结果集与融合导航空间
6. 导航页为单一 3D 虚拟空间，并实时显示骨骼和当前工具位姿
7. 评估页输出误差指标、分骨质量、导航过程摘要和导出能力
8. 所有页签可进入，但动作受统一门禁控制
9. 工作区恢复后必须基于正式结果和实时状态重新判定是否可继续

## 成功判定

这份设计成功落地的标志是：

- 导航工作区从“导航页功能集合”升级为“病例驱动的工作流编排器”
- 多骨、多部位、分骨配准与融合导航空间正式进入工作区模型
- 页面门禁、结果持久化和恢复语义保持一致
- 单一 3D 虚拟空间导航页成为明确的首版目标形态
