# Main Workspace Merge Handoff

**日期：** 2026-05-26
**适用范围：** 主工作区 `d:\Qtproject\medicalpro`
**目的：** 记录“配准并行加速 + 数字孪生/导航”从 worktree 回灌到主工作区后的当前状态、建议提交边界和暂不提交项，方便后续收口与提交。

---

## 1. 当前已验证状态

- 主工作区已经完成两类成果的合并：
  - 配准并行加速主链
  - 数字孪生 / 导航主链
- 已完成全量回归验证：
  - 命令：`ctest --test-dir build_x64_v142 -C Release --output-on-failure -j 8`
  - 结果：`98/98 PASS`
- 已确认此前出现的 7 个失败项不是本次功能逻辑缺陷，而是测试目标未重编或产物未补齐导致的运行时不一致；在补齐相关 target 构建后已恢复通过。
- 已保留本轮主工作区与 worktree 的外部备份：
  - `D:\mb\20260526-181143`

---

## 2. 本轮变更的主题划分

### 2.1 配准并行加速

核心目标是把当前 `PointRegistration -> RegistrationCore -> MeshGPU` 路线扩展为“目标敏感采点 + 多初值并行搜索 + coarse-to-fine + 局部 refine”的方案 B。

关键文件：

- `Plugins/PointRegistration/PointRegistrationDataStructures.h`
- `Plugins/PointRegistration/PointRegistrationServiceImpl.cpp`
- `Plugins/PointRegistration/widgets/PointRegistrationWidget.cpp`
- `Plugins/PointRegistration/widgets/PointRegistrationWidget.h`
- `Plugins/RegistrationCore/RegistrationServiceImpl.cpp`
- `Plugins/RegistrationCore/RegistrationServiceImpl.h`
- `Plugins/RegistrationCore/CMakeLists.txt`
- `Plugins/RegistrationCore/ankle_registration_parallel_search.h`
- `Plugins/RegistrationCore/ankle_registration_parallel_search.cpp`
- `algorithms/meshgpu/include/mesh_gpu_runtime_api.h`
- `algorithms/meshgpu/src/mesh_gpu_interface.cu`

对应测试与实验支撑：

- `tests/unit/AnkleRegistrationParallelSearchTest.cpp`
- `tests/unit/RegistrationCoreMeshGpuSmokeTest.cpp`
- `tests/unit/PointRegistrationRegistrationCoreIntegrationTest.cpp`
- `tests/unit/PointRegistrationWidgetContractTest.cpp`
- `tests/unit/Innovation2RegistrationExperimentTest.cpp`
- `tests/unit/InnovationExperimentBatchRunnerTest.cpp`
- `tests/unit/NavigationEvaluationServiceTest.cpp`

本块功能关键词：

- 目标区域约束两阶段配准
- GPU 候选初值批量打分
- 并行搜索指标写回 `metrics`
- 实验输出可直接支撑论文章节

### 2.2 数字孪生 / 导航

核心目标是在现有导航实时位姿链上加一层“误差感知数字孪生状态”，并把 target region、risk、confidence、HUD、VTK 表达和评估输出串起来。

关键文件：

