# medicalpro 模块目录规划

## 1. 文档目标

本文档用于规划 `D:\Qtproject\medicalpro` 作为毕业版手术导航系统主工程时的模块目录结构。

规划目标不是做一个过度复杂的平台，而是让 `medicalpro` 能稳定承担以下职责：

- 作为系统唯一主入口
- 串联 `ProbeCalibration`、`MeshGPU`、`NavGUI`
- 提供完整导航流程
- 提供实验记录和误差输出
- 为论文和答辩提供稳定演示版本

## 2. 规划原则

- 目录按职责拆分，不按页面零散堆积
- 算法、流程、界面、设备、数据分层
- 旧项目先包起来接入，不一开始就全部重写
- 优先最小闭环，后续再逐步细化
- 所有核心模块围绕“标定 -> 配准 -> 导航 -> 评估”组织

## 3. 推荐目录结构

建议 `medicalpro` 最终主目录采用如下结构：

```text
medicalpro/
├─ CMakeLists.txt
├─ README.md
├─ docs/
├─ scripts/
├─ resources/
├─ config/
├─ data/
├─ external/
├─ src/
│  ├─ app/
│  ├─ bootstrap/
│  ├─ common/
│  ├─ domain/
│  ├─ modules/
│  ├─ services/
│  ├─ adapters/
│  ├─ workflow/
│  ├─ ui/
│  └─ infrastructure/
├─ tests/
│  ├─ unit/
│  ├─ integration/
│  └─ demo/
└─ build/
```

下面按目录说明。

## 4. 顶层目录职责

### `docs/`

用于放设计文档、实验方案、接口文档、坐标系说明、答辩截图说明。

建议包含：

- 系统架构图
- 坐标变换关系图
- 接口说明
- 实验记录模板

### `scripts/`

用于放构建辅助、数据转换、实验数据整理脚本。

不要把核心业务逻辑写在这里，这里只放辅助脚本。

### `resources/`

用于放 Qt 资源文件和系统内嵌资源。

建议包含：

- 图标
- 默认模型
- 示例数据
- 默认样式
- Qt `.qrc`

### `config/`

用于放系统配置文件。

建议包含：

- 设备连接配置
- 模块参数配置
- 默认路径配置
- 实验配置模板

### `data/`

用于放本地测试数据和运行期导出数据。

建议再分：

```text
data/
├─ sample_models/
├─ sample_pointclouds/
├─ calibration_results/
├─ registration_results/
├─ navigation_logs/
└─ experiments/
```

### `external/`

用于管理外部依赖工程或三方库封装。

这里很适合放：

- `ProbeCalibration` 的外部接入层
- `MeshGPU` 的外部接入层
- 其他第三方算法库

如果这两个项目不直接拷入 `medicalpro`，也可以只在这里放桥接代码或子模块说明。

## 5. `src/` 目录规划

`src/` 是主工程核心，建议拆成 9 层。

## 5.1 `src/app/`

这是应用入口层。

建议放：

- `main.cpp`
- `application_context.*`
- `application_initializer.*`
- `app_routes.*`

职责：

- 启动 Qt 应用
- 初始化全局资源
- 注册模块
- 创建主窗口

这里不要放具体业务逻辑。

## 5.2 `src/bootstrap/`

这是系统启动装配层。

建议放：

- 模块注册
- 服务实例装配
- 配置加载
- 日志初始化

可包含：

```text
src/bootstrap/
├─ register_modules.*
├─ init_logging.*
├─ init_config.*
└─ init_resources.*
```

职责：

- 把 `services`、`adapters`、`modules` 连接起来
- 控制应用启动顺序

## 5.3 `src/common/`

这是共享基础层。

建议放：

```text
src/common/
├─ types/
├─ math/
├─ utils/
├─ constants/
├─ logging/
└─ error/
```

职责：

- 通用数据结构
- 矩阵和坐标相关基础类型
- 日志工具
- 错误码定义
- 时间、文件、路径等通用工具

这里尤其要统一以下核心类型：

- 位姿类型
- 4x4 变换矩阵类型
- 点云点类型
- 标定结果类型
- 配准结果类型
- 导航状态类型

## 5.4 `src/domain/`

这是领域模型层，用来表达“手术导航系统是什么”。

建议放：

```text
src/domain/
├─ entities/
├─ value_objects/
└─ enums/
```

建议领域对象至少包含：

- `probe_info`
- `calibration_result`
- `registration_result`
- `navigation_session`
- `target_info`
- `patient_case`
- `tracking_frame`

职责：

- 定义稳定的数据模型
- 定义模块之间传递的数据对象

这样可以避免 UI 层和算法层直接互相传裸数据。

## 5.5 `src/modules/`

这是业务模块层，也是整个工程最重要的目录。

