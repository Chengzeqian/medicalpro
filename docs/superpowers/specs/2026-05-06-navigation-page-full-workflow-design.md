# Navigation Page Full Workflow Design

日期：2026-05-06

## 背景

当前 `NavigationPage` 的 UI 外壳已经重构为左侧流程栏、中部工作区、右侧状态栏的工作站布局，页面内部也已经接入了部分真实服务和运行时能力，包括：

- 病例上下文传入与阶段切换
- 器械相关操作入口
- DICOM/骨骼模型相关入口
- 探针标定入口
- 光学点配准入口
- 导航启动入口
- 评估快照和导出入口

但这些能力目前仍然是“页面内局部动作”，还没有围绕单个病例形成统一业务闭环。用户可以看到 UI，也可以触发部分动作，但“病例 -> 器械准备 -> 标定 -> 规划 -> 配准 -> 导航 -> 评估”这条链路并没有被统一状态、统一门禁、统一持久化和统一恢复机制串起来。

当前代码中的主要边界如下：

- `NavigationPage.cpp` 已经承载大量 UI、服务调用、运行态和持久化逻辑
- `NavigationWorkflowContext` 目前只保存病例身份和当前阶段，无法表达完整工作区状态
- `NavigationRuntimeState` / `NavigationRuntimeCoordinator` 目前主要负责配准结果、跟踪质量、confidence 和评估报告快照
- `AnkleCaseWorkspaceRepository`、`AnklePlanningService`、`NavigationEvaluationService` 已经提供了工作区目录、规划结果、评估结果等持久化基础
- 真实链路服务来源已经存在：`InstrumentManagementService`、`OpticalTrackingService`、`PointRegistrationService`

因此，首版问题不是“再补几个按钮”，而是缺少一个真正的病例工作区编排层。

## 首版范围

### 目标

首版只解决当前最接近可用的真实链路，并把它做成软件内可操作的闭环：

- 导航页只从已有病例工作台进入
- 导航页直接依赖真实服务，不做本地伪闭环回退
- 配准链路只覆盖“光学点配准 + 探针标定 + 导航”
- 首版同时覆盖自动恢复/持久化和流程门禁

### 非目标

本次不纳入以下范围：

- 导航页内部新建病例、选择病例、切换病例
- 2D-3D 配准链路接入
- 为服务缺失场景提供本地文件级假闭环
- 重写现有跟踪、配准或评估算法
- 改造插件框架或 CTK 生命周期
- 大规模拆分 `NavigationPage.cpp` 之外的无关模块

## 设计目标

首版设计需要满足以下结果：

1. 用户从病例工作台进入导航页后，能看到当前病例的全链路状态，而不是零散状态文本
2. 页面内所有关键阶段都有统一准入判断，不能进入的阶段必须有明确阻塞原因
3. 页面关闭再打开时，能恢复当前病例上次的导航工作区状态
4. 恢复后必须以本次真实服务状态重新计算准入，不能盲信旧快照
5. 页面不再自己维护多份彼此脱节的状态真源

## 方案选择

### 方案 A：继续在 `NavigationPage.cpp` 内直接补链路

优点：

- 起步快
- 代码都在一个文件里

缺点：

- 状态、门禁、持久化会继续堆到页面类里
- 当前文件已经过重，再堆首版全链路会进一步失控
- 自动恢复和门禁逻辑会继续散落在槽函数中

### 方案 B：新增“工作区应用服务 + 页面绑定层”

优点：

- 页面只负责展示和动作转发
- 全链路状态有统一聚合模型
- 自动恢复、门禁、服务状态同步都能集中处理
- 能复用现有 repository / planning / evaluation 基础设施

缺点：

- 需要补充一层状态模型和 application service

### 方案 C：直接重做完整导航域服务中心

优点：

- 长期最干净

缺点：

- 范围明显超出首版
- 风险过高，不适合当前阶段

### 结论

采用方案 B。

即：保留现有页面、控制器和基础服务，新增一层专门服务于单病例导航工作区的应用服务与状态模型，把真实服务主链、持久化和门禁统一编排起来。

## 总体架构

首版架构分三层：

### 1. 页面层：`NavigationPage`

职责：

- 显示左侧流程、中央工作区、右侧状态栏
- 转发用户动作
- 展示门禁状态、错误信息和恢复结果
- 响应 application service 输出的统一快照

页面层不再承担完整业务状态真源职责。

### 2. 应用层：`NavigationWorkspaceApplicationService`

这是首版新增核心。

职责：

- 加载当前病例工作区
- 聚合 manifest、planning、registration、navigation、evaluation 等结果
- 同步真实服务状态
- 生成统一工作区快照
- 计算阶段门禁
- 驱动工作区快照持久化
- 在病例重新进入时恢复工作区状态

这一层是导航页唯一应消费的业务状态入口。

### 3. 基础设施层：现有服务与仓储

继续复用：

