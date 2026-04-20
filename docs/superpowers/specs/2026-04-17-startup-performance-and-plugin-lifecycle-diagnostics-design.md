# Startup Performance and Plugin Lifecycle Diagnostics Design

## Scope Note (2026-04-19)

- This document remains the target design for the startup performance and plugin lifecycle diagnostics slice.
- The currently accepted baseline in this worktree is the lifecycle-event-based diagnostics foundation plus the implementation-plan subset that is already wired through `PlatformDiagnosticsService` and `PlatformDiagnosticsPage`.
- The full page field matrix, expanded tables, and broader presentation rules defined below should be treated as follow-up delivery scope unless they are explicitly called out as implemented by a later rollout note.

## Implementation Links

- Base governance design: `docs/superpowers/specs/2026-04-16-platform-kernel-governance-design.md`
- Governance implementation plan: `docs/superpowers/plans/2026-04-16-platform-kernel-governance-implementation.md`
- Governance matrix: `docs/superpowers/tracking/platform-plugin-governance-matrix.md`
- Decision log: `docs/superpowers/tracking/platform-migration-decision-log.md`

日期：2026-04-17  
范围：`medicalpro` 平台治理后续专项“启动性能与插件生命周期诊断”设计  
目标：在不回退到 UI 直连 CTK、不打破现有 `observe_only / facade_mode / orchestrate_core` 三档治理边界的前提下，把平台启动过程变成可量化、可定位、可解释、可展示的时间线与生命周期诊断系统，明确回答：

1. 启动为什么慢
2. 慢在哪个阶段
3. 慢在哪个插件
4. 是 `install` 慢、`start` 慢、`service ready` 慢，还是 `warmup` 慢
5. 哪些失败会导致降级
6. 哪些问题应该在 diagnostics page 直接展示给开发者

## 1. 设计背景

平台治理第一阶段已经完成以下基础工作：

- 主启动链已经通过 `PlatformRuntimeConfig + PlatformStartupCoordinator` 接入 `observe_only / facade_mode / orchestrate_core`
- 核心插件已具备 `platform/plugin.json`
- `PlatformDiagnosticsService` 与 `PlatformDiagnosticsPage` 已存在基础诊断闭环
- UI 层已清理直接 `CTKManager::instance()` 与直接 `getService<...>()` 的违规入口

但当前诊断能力仍然停留在“阶段级 trace + 插件状态快照”：

- `StartupTrace` 只能说明某个 phase 成功或失败，不能说明 phase 内哪个插件、哪个子步骤更慢
- `PlatformDiagnosticsService` 只能把 observation 与 snapshot 拼装起来，无法得出“最慢插件”“阻塞点”“失败点”
- `PlatformDiagnosticsPage` 只能显示基础插件表与 phase 表，无法直接回答开发者最关心的性能与生命周期问题
- `CTK` 已有“安装了”“激活了”“服务是否可查到”这些观察点，但平台侧还没有统一的生命周期事件模型来吸收它们

因此本专项不做插件内部重构，不做启动链重写，而是做“平台解释层增强”：

- 把启动过程建模成事件账本
- 从事件账本派生量化时间线与插件生命周期快照
- 把失败、降级、恢复建议和阻塞信息统一汇总到 diagnostics service
- 让 diagnostics page 成为平台治理层面对开发者的正式解释界面

## 2. 现状梳理

### 2.1 当前启动主链

当前主链已固定为：

1. 读取 `config/platform_runtime.json`
2. 按运行模式决定是否初始化 CTK framework
3. 安装插件
4. 启动核心插件
5. 启动 deferred 插件
6. 执行 service warmup

其中：

- `observe_only`：只观察，不接管 framework init、plugin install、core start、deferred start、service warmup
- `facade_mode`：接管 framework init、plugin install、core start；不接管 deferred start、service warmup
- `orchestrate_core`：接管全部平台管理阶段

### 2.2 当前状态模型

现有平台状态模型仍沿用第一阶段固定 6 态：

- `discovered`
- `installed`
- `starting`
- `ready`
- `degraded`
- `failed`

本专项保持该 6 态不变，但补充“为什么进入该状态”的事件级解释。

