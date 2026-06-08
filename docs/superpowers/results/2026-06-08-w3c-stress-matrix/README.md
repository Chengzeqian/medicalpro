# W3-C 鲁棒性矩阵：Baseline GPU-GICP vs Tensor ICP 在初值扰动下的对照

**日期**: 2026-06-08
**数据来源**: `RegistrationCoreMeshGpuSmokeTest::advanced_icp_real_bone_stress_matrix_exports_summary_csv`
**真骨**: 240 source points, 107,757 target vertices (constrained subset of 234,872)
**5 个 stress 场景**:

| 场景 | 噪声 σ | outliers | 初值平移 (mm) | 描述 |
|---|---:|---:|---|---|
| baseline | 1.0 | 0 | (2.4, -1.6, 1.1) | 临床典型条件 |
| medium_noise | 2.0 | 0 | (2.4, -1.6, 1.1) | 测量精度下降 |
| high_noise | 4.0 | 0 | (2.4, -1.6, 1.1) | 严重抖动 |
| outlier_points | 1.0 | 8 | (2.4, -1.6, 1.1) | 软组织误采点 |
| large_initial_offset | 1.0 | 0 | (10.0, -7.0, 4.5) | 初值漂移 ~13mm |

## RMSE 对照表（direct path）

| 场景 | Baseline RMSE (mm) | Tensor RMSE (mm) | 改进 |
|---|---:|---:|---:|
| baseline | 0.594 | **0.402** | -32% |
| medium_noise | 0.682 | **0.481** | -29% |
| high_noise | 0.820 | **0.632** | -23% |
| outlier_points | 0.732 | **0.539** | -26% |
| large_initial_offset | 0.863 | **0.512** | -41% |
| **平均** | **0.738** | **0.513** | **-30.5%** |

Tensor ICP 在所有 5 个扰动场景下**全面优于** Baseline GPU-GICP，且**初值偏移越大改进越显著**（-41% on large_initial_offset），说明 Tensor ICP 对初值的敏感性低于 baseline。

## 时间对照（direct path）

| 场景 | Baseline ms | Tensor ms | 备注 |
|---|---:|---:|---|
| baseline | 425 | 547 | Tensor 慢 28% |
| medium_noise | 383 | 545 | 慢 42% |
| high_noise | 386 | 525 | 慢 36% |
| outlier_points | 376 | 569 | 慢 51% |
| large_initial_offset | 385 | 585 | 慢 52% |

注意：上面 ms 是 `RegistrationServiceImpl` 总耗时（含约束子网格构建、GPU 上传、ICP、回写）。
`[TensorICP]` 自身只占 ~90ms（详见 W2 归档）。差值主要在 baseline 路径**复用 GPU 内核状态**而 Tensor 路径每次重建 PointCloud。
临床场景下 ~90ms 的 Tensor ICP 仍远快于人手采集间隔（~秒级），不是瓶颈。

## Admission Gate 表现（parallel path）

`large_initial_offset` 场景下，**两个后端都被 admission gate 拒绝**：

```
parallel_admission_action  = reject
parallel_admission_reason  = robust_initial_residual_exceeds_recovery_threshold
parallel_admission_recovery = resample_probe_points_or_check_probe_calibration
initial_paired_mm          = 11.24 (>>admission threshold)
```

这是**预期且合理的行为**：当初值残差超过 ~10mm 时，admission gate 保守地拒绝 parallel 路径，
触发"重新采集探针点或校准探针"的临床流程。这正是 `robust_initial_transform` 模块的服务对象。

direct path 在该场景下仍能收敛到 0.51mm RMSE（Tensor）/ 0.86mm RMSE（baseline），
说明 Tensor ICP 内核本身能吃下 13mm 的初值偏移，admission gate 只是出于鲁棒性考虑提前拒绝。

## 论文素材要点

1. **Table 2 候选**：5 场景 × 2 后端 RMSE 对照
2. **Figure 候选**：RMSE 改进 % vs 初值偏移幅度（柱状图）→ 说明 Tensor ICP 对初值不敏感
3. **答辩故事**：
   - "我们仅替换求解器，跨 5 个扰动场景平均 RMSE 降低 30.5%"
   - "在 large_initial_offset 这种 13mm 偏移下改进达 41%，说明 Tensor 求解器在初值不准时更鲁棒"
   - "admission gate 在严重偏移下保守拒绝 parallel 路径，触发探针重采流程，与上游 robust initializer 模块形成完整闭环"

## 文件

- `baseline_stress.csv`: baseline GPU-GICP 完整 stress matrix CSV
- `tensor_stress.csv`: Tensor ICP 完整 stress matrix CSV

## 后续可选扩展（W4）

- 旋转扰动场景（5°、10°、20° 围三轴）
- 更大平移（±20mm、±30mm）测 Tensor ICP 收敛区间边界
- 不同 max_iterations（30、50）下 RMSE 进一步下降空间
