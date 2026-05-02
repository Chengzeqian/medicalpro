# Ankle Navigation Architecture Remediation Design

**日期：** 2026-04-29  
**范围：** `medicalpro` 踝关节置换导航主链的结构治理  
**目标：** 在不引入新的重型宿主框架的前提下，收口启动链、`NavigationPage` 职责和 VTK 宿主边界，为后三个创新点的稳定开发和实验评估提供可维护底座。

## 1. 背景

当前踝关节导航主链已经具备病例工作区、规划、点配准、导航准入和评估导出的基本骨架，但结构层面存在明显问题：

- `main.cpp` 同时承担应用启动、运行时宿主初始化、启动编排、主窗口启动、后台激活和诊断兜底
- `UI/NewPages/NavigationPage.cpp` 同时承担器械、分割、规划、配准、导航、评估、VTK 嵌入和 tracking 状态管理
- VTK 相关能力分散在 `Framework`、`UI/Widgets`、`Plugins/*/widgets` 多处，宿主边界不清
- `Framework` 目录文件过多，运行时治理代码与业务代码混杂，后续算法开发会继续堆在大文件上

这些问题已经开始直接影响开发效率和论文主线推进：创新点的 baseline、指标和实验闭环还未完成，而现有结构会放大后续开发成本和回归风险。

## 2. 设计目标

本次结构治理只解决最痛的主链问题，不做泛化平台重构。

### 2.1 必达目标

- 将 `main.cpp` 收口为“创建应用 + 触发启动引导 + 退出处理”的薄入口
- 将 `NavigationPage` 收口为“页面壳 + 阶段切换 + 信号连接”，把具体业务编排下沉
- 将 VTK 相关职责收口为统一的页面嵌入和渲染宿主管理边界
- 明确 `Framework/Navigation`、`Framework/Platform`、`Framework/VTK` 三层边界
- 为创新点 1/2/3 的新增策略、实验运行器和评估导出预留稳定挂载点

### 2.2 非目标

- 不做全工程目录大搬家
- 不重写现有 `StartupOrchestrator`
- 不重写 `FourViewDisplay`、`PointRegistration`、`OpticalTracking` 的核心算法
- 不追求一次性消灭所有历史耦合，只处理主链阻塞点

## 3. 当前问题归因

### 3.1 启动链问题

`main.cpp` 当前既包含 VTK 初始化，也包含 runtime host 初始化、插件激活、服务就绪等待、welcome/main interface 展示和诊断处理。结果是：

- 启动逻辑难以测试
- 单个服务异常会让入口文件继续膨胀
- 创新链路需要新增 warmup、病例恢复或实验入口时，没有明确扩展点

### 3.2 页面问题

`NavigationPage` 已经成为“所有功能都往里放”的容器，直接后果是：

- 一次修改容易影响多个阶段
- 页面状态无法稳定复用到 replay、experiment、论文评估模式
- UI 行为和业务判断耦合，测试只能靠重 UI 集成

### 3.3 VTK 问题

当前 VTK 相关能力至少分成三类但没有统一边界：

- widget 创建与上下文校验
- render window / renderer 生命周期管理
- 页面中的嵌入、切换、暂停、恢复

这导致导航页、器械预览、配准页、光学注册页各自维护一套近似逻辑。

## 4. 目标结构

本次采用“最小收口，不额外造平台”的方案。

### 4.1 启动链目标结构

`main.cpp`

- 只负责 `QApplication` 创建
- 调用 `AppBootstrap` 启动
- 处理退出码和最外层异常

`Framework/Platform/Startup/AppBootstrap`

- 创建并持有 runtime host、startup context、主界面引用
- 统一组装启动依赖
- 调用 phase 注册器

`Framework/Platform/Startup/StartupPhaseRegistrar`

- 负责把 runtime init、managed plugin activation、service warmup 等 phase 注册到 `StartupOrchestrator`
- 每个 phase handler 只关注单一 phase 逻辑

`Framework/Platform/Startup/StartupUiCoordinator`

- 负责 welcome/shell/main interface 的展示切换
- 负责后台启动成功/失败后的 UI 表现
- 负责把诊断结果反馈到页面层

### 4.2 导航页目标结构

`NavigationPage` 保留为页面壳，不再承载完整业务编排。页面内新增三个轻量协作者：

`NavigationWorkflowContext`

- 持有 `caseId`、`patientId`、路径、当前阶段、当前 planning/registration/navigation/evaluation 快照
- 作为整个导航页的共享上下文

`NavigationServiceBundle`

- 统一封装页面需要访问的外部服务
- 负责获取 `segmentationService`、`pointRegistrationService`、`opticalTrackingService`、`fourViewService`、`instrumentService`
- 页面不再到处直接从 adapter 拉服务

