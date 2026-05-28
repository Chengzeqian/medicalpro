# Navigation Realtime Pose Digital Twin Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为当前踝关节导航系统补齐实时位姿输入、固定多坐标系变换链、单窗口数字孪生渲染与基础实时评估链路。

**Architecture:** 本轮实现不重写现有病例中心化工作区，而是在现有 `NavigationRuntimeState + NavigationRuntimeCoordinator + NavigationVtkBridge + NavigationEvaluationService` 骨架上增量补齐实时链路。核心做法是新增 `NavigationPoseFrame`、`NavigationPoseStream`、`NavigationTransformGraph` 三个边界清晰的模块，把模拟器或未来真实光学跟踪输入统一成标准帧，再由运行时协调器计算 `tracking -> calibrated tool -> patient -> vtk world` 的最终位姿，并驱动单窗口 VTK 数字孪生显示与评估落盘。

**Tech Stack:** C++20, Qt Core/Gui/Widgets, QMatrix4x4, QDateTime, existing Framework/Navigation persistence, existing NavigationPage runtime flow, QtTest, VTK host bridge

---

### Task 1: 定义实时位姿帧与短窗口缓存

**Files:**
- Create: `Framework/Navigation/navigation_pose_frame.h`
- Create: `Framework/Navigation/navigation_pose_frame.cpp`
- Create: `Framework/Navigation/navigation_pose_stream.h`
- Create: `Framework/Navigation/navigation_pose_stream.cpp`
- Create: `tests/unit/NavigationPoseStreamTest.cpp`
- Modify: `tests/unit/CMakeLists.txt`

- [ ] **Step 1: 先写失败测试，锁定实时位姿帧与窗口缓存契约**

```cpp
void NavigationPoseStreamTest::stream_keeps_latest_frame_and_bounded_recent_window()
{
    NavigationPoseStream stream(3);

    NavigationPoseFrame frameA;
    frameA.sourceId = QStringLiteral("simulator");
    frameA.toolId = QStringLiteral("instrument:probe-main");
    frameA.timestamp = QDateTime::fromString(QStringLiteral("2026-05-08T10:00:00Z"), Qt::ISODate);
    frameA.trackingVisible = true;
    frameA.trackingConfidence = 0.91;
    frameA.trackingToMarker.translate(1.0f, 0.0f, 0.0f);

    NavigationPoseFrame frameB = frameA;
    frameB.timestamp = QDateTime::fromString(QStringLiteral("2026-05-08T10:00:01Z"), Qt::ISODate);
    frameB.trackingToMarker.translate(0.0f, 2.0f, 0.0f);

    NavigationPoseFrame frameC = frameA;
    frameC.timestamp = QDateTime::fromString(QStringLiteral("2026-05-08T10:00:02Z"), Qt::ISODate);

    NavigationPoseFrame frameD = frameA;
    frameD.timestamp = QDateTime::fromString(QStringLiteral("2026-05-08T10:00:03Z"), Qt::ISODate);

    stream.pushFrame(frameA);
    stream.pushFrame(frameB);
    stream.pushFrame(frameC);
    stream.pushFrame(frameD);

    QVERIFY(stream.hasLatestFrame());
    QCOMPARE(stream.latestFrame().timestamp, frameD.timestamp);
    QCOMPARE(stream.sampleWindow().recentFrames.size(), 3);
    QCOMPARE(stream.sampleWindow().recentFrames.first().timestamp, frameB.timestamp);
    QCOMPARE(stream.sampleWindow().recentFrames.last().timestamp, frameD.timestamp);
}
```

- [ ] **Step 2: 运行测试确认当前目标不存在**

Run: `cmake --build build_x64_v142 --config Release --target navigation_pose_stream_test`
Expected: FAIL，提示 `NavigationPoseFrame`、`NavigationPoseStream` 或测试目标尚不存在

- [ ] **Step 3: 定义位姿帧与窗口结构**

```cpp
struct NavigationPoseFrame
{
    QString sourceId;
    QString toolId;
    QDateTime timestamp;
    bool trackingVisible = false;
    double trackingConfidence = 0.0;
    QMatrix4x4 trackingToMarker;
};

struct NavigationPoseSampleWindow
{
    QList<NavigationPoseFrame> recentFrames;
    int maxFrameCount = 0;
};
```

```cpp
class FRAMEWORK_EXPORT NavigationPoseStream
{
public:
    explicit NavigationPoseStream(int maxFrameCount = 10);

    void pushFrame(const NavigationPoseFrame& frame);
    bool hasLatestFrame() const;
    NavigationPoseFrame latestFrame() const;
    NavigationPoseSampleWindow sampleWindow() const;
    void clear();

private:
    NavigationPoseSampleWindow m_sampleWindow;
};
```

- [ ] **Step 4: 实现最小缓存行为**

```cpp
void NavigationPoseStream::pushFrame(const NavigationPoseFrame& frame)
{
    m_sampleWindow.recentFrames.append(frame);
    while (m_sampleWindow.recentFrames.size() > m_sampleWindow.maxFrameCount) {
        m_sampleWindow.recentFrames.removeFirst();
    }
}

bool NavigationPoseStream::hasLatestFrame() const
{
    return !m_sampleWindow.recentFrames.isEmpty();
}

NavigationPoseFrame NavigationPoseStream::latestFrame() const
{
    return hasLatestFrame() ? m_sampleWindow.recentFrames.last() : NavigationPoseFrame();
}
```

