# Plugin Chain Remediation Phase 1 Design

## Scope Note

- This document defines the first remediation slice for the legacy plugin chain disorder in `medicalpro`.
- This slice does not replace CTK and does not attempt to refactor all legacy plugins at once.
- The goal of this slice is to make the governed startup chain stable, explainable, and diagnosable under a single source of truth.

## Implementation Links

- Base governance design: `docs/superpowers/specs/2026-04-16-platform-kernel-governance-design.md`
- Startup diagnostics design: `docs/superpowers/specs/2026-04-17-startup-performance-and-plugin-lifecycle-diagnostics-design.md`
- Governance implementation plan: `docs/superpowers/plans/2026-04-16-platform-kernel-governance-implementation.md`
- Decision log: `docs/superpowers/tracking/platform-migration-decision-log.md`

日期：2026-04-20  
范围：`medicalpro` 平台治理后续专项“插件链整改 Phase 1”设计  
目标：在不替换 CTK、不重写所有旧插件内部实现的前提下，把首批受管插件的发现、安装、启动、`service ready` 判定、诊断和文档回写统一到一条受管主链里，解决“框架初始化口径不一致、插件识别混乱、控制台与平台状态分叉、启动慢但不可解释”的问题。

## 1. 设计背景

当前项目已经完成平台治理和启动诊断基础设施的第一轮落地，但插件链仍然存在典型的“双轨真相”问题：

- 运行时配置已经引入 `platform_runtime.json`、descriptor、`PlatformStartupCoordinator` 和 diagnostics service。
- 实际插件安装链路仍然保留“扫描整个 `plugins/` 目录”的旧路径。
- 平台治理层只认 descriptor 和受管 plugin id，但 CTK 运行时仍可能安装大量不受管 bundle。
- `service ready`、`warmup`、旧服务访问和启动阶段桥接仍有部分逻辑混在 `main.cpp`。
- 默认 `observe_only` 模式下不会初始化框架和主启动链，这与“产品默认应该真的带起主链”的预期并不一致。

这导致开发者会同时看到两套互相打架的事实：

1. 控制台里可以看到一批插件被逐个 install 或 start。
2. 平台治理层最终又报告某些插件未纳管、未 ready 或根本未找到。

本专项的目标不是继续补日志，而是把首批主启动链真正收口成一条由 descriptor 驱动的受管链。

## 2. Phase 1 决策结论

本轮已经确认的设计决策如下：

- 默认产品运行模式切到 `facade_mode`。
- 首批受管插件只包含：
  - `UserManagement`
  - `DicomViewer`
  - `FourViewDisplay`
- `service ready` 的定义固定为：
  - 服务注册成功
  - 必需插件满足
  - 必需 capability 满足
  - 轻量 health check 通过
- 主启动链只纳管有 descriptor 的插件。
- 没有 descriptor 的旧插件不再参与 `platform ready` 判定。
- 主启动链的安装源不再来自“扫描整个 `plugins/` 目录”，而是来自 descriptor 反向解析出的受管安装清单。
- 本轮整改路线采用“强收口治理链”，而不是继续保留双轨兼容真相。

## 3. 设计目标与非目标

### 3.1 设计目标

本专项必须达成以下目标：

1. 产品默认启动进入 `facade_mode`，真正接管 framework 初始化、插件安装和首批核心插件启动。
2. 主启动链只有一套真相源：`platform_runtime.json + descriptor`。
3. 首批受管插件的安装、启动和 `service ready` 判定全部可被 diagnostics 解释。
4. 非受管旧插件即使仍存在于运行目录，也不再影响平台主链结论。
5. `ready` 和 `warmup` 的边界必须彻底切开。
6. `CTKManager`、`StartupOrchestrator`、`PlatformStartupCoordinator`、`main.cpp`、UI 之间的职责边界必须更清晰。
7. 文档、决策日志、状态页和验收命令必须随整改同步回写。

### 3.2 明确非目标

本轮不做以下事情：

- 不替换 CTK。
- 不一次性重构所有历史插件。
- 不把 `RegistrationCore` 和 `OpticalTracking` 拉进首批主启动链。
- 不把 Python 初始化、GPU 预热、大数据预加载、重型 widget 创建并入 `ready`。
- 不要求本轮就消灭所有运行目录中的旧 bundle。
- 不把 diagnostics page 变成原始日志终端。

## 4. 目标架构与主启动链收口

Phase 1 的目标架构如下：

- 唯一主链真相源：
  - `platform_runtime.json`
  - 运行时 descriptor 集合
- 唯一首批受管插件集：
  - `UserManagement`
  - `DicomViewer`
  - `FourViewDisplay`
- 唯一主链阶段：
  - framework init
  - managed install
  - core start
  - service ready
