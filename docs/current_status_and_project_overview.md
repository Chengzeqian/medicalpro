## Platform Kernel Governance

### 2026-04-21 Plugin Chain Remediation Phase 2 Acceptance

- `RegistrationCore` and `OpticalTracking` now enter through governed on-demand activation instead of direct CTK start shortcuts.
- `ensureReady(plugin_id)` now uses strict `descriptor -> install/start -> service_ready -> health_check` semantics.
- `platformReady` remains bound to the Phase 1 startup scope, while on-demand plugins now belong to governed scope and diagnostics.
- `observe_only`, `facade_mode`, and `orchestrate_core` now report consistent on-demand activation semantics.

### 2026-04-20 Plugin Chain Remediation Phase 1 Acceptance

- Default runtime mode is now `facade_mode` for the product startup path.
- The managed startup scope is now limited to `UserManagement`, `DicomViewer`, and `FourViewDisplay`.
- Startup install and core activation now run from a descriptor-driven managed plan instead of scanning the entire `plugins/` directory as the main truth source.
- `platform ready` now evaluates only the managed Phase 1 scope.
- `ready` remains limited to `service registration + required plugin/capability satisfaction + lightweight health checks`.
- Warmup is no longer part of the blocking ready path in Phase 1.
- Executed command (build):
  - `cmake --build build_x64 --config Release --target medicalpro platform_descriptor_loader_test platform_managed_plugin_plan_test platform_dependency_graph_test platform_startup_coordinator_test platform_diagnostics_service_test platform_warmup_coordinator_test startup_orchestrator_lifecycle_test`
- Executed command (ctest):
  - `ctest --test-dir build_x64 -C Release -R "platform_descriptor_loader_test|platform_managed_plugin_plan_test|platform_dependency_graph_test|platform_startup_coordinator_test|platform_diagnostics_service_test|platform_warmup_coordinator_test|startup_orchestrator_lifecycle_test|platform_descriptor_runtime_layout_test" --output-on-failure`
- Executed command (legacy-entry scan):
  - `rg -n "installPluginsFromDirectory\\(|loadPluginPolicy\\(" main.cpp`
- Expected outcomes alignment and actual results:
  - `medicalpro` build target: PASS.
  - Phase 1 plugin-chain suite: `8/8 PASS`.
  - Legacy entry scan: no output.

### 2026-04-20 Startup Lifecycle Diagnostics Infrastructure Acceptance

- The lifecycle diagnostics foundation is now landed in this worktree through `PlatformLifecycleTraceRecorder`, `PlatformPluginLifecycleAggregator`, and the enriched `PlatformDiagnosticsService` aggregation path.
- Startup facts are now modeled as governed lifecycle events covering `install`, `start`, `service_ready`, `warmup`, `failed`, `degraded`, and `skipped_by_mode`.
- `ready-path` and `warmup-tail` are now derived separately, and diagnostics can now identify the slowest plugin, the blocking point, the failure point, CTK/platform state mismatch, and stable recovery hints.
- The landed behavior stays aligned with all three runtime modes instead of collapsing back to direct CTK inspection.
- Executed command (build):
  - `cmake --build build_x64_noctk --config Release --target medicalpro platform_startup_trace_recorder_test platform_plugin_lifecycle_aggregator_test platform_startup_coordinator_test platform_diagnostics_service_test platform_ui_bridge_test startup_orchestrator_lifecycle_test`
- Executed command (ctest):
  - `ctest --test-dir build_x64_noctk -C Release -R "platform_startup_trace_recorder_test|platform_plugin_lifecycle_aggregator_test|platform_startup_coordinator_test|platform_diagnostics_service_test|platform_ui_bridge_test|startup_orchestrator_lifecycle_test" --output-on-failure`
- Executed command (CTK direct-call scan):
  - `rg -n "CTKManager::instance\(|getService<" UI\NewPages UI\MainInterfaceWidget.cpp`
- Expected outcomes alignment and actual results:
  - `medicalpro` build target: PASS (build command exit code 0).
  - Startup lifecycle diagnostics infrastructure suite: `7/7 PASS` (ctest reports 100% tests passed, 0 failed out of 7).
  - CTK direct-call scan: no output (`rg` returned no matches).

### 2026-04-20 Startup Diagnostics Page Matrix Follow-up Acceptance

- The diagnostics page now renders the full summary matrix, full plugin lifecycle matrix, full timeline matrix, and full problem matrix defined by the 2026-04-17 diagnostics design.
- `warmup tail` now shows `skipped_by_mode` outside `orchestrate_core`.
- The slowest plugin row and blocking point emphasis are now part of the landed page contract in this worktree.
- Executed command (build):
  - `cmake --build build_x64_noctk --config Release --target medicalpro platform_diagnostics_page_test platform_diagnostics_service_test platform_ui_bridge_test ui_ctk_decoupling_acceptance_test`
- Executed command (ctest):
  - `ctest --test-dir build_x64_noctk -C Release -R "platform_diagnostics_page_test|platform_diagnostics_service_test|platform_ui_bridge_test|ui_ctk_decoupling_acceptance_test" --output-on-failure`
- Executed command (CTK direct-call scan):
  - `rg -n "CTKManager::instance\(|getService<" UI\NewPages UI\MainInterfaceWidget.cpp`
- Expected outcomes alignment and actual results:
  - `medicalpro` build target: PASS (build command exit code 0).
  - Startup diagnostics follow-up suite: `4/4 PASS` (ctest reports 100% tests passed, 0 failed out of 4).
  - CTK direct-call scan: no output (`rg` returned no matches).

### 2026-04-17 Startup Diagnostics Rollout Acceptance

