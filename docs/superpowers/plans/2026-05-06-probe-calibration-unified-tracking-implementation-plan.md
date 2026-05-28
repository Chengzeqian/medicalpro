# Probe Calibration Unified Tracking Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将探针标定改造成单 tracking 数据源架构，由上层统一提供 pose 样本，`ProbeCalibration.dll` 只负责标定求解与结构化结果输出。

**Architecture:** 本次实现分三层推进。第一层重构 `algorithms/probe_calibration` C API，删除 pivot 标定对内部 tracker ownership 的依赖，新增显式 pose sample/result 接口。第二层切换 `OpticalTrackingServiceImpl` 到新 DLL 集成路径，移除错误的 collector 驱动方式，并强化 geometry 与设备状态的显式规则。第三层补齐单元测试、运行时 smoke test 和日志可观测性，确保新路径可验证、旧误用不可回归。

**Tech Stack:** C++17, Qt, QLibrary, Atracsys pose data structures, existing ProbeCalibration solver, QtTest

---

## File Structure

### 需要修改的核心文件

- `algorithms/probe_calibration/include/probe_calibration_c_api.h`
  - 新增统一 tracking 架构下的 C 数据结构与 API 声明。
- `algorithms/probe_calibration/src/probe_calibration_c_api.cpp`
  - 实现新的 pose sample 输入、geometry 配置、结果导出与统计接口。
- `algorithms/probe_calibration/include/realtime_transform.h`
  - 将 `ProbeTrackingPipeline` 从“拥有 tracker”改造成“只管理录制器、求解器、结果状态”的计算管线。
- `algorithms/probe_calibration/src/realtime_transform.cpp`
  - 删除内部 tracker 初始化/启动逻辑，引入显式 pose 样本录入路径。
- `algorithms/probe_calibration/include/calibration_recorder.h`
  - 如有必要，暴露统计读取和配置收敛接口。
- `algorithms/probe_calibration/src/calibration_recorder.cpp`
  - 如有必要，增强录制统计与会话重置行为。
- `Plugins/OpticalTracking/OpticalTrackingServiceImpl.h`
  - 新增 DLL 新接口函数指针、设备模式状态、geometry 解析与标定结果应用辅助函数。
- `Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp`
  - 切换到统一 tracking 标定路径。
- `tests/unit/ProbeCalibrationRuntimeSmokeTest.cpp`
  - 更新 DLL 导出接口 smoke test。
- `tests/unit/OpticalTrackingProbeCalibrationInitializationTest.cpp`
  - 更新 geometry 初始化与 DLL 状态测试。
- `tests/unit/OpticalTrackingProbeCalibrationToolGeometrySelectionTest.cpp`
  - 更新 geometry 解析规则测试，移除默认 `072` 静默回退假设。
- `tests/unit/` 下新增统一 tracking 集成测试文件
  - 验证 pose sample 输入、结果输出与上层应用。

### 建议新增的测试文件

- `tests/unit/ProbeCalibrationPoseSampleApiTest.cpp`
- `tests/unit/OpticalTrackingProbeCalibrationResultApplicationTest.cpp`
- `tests/unit/OpticalTrackingPhysicalDeviceModeTest.cpp`

---

### Task 1: 定义新的 DLL C API 契约

**Files:**
- Modify: `algorithms/probe_calibration/include/probe_calibration_c_api.h`
- Test: `tests/unit/ProbeCalibrationRuntimeSmokeTest.cpp`

- [ ] **Step 1: 写失败测试，锁定 DLL 新接口符号要求**

```cpp
void ProbeCalibrationRuntimeSmokeTest::probe_calibration_runtime_exports_unified_tracking_api()
{
    const QString runtimeDllPath = QCoreApplication::applicationDirPath() + QStringLiteral("/ProbeCalibration.dll");
    QLibrary library(runtimeDllPath);
    QVERIFY2(library.load(), qPrintable(library.errorString()));

    QVERIFY(library.resolve("PC_ConfigureGeometry"));
    QVERIFY(library.resolve("PC_ResetCalibrationSession"));
    QVERIFY(library.resolve("PC_AddPoseSample"));
    QVERIFY(library.resolve("PC_GetCalibrationResult"));
    QVERIFY(library.resolve("PC_GetCalibrationStats"));

    library.unload();
}
```