- 唯一主链状态结论：
  - `platform ready / degraded / failed`

受管主启动链固定收口为以下 8 步：

1. 读取 `config/platform_runtime.json`
2. 加载并校验运行时 descriptor 集合
3. 生成首批受管安装清单
4. 初始化并启动 CTK framework
5. 只安装受管清单中的 bundle
6. 只启动首批 core 插件
7. 对首批插件执行 `service ready` 判定
8. 生成统一 diagnostics snapshot 并回写 UI / 状态页

这样收口以后，系统必须只剩一套解释口径：

- 为什么安装这个插件：因为 descriptor 声明它属于本轮受管清单。
- 为什么没安装那个插件：因为它不在当前受管范围。
- 为什么插件 ready / not ready：因为受管依赖、服务注册和轻量健康检查结果可追踪。
- 为什么启动慢：因为 diagnostics 可以定位到具体插件和具体步骤。

## 5. 状态模型与 `service ready` 边界

Phase 1 继续沿用当前固定的 6 态，但进入条件必须明确：

- `discovered`
  - descriptor 已被平台接纳，并进入本轮受管安装清单
- `installed`
  - CTK bundle 安装成功
- `starting`
  - 已发起启动，正在等待服务与依赖满足
- `ready`
  - 服务注册成功
  - 必需插件满足
  - 必需 capability 满足
  - 轻量 health check 通过
- `degraded`
  - 主链已可继续，但存在非阻断问题
- `failed`
  - 安装失败、启动失败、`service ready` 超时或关键轻量检查失败

`service ready` 只允许回答一句话：

> 该插件现在是否具备进入下一步业务流程的最小可用能力。

因此，以下内容禁止并入 `ready`：

- 页面或 widget 真实创建成功
- Python 环境初始化
- GPU 预热
- 大数据预加载
- VTK 场景构建
- 任何重型业务预热

这意味着：

- `ready-path` 只覆盖 `install / start / service_ready`
- `warmup-tail` 是 `ready` 之后的独立尾段
- `warmup` 失败最多导致 `degraded`，不应反向污染首批主启动链是否可进入

### 5.1 首批插件的 `ready` 口径

`UserManagement`

- 身份相关服务已注册
- 平台可发现必需身份 service
- 轻量会话或查询检查通过

`DicomViewer`

- 影像相关 service 已注册
- 必需依赖满足
- 不要求实际加载影像数据
- 不要求创建 viewer widget

`FourViewDisplay`

- 显示相关 service 已注册
- 必需依赖满足
- 不要求创建四视图界面
- 不要求做重型 VTK 预热

### 5.2 首批 descriptor 的硬约束

首批受管插件必须显式声明：

- `diagnostics.required_services`
- `diagnostics.service_ready_timeout_ms`
- `health_checks`

如果缺失这些字段，descriptor 直接视为不合格，不能再靠运行时代码猜测默认值。

## 6. Descriptor 规范与受管安装清单

本轮明确以下真相源规则：

- 源码中的人工维护源仍然是 `Plugins/*/platform/plugin.json`
- 运行时目录中的 `plugins/descriptors/*.json` 是构建产物和部署镜像
- `platform_runtime.json` 负责声明：
  - 默认运行模式
  - 本轮 `core_plugin_ids`
- 平台运行时只从 descriptor 生成受管安装清单

首批受管 descriptor 至少强制以下字段有效：

- `id`
- `display_name`
- `runtime.ctk_symbolic_name`
- `runtime.startup_policy`
- `runtime.bootstrap_level`
- `provides`
- `requires`
- `diagnostics.required_services`
- `diagnostics.service_ready_timeout_ms`
- `health_checks`

### 6.1 受管安装清单生成规则

主链在启动前必须：

1. 从 `platform_runtime.json` 读取 `core_plugin_ids`
2. 在运行时 descriptor 集合中找到对应插件
3. 校验 descriptor 完整性
4. 按 `bootstrap_level=core` 和必要依赖补齐首批受管安装集
5. 生成明确有序的安装计划和启动计划
6. 同时把这份计划暴露给 diagnostics 和状态页

### 6.2 硬约束

首批受管插件如果出现以下问题，必须在启动前被拦下：

- 缺少 descriptor
- descriptor 缺少 `runtime.ctk_symbolic_name`
- descriptor 缺少关键 diagnostics 字段
- descriptor 的依赖声明与当前受管范围冲突

### 6.3 与旧 `plugin_load_policy.json` 的关系

`plugin_load_policy.json` 在 Phase 1 中降级为兼容层配置，而不是主链真相源。

也就是说：

- 平台治理真相源：`platform_runtime.json + descriptor`
- 旧加载策略文件：过渡期兼容信息