### 2.3 当前 `ready` 边界

本专项继续遵守既有治理规则：

> `ready = 依赖满足 + 服务注册成功 + 轻量健康检查通过`

以下内容不并入 `ready`：

- Python 初始化
- GPU 预热
- 设备真实握手
- 大量数据预加载
- 重型算法准备

因此：

- `service ready` 是 ready-path 的最后一段
- `warmup` 是 ready 之后的独立生命周期步骤
- 页面必须能区分“已 ready 但 warmup 未完成”和“尚未 ready”

### 2.4 当前诊断缺口

现状最主要的解释缺口如下：

1. phase trace 没有插件维度  
   `CriticalPluginStart 820ms` 无法回答究竟是 `DicomViewer` 还是 `FourViewDisplay` 慢

2. phase trace 没有步骤维度  
   无法区分是 `install` 慢、`start` 慢、`service ready` 慢还是 `warmup` 慢

3. 缺少“平台状态与 CTK 状态不一致”的显式问题模型  
   例如插件已 `ACTIVE`，但服务未注册，当前只能表现成“没 ready”，解释不充分

4. diagnostics page 没有问题列表和排序规则  
   开发者无法一眼看到最慢插件、阻塞点、失败点和恢复建议

## 3. 设计目标与非目标

### 3.1 设计目标

本专项必须达成：

1. 启动过程可被量化成时间线
2. 插件生命周期可被建模成稳定事件
3. 平台可明确记录 `install / start / service ready / warmup / failed / degraded`
4. `PlatformDiagnosticsService` 能派生出开发者关心的结论
5. `PlatformDiagnosticsPage` 能直接展示性能瓶颈、失败点和恢复建议
6. 设计必须兼容三种运行模式
7. 设计必须兼容现有平台治理层，不回退到 CTK 直连 UI

### 3.2 明确非目标

本专项不做：

- 不替换 CTK
- 不重写现有插件 activator
- 不把所有插件内部逻辑改造成平台契约事件源
- 不在本轮引入新的实现计划或代码提交
- 不把 diagnostics page 变成“日志终端”

## 4. 方案选择结论

本专项采用：

> 生命周期事件账本 + 派生时间线/快照 的混合设计

不采用“只扩 phase log”的原因：

- 只能得到更长的日志，仍难以稳定识别插件生命周期
- 很难表达 `service ready`、`degraded`、`skipped_by_mode` 这类跨 phase 语义

不采用“完全 descriptor 驱动大改 schema”的原因：

- 这轮目标是先把解释能力做完整，不宜把治理工作扩大成 descriptor 全面重构

最终结论：

- 底层记录统一的 `PlatformLifecycleEvent`
- 中层派生 `PlatformStartupTraceEntry` 与 `PlatformPluginLifecycleSnapshot`
- 上层由 `PlatformDiagnosticsService` 输出 `PlatformDiagnosticSnapshot`
- diagnostics page 只消费诊断快照，不直接感知 CTK

## 5. 数据模型设计

### 5.1 新增事件类型

新增平台生命周期事件枚举 `PlatformLifecycleEventKind`：

```cpp
enum class PlatformLifecycleEventKind
{
    StartupSessionStarted,
    StartupSessionFinished,
    PhaseStarted,
    PhaseFinished,
    PluginInstallStarted,
    PluginInstallFinished,
    PluginStartStarted,
    PluginStartFinished,
    PluginServiceReady,
    PluginWarmupStarted,
    PluginWarmupFinished,
    PluginFailed,
    PluginDegraded,
    PluginSkippedByMode
};
```

新增步骤枚举 `PlatformLifecycleStep`：

```cpp
enum class PlatformLifecycleStep
{
    None,
    Install,
    Start,
    ServiceReady,
    Warmup
};
```

新增结果枚举 `PlatformLifecycleResult`：

```cpp
enum class PlatformLifecycleResult
{
    Running,
    Succeeded,
    Failed,
    Degraded,
    Skipped,
    Timeout
};
```

新增问题等级枚举 `PlatformDiagnosticSeverity`：

```cpp
enum class PlatformDiagnosticSeverity
{
    Info,
    Warning,
    Error,
    Critical
};
```