- `Framework/Navigation/ankle_navigation_types.h`
- `Framework/Navigation/navigation_digital_twin_state_builder.h`
- `Framework/Navigation/navigation_digital_twin_state_builder.cpp`
- `Framework/Navigation/navigation_pose_frame.h`
- `Framework/Navigation/navigation_pose_frame.cpp`
- `Framework/Navigation/navigation_pose_stream.h`
- `Framework/Navigation/navigation_pose_stream.cpp`
- `Framework/Navigation/navigation_transform_graph.h`
- `Framework/Navigation/navigation_transform_graph.cpp`
- `UI/NewPages/Navigation/navigation_runtime_state.h`
- `UI/NewPages/Navigation/navigation_runtime_state.cpp`
- `UI/NewPages/Navigation/navigation_runtime_coordinator.h`
- `UI/NewPages/Navigation/navigation_runtime_coordinator.cpp`
- `UI/NewPages/Navigation/navigation_vtk_bridge.h`
- `UI/NewPages/Navigation/navigation_vtk_bridge.cpp`
- `UI/Widgets/Navigation3DViewWidget.h`
- `UI/Widgets/Navigation3DViewWidget.cpp`
- `UI/NewPages/Navigation/navigation_workspace_types.h`
- `UI/NewPages/Navigation/navigation_workspace_snapshot_store.cpp`
- `UI/NewPages/Navigation/navigation_workspace_application_service.cpp`
- `UI/NewPages/Navigation/navigation_workspace_ui_binder.h`
- `UI/NewPages/Navigation/navigation_workspace_ui_binder.cpp`
- `UI/NewPages/NavigationPage.cpp`
- `UI/NewPages/NavigationPage.h`
- `UI/Forms/NavigationPage.ui`

对应测试与评估支撑：

- `tests/unit/NavigationDigitalTwinStateBuilderTest.cpp`
- `tests/unit/NavigationPoseStreamTest.cpp`
- `tests/unit/NavigationTransformGraphTest.cpp`
- `tests/unit/NavigationRuntimeStateTest.cpp`
- `tests/unit/NavigationRuntimeCoordinatorContractTest.cpp`
- `tests/unit/NavigationVtkBridgeTest.cpp`
- `tests/unit/NavigationWorkspaceApplicationServiceTest.cpp`
- `tests/unit/NavigationWorkspaceSnapshotStoreTest.cpp`
- `tests/unit/NavigationEvaluationServiceTest.cpp`
- `tests/unit/NavigationEvaluationSummaryFormatterTest.cpp`
- `tests/unit/AnkleNavigationWorkflowContractTest.cpp`

本块功能关键词：

- target region 几何上下文
- realtime pose / transform graph
- digital twin state builder
- 风险提示、可信度、重配准建议
- HUD / VTK / summary 导出联动

### 2.3 病例资产与导航主链兼容补丁

这是本次回灌后为了让主工作区已有病例/导航链重新和新类型结构接上而补的兼容层，建议跟数字孪生提交放在一起，不要单独拆。

关键文件：

- `Framework/Navigation/ankle_case_workspace_repository.cpp`
- `Framework/Navigation/real_case_asset_bootstrapper.h`
- `Framework/Navigation/real_case_asset_bootstrapper.cpp`
- `Framework/Navigation/real_case_workspace_seed_coordinator.h`
- `Framework/Navigation/real_case_workspace_seed_coordinator.cpp`
- `Framework/Platform/UiBridge/NavigationPageServiceAccess.cpp`
- `tests/unit/AnkleCaseWorkspaceRepositoryTest.cpp`
- `tests/unit/RealCaseAssetBootstrapperTest.cpp`
- `tests/unit/RealCaseWorkspaceSeedCoordinatorTest.cpp`

兼容点摘要：

- 补回 `AnkleInstrumentAsset`
- 补回 `AnkleCaseManifest.instrumentAssets`
- 让病例工作区、资产引导器、导航 workspace snapshot 继续可序列化、可读取

### 2.4 构建、入口和测试注册

这部分是两条主链共用的基础收口层，提交时可以随主功能一起走，不建议另开独立提交。

关键文件：

- `CMakeLists.txt`
- `tests/unit/CMakeLists.txt`
- `Framework/Platform/Kernel/PlatformRuntimeConfig.cpp`
- `config/platform_runtime.json`
- `main.cpp`
- `UI/AppTheme.h`
- `UI/styles/three_pages_theme.qss`
- `UI/NewPages/ManagementPage.cpp`
- `UI/NewPages/ManagementPage.h`
- `tests/unit/PlatformStartupCoordinatorTest.cpp`
- `tests/unit/PlatformUiBridgeTest.cpp`

---

## 3. 建议的提交边界

### 提交 A：配准并行加速

建议提交信息：

`feat: add parallel target-sensitive ankle registration pipeline`

