# Ankle Navigation Algorithm Innovation Coverage Review

**日期：** 2026-05-03  
**范围：** `MeshGPU`、`ProbeCalibration` 以及主链集成代码对论文三个创新点的满足度审查  
**目标：** 形成“创新点要求 - 代码证据 - 当前状态 - 缺口 - 下一步”的专项审查矩阵，明确哪些已经实现，哪些只是接入了，哪些仍未满足论文表述。

## 1. 审查结论摘要

截至当前仓库状态，**三个创新点都不能宣称“完全完成”**。

更准确的结论是：

- 创新点 1：**实验框架和策略入口已接入，专项审查未完成**
- 创新点 2：**主链已接入解剖约束双阶段配准的部分能力，但还不能证明完全满足论文表述**
- 创新点 3：**联合准入主链已接入且页面内初步闭环，但运行时协作者边界和真实闭环证据仍不足**

因此，目前最多可以说：

- 已有主链能力
- 已有 baseline/summary/export 入口
- 已有 MeshGPU / ProbeCalibration 集成

但还不能说：

- 论文三个创新点全部完成
- MeshGPU 当前实现完全符合创新点 2 的论文表述
- ProbeCalibration 当前实现完全构成创新点 3 的真实闭环证据

## 2. 审查方法

本次审查只根据当前主仓库与已并回主线的算法目录做代码证据核对，不把外部历史项目或旧开发目录当作完成证据。

重点核查的路径：

- `Plugins/RegistrationCore/RegistrationServiceImpl.cpp`
- `Plugins/PointRegistration/*`
- `Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp`
- `Framework/Navigation/*`
- `algorithms/meshgpu/*`
- `algorithms/probe_calibration/*`

状态定义：

- **已实现**：已有清晰代码证据，且基本满足论文表述
- **已接入但未满足论文表述**：主链或算法已接入，但缺关键环节、边界或证据链
- **未实现/未完成审查**：当前还不能从仓库代码给出成立结论

## 3. 创新点 1 审查

### 3.1 论文要求

根据创新点基线设计，创新点 1 需要围绕目标区敏感点选取建立：

- `target_sensitive`
- `random`
- `uniform`
- `expert_rule`

四套可切换策略，并在统一实验框架下输出对比结果。

### 3.2 代码证据

- `Framework/Navigation/innovation_experiment_batch_runner.cpp:15`
- `Framework/Navigation/innovation_experiment_batch_runner.cpp:16`
- `Framework/Navigation/innovation_experiment_batch_runner.cpp:17`
- `Framework/Navigation/innovation_experiment_batch_runner.cpp:18`
  - 已显式列出 `target_sensitive`、`random`、`uniform`、`expert_rule`
- `Plugins/PointRegistration/PointRegistrationDataStructures.h:87`
  - 默认点选策略为 `target_sensitive`
- `Plugins/PointRegistration/registration_point_strategy_registry.cpp`
  - 已有策略注册入口
- `Plugins/PointRegistration/random_point_selection_strategy.cpp`
- `Plugins/PointRegistration/uniform_point_selection_strategy.cpp`
- `Plugins/PointRegistration/expert_rule_point_selection_strategy.cpp`
- `Plugins/PointRegistration/target_sensitive_point_selector.cpp`
  - 说明 4 套策略入口在仓库内已存在
- `Framework/Navigation/innovation_summary_csv_exporter.cpp:24`
  - 已有创新点 1 summary CSV 头定义

### 3.3 当前状态

**状态：已接入但专项审查未完成**

原因：

- 四套策略和实验输出入口已存在
- 批量实验框架也已存在
- 但本次审查重点是 `MeshGPU / ProbeCalibration`，创新点 1 尚未形成“策略实现细节 - 指标定义 - 论文要求”的逐项核查矩阵
- 当前也未见本轮专门产出的“已跑通病例 + 有效对比结果”证据汇总

### 3.4 缺口

- 还缺按论文要求逐项核对 `target_sensitive` 的评分依据是否完整
- 还缺病例级实验输出样本审查
- 还缺对“统一实验输出可直接支持论文结论”的证据汇总

### 3.5 下一步

- 单独补一轮创新点 1 策略级代码审查
- 核查 `innovation_1_summary.csv` 实际字段与论文指标是否一致
- 选取真实病例重跑至少一组 summary，确认不是只停留在框架层

## 4. 创新点 2 审查

### 4.1 论文要求

创新点 2 的目标是“踝关节解剖约束双阶段配准”，最低应包含：

- baseline 与 proposed 可切换
- 完整刚体求解
- 第一阶段粗配准
- 第二阶段 ROI / target-related refinement
- 目标区误差与覆盖度等指标输出
- 最好具备法向、曲率、局部可信度等约束证据

### 4.2 主链代码证据

