# Plugin Chain Remediation Phase 2 Design

## Scope Note

- This document defines the second remediation slice for `medicalpro` plugin-chain governance after Phase 1 landed the descriptor-driven managed startup path.
- This slice does not expand the cold-start core scope and does not attempt a full navigation UI refactor.
- The goal of this slice is to make `RegistrationCore` and `OpticalTracking` on-demand activation follow the same governed truth source as the Phase 1 startup chain.

## Implementation Links

- Phase 1 design: `docs/superpowers/specs/2026-04-20-plugin-chain-remediation-phase1-design.md`
- Base governance design: `docs/superpowers/specs/2026-04-16-platform-kernel-governance-design.md`
- Startup diagnostics design: `docs/superpowers/specs/2026-04-17-startup-performance-and-plugin-lifecycle-diagnostics-design.md`
- Phase 1 implementation plan: `docs/superpowers/plans/2026-04-20-plugin-chain-remediation-phase1-implementation.md`
- Decision log: `docs/superpowers/tracking/platform-migration-decision-log.md`

日期：2026-04-20  
范围：`medicalpro` 平台治理后续专项“插件链整改 Phase 2”设计  
目标：在不扩大首批冷启动范围、不回退到旧 CTK 直启逻辑的前提下，把 `RegistrationCore` 与 `OpticalTracking` 的按需启动链路收口为一条由 descriptor 驱动、由平台治理层执行、由 diagnostics 统一解释的正式链路。

## 1. 设计背景

Phase 1 已经把产品冷启动主链收口为：

- `platform_runtime.json`
- runtime descriptor 集合
- `PlatformManagedPluginPlanBuilder`
- `PlatformStartupCoordinator`
- `PlatformWarmupCoordinator`
- `PlatformStateStore`
- `PlatformDiagnosticsService`

这条链已经能够稳定解释首批三插件：

- `UserManagement`
- `DicomViewer`
- `FourViewDisplay`

但当前 `RegistrationCore` 与 `OpticalTracking` 的按需启动仍然存在典型的“治理外侧捷径”问题：

- `NavigationAppService.ensureReady(plugin_id)` 仍然只是透传到 `INavigationFacadePort`
- `LegacyNavigationAdapter` 仍然使用“plugin id -> CTK symbolic name -> direct start”的旧语义
- 这条链没有 descriptor 校验、没有 bundle path 真相、没有 `service_ready` 判定、没有 health check 收口
- `MainInterfaceWidget` 仍然自行构造 `LegacyNavigationAdapter`，没有共享 `main.cpp` 已建立的 descriptor/state/diagnostics 上下文
- `RegistrationCore` 与 `OpticalTracking` descriptor 目前缺少与 Phase 1 首批插件一致的 diagnostics 契约字段

这意味着当前系统仍然有两条并行真相：

1. 冷启动主链已经由平台治理层正式接管
2. 按需启动链仍可能绕过治理层直接触发 CTK 行为

Phase 2 的目的就是消除这条分叉，让按需启动也回到同一套平台治理语义中。

## 2. Phase 2 决策结论

本轮已经确认的设计决策如下：

- 范围只覆盖 `RegistrationCore` 与 `OpticalTracking`
- 不在本轮扩展到 `NavigationPage` UI 重构
- `ensureReady(plugin_id)` 采用严格治理语义，不允许失败后偷偷回退旧逻辑
- 三档运行模式都保持一致的解释语义：
  - `observe_only`：记录请求与跳过，不真正执行
  - `facade_mode`：严格执行按需 `install/start/service_ready/health_check`
  - `orchestrate_core`：复用同一套按需治理链路
- 按需插件失败会阻塞对应 capability 或对应导航流程，但不会反向污染 Phase 1 首批主启动 `platformReady`

## 3. 设计目标与非目标

### 3.1 设计目标

本轮必须达成以下目标：

1. `RegistrationCore` 与 `OpticalTracking` 的 `ensureReady()` 调用进入正式的平台治理链
2. 按需启动链与 Phase 1 冷启动链共享同一套 descriptor、runtime mode、state store、diagnostics 上下文
3. `ensureReady()` 的成功与失败必须能被 lifecycle trace、diagnostics problems、recovery hints 一致解释
4. `observe_only / facade_mode / orchestrate_core` 三档模式对按需启动都必须有清晰、稳定、可测试的语义
5. `RegistrationCore` 与 `OpticalTracking` descriptor 必须补齐 diagnostics 契约
6. `LegacyNavigationAdapter` 不再承担旧式 CTK 直启执行语义
7. `MainInterfaceWidget` 不再自己构造脱离主治理上下文的导航适配器

### 3.2 明确非目标

本轮不做以下事情：

- 不扩大 Phase 1 首批冷启动 scope
- 不把 `RegistrationCore` 与 `OpticalTracking` 拉进产品冷启动主链
- 不做完整的 `NavigationPage` 或导航 UI 架构重构
- 不一次性治理所有 deferred/on-demand 插件
- 不替换 CTK
- 不把 `warmup` 并回 `ready`
- 不要求本轮消除所有 legacy adapter，只要求导航按需启动不再绕开治理层