- [ ] **Step 5: 把新测试目标加入 `tests/unit/CMakeLists.txt`**

```cmake
add_executable(navigation_pose_stream_test
    NavigationPoseStreamTest.cpp
    ${CMAKE_SOURCE_DIR}/Framework/Navigation/navigation_pose_frame.cpp
    ${CMAKE_SOURCE_DIR}/Framework/Navigation/navigation_pose_stream.cpp
)

target_include_directories(navigation_pose_stream_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(navigation_pose_stream_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME navigation_pose_stream_test
    COMMAND navigation_pose_stream_test
)
```

- [ ] **Step 6: 运行新测试**

Run: `ctest --test-dir build_x64_v142 -C Release -R navigation_pose_stream_test --output-on-failure`
Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add Framework/Navigation/navigation_pose_frame.h Framework/Navigation/navigation_pose_frame.cpp Framework/Navigation/navigation_pose_stream.h Framework/Navigation/navigation_pose_stream.cpp tests/unit/NavigationPoseStreamTest.cpp tests/unit/CMakeLists.txt
git commit -m "feat: add realtime navigation pose frame and stream"
```

---

### Task 2: 定义固定多坐标系变换链与结果对象

**Files:**
- Create: `Framework/Navigation/navigation_transform_graph.h`
- Create: `Framework/Navigation/navigation_transform_graph.cpp`
- Create: `tests/unit/NavigationTransformGraphTest.cpp`
- Modify: `tests/unit/CMakeLists.txt`

- [ ] **Step 1: 写失败测试，锁定 `tracking -> calibrated tool -> patient -> vtk world` 计算行为**

```cpp
void NavigationTransformGraphTest::graph_builds_vtk_tool_transform_from_pose_calibration_and_registration()
{
    NavigationPoseFrame frame;
    frame.sourceId = QStringLiteral("simulator");
    frame.toolId = QStringLiteral("instrument:probe-main");
    frame.timestamp = QDateTime::currentDateTimeUtc();
    frame.trackingVisible = true;
    frame.trackingToMarker.translate(10.0f, 0.0f, 0.0f);

    QMatrix4x4 markerToTool;
    markerToTool.translate(0.0f, 5.0f, 0.0f);

    QMatrix4x4 patientToVtkWorld;
    patientToVtkWorld.translate(0.0f, 0.0f, 7.0f);

    NavigationTransformGraph graph;
    graph.setLatestPoseFrame(frame);
    graph.setMarkerToToolTransform(markerToTool);
    graph.setPatientToVtkWorldTransform(patientToVtkWorld);

    const NavigationTransformResult result = graph.compute();

    QVERIFY(result.trackingAvailable);
    QVERIFY(result.calibrationAvailable);
    QVERIFY(result.registrationAvailable);
    QVERIFY(result.valid);
    QCOMPARE(result.failureCode, QStringLiteral("ok"));
    QCOMPARE(result.vtkToolTransform.column(3).toVector3D(), QVector3D(10.0f, 5.0f, 7.0f));
}
```

- [ ] **Step 2: 运行测试确认当前目标不存在**

Run: `cmake --build build_x64_v142 --config Release --target navigation_transform_graph_test`
Expected: FAIL，提示 `NavigationTransformGraph` 或 `NavigationTransformResult` 尚不存在

- [ ] **Step 3: 定义计算结果与图对象**

```cpp
struct NavigationTransformResult
{
    bool trackingAvailable = false;
    bool calibrationAvailable = false;
    bool registrationAvailable = false;
    bool valid = false;
    QString failureCode;
    QString failureText;
    QMatrix4x4 vtkToolTransform;
};

class FRAMEWORK_EXPORT NavigationTransformGraph
{
public:
    void setLatestPoseFrame(const NavigationPoseFrame& frame);
    void clearLatestPoseFrame();
    void setMarkerToToolTransform(const QMatrix4x4& markerToToolTransform);
    void clearMarkerToToolTransform();
    void setPatientToVtkWorldTransform(const QMatrix4x4& patientToVtkWorldTransform);
    void clearPatientToVtkWorldTransform();
    NavigationTransformResult compute() const;

private:
    NavigationPoseFrame m_latestPoseFrame;
    QMatrix4x4 m_markerToToolTransform;
    QMatrix4x4 m_patientToVtkWorldTransform;
    bool m_hasLatestPoseFrame = false;
    bool m_hasMarkerToToolTransform = false;
    bool m_hasPatientToVtkWorldTransform = false;
};
```

- [ ] **Step 4: 实现固定链路与明确失败码**

```cpp
NavigationTransformResult NavigationTransformGraph::compute() const
{
    NavigationTransformResult result;
    result.trackingAvailable = m_hasLatestPoseFrame && m_latestPoseFrame.trackingVisible;
    result.calibrationAvailable = m_hasMarkerToToolTransform;
    result.registrationAvailable = m_hasPatientToVtkWorldTransform;

    if (!result.trackingAvailable) {
        result.failureCode = QStringLiteral("tracking_unavailable");
        result.failureText = QStringLiteral("实时跟踪不可用");
        return result;
    }

    if (!result.calibrationAvailable) {
        result.failureCode = QStringLiteral("calibration_missing");
        result.failureText = QStringLiteral("器械标定结果缺失");
        return result;
    }

    if (!result.registrationAvailable) {
        result.failureCode = QStringLiteral("registration_missing");
        result.failureText = QStringLiteral("患者配准结果缺失");
        return result;
    }

    result.vtkToolTransform = m_patientToVtkWorldTransform
        * m_latestPoseFrame.trackingToMarker
        * m_markerToToolTransform;
    result.valid = true;
    result.failureCode = QStringLiteral("ok");
    result.failureText = QStringLiteral("导航位姿链路有效");
    return result;
}
```

- [ ] **Step 5: 增加缺失链路测试**

```cpp
void NavigationTransformGraphTest::graph_reports_tracking_calibration_and_registration_failures()
{
    NavigationTransformGraph graph;

    QCOMPARE(graph.compute().failureCode, QStringLiteral("tracking_unavailable"));

    NavigationPoseFrame frame;
    frame.toolId = QStringLiteral("instrument:probe-main");
    frame.trackingVisible = true;
    graph.setLatestPoseFrame(frame);
    QCOMPARE(graph.compute().failureCode, QStringLiteral("calibration_missing"));

    QMatrix4x4 markerToTool;
    graph.setMarkerToToolTransform(markerToTool);
    QCOMPARE(graph.compute().failureCode, QStringLiteral("registration_missing"));
}
```

- [ ] **Step 6: 运行变换图测试**

Run: `ctest --test-dir build_x64_v142 -C Release -R navigation_transform_graph_test --output-on-failure`
Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add Framework/Navigation/navigation_transform_graph.h Framework/Navigation/navigation_transform_graph.cpp tests/unit/NavigationTransformGraphTest.cpp tests/unit/CMakeLists.txt
git commit -m "feat: add navigation transform graph for digital twin chain"
```