### 5.2 生命周期事件结构

新增结构 `PlatformLifecycleEvent`：

```cpp
struct PlatformLifecycleEvent
{
    QString sessionId;
    PlatformLifecycleEventKind kind = PlatformLifecycleEventKind::PhaseStarted;
    PlatformLifecycleStep step = PlatformLifecycleStep::None;
    PlatformLifecycleResult result = PlatformLifecycleResult::Running;
    PlatformRuntimeMode runtimeMode = PlatformRuntimeMode::ObserveOnly;

    QString phaseKey;
    QString phaseLabel;
    QString pluginId;
    QString ctkSymbolicName;

    qint64 offsetMs = 0;
    qint64 durationMs = 0;
    bool blockingStartup = false;
    bool critical = false;

    QString reasonCode;
    QString detail;
    QStringList missingServices;
    QStringList missingCapabilities;
    QStringList missingPlugins;
    QStringList recoveryHints;
};
```

字段约束：

- `sessionId`：一次应用启动过程唯一标识
- `phaseKey`：例如 `ctk_framework_init`、`plugin_installation`、`critical_plugin_start`
- `pluginId`：平台 descriptor id，不使用 CTK symbolic name 代替唯一主键
- `ctkSymbolicName`：仅用于桥接与页面展示
- `offsetMs`：相对本次启动开始的偏移时间
- `durationMs`：仅对 finished/timeout/failed/skipped 类型事件有意义
- `blockingStartup`：标识该事件是否进入 ready-path 关键链
- `reasonCode`：稳定机器码，如 `service_missing`、`service_ready_timeout`、`warmup_failed`

### 5.3 扩展 `StartupTrace`

保留现有 `PlatformStartupTraceEntry`，但把它从“phase 记录”扩展为“可排序 span”：

```cpp
struct PlatformStartupTraceEntry
{
    QString spanId;
    QString parentSpanId;

    QString phaseKey;
    QString phaseLabel;
    QString pluginId;
    QString ctkSymbolicName;

    PlatformLifecycleStep step = PlatformLifecycleStep::None;
    PlatformLifecycleResult result = PlatformLifecycleResult::Running;

    bool success = false;
    bool blockingStartup = false;

    qint64 startOffsetMs = 0;
    qint64 endOffsetMs = 0;
    qint64 elapsedMs = 0;

    QString reasonCode;
    QString detail;
};
```

兼容规则：

- 原有 diagnostics page 如果只读取 `phaseKey / phaseLabel / success / elapsedMs / detail` 仍可工作
- 新页面可进一步使用 `pluginId / step / blockingStartup / reasonCode / startOffsetMs`

### 5.4 新增插件生命周期快照

现有 `PlatformPluginRuntimeSnapshot` 无法承载性能信息和失败原因，因此新增：

```cpp
struct PlatformPluginLifecycleSnapshot
{
    QString pluginId;
    QString ctkSymbolicName;
    QString displayName;

    PlatformBootstrapLevel bootstrapLevel = PlatformBootstrapLevel::Deferred;
    PlatformStartupPolicy startupPolicy = PlatformStartupPolicy::Disabled;
    PlatformPluginState state = PlatformPluginState::Discovered;

    qint64 installMs = 0;
    qint64 startMs = 0;
    qint64 serviceReadyMs = 0;
    qint64 warmupMs = 0;
    qint64 blockingMs = 0;

    PlatformLifecycleStep slowestStep = PlatformLifecycleStep::None;
    bool serviceReadyObserved = false;
    bool warmupCompleted = false;
    bool startupBlocked = false;

    QString lastReasonCode;
    QString lastDetail;
    QStringList missingRequiredServices;
    QStringList missingRequiredCapabilities;
    QStringList missingRequiredPlugins;
    QStringList degradedReasons;
    QStringList failureReasons;
    QStringList recoveryHints;
};
```

约束：

- `blockingMs` 仅统计 ready-path 关键链时间
- `warmupMs` 独立统计，不计入 ready-path
- `slowestStep` 在 `install / start / serviceReady / warmup` 四段中取最大者

### 5.5 扩展诊断快照

扩展 `PlatformDiagnosticSnapshot`：