## 4. 运行模式与行为语义

### 4.1 `observe_only`

`observe_only` 下允许执行以下步骤：

- 校验 target descriptor
- 生成按需激活计划
- 记录 on-demand activation 请求事实

但明确禁止：

- install bundle
- start plugin
- wait for service ready
- run health checks

在该模式下：

- `ensureReady()` 返回 `false`
- lifecycle 中记录 `skipped_by_mode`
- diagnostics 明确说明当前模式只观察，不保证插件就绪

### 4.2 `facade_mode`

`facade_mode` 是本轮的主要产品路径，`ensureReady(plugin_id)` 必须按以下顺序执行：

1. descriptor 校验
2. 生成按需激活计划
3. install 必需 bundle
4. start target plugin 及必要前置插件
5. wait for `service_ready`
6. 执行 lightweight health checks
7. 回写 state store 与 diagnostics

任何一步失败都必须导致：

- `ensureReady()` 返回 `false`
- lifecycle 中记录明确的失败原因
- diagnostics 能够解释缺失依赖、超时或契约缺失

### 4.3 `orchestrate_core`

`orchestrate_core` 与 `facade_mode` 在 `ensureReady()` 语义上保持一致：

- 使用同一套 activation plan
- 使用同一套 coordinator 执行路径
- 使用同一套 diagnostics 事实结构

两者差别只体现在产品冷启动主链范围，而不体现在按需启动链的成败定义上。

### 4.4 统一返回口径

`ensureReady(plugin_id)` 统一只回答一件事：

> 目标插件现在是否具备进入下一步业务流程的最小可用能力。

对应 reason code 统一限定为：

- `skipped_by_mode`
- `descriptor_missing`
- `diagnostics_contract_missing`
- `bundle_path_missing`
- `dependency_unavailable`
- `install_failed`
- `start_failed`
- `service_ready_timeout`
- `health_check_failed`
- `already_ready`
- `service_ready`

## 5. 状态边界与 on-demand 结论

### 5.1 `ready` 边界

对 `RegistrationCore` 与 `OpticalTracking`，`ready` 继续维持严格定义：

- 必需服务已注册
- 必需插件依赖已满足
- 必需 capability 已满足
- 轻量 health check 已通过

以下内容禁止纳入 `ready`：

- Python 初始化
- GPU 预热
- 重型场景构建
- 真实导航流程启动
- 复杂硬件握手
- 大量数据预加载

### 5.2 对产品整体状态的影响

`RegistrationCore` 与 `OpticalTracking` 在 Phase 2 中属于：

- governance scope 内
- startup blocking scope 外

这意味着：

- 它们需要进入 state store 与 diagnostics 的治理视野
- 它们失败时应阻塞对应导航流程
- 但它们失败时不应把 Phase 1 已成立的首批冷启动 `platformReady` 反向改写为 false

## 6. 目标架构与数据流

### 6.1 总体思路

Phase 2 继续延用 Phase 1 的总架构：治理链的真相必须从 descriptor 出发，而不是从 UI 或 legacy adapter 的临时行为出发。

本轮新增一条正式的 on-demand activation 子链：

- target plugin id
- runtime descriptors
- runtime plugin directory
- on-demand activation plan
- startup coordinator execution
- state write-back
- diagnostics aggregation

### 6.2 目标组件拆分

#### `PlatformOnDemandActivationPlanBuilder`

新增内核级 plan builder，专门负责按需启动，不与 Phase 1 主启动 plan 混用。

职责：

- 接收 target plugin id
- 基于 descriptor 构建按需激活计划
- 补齐必要前置依赖
- 解析 bundle 路径
- 校验 diagnostics 契约是否完整

输出至少包含：

- target plugin id
- `ctk_symbolic_name`
- bundle file path
- required plugins
- required capabilities
- required services
- `service_ready_timeout_ms`
- `health_checks`

#### `PlatformStartupCoordinator`

扩展为“主启动链 + 按需链”的统一执行器。

新增职责：

- 执行 on-demand install
- 执行 on-demand start
- 执行 on-demand `service_ready` 判定
- 执行 on-demand health check
- 在三档 runtime mode 下给出统一的 skip/success/failure 事实

#### `LegacyNavigationAdapter`

保留 facade 边界，但降级为治理链适配器。

新职责：

- 接收 `ensureReady(plugin_id)` 请求
- 调用 on-demand activation plan builder
- 调用 startup coordinator
- 返回治理结果

移除的旧职责：

- 不再自己维护“platform id -> CTK symbolic name -> direct start”的直启语义

#### `MainInterfaceWidget`

本轮不重构 UI，但要修正实例来源。

新原则：

- 不再在 widget 内部裸建导航适配器
- 改为由 `main.cpp` 使用已建立的治理上下文完成组装后注入

#### `PlatformStateStore`

需要从 Phase 1 的单一 `managedPluginIds` 概念演进为两层 scope：

