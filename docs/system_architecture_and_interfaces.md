# 手术导航系统架构与接口设计草案

> 版本：250413.0 | 上次更新：2026-04-13

## 1. 文档目标

本文档用于定义毕业版手术导航系统的总体架构、模块边界、坐标系关系、关键数据流和接口设计，指导 `ProbeCalibration`、`MeshGPU`、`NavScene3D` 以及 `medicalpro` 的集成工作。

本文档面向的是毕业原型系统，设计原则为：

- 先保证完整闭环
- 先保证接口清晰
- 先保证可验证
- 先保证可以形成论文与答辩材料

## 2. 系统目标

系统需要完成以下主流程：

1. 加载术前模型或影像重建结果
2. 完成探针标定
3. 获取术中采点或点云
4. 完成术前与术中数据配准
5. 实时获取探针位姿
6. 在界面中显示导航结果
7. 输出误差评估结果

## 3. 总体架构

采用四层结构，Framework和src/两套代码融合。

### 3.1 表现层（UI层）

由 medicalpro 主界面承担（src/ui/），负责：

- 6个页面组织（Home、Model、Calibration、Registration、Navigation、Evaluation）
- NavigationPage嵌入QVTKOpenGLNativeWidget实现3D渲染
- 用户交互和状态反馈
- 操作流程控制

### 3.2 应用编排层（Services层）

由 src/services/ 和 src/workflow/ 承担，负责：

- 5个核心服务（Model、Calibration、Registration、Navigation、Tracking）
- 工作流状态机（9个状态）
- 数据回放控制器
- 日志和实验记录

### 3.3 算法服务层（DLL层）

由外部项目编译为DLL，通过适配器层调用：

- `ProbeCalibration.dll`：标定服务（TipCalibrationSolver、VoxelFuser）
- `MeshGPU.dll`：配准服务（GICPRegistration、RotationSearch、ProbeSimulator）

### 3.4 基础设施层（Framework层）

由 Framework/ 承担，负责：

- 统一日志（Logger）
- CTK插件框架（CTKManager）
- 数据库管理（DatabaseManager）
- 单例管理器（SingletonManager）
- VTK Widget工厂
- 资源管理（ResourceManager）

### 3.5 数据层

负责管理以下数据：

- 术前模型（PLY/STL文件）
- 模拟/回放数据（ProbeSimulator生成 + 固定数据文件）
- 标定结果（T_probe_to_tip + 误差）
- 配准结果（T_image_to_patient + 误差）
- 实时位姿数据（模拟或Atracsys追踪器）
- 实验记录和系统日志

### 3.6 架构拓扑图

```
┌─────────────────────────────────────────────────────┐
│                    medicalpro 主程序                   │
├─────────────────────────────────────────────────────┤
│  UI层 (src/ui/)                                      │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────────┐  │
│  │Model │ │Calib │ │Regis │ │ Nav  │ │  Eval    │  │
│  │Page  │ │Page  │ │Page  │ │ Page │ │  Page    │  │
│  └──┬───┘ └──┬───┘ └──┬───┘ └──┬───┘ └────┬─────┘  │
│     │        │        │     ┌──┴──┐        │        │
│     │        │        │     │ VTK │        │        │
│     │        │        │     │3D窗口│        │        │
│     │        │        │     └──┬──┘        │        │
├─────┴────────┴────────┴────────┴───────────┴────────┤
│  Services层 (src/services/)                          │
│  ┌─────────┐ ┌──────────┐ ┌──────────┐ ┌────────┐  │
│  │Calibra- │ │Registra- │ │Naviga-   │ │Tracking│  │
│  │tionSvc  │ │tionSvc   │ │tionSvc   │ │Service │  │
│  └────┬────┘ └────┬─────┘ └────┬─────┘ └───┬────┘  │
│       │           │            │            │        │
│  ┌────┴────┐ ┌────┴─────┐     │       ┌────┴────┐  │
│  │Workflow │ │DataReplay│     │       │Simulator│  │
│  │Manager  │ │Controller│     │       │/Atracsys│  │
│  └─────────┘ └──────────┘     │       └─────────┘  │
├───────────────────────────────┴─────────────────────┤
│  Adapters层 (src/adapters/)                          │
│  ┌──────────────────┐  ┌──────────────────┐         │
│  │ProbeCalibration  │  │MeshGPU           │         │
│  │Adapter           │  │Adapter           │         │
│  │(DLL动态加载)      │  │(DLL动态加载)      │         │
│  └────────┬─────────┘  └────────┬─────────┘         │
├───────────┼──────────────────────┼──────────────────┤
│  DLL层    │                      │                   │
│  ┌────────┴─────────┐  ┌────────┴─────────┐         │
│  │ProbeCalibration  │  │MeshGPU.dll       │         │
│  │.dll               │  │(CUDA加速)        │         │
│  │- TipCalibSolver  │  │- GICPRegistration│         │
│  │- VoxelFuser      │  │- RotationSearch  │         │
│  │- RANSAC          │  │- ProbeSimulator  │         │
│  └──────────────────┘  └──────────────────┘         │
├─────────────────────────────────────────────────────┤
│  Framework层                                         │
│  Logger │ CTKManager │ DatabaseMgr │ VTK │ Resource │
└─────────────────────────────────────────────────────┘
```