- Task 5 governance write-back is completed for design spec, governance matrix, decision log, and current status tracking.
- The accepted baseline at that time was the lifecycle-event-based diagnostics foundation plus the implementation-plan subset that was already wired through the diagnostics page.
- After the 2026-04-20 infrastructure acceptance and page-matrix acceptance above, the previous `subset` wording should be read as historical rollout context rather than the current worktree limitation.
- This section remains a 2026-04-17 supplement for Task 5 startup diagnostics acceptance evidence, not the latest landed-scope statement.
- Executed command (build):
  - `cmake --build build_x64_noctk --config Release --target medicalpro platform_startup_trace_recorder_test platform_plugin_lifecycle_aggregator_test platform_startup_coordinator_test platform_diagnostics_service_test platform_diagnostics_page_test platform_ui_bridge_test ui_ctk_decoupling_acceptance_test`
- Executed command (ctest):
  - `ctest --test-dir build_x64_noctk -C Release -R "platform_startup_trace_recorder_test|platform_plugin_lifecycle_aggregator_test|platform_startup_coordinator_test|platform_diagnostics_service_test|platform_diagnostics_page_test|platform_ui_bridge_test|ui_ctk_decoupling_acceptance_test" --output-on-failure`
- Executed command (CTK direct-call scan):
  - `rg -n "CTKManager::instance\(|getService<" UI\NewPages UI\MainInterfaceWidget.cpp`
- Expected outcomes alignment and actual results:
  - `medicalpro` build target: PASS (build command exit code 0).
  - Startup diagnostics special test suite: `7/7 PASS` (ctest reports 100% tests passed, 0 failed out of 7).
  - CTK direct-call scan: no output (`rg` returned no matches).

### 2026-04-17 Rollout Status

- Task 9 startup governance routing is complete in this worktree.
- Task 10 UI decoupling close-out is complete in this worktree.
- Verified with `cmake --build build_x64_noctk --config Release --target medicalpro platform_facades_test login_page_platform_providers_test core_pages_platform_providers_test platform_ui_bridge_test ui_ctk_decoupling_acceptance_test`.
- Verified with `ctest --test-dir build_x64_noctk -C Release -R "platform_facades_test|login_page_platform_providers_test|core_pages_platform_providers_test|platform_startup_coordinator_test|platform_descriptor_runtime_layout_test|platform_ui_bridge_test|ui_ctk_decoupling_acceptance_test" --output-on-failure`.
- Login, MainWindow, and MainInterface identity flows now enter through facade/provider wiring instead of direct CTK user-service lookups.
- MainInterface runtime status now enters through `CoreUiRuntimeStatusProvider`, and NavigationPage service loading now enters through `NavigationPageServiceAccess`.
- `rg -n "CTKManager::instance\(|getService<" UI\NewPages UI\MainInterfaceWidget.cpp` now returns no matches.

# MedicalPro 当前状态与项目说明

更新时间：2026-04-16

## 1. 当前已完成工作

### 1.1 构建与运行链路
- 已固定当前主开发与运行入口为 `build_x64/Release/medicalpro.exe`。
- 已确认项目应使用 `x64` 目标，不再使用错误的 `ARM64` 目录或 Kit。
- 已完成 `build_x64` 运行配置梳理，当前应优先从 `build_x64` 目录启动与调试。

### 1.2 调试控制台输出
- `main.cpp` 已启用 Windows 控制台分配逻辑。
- 运行主程序时应具备控制台输出，便于观察启动阶段、插件加载、服务注册和运行期日志。

### 1.3 Welcome / 免登录主链
- Welcome 页面保留，不跳过。
- 当前主链为免登录进入系统。
- 页面流转按以下路径工作：
  - `Welcome -> ModuleSelection`
  - `ModuleSelection -> Management / SystemSettings`
  - `Management -> Dashboard`
  - `Dashboard -> Navigation / Welcome / Management`

### 1.4 插件框架与服务加载修复
- `RegistrationCore` 已加入 `config/plugin_load_policy.json`，策略为 `deferred`。
- `DicomViewer` 已移除不存在的 `org.medicalpro.medicalimagecore` 依赖。
- `Release/plugins` 中遗留的 `SlicerBridge`、`Segmentation`、`SegmentationCore` 文件已清理。
- `Debug` 与 `Release` 目录的 `config` 已补齐并同步。

### 1.5 UI Phase 1
- Welcome / ModuleSelection / Dashboard 三页乱码修复已完成。
- 主题令牌与公共 QSS 已接入。
- 三页已经具备统一主题基础，可继续推进产品化视觉升级。

### 1.6 UI Phase 2 当前进展
- `ThreePagePresentationUtils` 已补齐 WelcomePage 运行摘要与可进入性结论文案，并由 `three_page_presentation_utils_test` 覆盖 WelcomePage 摘要/结论生成逻辑。
- WelcomePage 已升级为医疗旗舰工作站入口页，采用左品牌区 / 右运行摘要区 / 下方三状态卡结构。
- WelcomePage 保留欢迎页入口，不要求登录，主按钮继续进入 `ModuleSelectionPage`。
- WelcomePage 右侧摘要区已接入 3 条核心速览与状态结论标签，用于展示当前系统可进入性与运行概况。
- WelcomePage 下方 3 张状态卡继续承接真实运行状态：
  - 插件框架
  - 关键服务
  - `data` 目录
- WelcomePage 已补齐窄窗口下的上下布局与状态卡重排能力，可在较窄宽度下完成信息区重组。
- 2026-04-15 已完成一轮 WelcomePage 人工视觉验收向微调，重点收口了品牌区层级、主次按钮关系、摘要区信息密度与下方状态卡降噪。

### 1.7 Welcome 品牌资产与控制台日志链路
- WelcomePage 已接入正式品牌资产方案，不再以 `MNS` 作为默认占位。
- 当前品牌资源解析顺序为：
  - `build_x64/Release/data/branding/welcome_logo.png`
  - `build_x64/Release/data/logo.png`
  - `:/resoucce/logo.png`
