# Navigation Realtime Pose Digital Twin End-to-End Checklist

**日期：** 2026-05-08  
**适用范围：** `docs/superpowers/specs/2026-05-08-navigation-realtime-pose-digital-twin-design.md` 首版验收  
**目的：** 把“代码已实现并通过自动化验证”升级为“按病例主链完成一次人工端到端联调确认”

---

## 1. 当前已确认基线

以下项目已经过自动化或启动级验证，可作为人工联调前提：

- 主程序可构建：
  - `cmake --build build_x64_v142 --config Release --target medicalpro`
- 相关测试已通过：
  - `navigation_runtime_state_test`
  - `navigation_pose_stream_test`
  - `navigation_transform_graph_test`
  - `navigation_runtime_coordinator_contract_test`
  - `navigation_vtk_bridge_test`
  - `ankle_navigation_workflow_contract_test`
  - `navigation_evaluation_service_test`
  - `navigation_evaluation_summary_formatter_test`
- 主程序已做过一次启动冒烟：
  - `medicalpro.exe` 可启动，5 秒内未异常退出

这说明：

- 代码层面可编译、可启动
- realtime pose / digital twin 相关实现项已经落地
- 但“病例主链人工跑通”仍需按本文清单执行一次

---

## 2. 联调目标

本次人工联调要确认三类结果：

### 2.1 功能验收

- 病例工作区真源能带出骨模型、器械模型、标定结果、配准结果
- 导航单窗口能同时显示骨 STL 与当前器械 STL
- 模拟器或跟踪输入能驱动器械位姿实时更新
- tracking 丢失时，器械显示进入隐藏或不可用状态

### 2.2 架构验收

- 页面只负责展示和操作，不直接持有坐标链计算
- 坐标链计算发生在 `navigation_transform_graph`
- 运行时调度发生在 `navigation_runtime_coordinator`
- VTK 渲染更新发生在 `navigation_vtk_bridge`

### 2.3 评估验收

- 系统能记录连续位姿帧
- 系统能产出 `tracking_latency_ms`
- 系统能产出 `tracking_jitter_mm`
- 系统能产出 `visible_frame_ratio`

---

## 3. 执行前准备

执行人先确认以下条件：

- [ ] 使用的是包含本轮实现的本地工作区
- [ ] `build_x64_v142/Release/medicalpro.exe` 已成功构建
- [ ] 至少有一个可进入的病例
- [ ] 该病例已绑定骨模型资产，不是临时手选路径
- [ ] 至少有一个器械定义包含可用 `modelFilePath`
- [ ] 已具备模拟跟踪输入，或具备可替代的真实跟踪输入
- [ ] 若做完整主链联调，已准备好可用的标定结果与配准结果生成路径

推荐先执行一次自动化回归：

```powershell
ctest --test-dir build_x64_v142 -C Release -R "navigation_runtime_state_test|navigation_pose_stream_test|navigation_transform_graph_test|navigation_runtime_coordinator_contract_test|navigation_vtk_bridge_test|ankle_navigation_workflow_contract_test|navigation_evaluation_service_test|navigation_evaluation_summary_formatter_test" --output-on-failure
```

可选补充 smoke：

```powershell
ctest --test-dir build_x64_v142 -C Release -R optical_tracking_quality_snapshot_test --output-on-failure
```

---

## 4. 推荐联调路径

建议严格按下面顺序执行，不要跳步。

### 4.1 启动与病例进入

- [ ] 启动 `medicalpro.exe`
- [ ] 从病例工作台进入一个已有病例的导航页
- [ ] 确认导航页已绑定正确 `caseId`
- [ ] 确认导航页已显示正确 `patientId`
- [ ] 确认导航页已显示正确 `patientName`

通过标准：

- 当前导航页上下文来自病例工作区，而不是独立临时会话

### 4.2 骨模型真源���认

- [ ] 确认当前活动骨模型来自病例绑定资产
- [ ] 确认骨模型路径属于病例工作区真源，不是页面内临时导入
- [ ] 进入导航单窗口后，骨 STL 可以稳定显示

通过标准：

- 骨模型显示正确
- 骨模型来源正确

### 4.3 器械模型真源确认

- [ ] 在准备环节确认当前活动器械
- [ ] 确认该器械存在正确 `modelFilePath`
- [ ] 确认器械 STL 或几何文件可加载
- [ ] 进入导航显示后，当前器械模型可见

通过标准：

- 当前显示器械与病例工作区选中器械一致

### 4.4 跟踪接入确认

