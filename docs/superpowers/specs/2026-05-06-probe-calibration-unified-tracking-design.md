# Probe Calibration Unified Tracking Design

日期：2026-05-06

范围：
- `Plugins/OpticalTracking/OpticalTrackingServiceImpl.*`
- `Plugins/OpticalTracking/OpticalTrackingService.h`
- `algorithms/probe_calibration/include/probe_calibration_c_api.h`
- `algorithms/probe_calibration/src/probe_calibration_c_api.cpp`
- `algorithms/probe_calibration/include/realtime_transform.h`
- `algorithms/probe_calibration/src/realtime_transform.cpp`
- `algorithms/probe_calibration/include/calibration_recorder.h`
- `algorithms/probe_calibration/src/calibration_recorder.cpp`
- `tests/unit/*ProbeCalibration*`
- 相关运行时文档与调试文档

目标：
- 将探针标定改为“上层统一 tracking 数据源，DLL 只负责标定计算与结果输出”的单数据源架构。
- 消除 `OpticalTrackingServiceImpl` 对 `ProbeCalibration.dll` 的错误集成方式，不再把 `Collector API` 当作 pivot 标定输入。
- 让标定结果能够以结构化方式从 DLL 返回到上层，并被稳定应用到导航链路。
- 明确 geometry 文件解析与使用规则，禁止静默使用不透明默认值掩盖配置错误。
- 提升实机调试可观测性，明确区分真设备、模拟设备、DLL 路径、geometry 路径、标定执行路径和回退路径。

## 1. 背景

当前探针标定主目标是依赖 `algorithms/probe_calibration` 导出的 `ProbeCalibration.dll`，但现有集成方式存在结构性错位：

- `OpticalTrackingServiceImpl` 自己维护一套 tracking 会话与工具位姿采样。
- `ProbeCalibration.dll` 内部又维护一套 `ProbeTrackingPipeline`，其内部还会自行初始化 tracker、加载 geometry、启动 tracking、接收 `PoseData` 并录制标定样本。
- 上层在调用 DLL pivot 标定时，同时又把 `calibInfo.calibrationPoints` 通过 `PC_CollectorAddPoint` 喂给 DLL。

这导致三条数据语义被混在一起：

1. 上层自己的工具位姿流。
2. DLL 内部 tracker 回调收到的真实 `PoseData`。
3. DLL 的 super-point collector 输入点。

其中第 3 条链并不是 pivot 标定录入接口，却被上层拿来参与 pivot 流程，导致：

- 标定是否真的基于同一套 tracker 数据源不清晰。
- 上层以为自己在给 DLL 喂标定点，实际上 DLL 标定内部仍依赖自己的 tracker pose callback。
- 标定结果没有以显式 `tipOffset` 结构返回给上层，后续结果应用链存在语义错位风险。

## 2. 当前问题归因

### 2.1 双 tracking ownership

- `OpticalTrackingServiceImpl` 负责平台 tracking。
- `ProbeCalibration.dll` 当前设计也负责 tracker 生命周期。

这使“显示位置”“标定输入”“标定输出应用后的导航位置”可能来自不同链路。

### 2.2 标定输入接口使用错误

- `PC_CollectorAddPoint` 的语义是 super-point collection。
- 该接口不应作为 pivot 标定的主输入接口。
- Pivot 标定需要完整 pose，而不是只有 XYZ 点。

### 2.3 标定结果输出不完整

现有上层只知道 DLL 成功或失败，没有稳定获取：

- `tipOffset`
- `residualError`
- `numPosesUsed`
- `geometryId`
- 标定质量统计

这导致 `applyCalibrationResult()` 只能依赖不完整结果或模糊回退逻辑。

### 2.4 geometry 规则不透明

当前 geometry 解析顺序是：

1. `toolConfigurations[toolId].geometryFile`
2. `toolConfigurations[toolId].geometryId`
3. fallback 到 `072`

该回退会让缺失配置在实机时表现为“能继续跑，但结果可能错”，这不符合实机标定的失败优先原则。

### 2.5 设备状态可观测性不足

当前扫不到真设备时会自动加入模拟设备，容易在实机验证中造成“连接成功”的假象。

## 3. 设计目标

### 3.1 主目标

- 整个系统只保留一套 tracker 数据源，上层负责 tracker 生命周期与实时位姿流。
- `ProbeCalibration.dll` 改为“纯标定引擎”，接收上层传入的 pose 数据，输出结构化标定结果。

### 3.2 辅助目标

- geometry 必须显式可见、可追踪、可验证。
- 实机模式中禁止无提示模拟回退掩盖连接问题。
- 标定失败必须可解释，不允许静默失败或静默回退。

### 3.3 非目标

- 本次不重写整个 `OpticalTrackingServiceImpl` 导航渲染架构。
- 本次不重构所有工具管理数据库结构。
- 本次不引入新的硬件 SDK。
- 本次不改变现有 pivot 数学求解主公式，重点是重构数据流与集成边界。