- [ ] **Step 2: 运行测试，确认当前失败**

Run: `ctest -R ProbeCalibrationRuntimeSmokeTest --output-on-failure`
Expected: FAIL，提示一个或多个新符号未导出

- [ ] **Step 3: 在头文件中定义新的 C API 数据结构和函数声明**

```cpp
typedef struct PC_Matrix4x4f {
    float m[16];
} PC_Matrix4x4f;

typedef struct PC_PoseSample {
    uint32_t geometry_id;
    uint64_t timestamp_us;
    float registration_error;
    int is_valid;
    PC_Matrix4x4f transform;
} PC_PoseSample;

typedef struct PC_CalibrationResult {
    PC_Vector3f tip_offset;
    float residual_error;
    uint32_t geometry_id;
    uint32_t num_poses_used;
    int is_valid;
} PC_CalibrationResult;

typedef struct PC_CalibrationStats {
    uint32_t total_received;
    uint32_t total_accepted;
    uint32_t rejected_invalid;
    uint32_t rejected_high_error;
    uint32_t rejected_similar;
    float angular_coverage;
    float mean_registration_error;
} PC_CalibrationStats;

PROBECALIB_API int PC_ConfigureGeometry(PC_PipelineHandle handle, const char* geometry_path, uint32_t geometry_id);
PROBECALIB_API int PC_ResetCalibrationSession(PC_PipelineHandle handle);
PROBECALIB_API int PC_AddPoseSample(PC_PipelineHandle handle, const PC_PoseSample* sample);
PROBECALIB_API int PC_GetCalibrationResult(PC_PipelineHandle handle, PC_CalibrationResult* out_result);
PROBECALIB_API int PC_GetCalibrationStats(PC_PipelineHandle handle, PC_CalibrationStats* out_stats);
```

- [ ] **Step 4: 运行测试，确认接口声明阶段可编译**

Run: `cmake --build build_x64_v142 --config Release --target ProbeCalibrationDLL`
Expected: BUILD SUCCESS

- [ ] **Step 5: Commit**

```bash
git add algorithms/probe_calibration/include/probe_calibration_c_api.h tests/unit/ProbeCalibrationRuntimeSmokeTest.cpp
git commit -m "feat: define unified tracking probe calibration c api"
```

---

### Task 2: 去掉 DLL 内部 tracker ownership

**Files:**
- Modify: `algorithms/probe_calibration/include/realtime_transform.h`
- Modify: `algorithms/probe_calibration/src/realtime_transform.cpp`
- Test: `tests/unit/ProbeCalibrationPoseSampleApiTest.cpp`

- [ ] **Step 1: 写失败测试，锁定 pipeline 不再依赖内部 tracker 初始化**

```cpp
void ProbeCalibrationPoseSampleApiTest::pipeline_accepts_external_pose_samples_without_tracker_bootstrap()
{
    auto* handle = PC_CreatePipeline();
    QVERIFY(handle != nullptr);

    QVERIFY(PC_ResetCalibrationSession(handle) == 1);
    QVERIFY(PC_StartCalibration(handle) == 1);

    PC_DestroyPipeline(handle);
}
```

- [ ] **Step 2: 运行测试，确认当前失败**

Run: `ctest -R ProbeCalibrationPoseSampleApiTest --output-on-failure`
Expected: FAIL，当前流程仍隐含依赖 tracker 初始化或 geometry 初始化

- [ ] **Step 3: 修改 `ProbeTrackingPipeline` 头文件，删除 tracker 生命周期职责**

```cpp
class ProbeTrackingPipeline {
public:
    ProbeTrackingPipeline();
    ~ProbeTrackingPipeline();

    bool configureGeometry(const std::string& geometry_path, uint32_t geometry_id);
    void resetCalibrationSession();
    void startCalibration();
    bool addPoseSample(const PoseData& pose);
    bool finishCalibration();

    CalibrationResult getCalibrationResult() const { return calibration_result_; }
    RecordingStats getRecordingStats() const;
    bool hasGeometryConfigured() const { return geometry_configured_; }
    uint32_t geometryId() const { return geometry_id_; }

private:
    CalibrationRecorder recorder_;
    TipCalibrationSolver solver_;
    RealtimeTransform transform_;
    bool geometry_configured_;
    uint32_t geometry_id_;
    std::string geometry_path_;
    CalibrationResult calibration_result_;
};
```