## 4. 项目职责边界

### 4.1 ProbeCalibration（DLL）

职责：

- 采集标定所需的姿态样本
- 计算探针尖端相对工具坐标系的偏移（Pivot Calibration + RANSAC）
- 输出标定误差
- 提供结果保存与加载能力
- 体素融合（VoxelFuser）：将实时采集点去重降噪

不负责：

- 配准
- 导航界面显示
- 主流程调度

DLL导出接口：C API（pc_create_session, pc_add_sample, pc_run_calibration等）

### 4.2 MeshGPU（DLL）

职责：

- 加载或接收术前模型和术中点云
- 执行GPU加速GICP配准算法
- 支持多种曲率加权策略
- 支持多分辨率配准
- 支持旋转搜索（暴力/分层/全球）
- 输出空间变换矩阵和配准误差
- ProbeSimulator：从模型生成模拟点云

不负责：

- 探针标定
- 主界面流程
- 系统状态管理

DLL导出接口：C API（mg_create_mesh, mg_load_target, mg_run_registration等）

### 4.3 NavScene3D（参考实现，不直接集成）

作用：

- 提供3D导航场景的参考设计（探针模板、双空间显示）
- 其功能将用VTK在medicalpro中重新实现

参考要点：

- 探针模板：圆柱体轴(120mm) + 球体尖端(2.8mm) + 圆锥体头部
- 双空间显示：真实空间和虚拟空间并排
- 尖端位置计算：变换链式计算

### 4.4 medicalpro（主程序）

职责：

- 作为毕业版最终主程序
- Framework提供基础设施，src/提供业务逻辑
- 通过适配器层动态加载ProbeCalibration和MeshGPU DLL
- VTK 3D渲染（嵌入NavigationPage）
- 统一流程状态机
- 数据回放系统
- 统一实验数据和日志输出
- 为答辩演示提供稳定运行环境

## 5. 核心流程设计

建议将系统流程拆成以下阶段：

1. 系统初始化
2. 术前数据加载
3. 探针标定
4. 术中采点或点云采集
5. 配准
6. 导航运行
7. 误差评估与结果保存

每个阶段完成后都应产生明确输出，供后续阶段调用。

## 6. 坐标系设计

这是系统最关键的部分，必须先统一。

建议至少定义以下坐标系：

- `Tracker`：外部跟踪器坐标系
- `Probe`：探针工具坐标系
- `Tip`：探针尖端坐标系
- `Patient`：患者或术中目标坐标系
- `Image`：术前图像或模型坐标系
- `World`：可选的系统显示坐标系

## 7. 关键变换链

毕业版至少要明确以下变换关系：

- `T_tracker_to_probe`
- `T_probe_to_tip`
- `T_image_to_patient`
- `T_tracker_to_patient`
- `T_image_to_tracker`

导航时常见计算目标为：

通过实时跟踪得到探针姿态，再结合标定结果得到探针尖端位置；再通过配准结果把探针尖端映射到术前模型坐标系中进行显示。

建议在实现中强制规定：

- 所有矩阵命名必须体现起点和终点
- 所有模块接口文档明确输入输出坐标系
- 所有显示层调用前先打印或记录当前坐标系说明

## 8. 数据流设计

### 8.1 标定数据流

输入：

- 多帧探针姿态样本
- 跟踪设备返回的位姿数据

输出：

- `T_probe_to_tip`
- 标定误差
- 标定时间和样本数

### 8.2 配准数据流

输入：

- 术前模型或点集
- 术中点云或采样点

输出：

- `T_image_to_patient` 或等效配准矩阵
- 配准误差
- 配准统计信息

### 8.3 导航数据流

输入：

- 实时探针位姿
- 标定矩阵
- 配准矩阵
- 术前模型

输出：

- 探针尖端在模型中的实时位置
- 导航显示结果
- 导航误差或目标偏差

## 9. 接口设计