```cpp
struct PlatformDiagnosticSummary
{
    PlatformRuntimeMode runtimeMode = PlatformRuntimeMode::ObserveOnly;
    bool frameworkReady = false;
    bool platformReady = false;

    qint64 startupReadyPathMs = 0;
    qint64 startupWarmupTailMs = 0;
    qint64 fullObservedStartupMs = 0;

    QString slowestPhaseKey;
    QString slowestPluginId;
    QString blockingSpanLabel;
    QString failurePointLabel;
};

struct PlatformDiagnosticProblem
{
    PlatformDiagnosticSeverity severity = PlatformDiagnosticSeverity::Info;
    QString pluginId;
    QString phaseKey;
    PlatformLifecycleStep step = PlatformLifecycleStep::None;
    QString reasonCode;
    QString detail;
    bool blockingStartup = false;
    qint64 firstOffsetMs = 0;
    QStringList impactCapabilities;
    QStringList recoveryHints;
};

struct PlatformDiagnosticSnapshot
{
    PlatformDiagnosticSummary summary;
    PlatformCapabilitySnapshot capabilitySnapshot;
    QVector<PlatformPluginLifecycleSnapshot> pluginLifecycle;
    QVector<PlatformStartupTraceEntry> startupTrace;
    QVector<PlatformDiagnosticProblem> problems;
    QStringList recoveryHints;
};
```

兼容策略：

- 现有 `plugins` 字段可以过渡到 `pluginLifecycle`
- 若实现阶段考虑兼容旧页面，可在一段版本周期内保留 `plugins` 和 `pluginLifecycle` 并行输出

## 6. 事件生成规则

### 6.1 启动 session 级事件

每次应用启动固定生成：

1. `StartupSessionStarted`
2. 若启动自然结束，则 `StartupSessionFinished`

这两条事件用于计算：

- `fullObservedStartupMs`
- startup session 级会话 id
- 一次启动是否完整结束

### 6.2 phase 级事件

`StartupOrchestrator` 对每个 phase 固定生成：

- `PhaseStarted`
- `PhaseFinished`

phase 事件负责回答：

- 慢在哪个阶段
- 某个阶段是否被当前运行模式跳过
- 某个阶段失败是否属于阻塞 ready-path 的关键失败

### 6.3 插件级事件

每个插件生命周期固定按以下语义记录：

1. `PluginInstallStarted`
2. `PluginInstallFinished`
3. `PluginStartStarted`
4. `PluginStartFinished`
5. `PluginServiceReady`
6. `PluginWarmupStarted`
7. `PluginWarmupFinished`

异常时追加：

- `PluginFailed`
- `PluginDegraded`
- `PluginSkippedByMode`

### 6.4 四段时间定义

#### `install`

定义：

> 从平台开始安装该插件，到插件已进入 CTK installed/resolved 可启动状态

说明：

- 该时间不包含依赖启动和 service 等待
- 用于回答“是不是 install 慢”

#### `start`

定义：

> 从平台开始请求启动该插件，到插件 activator/start 返回且插件进入 CTK active 状态

说明：

- 该时间不等于 `service ready`
- 若插件已 `ACTIVE` 但服务还没起来，`start` 成功，`service ready` 仍未完成

#### `service ready`

定义：

> 从插件 `start finished` 起，到平台确认其 required services 与 required capabilities 达到 ready 判定为止

来源：

- descriptor `requires.services`
- descriptor `requires.capabilities`
- `health_checks` 中的 `service_registered`
- 运行时 service lookup/health check 通过

#### `warmup`

定义：

> 从插件进入 ready 后，到平台标记其 warmup 任务完成为止

说明：

- warmup 是 ready 之后的尾段
- warmup 慢不会自动推翻 ready
- warmup 失败只在规则声明需要降级时才升级为 `degraded`

### 6.5 `failed` 与 `degraded` 记录规则

#### 进入 `failed`

以下情况直接记 `PluginFailed`：

- install 失败
- start 失败
- required dependency 明确缺失且无法满足
- service ready 超时且该插件处于 ready-path 必经链
- core plugin health check 失败且该 health check 被标记为 ready 必需

#### 进入 `degraded`

