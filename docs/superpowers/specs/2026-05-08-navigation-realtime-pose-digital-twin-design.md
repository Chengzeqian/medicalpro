# Navigation Realtime Pose Digital Twin Design

**日期：** 2026-05-08  
**范围：** `medicalpro` 踝关节导航系统中的实时位姿链路、坐标变换链、单窗口数字孪生渲染与基础实时评估  
**目标：** 在现有病例中心化导航工作区基础上，新增一个面向单活动器械的实时位姿数字孪生框架，把模拟器或真实光学跟踪输入、器械标定结果、患者配准结果和 VTK 单窗口渲染统一到同一条可解释、可验证的多坐标系链路中。

## 1. 设计结论

本轮设计的目标不是继续扩展页面功能，而是为后续论文与系统演示建立一个稳定的实时导航主链路。

本轮明确采用以下技术路线：

- 不做 VR、AR、MR 显示
- 不做深度学习 6DoF 位姿估计
- 不做 Marker-less tracking
- 不做 RGBD、多视角或 Foundation Model 路线
- 保留光学跟踪或模拟跟踪、器械标定、分骨配准、VTK 3D 同步显示、延迟抖动评估

本轮创新点收敛为：

> 面向踝关节手术导航的多坐标系实时器械跟踪与虚拟空间同步方法

系统表达上可统一描述为：

> 基于光学或模拟跟踪与多坐标系变换链的实时数字孪生手术导航系统

## 2. 设计目标

本轮目标：

1. 建立统一的实时位姿帧模型，使模拟器输入和未来真实光学跟踪输入共享同一接口
2. 建立固定的多坐标系链路，稳定计算活动器械在 VTK 世界坐标系下的最终姿态
3. 在导航单窗口中显示骨骼 STL 和一个活动器械 STL，并让器械模型实时同步运动
4. 把延迟、抖动、可见率等实时指标接入现有评估体系，但不污染主实时链路
5. 让现有 `NavigationRuntimeCoordinator` 和 `NavigationVtkBridge` 升级为数字孪生导航的运行时骨架

本轮非目标：

- 不支持多器械同时实时显示
- 不建立可配置任意图的通用变换图引擎
- 不在本轮引入独立的 fused navigation space 层
- 不在本轮扩展为轨迹线、热力图、AR overlay 或术中图像叠加
- 不在本轮接入真实硬件的完整 SDK 联调，只预留接口

## 3. 第一版边界

第一版范围固定如下：

- 位姿源：统一接口，先接模拟器，后接真实光学跟踪
- 坐标链：`tracking -> calibrated tool -> patient -> vtk world`
- `patient -> vtk world`：当前阶段直接等于患者配准结果
- 单窗口显示对象：骨骼 STL + 1 个活动器械 STL
- 评估指标：`latency`、`jitter`、`visible frame ratio`、配准误差复用现有结果

第一版不做任何“猜测性回退”：

- 位姿不可见时，不伪造姿态
- 标定结果缺失时，不继续导航渲染
- 配准结果缺失时，不构造 `vtk world` 下的最终工具矩阵

## 4. 总体架构

建议新增三个核心模块，并升级两个现有模块：

### 4.1 新增 `navigation_pose_frame`

建议路径：

- `Framework/Navigation/navigation_pose_frame.h`
- `Framework/Navigation/navigation_pose_frame.cpp`

职责：

- 定义标准化的单帧位姿输入结构
- 把模拟器和未来真实光学跟踪输入统一为同一种数据格式
- 只描述“当前观察到的事实”，不做业务推理和渲染控制

### 4.2 新增 `navigation_pose_stream`

建议路径：

- `Framework/Navigation/navigation_pose_stream.h`
- `Framework/Navigation/navigation_pose_stream.cpp`

职责：

- 保存最新实时位姿帧
- 可选保存一小段历史帧用于抖动和延迟评估
- 作为实时输入缓冲层，为 runtime 和 evaluation 提供��一读取入口

### 4.3 新增 `navigation_transform_graph`

建议路径：

- `Framework/Navigation/navigation_transform_graph.h`
- `Framework/Navigation/navigation_transform_graph.cpp`

职责：

- 维护固定的多坐标系链路
- 读取跟踪原始位姿、工具标定结果和患者配准结果
- 计算最终的 `T_vtk_tool`
- 输出当前链路是否闭合、最终矩阵是否有效、失败原因是什么

### 4.4 升级 `navigation_runtime_coordinator`

现有路径：

- `UI/NewPages/Navigation/navigation_runtime_coordinator.h`
- `UI/NewPages/Navigation/navigation_runtime_coordinator.cpp`