- 当运行目录与资源都不可用时，WelcomePage 会退回到正式文本品牌 `MedicalPro`，而不是裸字母占位。
- WelcomePage 品牌容器已从小方形调整为更适合横向 wordmark 的尺寸，适配当前内置 `resoucce/logo.png`。
- `main.cpp` 已在控制台初始化后立即安装自定义 Qt 日志桥 `ConsoleLogBridge`。
- 当前 Qt 日志输出策略为：
  - 控制台句柄：走 `WriteConsoleW`
  - 重定向文件/管道：走 UTF-8 `WriteFile`
- 2026-04-15 已补充 Windows Terminal / ConPTY 场景兼容：
  - 当标准错误句柄表现为 `pipe`，但进程实际仍附着控制台时，`ConsoleLogBridge` 会优先回落到 `CONOUT$` 宽字符输出
  - 当标准错误句柄为磁盘文件时，仍保持 UTF-8 重定向输出
- 2026-04-15 已进一步补充 `pipe` 控制台编码策略：
  - 如果标准错误句柄不是磁盘文件、也拿不到可写控制台句柄，则视为伪控制台输出
  - 该路径下日志会按当前控制台 code page 编码，而不再强制写 UTF-8 字节
  - 这样可以避免 `应用程序启动` 一类中文在控制台里被显示成 `搴旂敤绋嬪簭鍚姩`
- 为避免污染重定向日志，`Framework/Core/MedicalDataStructures.cpp` 中 `main()` 前静态注册阶段的调试输出已移除。
- 为兼容第三方库直接输出到控制台的窄字符日志，控制台 code page 已切回系统 ANSI；本项目自有 Qt 日志仍通过宽字符写入，不受该设置影响。

### 1.8 2026-04-15 启动期控制台日志收口
- 基于用户终端实测，Windows 控制台中的中文日志链路仍存在不稳定显示问题。
- 为确保调试时立即可读，启动期关键日志已统一收口为英文/ASCII，覆盖：
  - `main.cpp`
  - `Framework/StartupOrchestrator.cpp`
  - `Framework/VTKWidgetFactory.cpp`
- 当前已收口的内容包括：
  - 启动阶段标题
  - CTK / 插件 / 服务预热日志
  - `StartupOrchestrator` 阶段名
  - `StartupOrchestrator::getDiagnosticReport()` 诊断摘要
- 已新增 `startup_console_log_policy_test`，用于回归检查启动链关键日志文本保持英文可读。

### 1.9 2026-04-15 运行期模块日志统一
- 在启动期日志收口之后，已继续把项目自有运行期模块日志按同样策略统一为英文/ASCII，避免调试过程中再次出现中文乱码。
- 本轮收口范围覆盖以下项目自有源码文件：
  - `Plugins/UserManagement/UserManagementActivator.cpp`
  - `Plugins/UserManagement/UserManagementServiceImpl.cpp`
  - `Plugins/Registration2D3D/Registration2D3DServiceImpl.cpp`
  - `Plugins/Registration2D3D/Registration2D3DWidget.cpp`
  - `Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp`
  - `UI/Widgets/Instrument3DPreviewWidget.cpp`
  - `Plugins/OpticalRegistration/widgets/OpticalRegistrationWidget.cpp`
  - `Plugins/OpticalRegistration/internal/OpticalRegistrationVTKWidget.cpp`
- 本轮策略仍然保持边界明确：
  - 只处理项目自有运行期调试日志
  - 不修改 Welcome / Dashboard 等界面中文文案
  - 不强行收口第三方 CTK / VTK / Python 原生日志
- 已新增 `runtime_console_log_policy_test`，用于回归检查上述运行期模块日志中的控制台输出文本保持英文可读。

### 1.10 2026-04-15 UI Phase 2 四页视觉升级续做
- 本轮按“先不碰插件链，继续 UI Phase 2 视觉升级”的要求，把主链正式修正为 `Welcome -> ModuleSelection -> Management -> Dashboard`，并继续推进 `ManagementPage`、`ModuleSelectionPage` 与 `DashboardPage` 的联动落地。
- `ModuleSelectionPage` 已从旧版双按钮页升级为正式门厅页，新增：
  - 顶部 `moduleStatusBar`，展示当前身份、当前时间、系统状态与返回欢迎页入口
  - 中部 `moduleHeroFrame`，作为模块门厅主文案区
  - 两张正式主卡：`ankleSurgeryCard`、`systemSettingsCard`
  - 动态状态标签：`ankleSurgeryStateTag`、`systemSettingsStateTag`
  - 动态提示文案：`ankleSurgeryHintLabel`、`systemSettingsHintLabel`
- `ModuleSelectionPage.cpp` 已接入真实运行态摘要：
  - 使用 `CTKManager::isCTKAvailable()` 与 `getMissingServices(...)` 判断系统状态
  - 顶部系统状态与主卡提示会随关键服务状态刷新
  - 顶部时间会定时刷新
- `ManagementPage` 已从旧版后台表格页升级为正式数据管理中台，新增：
  - 头部框架 `managementHeaderFrame`
  - 三张概览卡：`doctorOverviewCard`、`patientOverviewCard`、`surgeryOverviewCard`
  - 当前工作视图上下文条：`entityContextFrame`
  - 底部流程收口区：`managementFlowFrame`
- `ManagementPage.cpp` 已在不改动原有表格、搜索和 CRUD 槽函数的前提下完成壳层升级：
  - 通过代码包裹现有 header / tabWidget 结构，摆脱旧版内联样式依赖
  - 保留医生 / 患者 / 手术三类数据加载与搜索行为
  - 概览卡、当前视图上下文和进入工作台 CTA 会随当前数据状态刷新