以下情况记 `PluginDegraded`：

- optional dependency 缺失
- warmup 失败，但插件已 ready
- deferred/on-demand 插件在非关键链上出现 service ready 超时
- 插件 active 但部分非关键服务不可用
- capability 可部分开放，但完整功能未满足

### 6.6 `skipped_by_mode` 记录规则

三种模式下必须显式记录跳过，而不是静默不记：

- `observe_only`
  - phase 级记录 `framework init / install / core start / deferred start / warmup = skipped_by_mode`
  - 不生成平台托管的插件 install/start/warmup 事件
- `facade_mode`
  - 记录 framework init、install、core start
  - 对 deferred start、warmup 记 `skipped_by_mode`
- `orchestrate_core`
  - 全量记录

这样 diagnostics page 才能明确解释：

- “为什么当前看不到 deferred plugin 启动耗时”
- “为什么 warmup 未执行”
- “当前不是故障，而是当前模式本就跳过”

## 7. descriptor 与治理矩阵扩展

### 7.1 descriptor 扩展原则

本轮不强制立即重写所有 descriptor，但允许为每个核心插件增加一个可选 `diagnostics` 块：

```json
{
  "diagnostics": {
    "required_services": ["DicomViewerService"],
    "service_ready_timeout_ms": 3000,
    "warmup_tasks": ["dicom_cache", "preview_pipeline"],
    "warmup_timeout_ms": 15000,
    "warmup_impacts_ready": false,
    "degrade_on": [
      "optional_dependency_missing",
      "warmup_failed"
    ]
  }
}
```

约束：

- `required_services` 用于补充现有 `requires.services`
- `service_ready_timeout_ms` 与 `warmup_timeout_ms` 是显式治理契约，不是代码里的隐式 magic number
- `warmup_impacts_ready` 第一阶段默认必须为 `false`

### 7.2 治理矩阵回写内容

若 descriptor 暂未补齐，治理矩阵至少应补以下列：

- `required service`
- `service ready timeout`
- `warmup task`
- `warmup degrade impact`
- `diagnostic owner`

核心插件建议基线如下：

| Plugin | Required Service Baseline | Service Ready Timeout | Warmup Task Baseline | Warmup Failure Impact |
| --- | --- | --- | --- | --- |
| UserManagement | `UserManagementService` | 3000 ms | session cache | warning |
| DicomViewer | `DicomViewerService` | 5000 ms | data path precheck | degraded |
| FourViewDisplay | `FourViewDisplayService` | 5000 ms | render backend warmup | degraded |
| RegistrationCore | `RegistrationService` | 5000 ms | core binary probe | degraded |
| OpticalTracking | `InstrumentManagementService` or tracking adapter service | 5000 ms | adapter probe | degraded |

## 8. `PlatformDiagnosticsService` 聚合规则

### 8.1 聚合输入

`PlatformDiagnosticsService` 后续聚合输入固定为三类：

1. `PlatformLifecycleEvent` 账本
2. `PlatformStateStore` 当前 descriptor/state/capability 快照
3. `CtkRuntimeSnapshotCollector` 观察到的 framework/plugin/service runtime 状态

### 8.2 聚合输出

必须派生四层结果：

1. summary
2. startup trace
3. plugin lifecycle table
4. problem list + recovery hints

### 8.3 最慢插件识别

规则：

1. 先过滤 `blockingStartup = true` 的插件生命周期数据
2. 取 `blockingMs` 最大的插件
3. 若并列，按 `serviceReadyMs > startMs > installMs > warmupMs` 优先级选最慢步骤更靠前者

输出：

- `summary.slowestPluginId`
- 对应插件行高亮

### 8.4 阻塞点识别

规则：

1. 在 ready-path 上筛选 `blockingStartup = true` 的 trace span
2. 若存在失败 span，取最先发生的失败 span
3. 若不存在失败 span，取耗时最大的 blocking span

输出：

- `summary.blockingSpanLabel`
- problem list 中追加 `blocking_point` 问题

### 8.5 失败点识别

规则：

1. 在 lifecycle events 中筛选 `Failed / Timeout`
2. 按 `offsetMs` 升序取第一条
3. 若无失败但有 degraded，则取第一条 degraded 事件作为次级失败点