建议纳入：

- `Plugins/PointRegistration/**`
- `Plugins/RegistrationCore/**`
- `algorithms/meshgpu/**`
- `Framework/Navigation/innovation_2_registration_experiment.cpp`
- `Framework/Navigation/innovation_3_gate_experiment.cpp`
- `Framework/Navigation/innovation_3_gate_experiment.h`
- `Framework/Navigation/innovation_experiment_batch_runner.cpp`
- `Framework/Navigation/innovation_summary_csv_exporter.cpp`
- `Framework/Navigation/navigation_evaluation_service.cpp`
- `tests/unit/AnkleRegistrationParallelSearchTest.cpp`
- `tests/unit/RegistrationCoreMeshGpuSmokeTest.cpp`
- `tests/unit/PointRegistrationRegistrationCoreIntegrationTest.cpp`
- `tests/unit/PointRegistrationWidgetContractTest.cpp`
- `tests/unit/Innovation2RegistrationExperimentTest.cpp`
- `tests/unit/Innovation3GateExperimentTest.cpp`
- `tests/unit/InnovationExperimentBatchRunnerTest.cpp`
- `tests/unit/NavigationEvaluationServiceTest.cpp`
- `docs/superpowers/specs/2026-05-22-ankle-registration-parallel-acceleration-design.md`
- `docs/superpowers/specs/2026-05-23-ankle-registration-parallel-acceleration-experiment-guide.md`
- `docs/superpowers/plans/2026-05-23-ankle-registration-parallel-acceleration-implementation-plan.md`
- `docs/current_status_and_project_overview.md`

### 提交 B：数字孪生 / 导航

建议提交信息：

`feat: add error-aware ankle navigation digital twin workflow`

建议纳入：

- `Framework/Navigation/ankle_navigation_types.h`
- `Framework/Navigation/navigation_digital_twin_state_builder.*`
- `Framework/Navigation/navigation_pose_frame.*`
- `Framework/Navigation/navigation_pose_stream.*`
- `Framework/Navigation/navigation_transform_graph.*`
- `Framework/Navigation/ankle_case_workspace_repository.cpp`
- `Framework/Navigation/real_case_asset_bootstrapper.*`
- `Framework/Navigation/real_case_workspace_seed_coordinator.*`
- `Framework/Platform/UiBridge/NavigationPageServiceAccess.cpp`
- `UI/NewPages/Navigation/**`
- `UI/NewPages/NavigationPage.*`
- `UI/Forms/NavigationPage.ui`
- `UI/Widgets/Navigation3DViewWidget.*`
- `UI/AppTheme.h`
- `UI/styles/three_pages_theme.qss`
- `tests/unit/NavigationDigitalTwinStateBuilderTest.cpp`
- `tests/unit/NavigationPoseStreamTest.cpp`
- `tests/unit/NavigationTransformGraphTest.cpp`
- `tests/unit/NavigationRuntimeStateTest.cpp`
- `tests/unit/NavigationRuntimeCoordinatorContractTest.cpp`
- `tests/unit/NavigationVtkBridgeTest.cpp`
- `tests/unit/NavigationWorkspaceApplicationServiceTest.cpp`
- `tests/unit/NavigationWorkspaceSnapshotStoreTest.cpp`
- `tests/unit/NavigationEvaluationSummaryFormatterTest.cpp`
- `tests/unit/AnkleNavigationWorkflowContractTest.cpp`
- `tests/unit/AnkleCaseWorkspaceRepositoryTest.cpp`
- `tests/unit/RealCaseAssetBootstrapperTest.cpp`
- `tests/unit/RealCaseWorkspaceSeedCoordinatorTest.cpp`
- `docs/superpowers/specs/2026-05-23-ankle-navigation-error-aware-digital-twin-design.md`
- `docs/superpowers/specs/2026-05-25-ankle-navigation-error-aware-digital-twin-experiment-guide.md`
- `docs/superpowers/plans/2026-05-25-ankle-navigation-error-aware-digital-twin-implementation-plan.md`
- `docs/superpowers/specs/2026-05-08-navigation-realtime-pose-digital-twin-design.md`
- `docs/superpowers/specs/2026-05-11-navigation-workspace-professional-research-ui-design.md`