- `DashboardPage` 已从旧版详情堆叠页升级为病例工作台，新增：
  - 顶部 `dashboardHeaderFrame`
  - 四张概览卡：`overviewPatientCard`、`overviewDiagnosisCard`、`overviewDoctorCard`、`overviewDicomCard`
  - 三个内容分区：`basicInfoSectionFrame`、`doctorInfoSectionFrame`、`dicomSectionFrame`
  - 底部导航 CTA：`navigationCtaFrame`、`navigationCtaTitleLabel`、`navigationCtaHintLabel`
- `DashboardPage.cpp` 已同步接入动态状态映射：
  - 选择病例后实时更新顶部概览卡
  - 读取 DICOM 检查数后更新 `overviewDicomCard`
  - 底部导航 CTA 会根据“是否选择病例 / 是否存在 DICOM 检查”切换状态与提示
- `ThreePagePresentationUtils` 与对应单测已继续扩展，新增：
  - `buildModuleAccessHint(...)`
  - `buildDashboardNavigationHint(...)`
  - `buildDashboardNavigationTone(...)`
  - `buildManagementOverviewValue(...)`
  - `buildManagementOverviewHint(...)`
  - `buildManagementEntryHint(...)`
- `UI/styles/three_pages_theme.qss` 已追加 ModuleSelection / Management / Dashboard 的 Phase 2 对象样式，四页产品语言已进一步统一。

### 1.11 2026-04-15 UI Phase 2 四页视觉微调第一轮落地
- 在不改主链、不改插件链、不改控制台日志链的前提下，已完成 Welcome / ModuleSelection / Management / Dashboard 的“验收向微调”第一轮代码落地。
- WelcomePage 本轮继续收口：
  - 左品牌区与右摘要区主次进一步拉开
  - `进入系统` 按钮继续强化，`退出` 进一步弱化
  - 响应式布局下右侧摘要区宽度更克制，下方状态卡压屏感降低
- ModuleSelectionPage 本轮继续收口：
  - 主标题、副标题和 CTA 更明确指向“继续进入数据管理主链”
  - `ankleSurgeryCard` 继续作为唯一主卡
  - `systemSettingsCard` 与 `systemSettingsButton` 已退后为辅助入口层级
- ManagementPage 本轮继续收口：
  - Header、副标题、上下文条和底部 CTA 文案更贴近真实主流程
  - 表格密度、Tab 视觉节奏与概览卡摘要感进一步统一
  - `继续进入病例工作台` 的流程收口感已增强
- DashboardPage 本轮继续收口：
  - 顶部副标题和左侧病例列表说明更聚焦“先选病例，再核对状态”
  - `继续进入导航` CTA 的语气和视觉权重已提升
  - 当前病例概览卡与底部导航区的主次关系更清楚
- `UI/styles/three_pages_theme.qss` 已完成本轮四页公共样式权重重排，重点覆盖：
  - Welcome 品牌区 / 摘要区 / 状态卡
  - ModuleSelection 主卡 / 辅卡 / 按钮分级
  - Management Header / 概览卡 / Context / Table / CTA
  - Dashboard 概览条 / 当前病例卡 / 导航 CTA

## 2. 当前已验证结果

### 2.1 构建验证
以下命令已在 2026-04-14 实际通过：

```powershell
cmake --build --preset x64-release
```

验证结果：
- `Framework` 通过
- `NewPagesLib` 通过
- `medicalpro` 通过
- 输出产物为 `build_x64/Release/medicalpro.exe`

### 2.2 测试验证
以下命令已实际通过：

```powershell
ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure
```

验证结果：
- `three_page_presentation_utils_test` 通过

### 2.3 启动链验证
以下目标程序已在 2026-04-14 实际启动验证：

```powershell
build_x64/Release/medicalpro.exe
```

验证结果：
- 进程可以成功启动，启动后至少保持运行 8 秒，未出现立即崩溃。
- 本轮通过标准错误输出捕获到启动日志，说明启动阶段日志链路仍存在。
- 已捕获到启动日志内容，但终端输出存在编码显示问题。
- 本轮未执行人工视觉验收，因此未对 WelcomePage 最终界面观感做主观确认。

### 2.4 WelcomePage 视觉复核
以下目标程序已在 2026-04-15 实际启动并抓取 WelcomePage 窗口画面：

```powershell
build_x64/Release/medicalpro.exe
```

验证结果：
- 已基于 WelcomePage 的真实运行截图完成一轮视觉复核，而不是仅按 `.ui` / `QSS` 代码推断。
- 左侧品牌区已强化为首屏主焦点，`进入系统` 按钮明显高于 `退出`。
- 右侧运行摘要区保留完整状态信息，但视觉竞争感已低于左侧品牌区。
- 下方 3 张状态卡已去掉多余撑高留白，并降低整卡颜色饱和度，避免与主视觉区争抢注意力。

### 2.5 2026-04-15 品牌资产与日志编码验证
以下命令已在 2026-04-15 实际通过：

```powershell
cmake --build --preset x64-release --config Release
ctest --test-dir build_x64 -C Release --output-on-failure
```

补充验证：

```powershell
Start-Process build_x64/Release/medicalpro.exe -WorkingDirectory build_x64/Release -RedirectStandardError <temp-log>
```

验证结果：
- `branding_and_console_utils_test` 已新增并通过，覆盖：
  - Welcome 品牌路径优先级
  - Welcome 默认品牌回退文案
  - ConsoleLogBridge 日志格式化
  - ConsoleLogBridge UTF-8 重定向字节输出
- 主程序已重新编译通过，`Framework`、`NewPagesLib` 与 `medicalpro` 均构建成功。
- 重定向日志文件已验证可按 UTF-8 正确解码，日志中可正确读取：
  - `Medical Pro 应用程序启动`
  - `阶段1`