输出：

- `summary.failurePointLabel`
- problem list 中追加对应 failure item

### 8.6 恢复建议生成

恢复建议不直接拼接原始 detail，而是基于 `reasonCode` 映射：

| reasonCode | 恢复建议 |
| --- | --- |
| `descriptor_missing` | 检查 `plugins/descriptors` 是否完整部署 |
| `plugin_install_failed` | 检查插件二进制、依赖 DLL 与 runtime artifact layout |
| `plugin_start_failed` | 检查 activator/start 内异常与依赖插件状态 |
| `service_missing` | 检查服务是否注册、接口名是否一致、health check 是否过严 |
| `service_ready_timeout` | 检查插件 active 后是否阻塞主线程或延迟注册服务 |
| `warmup_failed` | 检查 warmup 任务是否依赖未声明资源 |
| `skipped_by_mode` | 检查当前 runtime mode 是否故意跳过该阶段 |
| `ctk_platform_state_mismatch` | 检查 CTK active 与平台 ready 条件是否一致 |

### 8.7 平台状态与 CTK 状态不一致识别

这是本专项必须直接识别的治理问题：

- CTK `ACTIVE`，但平台未 `ready`
- 插件已 `installed`，但平台仍处于 `discovered`
- descriptor 声称提供 capability，但 capability 未解锁

出现上述情况时统一生成问题：

- `reasonCode = ctk_platform_state_mismatch`
- `severity = warning` 或 `error`
- diagnostics page 直接展示

## 9. `PlatformDiagnosticsPage` 页面设计

### 9.1 页面目标

diagnostics page 的职责不是“把所有日志摊开”，而是让开发者在一页内回答：

- 当前是哪种运行模式
- 启动 ready-path 总共耗时多少
- 最慢插件是谁
- 当前阻塞点在哪里
- 哪个插件失败或降级
- 当前应该先看什么恢复建议

### 9.2 页面信息架构

页面固定分四块：

1. 顶部摘要区
2. 插件生命周期表
3. 启动时间线表
4. 问题与恢复建议区

### 9.3 顶部摘要区字段

必须展示：

- `runtime mode`
- `framework ready`
- `platform ready`
- `startup ready-path ms`
- `warmup tail ms`
- `full observed startup ms`
- `slowest plugin`
- `blocking point`
- `failure point`

显示规则：

- `warmup tail ms` 在 `observe_only / facade_mode` 下可显示 `skipped_by_mode`
- 若无失败点，显示 `none`
- 若平台未 ready，则 `blocking point` 必须高亮

### 9.4 插件生命周期表字段

插件表必须展示以下列：

1. `plugin id`
2. `ctk symbolic name`
3. `bootstrap`
4. `startup policy`
5. `state`
6. `install ms`
7. `start ms`
8. `service ready ms`
9. `warmup ms`
10. `blocking ms`
11. `slowest step`
12. `missing deps`
13. `last reason`
14. `recovery`

状态渲染规则：

- `ready`：正常色
- `degraded`：警告色
- `failed`：错误色
- `discovered / installed / starting`：中性状态色

### 9.5 启动时间线表字段

时间线表必须展示：

1. `start offset`
2. `duration`
3. `scope`
4. `plugin / phase`
5. `step`
6. `result`
7. `blocking`
8. `detail`

其中：

- `scope` 取值：`phase`、`plugin`
- `plugin / phase` 优先展示插件 display name，无插件则展示 phase
- `blocking` 直接显示 `yes / no`

### 9.6 问题列表字段

问题列表必须展示：

1. `severity`
2. `plugin`
3. `reason code`
4. `impact capability`
5. `detail`
6. `recovery hint`

以下问题必须直接展示给开发者：

- core plugin install 失败
- core plugin start 失败
- `service ready` 超时
- 插件 active 但服务未 ready
- 平台状态与 CTK 状态不一致
- 缺失 required dependency
- warmup 失败且被标记为 `degraded`
- 关键链路被当前插件阻塞超过阈值
- 当前模式跳过某阶段，导致某些指标不可用

### 9.7 排序规则

插件表默认排序：