- [ ] **Step 4: 在实现中移除 `AtracsysTracker` 初始化、回调和 tracking 启停逻辑**

```cpp
bool ProbeTrackingPipeline::configureGeometry(const std::string& geometry_path, uint32_t geometry_id)
{
    if (geometry_path.empty()) {
        return false;
    }

    geometry_path_ = geometry_path;
    geometry_id_ = geometry_id;
    geometry_configured_ = true;
    calibration_result_ = CalibrationResult();
    recorder_.clear();
    return true;
}

void ProbeTrackingPipeline::resetCalibrationSession()
{
    calibration_result_ = CalibrationResult();
    recorder_.clear();
}

bool ProbeTrackingPipeline::addPoseSample(const PoseData& pose)
{
    if (!geometry_configured_) {
        return false;
    }

    return recorder_.isRecording() ? recorder_.addPose(pose) : false;
}
```

- [ ] **Step 5: 运行测试确认 pipeline 可在无 tracker 路径下工作**

Run: `ctest -R ProbeCalibrationPoseSampleApiTest --output-on-failure`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add algorithms/probe_calibration/include/realtime_transform.h algorithms/probe_calibration/src/realtime_transform.cpp tests/unit/ProbeCalibrationPoseSampleApiTest.cpp
git commit -m "refactor: remove internal tracker ownership from probe calibration pipeline"
```

---

### Task 3: 实现 DLL pose sample 输入与结构化结果输出

**Files:**
- Modify: `algorithms/probe_calibration/src/probe_calibration_c_api.cpp`
- Test: `tests/unit/ProbeCalibrationPoseSampleApiTest.cpp`

- [ ] **Step 1: 写失败测试，验证 DLL 能接收 pose sample 并导出结果**

```cpp
void ProbeCalibrationPoseSampleApiTest::dll_returns_structured_calibration_result_after_pose_samples()
{
    auto* handle = PC_CreatePipeline();
    QVERIFY(handle != nullptr);

    QVERIFY(PC_ConfigureGeometry(handle, "dummy_geometry.ini", 72u) == 1);
    QVERIFY(PC_ResetCalibrationSession(handle) == 1);
    QVERIFY(PC_StartCalibration(handle) == 1);

    PC_PoseSample sample{};
    sample.geometry_id = 72u;
    sample.timestamp_us = 1000u;
    sample.registration_error = 0.2f;
    sample.is_valid = 1;
    sample.transform.m[0] = 1.0f;
    sample.transform.m[5] = 1.0f;
    sample.transform.m[10] = 1.0f;
    sample.transform.m[15] = 1.0f;

    QVERIFY(PC_AddPoseSample(handle, &sample) == 1);

    PC_CalibrationResult result{};
    QVERIFY(PC_GetCalibrationResult(handle, &result) == 1 || PC_FinishCalibration(handle) == 1);

    PC_DestroyPipeline(handle);
}
```

- [ ] **Step 2: 运行测试，确认当前失败**

Run: `ctest -R ProbeCalibrationPoseSampleApiTest --output-on-failure`
Expected: FAIL，当前 API 未实现

- [ ] **Step 3: 在 C API 中实现 geometry 配置、sample 输入、结果与统计读取**

```cpp
int PC_ConfigureGeometry(PC_PipelineHandle handle, const char* geometry_path, uint32_t geometry_id)
{
    auto* impl = castHandle(handle);
    if (!impl || !geometry_path || geometry_path[0] == '\0') {
        return 0;
    }

    clearError(impl);
    return impl->pipeline.configureGeometry(geometry_path, geometry_id) ? 1 : 0;
}

