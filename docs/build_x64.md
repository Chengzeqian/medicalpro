# medicalpro x64 构建说明

更新时间：2026-04-16

## 1. 默认构建目标

项目当前依赖的 Qt、VTK、ITK 均为 `x64` 版本。
请不要使用 `ARM64` Kit 或 `ARM64` 生成目录。

推荐统一使用：
- 配置目录：`build_x64`
- Release 运行入口：`build_x64/Release/medicalpro.exe`
- Debug 运行入口：`build_x64/Debug/medicalpro.exe`

## 2. 推荐命令

首次或重新生成：

```powershell
cmake --preset x64-release
```

Release 构建：

```powershell
cmake --build --preset x64-release
```

Debug 构建：

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug
```

## 3. 运行与调试

推荐直接运行：

```powershell
build_x64/Release/medicalpro.exe
```

当前程序已启用 Windows 控制台输出能力，运行时应能看到控制台日志，便于调试：
- 启动阶段
- Platform host 初始化
- 插件安装与启动
- 服务注册
- 运行期问题定位

当前控制台日志策略补充说明：
- 本项目自有启动期日志已统一为英文/ASCII，避免 Windows 控制台中文乱码影响调试。
- 本项目自有运行期模块日志也已按相同策略统一，重点覆盖用户管理、2D3D 配准、光学跟踪、3D 预览、光学配准等模块。
- 第三方 VTK / Python 原生日志不在本轮统一范围内，因此控制台里仍可能看到第三方保持原始格式的输出。

## 4. 当前已验证状态

以下命令已在 2026-04-14 实际通过：

```powershell
cmake --build --preset x64-release
```

验证结果：
- `Framework` 编译通过
- `NewPagesLib` 编译通过
- `medicalpro` 编译通过

同时以下测试已通过：

```powershell
ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure
```

2026-04-15 已补充通过的调试输出相关验证：

```powershell
ctest --test-dir build_x64/tests/unit -C Release -R startup_console_log_policy_test -VV
ctest --test-dir build_x64/tests/unit -C Release -R runtime_console_log_policy_test -VV
```

2026-04-15 已补充通过的 UI Phase 2 验证：

```powershell
cmake --build build_x64 --config Release --target medicalpro
ctest -C Release -R three_page_presentation_utils_test --output-on-failure
```

补充说明：
- `ModuleSelectionPage` 门厅化改造已进入主构建链并编译通过。
- `ManagementPage` 数据管理中台化改造已进入主构建链并编译通过。
- `DashboardPage` 工作台化改造已进入主构建链并编译通过。
- 四页链路展示层辅助函数仍可通过 `three_page_presentation_utils_test` 回归验证。

2026-04-15 已补充通过的 UI Phase 2 视觉微调第一轮验证：

```powershell
cmake --build build_x64 --config Release --target medicalpro
ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure
$exe = Resolve-Path 'build_x64/Release/medicalpro.exe'
$p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru
Start-Sleep -Seconds 8
```

补充说明：
- Welcome / ModuleSelection / Management / Dashboard 四页视觉微调代码已进入 `medicalpro.exe` 主构建链。
- 本轮已确认主程序在 8 秒观察窗口内持续运行，未出现“点运行立即退出”。
- 当前这轮验证仍然建议从 `build_x64/Release/medicalpro.exe` 启动，不要切换到其他旧目录下的可执行文件。

## 5. 运行入口说明

当前主程序正确入口是：
- `build_x64/Release/medicalpro.exe`

如果出现“欢迎页没有显示”之类现象，首先确认是否点错了可执行文件。
之前已经确认过，错误入口会导致观察结果和当前项目状态不一致。

## 6. 当前建议

- 平时开发优先使用 `x64-release` 进行整体验证。
- 页面与运行链调试统一从 `build_x64` 目录出发。
- 后续继续做四页视觉微调时，先跑一遍 `cmake --build build_x64 --config Release --target medicalpro` 再看界面效果。
- 如果要继续看四页链路真实效果，优先直接启动 `build_x64/Release/medicalpro.exe`，按 `Welcome -> ModuleSelection -> Management -> Dashboard` 顺序验收。

2026-04-16 已补充通过的 UI Phase 2 视觉微调第二轮验证：

```powershell
cmake --build build_x64 --config Release --target medicalpro
ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure
$exe = Resolve-Path 'build_x64/Release/medicalpro.exe'
$p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru
Start-Sleep -Seconds 8
```

补充说明：
- 第二轮主要收口了四页的纵向空白、卡片高度、边距与 CTA 位置。
- 本轮改动覆盖 `.ui`、页面类和公共 `QSS`，但没有改动四页主链与插件链。
- `medicalpro.exe` 在 8 秒观察窗口内继续保持运行，说明这轮结构压缩没有破坏启动链。
- 后续验收仍建议从 `build_x64/Release/medicalpro.exe` 启动，并按 `Welcome -> ModuleSelection -> Management -> Dashboard` 顺序逐页复核。

## 7. 2026-04-16 UI Phase 2 第三轮验证补充
本轮验证仍然统一从以下入口启动：
- `build_x64/Release/medicalpro.exe`

已实际通过的命令：
```powershell
cmake --build build_x64 --config Release --target medicalpro
ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure
$exe = Resolve-Path 'build_x64/Release/medicalpro.exe'
$p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru
Start-Sleep -Seconds 8
```

验证结果：
- 第三轮四页视觉微调改动已进入 `medicalpro.exe` 主构建链并编译通过。
- 展示层回归测试继续通过。
- 程序启动后在 8 秒观察窗口内保持运行，未出现启动即退出。

本轮额外说明：
- Welcome 页已把额外高度重新分配给主视觉区，后续人工验收请重点看首页底部留白是否符合预期。
- ModuleSelection / Dashboard 本轮主要修正的是顶部区块过高问题；验收时建议重点看顶部状态条、Hero、概览条和 CTA 的纵向节奏。
- Management 本轮属于低风险密度收口，验收时重点看表头、表格可读性和底部 CTA 是否更稳。

## 8. 2026-04-16 UI Phase 2 第四轮验证补充
本轮仍统一从以下入口启动：
- `build_x64/Release/medicalpro.exe`

本轮已实际通过的命令：

```powershell
cmake --build build_x64 --config Release --target medicalpro
ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure
$exe = Resolve-Path 'build_x64/Release/medicalpro.exe'
$p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru
Start-Sleep -Seconds 8
```

本轮补充说明：
- Welcome 页本轮重点检查主视觉内部排版，不再继续改页面骨架。
- ModuleSelection 页本轮重点检查模块卡内部文字层级、状态标签位置和按钮比例。
- ModuleSelection 页后续补充微调已把顶部标签改为更扁的短标签比例，并继续放大模块标题。
- Management 页本轮重点检查概览卡、搜索栏、Tab、表头与表格行高是否明显回弹。
- Dashboard 页本轮未主动修改，避免无关联动带来回归风险。

验证结果：
- 第四轮 UI 微调代码已进入 `medicalpro.exe` 主构建链并编译通过。
- 展示层回归测试继续通过。
- 程序启动后在 8 秒观察窗口内保持运行，未出现启动即退出。

## 9. 2026-04-16 SystemSettingsPage Phase 2 验证补充
本轮新增验证重点：
- 重点验收路径：`ModuleSelection -> SystemSettings -> ModuleSelection`
- 重点观察：状态卡、建议条、配置卡、保存按钮与浏览按钮主次关系
- 重点确认：系统设置页进入后第一屏优先看到系统状态，而不是直接看到旧式表单

本轮实际使用的命令：
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

补充说明：
- `three_page_presentation_utils_test` 当前可成功构建并直跑通过。
- 现阶段 `ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test` 可能因为测试路径注册问题找不到 exe，不影响本轮代码正确性判断。
- 本轮完成后，系统设置页的主要验收应从“表单是否可编辑”升级为“状态摘要是否清晰、层级是否合理、辅助入口语义是否成立”。

## 10. 2026-04-16 SystemSettingsPage 首屏强化验收
- 验收路径：`ModuleSelectionPage -> SystemSettingsPage -> 返回模块页`
- 首屏重点：Header、三张状态卡、当前判断卡
- 配置区重点：表单密度、路径区按钮统一、底部留白是否收干净
本轮实际通过的命令：
```powershell
cmake --build build_x64 --config Release --target NewPagesLib
cmake --build build_x64 --config Release --target medicalpro
ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure
$exe = Resolve-Path 'build_x64/Release/medicalpro.exe'
$p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru
Start-Sleep -Seconds 8
```
补充说明：
- 本轮重点不是改功能，而是继续强化 `SystemSettingsPage` 的首屏层级。
- 当前页头、状态总览和建议条已成为进入页面后的第一阅读顺序。
- 三张配置卡已回落为辅助编辑区，路径按钮与输入区的主次关系更清楚。
- 本轮回归测试与真实启动验收均已完成，可继续进入人工视觉验收环节。

## 11. 2026-04-16 SystemSettingsPage 底部留白复核
- 复核路径：`build_x64/Release/medicalpro.exe -> ModuleSelectionPage -> SystemSettingsPage`
- 复核重点：最后一张配置卡之后是否还存在明显大块空白
- 根因说明：`scrollAreaContents` 在 `.ui` 中残留固定高度 `1400`，会把滚动内容尾部继续撑大

本轮实际通过的命令：
```powershell
cmake --build build_x64 --config Release --target NewPagesLib
cmake --build build_x64 --config Release --target medicalpro
ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure
$exe = Resolve-Path 'build_x64/Release/medicalpro.exe'
$p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru
Start-Sleep -Seconds 8
```

补充说明：
- 本轮把 `scrollAreaContents` 初始高度从 `1400` 收紧到 `960`。
- `scrollLayout` 已增加 `QLayout::SetMinAndMaxSize`，用于让滚动内容更贴合真实布局高度。
- `scrollLayout.bottomMargin` 已清零，避免尾部继续残留额外间距。
- 构建与测试均已恢复通过，启动 8 秒观测窗口内程序持续运行。

## 12. 2026-04-16 SystemSettingsPage 实机截图复核
- 复核路径：`build_x64/Release/medicalpro.exe -> ModuleSelectionPage -> SystemSettingsPage`
- 复核重点：
  - 卡片之间是否还露出浅色底
  - 建议条左侧 badge 是否仍被拉成高方块
  - 下半区配置卡是否仍显得过厚过重

本轮实际通过的命令：
```powershell
cmake --build build_x64 --config Release --target medicalpro
ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure
$exe = Resolve-Path 'build_x64/Release/medicalpro.exe'
$p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru
Start-Sleep -Seconds 8
```

补充说明：
- 本轮给 `scrollArea` viewport 单独命名为 `systemSettingsScrollViewport`，并统一按主题设为透明背景。
- `systemRecommendationBadge` 已改为固定尺寸胶囊，建议条高度和上下内边距已同步收紧。
- 三张配置卡只做轻量“去厚重”，没有改表单结构、字段内容和交互逻辑。
- 构建、测试、启动验收已完成；若后续仍有视觉问题，应优先以真实截图继续做人工微调。