1. `state` 严重度：`failed > degraded > starting > installed > discovered > ready`
2. `blockingMs` 降序
3. `pluginId` 升序

时间线表默认排序：

1. `startOffsetMs` 升序
2. `elapsedMs` 降序

问题列表默认排序：

1. `severity` 降序
2. `blockingStartup` 降序
3. `firstOffsetMs` 升序

## 10. 失败、降级与能力锁定规则

### 10.1 核心 ready-path 插件

对于 `core + eager` 插件：

- `install` 失败：插件 `failed`，平台整体 `degraded`
- `start` 失败：插件 `failed`，平台整体 `degraded`
- `service ready` 超时：插件 `failed`，平台整体 `degraded`
- `warmup` 失败：默认插件 `degraded`，平台不回退 `ready`

### 10.2 deferred / on-demand 插件

对于 `deferred + on_demand` 插件：

- 未触发 `ensureReady()` 前，不参与应用启动 ready-path
- 被触发后 `install/start/service ready` 失败：插件 `failed` 或 `degraded`，对应 domain capability 锁定
- 不影响应用外壳与 diagnostics page 进入

### 10.3 optional 依赖

- optional dependency 缺失：插件 `degraded`
- 不影响平台 ready-path
- 必须在 diagnostics page 问题列表中直显

### 10.4 warmup 失败

warmup 失败默认处理：

- 如果 `warmup_impacts_ready = false`：`ready` 保持，插件记 `degraded`
- 如果未来某插件被治理规则显式声明 `warmup_impacts_ready = true`：可升级为 `failed`

第一阶段默认不允许把 warmup 纳入 ready。

## 11. 三种运行模式对齐

### 11.1 `observe_only`

行为：

- 不接管 framework init、plugin install、core start、deferred start、warmup
- phase 级 trace 中显式记录 `skipped_by_mode`
- 页面允许显示“当前模式下无平台托管 install/start 数据”

页面重点：

- 以 phase 解释为主
- 明确提示开发者：当前模式不具备完整插件生命周期计时能力

### 11.2 `facade_mode`

行为：

- 接管 framework init、plugin install、core start、service ready
- deferred start、warmup 记 `skipped_by_mode`

页面重点：

- 能直接比较核心插件 `install / start / service ready`
- deferred 插件只显示“尚未纳入当前模式”

### 11.3 `orchestrate_core`

行为：

- 全量接管
- 所有核心插件与 deferred 插件都可生成完整生命周期事件
- warmup 指标完整可见

页面重点：

- 输出完整 ready-path 与 warmup tail
- 可完整识别最慢插件与阻塞点

## 12. 实现边界建议

本节只定义后续实现边界，不进入 implementation plan。

### 12.1 事件写入边界

建议由以下层写事件：

- `StartupOrchestrator`：写 session/phase 事件
- `PlatformStartupCoordinator`：写 mode-skip、ensureReady、deferred start 相关事件
- `CTKManager` 桥接层：写 install/start/active 相关事件
- `PlatformDiagnosticsService`：只聚合，不直接伪造底层生命周期事实

### 12.2 service ready 判定边界

建议由平台治理层统一判定，不交给 UI，不交给单个页面：

- `PlatformStateStore`
- `CtkRuntimeSnapshotCollector`
- health check 结果

### 12.3 页面边界

`PlatformDiagnosticsPage` 只消费：

- `PlatformDiagnosticSnapshot`

禁止新增：

- 页面直接读 `CTKManager`
- 页面直接做 service lookup
- 页面自行拼接插件状态推理

## 13. 测试设计

### 13.1 需要新增的测试

新增单元测试：

- `tests/unit/PlatformStartupTraceRecorderTest.cpp`
  - 验证 span 生成、offset/duration 计算、blocking 标记、mode skip 记录

- `tests/unit/PlatformPluginLifecycleAggregatorTest.cpp`
  - 验证从 lifecycle events 派生插件 install/start/service ready/warmup 四段耗时

### 13.2 需要扩展的现有测试

扩展：

- `tests/unit/PlatformStartupCoordinatorTest.cpp`
  - 验证三种模式下哪些事件应记录、哪些步骤应 `skipped_by_mode`

