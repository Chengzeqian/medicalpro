# Repo Hygiene Baseline

日期：2026-04-17

## 1. 基线现状

- tracked files: 114
- untracked files: 75598
- deleted tracked files: 91

当前仓库不是“开发中有一些零散改动”，而是 Git 基线和真实工作目录已经严重脱节。直接在这个状态上继续做平台治理开发，风险主要有三类：

- 真正的源码修改会被本地缓存、实验目录和 SDK 目录淹没
- 版本回顾时很难区分“应纳入版本管理的代码”与“本地环境依赖”
- worktree、分支切换、回归验证都容易落在不完整基线上

## 2. 未跟踪目录主来源

按顶级目录统计，未跟踪文件主要集中在：

| 目录 | 规模判断 | 说明 |
| --- | --- | --- |
| `ICPtry/` | 极大 | 本地实验、资料、SDK 内容，优先排除出版本管理视野 |
| `Python39/` | 极大 | 本地 Python 运行时，不应进入项目版本库 |
| `patient_data/` | 中等 | 本地业务数据，需和源码边界隔离 |
| `docs/` | 中等 | 存在真实项目文档，应继续保留并整理 |
| `Plugins/` | 中等 | 存在真实源码，应逐步纳入版本管理 |
| `UI/` | 较小 | 存在真实源码，应逐步纳入版本管理 |
| `Framework/` | 较小 | 存在真实源码，应逐步纳入版本管理 |
| `.superpowers/` | 小 | 本地工具状态目录，应忽略 |
| `.kiro/` | 小 | 本地工具状态目录，应忽略 |
| `.vscode/` | 小 | 本地 IDE 配置，应忽略 |

## 3. 已跟踪删除项主来源

当前已跟踪删除项主要集中在旧插件与历史文档：

- `Plugins/ImageInteraction/`
- `Plugins/MedicalImageCore/`
- `Plugins/MedicalProcessing/`
- `Plugins/MedicalViewer/`
- `Plugins/PatientManagement/`
- 若干旧 CTK 教程和说明 markdown

这说明仓库里已经发生了一轮真实的结构迁移，但 Git 基线还没有被正式收口。

## 4. 当前保留边界

下列路径应作为“项目真实源码 / 配置 / 文档”的保留边界：

- `Framework/`
- `Plugins/`
- `UI/`
- `tests/`
- `cmake/`
- `config/`
- `docs/`
- `data/`
- `main.cpp`
- `resources.qrc`
- `CMakeLists.txt`
- `CMakePresets.json`

## 5. 当前忽略边界

下列路径应作为“本地环境 / 实验 / 缓存 / 归档”的忽略边界：

- `.worktrees/`
- `.superpowers/`
- `.kiro/`
- `.vscode/`
- `build/`
- `build_x64/`
- `segmentation_outputs/`
- `Python39/`
- `ICPtry/`
- `Regi/`
- `ThirdParty.zip`

## 6. 整理顺序

建议按以下顺序治理仓库基线：

1. 先用 `.gitignore` 把本地环境噪音移出 Git 视野
2. 再确认真实源码目录哪些必须纳入版本管理
3. 再处理旧插件删除项，形成一次明确的“结构迁移收口”提交
4. 最后再恢复平台治理开发

## 7. 当前结论

平台治理开发应该继续，但不应直接开始。正确顺序是：

1. 先做仓库基线治理
2. 再恢复平台治理 Task 1 起步
3. 后续所有平台改造都基于整理后的干净基线推进
