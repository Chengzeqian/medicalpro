# W4-B 配准前/后可视化（已发现 bug，blocked）

**状态**：blocked。`advanced_icp_real_bone_registration_visualization_exports_before_after_clouds`
测试导出的 `parallel_final_transform_matrix` 与 `initial_transform_matrix` 完全相同——
`service.performICPRegistrationAdvanced()` 在该测试参数下**根本没跑 GICP**，
直接返回了初值。两个后端跑出来视觉上一模一样，不能用作 Figure 5 对比图。

## 已验证事实

1. baseline 和 tensor 跑出来的 `metrics.csv` 数值完全一致
2. baseline 和 tensor 跑出来的 `parallel_final_transform_matrix.csv` 矩阵完全一致
3. 加大扰动 (2.4mm, 3.5°) → (5.5mm, 7°) 后 paired_residual 从 3.37 → 6.95mm，
   但 `parallel_final` 仍 = `initial` = 6.95mm，说明 service 在该路径下不进入 GICP
4. W2 stress matrix 数据真实可信（不同后端 RMSE 真有差异），与本测试无关

## 推测的根因（未验证）

`RegistrationServiceImpl::performICPRegistrationAdvanced` 在
`enablePairedResidualGuard=true` + admission gate 触发 reject/fast_path 时
不 fallback 到 direct GICP path，直接返回输入的 initial 矩阵。
真正修需要进 service 内部调试 admission 决策链 + reject 后的 fallback 行为，
工作量超出 W4-B 一图截图的范围。

## 论文 Figure 5 替代方案

不依赖此测试，改从 W4-A stress matrix 数据手工生成 Figure 5：
- `docs/superpowers/results/2026-06-08-w4a-extended-stress/baseline_extended.csv`
- `docs/superpowers/results/2026-06-08-w4a-extended-stress/tensor_extended.csv`

这两份 CSV 的 `direct_paired_mm` / `direct_rmse_mm` 列在多个扰动场景下
**确有差异**（W3-C/W4-A 文档表格已经验证）。可用 matplotlib/plotly 把
其中一个真实有差异的 case（如 `combined_extreme`：baseline 0.81mm vs tensor 0.56mm）
源点云画成 3D 散点图，作为 Figure 5。

## 下次调研入口（如果要修 visualization 测试）

```bash
# 在 RegistrationServiceImpl 中查看 admission decision 后的处理
grep -n "fast_path\|admission_action\|action ==" Plugins/RegistrationCore/RegistrationServiceImpl.cpp
```

关键代码点：[RegistrationServiceImpl.cpp:2698](../../../Plugins/RegistrationCore/RegistrationServiceImpl.cpp#L2698)
判断 `initialAdmissionDecision.action == "fast_path"` 时直接走捷径，
但 reject 路径下的 fallback 行为需要调试日志才能确定。

## 历史记录

- 早期 commit `5800c99`：写了截图操作指南，假设可视化测试会输出真实差异化结果
- 接续 commit（本次 revert）：发现两个后端输出完全相同，删除误导性归档
- 这个 README 是 W4-B 的最终状态：blocked，记录已知问题
