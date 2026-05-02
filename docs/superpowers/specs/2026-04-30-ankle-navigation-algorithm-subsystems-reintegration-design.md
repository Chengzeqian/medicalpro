# Ankle Navigation Algorithm Subsystems Reintegration Design

**日期：** 2026-04-30  
**范围：** 将 `MeshGPU` 与 `ProbeCalibration` 从当前独立开发目录并回 `medicalpro`，纳入踝关节导航毕业项目的统一源码交付边界。  
**目标：** 消除 `E:\ICPtry\...` 和历史 `ICPtry/...` 私有路径耦合，使配准与探针标定算法以项目内部算法子系统形式统一构建、统一部署、统一验证，并为后续论文创新实现和复现提供稳定底座。

## 1. 背景与问题定义

当前毕业项目的主创新点集中在配准算法，但其核心实现仍分散在两个独立目录：

- `E:\ICPtry\MeshGPU`
- `E:\ICPtry\ProbeCalibration`

`medicalpro` 当前已经具备对这两个算法项目的接入代码，但接入方式仍停留在个人开发态：

- [RegistrationServiceImpl.cpp](D:/Qtproject/medicalpro/.worktrees/ankle-nav-arch-remediation-20260430/Plugins/RegistrationCore/RegistrationServiceImpl.cpp#L85) 通过 `QLibrary` 加载 `MeshGPULib.dll`
- [OpticalTrackingServiceImpl.cpp](D:/Qtproject/medicalpro/.worktrees/ankle-nav-arch-remediation-20260430/Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp#L5985) 通过 `QLibrary` 加载 `ProbeCalibration.dll`
- [Plugins/RegistrationCore/CMakeLists.txt](D:/Qtproject/medicalpro/.worktrees/ankle-nav-arch-remediation-20260430/Plugins/RegistrationCore/CMakeLists.txt#L18) 仍默认假设 `ICPtry/MeshGPU/...`
- [Plugins/OpticalTracking/CMakeLists.txt](D:/Qtproject/medicalpro/.worktrees/ankle-nav-arch-remediation-20260430/Plugins/OpticalTracking/CMakeLists.txt#L88) 仍默认假设 `ICPtry/ProbeCalibration/...`

这会导致三个直接问题：

1. 论文主创新实现不在统一源码边界内，不利于答辩、归档和复现。
2. 构建与运行依赖机器私有路径，交付不可重复。
3. 宿主插件层和算法实现层边界不清，后续修改风险高。

## 2. 设计目标

本设计只解决“算法子系统回归主项目”的问题，不同时重构算法行为。

必须达成的结果：

- `MeshGPU` 与 `ProbeCalibration` 成为 `medicalpro` 仓库内可见、可构建、可验证的算法子系统。
- `medicalpro` 主工程可以统一构建算法 DLL，并在运行时从标准输出目录加载。
- 删除所有对 `D:\...`、`E:\...`、旧 `ICPtry/...` 目录的硬编码回退。
- `RegistrationCore` 和 `OpticalTracking` 保持宿主适配层身份，不直接吞并算法源码实现。
- 并入后不主动改变现有配准结果和标定结果语义。

明确不在本阶段完成的内容：

- 不在本阶段重写 `MeshGPU` 配准算法。
- 不在本阶段重写 `ProbeCalibration` 标定算法。
- 不在本阶段把 `Atracsys SDK` 一并并入仓库。
- 不在本阶段扩展新的论文功能点。

## 3. 核心决策

### 3.1 物理并入，而不是继续外部挂接

本设计采用“物理迁入主仓库”的方案，而不是保持 `E:\ICPtry\...` 外挂。

原因：

- 论文主创新资产必须位于统一源码边界内。
- 长期保留外挂路径会继续制造不可复现风险。
- 当前两个项目均为作者本人开发，不存在第三方源码归属障碍。

### 3.2 保留 DLL/Lib 边界，而不是把算法源码揉入插件

本设计不把 `MeshGPU` 和 `ProbeCalibration` 直接混进 `Plugins/RegistrationCore` 或 `Plugins/OpticalTracking`。

而是保留三层边界：

- `medicalpro`：主应用与工作流宿主
- `algorithms/*`：算法子系统
- `Plugins/*`：算法接入和业务编排层

原因：

- 容易区分“平台系统问题”和“算法引擎问题”
- 有利于论文中清晰表达系统层与算法层的关系
- 后续单独修改算法实现时，不会把宿主主链一并拖乱

### 3.3 先收编构建与路径，再动算法内部

迁移顺序必须固定为：

1. 源码迁入
2. CMake 收口
3. 运行时加载收口
4. 最小回归
5. 算法内部整理

禁止反过来先动算法逻辑。  
原因是当前目标是降低系统集成风险，而不是同步引入算法行为变化。

## 4. 目标目录结构

最终目录结构定义为：

```text
medicalpro/
├─ algorithms/
│  ├─ meshgpu/
│  │  ├─ include/
│  │  ├─ src/
│  │  ├─ CMakeLists.txt
│  │  └─ README.md
│  └─ probe_calibration/
│     ├─ include/
│     ├─ src/
│     ├─ CMakeLists.txt
│     └─ README.md
├─ Plugins/
│  ├─ RegistrationCore/
│  └─ OpticalTracking/
├─ Framework/
├─ UI/
├─ tests/
└─ CMakeLists.txt
```

### 4.1 允许迁入的内容

首批只迁入以下资产：

- `include/`
- `src/`
- `CMakeLists.txt`
- 最小必要说明文档

### 4.2 禁止迁入的内容

禁止把以下内容带入主仓库：

- `build/`
- `logs/`
- `benchmark_results/`
- `visualization_output/`
- `tmp_*`
- 本地测试脚本临时产物

## 5. 构建与依赖策略

### 5.1 顶层构建策略

`medicalpro` 顶层 CMake 负责：

- `add_subdirectory(algorithms/meshgpu)`
- `add_subdirectory(algorithms/probe_calibration)`
- 将生成的算法运行时产物复制到 `medicalpro` 应用输出目录
- 为主程序和测试暴露稳定的目标依赖关系

目标状态：

- `MeshGPULib.dll` 由仓库内源码构建产生
- `ProbeCalibration.dll` 由仓库内源码构建产生
- 主程序运行时只从应用输出目录查找这两个 DLL

### 5.2 子项目构建策略

`algorithms/meshgpu` 与 `algorithms/probe_calibration` 仍保留各自的子项目 `CMakeLists.txt`，但要做两类改造：

1. 统一相对路径基准  
   不再依赖原有 `E:\ICPtry\...` 目录结构，也不再假设旧的 `../eigen`、`../Atracsys` 恰好存在于外部实验目录旁。

2. 区分“主交付目标”和“开发工具目标”  
   子项目内原有 `benchmark`、`visualization`、`discovery`、`demo`、`collection_test` 等工具目标保留，但默认不应阻塞主应用交付构建。

### 5.3 外部依赖处理

依赖处理原则如下：

- `Eigen`：可纳入毕业项目统一源码边界，作为仓库内依赖管理
- `Atracsys SDK`：保持为外部 SDK，通过 CMake 变量、Preset 或明确路径配置提供
- CUDA、VTK、Qt：继续沿用当前主工程构建体系

`Atracsys SDK` 不建议直接迁入仓库，原因是其性质属于外部硬件 SDK，不是本论文创新实现本体。

## 6. 宿主接入层重构要求

### 6.1 RegistrationCore

[RegistrationServiceImpl.cpp](D:/Qtproject/medicalpro/.worktrees/ankle-nav-arch-remediation-20260430/Plugins/RegistrationCore/RegistrationServiceImpl.cpp#L85) 需要改为：

- 只优先从 `QCoreApplication::applicationDirPath()` 或明确配置的内部路径加载 `MeshGPULib.dll`
- 删除所有私有硬编码回退路径
- 保留当前 `QLibrary` 动态加载模型
- 保留 `MeshGPU` 不可用时的回退配准策略，但回退行为需可观测

### 6.2 OpticalTracking

[OpticalTrackingServiceImpl.cpp](D:/Qtproject/medicalpro/.worktrees/ankle-nav-arch-remediation-20260430/Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp#L5985) 需要改为：

- 只优先从 `QCoreApplication::applicationDirPath()` 或明确配置的内部路径加载 `ProbeCalibration.dll`
- 删除所有私有硬编码回退路径
- 保留当前 `QLibrary` 动态加载模型
- 保留 `ProbeCalibration` 不可用时的可解释退化路径

## 7. 迁移实施批次

### 批次 A：源码迁入与系统收口

目标：不改算法行为，只把两个项目安全收编到主仓库。

步骤：

1. 迁入 `MeshGPU` 源码到 `algorithms/meshgpu`
2. 迁入 `ProbeCalibration` 源码到 `algorithms/probe_calibration`
3. 收口两个子项目的 CMake
4. 接入主仓库顶层 CMake
5. 收口运行时 DLL 复制和加载路径
6. 验证 `medicalpro` 构建、启动、配准链、标定链

验收标准：

- 主工程构建时能生成 `MeshGPULib.dll` 与 `ProbeCalibration.dll`
- 主程序运行时能从统一输出目录加载两者
- 主配准链与探针标定链不依赖 `E:\ICPtry\...`

### 批次 B：算法内部整理与论文创新强化

目标：在已并回系统的前提下，对算法内部实现继续演进。

内容包括：

- `MeshGPU` 内部结构整理
- `ProbeCalibration` 内部结构整理
- 与踝关节导航三项创新点的接口进一步对齐
- 补充算法层回归验证和论文复现实验资产

## 8. 风险与控制

### 8.1 风险：迁移时引入算法行为漂移

控制策略：

- 批次 A 不主动修改算法数值逻辑
- 先确保“迁移前能跑”和“迁移后能跑”的行为一致

### 8.2 风险：子项目内部工具目标拖垮主构建

控制策略：

- 将主交付目标和开发工具目标拆分
- 默认只构建交付所需 DLL / Lib

### 8.3 风险：外部 SDK 路径继续污染主工程

控制策略：

- 明确 `Atracsys SDK` 为外部配置项
- 禁止在生产代码里写私有绝对路径

### 8.4 风险：宿主层和算法层再次缠绕

控制策略：

- 算法源码只放在 `algorithms/*`
- 插件层只保留适配和业务编排逻辑

## 9. 测试与验证要求

本阶段至少需要完成以下验证：

1. 构建验证  
   `medicalpro` 顶层构建可生成主程序、`MeshGPULib.dll`、`ProbeCalibration.dll`

2. 加载验证  
   主程序日志中可观测到两个算法 DLL 的成功加载

3. 功能回归验证  
   - 配准主链可继续运行
   - 探针标定主链可继续运行
   - 当前踝关节导航相关自动化测试不被打坏

4. 环境去耦验证  
   删除或断开 `E:\ICPtry\...` 后，主工程仍可从仓库内构建与运行

## 10. 最终结论

本设计将 `MeshGPU` 与 `ProbeCalibration` 从“独立外部实验项目”重新定义为“毕业项目内部算法子系统”，并要求：

- 统一源码边界
- 统一构建边界
- 统一运行时边界
- 统一验证边界

这样可以把论文主创新点真正收回到 `medicalpro` 主项目体系内，解决当前最关键的可复现性、可交付性和后续迭代稳定性问题。
