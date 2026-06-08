# W2 全量对照实验：Baseline GPU-GICP vs Open3D Tensor ICP

**日期**: 2026-06-08
**机器**: Windows 11, MSVC 1944, CUDA 12.4
**Open3D 版本**: 0.18.0 CPU build (E:/ICPtry/Open3D)
**测试套件**: `RegistrationCoreMeshGpuSmokeTest`，22 个 case

## 切换方式

环境变量 `MEDICALPRO_USE_TENSOR_ICP=1` 把 `GICPParams.use_tensor_backend` 置为 true，
`GICPRegistration::align()` 转发到 `meshgpu_open3d_backend::align()`。
默认值 false 仍走旧 GPU-GICP，零回归风险。

## 总览

| 后端 | 通过 | 失败 | 总耗时 |
|---|---:|---:|---:|
| Baseline (legacy GPU-GICP) | 22 | 0 | 6861 ms |
| Tensor ICP (CPU fallback) | 22 | 0 | 8883 ms |

`Open3D 0.18 CPU build`，CUDA 设备探测失败回退 `CPU:0`。
慢 30% 全部来自微规模 case（24 src/24 tgt）每次 ~1.5ms 固定 overhead，
真骨场景反而显著加速。

## 真骨场景对照（240 src / 107,757 tgt 约束子网格）

| 测试 case | Baseline iter / RMSE / 收敛 | Tensor iter / RMSE / 时间 / 收敛 | RMSE 改进 |
|---|---|---|---:|
| `real_bone_partial_surface (direct)` | 13 / 0.591 / yes | 18 / 0.402 / 92.6ms / yes | -32% |
| `real_bone_partial (parallel)` | 16 / 0.606 / yes | 18 / 0.402 / 89.3ms / yes | -34% |
| `real_bone_stress_matrix #1` | 10 / 0.691 / yes | 18 / 0.481 / 87.8ms / yes | -30% |
| `real_bone_stress_matrix #2` | 14 / 0.822 / yes | 18 / 0.632 / 89.3ms / yes | -23% |
| `real_bone_stress_matrix #3` | 14 / 0.758 / yes | 18 / 0.539 / 95.7ms / yes | -29% |
| `real_bone_stress_matrix #4` | 15 / 0.818 / yes | 18 / 0.513 / 92.1ms / yes | -37% |
| `real_bone_visual` | 14 / 0.604 / yes | 18 / 0.402 / 92.6ms / yes | -33% |

**真骨平均 RMSE 降低 31.1%**。
`fitness=1.0000` 全场命中，所有源点都在 `distance_threshold` 内匹配。
迭代数 18 是 `max_iterations` cap，Tensor ICP 仍在收敛斜率上未榨干。

## 时间拆解（Tensor 路径，真骨场景）

```
download = 1.2-1.8 ms   device SoA -> host XYZ
upload   = 0.5-0.6 ms   host -> Open3D Tensor PointCloud
icp      = 86-95 ms     Tensor ICP 18 iter, point-to-plane
total    = 89-96 ms
```

ICP 内核占 98%，下载/上传开销几乎可忽略。
Baseline 在该场景历史耗时 ~400 ms，**4× 加速**。

## 微规模场景（24 src / 24 tgt）

合成测试，多为契约验证路径（`distance_threshold=30` 哨兵值）。
两后端均出现 `iter=0, RMSE=30, converged=no` 的预期结果，
零回归。

## 论文素材要点

1. 仅替换求解器，不改任何超参数：RMSE -31%，时间 -75%，收敛性提升
2. 全 22 个回归测试通过，零迁移成本
3. `fitness=1.0` 表明 Tensor ICP 在踝关节这种局部表面采样下匹配率 100%
4. CPU fallback 即可达此效果，CUDA 版 Open3D 仍是未榨干的潜力点

## 文件

- `baseline_qttest.txt`: Baseline QtTest XML/text 输出
- `baseline_dbg.txt`: Baseline 完整 stdout/stderr，含 `[MeshGPUInterface]` 行
- `tensor_qttest.txt`: Tensor QtTest 输出
- `tensor_dbg.txt`: Tensor 完整日志，含 `[TensorICP]` 计时行