- `AnkleCaseWorkspaceRepository`
- `AnklePlanningService`
- `NavigationEvaluationService`
- `NavigationRuntimeCoordinator`
- `InstrumentManagementService`
- `OpticalTrackingService`
- `PointRegistrationService`

其中：

- repository / planning / evaluation 继续保存正式业务结果
- runtime coordinator 继续处理 confidence / evaluation 计算
- application service 负责把这些能力编排成导航工作区闭环

## 工作区状态模型

首版新增统一聚合快照：`NavigationWorkspaceSnapshot`。

它不是替代正式结果文件，而是页面消费的统一视图模型。

建议包含以下子状态：

### `case_context`

- `case_id`
- `patient_id`
- `patient_name`
- `current_stage`
- `last_updated_at`

### `preparation_state`

- 病例上下文是否有效
- DICOM 是否已选
- 骨骼相关模型是否就绪
- 已选器械 ID
- 已选导航工具 ID
- 已选 geometry 文件/geometry 标识
- `InstrumentManagementService` 是否可用
- `OpticalTrackingService` 是否可用
- `PointRegistrationService` 是否可用

### `planning_state`

- 规划是否存在
- 规划骨骼是否完整
- 关键点/目标区是否完整
- 最近保存时间

### `calibration_state`

- 标定是否开始
- 已采样点数
- 要求点数
- 标定是否完成
- 标定误差
- 标定对应工具 ID
- 标定完成时间

### `registration_state`

- 点数量
- 点配准是否成功
- `fre`
- `target_tre`
- `coverage_score`
- 配准矩阵是否有效
- 最近成功时间

### `navigation_state`

- 追踪器是否连接
- 导航工具是否可见
- 导航是否运行中
- 当前 confidence 分数
- 当前是否允许进入导航
- 当前阻塞原因列表

### `evaluation_state`

- 是否存在导航运行记录
- 是否存在评估报告
- 最近一次评估摘要
- 导出是否可用

## 流程门禁设计

首版不允许再把阶段准入判断散落在槽函数里。

新增统一门禁模型：`NavigationStageGate`。

每个阶段至少输出：

- `allow_enter`
- `blocking_reasons`
- `status_tone`

### 门禁规则

#### 准备阶段

允许进入条件：

- 已传入病例上下文

阻塞原因：

- 未从病例工作台传入有效病例

#### 规划阶段

允许进入条件：

- 病例上下文有效
- DICOM/骨骼基础资产已就绪

阻塞原因示例：

- 当前无病例上下文
- 未加载病例影像
- 骨骼模型未准备完成

#### 标定动作

允许执行条件：

- `OpticalTrackingService` 可用
- 已连接追踪器
- 已选定导航工具/器械
- geometry 已明确

阻塞原因示例：

- 跟踪服务不可用
- 跟踪器未连接
- 未选择导航工具
- 当前工具缺少 geometry 配置

#### 配准阶段

允许进入或执行条件：

- 标定已完成
- 规划结果存在
- 点数量达到要求
- `PointRegistrationService` 可用

阻塞原因示例：

- 探针标定未完成
- 规划目标区缺失
- 配准点数量不足
- 点配准服务不可用

#### 导航阶段

允许进入或启动条件：

- 追踪器在线
- 导航工具可见
- 探针标定通过
- 点配准成功
- confidence 达标

阻塞原因示例：

- 跟踪器未连接
- 导航工具不可见
- 标定误差超阈值
- 配准失败
- confidence 不满足准入标准

#### 评估阶段

允许进入条件：

- 存在导航运行记录或评估快照

阻塞原因示例：

- 还没有有效导航运行数据

### UI 落点

门禁结果必须同时反映到三处：

1. 左侧流程栏
   - 显示阶段可进入、当前阶段、被阻塞阶段

2. 中央工作区按钮
   - 对不允许执行的动作直接禁用
   - 为按钮或说明文本显示阻塞原因

3. 右侧状态栏
   - 固定显示当前总阻塞原因和 readiness 摘要

## 自动恢复与持久化设计

首版采用“轻聚合、重引用”策略。

原则：

- 正式业务结果继续保存到现有正式文件
- 工作区快照只保存页面恢复所需的最小事实
- 页面恢复后必须重新基于真实服务状态计算门禁

### 继续沿用的正式结果文件

- `case_manifest.json`
- `planning/planning.json`
- `registration/registration_result.json`
- `navigation/navigation_run.json`
- `evaluation/evaluation_report.json`

### 新增文件

- `navigation/workspace_snapshot.json`

它只保存页面恢复必须知道的轻量状态：

- 当前阶段
- 已选器械 ID
- 已选导航工具 ID
- 已选 geometry 信息
- 已加载骨骼/模型路径引用
- 标定状态摘要
- 配准状态摘要
- 导航状态摘要
- 最近一次门禁阻塞原因
- 最近同步时间

### 恢复流程

恢复顺序固定如下：

