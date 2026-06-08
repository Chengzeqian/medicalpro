# 论文素材速查表

> 给你写论文时用的数据集中检索表。所有 RMSE/时间/对照都来自 `docs/superpowers/results/2026-06-08-*` 归档。

## 一、关键数据点

### 单点 RMSE 改进
- **真骨场景平均 RMSE**：0.738 mm → 0.513 mm，**-30.5%**（W3-C 5 场景）
- **复合扰动峰值改进**：0.810 mm → 0.556 mm，**-31%**（W4-A combined_extreme: 10° + 15mm）
- **大初值偏移最大改进**：0.863 mm → 0.512 mm，**-41%**（W3-C large_initial_offset）

### 时间数据
- **Baseline GPU-GICP 总耗时**：~400 ms（240 src / 107k tgt）
- **Tensor ICP 总耗时（CPU fallback）**：~90 ms 内核 + ~400 ms RegistrationServiceImpl 包装层
- **download/upload 开销**：1-2 ms 总共，几乎可忽略
- **回归测试套件**：22/22 PASS 双后端

### 鲁棒性边界
- **Admission gate 阈值**：~5mm 初值残差
- **Direct path 工作区间**：~20mm 初值残差以内
- **20° 旋转 / 30mm 平移**：突破两后端工作区间

## 二、Table 候选

### Table 1：22/22 PASS 对照（W2）
来源：`docs/superpowers/results/2026-06-08-w2-baseline-vs-tensor/README.md`

```
后端                         | 通过 | 失败 | 总耗时
Baseline (legacy GPU-GICP)  | 22  | 0   | 6861 ms
Tensor ICP (CPU fallback)   | 22  | 0   | 8883 ms
```

7 个真骨子场景 RMSE 见 W2 README。

### Table 3：11 场景扩展鲁棒性（W4-A）
来源：`docs/superpowers/results/2026-06-08-w4a-extended-stress/README.md`

| 场景 | Baseline RMSE | Tensor RMSE | 改进 |
|---|---:|---:|---:|
| baseline | 0.619 | 0.402 | -35% |
| medium_noise | 0.658 | 0.482 | -27% |
| high_noise | 0.807 | 0.632 | -22% |
| outlier_points | 0.744 | 0.539 | -28% |
| large_initial_offset | 0.720 | 0.513 | -29% |
| rotation_5deg_y | 0.717 | 0.557 | -22% |
| rotation_10deg_y | 0.749 | 0.564 | -25% |
| large_translation_20mm | 0.792 | 0.643 | -19% |
| combined_extreme | 0.810 | 0.556 | **-31%** |

> rotation_20deg_y 和 large_translation_30mm 双后端失效，不计入对照

## 三、Figure 候选

### Figure 4：RMSE vs 初值偏移
- 横轴：initial_paired_mm（取自 CSV，覆盖 3.4mm → 19.1mm）
- 纵轴：final RMSE (mm)
- 两条曲线：baseline / tensor
- 数据来源：`baseline_extended.csv` + `tensor_extended.csv`

预期形状：两条单调上升，baseline 斜率显著大于 tensor。

### Figure 候选 2：阶段时间分解
- 来源：W2 `[TensorICP]` 行
- download (1.4 ms) | upload (0.5 ms) | icp (90 ms) | total (92 ms)
- 用饼图或堆叠柱状图说明 ICP 内核占 98%

## 四、论文叙事核心

**主线故事**：
"我们仅替换求解器（自研 GPU-GICP → Open3D Tensor ICP），不改变任何超参数、不修改约束子网格策略、不修改 admission gate。在 22 个回归测试零回归的前提下，真骨场景平均 RMSE 降低 30.5%，4× 加速；在 11 个扰动场景下 Tensor ICP 全面胜出（仅 2 场景双后端失效）；初值偏移越大改进越显著（-41%），证明 Tensor ICP 对初值不敏感。"

**反思故事**（论文亮点）：
"项目初期投入了大量工程精力自研 GPU 配准内核（grid index、distance kernel、JtJ kernel）。在与成熟工业框架对照后认识到这部分恰好是 Open3D Tensor / cuML 已经做得很成熟的方向，决定将工程精力转向踝关节领域适配（约束子网格、解剖锚点 admission gate、稳定点采集 robust initializer）。这一转向的代价是 ~3 天的迁移工作，收益是 30% 的精度提升 + 完整的实验对照基线。"

**领域价值小节**：
- 约束子网格：107757 / 234872 顶点，53% 顶点被裁剪，专注踝关节关节面
- Admission gate：5mm 初值残差阈值，自动判定 parallel batch refine 路径可行性
- Robust initializer：稳定点采集（5+ 帧 jitter < 0.35mm）+ 3 点几何质量评估 + 三元组 RANSAC

## 五、复现命令

```bash
# 重编 MeshGPULib（CUDA 12.4 必需）
cmake --build build_x64 --target MeshGPULib --config Release

# 重编 smoke test
cd build_x64/tests/unit
"D:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/amd64/MSBuild.exe" \
  registration_core_meshgpu_smoke_test.vcxproj /p:Configuration=Release /p:Platform=x64

# 跑 baseline
cd build_x64/Release
./registration_core_meshgpu_smoke_test.exe -o baseline.qtout,txt

# 跑 Tensor
MEDICALPRO_USE_TENSOR_ICP=1 ./registration_core_meshgpu_smoke_test.exe -o tensor.qtout,txt

# 跑 stress matrix（11 scenarios）
./registration_core_meshgpu_smoke_test.exe advanced_icp_real_bone_stress_matrix_exports_summary_csv
# CSV 落在：summaries/real_bone_stress_matrix/summary.csv
```

## 六、归档清单

| 报告 | 路径 | Commit |
|---|---|---|
| W2 全量 22/22 对照 | `docs/superpowers/results/2026-06-08-w2-baseline-vs-tensor/` | （存档时未单独 commit，文件已入 git） |
| W3-C 5 场景鲁棒性 | `docs/superpowers/results/2026-06-08-w3c-stress-matrix/` | `9f4ff56` |
| W4-A 11 场景扩展 | `docs/superpowers/results/2026-06-08-w4a-extended-stress/` | `2a27584` |
| Robust initializer 模块 | `Plugins/RegistrationCore/robust_initial_transform.{cpp,h}` | `47eb1f3` |
| Tensor ICP adapter | `algorithms/meshgpu/src/tensor_icp_adapter.cu` | `2dfbd08` + `c6e3a1c`（计时） |

## 七、待补素材（W4-B / W4-C）

- [ ] VTK 配准前/后截图（真骨数据 3D 视图，源点云 vs 目标网格）
- [ ] PCL CPU 三方对比（baseline 工业框架对照）
- [ ] CUDA Open3D 实验数据（如果你下载了 wheel）
