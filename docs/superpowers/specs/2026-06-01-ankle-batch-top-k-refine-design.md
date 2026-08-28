# Ankle Top-K Batch Refine Design

**日期：** 2026-06-01  
**范围：** `medicalpro` 当前 `ankle_two_stage_constrained` 主链中 `top-K` 候选精配准阶段的批量化改造  
**目标：** 为 `RegistrationCore -> MeshGPU` 增加批量 refine runtime API，把当前上层串行 `top-K` refine 循环下沉为底层批量执行入口，补齐并行加速主线的最后一块关键接口能力

## 1. 设计结论

本轮不直接重写 GICP 数学内核为单 kernel 多候选同步求解，而是先建立一条稳定的批量 refine 运行时能力：

1. `RegistrationCore` 继续负责候选生成、`top-K` 选择、最佳候选决策和 metadata 组装
2. `MeshGPU Runtime API` 新增批量 refine 接口，接收一组候选初始变换并返回每个候选的 refine 结果
3. `MeshGPUInterface` 第一版复用现有单候选 refine 能力，先完成批量入口、结果结构、兼容回退和测试闭环
4. 旧 runtime 仍保留串行 fallback，保证 DLL 兼容和旧环境可运行

这一定义把“上层串行循环”改为“底层批量 refine 能力”，从架构上补齐并行搜索之后的精配准承接层。

## 2. 背景与现状

当前并行加速主线已经具备以下能力：

- `MeshGPU` 支持候选初值 GPU 粗评分
- `RegistrationCore` 支持 `top-K` 候选筛选和多分辨率 coarse-to-fine
- constrained source runtime filter 已接入
- constrained target mesh 的顶点筛选、triangle 保留、索引映射和压缩索引已下沉到 GPU

当前剩余的主要串行瓶颈是 `top-K` refine：

- `RegistrationCore` 在选出 `top-K` 候选后，仍逐个调用 `performGICPRegistration(...)`
- 每个候选 refine 后再由 host 计算误差并比较最优候选
- `MeshGPU Runtime API` 只有单次 `runRegistration(...)` 和 `runRegistrationWithRotationSearch(...)`
- 因此系统虽然已经有“并行 coarse candidate scoring”，但 refine 阶段还没有对应的批量运行时接口

这会带来两个问题：

1. 并行加速故事在架构上不完整，`top-K` 之后重新退回上层串行
2. 后续若继续做更深层 CUDA 并行，缺少明确的批量 refine API 作为演进支点

## 3. 目标与非目标

### 3.1 本轮目标

1. 新增 `MeshGPU` 批量 refine runtime API
2. 让 `RegistrationCore` 用批量 refine 替代当前 `top-K` 串行循环
3. 让批量 refine 结果能够稳定回传 `candidateIndex / transform / rmse / iterations / converged / success`
4. 保留旧 DLL 和旧 runtime 的兼容回退路径
5. 补齐 runtime 直测、`RegistrationCore` 集成测试和兼容性测试

### 3.2 本轮非目标

- 不在这一轮把 GICP 核心数学求解重写成单 kernel batch GICP
- 不在这一轮引入分骨并行
- 不改写现有 candidate scoring 和 constrained mesh 已完成的 GPU 路径
- 不改变 `PointRegistration` 上层业务语义和结果结构

## 4. 总体架构

### 4.1 `RegistrationCore` 职责

`RegistrationCore` 保持以下职责不变：

- 生成候选初值
- 在 coarse scoring 后筛出 `top-K`
- 组装 runtime refine 请求
- 根据批量 refine 结果选出最优候选
- 写回 `parallelSearchReport` 和现有 metadata

`RegistrationCore` 不再负责：

- 自己写 `for` 循环逐个调用 `performGICPRegistration(...)`
- 自己维护候选 refine 的串行调度逻辑

### 4.2 `MeshGPU Runtime API` 职责

新增一条批量 refine 入口，负责：

- 接收 `top-K` 候选初始变换
- 对每个候选执行 refine
- 返回每个候选的运行结果
- 通过 `candidateIndex` 保持结果与输入候选的可映射关系

这一层只承担算法运行时能力，不承担业务决策。

### 4.3 `MeshGPUInterface` 第一版实现原则

第一版实现遵循以下原则：

1. 先建立批量 API，不直接追求单 kernel 多候选同步求解
2. 先复用现有单候选 refine 逻辑，减少 CUDA 回归风险
3. 先保证批量结果结构、错误传播和兼容回退稳定
4. 为后续多 stream 或更深层 GPU 并行保留演进空间

因此，本轮“批量 refine”强调的是运行时接口和调度边界批量化，而不是一次性完成最深层算法重写。

## 5. 数据结构与接口

### 5.1 新增 runtime 请求结构

建议新增：

```cpp
struct RuntimeRefineCandidateRequest {
    int candidateIndex = -1;
    Transform4x4 initialTransform;
};
```

含义：

- `candidateIndex`：输入候选在当前批次中的索引，用于回映到 `candidateId`
- `initialTransform`：该候选进入 refine 前的初始刚体变换

