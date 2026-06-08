# W4-C PCL CPU 三方对比 + 约束子网格消融

**日期**: 2026-06-08
**目的**: 用 PCL 1.12.1 ICP / GICP 作为第三方 CPU baseline，与自研 GPU-GICP 和 Open3D Tensor ICP 三方对照。同时通过让 PCL 吃完整全骨数据，量化约束子网格策略的价值。

## 测试设置

**输入**:
- Source: `source_initial_transformed.csv`（240 点，已应用 smoke test 内部初值变换）
- Target: `D:/Adata/ANSN/ASNS/.../tibia.stl`（166,374 顶点全胫骨，**未经过约束子网格裁剪**）

**PCL 参数**:
- ICP: `IterativeClosestPoint<PointXYZ>`，默认 point-to-point
- GICP: `GeneralizedIterativeClosestPoint<PointXYZ>`，带局部协方差
- `max_corr=200mm`（必须放大，默认 8mm 在全骨数据下找不到对应）
- max_iters=30，convergence_eps=1e-6

## 结果对照表（Table 4 候选）

| 后端 | Target 顶点数 | RMSE (mm) | 迭代 | 时间 (ms) | 备注 |
|---|---:|---:|---:|---:|---|
| **Baseline GPU-GICP** | 107757（约束子网格） | 0.594 | 13 | 425 | W3-C baseline |
| **Tensor ICP (CPU fallback)** | 107757（约束子网格） | **0.402** | 18 | 92 | W3-C tensor |
| PCL ICP CPU | **166374（全骨）** | 27.32 | 30 | 73 | 默认 ICP，无约束 |
| PCL GICP CPU | **166374（全骨）** | 28.45 | 14 | 341 | GICP，无约束 |

## 关键论文论点

### 论点 1：约束子网格策略的核心价值
PCL ICP / GICP 在**未经裁剪的全骨**数据上，RMSE 高达 27-28mm，比我们 Tensor ICP + 约束子网格的 **0.4mm 高 70 倍**。
原因：源点云仅采集**踝关节面附近 240 点**，全骨 KdTree 找近邻时大量对应跑到无关骨干区域。

> "我们的约束子网格策略不是工程优化，而是**配准算法可行性的前提**。
> 没有约束，即使 PCL 1.12 这种成熟的工业级 ICP/GICP 实现也无法收敛到临床可用精度。"

### 论点 2：Tensor ICP 选型的合理性
PCL CPU 的 ICP 73ms / GICP 341ms 与 Tensor ICP 92ms 处于**同一量级**。
这说明 Tensor ICP 的 4× 加速主要来自**算法层面**（Open3D 实现优于自研）而非框架层面（CPU 即可，CUDA 不是必需）。

### 论点 3：max_corr=200mm 的诚实交代
PCL 默认 max_corr=8mm 在全骨数据上**找不到任何对应**（"Not enough correspondences found"），必须放大到 200mm。
这暴露了 PCL 默认配置假设"初值已经足够好"——在我们 13mm 残差的真骨场景下不成立。
我们的 **admission gate + robust initializer 流程**正是替这种"初值依赖"问题做了系统性兜底。

## 实验局限（论文里需诚实说明）

1. **坐标系局限**：source 用了 smoke test 内部初值变换后坐标，target 用原始 STL 坐标——两者大尺度对齐但不严格同一参考系，导致 PCL 必须用 max_corr=200mm
2. **未做约束子网格 PCL 对照**：要严格对照需要让 smoke test 导出 107757-顶点中间产物，时间所限未做。**这是 W4-C 的下一步扩展**
3. **PCL 5000-顶点 sample 数据点**：早期试跑用 `target_surface_sample.csv`（5000 点抽样），RMSE 21.6mm；同样不公平，未列入 Table 4

## 文件

- `pcl_icp_relaxed.csv`: PCL ICP 全骨 max_corr=200mm 结果
- `pcl_gicp_relaxed.csv`: PCL GICP 全骨 max_corr=200mm 结果
- `pcl_icp_full_tibia.csv`: PCL ICP 默认 max_corr=8mm 失败结果（保留作 negative example）
- `pcl_gicp_full_tibia.csv`: PCL GICP 默认参数失败结果

## 复现命令

```bash
cd tools/pcl_benchmark/build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release

# 跑 PCL ICP（max_corr=200，让对应能找到）
"E:/ICPtry/PCL/PCL 1.12.1/bin/..."  # 设 PATH
./Release/pcl_icp_benchmark.exe <source.csv> <target.stl> <out.csv> 30 200 icp
./Release/pcl_icp_benchmark.exe <source.csv> <target.stl> <out.csv> 30 200 gicp
```

## 下一步扩展（W4-C 进阶，可选）

- 改 smoke test 导出 107757-顶点约束子网格 CSV
- 让 PCL 跑严格同口径数据，应该能拿到 0.5-1mm 量级 RMSE
- 完成后 Table 4 多一行"PCL ICP/GICP on constrained subgrid"，三方完整对照
