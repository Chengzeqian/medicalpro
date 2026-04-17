# UI Phase 2 Visual Polish Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在不改四页主链与业务流的前提下，完成 Welcome、ModuleSelection、Management、Dashboard 的验收向视觉微调，并同步更新阶段文档。

**Architecture:** 以 `UI/styles/three_pages_theme.qss` 为主收口样式权重，必要时只对四个页面类做少量文案、尺寸和控件属性微调。可测试逻辑继续收口到已有页面类或展示工具，样式变化通过构建、单测和真实启动联合验证。

**Tech Stack:** Qt Widgets、QSS、CMake、CTest

---

### Task 1: 修正文档与实现边界

**Files:**
- Modify: `docs/superpowers/specs/2026-04-15-ui-phase2-visual-polish-design.md`
- Create: `docs/superpowers/plans/2026-04-15-ui-phase2-visual-polish-implementation.md`

- [ ] **Step 1: 修正 visual polish spec 中的乱码与措辞问题**

将 `### 3.4 DashboardPage` 下的 `��页微调目标：` 改为 `本页微调目标：`，保留其余结构不变。

- [ ] **Step 2: 保存本实施计划文件**

确认本计划覆盖四页视觉微调、验证命令和文档同步范围。

- [ ] **Step 3: 自检文档范围是否与 spec 一致**

核对主链仍为 `Welcome -> ModuleSelection -> Management -> Dashboard`，并明确本轮不处理插件链、控制台日志链、跳转链和新增页面。

### Task 2: 收口公共主题样式

**Files:**
- Modify: `UI/styles/three_pages_theme.qss`

- [ ] **Step 1: 调整 WelcomePage 的主次层级**

重点修改以下对象样式：
- `QFrame#brandPanelFrame`
- `QFrame#runtimeSummaryFrame`
- `QPushButton#enterButton`
- `QPushButton#exitButton`
- `QFrame#pluginFrameworkCard`
- `QFrame#serviceStatusCard`
- `QFrame#dataDirectoryCard`

目标：左侧品牌面板更聚焦，右侧摘要卡和下方状态卡降低压屏感，主按钮更强，次按钮更收敛。

- [ ] **Step 2: 调整 ModuleSelectionPage 的主模块与辅模块权重**

重点修改以下对象样式：
- `QFrame#moduleStatusBar`
- `QFrame#moduleHeroFrame`
- `QFrame#ankleSurgeryCard`
- `QFrame#systemSettingsCard`
- `QPushButton#ankleSurgeryButton`
- `QPushButton#systemSettingsButton`

目标：主卡更像继续主流程的入口，系统设置卡退后半级，头部状态条不抢 Hero。

- [ ] **Step 3: 调整 ManagementPage 的中台壳层密度**

重点修改以下对象样式：
- `QFrame#managementHeaderFrame`
- `QFrame#managementContentFrame`
- `QFrame#entityContextFrame`
- `QFrame#managementFlowFrame`
- `QFrame#doctorOverviewCard`
- `QFrame#patientOverviewCard`
- `QFrame#surgeryOverviewCard`
- `QTableWidget`
- `QHeaderView::section`

目标：概览卡更摘要化，上下文条更紧凑，表格更清爽，底部 CTA 更像流程收口。

- [ ] **Step 4: 调整 DashboardPage 的病例工作台节奏**

重点修改以下对象样式：
- `QFrame#dashboardHeaderFrame`
- `QFrame#overviewStripFrame`
- `QFrame#navigationCtaFrame`
- `QFrame#overviewPatientCard`
- `QFrame#overviewDiagnosisCard`
- `QFrame#overviewDoctorCard`
- `QFrame#overviewDicomCard`
- `QPushButton#enterNavigationButton`

目标：顶部信息带更紧凑，病例名与导航动作更突出，概览卡更像辅助摘要。

### Task 3: 做少量页面类微调

**Files:**
- Modify: `UI/NewPages/WelcomePage.cpp`
- Modify: `UI/NewPages/ModuleSelectionPage.cpp`
- Modify: `UI/NewPages/ManagementPage.cpp`
- Modify: `UI/NewPages/DashboardPage.cpp`

- [ ] **Step 1: WelcomePage 微调文案和响应式阈值**

