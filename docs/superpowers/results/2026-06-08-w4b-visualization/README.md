# W4-B 配准前/后可视化（截图待补）

**状态**：数据已就绪，截图待用户手动补充。

## 已有产物

测试 `advanced_icp_real_bone_registration_visualization_exports_before_after_clouds`
导出在：

```
build_x64/Release/summaries/real_bone_registration_visualization/
├── registration_before_after_view.html  ← 可直接浏览器打开的 3D view
├── source_raw.csv                        ← 配准前源点云
├── source_initial_transformed.csv        ← 初值变换后
├── source_parallel_final_transformed.csv ← 配准最终结果
├── target_surface_sample.csv             ← 目标表面采样
├── target_probe.csv                      ← 目标 probe 点
├── initial_transform_matrix.csv
├── parallel_final_transform_matrix.csv
├── metrics.csv
└── registration_metadata.csv
```

## 截图步骤（用户操作）

1. **跑双后端各一次以获取最新数据**：
   ```bash
   cd build_x64/Release
   ./registration_core_meshgpu_smoke_test.exe advanced_icp_real_bone_registration_visualization_exports_before_after_clouds
   # 截 baseline 截图
   MEDICALPRO_USE_TENSOR_ICP=1 ./registration_core_meshgpu_smoke_test.exe advanced_icp_real_bone_registration_visualization_exports_before_after_clouds
   # 截 tensor 截图
   ```

2. **打开 HTML view**：
   - 浏览器打开 `summaries/real_bone_registration_visualization/registration_before_after_view.html`
   - 3 个 panel：Before（raw source vs target）/ Initial（初值变换后）/ Final（配准后）
   - 鼠标拖动各个 canvas 调整视角，露出踝关节关键面
   - 浏览器截图（Win+Shift+S）保存 PNG

3. **建议截图组合（论文 Figure 5 候选）**：
   - 大图：3 panel 横排，Final panel 重点
   - 小图：单独 Final panel 的 baseline vs tensor 对比（两次跑分别截）
   - 命名：`figure5_before_initial_final.png`、`figure5_baseline_vs_tensor_final.png`

4. **存档位置**：把截图放进
   `docs/superpowers/results/2026-06-08-w4b-visualization/` 并 commit

## 论文叙事价值

- Figure 5：直观展示配准前后源点云对齐程度
- 对照截图：肉眼可见的 Tensor ICP 改进（特别是边缘点贴合度）
- 配 Table 1+3 RMSE 数据，从数字到视觉的双证据

## 复现命令一行

```bash
cd build_x64/Release && \
  ./registration_core_meshgpu_smoke_test.exe advanced_icp_real_bone_registration_visualization_exports_before_after_clouds && \
  start summaries/real_bone_registration_visualization/registration_before_after_view.html
```