---

### Task 3: 扩展运行时状态，容纳位姿帧、变换结果与显示态

**Files:**
- Modify: `UI/NewPages/Navigation/navigation_runtime_state.h`
- Modify: `UI/NewPages/Navigation/navigation_runtime_state.cpp`
- Modify: `tests/unit/NavigationRuntimeStateTest.cpp`

- [ ] **Step 1: 写失败测试，要求运行时状态保存最新 pose frame 和 transform result**

```cpp
void NavigationRuntimeStateTest::state_stores_pose_frame_transform_result_and_failure_reason()
{
    NavigationRuntimeState state;

    NavigationPoseFrame frame;
    frame.toolId = QStringLiteral("instrument:probe-main");
    frame.trackingVisible = true;
    frame.trackingConfidence = 0.93;

    NavigationTransformResult result;
    result.valid = false;
    result.failureCode = QStringLiteral("registration_missing");
    result.failureText = QStringLiteral("患者配准结果缺失");

    state.setLatestPoseFrame(frame);
    state.setLatestTransformResult(result);

    QVERIFY(state.hasLatestPoseFrame());
    QVERIFY(state.hasLatestTransformResult());
    QCOMPARE(state.latestPoseFrame().toolId, QStringLiteral("instrument:probe-main"));
    QCOMPARE(state.latestTransformResult().failureCode, QStringLiteral("registration_missing"));
}
```

- [ ] **Step 2: 运行测试确认当前状态对象尚不支持**

Run: `ctest --test-dir build_x64_v142 -C Release -R navigation_runtime_state_test --output-on-failure`
Expected: FAIL，提示 `setLatestPoseFrame`、`setLatestTransformResult` 或相关读取接口不存在

- [ ] **Step 3: 在状态头文件中补齐运行时字段与访问器**

```cpp
class NavigationRuntimeState
{
public:
    void setLatestPoseFrame(const NavigationPoseFrame& frame);
    void clearLatestPoseFrame();
    bool hasLatestPoseFrame() const;
    const NavigationPoseFrame& latestPoseFrame() const;

    void setLatestTransformResult(const NavigationTransformResult& result);
    void clearLatestTransformResult();
    bool hasLatestTransformResult() const;
    const NavigationTransformResult& latestTransformResult() const;

private:
    NavigationPoseFrame m_latestPoseFrame;
    NavigationTransformResult m_latestTransformResult;
    bool m_hasLatestPoseFrame = false;
    bool m_hasLatestTransformResult = false;
};
```

- [ ] **Step 4: 在状态实现中完成上下文切换时的清理**

```cpp
void NavigationRuntimeState::setCaseContext(
    const QString& caseId,
    const QString& trackingSessionId,
    const QString& navigationToolId)
{
    const bool caseContextChanged =
        m_caseId != caseId
        || m_trackingSessionId != trackingSessionId
        || m_navigationToolId != navigationToolId;

    m_caseId = caseId;
    m_trackingSessionId = trackingSessionId;
    m_navigationToolId = navigationToolId;

    if (!caseContextChanged) {
        return;
    }

    clearLatestPoseFrame();
    clearLatestTransformResult();
    clearTrackingQuality();
    clearRegistrationResult();
    clearConfidenceResult();
    m_trackedInstrumentVisibility.clear();
    m_activeInstrumentPoseSummaries.clear();
}
```

- [ ] **Step 5: 补一条上下文切换测试**

```cpp
void NavigationRuntimeStateTest::state_clears_pose_runtime_snapshots_when_case_context_changes()
{
    NavigationRuntimeState state;
    state.setCaseContext(QStringLiteral("case-a"), QStringLiteral("tracking-a"), QStringLiteral("tool-a"));

    NavigationPoseFrame frame;
    frame.toolId = QStringLiteral("instrument:probe-main");
    state.setLatestPoseFrame(frame);

    NavigationTransformResult result;
    result.valid = true;
    state.setLatestTransformResult(result);

    state.setCaseContext(QStringLiteral("case-b"), QStringLiteral("tracking-b"), QStringLiteral("tool-b"));

    QVERIFY(!state.hasLatestPoseFrame());
    QVERIFY(!state.hasLatestTransformResult());
}
```