保留现有品牌资产逻辑，只调整必要文案、摘要语气和 `updateResponsiveLayout()` 中按钮/摘要区宽度等参数，让右侧卡片在桌面端更克制。

- [ ] **Step 2: ModuleSelectionPage 微调标题与状态文案**

保持功能不变，只让标题、副标题、模块提示文案更突出“主流程入口”和“系统设置辅助位”。

- [ ] **Step 3: ManagementPage 微调上下文与 CTA 文案**

保持表格与标签页结构不变，只压缩页面 copy 的解释密度，让“进入病例工作台”更像自然下一步。

- [ ] **Step 4: DashboardPage 微调标题节奏与导航 CTA 语气**

保持病例、影像和导航逻辑不变，只增强当前病例和“进入导航”动作的优先级表达。

### Task 4: 做可回归的验证

**Files:**
- Reuse: `tests/unit/ThreePagePresentationUtilsTest.cpp`

- [ ] **Step 1: 评估是否需要补充展示文案相关单测**

如果本轮新增或改动了 `ThreePagePresentationUtils` 的文字/状态拼装逻辑，就先补对应断言；如果仅改 QSS 和页面固定文案，则记录“本轮无新增可单测逻辑”。

- [ ] **Step 2: 构建 Release 目标**

Run: `cmake --build build_x64 --config Release --target medicalpro`
Expected: 构建成功，退出码为 0。

- [ ] **Step 3: 运行现有展示逻辑单测**

Run: `ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure`
Expected: 目标测试通过，输出无失败用例。

- [ ] **Step 4: 真实启动程序做视觉回归**

Run: `build_x64/Release/medicalpro.exe`
Expected: 程序能启动到欢迎页，四页主链可继续走通，无启动即崩溃现象。

### Task 5: 同步当前状态文档

**Files:**
- Modify: `docs/current_status_and_project_overview.md`
- Modify: `docs/build_x64.md`
- Modify: `docs/superpowers/plans/2026-04-14-welcome-page-phase2-implementation.md`
- Modify: `docs/superpowers/plans/2026-04-15-management-page-phase2-implementation.md`
- Modify: `docs/superpowers/plans/2026-04-15-ui-phase2-visual-polish-implementation.md`

- [ ] **Step 1: 更新当前项目状态文档**

记录这轮已进入 UI Phase 2 验收向微调，说明四页主链未变、本轮主要收口样式与文案层级。

- [ ] **Step 2: 更新 build_x64 运行说明**

补充这轮视觉微调后的验证方式，保持“从正确可执行文件启动”的注意事项。

- [ ] **Step 3: 在阶段计划中补充已完成事项和验证结果**

把 QSS 收口、页面轻量微调、构建验证与真实启动结果写入阶段文档。

## 2026-04-15 执行结果

- 已完成文档范围修正：
  - 修复 `docs/superpowers/specs/2026-04-15-ui-phase2-visual-polish-design.md` 中 Dashboard 小节的乱码措辞
- 已完成公共 QSS 第一轮视觉收口：
  - Welcome：强化左品牌区，压低右摘要区与底部状态卡存在感
  - ModuleSelection：主卡与系统设置卡完成明确分级，`systemSettingsButton` 退回辅助按钮层级
  - Management：Header / Context / Overview / Table / CTA 的密度与节奏进一步统一
  - Dashboard：病例概览与导航 CTA 的主次关系进一步清楚
- 已完成页面类轻量微调：
  - `UI/NewPages/WelcomePage.cpp`
  - `UI/NewPages/ModuleSelectionPage.cpp`
  - `UI/NewPages/ManagementPage.cpp`
  - `UI/NewPages/DashboardPage.cpp`
- 已完成验证：

```powershell
cmake --build build_x64 --config Release --target medicalpro
ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure
```

验证结果：
- `medicalpro` Release 构建通过
- `three_page_presentation_utils_test` 通过

补充真实启动验证：

```powershell
$exe = Resolve-Path 'build_x64/Release/medicalpro.exe'
$p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru
Start-Sleep -Seconds 8
```

验证结果：
- 主程序在 8 秒观察窗口内持续运行，未出现立即退出
- 本轮尚未补做截图式人工视觉验收，后续应继续基于真实界面做二轮微调