升级职责：

- 接收实时位姿帧
- 更新运行时位姿状态
- 调用 `navigation_transform_graph` 计算最终姿态
- 将最终渲染状态分发给 `NavigationVtkBridge`
- 将旁路评估数据分发给评估模块

约束：

- 不直接持有 VTK actor 细节
- 不直接承担模型加载职责
- 作为运行时调度者，而不是 UI 逻辑容器

### 4.5 升级 `navigation_vtk_bridge`

现有路径：

- `UI/NewPages/Navigation/navigation_vtk_bridge.h`
- `UI/NewPages/Navigation/navigation_vtk_bridge.cpp`

升级职责：

- 从“VTK 容器桥”升级成“单窗口数字孪生显示桥”
- 管理导航单窗口内骨骼模型与一个活动器械模型
- 接收最终位姿矩阵和可见性状态
- 更新活动器械 actor 的实时姿态

约束：

- 不负责坐标计算
- 不负责评估逻辑
- 不负责业务门禁判断

## 5. 坐标系与变换链定义

本轮固定采用如下链路：

`tracking -> calibrated tool -> patient -> vtk world`

### 5.1 `tracking`

- 表示跟踪系统输出的原始器械位姿坐标系
- 来自模拟器或未来光学跟踪适配器
- 第一版只要求能提供一个活动器械的 6DoF 原始姿态

### 5.2 `calibrated tool`

- 表示应用器械标定结果后的工具坐标系
- 跟踪坐标系到工具尖端或工具工作参考点之间的关系由标定结果给出
- 本层的本质是把原始 marker 位姿变成可用于手术导航的器械位姿

### 5.3 `patient`

- 表示患者配准坐标系
- 来自当前病例工作区的配准结果
- 该结果应与现有病例中心化工作区中的配准真源保持一致

### 5.4 `vtk world`

- 表示导航单窗口中的虚拟显示世界坐标系
- 第一版中直接等于患者配准结果映射后的世界坐标
- 第二版如果需要引入 fused navigation space，可在 `patient -> vtk world` 之间再插一层

### 5.5 最终输出

`navigation_transform_graph` 的关键输出是：

- `T_vtk_tool`

该矩阵直接用于驱动 VTK 中活动器械 STL 的 actor 姿态更新。

## 6. 数据模型

### 6.1 `NavigationPoseFrame`

建议最小字段：

```cpp
struct NavigationPoseFrame
{
    QString sourceId;
    QString toolId;
    QDateTime timestamp;
    bool trackingVisible = false;
    double trackingConfidence = 0.0;
    QMatrix4x4 trackingToMarker;
};
```

说明：

- `sourceId`：区分模拟器、真实跟踪器或未来其他输入源
- `toolId`：当前活动器械唯一标识
- `timestamp`：评估延迟、抖动的时间基础
- `trackingVisible`：当前是否可见
- `trackingConfidence`：原始跟踪可信度
- `trackingToMarker`：跟踪系统输出的原始姿态矩阵

### 6.2 `NavigationPoseSampleWindow`

建议用于 `NavigationPoseStream` 的短时缓存：

```cpp
struct NavigationPoseSampleWindow
{
    QList<NavigationPoseFrame> recentFrames;
    int maxFrameCount = 0;
};
```

第一版只需要：

- 最新一帧
- 最近短时间窗口

不需要引入复杂订阅总线或并发流图。

### 6.3 `NavigationTransformResult`

建议由 `navigation_transform_graph` 输出：

```cpp
struct NavigationTransformResult
{
    bool trackingAvailable = false;
    bool calibrationAvailable = false;
    bool registrationAvailable = false;
    bool valid = false;
    QString failureCode;
    QString failureText;
    QMatrix4x4 vtkToolTransform;
};
```

### 6.4 `NavigationDisplayState`

建议由 `NavigationRuntimeCoordinator` 传递给 `NavigationVtkBridge`：

```cpp
struct NavigationDisplayState
{
    QString activeToolId;
    QString activeToolModelPath;
    QStringList boneModelPaths;
    bool toolVisible = false;
    bool validPose = false;
    QString statusText;
    QMatrix4x4 vtkToolTransform;
};
```

## 7. 数据流设计

第一版实时链路固定为：

`Simulator / OpticalTrackingAdapter`
`-> NavigationPoseFrame`
`-> NavigationPoseStream`
`-> NavigationTransformGraph`
`-> T_vtk_tool`
`-> NavigationRuntimeCoordinator`
`-> NavigationVtkBridge`
`-> 单窗口 VTK actor 更新`

