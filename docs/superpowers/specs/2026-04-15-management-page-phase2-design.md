# ManagementPage Phase 2 Design

**日期：** 2026-04-15  
**范围：** `UI/NewPages/ManagementPage.*`、`UI/Forms/ManagementPage.ui`、`UI/styles/three_pages_theme.qss`、少量 `ModuleSelectionPage` / `DashboardPage` 文案衔接  
**目标：** 在不改变现有主业务顺序、不碰插件链、不推翻现有表格与 CRUD 数据逻辑的前提下，把 `ManagementPage` 从旧版后台表格页升级为四页主链中的正式“数据管理中台页”，并让 `Welcome -> ModuleSelection -> Management -> Dashboard` 形成统一的产品化工作流。

## 1. 设计目标

- 明确 `ManagementPage` 在当前系统中的角色，不再把它视为可忽略的中间过渡页。
- 让 `ManagementPage` 与已完成 Phase 2 的 `WelcomePage`、`ModuleSelectionPage`、`DashboardPage` 形成统一视觉语言。
- 保留现有医生 / 患者 / 手术三类管理对象与现有表格、搜索、CRUD 交互。
- 保留当前页面跳转链路：
  - `Welcome -> ModuleSelection`
  - `ModuleSelection -> Management / SystemSettings`
  - `Management -> Dashboard`
  - `Dashboard -> Management / Navigation / Welcome`
- 不改变现有业务数据来源和插件服务调用关系。

## 2. 页面定位

`ManagementPage` 在 Phase 2 中的角色定义为：

- 它是“数据管理中台”，不是单纯的后台 CRUD 页面。
- 它承接 `ModuleSelectionPage` 的“进入手术主链”动作。
- 它负责在进入 `DashboardPage` 之前，先让用户完成基础数据查看、实体切换和当前管理步骤确认。
- 它的使命不是替代 `DashboardPage`，而是为 `DashboardPage` 提供上游上下文。

一句话定义：

> `ManagementPage` 是从模块门厅进入病例工作台之前的正式中间站，用来完成数据管理、对象切换和进入下一步的流程收口。

## 3. 四页主链修正

此前三页 Phase 2 的设计默认主链更接近：

`Welcome -> ModuleSelection -> Dashboard`

但根据真实运行页面与现有跳转代码，当前正确主链应修正为：

`Welcome -> ModuleSelection -> Management -> Dashboard`

因此本轮设计不是孤立改 `ManagementPage`，而是对四页链路做统一：

- `WelcomePage`：系统正式入口
- `ModuleSelectionPage`：模块门厅
- `ManagementPage`：数据管理中台
- `DashboardPage`：病例工作台

## 4. 整体视觉方向

延续现有三页 Phase 2 视觉基调：

- 深蓝医疗工作站背景
- 深色实体面板
- 冷蓝高光 + 橙色主 CTA
- 明确的标题层级与卡片结构
- 更像专业工作站，而不是普通管理后台

`ManagementPage` 的目标气质：

- 比旧版更稳、更专业
- 比 `DashboardPage` 更偏“中台”
- 比 `ModuleSelectionPage` 更偏“执行层”

## 5. 页面结构

`ManagementPage` Phase 2 采用四层结构：

1. 顶部头部区
2. 概览卡区
3. 实体切换与内容区
4. 进入病例工作台的流程收口区

### 5.1 顶部头部区

顶部头部区负责说明当前页面在整条主链中的位置，包含：

- 返回模块页按钮
- 页面标题：`数据管理中台`
- 一句副标题，说明当前页面作用
- `进入病例工作台` 主按钮

建议文案：

- 眉标：`DATA OPERATIONS HUB`
- 主标题：`数据管理中台`
- 副标题：`先完成基础数据查看与对象切换，再进入病例工作台继续当前流程。`

### 5.2 概览卡区

头部之下新增 3 张概览卡，用于把“当前能管理什么”先讲清楚，而不是让用户一进来只看到表格。

建议卡片：

- `医生数据`
- `患者数据`
- `手术任务`

每张卡只放轻量信息：

- 当前数量或占位摘要
- 一句说明
- 可选状态标签

这三张卡不承载复杂点击逻辑，重点是建立页面层级和中台感。

### 5.3 实体切换与内容区

内容主区保持“医生 / 患者 / 手术”三种实体，不推翻现有 `QTabWidget` 思路，但会重新组织为更像中台页的结构。

建议布局：

- 左侧窄列：
  - 当前实体说明
  - 搜索框
  - 当前列表用途提示
  - 可选快捷说明