- [ ] 连接模拟器或真实跟踪输入
- [ ] 确认系统形成有效 `trackingSessionId`
- [ ] 确认系统形成有效 `navigationToolId`
- [ ] 确认最新位姿帧持续刷新

通过标准：

- 系统能稳定接收连续 `NavigationPoseFrame`

### 4.5 标定确认

- [ ] 在准备环节完成探针或器械标定
- [ ] 确认当前器械存在 calibration transform
- [ ] 确认未完成标定前，导航不可进入或显示阻塞原因
- [ ] 确认完成标定后，相关阻塞解除

通过标准：

- `tracking -> calibrated tool` 链路成立

### 4.6 配准确认

- [ ] 完成当前骨部位所需配准
- [ ] 确认当前病例存在 registration transform
- [ ] 确认未完成配准前，最终导航位姿不可用
- [ ] 确认完成配准后，导航页开始具备有效位姿更新

通过标准：

- `calibrated tool -> patient -> vtk world` 链路成立

### 4.7 导航单窗口确认

- [ ] 进入导航环节
- [ ] 确认页面是单一 3D 虚拟空间
- [ ] 确认该空间内同时显示：
  - 骨骼 STL
  - 当前探针或器械 STL
  - 实时位姿变化结果
- [ ] 持续观察 10 秒，确认器械模型随位姿连续运动

通过标准：

- 单窗口数字孪生链路成立
- 实际效果符合“骨 + 器械 + 实时位姿”的页面定义

### 4.8 tracking 丢失与恢复确认

- [ ] 人为制造 tracking 不可见状态
- [ ] 确认器械 actor 被隐藏，或显示为不可用
- [ ] 恢复 tracking
- [ ] 确认器械 actor 恢复显示并继续更新位姿

通过标准：

- 系统不伪造不可见时的姿态
- 丢失和恢复行为符合 spec 首版约束

### 4.9 评估结果确认

- [ ] 完成至少 10 秒连续导航
- [ ] 确认评估模块记录连续位姿帧
- [ ] 确认可查看或导出 `tracking_latency_ms`
- [ ] 确认可查看或导出 `tracking_jitter_mm`
- [ ] 确认可查看或导出 `visible_frame_ratio`

通过标准：

- realtime pose 相关指标已经进入现有评估体系

---

## 5. 建议记录模板

每次联调建议按下面格式记录：

```md
## 2026-05-08 Realtime Pose E2E Check

- 病例：
- 骨部位：
- 器械：
- 跟踪输入：模拟器 / 真实设备
- 标定结果：成功 / 失败
- 配准结果：成功 / 失败
- 导航单窗口显示：成功 / 失败
- 丢失恢复行为：成功 / 失败
- tracking_latency_ms：
- tracking_jitter_mm：
- visible_frame_ratio：
- 问题记录：
- 结论：通过 / 不通过
```

---

## 6. 通过判定

本轮可以判定“系统已人工跑通”的最小条件是：

- [ ] 病例主链进入正常
- [ ] 骨模型与器械模型均来自病例工作区真源
- [ ] 标定完成后可建立 calibration transform
- [ ] 配准完成后可建立 registration transform
- [ ] 导航单窗口内器械 STL 能随实时位姿连续运动
- [ ] tracking 丢失时系统不继续显示伪造姿态
- [ ] `tracking_latency_ms`、`tracking_jitter_mm`、`visible_frame_ratio` 可被记录

如果以上任一项失败，则当前结论只能写为：

- 代码实现完成
- 自动化验证通过
- 程序可启动
- 但端到端人工联调未完全通过

---

## 7. 当前建议

下一步不要继续扩功能，先做这份清单的一次真实执行。  
只有这份清单跑完并记录结果，才能把当前状态从"实现完成"升级为"系统联调通过"。

---

## 8. UI 收口附加确认（2026-05-08 navigation-workspace-ui-realignment）

- 准备页不再依赖页面内导入器械或骨模型（importInstrumentButton 点击只提示使用病例工作包）
- 规划页不再依赖旧分割 / 旧手动导模（autoSegmentButton / exportSTLButton / loadModelButton / toggleModelButton 已下线）
- 配准页不再保留 2D-3D 占位入口（load2DImageButton / start2D3DRegButton 已下线）
- 导航页主视图为单一 3D 虚拟空间（旧 Axial/Sagittal/Coronal/3D Volume 四视图标签已隐藏）
- 评估页主区已改为 NavigationWorkspaceUiBinder::applyEvaluationSummary 生成的病例级摘要