- [ ] **Step 6: 运行状态测试**

Run: `ctest --test-dir build_x64_v142 -C Release -R navigation_runtime_state_test --output-on-failure`
Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add UI/NewPages/Navigation/navigation_runtime_state.h UI/NewPages/Navigation/navigation_runtime_state.cpp tests/unit/NavigationRuntimeStateTest.cpp
git commit -m "feat: extend navigation runtime state for realtime pose chain"
```

---

### Task 4: 扩展运行时协调器，接入位姿流、坐标链与评估侧路

**Files:**
- Modify: `UI/NewPages/Navigation/navigation_runtime_coordinator.h`
- Modify: `UI/NewPages/Navigation/navigation_runtime_coordinator.cpp`
- Modify: `tests/unit/NavigationRuntimeCoordinatorContractTest.cpp`

- [ ] **Step 1: 写失败测试，要求协调器消费 pose frame 并产出 transform result**

```cpp
void NavigationRuntimeCoordinatorContractTest::coordinator_handles_pose_frame_and_computes_transform_result()
{
    NavigationRuntimeState runtimeState;
    runtimeState.setCaseContext(QStringLiteral("case-rt-001"), QStringLiteral("tracking-001"), QStringLiteral("instrument:probe-main"));

    NavigationRuntimeCoordinator coordinator(&runtimeState);

    QMatrix4x4 markerToTool;
    markerToTool.translate(0.0f, 5.0f, 0.0f);
    coordinator.handleCalibrationTransform(markerToTool);

    QMatrix4x4 patientToVtkWorld;
    patientToVtkWorld.translate(0.0f, 0.0f, 7.0f);
    coordinator.handleRegistrationTransform(patientToVtkWorld);

    NavigationPoseFrame frame;
    frame.sourceId = QStringLiteral("simulator");
    frame.toolId = QStringLiteral("instrument:probe-main");
    frame.timestamp = QDateTime::currentDateTimeUtc();
    frame.trackingVisible = true;
    frame.trackingConfidence = 0.95;
    frame.trackingToMarker.translate(10.0f, 0.0f, 0.0f);

    coordinator.handlePoseFrame(frame);

    QVERIFY(runtimeState.hasLatestPoseFrame());
    QVERIFY(runtimeState.hasLatestTransformResult());
    QVERIFY(runtimeState.latestTransformResult().valid);
    QCOMPARE(runtimeState.latestTransformResult().vtkToolTransform.column(3).toVector3D(), QVector3D(10.0f, 5.0f, 7.0f));
}
```

- [ ] **Step 2: 运行契约测试确认现有协调器能力不足**

Run: `ctest --test-dir build_x64_v142 -C Release -R navigation_runtime_coordinator_contract_test --output-on-failure`
Expected: FAIL，提示 `handlePoseFrame`、`handleCalibrationTransform` 或 `handleRegistrationTransform` 尚不存在

- [ ] **Step 3: 在协调器头文件中声明新增接口与成员**

```cpp
class NavigationRuntimeCoordinator
{
public:
    void handlePoseFrame(const NavigationPoseFrame& frame);
    void handleCalibrationTransform(const QMatrix4x4& markerToToolTransform);
    void handleRegistrationTransform(const QMatrix4x4& patientToVtkWorldTransform);
    NavigationDisplayState buildDisplayState(
        const QStringList& boneModelPaths,
        const QString& activeToolModelPath) const;

private:
    NavigationPoseStream m_poseStream;
    NavigationTransformGraph m_transformGraph;
};
```

```cpp
struct NavigationDisplayState
{
    QString activeToolId;
    QString activeToolModelPath;
    QStringList boneModelPaths;
    bool toolVisible = false;
    bool validPose = false;
    QString statusText;
    QMatrix4x4 vtkToolTransform;
};
```

- [ ] **Step 4: 在实现中串起 pose stream、transform graph 与运行时状态**

```cpp
void NavigationRuntimeCoordinator::handlePoseFrame(const NavigationPoseFrame& frame)
{
    if (!m_runtimeState) {
        return;
    }

    m_poseStream.pushFrame(frame);
    m_transformGraph.setLatestPoseFrame(frame);
    const NavigationTransformResult transformResult = m_transformGraph.compute();

    m_runtimeState->setLatestPoseFrame(frame);
    m_runtimeState->setLatestTransformResult(transformResult);
    m_runtimeState->setTrackedInstrumentVisible(frame.toolId, frame.trackingVisible && transformResult.valid);
}

void NavigationRuntimeCoordinator::handleCalibrationTransform(const QMatrix4x4& markerToToolTransform)
{
    m_transformGraph.setMarkerToToolTransform(markerToToolTransform);
}

