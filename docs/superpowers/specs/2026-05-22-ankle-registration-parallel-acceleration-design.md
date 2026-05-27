# Ankle Registration Parallel Acceleration Design

**日期：** 2026-05-22  
**范围：** `medicalpro` 当前 `PointRegistration -> RegistrationCore GPU refine` 主路线的算法增强、并行加速与论文表达  
**目标：** 在不推翻现有主流程的前提下，提出一套面向踝关节导航的目标区域约束两阶段配准方法，并将并行计算作为方法框架中的关键组成部分，而不是单纯的实现细节。

## 1. 设计结论

本轮方案不把创新点定义为“GPU 加速 GICP”，而定义为：

> 面向踝关节导航的目标区域约束两阶段配准方法及其并行加速研究

该方法以当前 `PointRegistration` 为主流程入口，以 `RegistrationCore` 为算法执行核心，以 `MeshGPU` 为并行计算承载层，形成如下主链：

1. 基于术前规划的目标区域和解剖约束区域，组织目标敏感采点
2. 根据 landmark 粗配准结果生成多组候选初值
3. 在 GPU 上并行评估候选初值，筛选最优或前 `K` 个候选
4. 在约束区域内执行局部 GICP 精配准
5. 输出最终变换、FRE、Target TRE、覆盖率和并行阶段统计指标

该方案兼顾三个目标：

- 与现有代码主路线高度兼容
- 相比单纯 GPU ICP 具备更明确的方法创新性
- 后续可以自然衔接导航准入、数字孪生与论文实验体系

## 2. 背景与现状

当前项目中的主配准路径已经收口到：

- `PointRegistration` 负责点对组织、约束信息传递、粗配准和结果汇总
- `RegistrationCore` 负责 landmark、ICP、GICP、2D-3D 等通用配准能力
- `MeshGPU` 已经被 `RegistrationCore` 以动态库形式接入，用于 GPU GICP 精配准

现状优势：

- `PointRegistration` 已经默认使用 `ankle_two_stage_constrained`
- 规划目标区与约束区已经能进入主链
- `RegistrationCore` 已支持 constrained source / target 构建
- `MeshGPU` 已具备 rotation search、GPU GICP、多分辨率相关基础

现状不足：

- 当前 GPU 主要承担 refine 阶段，方法层创新感不足
- 候选初值基本仍是单一路径，抗初值扰动能力有限
- 约束区 source / target 筛选仍主要由 CPU 串行组织
- 论文层面难以仅凭“GICP 上 GPU”形成一章独立方法贡献

## 3. 本章研究目标

本章的研究目标固定为以下四项：

1. 在现有踝关节点配准主路线上引入多初值搜索机制，提升粗配准鲁棒性
2. 将规划目标区域与解剖约束区域显式引入候选生成、候选筛选和局部精配准
3. 将多初值搜索和约束区筛选并行化，实现速度提升与收敛域扩展
4. 以 `Target TRE`、成功率和总耗时为主要指标，建立一套可用于论文论证的实验框架

本章非目标：

- 不重写现有 `PointRegistration` 主流程
- 不以 2D-3D 配准作为主研究对象
- 不把“分骨并行 + fused navigation space”作为本章主创新点
- 不在本轮引入多器械导航、AR 显示或更大范围的导航平台重构

## 4. 总体方法框架

### 4.1 阶段 A：目标敏感采点

输入：

- 术前规划给出的 `targetRegion`
- 解剖约束区域 `constraintRegions`
- 当前病例中的候选采点集合

过程：

- 保留当前 `target_sensitive` 采点策略作为默认策略
- 利用目标区半径、主轴、解剖区域等信息对候选点进行排序
- 优先保证目标区附近的几何代表性和点集空间分布

输出：

- 满足目标区约束的配准点对
- 供后续 coarse 初始化使用的 source / target landmark 对

### 4.2 阶段 B：多初值候选生成

输入：

- landmark 粗配准结果
- 目标区中心
- 约束区主轴或解剖方向先验

过程：

- 以当前加权 landmark 结果作为中心初值
- 在其周围生成候选刚体变换集合
- 候选生成方式可包含：
  - 绕主轴的小范围旋转扰动
  - 绕次轴的小角度姿态扰动
  - 面向目标区中心的平移微扰动
  - 基于 anchor 点的局部初值修正

输出：

- `N` 组候选初值变换
- 每组候选对应的候选来源和扰动参数

### 4.3 阶段 C：候选初值并行评估

输入：

- 候选初值集合
- 约束 source 点云
- 约束 target mesh 或 target 点集

过程：

- 对每组候选初值并行计算粗评分
- 粗评分不直接使用完整 refine，而使用更轻量的评价指标
- 推荐粗评分由以下项组成：
  - source 到 target 的近邻距离均值
  - 目标区有效命中率
  - 法向一致性或曲率一致性
  - 约束区内匹配覆盖率

输出：

- 所有候选初值的粗评分
- 最优 1 个或前 `K` 个候选