## 4. 方案选择

本次采用方案 C：统一 tracking 架构。

核心原则：

- 上层负责 tracker。
- DLL 不再创建、不再拥有、不再驱动 tracker。
- DLL 只接收上层提供的 pose 样本与配置。
- DLL 只输出标定结果，不直接参与上层实时导航状态机。

选择理由：

- 从根源上消除双 tracking ownership。
- 让 pivot 标定和导航共用同一数据源，避免“显示看到的是 A，标定算的是 B”。
- 为后续实机调试、日志追踪、自动化测试提供稳定接口。

## 5. 新架构设计

### 5.1 上层职责

`OpticalTrackingServiceImpl` 负责：

- 设备发现、连接、断开
- 真设备与模拟设备状态区分
- 维护工具位姿流
- 收集 pivot 标定所需完整 pose 样本
- 解析并验证 geometry 路径
- 调用 DLL 标定接口
- 将 DLL 输出的 `tipOffset` 等结果应用到导航链路

### 5.2 DLL 职责

`ProbeCalibration.dll` 负责：

- 保存一次标定会话的录制状态
- 接收完整 pose 样本
- 执行 pose 质量过滤与去重
- 执行 robust pivot calibration
- 返回结构化标定结果

DLL 不再负责：

- 枚举设备
- 初始化 tracker SDK
- 启停跟踪线程
- 从几何文件之外的任何运行时路径隐式发现设备

### 5.3 数据流

新数据流如下：

1. 上层建立 tracking session。
2. 上层为目标 tool 明确解析 geometry。
3. 用户开始 pivot 标定。
4. 上层调用 DLL `StartCalibrationSession` 类接口，传入 geometry 元数据与标定配置。
5. 上层在自己的实时 pose 回调中，将目标工具的完整 pose 连续送入 DLL。
6. DLL 过滤、录制、统计并在结束时执行求解。
7. DLL 返回结构化 `CalibrationResult`。
8. 上层验证结果，写回 `toolConfigurations` 和 `m_toolTrackingData`。
9. 导航继续使用现有上层 tracking 数据流加上新 `tipOffset`。

## 6. DLL API 设计调整

### 6.1 保留接口

以下接口可以保留，但语义需要收敛：

- `PC_CreatePipeline`
- `PC_DestroyPipeline`
- `PC_StartCalibration`
- `PC_FinishCalibration`
- `PC_SaveCalibration`
- `PC_LoadCalibration`
- `PC_GetLastError`

### 6.2 弃用接口

以下接口不再参与 pivot 标定主流程：

- `PC_InitializePipeline(handle, geometry_path)` 的“内部 tracker 初始化”语义
- `PC_GetTipPose`
- `PC_CollectorReset`
- `PC_CollectorAddPoint`
- `PC_CollectorGetSuperPointCount`
- `PC_CollectorExport`

说明：

- `Collector API` 可以继续保留给点云融合/仿真等其他用途，但不得再被上层 pivot 标定流程使用。
- `PC_GetTipPose` 在统一 tracking 架构下不是标定结果主输出接口，它描述的是实时 tip pose，不是 `tipOffset` 标定结果。

### 6.3 新增接口

DLL 需要新增面向统一 tracking 的 C API：

- `PC_ConfigureGeometry(handle, const char* geometry_path, uint32_t geometry_id_optional)`
- `PC_ResetCalibrationSession(handle)`
- `PC_AddPoseSample(handle, const PC_PoseSample* sample)`
- `PC_GetCalibrationResult(handle, PC_CalibrationResult* out_result)`
- `PC_GetCalibrationStats(handle, PC_CalibrationStats* out_stats)`

其中：

`PC_PoseSample` 需要至少包含：

- `geometry_id`
- `timestamp_us`
- `registration_error`
- `is_valid`
- `4x4 transform` 或 `3x3 rotation + 3 translation`

`PC_CalibrationResult` 需要至少包含：

- `tip_offset_x/y/z`
- `residual_error`
- `num_poses_used`
- `is_valid`
- `geometry_id`

`PC_CalibrationStats` 需要至少包含：

- `total_received`
- `total_accepted`
- `rejected_invalid`
- `rejected_high_error`
- `rejected_similar`
- `angular_coverage`
- `mean_registration_error`

### 6.4 API 原则

- 新接口必须是纯数据输入输出。
- 不允许在 `AddPoseSample` 中隐式访问硬件。
- 不允许在 `ConfigureGeometry` 中隐式连接设备。
- 不允许任何未显式传入的 pose 数据进入求解器。

## 7. geometry 规则

### 7.1 唯一来源

geometry 解析由上层统一完成，DLL 只接受上层传入的最终 geometry 信息。

### 7.2 解析顺序

上层仍可按以下优先级解析：