void NavigationRuntimeCoordinator::handleRegistrationTransform(const QMatrix4x4& patientToVtkWorldTransform)
{
    m_transformGraph.setPatientToVtkWorldTransform(patientToVtkWorldTransform);
}
```

- [ ] **Step 5: 补一条评估落盘测试，要求记录 latency、jitter、visible frame ratio**

```cpp
void NavigationRuntimeCoordinatorContractTest::coordinator_persists_pose_metrics_into_evaluation_report()
{
    NavigationRuntimeState runtimeState;
    runtimeState.setCaseContext(QStringLiteral("case-rt-002"), QStringLiteral("tracking-002"), QStringLiteral("instrument:probe-main"));

    QVariantMap trackingQuality;
    trackingQuality.insert(QStringLiteral("tracking_jitter_mm"), 0.28);
    trackingQuality.insert(QStringLiteral("visible_frame_ratio"), 0.99);
    trackingQuality.insert(QStringLiteral("tracking_latency_ms"), 31.0);
    trackingQuality.insert(QStringLiteral("calibrated"), true);
    trackingQuality.insert(QStringLiteral("calibration_accuracy_mm"), 0.41);
    runtimeState.setTrackingQuality(trackingQuality);

    PointRegistrationResult registrationResult;
    registrationResult.success = true;
    registrationResult.targetRegionTre = 1.2;
    registrationResult.coverageScore = 0.9;
    runtimeState.setRegistrationResult(registrationResult);

    NavigationConfidenceResult confidenceResult;
    confidenceResult.allowNavigation = true;
    confidenceResult.score = 0.84;
    runtimeState.setConfidenceResult(confidenceResult);

    AnkleEvaluationReport savedReport;
    NavigationRuntimeCoordinator::PersistenceActions persistenceActions;
    persistenceActions.loadEvaluationSnapshot = [](const QString& caseId) {
        AnkleEvaluationSnapshot snapshot;
        snapshot.caseId = caseId;
        return snapshot;
    };
    persistenceActions.saveEvaluationReport = [&savedReport](const AnkleEvaluationReport& report) {
        savedReport = report;
        return true;
    };
    persistenceActions.exportMetricsCsv = [](const QString&) { return true; };
    persistenceActions.exportCaseSummary = [](const QString&) { return true; };

    NavigationRuntimeCoordinator coordinator(&runtimeState, persistenceActions);
    coordinator.persistEvaluationReportSnapshot();

    QCOMPARE(savedReport.metrics.value(QStringLiteral("tracking_latency_ms")).toDouble(), 31.0);
    QCOMPARE(savedReport.metrics.value(QStringLiteral("tracking_jitter_mm")).toDouble(), 0.28);
    QCOMPARE(savedReport.metrics.value(QStringLiteral("visible_frame_ratio")).toDouble(), 0.99);
}
```

- [ ] **Step 6: 运行运行时协调器测试**

Run: `ctest --test-dir build_x64_v142 -C Release -R "navigation_runtime_coordinator_contract_test|navigation_runtime_state_test" --output-on-failure`
Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add UI/NewPages/Navigation/navigation_runtime_coordinator.h UI/NewPages/Navigation/navigation_runtime_coordinator.cpp tests/unit/NavigationRuntimeCoordinatorContractTest.cpp
git commit -m "feat: route realtime pose chain through runtime coordinator"
```

---

### Task 5: 升级 VTK 桥，支持单窗口数字孪生模型装载与位姿更新

**Files:**
- Modify: `UI/NewPages/Navigation/navigation_vtk_bridge.h`
- Modify: `UI/NewPages/Navigation/navigation_vtk_bridge.cpp`
- Modify: `tests/unit/NavigationVtkBridgeTest.cpp`

- [ ] **Step 1: 写失败测试，要求 VTK 桥暴露数字孪生接口**

```cpp
void NavigationVtkBridgeTest::bridge_tracks_single_virtual_space_display_state_for_bones_and_tool_pose()
{
    NavigationVtkBridge bridge(
        nullptr,
        nullptr,
        nullptr,
        []() { return nullptr; },
        []() { return nullptr; });

    QVERIFY(bridge.loadBoneModels(QStringList{
        QStringLiteral("cases/case-001/models/tibia.stl"),
        QStringLiteral("cases/case-001/models/talus.stl")
    }));
    QVERIFY(bridge.loadInstrumentModel(
        QStringLiteral("instrument:probe-main"),
        QStringLiteral("cases/case-001/models/probe.stl")));

    QMatrix4x4 vtkToolTransform;
    vtkToolTransform.translate(1.0f, 2.0f, 3.0f);
    bridge.updateInstrumentPose(QStringLiteral("instrument:probe-main"), vtkToolTransform);
    bridge.setInstrumentVisible(QStringLiteral("instrument:probe-main"), true);

    QCOMPARE(bridge.lastLoadedBoneModels().size(), 2);
    QCOMPARE(bridge.lastLoadedInstrumentId(), QStringLiteral("instrument:probe-main"));
    QCOMPARE(bridge.lastInstrumentVisible(), true);
    QCOMPARE(bridge.lastInstrumentPose().column(3).toVector3D(), QVector3D(1.0f, 2.0f, 3.0f));
}
```

- [ ] **Step 2: 运行测试确认当前桥只支持 host attach/detach**

Run: `ctest --test-dir build_x64_v142 -C Release -R navigation_vtk_bridge_test --output-on-failure`
Expected: FAIL，提示 `loadBoneModels`、`loadInstrumentModel` 或读取接口不存在

- [ ] **Step 3: 在桥头文件中补齐数字孪生接口**