### 4.4 阶段 D：约束区局部 GICP 精配准

输入：

- 前 `K` 个候选初值
- 约束区 target mesh
- 约束区 source 点云

过程：

- 使用约束区域缩小 refine 空间
- 对每个候选执行局部 GICP 精配准
- 比较 refine 后的 RMS、Target TRE、覆盖率和稳定性指标
- 选出最终解

输出：

- 最终配准变换
- `FRE`
- `Target TRE`
- `coverageScore`
- 并行搜索和 refine 阶段的耗时指标

### 4.5 阶段 E：结果回写与导航承接

输出结果继续回写到现有主链：

- `PointRegistrationResult`
- `NavigationRuntimeState`
- `NavigationEvaluationService`
- 导航准入评分
- 数字孪生显示链

这样本章方法不会变成独立试验代码，而是保留在现有产品主路径内。

## 5. 并行加速重点

### 5.1 第一优先级：多初值并行搜索

这是本章的主创新点，也是论文价值最高的并行化位置。

建议设计：

- 候选数 `N` 可配置，默认从 `32 / 64 / 128` 三档开始
- 第一轮粗筛保留前 `K=4~8`
- 候选评分由 GPU kernel 并行完成
- 候选之间完全独立，天然适合 block 级并行

论文表述重点：

- 相比单初值方法，收敛域更大
- 面对初值扰动时成功率更高
- 总耗时增长可控，甚至在 GPU 下优于串行多候选方案

### 5.2 第二优先级：约束区候选点 / 候选面并行筛选

当前 constrained source / target 主要由 CPU 组织，后续建议并行化：

- target mesh 顶点 ROI 过滤
- triangle 保留判断
- source 点的目标区距离筛选
- 近邻排序与 top-k 保留

这部分是“方法约束显式化”的重要支撑，也能减少 refine 阶段的无效计算量。

### 5.3 第三优先级：多分辨率 coarse-to-fine

建议引入三级策略：

1. 粗层：低分辨率 target mesh 上评估全部候选
2. 中层：中分辨率 target mesh 上评估前 `K`
3. 细层：高分辨率约束区 mesh 上 refine 前 `1~3`

优势：

- 候选数可以更大
- 每轮筛选代价更低
- 方法上更像“分层并行搜索”而不是暴力搜索

### 5.4 暂缓项：分骨并行

分骨并行适合作为后续扩展，而不作为本章主创新点。

原因：

- 当前病例工作区和 fused navigation space 还在成长
- 提前把分骨并行拉进主研究范围会放大工程复杂度
- 容易稀释本章主线“多初值并行搜索 + 约束区局部 refine”

## 6. 模块设计

### 6.1 `PointRegistration` 侧设计

职责：

- 保持现有配准主入口不变
- 负责组织方法层逻辑和上下文信息
- 负责汇总最终结果与实验指标

建议新增概念：

- `candidate_initial_transforms`
- `parallel_search_profile_id`
- `candidate_search_count`
- `candidate_top_k`
- `coarse_search_score`
- `best_candidate_rank`

建议扩展 `PointRegistrationExecutionOptions`：

- `candidateCount`
- `topKCandidateCount`
- `enableParallelInitialSearch`
- `enableConstraintParallelFilter`
- `multiResolutionProfileId`

建议扩展 `PointRegistrationResult.metrics`：

- `candidate_count`
- `top_k_count`
- `coarse_search_ms`
- `roi_filter_ms`
- `refine_ms`
- `best_candidate_rank`
- `coarse_score`
- `target_region_hit_ratio`
- `constraint_region_count`
- `parallel_search_enabled`
- `multi_resolution_profile`

### 6.2 `RegistrationCore` 侧设计

职责：

- 保持当前算法调度核心地位
- 新增候选生成、候选筛选与并行评估接口
- 保持 refine 结果与现有导航链兼容

建议新增逻辑边界：

- `buildCandidateInitialTransforms(...)`
- `buildConstrainedTargetRegion(...)`
- `buildConstrainedSourceRegion(...)`
- `evaluateCandidateTransformsGpu(...)`
- `selectTopKCandidates(...)`
- `runConstrainedRefineFromCandidate(...)`

推荐数据流：

1. `PointRegistration` 传入目标区、约束区、粗初值和执行选项
2. `RegistrationCore` 生成候选
3. `RegistrationCore` 调用 `MeshGPU` 并行评分
4. `RegistrationCore` 选出前 `K`
5. `RegistrationCore` 对前 `K` 做约束 refine
6. 返回最优结果与全量指标

### 6.3 `MeshGPU` 侧设计

职责：

- 承担真正的并行算子计算
- 不负责业务决策
- 不直接依赖 UI 或病例工作区语义

建议分成三类 kernel：

1. 候选变换批量评分 kernel
2. 约束区 ROI / top-k 筛选 kernel
3. refine 阶段的 GICP kernel

建议保留当前已有基础：

- `rotation_search`
- 多分辨率 grid
- GPU GICP refine

建议新增输出统计：