### 9.1 ProbeCalibration DLL C API

```c
// probe_calibration_api.h
#ifdef PROBE_CALIBRATION_EXPORTS
#define PC_API __declspec(dllexport)
#else
#define PC_API __declspec(dllimport)
#endif

extern "C" {
    // 会话管理
    PC_API void* pc_create_session();
    PC_API void pc_destroy_session(void* session);

    // 标定数据采集
    PC_API int pc_add_sample(void* session, const float* transform_4x4);
    PC_API int pc_get_sample_count(void* session);
    PC_API void pc_clear_samples(void* session);

    // 标定计算
    PC_API int pc_run_calibration(void* session);
    PC_API int pc_run_calibration_ransac(void* session);

    // 结果获取
    PC_API int pc_get_tip_offset(void* session, float* offset_xyz);
    PC_API float pc_get_error(void* session);
    PC_API int pc_is_valid(void* session);

    // 结果持久化
    PC_API int pc_save_result(void* session, const char* filepath);
    PC_API int pc_load_result(void* session, const char* filepath);
}
```

### 9.2 MeshGPU DLL C API

```c
// meshgpu_api.h
#ifdef MESHGPU_EXPORTS
#define MG_API __declspec(dllexport)
#else
#define MG_API __declspec(dllimport)
#endif

extern "C" {
    // 网格管理
    MG_API void* mg_create_context();
    MG_API void mg_destroy_context(void* ctx);

    // 数据加载
    MG_API int mg_load_target_mesh(void* ctx, const float* vertices,
                                    int num_verts, const int* faces,
                                    int num_faces);
    MG_API int mg_load_source_points(void* ctx, const float* points,
                                      int num_points);
    MG_API int mg_load_target_from_file(void* ctx, const char* filepath);

    // 配准参数
    MG_API void mg_set_max_iterations(void* ctx, int max_iter);
    MG_API void mg_set_convergence_threshold(void* ctx, float threshold);
    MG_API void mg_set_curvature_weight_mode(void* ctx, int mode);
    // mode: 0=NONE, 1=HIGH_CURVATURE, 2=LOW_CURVATURE,
    //        3=GAUSSIAN_AWARE, 4=ADAPTIVE, 5=COVERAGE_AWARE

    // 配准执行
    MG_API int mg_run_registration(void* ctx, float* result_4x4);
    MG_API int mg_run_rotation_search(void* ctx, int strategy,
                                       float* result_4x4);
    // strategy: 0=BRUTE_FORCE, 1=HIERARCHICAL, 2=GLOBAL, 3=GLOBAL_HIERARCHICAL

    // 结果获取
    MG_API float mg_get_rms_error(void* ctx);
    MG_API float mg_get_max_error(void* ctx);
    MG_API int mg_get_iterations(void* ctx);
    MG_API float mg_get_elapsed_ms(void* ctx);

    // ProbeSimulator（模拟数据生成）
    MG_API void* mg_create_simulator(void* ctx);
    MG_API int mg_simulate_probe_cloud(void* sim, int mode,
                                        float noise_mm,
                                        float* out_points,
                                        int* out_count);
    // mode: 0=FULL, 1=PARTIAL, 2=CLINICAL_REACHABLE, 3=ROTATED_3D
    MG_API void mg_destroy_simulator(void* sim);
}
```

### 9.3 medicalpro内部服务接口（已实现，需更新）

#### CalibrationService（更新为DLL调用）

```cpp
class CalibrationService : public QObject {
    // 现有接口保持不变，内部实现从模拟切换为DLL调用
    void startSession();
    void addSample(const Matrix4f& pose);
    void runCalibration();
    CalibrationResult getResult() const;
    void saveResult(const QString& path);
    void loadResult(const QString& path);

private:
    ProbeCalibrationAdapter* m_adapter; // DLL适配器
};
```

#### RegistrationService（更新为DLL调用）

```cpp
class RegistrationService : public QObject {
    void loadPreoperativeModel(const QString& path);
    void loadIntraoperativePoints(const std::vector<Vector3f>& points);
    void runRegistration();
    RegistrationResult getResult() const;

private:
    MeshGPUAdapter* m_adapter; // DLL适配器
};
```

### 9.4 VTK 3D渲染接口（新增）