建议拆成：

```text
src/modules/
├─ model_management/
├─ probe_calibration/
├─ registration/
├─ tracking/
├─ navigation/
├─ evaluation/
├─ experiment/
└─ patient_case/
```

### `model_management/`

职责：

- 加载术前模型
- 模型格式转换
- 模型元数据管理
- 向界面提供模型对象

### `probe_calibration/`

职责：

- 启动标定会话
- 收集样本
- 调用标定算法
- 输出 `T_probe_to_tip`
- 管理标定结果保存和加载

建议内部文件：

```text
probe_calibration/
├─ probe_calibration_controller.*
├─ probe_calibration_service.*
├─ probe_calibration_repository.*
├─ probe_calibration_types.*
└─ probe_calibration_validator.*
```

### `registration/`

职责：

- 管理术前数据与术中数据
- 调用配准算法
- 输出配准矩阵
- 输出配准误差

建议内部文件：

```text
registration/
├─ registration_controller.*
├─ registration_service.*
├─ registration_repository.*
├─ registration_types.*
└─ registration_validator.*
```

### `tracking/`

职责：

- 接收跟踪设备实时位姿
- 管理帧更新
- 转换为统一数据结构
- 为导航模块提供实时探针姿态

### `navigation/`

职责：

- 组合标定结果、配准结果和实时位姿
- 计算探针尖端在模型坐标系下的位置
- 更新实时导航状态
- 对目标点进行误差计算

这层是主链路核心，不要和界面代码混写。

### `evaluation/`

职责：

- 计算标定误差
- 计算配准误差
- 计算导航误差
- 生成统计报告

毕业阶段这层非常重要，因为论文和答辩都依赖它。

### `experiment/`

职责：

- 记录实验配置
- 记录实验输入输出
- 导出结果表
- 生成实验日志

### `patient_case/`

职责：

- 管理病例或任务信息
- 管理当前导航目标
- 管理与病例相关的数据路径

如果时间紧，可以先做轻量版，不必做成完整病例系统。

## 5.6 `src/services/`

这是应用服务层，负责跨模块编排。

建议放：

```text
src/services/
├─ calibration_service/
├─ registration_service/
├─ navigation_service/
├─ experiment_service/
└─ session_service/
```

职责：

- 组织业务流程
- 调用多个模块完成一项操作
- 保持界面层简洁

建议这里放“流程级”方法，例如：

- `run_calibration_workflow`
- `run_registration_workflow`
- `start_navigation_session`
- `finish_navigation_session`

## 5.7 `src/adapters/`

这是外部系统适配层，用来连接已有项目和设备。

建议拆成：

```text
src/adapters/
├─ probe_calibration_adapter/
├─ meshgpu_adapter/
├─ tracker_device_adapter/
├─ model_io_adapter/
└─ file_export_adapter/
```

这是你当前最需要的层，因为它能让 `medicalpro` 在不重写原项目的前提下接入已有能力。

### `probe_calibration_adapter/`

职责：

- 封装 `ProbeCalibration` 项目调用
- 做输入输出类型转换
- 隔离原项目接口变化

### `meshgpu_adapter/`

职责：

- 封装 `MeshGPU` 项目调用
- 输出统一的配准结果对象

### `tracker_device_adapter/`

职责：

- 封装跟踪设备 SDK
- 统一输出位姿帧

### `model_io_adapter/`

职责：

- 读写模型、点云、标记点

### `file_export_adapter/`

职责：

- 导出实验记录
- 导出日志
- 导出误差报告

## 5.8 `src/workflow/`

这是流程状态机层。

建议放：

```text
src/workflow/
├─ workflow_state.*
├─ workflow_event.*
├─ navigation_workflow.*
└─ workflow_guard.*
```

职责：

- 控制系统状态流转
- 限制用户误操作
- 保证前一步没完成时不能进入下一步

建议状态至少包括：

- `idle`
- `model_loaded`
- `calibrating`
- `calibrated`
- `registering`
- `registered`
- `navigating`
- `completed`
- `error`

## 5.9 `src/ui/`

这是界面层。

建议拆成：

```text
src/ui/
├─ windows/
├─ pages/
├─ widgets/
├─ dialogs/
├─ view_models/
└─ rendering/
```

### `windows/`

- 主窗口
- 子窗口

### `pages/`

建议按流程拆页面：

```text
pages/
├─ home_page/
├─ model_page/
├─ calibration_page/
├─ registration_page/
├─ navigation_page/
└─ evaluation_page/
```

### `widgets/`

- 可复用按钮区
- 状态栏
- 参数面板
- 数据表格

### `dialogs/`

- 参数弹窗
- 错误提示
- 文件选择

### `view_models/`

职责：