1. `geometryFile`
2. `geometryId`

但本次设计要求：

- 不允许在实机标定主流程中静默 fallback 到 `072`
- 若未配置 geometry，则直接失败

### 7.3 日志要求

每次开始标定前都必须记录：

- `sessionId`
- `toolId`
- geometry 来源类型：`geometryFile` / `geometryId`
- geometry 原始配置值
- geometry 最终绝对路径
- geometry 是否通过验证

### 7.4 实机失败策略

若 geometry 缺失、不可读或解析无效：

- DLL 不得初始化
- 标定流程直接失败
- UI 与日志同时给出明确错误

## 8. 设备与仿真状态规则

### 8.1 状态分离

系统必须明确区分：

- `physical_connected`
- `physical_not_found`
- `simulation_available`
- `simulation_active`

### 8.2 实机模式原则

当用户进入实机标定流程时：

- 若当前无真设备，不允许默认切换成模拟设备继续伪装成功
- 必须显式提示当前设备状态

### 8.3 日志要求

每次连接与标定时都要记录：

- 当前使用的是 `physical` 还是 `simulation`
- 设备序列号
- device type
- tracking session id

## 9. 上层应用结果设计

### 9.1 结果应用

`applyCalibrationResult()` 只接受结构完整的 DLL 标定结果：

- `tipOffset`
- `accuracy` / `residualError`
- `geometryId`

不得再使用 `pivotPoint` 冒充 `tipOffset`。

### 9.2 数据持久化

上层应在 tool configuration 中记录：

- `calibrated = true`
- `calibrationTime`
- `calibrationAccuracy`
- `calibrationGeometryPath`
- `calibrationGeometryId`
- `calibrationSource = "ProbeCalibrationDLL"`

### 9.3 会话缓存

`m_toolTrackingData[sessionId][toolId]` 中缓存的 offset 必须来自 DLL 的结构化输出，而不是从实时 tip pose 逆推。

## 10. 质量与验证要求

### 10.1 DLL 录制规则

统一 tracking 架构下，DLL 仍沿用高质量 pivot 录制策略：

- 最少 pose 数
- registration error 上限
- 最小姿态变化阈值
- 最小总角度覆盖

这些阈值可配置，但不得在上层和 DLL 各自维护一套互相矛盾的默认值。

### 10.2 单元测试

至少补充以下测试：

- 上层 geometry 解析失败时，DLL 标定不会继续
- 上层不再调用 `CollectorAddPoint` 参与 pivot 标定
- DLL 新结果接口能返回 `tipOffset`
- `applyCalibrationResult()` 在缺失 `tipOffset` 时失败
- 实机模式下无真设备不会静默落入模拟成功

### 10.3 集成测试

至少补充以下集成测试：

- 模拟一组完整 pose 样本输入 DLL，可得到有效 `tipOffset`
- `OpticalTrackingServiceImpl` 能将 DLL 结果写回工具配置
- 统一 tracking 数据源下，标定前后导航链的 tip offset 应用一致

### 10.4 手工验证

实机验证需覆盖：

- 明确 geometry 配置的探针标定成功
- 缺失 geometry 时标定直接失败
- 真设备断开时不出现模拟伪成功
- 标定完成后重复点测试稳定

## 11. 迁移策略

### 11.1 第一阶段

- 新增 DLL pose sample/result API
- 保持旧 collector API 存在，但标注为非 pivot 主流程接口

### 11.2 第二阶段

- 上层切换到新统一 tracking 集成
- 删除 `performPivotCalibrationDLL()` 中对 collector 的错误调用

### 11.3 第三阶段

- 清理旧的 DLL tracker ownership 代码路径
- 将 `ProbeTrackingPipeline` 改造成不再依赖内部 `AtracsysTracker`

## 12. 风险

### 12.1 API 兼容风险

新增 C API 会影响：

- DLL 导出符号
- 上层动态加载逻辑
- 现有 smoke test

需要同步更新单测和加载校验。

### 12.2 迁移期双路径风险

在旧接口和新接口并存阶段，最容易出现：

- 某些流程仍走旧 collector 路径
- 某些流程已经走新 pose sample 路径

因此日志必须明确标出当前标定执行路径。

### 12.3 实机联调风险

统一 tracking 后，pose 时间戳、geometryId、registration_error 的字段语义必须和 DLL 内部求解预期完全一致，否则会引入新的标定误差。

## 13. 成功判定标准

- Pivot 标定全流程只依赖一套上层 tracking 数据源。
- `OpticalTrackingServiceImpl` 不再使用 collector API 驱动 pivot 标定。
- DLL 能显式输出 `tipOffset` 和标定质量结果。
- 上层能稳定应用 `tipOffset` 到导航链路。
- 标定所用 geometry 文件在日志中唯一明确、可追踪。
- 实机模式下真设备与模拟设备状态不会再混淆。