- 右侧主区：
  - 当前实体的主操作按钮组
  - 数据表格

这意味着实现时可以有两种方式：

- 继续保留 `QTabWidget`，但增强其样式与内容容器结构
- 或用自定义切换按钮替换 Tab 外观，但底层仍复用现有三套表格逻辑

本轮推荐保留 `QTabWidget`，因为风险更低，也更适合快速落地。

### 5.4 流程收口区

页面底部或右上角要继续保留并强化“进入病例工作台”的动作语义。

它不是普通跳转按钮，而是这页的主 CTA。

收口区职责：

- 告诉用户“当前数据管理完成后，下一步去哪”
- 明确把后续工作引导到 `DashboardPage`

建议文案：

- 标题：`下一步：进入病例工作台`
- 说明：`在数据管理中确认当前对象后，可进入病例工作台继续病例与影像流程。`

## 6. 保留与重做边界

### 6.1 保留内容

- 三类实体：医生 / 患者 / 手术
- 现有搜索入口
- 现有 CRUD 按钮逻辑
- 现有表格控件
- `enterMainSystemRequested` 跳转信号
- 当前数据加载入口函数：
  - `loadDoctors()`
  - `loadPatients()`
  - `loadSurgeries()`

### 6.2 重做内容

- `ManagementPage.ui` 页面骨架
- 页面头部层级
- 标签页外观与工作区结构
- 页面文案体系
- 与 `ModuleSelectionPage`、`DashboardPage` 的链路语义衔接
- 公共 QSS 中对 `ManagementPage` 的样式覆盖

### 6.3 不在本轮处理的内容

- 不改插件链与 CTK 服务加载问题
- 不改业务数据模型
- 不做 QML 化
- 不做复杂拖拽 / 停靠式中台
- 不把 `ManagementPage` 做成和 `DashboardPage` 重叠的“超级总控台”

## 7. 与前后页面的联动调整

### 7.1 ModuleSelectionPage

`ModuleSelectionPage` 当前已经是门厅化页面，但文案上还要更明确地指向数据管理中台，而不是给人“直接进入病例工作台”的感觉。

需要微调的地方：

- `ankleSurgeryCard` 的说明文案
- CTA 按钮附近的提示文案
- 系统主链描述

目标表达：

> 从模块门厅进入数据管理主链，而不是直接跳进病例工作台。

### 7.2 DashboardPage

`DashboardPage` 已完成工作台化改造，但它的头部文案和返回动作要更清楚地体现：

- 它是 `ManagementPage` 之后的病例工作台
- 返回时返回的是 `数据管理中台`

需要微调的地方：

- 头部副标题
- 返回按钮和辅助说明
- CTA 提示文案中的前后步骤语义

## 8. 技术实现原则

- 保持 `Qt Widgets + .ui + NewPages` 架构不变
- 优先通过 `.ui` 结构重组 + 公共 QSS 覆盖完成视觉升级
- 尽量不把大量样式继续堆在 `ManagementPage.ui` 内联 `styleSheet`
- 用现有 `three_pages_theme.qss` 扩展为四页统一主题，而不是为 `ManagementPage` 再写一套割裂样式
- 如果需要概览卡摘要文案，优先在 `ManagementPage.cpp` 中直接计算，不额外引入高风险结构

## 9. 文档影响

本轮设计确认后，需要同步更新：

- `docs/current_status_and_project_overview.md`
- `docs/build_x64.md`（如有构建验证变化）
- `docs/superpowers/plans/2026-04-14-ui-phase2-three-pages-implementation.md`

并补充说明：

- 三页链路已修正为四页链路统一
- `ManagementPage` 已纳入 UI Phase 2 正式范围

## 10. 验收标准

- `ManagementPage` 不再像独立后台页，而是明显属于同一套医疗工作站产品
- 用户能清楚理解当前链路：
  - Welcome
  - ModuleSelection
  - Management
  - Dashboard
- 页面仍然保留医生 / 患者 / 手术三类管理逻辑
- 页面主 CTA 明确引导到病例工作台
- 不破坏现有表格数据加载与跳转信号
- 不破坏当前主程序编译与运行链

## 11. 实施顺序

1. 重写 `ManagementPage.ui` 结构并去掉旧版内联大段样式
2. 扩展 `ManagementPage.cpp`，加入概览卡、头部说明和流程收口逻辑
3. 在公共 QSS 中补 `ManagementPage` Phase 2 样式
4. 微调 `ModuleSelectionPage` 和 `DashboardPage` 的链路文案
5. 更新项目现状与 UI Phase 2 文档
6. 构建验证并做一轮真实运行视觉验收