- `tests/unit/PlatformDiagnosticsServiceTest.cpp`
  - 验证 `最慢插件 / 阻塞点 / 失败点 / 恢复建议`
  - 验证 `service ready` 与 `warmup` 分离
  - 验证 `ctk_platform_state_mismatch`

- `tests/unit/PlatformDiagnosticsPageTest.cpp`
  - 验证摘要区新增字段
  - 验证插件表排序
  - 验证问题列表渲染

- `tests/unit/PlatformUiBridgeTest.cpp`
  - 验证 `plugin active but service not ready`
  - 验证 service ready 迟到事件
  - 验证 on-demand 插件失败后的 domain degrade 映射

- `tests/unit/UiCtkDecouplingAcceptanceTest.cpp`
  - 保持原有 CTK 解耦验收，确保 diagnostics 增强后不回退到 UI 直连

### 13.3 必测场景矩阵

必须覆盖：

1. 核心插件 install 慢，但最终成功
2. 核心插件 start 慢，但最终成功
3. 核心插件 active 了，但 service ready 超时
4. warmup 很慢，但 ready 已经成立
5. warmup 失败导致 degraded
6. deferred/on-demand 插件在 `facade_mode` 下被跳过
7. deferred/on-demand 插件在 `orchestrate_core` 下被纳入完整时间线
8. CTK active 与平台 ready 不一致
9. optional dependency 缺失但应用外壳仍可启动

## 14. 验收命令

后续实现完成后的专项验收建议至少包括：

```powershell
cmake --build build_x64_noctk --config Release --target ^
  platform_startup_trace_recorder_test ^
  platform_plugin_lifecycle_aggregator_test ^
  platform_startup_coordinator_test ^
  platform_diagnostics_service_test ^
  platform_diagnostics_page_test ^
  platform_ui_bridge_test ^
  ui_ctk_decoupling_acceptance_test
```

```powershell
ctest --test-dir build_x64_noctk -C Release -R ^
  "platform_startup_trace_recorder_test|platform_plugin_lifecycle_aggregator_test|platform_startup_coordinator_test|platform_diagnostics_service_test|platform_diagnostics_page_test|platform_ui_bridge_test|ui_ctk_decoupling_acceptance_test" ^
  --output-on-failure
```

```powershell
rg -n "CTKManager::instance\(|getService<" UI\NewPages UI\MainInterfaceWidget.cpp
```

验收结果必须能够证明：

- 启动慢问题可测量
- 慢点可定位到 phase + plugin + step
- 失败与降级可解释
- diagnostics page 可直接给开发者恢复建议
- UI 未回退到 CTK 直连

## 15. 文档回写点

本专项后续实现落地时，文档必须同步回写：

1. `docs/superpowers/specs/2026-04-16-platform-kernel-governance-design.md`
   - 补充 `StartupTrace` 已从阶段级扩展到生命周期时间线

2. `docs/superpowers/tracking/platform-plugin-governance-matrix.md`
   - 为核心插件补 `required service / service ready timeout / warmup task / failure impact`

3. `docs/superpowers/tracking/platform-migration-decision-log.md`
   - 记录“ready-path 与 warmup-tail 分离”
   - 记录“采用生命周期事件账本而非单纯 phase log”

4. 本专项 spec 本身
   - 若后续结构命名、字段或判定矩阵发生变化，必须同步更新

## 16. 设计结论

本专项的核心不是“多加一些日志”，而是建立一套平台治理层自己的解释语言：

- 底层用生命周期事件表达事实
- 中层用时间线和插件快照表达结构
- 上层用问题列表和恢复建议表达结论

设计落地后，平台将能够稳定回答：

- 启动为什么慢
- 慢在哪个阶段
- 慢在哪个插件
- 是 `install / start / service ready / warmup` 哪一段慢
- 哪些失败导致降级
- 哪些问题需要在 diagnostics page 直接展示给开发者

同时，该设计继续遵守第一阶段治理边界：

- 不替换 CTK
- 不回退 UI 直连
- 不混淆 `ready` 与 `warmup`
- 不打破 `observe_only / facade_mode / orchestrate_core` 的运行模式分层