### 7.1 位姿输入阶段

- 模拟器或未来真实光学跟踪适配器输出 `NavigationPoseFrame`
- 所有外部位姿源都不能直接进入页面或 VTK 桥
- 所有实时输入必须先经过统一帧模型

### 7.2 帧流缓冲阶段

- `NavigationPoseStream` 保存最新帧
- 可选保存短窗口历史帧
- 为 runtime 计算和评估记录提供统一读取

### 7.3 坐标链计算阶段

- `NavigationTransformGraph` 读取最新 pose frame
- 读取工具标定结果
- 读取患者配准结果
- 计算最终 `T_vtk_tool`
- 生成 `NavigationTransformResult`

### 7.4 运行时调度阶段

- `NavigationRuntimeCoordinator` 接收 `NavigationTransformResult`
- 更新 runtime state
- 构建 `NavigationDisplayState`
- 同时把时序数据和质量数据送给评估模块

### 7.5 VTK 显示阶段

- `NavigationVtkBridge` 保证骨骼 STL 已加载
- 保证活动器械 STL 已加载
- 如果 `validPose == true`，更新活动器械 actor 姿态
- 如果 `validPose == false` 或 `toolVisible == false`，隐藏器械 actor 或显示不可用状态

## 8. 现有模块集成方式

### 8.1 与病例中心化工作区集成

位姿链路不能绕开现有病例工作区真源。

必须复用现有工作区中以下信息：

- 活动骨模型路径
- 当前活动器械几何文件路径
- 当前器械标定结果
- 当前患者配准结果
- 当前病例上下文

这意味着第一版位姿数字孪生能力是现有 `case-centered` 和 `workspace orchestrator v2` 的增量能力，而不是平行新系统。

### 8.2 与 `NavigationRuntimeState` 集成

`NavigationRuntimeState` 需要补充新的运行时字段，至少包括：

- 当前最新 pose frame 是否存在
- 当前最新 transform result 是否有效
- 当前活动器械是否可见
- 当前活动器械最终位姿矩阵
- 当前位姿链路失败原因

但不建议把坐标计算本身塞回 `NavigationRuntimeState`。

### 8.3 与 `NavigationRuntimeCoordinator` 集成

建议新增方法，例如：

```cpp
void handlePoseFrame(const NavigationPoseFrame& frame);
void handleRegistrationTransform(const QMatrix4x4& patientToVtkWorld);
void refreshNavigationDisplay();
```

其中：

- `handlePoseFrame(...)` 作为实时位姿主入口
- `handleRegistrationTransform(...)` 负责同步患者配准结果
- `refreshNavigationDisplay()` 统一驱动 VTK 更新

### 8.4 与 `NavigationVtkBridge` 集成

建议新增接口，例如：

```cpp
bool loadBoneModels(const QStringList& boneModelPaths);
bool loadInstrumentModel(const QString& toolId, const QString& modelPath);
void updateInstrumentPose(const QString& toolId, const QMatrix4x4& vtkToolTransform);
void setInstrumentVisible(const QString& toolId, bool visible);
```

第一版约束：

- 可以保留 `toolId` 作为接口参数
- 但内部只需要真正支持一个活动器械实例

## 9. 错误处理

第一版只定义明确、可解释的失败状态，不做猜测性恢复。

### 9.1 `tracking_unavailable`

条件：

- 当前没有有效 pose frame
- 或 `trackingVisible == false`

行为：

- 不更新新的器械姿态
- 隐藏器械 actor
- 状态提示“跟踪不可用”

### 9.2 `calibration_missing`

条件：

- 原始位姿存在
- 但器械标定结果不存在或无效

行为：

- 不计算最终 `T_vtk_tool`
- 状态提示“器械未标定”

### 9.3 `registration_missing`

条件：

- 跟踪和标定存在
- 但患者配准结果不存在或无效

行为：

- 不构建 `vtk world` 下最终工具姿态
- 状态提示“患者配准未完成”

### 9.4 `model_missing`

条件：

- 骨骼 STL 未加载
- 或活动器械 STL 未加载

行为：

- 不进入完整导航渲染
- 状态提示“导航模型未就绪”

## 10. 评估设计

评估模块不进入主实时链路，只作为旁路消费者。

### 10.1 第一版记录指标

- `latency`
- `jitter`
- `visible frame ratio`
- 配准误差沿用现有结果，如 `TRE`、`coverage score`

### 10.2 第一版评估输入

评估模块读取：

- pose frame 时间戳
- pose frame 可见性
- transform result 是否有效
- runtime 中当前导航允许状态
- 已有 registration/evaluation metrics