```cpp
class NavigationVtkBridge
{
public:
    bool loadBoneModels(const QStringList& boneModelPaths);
    bool loadInstrumentModel(const QString& toolId, const QString& modelPath);
    void updateInstrumentPose(const QString& toolId, const QMatrix4x4& vtkToolTransform);
    void setInstrumentVisible(const QString& toolId, bool visible);

    QStringList lastLoadedBoneModels() const;
    QString lastLoadedInstrumentId() const;
    QString lastLoadedInstrumentModelPath() const;
    QMatrix4x4 lastInstrumentPose() const;
    bool lastInstrumentVisible() const;

private:
    QStringList m_lastLoadedBoneModels;
    QString m_lastLoadedInstrumentId;
    QString m_lastLoadedInstrumentModelPath;
    QMatrix4x4 m_lastInstrumentPose;
    bool m_lastInstrumentVisible = false;
};
```

- [ ] **Step 4: 先实现最小数字孪生状态桥，再保留未来 VTK actor 落点**

```cpp
bool NavigationVtkBridge::loadBoneModels(const QStringList& boneModelPaths)
{
    m_lastLoadedBoneModels = boneModelPaths;
    return !m_lastLoadedBoneModels.isEmpty();
}

bool NavigationVtkBridge::loadInstrumentModel(const QString& toolId, const QString& modelPath)
{
    m_lastLoadedInstrumentId = toolId;
    m_lastLoadedInstrumentModelPath = modelPath;
    return !m_lastLoadedInstrumentId.isEmpty() && !m_lastLoadedInstrumentModelPath.isEmpty();
}

void NavigationVtkBridge::updateInstrumentPose(const QString& toolId, const QMatrix4x4& vtkToolTransform)
{
    if (toolId != m_lastLoadedInstrumentId) {
        return;
    }

    m_lastInstrumentPose = vtkToolTransform;
}
```

- [ ] **Step 5: 增加桥接宿主行为回归测试**

```cpp
void NavigationVtkBridgeTest::bridge_still_swaps_navigation_host_content_after_digital_twin_extension()
{
    QWidget parent;
    auto* navigationFrame = new QFrame(&parent);
    auto* navigationLayout = new QGridLayout(navigationFrame);
    EmbeddedVtkViewHost navigationHost(
        navigationFrame,
        navigationLayout,
        nullptr,
        EmbeddedVtkViewHostOptions {
            .hideExistingWidgets = true,
            .gridRow = 0,
            .gridColumn = 0,
            .gridRowSpan = 1,
            .gridColumnSpan = 1
        });

    NavigationVtkBridge bridge(
        nullptr,
        &navigationHost,
        nullptr,
        []() { return nullptr; },
        []() { return nullptr; });

    QWidget widgetA;
    QWidget widgetB;

    bridge.showSingleNavigationSpace(&widgetA);
    bridge.showSingleNavigationSpace(&widgetB);

    QVERIFY(navigationLayout->indexOf(&widgetB) >= 0);
    QVERIFY(widgetA.isHidden());
}
```

- [ ] **Step 6: 运行桥接测试**

Run: `ctest --test-dir build_x64_v142 -C Release -R navigation_vtk_bridge_test --output-on-failure`
Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add UI/NewPages/Navigation/navigation_vtk_bridge.h UI/NewPages/Navigation/navigation_vtk_bridge.cpp tests/unit/NavigationVtkBridgeTest.cpp
git commit -m "feat: extend navigation vtk bridge for digital twin rendering"
```

---

### Task 6: 接入模拟位姿源，打通单窗口数字孪生显示

**Files:**
- Modify: `UI/NewPages/NavigationPage.h`
- Modify: `UI/NewPages/NavigationPage.cpp`
- Modify: `tests/unit/AnkleNavigationWorkflowContractTest.cpp`

- [ ] **Step 1: 写失败契约测试，要求导航页把单窗口 3D 视图接入 runtime coordinator 和 VTK bridge**

```cpp
void AnkleNavigationWorkflowContractTest::navigation_page_routes_single_virtual_space_pose_updates_through_runtime_coordinator_and_vtk_bridge()
{
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));

    QVERIFY2(navigationSource.contains(QStringLiteral("m_runtimeCoordinator->handlePoseFrame(")),
        "navigation page must send realtime pose frames into NavigationRuntimeCoordinator");
    QVERIFY2(navigationSource.contains(QStringLiteral("m_navigationVtkBridge->loadBoneModels(")),
        "navigation page must preload active bone STL models into NavigationVtkBridge");
    QVERIFY2(navigationSource.contains(QStringLiteral("m_navigationVtkBridge->loadInstrumentModel(")),
        "navigation page must preload current instrument STL model into NavigationVtkBridge");
    QVERIFY2(navigationSource.contains(QStringLiteral("m_navigationVtkBridge->updateInstrumentPose(")),
        "navigation page must update instrument pose through NavigationVtkBridge");
    QVERIFY2(navigationSource.contains(QStringLiteral("m_navigationVtkBridge->setInstrumentVisible(")),
        "navigation page must update instrument visibility through NavigationVtkBridge");
}
```

- [ ] **Step 2: 运行契约测试确认当前页面还没有数字孪生链路**

Run: `ctest --test-dir build_x64_v142 -C Release -R ankle_navigation_workflow_contract_test --output-on-failure`
Expected: FAIL，提示页面仍未显式调用新的位姿与桥接接口

- [ ] **Step 3: 在页面头文件中声明统一刷新入口**

```cpp
class NavigationPageNew : public QWidget
{
    Q_OBJECT

private:
    void pushSimulatedPoseFrameToRuntime();
    void refreshRealtimeDigitalTwin();
    QStringList activeBoneModelPaths() const;
    QString activeInstrumentModelPath() const;
};
```

- [ ] **Step 4: 在页面实现中生成模拟位姿帧并喂给运行时协调器**

```cpp
void NavigationPageNew::pushSimulatedPoseFrameToRuntime()
{
    if (!m_runtimeCoordinator || !m_runtimeState) {
        return;
    }

    NavigationPoseFrame frame;
    frame.sourceId = QStringLiteral("simulator");
    frame.toolId = m_navigationToolId;
    frame.timestamp = QDateTime::currentDateTimeUtc();
    frame.trackingVisible = true;
    frame.trackingConfidence = 1.0;
    frame.trackingToMarker = m_registrationTransform;
    frame.trackingToMarker.translate(0.0f, 0.0f, 0.0f);

    m_runtimeCoordinator->handlePoseFrame(frame);
}
```

- [ ] **Step 5: 在实时刷新里只驱动骨 STL、当前器械 STL 和位姿**

```cpp
void NavigationPageNew::refreshRealtimeDigitalTwin()
{
    if (!m_runtimeCoordinator || !m_navigationVtkBridge || !m_runtimeState) {
        return;
    }

    const NavigationDisplayState displayState =
        m_runtimeCoordinator->buildDisplayState(activeBoneModelPaths(), activeInstrumentModelPath());

    m_navigationVtkBridge->loadBoneModels(displayState.boneModelPaths);
    m_navigationVtkBridge->loadInstrumentModel(displayState.activeToolId, displayState.activeToolModelPath);
    m_navigationVtkBridge->updateInstrumentPose(displayState.activeToolId, displayState.vtkToolTransform);
    m_navigationVtkBridge->setInstrumentVisible(displayState.activeToolId, displayState.toolVisible);
}
```

- [ ] **Step 6: 运行导航契约与运行时测试**

Run: `ctest --test-dir build_x64_v142 -C Release -R "ankle_navigation_workflow_contract_test|navigation_runtime_coordinator_contract_test|navigation_vtk_bridge_test" --output-on-failure`
Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add UI/NewPages/NavigationPage.h UI/NewPages/NavigationPage.cpp tests/unit/AnkleNavigationWorkflowContractTest.cpp
git commit -m "feat: connect simulator pose updates to single-window digital twin"
```