`NavigationWorkflowCoordinator`

- 负责五阶段流程切换：准备、规划、配准、导航、评估
- 负责调用具体子流程控制器
- 负责把流程结果写回 `NavigationWorkflowContext`

在此基础上，分三个阶段控制器，而不是过度拆分：

- `PreparationPlanningController`
- `RegistrationController`
- `NavigationEvaluationController`

这样既能把 1700+ 行页面文件压下来，也不会引入过多中间层。

### 4.3 VTK 目标结构

保留现有 `VTKWidgetFactory` 和 `VTKWidgetPool`，但新增统一的页面级宿主边界：

`Framework/VTK/EmbeddedVtkViewHost`

- 统一负责 widget 创建、attach、detach、pause、resume、dispose
- 统一处理 placeholder 与失败兜底

`UI/NewPages/Navigation/navigation_vtk_bridge`

- 负责导航页内的 `FourView` 和 `Registration VTK widget` 嵌入
- 不包含业务判断，只管理 VTK 视图生命周期

首轮仅要求导航页接入该边界，器械预览和其他 VTK widget 后续按收益逐步迁移。

## 5. 目录与边界约束

### 5.1 `Framework/Platform`

只放：

- runtime host
- startup orchestration
- UI bridge / legacy adapter
- diagnostics / state store

禁止再向这里放导航业务逻辑。

### 5.2 `Framework/Navigation`

只放：

- 病例工作区
- planning 持久化
- evaluation / experiment / 指标定义
- 导航工作流上下文和页面编排器

禁止放底层 VTK 细节和 plugin runtime 细节。

### 5.3 `Framework/VTK`

只放：

- widget factory
- widget pool
- OpenGL / context guard
- embedded view host

禁止放导航、分割、配准等业务判断。

## 6. 分阶段实施

### 阶段 A1：启动链收口

- 提取 `AppBootstrap`
- 提取 `StartupPhaseRegistrar`
- 提取 `StartupUiCoordinator`
- `main.cpp` 只保留顶层入口职责

### 阶段 A2：导航页收口

- 提取 `NavigationWorkflowContext`
- 提取 `NavigationServiceBundle`
- 提取 `NavigationWorkflowCoordinator`
- 将 `NavigationPage` 降为页面壳

### 阶段 A3：VTK 宿主边界收口

- 提取 `EmbeddedVtkViewHost`
- 提取 `navigation_vtk_bridge`
- 统一管理导航页内四视图和配准视图的嵌入/清理

## 7. 验收标准

结构治理完成后，至少满足以下条件：

1. `main.cpp` 不再直接内联注册大段 startup phase 处理逻辑
2. `NavigationPage.cpp` 的职责收缩为页面交互、阶段切换和协调器调用
3. 导航页不再自己直接散落管理 VTK widget 创建、布局嵌入和清理
4. 创新点相关新代码可以挂到 `Framework/Navigation` 或对应 plugin 下，不需要继续塞进 `main.cpp` 或 `NavigationPage.cpp`
5. 现有病例工作区、规划、配准、导航、评估主链行为不回退

## 8. 风险与控制

### 8.1 风险：结构拆分导致服务接线断裂

控制：

- 保持现有 adapter / port 契约不变
- 先提取包装层，再搬迁调用点
- 每完成一个提取单元都要构建和主链回归

### 8.2 风险：过度拆分类太多，反而更难维护

控制：

- 只引入最小协作者集合
- 页面层只拆三类：上下文、服务束、流程协调器
- 禁止为了“看起来架构更高级”而继续细分

### 8.3 风险：VTK 生命周期迁移引入渲染回归

控制：

- 首轮只迁导航页
- 保留原有 `VTKWidgetFactory` 和 `VTKWidgetPool`
- 统一用新 host 做 attach/detach，避免散点修改

## 9. 与创新点开发的关系

这次结构治理不是独立美化工程，而是创新点开发的前置清障：

- 创新点 1 需要稳定的规划数据、推荐策略入口和实验调用点
- 创新点 2 需要把配准算法调用从页面事件中解耦出来
- 创新点 3 需要把导航准入、tracking 质量和评估导出挂到统一流程节点

因此，结构治理完成的标志不是“代码更好看”，而是后续 baseline 和实验闭环能在清晰边界内继续推进。

## 10. 结论

本次结构治理采用双轨方案中的 `Phase A`，核心原则是：

> 只收口主链最痛的三个边界：启动链、导航页、VTK 宿主。  
> 不做泛化平台重构，不追求形式化架构图，而是为后三个创新点留出稳定、可测试、可持续扩展的落点。