### 10.3 评估输出

第一版输出要求：

- 能进入现有 `NavigationEvaluationService`
- 能进入病例级评估记录
- 能作为后续病例汇总的一部分

第一版不要求：

- 复杂报表 UI
- 实时曲线面板
- 多病例跨时间段聚合分析界面

## 11. 测试策略

### 11.1 单元测试

需要覆盖：

- `NavigationPoseStream` 最新帧与窗口缓存行为
- `NavigationTransformGraph` 在链路完整时正确输出 `T_vtk_tool`
- `NavigationTransformGraph` 在标定缺失、配准缺失、跟踪不可见时返回明确失败原因
- `NavigationRuntimeCoordinator` 在收到 pose frame 后刷新运行时状态

### 11.2 集成测试

需要覆盖：

- 模拟器输出位姿帧后，runtime 能驱动 `NavigationVtkBridge`
- 只有一把活动器械时，器械 actor 姿态能实时更新
- tracking 丢失后，器械 actor 进入隐藏或不可用状态

### 11.3 契约测试

需要补充或扩展现有导航契约测试，确保：

- 页面不直接持有坐标链计算逻辑
- `NavigationVtkBridge` 提供器械姿态更新接口
- `NavigationRuntimeCoordinator` 成为实时位姿处理入口

## 12. 第一版验收标准

### 12.1 功能验收

必须满足：

1. 能加载骨骼 STL
2. 能加载一个活动器械 STL
3. 模拟器按固定周期生成位姿帧
4. 系统能计算 `T_vtk_tool`
5. 单窗口内活动器械会随位姿实时变化
6. tracking 丢失时，器械能隐藏或进入不可用状态

### 12.2 架构验收

必须满足：

1. `NavigationPage.cpp` 不直接持有坐标链计算逻辑
2. 坐标链计算集中在 `navigation_transform_graph`
3. 运行时调度集中在 `navigation_runtime_coordinator`
4. VTK 更新集中在 `navigation_vtk_bridge`
5. 模拟器输入与未来真实光学跟踪输入共享同一 `NavigationPoseFrame` 接口

### 12.3 评估验收

必须满足：

1. 能记录连续一段时间的位姿帧
2. 能输出 `latency`、`jitter`、`visible frame ratio`
3. 指标能进入现有评估体系，而不是仅打印日志

## 13. 风险与控制

### 13.1 风险：坐标链概念继续散落在页面或服务层

控制：

- 统一由 `navigation_transform_graph` 承担坐标链计算
- 页面只消费最终显示状态

### 13.2 风险：`NavigationRuntimeCoordinator` 变成新的“大页面”

控制：

- coordinator 只做调度，不持有 VTK actor 细节
- 坐标计算、显示桥、评估记录继续分层

### 13.3 风险：第一版过早支持多器械，拖慢闭环

控制：

- 第一版只支持一个活动器械
- 接��可以带 `toolId`，实现先不做多实例复杂管理

### 13.4 风险：评估逻辑反向污染实时链路

控制：

- 评估作为旁路消费者
- 实时姿态更新不等待评估完成

## 14. 结论

本设计在现有病例中心化导航工作区基础上，新增一条清晰、可解释、可扩展的实时数字孪生位姿链路。

## Implementation Status

- 实时位姿帧：已实现（`Framework/Navigation/navigation_pose_frame.*`）
- 位姿流缓冲：已实现（`Framework/Navigation/navigation_pose_stream.*`）
- 固定多坐标系链路：已实现（`Framework/Navigation/navigation_transform_graph.*`）
- 运行时状态与协调器实时链路：已实现（`navigation_runtime_state.*`, `navigation_runtime_coordinator.*`）
- 单窗口数字孪生渲染桥：已实现（`navigation_vtk_bridge.*` 新增 `loadBoneModels/loadInstrumentModel/updateInstrumentPose/setInstrumentVisible`）
- NavigationPage 位姿采样与 VTK 桥对接：已实现（`pushSimulatedPoseFrameToRuntime` / `refreshRealtimeDigitalTwin`）
- latency / jitter / visible frame ratio 病例级导出：已实现（`navigation_evaluation_service.cpp`, `navigation_evaluation_summary_formatter.cpp`）

第一版明确采用：

- 单活动器械
- 固定多坐标系链路
- 配准结果直接定义 `patient -> vtk world`
- 单窗口显示骨骼 STL 与器械 STL
- 评估模块旁路记录实时质量指标

这条路径最适合当前系统、论文表达和演示验证，也为第二版继续扩展多器械、融合导航空间和更丰富评估提供稳定边界。