- `Plugins/PointRegistration/PointRegistrationDataStructures.h:88`
  - 默认配准方法 ID 为 `ankle_two_stage_constrained`
- `Plugins/PointRegistration/PointRegistrationServiceImpl.cpp:483`
- `Plugins/PointRegistration/PointRegistrationServiceImpl.cpp:495`
- `Plugins/PointRegistration/PointRegistrationServiceImpl.cpp:544`
  - 已存在 constrained pair / constrained refine points / refine 路径
- `Plugins/PointRegistration/PointRegistrationServiceImpl.cpp:656`
- `Plugins/PointRegistration/PointRegistrationServiceImpl.cpp:664`
- `Plugins/PointRegistration/PointRegistrationServiceImpl.cpp:670`
  - 已输出 `targetRegionTre`、`coverageScore`、`registration_mode`

### 4.3 MeshGPU 接入证据

- `Plugins/RegistrationCore/RegistrationServiceImpl.cpp:349`
  - 已支持 MeshGPU DLL 加载
- `Plugins/RegistrationCore/RegistrationServiceImpl.cpp:421`
  - MeshGPU 不可用时回退到 VTK ICP
- `Plugins/RegistrationCore/RegistrationServiceImpl.cpp:510`
  - 已构造 constrained target mesh
- `Plugins/RegistrationCore/RegistrationServiceImpl.cpp:569`
  - 已构造 constrained source point cloud
- `Plugins/RegistrationCore/RegistrationServiceImpl.cpp:675`
  - 已记录 `coreConstraintApplied`
- `algorithms/meshgpu/include/mesh_gpu_runtime_api.h`
  - 已导出运行时 API
- `algorithms/meshgpu/src/mesh_gpu_interface.cu:1934`
  - 存在 coarse-to-fine registration 路径
- `algorithms/meshgpu/src/mesh_gpu_interface.cu:1716`
- `algorithms/meshgpu/src/mesh_gpu_interface.cu:1765`
- `algorithms/meshgpu/src/mesh_gpu_interface.cu:1814`
  - 存在 rotation search / hierarchical rotation search / full sphere search 能力
- `algorithms/meshgpu/src/mesh_gpu.cu:290`
  - MeshGPU 内部具备曲率计算能力

### 4.4 当前状态

**状态：已接入但未满足“已完全符合论文表述”的证明要求**

理由分两层：

1. 主链层面：

- 已有双阶段、约束点、目标区 TRE、coverage 等明显证据
- 已有 `ankle_two_stage_constrained` 入口
- 已有 GPU 路径和 VTK ICP 回退

2. 论文匹配层面：

- 当前还没完成“第一阶段权重是否确实由目标敏感采点或解剖先验驱动”的逐项核对
- 当前还没完成“第二阶段 refinement 是否严格受 ROI 约束”的完整算法证据梳理
- 当前还没把 MeshGPU 内部的曲率、法向、旋转搜索、候选打分能力与主链实际调用一一对上
- 当前也没有形成“主链当前真的用到了哪些能力，哪些只是库里存��但没有被主链消费”的明确结论

### 4.5 风险点

- `Plugins/RegistrationCore/RegistrationServiceImpl.cpp:421` 与 `Plugins/RegistrationCore/RegistrationServiceImpl.cpp:999`
  - 工具链或 DLL 不可用时会回退到 VTK ICP，这说明运行时结果未必总是走论文拟定的 GPU 约束路径
- `algorithms/meshgpu/src/main_benchmark.cu`
  - 库里存在大量 benchmark / ablation 能力，但不能自动等同于主链已使用
- `algorithms/meshgpu/src/main_visualization.cu`、`main_realtime.cu`
  - demo / tool 能力不是论文主链完成证据

### 4.6 下一步

- 单独核查 `RegistrationServiceImpl` 传给 MeshGPU 的参数集是否完整映射论文设计
- 区分“MeshGPU 内部可用能力”和“当前主链已使用能力”
- 输出创新点 2 的二级矩阵：
  - 刚体求解
  - coarse stage
  - ROI refine
  - 法向/曲率/候选打分
  - 指标输出
  - fallback 风险

## 5. 创新点 3 审查

### 5.1 论文要求

创新点 3 关注“配准-跟踪联合可信度准入”，至少应有：

- registration 与 tracking 的联合准入决策
- 标定状态参与准入
- 有明确拒绝原因和评分
- 有导航期间告警/中断或评估输出

### 5.2 主链代码证据

- `UI/NewPages/NavigationPage.cpp:1720`
  - 页面端已存在联合准入刷新入口
- `UI/NewPages/NavigationPage.cpp:1775`
  - 评估快照已可合并 tracking / registration / gate 信息