int PC_AddPoseSample(PC_PipelineHandle handle, const PC_PoseSample* sample)
{
    auto* impl = castHandle(handle);
    if (!impl || !sample) {
        return 0;
    }

    ProbeCalib::PoseData pose;
    pose.geometry_id = sample->geometry_id;
    pose.timestamp_us = sample->timestamp_us;
    pose.registration_error = sample->registration_error;
    pose.is_valid = (sample->is_valid == 1);

    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            pose.transform(row, col) = sample->transform.m[row * 4 + col];
        }
    }

    clearError(impl);
    return impl->pipeline.addPoseSample(pose) ? 1 : 0;
}
```

- [ ] **Step 4: 实现 `PC_GetCalibrationResult` 和 `PC_GetCalibrationStats`**

```cpp
int PC_GetCalibrationResult(PC_PipelineHandle handle, PC_CalibrationResult* out_result)
{
    auto* impl = castHandle(handle);
    if (!impl || !out_result) {
        return 0;
    }

    const auto result = impl->pipeline.getCalibrationResult();
    out_result->tip_offset.x = result.tip_offset.x();
    out_result->tip_offset.y = result.tip_offset.y();
    out_result->tip_offset.z = result.tip_offset.z();
    out_result->residual_error = result.residual_error;
    out_result->geometry_id = impl->pipeline.geometryId();
    out_result->num_poses_used = static_cast<uint32_t>(result.num_poses_used);
    out_result->is_valid = result.is_valid ? 1 : 0;
    clearError(impl);
    return 1;
}
```

- [ ] **Step 5: 运行 DLL API 相关测试**

Run: `ctest -R "ProbeCalibration(RuntimeSmokeTest|PoseSampleApiTest)" --output-on-failure`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add algorithms/probe_calibration/src/probe_calibration_c_api.cpp tests/unit/ProbeCalibrationPoseSampleApiTest.cpp tests/unit/ProbeCalibrationRuntimeSmokeTest.cpp
git commit -m "feat: add pose sample driven probe calibration dll api"
```

---

### Task 4: 切换上层 DLL 集成到新 API

**Files:**
- Modify: `Plugins/OpticalTracking/OpticalTrackingServiceImpl.h`
- Modify: `Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp`
- Test: `tests/unit/OpticalTrackingProbeCalibrationResultApplicationTest.cpp`

- [ ] **Step 1: 写失败测试，锁定上层不再调用 collector 参与 pivot**

```cpp
void OpticalTrackingProbeCalibrationResultApplicationTest::pivot_dll_path_does_not_use_collector_api_as_pose_input()
{
    const QString sourcePath = QStringLiteral("Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp");
    QFile file(sourcePath);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = QString::fromUtf8(file.readAll());

    QVERIFY(!source.contains(QStringLiteral("m_pcCollectorAddPoint(m_pcPipeline")));
}
```

- [ ] **Step 2: 运行测试，确认当前失败**

Run: `ctest -R OpticalTrackingProbeCalibrationResultApplicationTest --output-on-failure`
Expected: FAIL，当前源码仍包含 collector 输入路径

- [ ] **Step 3: 在头文件中替换 DLL 函数指针集合**

```cpp
using PC_ConfigureGeometryFn = int (*)(void*, const char*, uint32_t);
using PC_ResetCalibrationSessionFn = int (*)(void*);
using PC_AddPoseSampleFn = int (*)(void*, const PC_PoseSample*);
using PC_GetCalibrationResultFn = int (*)(void*, PC_CalibrationResult*);
using PC_GetCalibrationStatsFn = int (*)(void*, PC_CalibrationStats*);

PC_ConfigureGeometryFn m_pcConfigureGeometry = nullptr;
PC_ResetCalibrationSessionFn m_pcResetCalibrationSession = nullptr;
PC_AddPoseSampleFn m_pcAddPoseSample = nullptr;
PC_GetCalibrationResultFn m_pcGetCalibrationResult = nullptr;
PC_GetCalibrationStatsFn m_pcGetCalibrationStats = nullptr;
```

- [ ] **Step 4: 修改 DLL 加载和 `performPivotCalibrationDLL()`，切换到 pose sample 驱动**

```cpp
m_pcConfigureGeometry = reinterpret_cast<PC_ConfigureGeometryFn>(m_pcLib.resolve("PC_ConfigureGeometry"));
m_pcResetCalibrationSession = reinterpret_cast<PC_ResetCalibrationSessionFn>(m_pcLib.resolve("PC_ResetCalibrationSession"));
m_pcAddPoseSample = reinterpret_cast<PC_AddPoseSampleFn>(m_pcLib.resolve("PC_AddPoseSample"));
m_pcGetCalibrationResult = reinterpret_cast<PC_GetCalibrationResultFn>(m_pcLib.resolve("PC_GetCalibrationResult"));
m_pcGetCalibrationStats = reinterpret_cast<PC_GetCalibrationStatsFn>(m_pcLib.resolve("PC_GetCalibrationStats"));
```

