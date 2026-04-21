# Platform Kernel Governance Design

## Plugin Chain Remediation Phase 2 Follow-up (2026-04-21)

- `RegistrationCore` and `OpticalTracking` now enter through governed on-demand activation.
- `ensureReady()` now follows strict `descriptor -> install/start -> service_ready -> health_check` semantics.
- Startup scope and governed scope are now separated, so on-demand failures no longer rewrite Phase 1 startup readiness.

## Plugin Chain Remediation Follow-up (2026-04-20)

- The default runtime mode is now `facade_mode` for the product startup path.
- The governed startup path is now generated from a descriptor-driven managed plan instead of whole-directory plugin installation.
- Phase 1 managed scope is limited to `UserManagement`, `DicomViewer`, and `FourViewDisplay`.
- `platform ready` now evaluates only the managed Phase 1 scope.
- `ready` remains bounded to dependency satisfaction, service registration, and lightweight health checks.
- Warmup is now routed through `PlatformWarmupCoordinator` and is no longer part of the blocking ready path in Phase 1.
- Acceptance evidence was refreshed in `build_x64` with `medicalpro`, `platform_descriptor_loader_test`, `platform_managed_plugin_plan_test`, `platform_dependency_graph_test`, `platform_startup_coordinator_test`, `platform_diagnostics_service_test`, `platform_warmup_coordinator_test`, `startup_orchestrator_lifecycle_test`, and `platform_descriptor_runtime_layout_test`.

## Startup Diagnostics Follow-up (2026-04-17)

- `StartupTrace` has been expanded from phase-level trace into a full lifecycle timeline.
- `ready-path` and `warmup-tail` are now separated in platform diagnostics.
- A `PlatformLifecycleEvent` ledger is now recorded and aggregated into `PlatformPluginLifecycleSnapshot` outputs.
- The landed lifecycle recorder now covers `install / start / service_ready / warmup / failed / degraded / skipped_by_mode` without bypassing the governance layer back into direct UI-to-CTK wiring.
- `PlatformDiagnosticsService` now derives `slowestPluginId`, `blockingSpanLabel`, `failurePointLabel`, recovery hints, and `ctk_platform_state_mismatch` problems from the same lifecycle ledger.

## Implementation Links

### 2026-04-20 Lifecycle Diagnostics Infrastructure Acceptance

- `PlatformLifecycleTraceRecorder`, `PlatformPluginLifecycleAggregator`, and the diagnostics-service lifecycle aggregation path are now landed in this worktree.
- The three governance runtime modes remain aligned: `observe_only` records explicit `skipped_by_mode` facts, `facade_mode` records the governed framework/core path while skipping deferred warmup, and `orchestrate_core` records the full ready-path plus warmup tail.
- The accepted infrastructure contract now explains startup slowness by phase, plugin, and lifecycle step instead of stopping at phase-only trace output.
- Acceptance evidence was refreshed in `build_x64_noctk` with `medicalpro`, `platform_startup_trace_recorder_test`, `platform_plugin_lifecycle_aggregator_test`, `platform_startup_coordinator_test`, `platform_diagnostics_service_test`, `platform_ui_bridge_test`, and `startup_orchestrator_lifecycle_test`.

### 2026-04-17 Rollout Status

- Startup governance routing has been validated in `build_x64_noctk`.
- Runtime artifact descriptor validation is passing in this worktree.
- Identity-flow facade decoupling is now complete for MainInterface, Login, and MainWindow.
- Direct CTK cleanup is now complete for `UI/MainInterfaceWidget.cpp` runtime-status access and `UI/NewPages/NavigationPage.cpp` service lookups through `CoreUiRuntimeStatusProvider` and `NavigationPageServiceAccess`.

- Implementation plan: `docs/superpowers/plans/2026-04-16-platform-kernel-governance-implementation.md`
- Governance matrix: `docs/superpowers/tracking/platform-plugin-governance-matrix.md`
- Decision log: `docs/superpowers/tracking/platform-migration-decision-log.md`


日期：2026-04-16  
范围：`medicalpro` 第一阶段“平台内核治理”设计  
目标：在不替换 CTK、不重写全部业务插件内部实现的前提下，为核心主链建立统一的平台内核、统一插件描述、统一状态模型、统一门面入口和统一诊断机制，解决当前插件识别混乱、初始化链路不清、服务加载无序、启动耗时不可解释的问题。

## 1. 设计背景