后续如果两者冲突，应以受管 descriptor 计划为准，而不是继续让旧策略文件主导主启动链。

## 7. 组件职责边界

### 7.1 `CTKManager`

`CTKManager` 只负责 CTK 原语能力：

- framework 初始化 / 启动 / 停止
- 按明确清单安装 bundle
- 启动指定 bundle
- 查询 service 是否存在
- 发出 install/start 成败事件

`CTKManager` 不再负责：

- 决定扫描整个 `plugins/` 目录装谁
- 解释谁是核心插件
- 判定谁 `ready`
- 判定平台是否 `degraded` 或 `failed`

### 7.2 `PlatformStartupCoordinator`

`PlatformStartupCoordinator` 成为首批主启动链的真正执行器：

- 接收受管安装清单
- 驱动 `framework init -> install -> core start -> service ready`
- 记录 lifecycle 事件
- 输出统一阶段结果

### 7.3 `StartupOrchestrator`

`StartupOrchestrator` 保留为阶段调度器，但不再写插件特例：

- 负责 phase 顺序
- 负责阶段开始 / 结束 / 失败汇总
- 不再在内部硬编码某个插件的 service 逻辑

### 7.4 `main.cpp`

`main.cpp` 只做 composition root：

- 读取配置
- 加载 descriptor
- 创建 coordinator / diagnostics / state store
- 连接信号
- 启动 orchestrator

`main.cpp` 不再直接承载：

- `service ready` 轮询细节
- 某个插件的 warmup 细节
- 某个 service 的业务访问
- 大段插件状态桥接特判

### 7.5 UI

UI 包括 `MainInterfaceWidget` 和 diagnostics page，只消费平台状态与 diagnostics snapshot：

- 展示受管插件状态
- 展示阻塞点和失败点
- 根据 capability 快照决定是否允许进入流程

UI 不再参与：

- 判断 CTK 真相
- 决定谁该被启动
- 猜测谁算 ready

## 8. Diagnostics、验收与文档回写

本轮把 diagnostics 视为正式契约，而不是附属日志。

只要是首批受管插件，就必须能在诊断链路中看到至少以下事实：

- `discovered`
- `install`
- `start`
- `service_ready`
- `failed`
- `skipped`

控制台输出和 diagnostics page 必须使用同一套受管插件口径，不能再出现“控制台装了很多，平台页却没人解释为什么只认其中一部分”的情况。

### 8.1 验收分层

单元验收：

- 受管安装清单生成正确
- descriptor 缺失或关键字段缺失时明确失败
- `service ready` 判定符合新边界
- 非受管旧插件不参与 `platform ready`
- diagnostics 能给出阻塞点和失败点

运行时验收：

- 默认运行模式为 `facade_mode`
- 主启动链只安装首批受管插件
- `UserManagement / DicomViewer / FourViewDisplay` 可以进入受控 `ready`
- 非受管旧插件即使在运行目录存在，也不污染平台状态
- diagnostics page 能直接展示本轮受管插件矩阵和失败原因

反回退验收：

- 不允许重新回到“扫描整个目录即主链真相”
- 不允许 UI 层重新直接判断 CTK 真相
- 不允许把重型 warmup 重新塞回 `ready`
- 不允许继续依靠硬编码 service 访问决定平台状态

### 8.2 文档回写要求

本轮整改实施时必须同步更新：

- `docs/current_status_and_project_overview.md`
- `docs/superpowers/tracking/platform-migration-decision-log.md`
- `docs/superpowers/specs/2026-04-16-platform-kernel-governance-design.md`
- 本专项 implementation plan

每次 rollout 结论至少要写清：

- 本轮默认运行模式
- 本轮受管插件范围
- 本轮 `ready` 定义
- 本轮排除出主链的旧插件范围
- 本轮执行过的构建、测试和运行时验收命令
- 当前仍未纳管的风险点

## 9. Phase 1 实施优先级

本轮实施顺序固定为：

1. 先收口默认模式和受管安装清单
2. 再收口 `service ready` 判定和 descriptor 硬约束
3. 再拆出 `main.cpp` 中混杂的主链逻辑
4. 再补齐 diagnostics 和文档回写

优先级顺序固定为：

1. 结构稳定性
2. 主链可解释性
3. 启动性能

如果三者冲突，先保证结构稳定和诊断可信，再做性能优化。

## 10. 风险与后续切片

本轮完成后，仍然预期存在以下后续工作：

- `RegistrationCore` 和 `OpticalTracking` 的纳管与按需启动治理
- `warmup-tail` 的显式配置化，而不是保留在代码硬编码里
- 运行目录中历史 bundle 的进一步清理
- 更完整的 capability 和健康检查矩阵

但这些都属于 Phase 2 之后的治理切片，不应阻断当前 Phase 1 的强收口落地。
