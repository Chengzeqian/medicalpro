# Ankle Navigation Innovation Baselines And Evaluation Design

**日期：** 2026-04-29  
**范围：** `medicalpro` 踝关节置换导航的 3 个创新点、baseline、指标与实验闭环  
**目标：** 将“已有算法骨架和主链入口”补齐为可对比、可统计、可复现实验体系，形成论文可用的客观证据链。

## 1. 背景

当前项目已经有以下主链能力：

- 病例工作区与 `planning.json`
- 点配准与 `registration_result.json`
- 导航准入评分与 `navigation_run.json`
- 评估导出与 `evaluation_report.json`、`evaluation_metrics.csv`

但这还不等于 3 个创新点已经完成。当前缺口主要在三类：

- 缺 baseline：没有统一的对照策略和可切换算法入口
- 缺指标闭环：结果文件能写，但不是面向实验统计的结构化结果
- 缺实验执行器：没有“同一病例、同一扰动、跑多组策略、导出对比结果”的能力

因此，本设计要解决的是“把创新点变成能证明自己的实验系统”，不是只在 UI 上补几个按钮。

## 2. 总体原则

### 2.1 创新点完成的判定标准

一个创新点只有同时满足下面 4 项，才算真正完成：

1. 同一接口下可切换 `proposed` 与 `baseline`
2. 同一输入下可重复运行并导出结构化结果
3. 有明确、可解释、可复核的指标定义
4. 至少在小规模病例集和扰动实验上形成对比表

### 2.2 首版实验边界

- 首版以 replay / simulated / offline 实验闭环为主
- 不把真实硬件接入作为论文主链完成前提
- 不追求大样本统计学论文，重点是“小样本可重复 + 对照充分 + 指标完整”

## 3. 创新点 1：目标区敏感点选取

### 3.1 目标

围绕假体目标区，比较不同采点策略在相同点数预算和相同病例输入下对目标区误差的影响，证明“目标敏感选点”优于随意采点。

### 3.2 策略集合

必须实现 4 套可切换策略：

- `target_sensitive`
  - 基于目标轴、目标区半径、局部覆盖度和空间分散度进行评分
- `random`
  - 在候选点集中随机抽取固定数量点
- `uniform`
  - 采用空间均匀覆盖策略，优先选择分布更均匀的点
- `expert_rule`
  - 用固定解剖规则模拟经验选点，例如胫骨远端内外侧、距骨穹隆前后区域优先

### 3.3 输入与输出

输入：

- `planning.json` 中的目标区、主骨、参考解剖点
- 候选点集或候选采点区域
- 点数预算 `N`
- 可选扰动配置：位置噪声、漏点、异常点

输出：

- 选点序列
- 每个点的推荐理由
- 采点阶段耗时
- 该策略下配准后的误差指标

### 3.4 指标

必须记录：

- `target_tre_mm`
- `overall_tre_mm`
- `point_count`
- `picking_time_ms`

建议附加：

- `coverage_score`
- `target_axis_distance_mean_mm`
- `selected_region_entropy`

### 3.5 验证方式

对每个病例，在相同候选点池和相同 `N` 条件下，运行 4 套策略：

- `N = 4 / 5 / 6 / 8`
- 每个 `N` 下，`random` 至少重复多次后取均值和方差
- `uniform / expert_rule / target_sensitive` 各跑一次或固定重试次数

输出表格至少包含：

- 各策略在各点数预算下的 `target_tre_mm`
- 达到目标区误差阈值所需最小采点数
- 采点时间对比

## 4. 创新点 2：踝关节解剖约束双阶段配准

### 4.1 目标

证明“带解剖先验的双阶段配准”在目标区精度和稳定性上优于单阶段点配准与普通全局表面精配准。

### 4.2 baseline 与 proposed

必须实现 4 套方法：

- `single_stage_landmark`
  - 单阶段点配准
- `landmark_plus_global_icp`
  - 点配准后直接全局 ICP
- `landmark_plus_global_gicp`
  - 点配准后直接全局 GICP
  - 如运行环境不支持 GPU-GICP，则允许回退为 CPU 可运行版本，但实验记录必须标明
- `ankle_two_stage_constrained`
  - 第一阶段：加权刚体粗配准
  - 第二阶段：ROI 约束局部表面 refinement

### 4.3 proposed 的最低实现要求

`ankle_two_stage_constrained` 不能只停留在标签层面，必须实际具备：

- 完整刚体求解，不仅是平移
- 第一阶段权重可由目标敏感选点结果或解剖先验决定
- 第二阶段只在目标相关 ROI 内进行 refinement
- 可以利用法向一致性、局部曲率或 ROI 可置信度做约束

### 4.4 输入与输出

输入：

- `planning.json` 中的目标区和 ROI 定义
- image-space / patient-space 配准点
- 模型表面点或表面 mesh
- 可选扰动配置：噪声、漏点、偏心采点、局部遮挡

输出：