当前项目处于典型的老项目治理阶段，核心问题不是某一个插件单点失效，而是整条链路缺少统一真相和统一边界：

- `data` 已可定位，但插件框架初始化、插件识别、服务加载和 UI 状态之间仍然存在断裂
- 控制台能看到插件一个个安装，但最后平台仍可能报告插件未就绪
- 启动慢，但目前只能感知“慢”，不能解释究竟慢在发现、安装、启动、服务注册还是健康检查
- UI、流程层、旧 CTK 服务和插件内部实现之间边界模糊，导致任何局部修补都容易把系统进一步拉乱

因此第一阶段不追求“全部重写”，而是优先做平台治理，把主链先变成一个可解释、可收口、可演进的系统。

## 2. 第一阶段范围与边界

### 2.1 第一阶段子项目

本轮确认采用：

> 平台内核治理

优先级顺序固定为：

1. 结构优先
2. 稳定性优先
3. 性能优先

### 2.2 技术路线

本轮确认采用：

> 保留 CTK，但在其外层建立平台内核

原因：

- 直接替换 CTK 风险过高，不适合老项目第一阶段
- 当前真正缺的是统一治理层，而不是立刻更换底层运行时
- 旧插件内部实现可先通过兼容层纳管，避免一次性翻修所有插件

### 2.3 第一阶段纳管范围

本轮只治理核心主链，核心插件包括：

- `UserManagement`
- `DicomViewer`
- `FourViewDisplay`
- `RegistrationCore`
- `OpticalTracking`

### 2.4 最小启动边界

第一阶段采用：

> 最小启动

启动即就绪的插件：

- `UserManagement`
- `DicomViewer`
- `FourViewDisplay`

按需启动的插件：

- `RegistrationCore`
- `OpticalTracking`

### 2.5 第一阶段明确不做

- 不全量迁移所有插件
- 不先重写每个业务插件内部算法实现
- 不替换 CTK
- 不在第一阶段一次性建立配置中心、权限系统或完整插件设置规范

## 3. 架构目标与核心原则

### 3.1 总体目标

第一阶段要建立一层独立于 CTK 细节的“平台内核层”，让页面和业务流程不再直接依赖 CTK bundle 和 service 细节，而只依赖平台提供的业务门面、能力快照和诊断快照。

### 3.2 核心原则

- 平台描述文件是插件元数据唯一真相来源
- UI 和业务流程只依赖平台门面，不直接依赖 CTK 服务
- 插件之间禁止直接依赖彼此实现，只允许依赖平台契约、事件和 DTO
- `ready` 只表示“可进入下一步”，不表示“所有重活都已做完”
- 异常时应用外壳仍可启动，但核心流程按能力锁定
- 旧链路只允许保留在兼容层，不继续向上层扩散

## 4. 目标分层与目录结构

### 4.1 目标分层

建议目标结构如下：

- `Framework/Platform/Contracts`
- `Framework/Platform/Kernel`
- `Framework/Platform/Facades`
- `Framework/Platform/CtkBridge`
- `Framework/Platform/LegacyAdapters`
- `Framework/Platform/Diagnostics`
- `UI/PlatformPages/PlatformDiagnosticsPage.*`
- `Plugins/<CorePlugin>/platform/plugin.json`

### 4.2 分层职责

`Contracts`

- 定义平台统一插件契约、快照对象、状态枚举、服务接口和 DTO

`Kernel`

- 负责描述解析、依赖图、生命周期、启动编排、状态聚合、平台服务注册表和轻量健康检查

`Facades`

- 提供给 UI 和业务流程使用的三大门面服务
- 暴露业务动作、业务上下文、能力快照，不暴露 CTK 细节

`CtkBridge`

- 把 CTK bundle 和 service 事件翻译成平台状态语言

`LegacyAdapters`

- 把旧插件服务适配为平台契约
- 把旧错误、旧状态、旧依赖映射为平台诊断语言

`Diagnostics`

- 输出 `StartupTrace`、`DiagnosticSnapshot`、`CapabilitySnapshot`

### 4.3 关键对象

- `PluginDescriptor`
- `PluginRuntimeState`
- `PlatformServiceRegistry`
- `HealthCheckResult`
- `StartupCoordinator`
- `DiagnosticSnapshot`
- `CapabilitySnapshot`
- `ImagingContextSnapshot`
- `NavigationReadinessSnapshot`

## 5. 生命周期、启动编排与状态模型

### 5.1 固定状态模型

第一阶段统一采用 6 个状态：