1. 病例工作台调用 `setCaseContext`
2. application service 读取 manifest、planning、registration、navigation、evaluation、workspace snapshot
3. 生成统一 `NavigationWorkspaceSnapshot`
4. 重新计算全部阶段门禁
5. 页面一次性刷新流程栏、工作区和状态栏
6. 如果真实服务当前在线，再以实时服务状态覆盖磁盘快照中的运行时状态

### 恢复原则

- 磁盘快照用于恢复“上次做到哪一步”
- 真实服务状态用于判定“本次还能不能继续”
- 旧快照中的 allow/deny 结论不能直接沿用

## 页面与应用层交互

页面不直接拼装工作区快照。

建议由 application service 提供统一接口，例如：

- `loadWorkspace(caseId, patientId, patientName)`
- `refreshWorkspace()`
- `selectInstrument(instrumentId)`
- `selectNavigationTool(toolId)`
- `startCalibration()`
- `captureCalibrationSample()`
- `finishCalibration()`
- `acceptRegistrationResult(result)`
- `startNavigation()`
- `stopNavigation()`
- `exportEvaluationSummary()`
- `workspaceSnapshot()`
- `stageGates()`

页面只做：

- 调用命令
- 订阅状态变化
- 刷新 UI

## 文件边界

首版建议新增或扩展以下模块：

### 新增

- `Framework/Navigation/navigation_workspace_types.h`
  - 定义 `NavigationWorkspaceSnapshot`、阶段门禁类型、子状态结构

- `Framework/Navigation/navigation_workspace_application_service.h/.cpp`
  - 统一工作区编排、恢复、持久化和门禁计算

- `Framework/Navigation/navigation_workspace_snapshot_store.h/.cpp`
  - 负责 `navigation/workspace_snapshot.json` 读写

### 修改

- `UI/NewPages/NavigationPage.h/.cpp`
  - 页面改为消费统一工作区快照和门禁结果
  - 页面动作改为调用 application service

- `UI/NewPages/Navigation/navigation_workflow_context.h/.cpp`
  - 保持轻量身份上下文，不再承担完整状态

- `UI/NewPages/Navigation/navigation_workflow_coordinator.h/.cpp`
  - 改为配合 application service 做阶段推进和准入检查

- `UI/NewPages/Navigation/navigation_runtime_coordinator.h/.cpp`
  - 继续负责 confidence / evaluation 计算，但由 application service 驱动

- `Framework/Navigation/ankle_case_workspace_repository.*`
  - 必要时补充 manifest 更新辅助接口

## 测试策略

首版至少要覆盖三类测试。

### 1. 工作区状态与门禁单测

验证：

- 病例上下文缺失时不能进入后续阶段
- 标定未完成时不能执行点配准
- 配准未成功时不能启动导航
- 快照恢复后会重新基于实时状态计算门禁

### 2. 持久化单测

验证：

- `workspace_snapshot.json` 正确写入和读取
- 正式结果文件缺失时，快照恢复行为可解释
- 快照存在但服务离线时，页面显示阻塞原因而不是假成功

### 3. 页面契约测试

验证：

- `NavigationPage` 不再把全链路状态真源散落在页面成员中
- 页面会基于统一快照刷新右侧状态栏
- 左侧流程栏会反映门禁状态

## 验收标准

首版完成后应满足：

- 用户只能从病例工作台进入导航页，页面不提供选病例入口
- 导航页能展示单病例全链路状态摘要
- 器械准备、标定、配准、导航、评估的关键状态在页面内可见
- 阶段切换和动作执行都受统一门禁控制
- 阻塞原因在页面内可见，不只靠弹框
- 重进同一病例导航页时可自动恢复到上次工作区状态
- 恢复后会基于本次真实服务状态重新判定是否允许继续导航
- 首版只覆盖光学点配准链路，不引入 2D-3D 配准流程

## 风险与约束

### 1. 页面仍然很重

即使新增 application service，首版 `NavigationPage.cpp` 仍然会比较大，因此后续仍需要继续拆分。但本次不做超范围重构。

### 2. 真实服务可用性决定闭环完整度

本次选择真实服务主链，不做本地回退，因此服务缺失不是异常边角，而是首版门禁的一部分，必须在 UI 明确表达。

### 3. 持久化与实时状态可能短暂不一致

例如上次退出前标定完成，但本次进入时追踪器未连接。这里必须以实时状态重新算门禁，不能把旧的“可导航”状态直接恢复成当前可导航。

## 成功判定

这份设计成功落地的标准是：

- 页面从“有 UI 的多个局部动作”升级成“有统一状态、统一门禁、统一恢复”的病例工作区
- 真正打通“病例工作台 -> 准备 -> 标定 -> 光学点配准 -> 导航 -> 评估”
- 不再依赖页面私有成员拼接整条链路状态
- 页面重进后能恢复工作现场，并明确告诉用户当前能否继续