- 每轮候选评分耗时
- 每层筛选剩余候选数
- GPU kernel 总耗时
- 评分阶段设备吞吐量估计

## 7. 数据模型建议

### 7.1 候选初值

建议新增 `CandidateInitialTransform` 结构，至少包含：

- `candidateId`
- `seedType`
- `transformMatrix`
- `rotationDelta`
- `translationDelta`
- `coarseScore`
- `rank`

### 7.2 并行搜索报告

建议新增 `ParallelSearchReport`，至少包含：

- `candidateCount`
- `topKCount`
- `coarseSearchMs`
- `multiResolutionLevelCount`
- `bestCandidateId`
- `bestCandidateRank`
- `coarseScoreDistribution`

### 7.3 精配准报告

建议 refine 报告至少包含：

- `refineCandidateId`
- `refineMethod`
- `refineMs`
- `refinedRms`
- `targetRegionTre`
- `coverageScore`
- `converged`

## 8. 与现有代码的对接策略

本轮设计遵循“增量演进，不推翻现有主链”的原则。

具体约束：

- `NavigationPage` 不直接持有新的并行算法细节
- `PointRegistrationServiceImpl::executeRegistration()` 仍是主入口
- `RegistrationCore::performICPRegistrationAdvanced()` 可继续作为 refine 入口
- 先在 `RegistrationCore` 内部插入“候选生成 -> 并行评分 -> top-k refine”的新中间层

建议主入口演化为：

1. 现有加权 landmark 粗配准
2. 若启用 `enableParallelInitialSearch`
3. 进入 `RegistrationCore` 候选评估阶段
4. 再进入局部 GICP refine
5. 返回与当前 `PointRegistrationResult` 兼容的结果对象

## 9. 实验设计

### 9.1 对比组

建议至少包含四组：

1. 单 landmark
2. landmark + CPU ICP
3. landmark + GPU GICP
4. 本章方法：目标区约束两阶段配准 + 并行多初值搜索

### 9.2 指标

核心指标：

- `FRE`
- `Target TRE`
- `coverageScore`
- 注册成功率
- 总耗时

阶段指标：

- 候选生成耗时
- 并行粗评分耗时
- ROI 筛选耗时
- refine 耗时
- speedup

### 9.3 鲁棒性实验

必须补充初值扰动实验：

- 对初始姿态施加不同角度和位移扰动
- 比较不同方法成功率与 Target TRE

这是证明“并行多初值搜索”价值的关键实验。

### 9.4 参数敏感性实验

建议考察：

- 候选数 `N`
- 前 `K` 保留数
- 目标区半径
- 约束区数量
- 多分辨率层数

## 10. 论文章节建议

本章建议组织为：

1. 问题定义与现有方法不足
2. 面向目标区域的两阶段配准总体框架
3. 目标敏感采点与约束区建模
4. 多初值并行搜索方法
5. 约束区局部 GICP 精配准
6. 并行实现与复杂度分析
7. 实验设计与结果分析
8. 本章小结

章节关键词建议统一使用：

- 目标区域约束
- 两阶段配准
- 多初值并行搜索
- 局部精配准
- Target TRE
- 并行加速

## 11. 风险与控制

### 11.1 风险：候选过多导致 GPU 粗评分阶段收益不明显

控制：

- 先做小规模 `32/64/128` 候选实验
- 引入多分辨率 coarse-to-fine 控制总成本

### 11.2 风险：方法写成“工程加速”，论文创新点不够集中

控制：

- 把主叙事固定在“目标区约束两阶段配准”
- 把 GPU 并行搜索定义为方法核心，而不是实现细节

### 11.3 风险：约束区过强导致全局收敛能力下降

控制：

- 保留 landmark 粗配准作为全局先验
- 约束区只作用于候选评估和局部 refine，不完全替代全局信息

### 11.4 风险：模块范围膨胀到分骨并行和 fused navigation space

控制：

- 本章先不把分骨并行作为主目标
- 将其明确标记为后续扩展方向

## 12. 后续开发顺序建议

建议按以下顺序开发：

1. 在 `PointRegistration` 中补齐并行搜索配置项和结果指标
2. 在 `RegistrationCore` 中插入候选生成和 top-k 调度层
3. 在 `MeshGPU` 中补齐候选评分 kernel
4. 将约束区 source / target 构建迁移到可并行筛选路径
5. 建立粗层 / 细层多分辨率实验配置
6. 补齐实验导出与统计分析

## 13. 结论

本设计将当前项目中已经成立的 `PointRegistration -> RegistrationCore GPU refine` 主路线，扩展为一套更适合作为硕士毕设章节的方法框架：

- 以目标区和解剖约束区为显式先验
- 以两阶段配准为主结构
- 以多初值并行搜索为核心创新点
- 以局部约束区 GICP 为精配准核心
- 以 `Target TRE + 成功率 + 总耗时` 为主要验证指标

这条路线既尊重现有工程积累，也能形成一章完整、可实验、可落地、可扩展的研究内容。