- 运行过程中第三方 CTK 直接输出到控制台的中文时间前缀也已恢复正常显示，不再出现明显乱码。
- 已新增输出模式单测，覆盖：
  - 控制台句柄直写
  - 磁盘文件重定向
  - 附着控制台的管道句柄
  - 无附着控制台时的回退重定向
- 已新增控制台 `CP936` 编码单测，验证 `"[DEBUG] 应用程序启动"` 这类中文日志在 pipe 控制台路径下按系统代码页编码输出。
- 本轮未再次做 WelcomePage 人工截图验收，但运行时代码路径已保证：
  - 默认优先显示正式 logo 资源
  - 不再显示 `MNS`

### 2.6 2026-04-15 启动期英文日志验证
以下命令已在 2026-04-15 实际通过：

```powershell
cmake --build --preset x64-release --config Release
ctest --test-dir build_x64 -C Release --output-on-failure
```

补充验证：

```powershell
ctest --test-dir build_x64/tests/unit -C Release --output-on-failure
Start-Process build_x64/Release/medicalpro.exe -WorkingDirectory build_x64/Release -RedirectStandardError <temp-log>
```

验证结果：
- `startup_console_log_policy_test` 已新增并通过，覆盖：
  - `main.cpp` 启动标题与阶段日志
  - `StartupOrchestrator.cpp` 阶段名与诊断摘要模板
  - `VTKWidgetFactory.cpp` 启动期 VTK Widget 日志
- 主程序重新启动后，启动链日志已可直接读取英文文本，例如：
  - `Medical Pro application startup`
  - `Executing phase: VTK initialization`
  - `Startup duration: ...`
- 用户先前看到的 `搴旂敤绋嬪簭鍚姩`、`闃舵1` 一类乱码，不再出现在本项目自有启动期 Qt 日志中。
- 第三方 CTK 直接输出的日期前缀仍可能保持中文，但当前显示正常，不影响调试。

### 2.7 2026-04-15 运行期模块英文日志验证
以下命令已在 2026-04-15 实际通过：

```powershell
ctest --test-dir build_x64/tests/unit -C Release -R runtime_console_log_policy_test -VV
```

补充验证：

```powershell
runtime_console_log_policy_test.exe
```

验证结果：
- `runtime_console_log_policy_test` 已通过，覆盖：
  - `UserManagementActivator`
  - `UserManagementServiceImpl`
  - `Registration2D3DServiceImpl`
  - `Registration2D3DWidget`
  - `OpticalTrackingServiceImpl`
  - `Instrument3DPreviewWidget`
  - `OpticalRegistrationWidget`
  - `OpticalRegistrationVTKWidget`
- 本轮测试会检查目标源码中的运行期日志调用行，确保控制台日志文本不再混入非 ASCII 内容。
- 运行期诊断输出已与启动期策略保持一致，后续在控制台观察模块初始化、用户管理、2D3D 配准、光学跟踪、3D 预览等链路时，可直接读取英文日志。
- 第三方组件直接输出的日志仍保留其原始行为，因此控制台里若再出现中文或其他编码风格，优先视为第三方输出而不是本项目自有 Qt 日志回退。

### 2.8 2026-04-15 最新真实启动验证
以下命令已在 2026-04-15 实际通过：

```powershell
Start-Process build_x64/Release/medicalpro.exe -WorkingDirectory build_x64/Release -RedirectStandardOutput build_x64/Release/logs/medicalpro_stdout_20260415.log -RedirectStandardError build_x64/Release/logs/medicalpro_stderr_20260415.log
```

验证结果：
- 主程序可成功启动，并在 8 秒观察窗口内持续运行，未出现立即崩溃。
- `medicalpro_stderr_20260415.log` 中已确认本项目自有启动链日志为英文可读，例如：
  - `Medical Pro application startup`
  - `Executing phase: CTK framework initialization`
  - `Startup duration: ...`
- `medicalpro_stdout_20260415.log` 中第三方 CTK EventAdmin 原生日志仍保持其原始输出格式。
- 同一次真实启动抓取中仍观察到残留启动问题：
  - `Plugin handle not found for UserManagement`
  - `Critical plugin start failed: "UserManagement"`
- 该问题说明当前“控制台调试输出统一”目标已经达成，但 `UserManagement` 插件发现/安装链仍需单独排查。

### 2.9 2026-04-15 UI Phase 2 代码验证
以下命令已在 2026-04-15 实际通过：

```powershell
cmake --build build_x64 --config Release --target medicalpro
ctest -C Release -R three_page_presentation_utils_test --output-on-failure
```

验证结果：
- `ModuleSelectionPage`、`ManagementPage`、`DashboardPage`、公共 QSS 与展示层辅助函数改动均已纳入主程序构建并通过。
- `medicalpro.exe` 已重新生成，说明本轮四页视觉升级没有破坏当前主构建链。
- `three_page_presentation_utils_test` 继续通过，说明新增的门厅提示、ManagementPage 中台文案与导航 CTA 逻辑已被单测覆盖。
- 本轮已补做 8 秒稳定启动检查，`build_x64/Release/medicalpro.exe` 未出现立即退出。
- 本轮验证为“编译 + 展示层单测”级别，尚未补做新的人工视觉验收截图。

### 2.10 2026-04-15 UI Phase 2 视觉微调第一轮验证
以下命令已在 2026-04-15 实际通过：

```powershell
cmake --build build_x64 --config Release --target medicalpro
ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure
Start-Process build_x64/Release/medicalpro.exe -WorkingDirectory build_x64/Release
```