- 把领域数据转成界面数据
- 隔离 UI 和业务逻辑

### `rendering/`

职责：

- 三维场景显示
- 模型显示
- 探针显示
- 目标点显示

如果你已有 VTK、PCL、OpenGL、Qt3D 相关渲染代码，尽量集中放这里，不要散落在页面类里。

## 5.10 `src/infrastructure/`

这是基础设施层。

建议放：

```text
src/infrastructure/
├─ persistence/
├─ config/
├─ logging/
├─ threading/
└─ platform/
```

职责：

- 文件存储
- 配置读取
- 日志落盘
- 线程调度
- 平台相关封装

如果后续你要做异步采集、后台配准、日志持久化，这层会比较有用。

## 6. `tests/` 目录规划

毕业项目也建议保留最小测试结构。

```text
tests/
├─ unit/
│  ├─ probe_calibration/
│  ├─ registration/
│  └─ navigation/
├─ integration/
│  ├─ calibration_to_navigation/
│  └─ registration_to_navigation/
└─ demo/
   └─ fixed_dataset_demo/
```

重点不是追求大量单元测试，而是保证：

- 固定数据能复现
- 主链路能回归验证
- 答辩 demo 能重复运行

## 7. 目录依赖规则

为了避免后期项目再次混乱，建议强制以下依赖方向：

`ui -> services -> modules -> adapters -> external`

并且：

- `ui` 不直接调用底层外部项目
- `modules` 不直接依赖页面类
- `common` 不依赖具体业务模块
- `workflow` 只控制流程，不实现算法
- `adapters` 负责把旧项目接进来，不承载业务判断

## 8. 最小落地版目录

如果你现在时间紧，不建议一次性建完整结构。可以先落一个毕业版精简目录：

```text
medicalpro/
├─ src/
│  ├─ app/
│  ├─ common/
│  ├─ modules/
│  │  ├─ probe_calibration/
│  │  ├─ registration/
│  │  ├─ navigation/
│  │  └─ evaluation/
│  ├─ adapters/
│  │  ├─ probe_calibration_adapter/
│  │  ├─ meshgpu_adapter/
│  │  └─ tracker_device_adapter/
│  ├─ workflow/
│  └─ ui/
├─ config/
├─ resources/
├─ data/
└─ tests/
```

这版已经足够支撑毕业闭环。

## 9. 建议的迁移顺序

不要一开始大范围重构，建议按下面顺序推进。

### 第一步：建立骨架

- 建 `src/modules`
- 建 `src/adapters`
- 建 `src/workflow`
- 建 `src/ui/pages`

先把目录建起来，不急着搬所有旧代码。

### 第二步：先接入旧项目

- 用 `probe_calibration_adapter` 封装 `ProbeCalibration`
- 用 `meshgpu_adapter` 封装 `MeshGPU`

这一阶段目标是“接起来能跑”，不是“代码最优雅”。

### 第三步：抽主流程

- 把标定流程放进 `modules/probe_calibration`
- 把配准流程放进 `modules/registration`
- 把导航计算放进 `modules/navigation`

### 第四步：补界面层

- 页面只负责按钮和显示
- 页面不直接操作底层算法实现

### 第五步：补评估与实验

- 建 `modules/evaluation`
- 建 `modules/experiment`
- 导出误差和实验记录

## 10. 建议的主页面流程

为匹配目录规划，界面建议对应以下页面：

1. 首页
2. 模型加载页
3. 探针标定页
4. 配准页
5. 导航页
6. 评估结果页

这样目录结构和用户流程是一一对应的，后期维护会轻很多。

## 11. 推荐优先建设的 6 个核心模块

如果你现在只能先做一批模块，优先级建议如下：

1. `modules/probe_calibration`
2. `modules/registration`
3. `modules/navigation`
4. `adapters/probe_calibration_adapter`
5. `adapters/meshgpu_adapter`
6. `workflow`

只要这 6 个做出来，系统主链路就基本成形。

## 12. 不建议现在就做得很重的目录

毕业阶段不建议优先建设这些复杂目录：

- 完整权限系统
- 完整数据库层
- 多用户管理
- 完整病例生命周期管理
- 复杂插件化系统

这些都容易拖垮进度，但对毕业闭环帮助有限。

## 13. 结论

`medicalpro` 最合理的定位不是继续做一个“功能堆积工程”，而是做成：

- 一个主入口
- 一个流程调度中心
- 一个界面展示中心
- 一个实验记录中心

目录结构也应该围绕这个定位来组织。

毕业阶段建议你先按精简版目录落地，先把 `ProbeCalibration` 和 `MeshGPU` 通过 `adapters` 接进来，再把标定、配准、导航、评估四个模块立住。等主链路跑通后，再逐步细化目录和代码归属。