- 粗配准结果
- 精配准结果
- 各阶段耗时
- 收敛状态
- 目标区误差和整体误差

### 4.5 指标

必须记录：

- `fre_mm`
- `overall_tre_mm`
- `target_tre_mm`
- `convergence_success`
- `runtime_ms`

建议附加：

- `translation_error_mm`
- `rotation_error_deg`
- `roi_residual_mm`
- `retry_count`

### 4.6 稳定性定义

稳定性不只看一次结果，而是看在相同病例不同扰动下的表现：

- 收敛成功率
- 目标区 TRE 方差
- 是否容易陷入局部最优
- 对异常点和点分布不均匀的敏感程度

## 5. 创新点 3：配准-跟踪联合可信度准入

### 5.1 目标

证明仅凭单阈值不能可靠判断是否可导航，而联合利用配准质量、目标区误差、采点覆盖度和 tracking 质量的准入机制可以更有效拦截坏配准和坏跟踪。

### 5.2 策略集合

必须实现 3 套准入策略：

- `no_gate`
  - 无准入，配准完成后直接允许导航
- `threshold_only`
  - 只基于单项阈值告警，例如 `FRE` 或 `RMS`
- `joint_confidence`
  - 联合 `FRE / targetTRE / coverage / surfaceResidual / trackingJitter / visibleFrameRatio`

### 5.3 `joint_confidence` 的最低要求

联合准入不能只是静态文本提示，必须具备：

- 明确输入项
- 明确评分公式或规则组合
- 明确放行条件
- 明确拒绝原因和建议动作

### 5.4 输入与输出

输入：

- 注册结果
- tracking 质量结果
- replay 轨迹或 simulated 轨迹
- 遮挡、抖动、漂移等扰动注入配置

输出：

- 是否放行
- 可信度分数
- 拒绝原因
- 导航期间告警和中断记录

### 5.5 指标

必须记录：

- `error_intercept_rate`
- `false_pass_rate`
- `navigation_success_rate`
- `interruption_count`

建议附加：

- `mean_confidence_score`
- `recheck_count`
- `occlusion_related_abort_count`

### 5.6 坏样本定义

用于统计拦截率和误放行率的“坏样本”必须有一致定义，例如：

- 目标区 TRE 超阈值
- tracking jitter 超阈值
- visible frame ratio 低于阈值
- 导航终点偏差超阈值

没有坏样本定义，拦截率和误放行率就没有可比性。

## 6. 实验运行框架

需要新增统一实验运行框架，而不是分散在 UI 点击路径中。

### 6.1 实验层级

- `case level`
  - 单病例、单配置运行
- `batch level`
  - 多病例批量运行
- `perturbation level`
  - 对同一病例注入多组扰动

### 6.2 运行维度

每次实验运行至少固化以下维度：

- `case_id`
- `innovation_id`
- `strategy_id`
- `noise_profile`
- `point_budget`
- `tracking_profile`
- `run_index`

### 6.3 输出资产

每次运行都要能生成：

- 单次结果 JSON
- 统一汇总 CSV
- 便于论文作图的长表数据

建议目录结构：

```text
data/cases/<case_id>/evaluation/
  experiments/
    innovation_1/
    innovation_2/
    innovation_3/
  summaries/
    innovation_1_summary.csv
    innovation_2_summary.csv
    innovation_3_summary.csv
```

## 7. 数据与病例组织

### 7.1 病例级输入

每个病例至少需要：

- 术前 CT / DICOM
- 已分割骨模型
- `planning.json`
- 用于 replay 或模拟的点采集 / tracking 输入

### 7.2 小样本策略

考虑到病例量有限，首版实验采用：

- `5-10` 例可完整运行病例
- 每例多次扰动重复
- 通过“病例数 × 重复实验数”增强对比可信度

这比空谈大样本更现实，也更适合毕业项目。

## 8. 与代码结构的关系

这份设计要求算法和实验闭环以后不要继续塞进 `NavigationPage`。

代码落点约束：

- 点选策略和接口放在 `Plugins/PointRegistration`
- 双阶段配准核心放在 `Plugins/RegistrationCore`
- 指标定义、实验运行器、结果汇总放在 `Framework/Navigation`
- 页面层只负责触发运行和展示结果

## 9. 完成标准

三个创新点只有在满足以下条件后才算真正完成：

1. 每个创新点都能切换 `proposed` 和所有 baseline
2. 每个创新点都能输出统一结构化实验结果
3. 指标 CSV 中能直接形成论文表格
4. 至少有一套病例批量结果能支撑趋势性结论
5. UI 不再是实验唯一入口，离线或批量运行同样可执行

## 10. 结论

双轨方案中的 `Phase B/C/D` 不再以“把现有代码再补一点”为目标，而是以如下结果为目标：

> 让三个创新点都拥有 baseline、指标定义、实验运行器和结果导出，  
> 使每个创新点都能被同一套病例和扰动条件客观比较，  
> 最终形成毕业论文可以直接引用的证据链，而不是主观描述。