- `discovered`
- `installed`
- `starting`
- `ready`
- `degraded`
- `failed`

### 5.2 `ready` 定义

`ready` 统一定义为：

> 依赖满足 + 服务注册成功 + 轻量健康检查通过

明确禁止把以下内容纳入 `ready`：

- Python 初始化
- GPU 预热
- 设备真实握手
- 大量数据预加载
- 真实业务计算

### 5.3 启动编排

`StartupCoordinator` 启动主链建议固定为：

1. 读取核心插件 `platform/plugin.json`
2. 构建平台依赖图并做静态校验
3. 发现并安装最小启动集
4. 启动 `UserManagement / DicomViewer / FourViewDisplay`
5. 等待服务注册
6. 执行轻量健康检查
7. 生成首个 `DiagnosticSnapshot`
8. 决定是否解锁核心流程入口

`RegistrationCore` 与 `OpticalTracking` 不进入应用启动主链，而由 `NavigationAppService.ensureReady(plugin_id)` 按需启动。

### 5.4 失败与降级

核心链异常时采用：

> 应用可启动，但核心流程锁定

具体行为：

- 应用外壳照常启动
- 平台状态标记为 `degraded` 或 `failed`
- 相关主流程入口禁用
- `Settings` 与 `Diagnostics` 永远可进入
- 页面不再自己猜状态，只消费平台 `CapabilitySnapshot` 与 `DiagnosticSnapshot`

## 6. 三大门面与上下文模型

### 6.1 `IdentityAppService`

职责：

- 登录态、登出、会话恢复
- 当前用户资料
- 角色和权限摘要
- 用户管理基础查询
- 身份相关能力快照

封装目标插件：

- `UserManagement`

明确不负责：

- 患者、病例、DICOM 数据
- 配准、跟踪、导航流程
- 暴露 CTK service reference

### 6.2 `ImagingAppService`

职责：

- 患者、病例、检查列表查询
- 当前患者、当前病例、当前检查的唯一真相来源
- DICOM 数据访问和影像预览门面
- `FourViewDisplay` 所需影像上下文协调
- 输出只读 `ImagingContextSnapshot`

封装目标插件：

- `DicomViewer`
- `FourViewDisplay`

明确不负责：

- 设备跟踪
- 配准执行
- 导航流程状态机

### 6.3 `NavigationAppService`

职责：

- 按需确保 `RegistrationCore` 和 `OpticalTracking` 就绪
- 汇总导航相关 capability
- 输出导航流程状态和入口动作
- 只读消费 `ImagingContextSnapshot`
- 输出 `NavigationReadinessSnapshot`

明确不负责：

- 自己查询患者、病例和 DICOM
- 自己维护影像上下文副本
- 直接管理 CTK 生命周期细节

### 6.4 快照模型

`ImagingContextSnapshot` 建议至少包含：

- 当前用户可见患者集合摘要
- 当前患者
- 当前病例
- 当前检查
- 当前影像资源摘要
- 上下文完整性标记

`NavigationReadinessSnapshot` 建议至少包含：

- 配准插件状态
- 跟踪插件状态
- 当前影像上下文是否满足导航前置条件
- 导航入口是否可解锁
- 失败原因摘要

## 7. 插件描述文件与依赖图规则

### 7.1 `platform/plugin.json`

平台描述文件是插件元数据唯一真相来源，CTK `MANIFEST.MF` 仅保留运行时需要的字段，不再承担平台设计真相职责。

推荐最小结构：

```json
{
  "id": "org.medicalpro.dicom_viewer",
  "version": "1.0.0",
  "display_name": "DicomViewer",
  "domain": "imaging",
  "enabled": true,
  "runtime": {
    "ctk_symbolic_name": "org.medicalpro.plugins.dicomviewer",
    "startup_policy": "eager",
    "bootstrap_level": "core",
    "entry_capability": "imaging.data"
  },
  "provides": {
    "services": ["imaging.study_query"],
    "capabilities": ["imaging.data"]
  },
  "requires": {
    "services": [],
    "capabilities": [],
    "plugins": []
  },
  "optional": {
    "services": [],
    "capabilities": [],
    "plugins": []
  },
  "health_checks": ["service_registered", "data_path_accessible"]
}
```

### 7.2 字段约束

必填字段：

- `id`
- `version`
- `display_name`
- `domain`
- `runtime.startup_policy`
- `runtime.bootstrap_level`

第一阶段限制：