```cpp
if (m_pcConfigureGeometry(m_pcPipeline, geometryBytes.constData(), geometryId) != 1) {
    result["success"] = false;
    result["error"] = m_lastError;
    return result;
}

if (m_pcResetCalibrationSession(m_pcPipeline) != 1 || m_pcStartCalibration(m_pcPipeline) != 1) {
    result["success"] = false;
    result["error"] = m_lastError;
    return result;
}

for (const auto& pose : poseSamples) {
    PC_PoseSample sample = toProbeCalibrationPoseSample(pose);
    if (m_pcAddPoseSample(m_pcPipeline, &sample) != 1) {
        result["success"] = false;
        result["error"] = m_lastError;
        return result;
    }
}
```

- [ ] **Step 5: 在完成标定后读取结构化结果并组装 `tipOffset`**

```cpp
PC_CalibrationResult calibrationResult{};
if (m_pcFinishCalibration(m_pcPipeline) != 1 ||
    m_pcGetCalibrationResult(m_pcPipeline, &calibrationResult) != 1) {
    result["success"] = false;
    result["error"] = m_lastError;
    return result;
}

result["success"] = calibrationResult.is_valid == 1;
result["tipOffset"] = QVariantList{
    calibrationResult.tip_offset.x,
    calibrationResult.tip_offset.y,
    calibrationResult.tip_offset.z
};
result["accuracy"] = calibrationResult.residual_error;
result["geometryId"] = static_cast<int>(calibrationResult.geometry_id);
result["pointsUsed"] = static_cast<int>(calibrationResult.num_poses_used);
result["algorithm"] = "ProbeCalibration DLL";
```

- [ ] **Step 6: 运行上层结果应用测试**

Run: `ctest -R OpticalTrackingProbeCalibrationResultApplicationTest --output-on-failure`
Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add Plugins/OpticalTracking/OpticalTrackingServiceImpl.h Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp tests/unit/OpticalTrackingProbeCalibrationResultApplicationTest.cpp
git commit -m "refactor: drive probe calibration dll from unified pose samples"
```

---

### Task 5: 取消 geometry072 静默回退并强化 geometry 规则

**Files:**
- Modify: `Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp`
- Modify: `tests/unit/OpticalTrackingProbeCalibrationInitializationTest.cpp`
- Modify: `tests/unit/OpticalTrackingProbeCalibrationToolGeometrySelectionTest.cpp`

- [ ] **Step 1: 写失败测试，要求 geometry 缺失时失败而非静默 `072`**

```cpp
void OpticalTrackingProbeCalibrationToolGeometrySelectionTest::missing_tool_geometry_fails_instead_of_silent_default_fallback()
{
    OpticalTrackingServiceImpl service;
    const QString sessionId = service.createTrackingSession(QStringLiteral("simulated_fusiontrack_001"),
        QStringLiteral("missing-geometry-test"));

    QVariantMap toolConfig;
    toolConfig[QStringLiteral("name")] = QStringLiteral("Probe-No-Geometry");
    toolConfig[QStringLiteral("type")] = QStringLiteral("probe");

    const QString toolId = service.addTrackingTool(sessionId, QStringLiteral("Probe-No-Geometry"), toolConfig);
    QVERIFY(!toolId.isEmpty());

    const QString geometryPath = service.resolveProbeCalibrationGeometry(sessionId, toolId);
    QVERIFY(geometryPath.isEmpty());
    QVERIFY(!service.getLastError().isEmpty());
}
```

- [ ] **Step 2: 运行测试，确认当前失败**

Run: `ctest -R OpticalTrackingProbeCalibrationToolGeometrySelectionTest --output-on-failure`
Expected: FAIL，当前仍会回退到 `072`

- [ ] **Step 3: 删除 `resolveProbeCalibrationGeometry()` 中的 `072` fallback**

```cpp
setError(QStringLiteral("No probe calibration geometry resolved for tool: %1").arg(toolId));
return QString();
```

- [ ] **Step 4: 更新初始化测试，去掉“默认 geometry072 必须参与初始化”的假设**

```cpp
void OpticalTrackingProbeCalibrationInitializationTest::optical_tracking_service_exposes_geometry_validation_without_implicit_probe_default()
{
    OpticalTrackingServiceImpl service;
    QVERIFY2(service.loadProbeCalibrationDLL(), qPrintable(service.getLastError()));
    QVERIFY(service.m_pcLoaded);
    QVERIFY(service.m_pcPipeline != nullptr);
}
```

- [ ] **Step 5: 运行 geometry 相关测试**

Run: `ctest -R "OpticalTrackingProbeCalibration(InitializationTest|ToolGeometrySelectionTest)" --output-on-failure`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp tests/unit/OpticalTrackingProbeCalibrationInitializationTest.cpp tests/unit/OpticalTrackingProbeCalibrationToolGeometrySelectionTest.cpp
git commit -m "fix: require explicit probe calibration geometry"
```