```cpp
class NavigationVTKRenderer : public QObject {
    // 初始化
    void initialize(QVTKOpenGLNativeWidget* widget);

    // 模型管理
    void loadBoneModel(const QString& filepath);
    void setProbeTemplate(float shaft_length, float shaft_radius,
                          float tip_radius);

    // 实时更新
    void updateProbePose(const Matrix4f& transform);
    void updateBoneTransform(const Matrix4f& transform);

    // 显示控制
    void showRegistrationPoints(const std::vector<Vector3f>& pre_points,
                                 const std::vector<Vector3f>& intra_points);
    void showCoordinateFrame(const Matrix4f& transform, double size);
    void setProbeColor(double r, double g, double b);
    void setBoneColor(double r, double g, double b);
    void setBoneOpacity(double opacity);

    // 相机控制
    void resetCamera();
    void setCameraFocusOnTip();
};
```

## 10. 建议的数据结构

以下字段建议至少保留。

### 10.1 标定结果

- `probe_id`
- `sample_count`
- `transform_probe_to_tip`
- `calibration_error`
- `created_at`

### 10.2 配准结果

- `registration_id`
- `transform_image_to_patient`
- `registration_error`
- `algorithm_name`
- `iterations`
- `created_at`

### 10.3 导航状态

- `navigation_state`
- `current_probe_pose`
- `current_tip_position`
- `target_position`
- `target_distance`
- `last_update_time`

## 11. 界面结构（已实现 + 待增强）

毕业版界面6个页面（已实现）：

- HomePage：系统首页、快速操作、状态概览
- ModelPage：术前模型加载（PLY/STL）、统计信息显示
- CalibrationPage：探针标定流程、样本采集、误差显示
- RegistrationPage：配准流程、配准点表格、ICP精细配准
- NavigationPage：实时导航（待增强：嵌入VTK 3D窗口）
- EvaluationPage：误差评估、统计报告（待完善：连接评估服务）

NavigationPage 3D渲染增强（VTK嵌入）：

- QVTKOpenGLNativeWidget嵌入到NavigationPage布局中
- 左侧：3D渲染区域（骨骼模型 + 探针 + 配准点 + 坐标系）
- 右侧：数值显示（LCD探针位置 + 目标距离 + 误差 + 状态）
- 底部：导航控制按钮（开始/暂停/冻结/停止）

## 12. 状态机建议

主程序建议维护简单状态机：

- `Idle`
- `ModelLoaded`
- `CalibrationReady`
- `Calibrated`
- `RegistrationReady`
- `Registered`
- `Navigating`
- `Completed`
- `Error`

状态机的作用是避免用户在未完成前置步骤时误操作。

## 13. 日志与实验记录

为毕业论文和答辩准备，系统应统一记录以下内容：

- 标定时间
- 标定样本数
- 标定误差
- 配准数据来源
- 配准误差
- 导航目标点
- 导航偏差
- 操作时间戳
- 系统异常信息

建议所有实验结果导出为结构化文件，便于后续统计和论文制表。

## 14. 验证方案

### 14.1 模块验证

- 标定模块独立验证
- 配准模块独立验证
- 导航显示模块独立验证

### 14.2 集成验证

- 无设备时使用固定数据回放
- 有设备时做实时联调
- 每轮集成都记录误差与异常

### 14.3 答辩验证

- 能在固定数据上稳定跑完整流程
- 能现场展示探针导航界面
- 能展示实验表格和误差结果

## 15. 落地顺序（基于当前进度更新）

已完成：

1. ✅ 统一坐标系命名和矩阵方向（6个坐标系已定义）
2. ✅ 固定标定结果数据结构
3. ✅ 固定配准结果数据结构
4. ✅ 6个UI页面和5个服务层已实现
5. ✅ 模拟模式下完整流程可运行

下一步：

1. ProbeCalibration编译为DLL，在medicalpro中动态加载调用
2. MeshGPU编译为DLL，在medicalpro中动态加载调用
3. NavigationPage嵌入QVTKOpenGLNativeWidget，实现3D渲染
4. 集成ProbeSimulator生成模拟数据 + 实现固定数据回放
5. 跑通完整闭环（真实DLL + VTK 3D）
6. 完善EvaluationPage，完成实验数据采集
7. 导出论文图表和实验结果

## 16. 最小可交付架构结论

毕业版系统的核心不是功能多，而是这条链必须稳定：

`模型加载 -> 标定(DLL) -> 配准(DLL+CUDA) -> 导航(VTK 3D) -> 误差输出`

只要以下五点成立，系统架构就可以认为满足毕业要求：

- 模块职责清晰（四层架构 + DLL隔离）
- 坐标关系清晰（6个坐标系 + 统一变换链）
- 数据流和接口清晰（C API + 适配器层）
- 演示和实验可以稳定复现（数据回放模式）
- GPU加速配准有性能数据支撑（论文核心亮点）

后续一切开发都应围绕这五点推进，而不是继续扩展外围功能。