- `startup_policy` 只允许 `eager / on_demand / disabled`
- `bootstrap_level` 只允许 `core / deferred`
- `health_checks` 只允许轻量检查项名，不允许复杂脚本

### 7.3 依赖图规则

平台依赖图规则固定如下：

- 节点是平台插件 `id`，不是 CTK bundle 文件名
- 强依赖边只来自 `requires.services`、`requires.capabilities`、`requires.plugins`
- 弱依赖边只来自 `optional.*`
- `core` 启动集内禁止强依赖环
- `on_demand` 插件不能成为 `core` 插件的强前置
- 启动顺序依据平台依赖图拓扑序，不再依据扫描顺序
- 缺失弱依赖时进入 `degraded`，但不阻塞应用外壳启动

### 7.4 第一阶段推荐能力图

- `UserManagement` 提供 `identity.core`
- `DicomViewer` 提供 `imaging.data`
- `FourViewDisplay` 提供 `imaging.viewport`
- `FourViewDisplay` 可强依赖 `imaging.data`
- `RegistrationCore` 提供 `navigation.registration`
- `OpticalTracking` 提供 `navigation.tracking`
- `NavigationAppService` 聚合导航能力并消费只读影像上下文

## 8. 兼容层策略与禁用边界

### 8.1 兼容层策略

第一阶段采用：

> 适配器过渡

过渡路径：

1. 保留旧 CTK 插件和旧服务实现
2. 新平台内核接管描述解析、状态机、启动编排和诊断输出
3. `CtkBridge` 负责 CTK bundle 和 service 事件翻译
4. `LegacyAdapters` 负责旧服务到平台契约的映射
5. 页面和主流程先迁移到三大门面
6. 插件内部重构放到后续阶段，适配器逐步变薄

### 8.2 适配器挂接关系

- `IdentityAppService <- LegacyUserManagementAdapter`
- `ImagingAppService <- LegacyDicomViewerAdapter + LegacyFourViewDisplayAdapter`
- `NavigationAppService <- LegacyRegistrationAdapter + LegacyOpticalTrackingAdapter`

### 8.3 第一阶段正式禁用

以下旧调用从第一阶段开始正式判定为违规：

- UI 页面直接调用 `CTKManager::instance()`
- UI 页面直接做 CTK service lookup
- 业务流程层直接安装、启动、停止 bundle
- 页面自己根据某个 service 是否存在猜系统是否可进入
- 插件 A 直接依赖插件 B 的实现类、实现头文件或内部单例
- 页面层自己维护患者、病例、检查当前上下文的副本
- 在 plugin activator 或 start 阶段执行重型初始化
- 把 `MANIFEST.MF` 当作平台设计真相来源
- 继续新增“全局单例 + 到处可取旧服务”的入口

以下能力允许暂留，但只能位于兼容层：

- CTK bundle 启停
- 旧 service reference 获取
- 旧接口到平台契约的转换
- 旧错误码到平台诊断语言的映射

## 9. 诊断输出与页面收口

### 9.1 诊断输出

第一阶段平台必须提供：

- `StartupTrace`
- `DiagnosticSnapshot`
- `CapabilitySnapshot`

推荐诊断信息包括：

- 插件状态
- 服务状态
- 健康检查结果
- 缺失依赖
- 启动耗时分解
- 错误摘要
- 恢复建议
- 当前运行模式

### 9.2 页面收口规则

页面层规则固定如下：

- `Welcome / ModuleSelection / Management / Dashboard` 只消费门面快照和 capability
- `DiagnosticsPage` 只消费 `DiagnosticSnapshot`
- 页面层禁止直接访问 `CTKManager`
- 页面层禁止直接 service lookup

## 10. 测试策略与验收标准

### 10.1 测试策略

第一阶段测试分为五层：

- `plugin.json` 校验测试
- 平台内核单元测试
- 兼容层适配测试
- 三大门面服务测试
- 核心主链集成与回归测试

测试重点包括：

- 必填字段和枚举值校验
- 依赖图合法性和无强依赖环
- 生命周期状态流转
- `ready / degraded / failed` 判定
- `ensureReady(plugin_id)` 单飞与稳定性
- 旧服务到平台契约的正确映射
- 页面不再直接依赖 CTK service

同时必须建立启动可观测性回归：

- 记录总启动耗时
- 记录插件发现耗时
- 记录安装耗时
- 记录启动耗时
- 记录健康检查耗时

### 10.2 第一阶段验收标准