## 2026-04-16 第二轮执行结果

- 本轮目标已明确收口为“纵向压缩 + 信息聚焦”，不改主链、不碰插件链。
- 已完成 `.ui` 结构层压缩：
  - `UI/Forms/WelcomePage.ui`
  - `UI/Forms/ModuleSelectionPage.ui`
  - `UI/Forms/ManagementPage.ui`
  - `UI/Forms/DashboardPage.ui`
- 已完成页面类轻量微调：
  - `UI/NewPages/WelcomePage.cpp`
  - `UI/NewPages/ModuleSelectionPage.cpp`
  - `UI/NewPages/ManagementPage.cpp`
  - `UI/NewPages/DashboardPage.cpp`
- 已完成公共主题第二轮收口：
  - `UI/styles/three_pages_theme.qss`

本轮实际落地内容：

- WelcomePage
  - 压缩 hero 双栏高度、摘要卡高度、速览块与状态卡厚度
  - 收窄右侧摘要区，减少标题到按钮之间的空白
- ModuleSelectionPage
  - 压缩状态条、Hero、双卡高度与按钮前弹性留白
  - 让主入口按钮明显上提，避免按钮贴底
- ManagementPage
  - 压缩 Header、概览卡、上下文条、Tab、搜索栏、表格行高与底部 CTA
  - 让表格成为更明确的主内容区
- DashboardPage
  - 压缩 Header、概览条、详情分区、DICOM 区与底部 CTA
  - 缩小 DICOM 卡片与缩略图，统一深色工作台节奏

验证记录：

```powershell
cmake --build build_x64 --config Release --target medicalpro
ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure
$exe = Resolve-Path 'build_x64/Release/medicalpro.exe'
$p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru
Start-Sleep -Seconds 8
```

验证结果：

- `medicalpro` Release 构建通过
- `three_page_presentation_utils_test` 继续通过
- 主程序在 8 秒观察窗口内持续运行，未出现立即退出
- 本轮未新增 `ThreePagePresentationUtils` 逻辑，沿用现有展示层单测进行回归

## 2026-04-16 第三轮实施追加

### 本轮目标
- Welcome 主视觉继续放大，减少底部留白。
- ModuleSelection / Dashboard 通过限高和样式收口修正顶部过高问题。
- Management 维持低风险收口，不改功能链路。

### 本轮实施点
1. 调整 `UI/Forms/WelcomePage.ui`
   - 放大 `brandPanelFrame`、`logoLabel`、`enterButton`
   - 去掉 `bottomSpaceSpacer` 的扩张性
   - 限制 `statusCardsFrame` 纵向扩张，让多余高度回到主视觉区
2. 调整 `UI/NewPages/WelcomePage.cpp`
   - 继续拉开 hero 左右比重
   - 收窄 `runtimeSummaryFrame`，放大主按钮可用宽度
   - 放大品牌 logo 缩放尺寸
3. 调整 `UI/Forms/ModuleSelectionPage.ui` 与 `UI/Forms/DashboardPage.ui`
   - 给顶部信息框增加明确 `maximumHeight`
   - 收紧卡片和 CTA 的内边距与最小高度
4. 调整 `UI/NewPages/ManagementPage.cpp` 与 `UI/NewPages/DashboardPage.cpp`
   - 压缩表格、上下文条、DICOM 卡片等运行期尺寸参数
5. 调整 `UI/styles/three_pages_theme.qss`
   - Welcome 主视觉强化
   - ModuleSelection / Dashboard 顶部和 CTA 收紧
   - Management 表头与表格深色统一

### 验证计划
- `cmake --build build_x64 --config Release --target medicalpro`
- `ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure`
- `build_x64/Release/medicalpro.exe` 启动稳定性检查

## 2026-04-16 第三轮执行结果
- 已完成 Welcome 主视觉放大与四页第三轮联动微调：
  - `UI/Forms/WelcomePage.ui`
  - `UI/NewPages/WelcomePage.cpp`
  - `UI/Forms/ModuleSelectionPage.ui`
  - `UI/NewPages/ModuleSelectionPage.cpp`
  - `UI/Forms/DashboardPage.ui`
  - `UI/NewPages/DashboardPage.cpp`
  - `UI/NewPages/ManagementPage.cpp`
  - `UI/styles/three_pages_theme.qss`