- `UI/NewPages/NavigationPage.cpp:2108`
- `UI/NewPages/NavigationPage.cpp:2115`
- `UI/NewPages/NavigationPage.cpp:2121`
- `UI/NewPages/NavigationPage.cpp:2129`
  - 页面已订阅 registration / session / tool / calibration 相关信号
- `Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp:3578`
  - 实时 tracking 结果已暴露 `calibrated`
- `Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp:3580`
  - 实时 tracking 结果已暴露 `calibration_accuracy_mm`
- `Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp:3471`
- `Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp:3473`
  - 跟踪质量已暴露 `visible_frame_ratio`、`tracking_confidence_score`
- `Framework/Navigation/innovation_summary_csv_exporter.cpp:21`
  - 创新点 3 summary 已包含 `allow_navigation`、`calibrated`、`calibration_accuracy_mm` 等字段

### 5.3 ProbeCalibration 接入证据

- `Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp:1426`
  - pivot calibration 优先使用 `ProbeCalibration.dll`
- `Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp:1433`
  - DLL 成功时会返回成功路径
- `Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp:1441`
  - DLL 失败时会回退到内置最小二乘 pivot calibration
- `algorithms/probe_calibration/src/tip_calibration_solver.cpp:331`
  - 内部明确在按 pivot residual 计算最终误差
- `algorithms/probe_calibration/include/realtime_transform.h`
  - 子系统具备 calibrated tip offset 的实时变换能力

### 5.4 当前状态

**状态：主链已接入，仍未满足“真实闭环已彻底完成”的结论**

主要原因：

- 运行时联动仍主要堆在 `NavigationPage.cpp`
- 当前还没有独立的 runtime coordinator 统一持有 calibration / tracking / gate / evaluation 状态
- 页面联动虽然能跑，但还不是稳定、清晰、可复用的闭环边界

### 5.5 明确的非证据

以下内容**不能**被当作创新点 3 已完全落地的证据：

- `Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp:3485`
- `Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp:3486`
  - replay/模拟路径直接写入 `calibrated=true` 和 `calibration_accuracy_mm=0.35`
- `algorithms/probe_calibration/src/main.cpp:66`
- `algorithms/probe_calibration/src/main_collection_test.cpp:67`
  - 仍存在旧绝对路径示例，说明有独立开发历史残留，这些不是主链证据

### 5.6 下一步

- 先完成运行时联动下沉，把创���点 3 的状态流从页面私有逻辑中抽出来
- 再补“真实数据下 calibration -> tracking quality -> gate -> evaluation”链路验证
- 将 replay/模拟路径与真实闭环证据分开标注

## 6. ProbeCalibration 单独判断

如果只问“ProbeCalibration 是否符合当前需求”，结论应分开说：

- 作为**探针 pivot calibration 子系统**：已接入，且具备 DLL 优先、内置算法回退、误差输出的基本能力
- 作为**论文创新点 3 已完全完成的证据**：不成立

原因：

- 它证明了标定算法与主链集成存在
- 但不能单独证明联合准入与真实导航闭环已经完成

## 7. MeshGPU 单独判断

如果只问“MeshGPU 是否满足创新点 2”，当前最严谨的说法是：

- `MeshGPU` 提供了明显相关的配准能力基础
- 主链也已经接入 constrained target/source 与双阶段思路
- 但还没有完成“论文要求逐条对齐”的专项核查

因此当前不能直接下结论说“MeshGPU 已完全满足创新点 2 论文表述”。

## 8. 总结矩阵

| 创新点 | 当前状态 | 可以确认的内容 | 不能确认的内容 |
|---|---|---|---|
| 创新点 1 目标区敏感点选取 | 已接入但审查未完成 | 4 套策略、实验批跑、summary 导出入口已存在 | 策略细节与论文要求是否完全一致 |
| 创新点 2 解剖约束双阶段配准 | 已接入但未满足完成证明 | constrained target/source、targetRegionTre、coverage、MeshGPU 集成已存在 | 主链是否完整消费了论文要求的全部算法能力 |
| 创新点 3 配准-跟踪联合准入 | 主链半闭环，未彻底完成 | calibration/tracking/gate/evaluation 已接入主链 | 运行时协作者边界、真实闭环证据、模拟与真实路径区分仍不足 |

## 9. 最终结论

这三项创新点目前**都不能宣称“全部完成”**。  
更准确的状态是：

- 创新点 1：框架已在，专项审查未完成
- 创新点 2：主链已接入关键能力，但论文匹配度尚未逐项证实
- 创新点 3：页面内联动已打通一部分，但真正的运行时闭环结构还没完成

因此，下一步顺序应该是：

1. 先完成运行时联动下沉  
2. 再补 MeshGPU / ProbeCalibration 的逐项匹配核查  
3. 最后才有资格对“论文创新点全部完成”做结论