### 提交 C：通用构建与测试接线

如果前两块在 `git add -p` 时很难切干净，可以保留一个小的收口提交。

建议提交信息：

`chore: wire navigation and registration integration targets`

建议纳入：

- `CMakeLists.txt`
- `tests/unit/CMakeLists.txt`
- `Framework/Platform/Kernel/PlatformRuntimeConfig.cpp`
- `config/platform_runtime.json`
- `main.cpp`
- `UI/NewPages/ManagementPage.cpp`
- `UI/NewPages/ManagementPage.h`
- `tests/unit/PlatformStartupCoordinatorTest.cpp`
- `tests/unit/PlatformUiBridgeTest.cpp`

---

## 4. 暂不建议直接提交的文件

这些更像运行产物、实验结果、临时输出或本地数据，建议先排除：

- `ankle_contract_out.txt`
- `contract_out.txt`
- `meshgpu_smoke_stdout.txt`
- `meshgpu_smoke_stderr.txt`
- `nav_appsvc_test_out.txt`
- `nav_appsvc_test_result.txt`
- `nav_contract_test_output.txt`
- `nav_vtk_test_output.txt`
- `point_registration_stdout.txt`
- `point_registration_stderr.txt`
- `point_reg_integration_stdout.txt`
- `point_reg_integration_stderr.txt`
- `data/medical.db`
- `summaries/innovation_1_summary.csv`
- `summaries/innovation_2_summary.csv`
- `summaries/innovation_3_summary.csv`
- `geometry/geometry10.ini`
- `geometry/geometry40.ini`
- `geometry/geometry60.ini`

---

## 5. 文档状态

当前与本轮成果直接相关、建议保留的文档包括：

- `docs/superpowers/specs/2026-05-22-ankle-registration-parallel-acceleration-design.md`
- `docs/superpowers/plans/2026-05-23-ankle-registration-parallel-acceleration-implementation-plan.md`
- `docs/superpowers/specs/2026-05-23-ankle-registration-parallel-acceleration-experiment-guide.md`
- `docs/superpowers/specs/2026-05-23-ankle-navigation-error-aware-digital-twin-design.md`
- `docs/superpowers/plans/2026-05-25-ankle-navigation-error-aware-digital-twin-implementation-plan.md`
- `docs/superpowers/specs/2026-05-25-ankle-navigation-error-aware-digital-twin-experiment-guide.md`

如果后面要整理毕业论文素材，建议优先从这 6 份文档出发，不要再从零归纳。

---

## 6. 推荐下一动作

建议按下面顺序继续：

1. 先根据本文件把“暂不提交项”排除出本轮提交视野。
2. 再按“提交 A / 提交 B / 提交 C”做 `git add -p` 分组。
3. 每组 staged 后分别执行一次最小回归。
4. 最后再决定是本地提交，还是继续拆分后再合并。

最小回归建议：

- 配准组：
  - `ctest --test-dir build_x64_v142 -C Release -R "ankle_registration_parallel_search_test|registration_core_meshgpu_smoke_test|point_registration_registration_core_integration_test|innovation_2_registration_experiment_test|innovation_3_gate_experiment_test|innovation_experiment_batch_runner_test" --output-on-failure`
- 数字孪生 / 导航组：
  - `ctest --test-dir build_x64_v142 -C Release -R "navigation_digital_twin_state_builder_test|navigation_pose_stream_test|navigation_transform_graph_test|navigation_runtime_coordinator_contract_test|navigation_vtk_bridge_test|navigation_evaluation_service_test|navigation_evaluation_summary_formatter_test|ankle_navigation_workflow_contract_test" --output-on-failure`
- 最终整体验证：
  - `ctest --test-dir build_x64_v142 -C Release --output-on-failure -j 8`