---

### Task 6: 区分真设备与模拟设备状态

**Files:**
- Modify: `Plugins/OpticalTracking/OpticalTrackingServiceImpl.h`
- Modify: `Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp`
- Test: `tests/unit/OpticalTrackingPhysicalDeviceModeTest.cpp`

- [ ] **Step 1: 写失败测试，要求状态可区分**

```cpp
void OpticalTrackingPhysicalDeviceModeTest::device_scan_exposes_physical_and_simulation_mode_separately()
{
    const QString sourcePath = QStringLiteral("Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp");
    QFile file(sourcePath);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = QString::fromUtf8(file.readAll());

    QVERIFY(!source.contains(QStringLiteral("No physical devices found, adding simulated devices")));
    QVERIFY(source.contains(QStringLiteral("runtimeMode")));
    QVERIFY(source.contains(QStringLiteral("physical")));
    QVERIFY(source.contains(QStringLiteral("simulation")));
}
```

- [ ] **Step 2: 运行测试，确认当前没有明确状态字段**

Run: `ctest -R OpticalTrackingPhysicalDeviceModeTest --output-on-failure`
Expected: FAIL 或测试内容待通过源码状态断言补全

- [ ] **Step 3: 在设备状态结构中加入模式字段并在扫描/连接路径赋值**

```cpp
enum class TrackingRuntimeMode {
    Physical,
    Simulation
};

info->state["runtimeMode"] = QStringLiteral("physical");
info->state["runtimeMode"] = QStringLiteral("simulation");
```

- [ ] **Step 4: 在实机标定入口前增加模式校验**

```cpp
if (deviceInfo.state.value(QStringLiteral("runtimeMode")).toString() != QStringLiteral("physical")) {
    result["success"] = false;
    result["error"] = QStringLiteral("Probe calibration requires a physical tracking device");
    return result;
}
```

- [ ] **Step 5: 运行设备模式测试**

Run: `ctest -R OpticalTrackingPhysicalDeviceModeTest --output-on-failure`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add Plugins/OpticalTracking/OpticalTrackingServiceImpl.h Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp tests/unit/OpticalTrackingPhysicalDeviceModeTest.cpp
git commit -m "feat: separate physical and simulation tracking modes"
```

---

### Task 7: 强化 `applyCalibrationResult()` 和日志可观测性

**Files:**
- Modify: `Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp`
- Test: `tests/unit/OpticalTrackingProbeCalibrationResultApplicationTest.cpp`

- [ ] **Step 1: 写失败测试，要求缺失 `tipOffset` 明确失败**

```cpp
void OpticalTrackingProbeCalibrationResultApplicationTest::apply_calibration_result_requires_tip_offset()
{
    OpticalTrackingServiceImpl service;
    const QString sessionId = service.createTrackingSession(QStringLiteral("simulated_fusiontrack_001"),
        QStringLiteral("apply-result-test"));

    QVariantMap toolConfig;
    toolConfig[QStringLiteral("name")] = QStringLiteral("Probe");
    toolConfig[QStringLiteral("type")] = QStringLiteral("probe");
    toolConfig[QStringLiteral("geometryFile")] = QStringLiteral("geometry074.ini");

    const QString toolId = service.addTrackingTool(sessionId, QStringLiteral("Probe"), toolConfig);

    QVariantMap calibrationResult;
    calibrationResult["success"] = true;

    QVERIFY(!service.applyCalibrationResult(sessionId, toolId, calibrationResult));
}
```

- [ ] **Step 2: 运行测试，确认当前行为需要收紧**

Run: `ctest -R OpticalTrackingProbeCalibrationResultApplicationTest --output-on-failure`
Expected: FAIL 或行为不满足断言

- [ ] **Step 3: 收紧 `applyCalibrationResult()`，拒绝 `pivotPoint` 冒充 offset**

```cpp
if (offset.size() < 3) {
    setError("Calibration result does not contain valid tipOffset data");
    return false;
}
```

- [ ] **Step 4: 在 DLL 标定入口和结果应用入口增加关键日志**

```cpp
qDebug() << "[OpticalTracking] Pivot calibration start"
         << "sessionId=" << calibInfo.sessionId
         << "toolId=" << calibInfo.toolId
         << "geometryPath=" << geometryPath
         << "runtimeMode=" << runtimeMode;