验证结果：
- Welcome / ModuleSelection / Management / Dashboard 的本轮视觉微调代码已进入主构建链并构建通过。
- `three_page_presentation_utils_test` 继续通过，说明展示层辅助逻辑未被本轮微调破坏。
- `build_x64/Release/medicalpro.exe` 在 8 秒观察窗口内保持运行，未出现“点开即退”的回退。
- 本轮完成的是代码级视觉微调和真实启动级验证，尚未补做新的截图式人工验收。

## 3. 当前项目状态判断
- 当前 `x64-release` 主程序已经可以完整编译。
- Welcome 页入口保留且应作为第一屏显示。
- Welcome 页进入系统后走 `ModuleSelection`，不强制登录。
- Welcome 页本轮相关状态摘要能力已接上。
- Welcome 页正式品牌资产方案已接入。
- 当前 UI Phase 2 主链已统一为 `Welcome -> ModuleSelection -> Management -> Dashboard`。
- `ModuleSelectionPage` 已完成 Phase 2 门厅化改造。
- `ManagementPage` 已完成 Phase 2 数据管理中台化改造。
- `DashboardPage` 已完成 Phase 2 工作台化改造。
- 四页已完成一轮验收向视觉微调代码落地，当前更接近正式产品化观感。
- 本项目自有启动期 Qt 日志已统一收口为英文/ASCII，控制台调试可读性已恢复。
- 本项目自有运行期模块日志也已按相同策略统一，控制台调试输出已具备一致性。
- 但最新真实启动验证显示 `UserManagement` 关键插件激活仍失败，插件加载链还未完全收口。

## 4. 当前仍需注意的事项
- 工作区仍然较脏，存在大量历史改动与未跟踪文件，后续修改要避免误覆盖。
- 项目内仍有部分旧文件存在编码注释混乱问题，但当前主构建链已打通。
- Windows 控制台对中文输出的兼容仍受终端环境影响；当前策略是不继续扩大编码黑科技范围，而是优先保证启动诊断日志稳定可读。
- 第三方库与脚本侧原生日志没有在本轮被统一为英文，后续若需要进一步收口，应单独区分“项目自有日志”和“第三方日志”处理。
- 最新真实启动日志表明 `UserManagement` 仍未进入已安装插件列表，后续应优先排查插件拷贝、插件发现路径和 `MANIFEST.MF` 安装链。
- 若后续继续改 Welcome / ModuleSelection / Management / Dashboard，建议优先沿 `UI/NewPages` 与 `UI/Forms` 主线推进，不再回到旧页面架构。
- 四页本轮仅完成代码级视觉微调与真实启动级验证，还需要下一轮由你基于真实界面继续做人工视觉验收与微调。

## 5. 建议的下一步顺序
1. 先运行 `build_x64/Release/medicalpro.exe`，对 Welcome / ModuleSelection / Management / Dashboard 做一轮新的人工视觉验收。
2. 基于真实画面继续微调四页层级、间距、状态信息密度和 CTA 强弱。
3. UI Phase 2 稳定后，再单独回到 `UserManagement` 插件发现/安装链问题。

### 1.12 2026-04-16 UI Phase 2 四页视觉微调第二轮落地
- 本轮继续按“纵向压缩 + 信息聚焦”方向推进，保持主链 `Welcome -> ModuleSelection -> Management -> Dashboard` 不变。
- 已完成 WelcomePage 第二轮压缩：
  - 压缩品牌区到按钮之间的弹性空白
  - 收窄右侧运行摘要区宽度并降低摘要块厚度
  - 继续压薄底部三张状态卡
- 已完成 ModuleSelectionPage 第二轮压缩：
  - 压缩顶部状态条与 Hero 区的边距
  - 将 `ankleSurgeryCard` / `systemSettingsCard` 由高卡压缩为更紧凑的流程卡
  - 去掉按钮前过大的弹性留白，主 CTA 明显上提
- 已完成 ManagementPage 第二轮压缩：
  - 压缩 Header、概览卡、当前工作视图上下文条与底部流程 CTA
  - 继续降低 Tab、搜索栏、表格表头与行高厚度
  - 让表格区成为更明确的主内容区
- 已完成 DashboardPage 第二轮压缩：
  - 压缩 Header、概览条、详情区与底部导航 CTA
  - 缩小 DICOM 区与 DICOM 卡片尺寸
  - 继续统一深色工作台节奏，减轻大块面板的压屏感
- 第二轮代码主要落在：
  - `UI/Forms/WelcomePage.ui`
  - `UI/Forms/ModuleSelectionPage.ui`
  - `UI/Forms/ManagementPage.ui`
  - `UI/Forms/DashboardPage.ui`
  - `UI/NewPages/WelcomePage.cpp`
  - `UI/NewPages/ModuleSelectionPage.cpp`
  - `UI/NewPages/ManagementPage.cpp`
  - `UI/NewPages/DashboardPage.cpp`
  - `UI/styles/three_pages_theme.qss`

### 2.11 2026-04-16 UI Phase 2 视觉微调第二轮验证
以下命令已在 2026-04-16 实际通过：

```powershell
cmake --build build_x64 --config Release --target medicalpro
ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure
$exe = Resolve-Path 'build_x64/Release/medicalpro.exe'
$p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru
Start-Sleep -Seconds 8
```

验证结果：
- 四页第二轮“纵向压缩 + 信息聚焦”代码已进入主构建链并构建通过。
- `three_page_presentation_utils_test` 继续通过，说明现有展示层辅助逻辑未被本轮视觉压缩破坏。
- `build_x64/Release/medicalpro.exe` 在 8 秒观察窗口内持续运行，未出现立即退出。
- 本轮完成的是结构压缩、样式收口和真实启动级验证，下一步仍需结合真实界面继续做人工视觉验收。