### 5.2 新增 runtime 结果结构

建议新增：

```cpp
struct RuntimeRefineCandidateResult {
    int candidateIndex = -1;
    Transform4x4 transform;
    float rmse = 0.0f;
    int iterations = 0;
    bool converged = false;
    bool success = false;
};
```

含义：

- `candidateIndex`：与输入候选映射
- `transform`：refine 后变换
- `rmse / iterations / converged`：当前 refine 结果统计
- `success`：本候选是否成功完成 refine

### 5.3 新增 runtime API

在 `mesh_gpu_runtime_api.h` 中新增：

```cpp
virtual std::vector<RuntimeRefineCandidateResult> refineTransformCandidates(
    const std::vector<RuntimeRefineCandidateRequest>& candidates,
    const RegistrationParams& params) = 0;
```

设计约束：

- 只新增，不修改现有单次注册接口签名
- 输入使用 `std::vector`
- 输出顺序允许与输入一致，也允许按内部实现返回，但必须保证 `candidateIndex` 可映射

## 6. 调用链设计

### 6.1 `RegistrationCore` 批量 refine 路径

在 `top-K` 已筛出的前提下：

1. 把 `topKCandidateScores` 对应候选转换成 `RuntimeRefineCandidateRequest`
2. 调用 `m_meshGPU->refineTransformCandidates(...)`
3. 遍历批量结果，按 `candidateIndex` 找回候选 `candidateId`
4. 用批量结果中的 `rmse` 选择最优候选
5. 写回：
   - `refineCandidateCount`
   - `refineMs`
   - `bestCandidateRank`
   - 必要的 fallback 标志

### 6.2 兼容回退

必须保留旧 runtime 兼容路径：

- 新 runtime 支持批量 refine 时，走批量 refine 路径
- 旧 runtime 或 legacy adapter 不支持时，继续走现有串行 `top-K` refine 路径
- metadata 中需要能体现当前是否走了 batch refine

这样可以保证：

- 新 DLL 可以提供新能力
- 旧 DLL 不会因缺少符号而失效
- 测试环境可同时验证新旧路径

## 7. `MeshGPU` 第一版实现策略

### 7.1 运行时实现

第一版 `MeshGPUInterface::refineTransformCandidates(...)` 采用以下策略：

1. 复用现有单候选 refine 逻辑
2. 在批量入口中为每个候选建立独立结果对象
3. 聚合每个候选的 refine 输出
4. 返回批量结果数组

### 7.2 并行化深度控制

第一版不承诺：

- 单 kernel 内同时 refine `K` 个候选
- 改写 GICP 内部核心数学实现
- 对所有候选共享复杂迭代状态

第一版承诺：

- 上层不再持有串行 `top-K refine` 业务循环
- 底层暴露稳定的批量 refine runtime 能力
- 批量 refine 路径可以作为后续更深层 GPU 并行的正式演进入口

## 8. 测试设计

### 8.1 Runtime 直测

新增或扩展 runtime smoke test，直接调用 `refineTransformCandidates(...)`，断言：

- 返回结果数量与输入候选数量一致
- `candidateIndex` 可正确映射
- 至少一个候选 `success == true`
- 成功候选的 `rmse >= 0`
- 最优候选的 `rmse` 不劣于其他成功候选

### 8.2 `RegistrationCore` 集成测试

扩展现有 parallel search smoke test，断言：

- 新 runtime 可用时走 batch refine 路径
- `refineCandidateCount` 等于 `top-K`
- `bestCandidateRank` 来自批量 refine 结果
- `refineMs` 被记录

### 8.3 兼容性测试

保留 legacy runtime compatibility 测试，断言：

- 旧 runtime 缺失批量 refine 能力时不会崩溃
- 系统自动回退到串行 refine
- metadata 中能反映 fallback 状态

## 9. 风险与控制

### 9.1 风险：把“批量接口”误当成“深度 batch GICP 已完成”

控制策略：

- 在代码命名、日志和说明中明确区分“批量 runtime refine 接口”和“单 kernel batch GICP”
- 本轮只承诺前者，不夸大后者

### 9.2 风险：新 DLL 破坏旧接口兼容

控制策略：

- 只加接口，不改旧接口签名
- legacy adapter 保留旧行为
- 通过 compatibility test 锁住 fallback

### 9.3 风险：测试只验证成功，不验证路径选择

控制策略：

- 在 metadata 中加入 batch refine 路径标记
- 让测试同时验证“结果正确”和“走了哪条路径”

## 10. 预期结果

完成本轮后，并行加速主线将形成更完整的结构：

1. GPU coarse candidate scoring
2. GPU constrained source / target filtering
3. GPU constrained target mesh compaction
4. Batch refine runtime path
5. Legacy runtime fallback path

这会让当前方案从“并行搜索 + 串行 refine”进一步演进到“并行搜索 + 批量 refine 运行时承接”，为后续继续向更深层 GPU 并行 refine 推进打下接口和测试基础。