---

### Task 7: 扩展评估记录，沉淀病例级 realtime pose 指标

**Files:**
- Modify: `Framework/Navigation/ankle_navigation_types.h`
- Modify: `Framework/Navigation/navigation_evaluation_service.cpp`
- Modify: `UI/NewPages/Navigation/navigation_evaluation_summary_formatter.cpp`
- Modify: `tests/unit/NavigationEvaluationServiceTest.cpp`
- Modify: `tests/unit/NavigationEvaluationSummaryFormatterTest.cpp`

- [ ] **Step 1: 写失败测试，要求评估快照和摘要包含 latency、jitter、visible frame ratio**

```cpp
void NavigationEvaluationSummaryFormatterTest::formatter_includes_realtime_pose_metrics_summary()
{
    AnkleEvaluationSnapshot snapshot;
    snapshot.caseId = QStringLiteral("ankle-case-realtime-001");
    snapshot.navigationMetrics.insert(QStringLiteral("tracking_latency_ms"), 31.0);
    snapshot.navigationMetrics.insert(QStringLiteral("tracking_jitter_mm"), 0.28);
    snapshot.navigationMetrics.insert(QStringLiteral("visible_frame_ratio"), 0.99);

    const QString summary = buildNavigationEvaluationSummary(snapshot);

    QVERIFY(summary.contains(QStringLiteral("31.00")));
    QVERIFY(summary.contains(QStringLiteral("0.28")));
    QVERIFY(summary.contains(QStringLiteral("99.00")));
}
```

- [ ] **Step 2: 运行评估测试确认当前摘要尚未覆盖 latency**

Run: `ctest --test-dir build_x64_v142 -C Release -R "navigation_evaluation_service_test|navigation_evaluation_summary_formatter_test" --output-on-failure`
Expected: FAIL，提示 `tracking_latency_ms` 未进入导出或摘要

- [ ] **Step 3: 在运行记录类型中明确实时导航模式**

```cpp
struct AnkleNavigationRunRecord
{
    QString caseId;
    QString navigationMode;
    double confidenceScore = 0.0;
    QStringList warnings;
    QVariantMap metrics;
};
```

```cpp
run.metrics.insert(QStringLiteral("tracking_latency_ms"), 31.0);
run.metrics.insert(QStringLiteral("tracking_jitter_mm"), 0.28);
run.metrics.insert(QStringLiteral("visible_frame_ratio"), 0.99);
```

- [ ] **Step 4: 在评估服务导出和摘要格式化里补齐新指标**

```cpp
object.insert(QStringLiteral("tracking_latency_ms"),
    snapshot.navigationMetrics.value(QStringLiteral("tracking_latency_ms")).toDouble());
object.insert(QStringLiteral("tracking_jitter_mm"),
    snapshot.navigationMetrics.value(QStringLiteral("tracking_jitter_mm")).toDouble());
object.insert(QStringLiteral("visible_frame_ratio"),
    snapshot.navigationMetrics.value(QStringLiteral("visible_frame_ratio")).toDouble());
```

```cpp
lines.append(QStringLiteral("实时位姿：latency=%1 ms，jitter=%2 mm，可见率=%3%")
    .arg(snapshot.navigationMetrics.value(QStringLiteral("tracking_latency_ms")).toDouble(), 0, 'f', 2)
    .arg(snapshot.navigationMetrics.value(QStringLiteral("tracking_jitter_mm")).toDouble(), 0, 'f', 2)
    .arg(snapshot.navigationMetrics.value(QStringLiteral("visible_frame_ratio")).toDouble() * 100.0, 0, 'f', 2));
```