### 1.13 2026-04-16 UI Phase 2 第三轮视觉微调落地
- 本轮基于真实运行截图继续做人工视觉验收向微调，重点聚焦 Welcome 主视觉放大与四页联动收口。
- WelcomePage 本轮已完成：
  - 放大 `logoLabel`、主标题区与 `enterButton`
  - 通过限制 `statusCardsFrame` 纵向扩张并收起 `bottomSpaceSpacer`，把额外高度重新分配给主视觉区
  - 收窄 `runtimeSummaryFrame`，继续弱化右侧摘要区对主入口的竞争
- ModuleSelectionPage 本轮已完成：
  - 为 `moduleStatusBar` 与 `moduleHeroFrame` 增加明确限高
  - 继续压缩双卡高度和按钮前留白，让主卡更快进入视线中心
  - 文案进一步强调“数据管理主链优先，系统设置仅作辅助核对”
- ManagementPage 本轮已完成：
  - 压缩 header、概览卡、上下文条、表格行高与底部 CTA
  - 将表头继续收口到深色工作台语义，减少浅色跳点
- DashboardPage 本轮已完成：
  - 限制 header、概览条和底部 CTA 高度
  - 收紧详情区与 DICOM 区内边距，统一深色工作台背景
  - 缩小 DICOM 缩略卡与空态块，降低对主病例信息的干扰

### 2.11 2026-04-16 UI Phase 2 第三轮验证
以下命令已在 2026-04-16 实际通过：
```powershell
cmake --build build_x64 --config Release --target medicalpro
ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure
$exe = Resolve-Path 'build_x64/Release/medicalpro.exe'
$p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru
Start-Sleep -Seconds 8
```

验证结果：
- `medicalpro` Release 构建通过。
- `three_page_presentation_utils_test` 继续通过。
- `build_x64/Release/medicalpro.exe` 启动后在 8 秒观察窗口内保持运行，未出现“点运行即退出”的回归。
- 本轮未新增 `ThreePagePresentationUtils` 逻辑，沿用现有展示层回归测试做验证。

### 3.1 当前 UI 阶段状态补充
- Welcome -> ModuleSelection -> Management -> Dashboard 四页主链保持不变。
- 第三轮之后，Welcome 首页主视觉权重已明显高于底部状态卡和右侧摘要区。
- ModuleSelection、Management、Dashboard 的顶部区块高度已进一步收口，四页纵向节奏更统一。
- 下一步更适合继续基于真实运行界面做人工视觉验收向微调，而不是回头改动主链或插件链。

### 3.2 2026-04-16 UI Phase 2 第四轮人工视觉验收微调
- 本轮按最新真实运行反馈继续收口，但只处理 `WelcomePage`、`ModuleSelectionPage`、`ManagementPage`，`DashboardPage` 保持现状。
- WelcomePage 本轮重点：
  - 放大并左对齐品牌 logo，避免 logo 与标题组关系发散。
  - 收紧英文标题、中文标题、描述文案与 `进入系统` CTA 的内部间距。
  - 继续压缩右侧运行摘要区可用宽度，让主视觉信息组成为首屏第一落点。
- ModuleSelectionPage 本轮重点：
  - `ANKLE` / `CONFIG` 标识与状态标签改为左侧同组，不再被横向 spacer 拉到两端。
  - 主卡与辅卡内部标题、描述、提示、按钮的比例重新配平。
  - 主卡 CTA 进一步强化，辅卡 CTA 与说明维持辅助入口定位。
  - 后续补充微调已继续压低 `ANKLE / 需排查`、`CONFIG / 辅助入口` 两组标签高度，并同步放大标签字体与模块标题字体。
  - 两行说明文案之间的垂直间距已进一步收紧，避免模块卡内部出现过松的空场。
- ManagementPage 本轮重点：
  - 概览卡、上下文条、搜索栏、Tab、表头和表格行高整体回弹。
  - 数据管理页当前已明显减轻“空间很大但字和块偏小”的问题。
  - 额外补齐了 Tab 标题与编辑 / 删除 / 刷新按钮中文文案的代码级设置，避免旧 `.ui` 文本残留影响显示。

### 3.3 2026-04-16 第四轮 UI 微调验证
以下命令已实际通过：

```powershell
cmake --build build_x64 --config Release --target medicalpro
ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure
$exe = Resolve-Path 'build_x64/Release/medicalpro.exe'
$p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru
Start-Sleep -Seconds 8
```

验证结果：
- `medicalpro` Release 构建通过。
- `three_page_presentation_utils_test` 继续通过。
- `medicalpro.exe` 在 8 秒观察窗口内保持运行，说明本轮 UI 第四轮微调未破坏主程序启动链。

### 1.14 2026-04-16 SystemSettingsPage Phase 2 辅助检查页落地
- `SystemSettingsPage` 已从旧式 `QGroupBox + QFormLayout` 表单页升级为“状态先行 + 配置随后”的辅助检查页。
- 页面结构现已调整为：Header -> 状态总览 -> 当前建议 -> 常规设置 / 设备连接 / 路径配置。
- 页面运行时会直接展示插件框架、关键服务、路径可访问性三类状态摘要，并给出当前建议条。
- 现有设置项、页面跳转关系与 `QSettings` key 保持不变，未改动插件链与服务链核心逻辑。
- `three_pages_theme.qss` 已新增 `SystemSettingsPage` 专属样式段，页面视觉语言现已接入 UI Phase 2 公共主题。
- 本轮同时清理了 `SystemSettingsPage` 旧 `.ui` 内联样式和可见乱码文本来源，运行时文案统一由 `SystemSettingsPage.cpp` 回填。