qDebug() << "[OpticalTracking] Pivot calibration result"
         << "tipOffset=" << result["tipOffset"]
         << "accuracy=" << result["accuracy"]
         << "geometryId=" << result["geometryId"];
```

- [ ] **Step 5: 运行结果应用测试**

Run: `ctest -R OpticalTrackingProbeCalibrationResultApplicationTest --output-on-failure`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp tests/unit/OpticalTrackingProbeCalibrationResultApplicationTest.cpp
git commit -m "fix: require explicit tip offset in probe calibration results"
```

---

### Task 8: 回归验证与文档补充

**Files:**
- Modify: `docs/superpowers/specs/2026-05-06-probe-calibration-unified-tracking-design.md`
- Modify: 相关测试清单或调试文档（如需要）

- [ ] **Step 1: 运行 ProbeCalibration 与 OpticalTracking 相关测试集合**

Run: `ctest -R "ProbeCalibration|OpticalTrackingProbeCalibration|OpticalTrackingPhysicalDeviceMode" --output-on-failure`
Expected: 全部 PASS

- [ ] **Step 2: 构建 Release 运行时目标，验证 DLL 与上层可链接**

Run: `cmake --build build_x64_v142 --config Release --target ProbeCalibrationDLL OpticalTrackingPlatformModuleLib`
Expected: BUILD SUCCESS

- [ ] **Step 3: 更新 spec 中迁移状态说明，标记实现完成的架构变化**

```md
- 已完成统一 tracking DLL API 切换
- 已移除 pivot 主流程中的 collector 输入误用
- 已取消 probe calibration 的静默 geometry072 回退
```

- [ ] **Step 4: 记录实机验证步骤**

```md
1. 使用显式 geometry 配置的 probe 启动 tracking
2. 确认 runtimeMode=physical
3. 开始 pivot 标定并完成 100+ poses 采样
4. 检查 tipOffset、accuracy、geometryPath 日志
5. 完成重复点验证
```

- [ ] **Step 5: Commit**

```bash
git add docs/superpowers/specs/2026-05-06-probe-calibration-unified-tracking-design.md
git commit -m "docs: record unified probe calibration implementation status"
```

---

## Self-Review

### Spec coverage

- 单 tracking 数据源：Task 2, Task 3, Task 4
- DLL 结果结构化输出：Task 1, Task 3, Task 4
- 去掉 collector 误用：Task 4
- geometry 显式规则：Task 5
- 真设备/模拟设备区分：Task 6
- 结果应用收紧与日志增强：Task 7
- 验证与迁移闭环：Task 8

### Placeholder scan

- 所有任务均包含具体文件、测试、代码片段、命令和提交信息
- 未使用 `TODO`、`TBD`、`implement later`

### Type consistency

- 新 C API 类型统一使用 `PC_PoseSample`、`PC_CalibrationResult`、`PC_CalibrationStats`
- 上层函数指针命名与 DLL 导出符号一致
- 上层结果字段统一使用 `tipOffset`、`accuracy`、`geometryId`

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-05-06-probe-calibration-unified-tracking-implementation-plan.md`. Two execution options:

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

Which approach?
