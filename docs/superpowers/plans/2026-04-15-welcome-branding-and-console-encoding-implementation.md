# Welcome Branding And Console Encoding Implementation Plan

更新时间：2026-04-15

## 1. 目标
- 为 `WelcomePage` 接入正式品牌资产方案，替换 `MNS` 占位标识。
- 为主程序接入 `ConsoleLogBridge`，统一 Qt 日志桥接行为。
- 修复 Windows 控制台与重定向日志场景中的中文乱码问题。
- 当中文控制台兼容仍不稳定时，优先保证调试可读性，把项目自有关键日志统一收口为英文/ASCII。

## 2. 本轮已完成实施

### 2.1 Welcome 品牌资产接入
- 已新增：
  - `UI/NewPages/WelcomeBrandingUtils.h`
  - `UI/NewPages/WelcomeBrandingUtils.cpp`
- 已将 Welcome 品牌资源解析顺序固定为：
  1. `<appDir>/data/branding/welcome_logo.png`
  2. `<appDir>/data/logo.png`
  3. `:/resoucce/logo.png`
- 当运行目录与资源都不可用时，Welcome 页面会回退到正式文本品牌 `MedicalPro`，不再显示 `MNS`。
- Welcome 品牌容器已调整为更适合横向 wordmark 的尺寸，适配当前品牌资源。

### 2.2 控制台日志桥接
- 已新增：
  - `Framework/ConsoleLogBridge.h`
  - `Framework/ConsoleLogBridge.cpp`
- `main.cpp` 已在首条 Qt 日志输出前安装自定义 Qt message handler。
- 当前输出策略为：
  - 控制台句柄优先走 `WriteConsoleW`
  - 文件重定向走 UTF-8 `WriteFile`
  - 对 Windows Terminal / ConPTY 场景补充 `CONOUT$` 检测与回退
- 为兼容第三方窄字符日志，控制台 code page 已回到系统 ANSI；项目自有 Qt 日志继续走宽字符输出。

### 2.3 启动期日志英文化收口
- 已将以下启动链关键日志统一为英文/ASCII：
  - `main.cpp`
  - `Framework/StartupOrchestrator.cpp`
  - `Framework/VTKWidgetFactory.cpp`
- 已新增 `startup_console_log_policy_test`，锁定启动标题、阶段名和诊断摘要模板保持英文可读。

### 2.4 运行期模块日志统一
- 继续沿同一策略，把项目自有运行期模块日志统一为英文/ASCII，覆盖：
  - `Plugins/UserManagement/UserManagementActivator.cpp`
  - `Plugins/UserManagement/UserManagementServiceImpl.cpp`
  - `Plugins/Registration2D3D/Registration2D3DServiceImpl.cpp`
  - `Plugins/Registration2D3D/Registration2D3DWidget.cpp`
  - `Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp`
  - `UI/Widgets/Instrument3DPreviewWidget.cpp`
  - `Plugins/OpticalRegistration/widgets/OpticalRegistrationWidget.cpp`
  - `Plugins/OpticalRegistration/internal/OpticalRegistrationVTKWidget.cpp`
- 本轮只收口项目自有运行期日志，不改第三方库原生日志，不改页面中文 UI 文案。

## 3. 测试与验证

### 3.1 Welcome / ConsoleLogBridge
已通过：

```powershell
cmake --build --preset x64-release --config Release
ctest --test-dir build_x64 -C Release --output-on-failure
```

覆盖结果：
- `branding_and_console_utils_test`
- `startup_console_log_policy_test`

补充验证：

```powershell
Start-Process build_x64/Release/medicalpro.exe -WorkingDirectory build_x64/Release -RedirectStandardError <temp-log>
```

验证重点：
- Welcome 首屏优先显示正式品牌资产或正式文本品牌回退
- 重定向日志文件可按 UTF-8 正确读取
- 启动期关键日志已转为英文可读文本

### 3.2 运行期模块日志统一
已通过：

```powershell
ctest --test-dir build_x64/tests/unit -C Release -R runtime_console_log_policy_test -VV
```

覆盖结果：
- 目标源码中所有纳入范围的运行期日志调用行已通过 ASCII 检查
- 关键 marker 已锁定，避免后续回退到中文控制台日志

### 3.3 最新真实启动验证
已通过：

```powershell
Start-Process build_x64/Release/medicalpro.exe -WorkingDirectory build_x64/Release -RedirectStandardOutput build_x64/Release/logs/medicalpro_stdout_20260415.log -RedirectStandardError build_x64/Release/logs/medicalpro_stderr_20260415.log
```

验证结论：
- 项目自有启动链日志已在真实程序运行中保持英文可读。
- 第三方 CTK 日志仍按原始格式输出，这符合本轮边界。
- 同一次验证中仍发现残留插件问题：
  - `Plugin handle not found for UserManagement`
  - `Critical plugin start failed: "UserManagement"`
- 该问题不属于日志编码回退，而是后续需要单独排查的插件安装/发现链问题。

## 4. 关键决策
- 不跳过 Welcome 页面。
- 不恢复登录拦截。
- 不继续扩大 Windows 控制台中文兼容黑科技范围。
- 当中文控制台显示不稳定时，以“项目自有调试日志英文可读”为第一优先级。
- 第三方日志保持原样，后续如需处理，单独立项。

## 5. 当前结果
- Welcome 页面已具备正式品牌资产方案。
- 项目自有启动期 Qt 日志已统一为英文/ASCII。
- 项目自有运行期模块日志已统一为英文/ASCII。
- 控制台现在更适合拿来观察插件加载、服务初始化、运行期模块行为与调试问题。
- 最新真实启动验证还暴露出 `UserManagement` 插件激活问题，后续应优先处理插件加载链，而不是日志链。

## 6. 后续建议
1. 先排查 `UserManagement` 为何没有进入已安装插件集合。
2. 再用 `build_x64/Release/medicalpro.exe` 做一次人工运行验证，重点观察 Welcome 进入系统后几个核心模块的控制台输出。
3. 后续新增模块如果需要控制台调试输出，默认沿用英文/ASCII 策略，避免再次引入中文乱码回归。
