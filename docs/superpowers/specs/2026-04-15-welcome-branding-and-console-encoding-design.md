# Welcome Branding And Console Encoding Design

日期：2026-04-15

范围：
- `UI/NewPages/WelcomePage.*`
- `UI/Forms/WelcomePage.ui`
- `UI/styles/three_pages_theme.qss`
- `main.cpp`
- `Framework/ConsoleLogBridge.*`
- `Framework/StartupOrchestrator.cpp`
- `Framework/VTKWidgetFactory.cpp`
- `tests/unit/*`
- 相关项目文档

目标：
- 为 `WelcomePage` 接入正式品牌资产方案。
- 修复 Windows 控制台与重定向日志中的中文乱码问题。
- 在终端中文兼容仍不稳定时，优先保证项目自有调试输出可读。
- 保持 Welcome 首屏链路不变，不恢复登录拦截。

## 1. 背景
- Welcome 页面最初只尝试加载运行目录下的 `data/logo.png`。
- 当该资源不存在时，界面会回退显示 `MNS` 占位字样，不符合产品化入口页的视觉要求。
- 主程序已经能弹出控制台窗口，但 Qt 日志仍走默认输出链路。
- 用户终端实测表明，Windows 控制台里的中文 Qt 日志在不同句柄类型、不同终端环境下都可能乱码。

## 2. 设计目标
- Welcome 启动后优先显示正式品牌标识，而不是字母占位。
- 品牌资源既支持运行目录覆盖，也支持仓库内置默认资产。
- Welcome 首屏交互链保持不变：
  - 不跳过 Welcome 页面
  - 不恢复登录拦截
  - `进入系统` 继续跳转到 `ModuleSelectionPage`
- 控制台调试输出在以下场景下都应尽量可读：
  - 直接运行程序时的控制台窗口
  - 标准错误/标准输出重定向到文件

## 3. Welcome 品牌资产方案

### 3.1 资源优先级
Welcome 页面按以下顺序解析品牌资源：
1. `<appDir>/data/branding/welcome_logo.png`
2. `<appDir>/data/logo.png`
3. `:/resoucce/logo.png`

设计意图：
- 前两项用于运行目录覆盖，方便部署后替换品牌资源而不必重新编译。
- 第三项作为仓库内置默认品牌资源，确保默认情况下首页不会退回裸占位。

### 3.2 视觉回退策略
- 若以上路径均不可用，不再显示 `MNS`。
- 改为更正式的文本品牌回退：`MedicalPro`。
- 文本品牌仍放在统一品牌容器中，避免布局塌陷。

### 3.3 布局适配
- 当前内置 `resoucce/logo.png` 更偏横向 wordmark，因此品牌容器不再使用过小的正方形方案。
- 本次调整只影响品牌区表现，不改右侧摘要区和下方状态卡信息结构。

## 4. 控制台日志输出方案

### 4.1 根因判断
- Qt 默认消息处理器在 Windows 下无法保证输出字节与控制台 code page 严格匹配。
- 即使控制台切到 UTF-8，日志字节链路和终端解释链路仍可能不一致。
- 因此仅依赖“切 code page”为 UTF-8 不能稳定解决中文乱码。

### 4.2 基础策略
- 保留现有控制台分配逻辑：
  - `AttachConsole`
  - `AllocConsole`
- 增加自定义 Qt 日志桥 `ConsoleLogBridge`，统一接管：
  - `qDebug`
  - `qInfo`
  - `qWarning`
  - `qCritical`
  - `qFatal`
- 输出路径区分：
  - 控制台句柄优先使用 `WriteConsoleW`
  - 文件重定向使用 UTF-8 `WriteFile`
  - 对 `Windows Terminal / ConPTY` 增加 `CONOUT$` 回退探测

### 4.3 最终调试策略
- 对于项目自有日志，优先保证“立刻可读”，而不是继续追求中文控制台全覆盖。
- 因此本轮最终采用“双层收口”方案：
  - 启动期关键日志统一为英文/ASCII
  - 运行期项目自有模块日志也统一为英文/ASCII
- 该策略只作用于项目自有调试日志，不改变页面 UI 中文文案。

### 4.4 覆盖范围
启动期覆盖：
- `main.cpp`
- `Framework/StartupOrchestrator.cpp`
- `Framework/VTKWidgetFactory.cpp`

运行期模块覆盖：
- `Plugins/UserManagement/UserManagementActivator.cpp`
- `Plugins/UserManagement/UserManagementServiceImpl.cpp`
- `Plugins/Registration2D3D/Registration2D3DServiceImpl.cpp`
- `Plugins/Registration2D3D/Registration2D3DWidget.cpp`
- `Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp`
- `UI/Widgets/Instrument3DPreviewWidget.cpp`
- `Plugins/OpticalRegistration/widgets/OpticalRegistrationWidget.cpp`
- `Plugins/OpticalRegistration/internal/OpticalRegistrationVTKWidget.cpp`

## 5. 测试策略

### 5.1 Welcome 品牌与 ConsoleLogBridge
- 验证品牌路径优先级解析正确。
- 验证默认文本品牌回退为 `MedicalPro`。
- 验证日志格式化与重定向 UTF-8 输出行为。

### 5.2 启动期日志策略
- 用 `startup_console_log_policy_test` 回归检查启动标题、阶段名、诊断摘要模板。

### 5.3 运行期日志策略
- 用 `runtime_console_log_policy_test` 回归检查目标源码中的运行期日志调用行。
- 锁定关键 marker，避免后续又回到中文控制台输出。

## 6. 非目标
- 不修改 Welcome 到 ModuleSelection 的导航链。
- 不恢复登录拦截。
- 不引入新的 UI 框架。
- 不系统性清洗所有历史中文注释或 UI 文案。
- 不强行改写第三方 CTK / VTK / Python 原生日志。

## 7. 结果判断标准
- Welcome 首页不再显示 `MNS` 占位标识。
- 项目自有启动期控制台日志可直接英文读取。
- 项目自有运行期模块日志也保持同样的英文可读策略。
- 终端里若仍有中文或其他风格日志，优先判定为第三方输出，不影响本轮目标达成。