- `startupScopePluginIds`
  - 只用于冷启动 `platformReady`
  - 当前仍是 `UserManagement / DicomViewer / FourViewDisplay`
- `governedPluginIds`
  - 表示当前纳入平台治理解释范围的全部插件
  - Phase 2 后应包含 `RegistrationCore / OpticalTracking`

这样可以同时满足：

- 按需插件不再被 diagnostics 当成“治理外插件”
- 按需插件失败不污染首批主启动 ready 结论

### 6.3 固定数据流

固定数据流如下：

1. `main.cpp` 读取 runtime config 与全量 descriptor
2. `main.cpp` 写入 startup scope 与 governed scope
3. UI 或业务调用 `NavigationAppService.ensureReady(plugin_id)`
4. 导航适配器将请求交给 on-demand activation plan builder
5. plan builder 生成严格校验后的 activation plan
6. `PlatformStartupCoordinator` 根据 runtime mode 执行或跳过
7. 执行事实写入 lifecycle recorder、state store、diagnostics
8. facade 层只把最终 `success/failure` 折叠为 `bool`

## 7. Descriptor 契约要求

### 7.1 `RegistrationCore`

当前 descriptor 只有：

- `health_checks`

但缺少：

- `diagnostics.required_services`
- `diagnostics.service_ready_timeout_ms`

本轮必须补齐，才能满足严格治理语义。

### 7.2 `OpticalTracking`

同样必须补齐：

- `diagnostics.required_services`
- `diagnostics.service_ready_timeout_ms`

### 7.3 契约约束

Phase 2 中，任何 on-demand governed plugin 若缺少以下字段，必须直接视为 descriptor 不合格：

- `runtime.ctk_symbolic_name`
- `runtime.startup_policy=on_demand`
- `runtime.bootstrap_level=deferred`
- `diagnostics.required_services`
- `diagnostics.service_ready_timeout_ms`
- `health_checks`

不得再通过运行时代码猜测默认值掩盖 descriptor 问题。

## 8. Diagnostics 与状态回写要求

本轮要求 diagnostics 不只说明“按需启动失败”，还必须说明失败属于哪一类治理问题。

至少需要覆盖：

- descriptor 缺失
- diagnostics 契约缺失
- bundle path 缺失
- install 失败
- start 失败
- service ready 超时
- health check 失败
- skipped by mode
- already ready

同时必须满足：

- `RegistrationCore / OpticalTracking` 能作为 governed plugin 出现在 diagnostics 中
- 其 failure 或 degraded 结论不改变冷启动 startup scope 的 `platformReady`
- recovery hints 能指出依赖缺失、descriptor 缺失或 timeout 原因

## 9. 验收与测试策略

### 9.1 单元验收

必须覆盖：

- on-demand activation plan 生成正确
- descriptor 缺失时明确失败
- diagnostics 契约缺失时明确失败
- bundle 路径解析失败时明确失败
- `observe_only` 跳过但留下正式 trace
- `facade_mode` 严格执行按需 install/start/service_ready
- `orchestrate_core` 复用同一套按需治理链
- `already_ready` 短路成功
- on-demand failure 不污染冷启动 `platformReady`
- facade 层仍保持 `NavigationAppService.ensureReady(plugin_id)` 入口稳定

### 9.2 运行时验收

必须满足：

- `RegistrationCore` 与 `OpticalTracking` runtime descriptor 仍能复制到运行时目录
- `ensureReady("org.medicalpro.registration_core")` 与 `ensureReady("org.medicalpro.optical_tracking")` 在三档模式下都能给出可解释结论
- diagnostics 页面与控制台解释口径一致
- `UI/MainInterfaceWidget.cpp` 不再自己构造脱离治理上下文的导航适配器

### 9.3 推荐验收命令

```powershell
cmake --build build_x64 --config Release --target medicalpro platform_on_demand_activation_plan_test platform_startup_coordinator_test platform_diagnostics_service_test platform_facades_test
ctest --test-dir build_x64 -C Release -R "platform_on_demand_activation_plan_test|platform_startup_coordinator_test|platform_diagnostics_service_test|platform_facades_test|platform_descriptor_runtime_layout_test" --output-on-failure
rg -n "new LegacyNavigationAdapter\\(|CTKManager::instance\\(|startPlugin\\(" UI/MainInterfaceWidget.cpp Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.cpp
```

期望结果：

- `medicalpro` 构建通过
- 相关单测全部通过
- `platform_descriptor_runtime_layout_test` 通过
- `UI/MainInterfaceWidget.cpp` 中不再出现 `new LegacyNavigationAdapter(`
- `LegacyNavigationAdapter.cpp` 中不再保留直接 CTK 直启语义

## 10. 提交策略建议

推荐分为两个提交批次：

1. `feat`：descriptor 契约补齐、on-demand activation plan、coordinator 与单测
2. `refactor/docs`：adapter 注入、state/diagnostics scope 收口、文档回写

这样可以保证：

- descriptor 与执行链的事实基础先独立落地
- UI 组装与治理收口变更单独回溯

