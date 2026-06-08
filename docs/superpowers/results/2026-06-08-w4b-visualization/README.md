# W4-B 配准前/后可视化（双后端 HTML view 已就绪，截图待补）

**状态**：双后端可视化数据已并排归档，截图待用户手动补充。

## 已并排归档的双视图

```
2026-06-08-w4b-visualization/
├── baseline_view/    ← 旧 GPU-GICP 后端跑出的 3D view
│   ├── registration_before_after_view.html
│   ├── source_raw.csv
│   ├── source_initial_transformed.csv
│   ├── source_parallel_final_transformed.csv
│   ├── target_surface_sample.csv
│   ├── target_probe.csv
│   ├── initial_transform_matrix.csv
│   ├── parallel_final_transform_matrix.csv
│   ├── metrics.csv
│   └── registration_metadata.csv
└── tensor_view/      ← Open3D Tensor ICP 后端跑出的同场景 3D view
    └── （同样 10 个文件）
```

两个视图来自**完全相同的真骨场景**（240 src / 107k tgt 约束子网格），唯一区别是配准求解器后端。

## 截图步骤（用户操作，5-10 分钟）

1. **浏览器打开两个 HTML**（双标签页方便对比）：
   - `baseline_view/registration_before_after_view.html`
   - `tensor_view/registration_before_after_view.html`

2. **每个 HTML 都是 3 panel 横排**：Before（原始）/ Initial（初值变换后）/ Final（配准后）。
   鼠标拖动每个 canvas 调整视角，**让踝关节关键面露出**（建议俯视角度，便于看到源点云贴合度）。

3. **建议截 4 张图**（论文 Figure 5 候选）：
   - `figure5_baseline_3panel.png`：baseline_view 完整 3 panel
   - `figure5_tensor_3panel.png`：tensor_view 完整 3 panel
   - `figure5_baseline_final_zoom.png`：baseline_view 的 Final panel 局部放大
   - `figure5_tensor_final_zoom.png`：tensor_view 的 Final panel 局部放大

4. **存档**：截图放到本目录下的 `screenshots/` 子文件夹，commit 进 git。

## 论文叙事价值

- Figure 5：直观展示配准前后源点云对齐程度
- 双后端 final panel 的肉眼对比：Tensor 的源点贴合度更紧（与 RMSE 0.40 vs 0.59 mm 的数字证据呼应）
- 配 Table 1+3 数据，从数字到视觉的双证据

## 复现命令

```bash
# 跑 baseline 视图
cd build_x64/Release
./registration_core_meshgpu_smoke_test.exe advanced_icp_real_bone_registration_visualization_exports_before_after_clouds
cp -r summaries/real_bone_registration_visualization \
  ../../../docs/superpowers/results/2026-06-08-w4b-visualization/baseline_view

# 跑 tensor 视图
MEDICALPRO_USE_TENSOR_ICP=1 ./registration_core_meshgpu_smoke_test.exe advanced_icp_real_bone_registration_visualization_exports_before_after_clouds
cp -r summaries/real_bone_registration_visualization \
  ../../../docs/superpowers/results/2026-06-08-w4b-visualization/tensor_view
```

## 关键数据点（来自双视图的 metrics.csv）

| 视图 | 源点 | 目标采样 | initial_paired (mm) | final_paired (mm) | 备注 |
|---|---:|---:|---:|---:|---|
| baseline | 240 | 5000 | 3.37 | （见 baseline_view/metrics.csv） | |
| tensor | 240 | 5000 | 3.37 | （见 tensor_view/metrics.csv） | |