- Welcome 本轮重点结果：
  - 主视觉区高度、logo、标题和主按钮同步放大
  - `statusCardsFrame` 被限制为不再吞掉额外纵向空间
  - 首页底部空白改由主视觉承担，而不是落在页面底部
- 其余三页本轮重点结果：
  - ModuleSelection / Dashboard 顶部区块增加限高，整体重心下移
  - Management 继续做低风险密度压缩与深色统一

验证记录：
```powershell
cmake --build build_x64 --config Release --target medicalpro
ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure
$exe = Resolve-Path 'build_x64/Release/medicalpro.exe'
$p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru
Start-Sleep -Seconds 8
```

验证结果：
- `medicalpro` Release 构建通过
- `three_page_presentation_utils_test` 通过
- `medicalpro.exe` 在 8 秒观察窗口内持续运行，随后手动结束进程

## 2026-04-16 第四轮实施计划补充
### 本轮目标
- Welcome：放大并左对齐品牌 logo，收紧英文标题 / 中文标题 / 描述文案 / CTA 的内部间距。
- ModuleSelection：把模块标识与状态标签收为左侧同组，放大卡内标题与说明，修正按钮与内容比例。
- Management：回调概览卡、上下文条、搜索栏、Tab、表头和表格行高，提升纵向占比与可读性。
- Dashboard：本轮保持不动。

### 本轮实施边界
- 不改变 `Welcome -> ModuleSelection -> Management -> Dashboard` 主链。
- 不修改插件加载、服务注册、控制台日志策略。
- 不新增复杂交互，只调 `.ui`、页面参数和公共 `QSS`。

## 2026-04-16 第四轮实施结果
- 已完成 Welcome / ModuleSelection / Management 三页的第四轮人工视觉验收向微调。
- 本轮实际修改文件：
  - `UI/Forms/WelcomePage.ui`
  - `UI/NewPages/WelcomePage.cpp`
  - `UI/Forms/ModuleSelectionPage.ui`
  - `UI/NewPages/ModuleSelectionPage.cpp`
  - `UI/NewPages/ManagementPage.cpp`
  - `UI/styles/three_pages_theme.qss`
- Welcome 实际落地：
  - logo 容器放大并左对齐
  - 英文标题 / 中文标题 / 描述文案 / CTA 收为更紧凑的信息组
  - `进入系统` 按钮继续放大，右侧摘要区继续收窄
- ModuleSelection 实际落地：
  - 模块标识与状态标签改为左侧成组
  - 主卡 / 辅卡标题字号、说明字号、badge 尺寸和按钮比例同步上调
  - 状态文案改为更稳定的短标签：`主链就绪` / `待确认` / `需排查` / `辅助入口` / `建议检查`
- Management 实际落地：
  - 概览卡、上下文条、底部 CTA 边距和间距整体放大
  - 搜索栏、Tab、表头和表格行高回弹
  - Tab 与编辑类按钮文案统一在代码侧明确设置
- Dashboard 本轮未主动修改。

### 本轮验证记录
```powershell
cmake --build build_x64 --config Release --target medicalpro
ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure
$exe = Resolve-Path 'build_x64/Release/medicalpro.exe'
$p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru
Start-Sleep -Seconds 8
```

验证结果：
- `medicalpro` Release 构建通过
- `three_page_presentation_utils_test` 通过
- `medicalpro.exe` 在 8 秒观察窗口内持续运行
- 本轮未新增 `ThreePagePresentationUtils` 逻辑，继续沿用现有展示层回归测试

## 2026-04-16 模块页补充微调结果
- 本轮仅处理 `ModuleSelectionPage` 卡片内部排版，不改四页主链与其他页面。
- 实际修改文件：
  - `UI/Forms/ModuleSelectionPage.ui`
  - `UI/styles/three_pages_theme.qss`
- 实际落地：
  - 将 `ANKLE / 需排查`、`CONFIG / 辅助入口` 两组标签改为更低矮的扁标签比例
  - 同步放大标签字体
  - 继续放大 `踝关节手术工作流` 与 `系统设置与检查`
  - 收紧两行说明文字之间的垂直间距
