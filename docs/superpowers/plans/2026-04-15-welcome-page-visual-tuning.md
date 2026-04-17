# WelcomePage Visual Tuning Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在不改变 WelcomePage 主链、免登录流程和运行摘要能力的前提下，完成一轮面向人工视觉验收的首屏微调，让页面更像“医疗旗舰工作站入口”。

**Architecture:** 这轮只改 `WelcomePage.ui`、`three_pages_theme.qss` 和少量 `WelcomePage.cpp` 的初始展示文案，不碰导航逻辑和运行状态判断。先基于实际运行截图定位问题，再用布局留白、视觉层级、按钮主次和摘要区密度调整收口。

**Tech Stack:** Qt Widgets、Qt Designer `.ui`、Qt Style Sheets、CMake、PowerShell 截图辅助

---

## File Structure

- Modify: `UI/Forms/WelcomePage.ui`
- Modify: `UI/styles/three_pages_theme.qss`
- Modify: `UI/NewPages/WelcomePage.cpp`
- Modify: `docs/current_status_and_project_overview.md`

职责边界：

- `WelcomePage.ui` 只处理版式、尺寸、间距和静态结构。
- `three_pages_theme.qss` 只处理视觉层级、颜色、描边、强调和 hover/focus 反馈。
- `WelcomePage.cpp` 只允许调整首屏默认展示文案，不改运行时判断逻辑。
- `current_status_and_project_overview.md` 只记录这轮视觉验收向微调结论。

### Task 1: 基于实际运行画面确认微调点

**Files:**
- Modify: `docs/current_status_and_project_overview.md`

- [ ] **Step 1: 启动 Release 主程序并停留在 WelcomePage**

Run: `build_x64/Release/medicalpro.exe`

Expected: WelcomePage 作为第一屏显示

- [ ] **Step 2: 抓取 WelcomePage 当前截图，确认层级问题**

Run: 使用 PowerShell 启动程序、抓取窗口截图到临时 PNG

Expected:
- 能看到 WelcomePage 当前真实画面
- 可以明确记录品牌区、摘要区、按钮和状态卡的视觉问题

- [ ] **Step 3: 记录这轮 A 方案的微调目标**

验收目标：
- 左侧品牌区更强，成为首屏唯一主焦点
- 右侧摘要区更克制，更像运行摘要而不是第二主视觉
- 下方三张状态卡统一但降权，不和上方双区��注意力
- `进入系统` 明显高于 `退出`

### Task 2: 调整 WelcomePage 版式和默认展示文案

**Files:**
- Modify: `UI/Forms/WelcomePage.ui`
- Modify: `UI/NewPages/WelcomePage.cpp`

- [ ] **Step 1: 微调 `.ui` 布局参数**

调整方向：
- 主布局外边距回到更稳的桌面工作站节奏
- `brandPanelFrame` 保持更大的纵向呼吸感
- `runtimeSummaryFrame` 略收紧，避免和左侧抢主体
- 下方卡片区与上方主视觉区保留清晰层次断点

- [ ] **Step 2: 调整 WelcomePage 初始文案状态**

调整方向：
- 首屏未刷新前的摘要文案更中性、更像系统准备态
- 默认 badge 文案更像“启动自检中”，避免首屏静态状态过于乐观

- [ ] **Step 3: 编译 WelcomePage 相关目标验证 `.ui` 与 cpp 一致**

Run: `cmake --build --preset x64-release`

Expected: `NewPagesLib` 与 `medicalpro` 构建成功

### Task 3: 调整视觉层级和主次关系

**Files:**
- Modify: `UI/styles/three_pages_theme.qss`

- [ ] **Step 1: 强化左侧品牌区的旗舰入口感**

调整方向：
- 强化品牌区背景层次和边界光感
- 拉开 eyebrow、主标题、副标题的字号和颜色层级
- 给主按钮更明确的高亮、阴影和 hover 反馈

- [ ] **Step 2: 收敛右侧摘要区的信息密度**

调整方向：
- 弱化右侧面板背景对比度
- 让 summary title、summary text、quick stats 呈现清晰主次
- 让 decision badge 更像结论标签，而不是第二 CTA

- [ ] **Step 3: 下调三张状态卡的竞争感**

调整方向：
- 让卡片背景更统一、更轻
- 保留状态 tone，但避免整卡过亮
- 保持信息可读，同时避免和 hero 区争主视觉

- [ ] **Step 4: 重新编译主程序**

Run: `cmake --build --preset x64-release`

Expected: `medicalpro` 构建成功

### Task 4: 二次截图复核并回写文档

**Files:**
- Modify: `docs/current_status_and_project_overview.md`

- [ ] **Step 1: 再次启动并抓取 WelcomePage 截图**

Run: `build_x64/Release/medicalpro.exe`

Expected:
- WelcomePage 仍为第一屏
- 视觉微调已可见

- [ ] **Step 2: 验证主链未回退**

Run: 人工点击 `进入系统`

Expected:
- 仍进入 `ModuleSelectionPage`
- 不出现登录强制页

- [ ] **Step 3: 更新项目总览文档**

写回内容：
- WelcomePage 已完成一轮人工视觉验收向微调
- 本轮主要收口了品牌区层级、摘要区密度、主次按钮关系和状态卡降噪
- 主链和免登录入口保持不变

- [ ] **Step 4: 跑最终构建验证**

Run: `cmake --build --preset x64-release`

Expected: `medicalpro` 继续构建通过