第一阶段完成时应满足：

- 核心插件都具备 `platform/plugin.json`
- 平台内核可独立解析描述、构建依赖图、接管核心主链启动编排
- `UserManagement / DicomViewer / FourViewDisplay` 成为最小启动集
- `RegistrationCore / OpticalTracking` 改为按需启动
- 应用启动后，即使核心链异常，也能进入应用外壳和诊断页
- 核心页面不再直接依赖 CTK service lookup
- 三大门面成为页面和主流程唯一正式入口
- 诊断页能展示插件状态、服务状态、健康检查、缺失依赖、启动耗时、错误摘要和恢复建议
- 启动慢问题至少做到可测量、可定位、可解释
- 旧 CTK 直接调用只保留在 `CtkBridge / LegacyAdapters`

## 11. 迁移阶段划分与推荐实施顺序

### 11.1 迁移阶段

`P0：现状基线与治理入口`

- 盘点当前页面、流程、插件对 CTK 的直接调用
- 为核心插件补 `plugin.json`
- 建立 `StartupTrace` 和基础诊断快照
- 运行模式保持 `observe_only`

`P1：平台内核落地`

- 落地 `Contracts / Kernel / Diagnostics`
- 实现描述解析、依赖图、状态机、服务注册表、轻量健康检查

`P2：门面与兼容层接管`

- 落地 `CtkBridge`
- 落地 `LegacyAdapters`
- 落地三大门面
- 页面和流程层改为消费门面与快照

`P3：主链正式切换`

- `StartupCoordinator` 接管最小启动集
- `NavigationAppService.ensureReady(...)` 接管按需启动
- 诊断页成为唯一正式状态观察入口

`P4：收尾与规范冻结`

- 清理残留 CTK 直连
- 冻结 `plugin.json` 规范、门面契约、状态模型和诊断格式
- 更新治理文档和迁移清单

### 11.2 推荐实施顺序

第一阶段建议按以下顺序推进：

1. 先建立平台观察层，不接管启动
2. 再补齐依赖图和状态机
3. 再落地 `CtkBridge` 和 `LegacyAdapters`
4. 先做三大门面和页面依赖收口
5. 先把诊断页正式化
6. 再切最小启动集到 `StartupCoordinator`
7. 最后接入按需启动
8. 收尾清理与规范冻结

### 11.3 首轮切入包

首轮实施建议采用最小切入包：

> 平台观察层 + 核心插件描述文件 + 启动诊断

首轮只做：

- 建立 `Framework/Platform` 基础目录
- 为 5 个核心插件补 `platform/plugin.json`
- 实现描述文件解析和基础校验
- 实现 `PluginRuntimeState`
- 实现 `DiagnosticSnapshot`
- 实现 `StartupTrace`
- 把当前真实启动链接入观察层
- 做初版诊断页或诊断面板
- 运行模式保持 `observe_only`

## 12. 风险、回滚与文档机制

### 12.1 主要风险

- 双真相风险
- 隐式依赖风险
- 激活期重活风险
- 兼容层失真风险
- 按需启动竞态风险
- 页面偷穿透风险
- 迁移半途悬空风险

### 12.2 回滚策略

第一阶段采用三档运行模式：

- `observe_only`
- `facade_mode`
- `orchestrate_core`

回滚原则：

- 只允许整段回退到前一模式，不做现场拼接式热修补
- 新平台接管某条链路前，旧链必须仍然可运行
- 每切一段主链，至少保留一个版本周期的兼容退路
- 诊断页必须显示当前实际运行模式

### 12.3 文档更新机制

第一阶段至少持续维护以下文档：

- 设计总文档
- 实施计划文档
- 插件治理清单
- 迁移决策日志

文档更新规则：

- 每次变更平台契约，必须同步更新设计文档
- 每次迁移一个核心插件，必须同步更新插件治理清单
- 每次调整阶段计划，必须同步更新实施计划
- 每次做重要取舍，必须补一条迁移决策日志
- 文档和代码最多在同一个 patch 内一起落地，不允许长期漂移

## 13. 设计结论

第一阶段的核心不是把所有插件都修好，而是先建立以下四件事：

- 平台能识别插件
- 平台能解释启动链
- 页面只依赖平台门面
- 异常能被真实诊断

本设计落地后，`medicalpro` 将从“症状驱动修补”转向“诊断驱动治理”，为后续性能专项、插件内部重构和更深层平台化改造建立稳定入口。