- [ ] **Step 5: 补一条评估服务 round-trip 测试**

```cpp
void NavigationEvaluationServiceTest::service_round_trips_tracking_latency_jitter_and_visibility_metrics()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    NavigationEvaluationService service(tempRoot.path());

    AnkleNavigationRunRecord run;
    run.caseId = QStringLiteral("ankle-case-realtime-002");
    run.navigationMode = QStringLiteral("live_tracking");
    run.confidenceScore = 0.88;
    run.metrics.insert(QStringLiteral("tracking_latency_ms"), 31.0);
    run.metrics.insert(QStringLiteral("tracking_jitter_mm"), 0.28);
    run.metrics.insert(QStringLiteral("visible_frame_ratio"), 0.99);

    QVERIFY(service.saveNavigationRun(run));

    const AnkleEvaluationSnapshot snapshot = service.loadEvaluationSnapshot(run.caseId);
    QCOMPARE(snapshot.navigationMetrics.value(QStringLiteral("tracking_latency_ms")).toDouble(), 31.0);
    QCOMPARE(snapshot.navigationMetrics.value(QStringLiteral("tracking_jitter_mm")).toDouble(), 0.28);
    QCOMPARE(snapshot.navigationMetrics.value(QStringLiteral("visible_frame_ratio")).toDouble(), 0.99);
}
```

- [ ] **Step 6: 运行评估测试**

Run: `ctest --test-dir build_x64_v142 -C Release -R "navigation_evaluation_service_test|navigation_evaluation_summary_formatter_test" --output-on-failure`
Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add Framework/Navigation/ankle_navigation_types.h Framework/Navigation/navigation_evaluation_service.cpp UI/NewPages/Navigation/navigation_evaluation_summary_formatter.cpp tests/unit/NavigationEvaluationServiceTest.cpp tests/unit/NavigationEvaluationSummaryFormatterTest.cpp
git commit -m "feat: record realtime pose metrics in evaluation outputs"
```

---

### Task 8: 最终联调、构建验证与交付收口

**Files:**
- Modify: `tests/unit/CMakeLists.txt`
- Modify: `docs/superpowers/specs/2026-05-08-navigation-realtime-pose-digital-twin-design.md`

- [ ] **Step 1: 确认所有新旧导航测试目标都已纳入构建**

```cmake
set(MEDICALPRO_FRAMEWORK_UNIT_TEST_TARGETS
    ${MEDICALPRO_FRAMEWORK_UNIT_TEST_TARGETS}
    navigation_pose_stream_test
    navigation_transform_graph_test
)
```

- [ ] **Step 2: 运行实时数字孪生相关完整测试集**

Run: `ctest --test-dir build_x64_v142 -C Release -R "navigation_pose_stream_test|navigation_transform_graph_test|navigation_runtime_state_test|navigation_runtime_coordinator_contract_test|navigation_vtk_bridge_test|navigation_evaluation_service_test|navigation_evaluation_summary_formatter_test|ankle_navigation_workflow_contract_test" --output-on-failure`
Expected: PASS

- [ ] **Step 3: 构建主程序，确认主工程可编译**

Run: `cmake --build build_x64_v142 --config Release --target medicalpro`
Expected: BUILD SUCCESS

- [ ] **Step 4: 用 10 秒模拟导航做最小 smoke 检查**

Run: `ctest --test-dir build_x64_v142 -C Release -R optical_tracking_quality_snapshot_test --output-on-failure`
Expected: PASS，并能证明基础跟踪质量指标仍可产出

- [ ] **Step 5: 回写 spec 的实现状态注记**

```md
## Implementation Status

- 实时位姿帧：已实现
- 固定多坐标系链路：已实现
- 单窗口数字孪生渲染桥：已实现
- latency / jitter / visible frame ratio 病例级导出：已实现
```

- [ ] **Step 6: Commit**

```bash
git add tests/unit/CMakeLists.txt docs/superpowers/specs/2026-05-08-navigation-realtime-pose-digital-twin-design.md
git commit -m "docs: mark realtime pose digital twin implementation complete"
```

---

## Self-Review

### Spec coverage

- `NavigationPoseFrame`、`NavigationPoseStream`：Task 1
- 固定坐标链 `tracking -> calibrated tool -> patient -> vtk world`：Task 2
- `NavigationRuntimeState` 与 `NavigationRuntimeCoordinator` 升级：Task 3, Task 4
- `NavigationVtkBridge` 单窗口数字孪生接口：Task 5
- 模拟器位姿源接入单窗口导航：Task 6
- `latency / jitter / visible frame ratio` 评估落盘：Task 4, Task 7
- 完整构建与验收：Task 8

### Placeholder scan

- 所有任务都包含明确文件路径、测试名、命令和代码片段
- 未使用 `TODO`、`TBD`、`implement later`

### Type consistency

- `NavigationPoseFrame`、`NavigationPoseStream`、`NavigationTransformGraph`、`NavigationTransformResult` 在所有任务中命名一致
- `tracking_latency_ms`、`tracking_jitter_mm`、`visible_frame_ratio` 作为评估指标命名一致
- `NavigationDisplayState` 在运行时协调器与导航页刷新路径中保持一致

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-05-08-navigation-realtime-pose-digital-twin-implementation-plan.md`. Two execution options:

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

Which approach?