### 2.12 2026-04-16 SystemSettingsPage Phase 2 验证
已实际通过的命令：
```powershell
cmake --build build_x64 --config Release --target three_page_presentation_utils_test
cmake --build build_x64 --config Release --target NewPagesLib
cmake --build build_x64 --config Release --target medicalpro
$path = Get-ChildItem -Path 'build_x64' -Recurse -Filter 'three_page_presentation_utils_test.exe' | Select-Object -First 1 -ExpandProperty FullName
& $path
$exe = Resolve-Path 'build_x64/Release/medicalpro.exe'
$p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru
Start-Sleep -Seconds 8
```
验证结果：
- `ThreePagePresentationUtils` 新增的 SystemSettings 展示层函数已编译并通过单测可执行文件直跑验证。
- `NewPagesLib` 与 `medicalpro` Release 构建通过，说明 `.ui` 重排、页面逻辑与公共主题接入均未破坏主构建链。
- `medicalpro.exe` 在 8 秒观察窗口内持续运行，未出现启动即退出。
- 当前已具备从模块页进入系统设置页进行人工视觉验收的代码基础。
- 现存一个测试基础设施问题：`ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test` 仍可能因可执行路径注册不一致而报找不到 exe，本轮以目标可执行文件直跑结果为准。

### 1.15 2026-04-16 SystemSettingsPage Phase 2 首屏强化微调
- `SystemSettingsPage` 已在首轮结构落地基础上继续完成首屏强化微调。
- 当前页头、状态总览与建议条已成为第一视觉层，配置区回落为稳定的辅助编辑区。
- 本轮未改页面跳转、设置项、`QSettings` key 和状态来源逻辑，只做结构密度与视觉层级收口。
- `UI/Forms/SystemSettingsPage.ui` 已进一步抬高 Header、三张状态卡与建议条的首屏权重，并同步收紧配置区与页面底部留白。
- `UI/styles/three_pages_theme.qss` 已把 SystemSettingsPage 的页头、状态卡、建议条和配置卡视觉权重重新拉开。
- `UI/NewPages/SystemSettingsPage.cpp` 已把建议条 badge 和三张状态卡的说明文案收口为更短、更像当前判断结果的表达。

### 2.13 2026-04-16 SystemSettingsPage 首屏强化验收
本轮实际通过的命令：
```powershell
cmake --build build_x64 --config Release --target NewPagesLib
cmake --build build_x64 --config Release --target medicalpro
ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure
$exe = Resolve-Path 'build_x64/Release/medicalpro.exe'
$p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru
Start-Sleep -Seconds 8
```
验收结果：
- `NewPagesLib` 构建通过，说明 `SystemSettingsPage.ui` 两轮布局微调未破坏 UIC 生成。
- `medicalpro` Release 构建通过，说明 QSS 和页面文案收口未破坏主程序构建链。
- `three_page_presentation_utils_test` 本轮已通过 `ctest` 直接执行验证。
- `medicalpro.exe` 在 8 秒观察窗口内持续运行，未出现启动即退出。
- 当前可进入 `SystemSettingsPage` 继续做真实界面的人工视觉验收。

### 1.16 2026-04-16 SystemSettingsPage 底部留白修正
- 用户实测反馈 `SystemSettingsPage` 底部仍存在明显空白，本轮已按真实运行现象回查，不再沿用“已清掉留白”的旧判断。
- 根因已定位到 `UI/Forms/SystemSettingsPage.ui` 内 `scrollAreaContents` 仍保留固定高度 `1400`，导致最后一张配置卡下方被额外撑出空白。
- 本轮已将该固定高度收紧到 `960`，同时为 `scrollLayout` 增加 `QLayout::SetMinAndMaxSize`，并将底部 margin 清零。
- 修复范围只涉及滚动内容容器和布局约束，没有改动页面跳转、状态来源、设置项或插件链逻辑。

### 2.14 2026-04-16 SystemSettingsPage 底部留白修正验证
以下命令已在 2026-04-16 实际通过：
```powershell
cmake --build build_x64 --config Release --target NewPagesLib
cmake --build build_x64 --config Release --target medicalpro
ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure
$exe = Resolve-Path 'build_x64/Release/medicalpro.exe'
$p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru
Start-Sleep -Seconds 8
```

验证结果：
- `NewPagesLib` 构建通过。
- `medicalpro` Release 构建通过。
- `three_page_presentation_utils_test` 继续通过，说明展示层辅助逻辑未被本轮留白修正误伤。
- `medicalpro.exe` 在 8 秒观察窗口内保持运行，`HasExited=False`。
- 当前代码层面已经完成底部留白根因修复，下一步以真实界面人工复核为准。

### 1.17 2026-04-16 SystemSettingsPage 实机截图视觉收口
- 用户补充的真实运行截图显示，当前页剩余的主要异常不再是底部留白，而是滚动区浅色底露出、建议条左 badge 被拉高、配置卡块感过重。
- 本轮已为 `scrollArea` viewport 单独命名并收口背景，让状态卡与建议条之间不再露出浅色分隔带。
- 本轮已把 `systemRecommendationBadge` 调整为固定尺寸胶囊，并同步压缩建议条上下内边距，避免出现高方块式 badge。
- 本轮继续降低了 `generalSettingsCard`、`deviceSettingsCard`、`pathSettingsCard` 的背景厚重感，让它们更明确地退回辅助编辑层。

### 2.15 2026-04-16 SystemSettingsPage 实机截图视觉收口验证
以下命令已在 2026-04-16 实际通过：
```powershell
cmake --build build_x64 --config Release --target medicalpro
ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure
$exe = Resolve-Path 'build_x64/Release/medicalpro.exe'
$p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru
Start-Sleep -Seconds 8
```

验证结果：
- `medicalpro` Release 构建通过。
- `three_page_presentation_utils_test` 继续通过。
- `medicalpro.exe` 在 8 秒观察窗口内持续运行，`HasExited=False`。
- 本轮改动没有触碰页面跳转、状态来源、设置项结构和插件链逻辑，属于纯展示层收口。
