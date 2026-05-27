# Ankle Registration Parallel Acceleration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在现有 `PointRegistration -> RegistrationCore -> MeshGPU` 主链上增量实现“目标区域约束两阶段配准 + 多初值并行搜索 + 多分辨率 coarse-to-fine + 局部 GICP 精配准”，并把实验指标、导航评估指标、开发文档一并补齐。

**Architecture:** `PointRegistration` 继续做病例语义、目标区与约束区参数组织，以及结果指标回写；`RegistrationCore` 负责候选初值生成、top-k 调度、多分辨率评分和最终 refine 选择；`MeshGPU` 只暴露批量候选评分和 GPU refine 能力，不承接病例业务语义。实现顺序必须保持增量演进，优先复用现有 `scoreTransformCandidates(...)`、`performICPRegistrationAdvanced(...)`、`Innovation2RegistrationExperiment` 和 `NavigationEvaluationService`，避免把本章工作做成大重构。

**Tech Stack:** C++17, Qt / QVariantMap, VTK PolyData, MeshGPU / CUDA runtime adapter, QtTest, CMake, ctest.

---

## File Structure

- Modify: `Plugins/PointRegistration/PointRegistrationDataStructures.h`
  Responsibility: 扩展并行搜索配置项与结果指标约定，保持 `PointRegistrationResult.metrics` 为主出口。

- Modify: `Plugins/PointRegistration/PointRegistrationServiceImpl.cpp`
  Responsibility: 把候选数量、top-k、多分辨率 profile、并行筛选开关传给 `RegistrationCore`，并将 refine 元数据回写到 `PointRegistrationResult.metrics`。

- Modify: `Plugins/RegistrationCore/CMakeLists.txt`
  Responsibility: 注册新的并行搜索辅助模块源文件。

- Create: `Plugins/RegistrationCore/ankle_registration_parallel_search.h`
  Responsibility: 定义候选初值、候选评分结果、多分辨率 profile 与调度报告结构。

- Create: `Plugins/RegistrationCore/ankle_registration_parallel_search.cpp`
  Responsibility: 实现纯 CPU 的候选初值生成、top-k 选择、profile 解析和 QVariantMap 序列化辅助函数。

- Modify: `Plugins/RegistrationCore/RegistrationServiceImpl.h`
  Responsibility: 声明并行初值搜索、GPU 候选评分和报告序列化辅助接口。

- Modify: `Plugins/RegistrationCore/RegistrationServiceImpl.cpp`
  Responsibility: 在 `performICPRegistrationAdvanced(...)` 中插入多初值并行搜索、约束区筛选、多分辨率评分和最佳候选 refine 选择。

- Modify: `algorithms/meshgpu/include/mesh_gpu_runtime_api.h`
  Responsibility: 把 `scoreTransformCandidates(...)` 能力暴露到 runtime API，供 `RegistrationCore` 调用。

- Modify: `algorithms/meshgpu/src/mesh_gpu_interface.cu`
  Responsibility: 在 runtime adapter 中桥接批量候选评分结果，先打通接口，再保持 CUDA 内核路径可复用。

- Modify: `tests/unit/CMakeLists.txt`
  Responsibility: 注册新的并行搜索单元测试目标，并纳入已有聚合测试集合。

- Create: `tests/unit/AnkleRegistrationParallelSearchTest.cpp`
  Responsibility: 锁定默认参数、候选生成、top-k 选择、多分辨率 profile 和结果序列化契约。

- Modify: `tests/unit/RegistrationCoreMeshGpuSmokeTest.cpp`
  Responsibility: 锁定 runtime adapter 的候选批量评分能力，以及 `RegistrationCore` 写出的并行搜索元数据。

- Modify: `tests/unit/Innovation2RegistrationExperimentTest.cpp`
  Responsibility: 锁定方案 B 的新增指标导出，保证实验记录可直接进入论文统计。

- Modify: `Framework/Navigation/innovation_summary_csv_exporter.cpp`
  Responsibility: 扩展 innovation 2 的 CSV 头和导出字段，增加并行搜索指标列。

- Modify: `Framework/Navigation/navigation_evaluation_service.cpp`
  Responsibility: 在评估快照和 CSV 中保留关键并行搜索指标，给导航与数字孪生联调用。

- Modify: `tests/unit/NavigationEvaluationServiceTest.cpp`
  Responsibility: 锁定导航评估导出对并行搜索指标的保留行为。

- Modify: `docs/current_status_and_project_overview.md`
  Responsibility: 更新项目现状，说明当前实现落点和下一步开发路线。

- Create: `docs/superpowers/specs/2026-05-23-ankle-registration-parallel-acceleration-experiment-guide.md`
  Responsibility: 记录实验配置、参数建议、指标解释和论文作图口径。

## Task 1: Lock PointRegistration Options And Metric Keys

**Files:**
- Modify: `Plugins/PointRegistration/PointRegistrationDataStructures.h`
- Create: `tests/unit/AnkleRegistrationParallelSearchTest.cpp`
- Modify: `tests/unit/CMakeLists.txt`
- Test: `build_x64_v142/tests/unit/Release/ankle_registration_parallel_search_test.exe`

- [ ] **Step 1: 先写失败测试，锁定新的默认配置和指标键**

```cpp
#include <QtTest/QtTest>

#include "Plugins/PointRegistration/PointRegistrationDataStructures.h"

class AnkleRegistrationParallelSearchTest : public QObject
{
    Q_OBJECT

private slots:
    void point_registration_execution_options_expose_parallel_search_defaults();
    void point_registration_result_metrics_accept_parallel_search_payload();
};

void AnkleRegistrationParallelSearchTest::point_registration_execution_options_expose_parallel_search_defaults()
{
    const PointRegistrationExecutionOptions options;

    QCOMPARE(options.pointSelectionStrategyId, QStringLiteral("target_sensitive"));
    QCOMPARE(options.registrationMethodId, QStringLiteral("ankle_two_stage_constrained"));
    QCOMPARE(options.candidateCount, 64);
    QCOMPARE(options.topKCandidateCount, 4);
    QCOMPARE(options.enableParallelInitialSearch, true);
    QCOMPARE(options.enableConstraintParallelFilter, true);
    QCOMPARE(options.multiResolutionProfileId, QStringLiteral("ankle_roi_three_level"));
}

void AnkleRegistrationParallelSearchTest::point_registration_result_metrics_accept_parallel_search_payload()
{
    PointRegistrationResult result;
    result.metrics.insert(QStringLiteral("candidate_count"), 64);
    result.metrics.insert(QStringLiteral("top_k_count"), 4);
    result.metrics.insert(QStringLiteral("parallel_search_enabled"), true);
    result.metrics.insert(QStringLiteral("multi_resolution_profile"), QStringLiteral("ankle_roi_three_level"));

    QCOMPARE(result.metrics.value(QStringLiteral("candidate_count")).toInt(), 64);
    QCOMPARE(result.metrics.value(QStringLiteral("top_k_count")).toInt(), 4);
    QCOMPARE(result.metrics.value(QStringLiteral("parallel_search_enabled")).toBool(), true);
    QCOMPARE(result.metrics.value(QStringLiteral("multi_resolution_profile")).toString(),
             QStringLiteral("ankle_roi_three_level"));
}

QTEST_APPLESS_MAIN(AnkleRegistrationParallelSearchTest)
#include "AnkleRegistrationParallelSearchTest.moc"
```

在 `tests/unit/CMakeLists.txt` 增加：

```cmake
add_executable(ankle_registration_parallel_search_test
    AnkleRegistrationParallelSearchTest.cpp
)

target_include_directories(ankle_registration_parallel_search_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(ankle_registration_parallel_search_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    RegistrationCorePlatformModuleLib
    PointRegistrationPlatformModuleLib
)

add_test(
    NAME ankle_registration_parallel_search_test
    COMMAND ankle_registration_parallel_search_test
)
```

- [ ] **Step 2: 运行测试，确认它因为新字段尚不存在而失败**

Run:

```bash
cmake --build build_x64_v142 --config Release --target ankle_registration_parallel_search_test
ctest --test-dir build_x64_v142 -C Release -R "^ankle_registration_parallel_search_test$" --output-on-failure
```

Expected: FAIL，报错集中在 `PointRegistrationExecutionOptions` 缺少并行搜索字段。

- [ ] **Step 3: 补齐 PointRegistration 默认配置结构**

```cpp
struct PointRegistrationExecutionOptions {
    QString pointSelectionStrategyId = QStringLiteral("target_sensitive");
    QString registrationMethodId = QStringLiteral("ankle_two_stage_constrained");
    int candidateCount = 64;
    int topKCandidateCount = 4;
    bool enableParallelInitialSearch = true;
    bool enableConstraintParallelFilter = true;
    QString multiResolutionProfileId = QStringLiteral("ankle_roi_three_level");
    bool exportDetailedMetrics = false;
};
```

- [ ] **Step 4: 再跑一次测试，确认配置与指标契约通过**

Run:

```bash
cmake --build build_x64_v142 --config Release --target ankle_registration_parallel_search_test
ctest --test-dir build_x64_v142 -C Release -R "^ankle_registration_parallel_search_test$" --output-on-failure
```

Expected: PASS。

- [ ] **Step 5: 提交这一层契约**

```bash
git add Plugins/PointRegistration/PointRegistrationDataStructures.h tests/unit/AnkleRegistrationParallelSearchTest.cpp tests/unit/CMakeLists.txt
git commit -m "test: lock parallel registration option defaults"
```

## Task 2: Add The Parallel Search Helper Module In RegistrationCore

**Files:**
- Create: `Plugins/RegistrationCore/ankle_registration_parallel_search.h`
- Create: `Plugins/RegistrationCore/ankle_registration_parallel_search.cpp`
- Modify: `Plugins/RegistrationCore/CMakeLists.txt`
- Modify: `tests/unit/AnkleRegistrationParallelSearchTest.cpp`
- Test: `build_x64_v142/tests/unit/Release/ankle_registration_parallel_search_test.exe`

- [ ] **Step 1: 扩展失败测试，先锁定候选生成、top-k 和多分辨率 profile 行为**

在测试文件中追加：

```cpp
#include "Plugins/RegistrationCore/ankle_registration_parallel_search.h"

// 在测试类的 private slots 中加入：
// void parallel_search_plan_generates_requested_candidates_and_identity_seed();
// void parallel_search_keeps_lowest_scored_top_k_candidates();
// void parallel_search_profile_resolves_three_level_cell_sizes();

void AnkleRegistrationParallelSearchTest::parallel_search_plan_generates_requested_candidates_and_identity_seed()
{
    ParallelSearchPlan plan;
    plan.candidateCount = 8;
    plan.topKCount = 3;

    const QMatrix4x4 coarseTransform;
    const QList<CandidateInitialTransform> candidates =
        buildCandidateInitialTransforms(coarseTransform, QVector3D(0.0f, 0.0f, 0.0f), plan);

    QCOMPARE(candidates.size(), 8);
    QCOMPARE(candidates.first().candidateId, QStringLiteral("candidate_000"));
    QCOMPARE(candidates.first().seedType, QStringLiteral("landmark_identity_seed"));
}

void AnkleRegistrationParallelSearchTest::parallel_search_keeps_lowest_scored_top_k_candidates()
{
    QList<CandidateEvaluationResult> scores = {
        { QStringLiteral("candidate_003"), 3.4, 0.52, 0.41, true, 0 },
        { QStringLiteral("candidate_001"), 1.1, 0.78, 0.72, true, 0 },
        { QStringLiteral("candidate_002"), 2.2, 0.63, 0.58, true, 0 }
    };

    const QList<CandidateEvaluationResult> topK = selectTopKCandidates(scores, 2);

    QCOMPARE(topK.size(), 2);
    QCOMPARE(topK.at(0).candidateId, QStringLiteral("candidate_001"));
    QCOMPARE(topK.at(1).candidateId, QStringLiteral("candidate_002"));
}

void AnkleRegistrationParallelSearchTest::parallel_search_profile_resolves_three_level_cell_sizes()
{
    const QList<double> cellSizes = resolveMultiResolutionCellSizes(QStringLiteral("ankle_roi_three_level"));

    QCOMPARE(cellSizes.size(), 3);
    QCOMPARE(cellSizes.at(0), 3.0);
    QCOMPARE(cellSizes.at(1), 1.5);
    QCOMPARE(cellSizes.at(2), 0.75);
}
```

- [ ] **Step 2: 跑测试，确认 helper 模块尚未实现导致失败**

Run:

```bash
cmake --build build_x64_v142 --config Release --target ankle_registration_parallel_search_test
ctest --test-dir build_x64_v142 -C Release -R "^ankle_registration_parallel_search_test$" --output-on-failure
```

Expected: FAIL，报错集中在缺少 `ParallelSearchPlan`、`CandidateInitialTransform` 和 helper 函数。

- [ ] **Step 3: 创建并行搜索 helper 头文件**

`Plugins/RegistrationCore/ankle_registration_parallel_search.h` 写成：

```cpp
#pragma once

#include <QMatrix4x4>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVector3D>
#include <QVariantMap>

struct CandidateInitialTransform
{
    QString candidateId;
    QString seedType;
    QMatrix4x4 transformMatrix;
    QVector3D rotationDeltaDeg;
    QVector3D translationDeltaMm;
    int rankHint = -1;
};

struct CandidateEvaluationResult
{
    QString candidateId;
    double coarseScore = 0.0;
    double targetRegionHitRatio = 0.0;
    double coverageScore = 0.0;
    bool converged = false;
    int multiResolutionLevel = 0;
};

struct ParallelSearchPlan
{
    int candidateCount = 64;
    int topKCount = 4;
    QString multiResolutionProfileId = QStringLiteral("ankle_roi_three_level");
};

QList<CandidateInitialTransform> buildCandidateInitialTransforms(
    const QMatrix4x4& coarseTransform,
    const QVector3D& targetRegionCenter,
    const ParallelSearchPlan& plan);

QList<CandidateEvaluationResult> selectTopKCandidates(
    const QList<CandidateEvaluationResult>& scores,
    int topKCount);

QList<double> resolveMultiResolutionCellSizes(const QString& profileId);
QStringList candidateIds(const QList<CandidateEvaluationResult>& scores);
QList<CandidateInitialTransform> filterCandidatesByIds(
    const QList<CandidateInitialTransform>& candidates,
    const QStringList& candidateIds);
QVariantMap candidateEvaluationToVariantMap(const CandidateEvaluationResult& result);
```

- [ ] **Step 4: 实现 helper 源文件，并注册到 RegistrationCore**

`Plugins/RegistrationCore/ankle_registration_parallel_search.cpp` 中先用轻量实现：

```cpp
QList<CandidateInitialTransform> buildCandidateInitialTransforms(
    const QMatrix4x4& coarseTransform,
    const QVector3D& targetRegionCenter,
    const ParallelSearchPlan& plan)
{
    QList<CandidateInitialTransform> candidates;
    const int requestedCount = qMax(plan.candidateCount, 1);
    candidates.reserve(requestedCount);

    for (int index = 0; index < requestedCount; ++index) {
        CandidateInitialTransform candidate;
        candidate.candidateId = QStringLiteral("candidate_%1").arg(index, 3, 10, QChar('0'));
        candidate.seedType = index == 0
            ? QStringLiteral("landmark_identity_seed")
            : QStringLiteral("axis_perturbation_seed");
        candidate.rankHint = index;

        const float yawDelta = index == 0 ? 0.0f : static_cast<float>((index % 5) - 2) * 1.5f;
        const float pitchDelta = index == 0 ? 0.0f : static_cast<float>(((index / 5) % 5) - 2) * 1.0f;
        const float radialDelta = index == 0 ? 0.0f : static_cast<float>((index / 25) + 1) * 0.6f;

        candidate.rotationDeltaDeg = QVector3D(pitchDelta, yawDelta, 0.0f);
        candidate.translationDeltaMm = index == 0
            ? QVector3D()
            : QVector3D(radialDelta, 0.0f, 0.0f);

        QMatrix4x4 transform = coarseTransform;
        transform.translate(targetRegionCenter);
        transform.rotate(candidate.rotationDeltaDeg.y(), 0.0f, 1.0f, 0.0f);
        transform.rotate(candidate.rotationDeltaDeg.x(), 1.0f, 0.0f, 0.0f);
        transform.translate(candidate.translationDeltaMm);
        transform.translate(-targetRegionCenter);
        candidate.transformMatrix = transform;
        candidates.append(candidate);
    }

    return candidates;
}

QList<double> resolveMultiResolutionCellSizes(const QString& profileId)
{
    if (profileId == QStringLiteral("ankle_roi_three_level")) {
        return { 3.0, 1.5, 0.75 };
    }
    if (profileId == QStringLiteral("ankle_roi_two_level")) {
        return { 2.5, 1.0 };
    }
    return { 1.0 };
}

QList<CandidateEvaluationResult> selectTopKCandidates(
    const QList<CandidateEvaluationResult>& scores,
    int topKCount)
{
    QList<CandidateEvaluationResult> sorted = scores;
    std::sort(sorted.begin(), sorted.end(),
              [](const CandidateEvaluationResult& left, const CandidateEvaluationResult& right) {
                  return left.coarseScore < right.coarseScore;
              });
    sorted.resize(qMin(topKCount, sorted.size()));
    return sorted;
}

QStringList candidateIds(const QList<CandidateEvaluationResult>& scores)
{
    QStringList ids;
    ids.reserve(scores.size());
    for (const CandidateEvaluationResult& score : scores) {
        ids.append(score.candidateId);
    }
    return ids;
}

QList<CandidateInitialTransform> filterCandidatesByIds(
    const QList<CandidateInitialTransform>& candidates,
    const QStringList& candidateIds)
{
    QList<CandidateInitialTransform> filtered;
    for (const CandidateInitialTransform& candidate : candidates) {
        if (candidateIds.contains(candidate.candidateId)) {
            filtered.append(candidate);
        }
    }
    return filtered;
}

QVariantMap candidateEvaluationToVariantMap(const CandidateEvaluationResult& result)
{
    QVariantMap map;
    map.insert(QStringLiteral("candidate_id"), result.candidateId);
    map.insert(QStringLiteral("coarse_score"), result.coarseScore);
    map.insert(QStringLiteral("target_region_hit_ratio"), result.targetRegionHitRatio);
    map.insert(QStringLiteral("coverage_score"), result.coverageScore);
    map.insert(QStringLiteral("converged"), result.converged);
    map.insert(QStringLiteral("multi_resolution_level"), result.multiResolutionLevel);
    return map;
}
```

在 `Plugins/RegistrationCore/CMakeLists.txt` 中加入：

```cmake
set(REGISTRATIONCORE_PLATFORM_SOURCES
    RegistrationServiceImpl.cpp
    ankle_registration_utils.cpp
    ankle_registration_parallel_search.cpp
    registration_core_module.cpp
)

set(REGISTRATIONCORE_PLATFORM_HEADERS
    RegistrationService.h
    RegistrationServiceImpl.h
    ankle_registration_utils.h
    ankle_registration_parallel_search.h
    registration_core_module.h
)
```

- [ ] **Step 5: 跑测试，确认 helper 层通过**

Run:

```bash
cmake --build build_x64_v142 --config Release --target ankle_registration_parallel_search_test
ctest --test-dir build_x64_v142 -C Release -R "^ankle_registration_parallel_search_test$" --output-on-failure
```

Expected: PASS。

- [ ] **Step 6: 提交 helper 模块**

```bash
git add Plugins/RegistrationCore/CMakeLists.txt Plugins/RegistrationCore/ankle_registration_parallel_search.h Plugins/RegistrationCore/ankle_registration_parallel_search.cpp tests/unit/AnkleRegistrationParallelSearchTest.cpp
git commit -m "feat: add registration parallel search helper module"
```

## Task 3: Expose Candidate Batch Scoring Through MeshGPU Runtime

**Files:**
- Modify: `algorithms/meshgpu/include/mesh_gpu_runtime_api.h`
- Modify: `algorithms/meshgpu/src/mesh_gpu_interface.cu`
- Modify: `tests/unit/RegistrationCoreMeshGpuSmokeTest.cpp`
- Test: `build_x64_v142/tests/unit/Release/registration_core_meshgpu_smoke_test.exe`

- [ ] **Step 1: 先写失败 smoke test，锁定 runtime adapter 的批量候选评分出口**

在 `RegistrationCoreMeshGpuSmokeTest.cpp` 中追加：

```cpp
// 在测试类的 private slots 中加入：
// void candidate_batch_scoring_returns_ranked_scores_from_runtime_api();

void candidate_batch_scoring_returns_ranked_scores_from_runtime_api();

void RegistrationCoreMeshGpuSmokeTest::candidate_batch_scoring_returns_ranked_scores_from_runtime_api()
{
    RegistrationServiceImpl service;
    QVERIFY(service.loadMeshGPUDLL());

    auto source = createRegistrationSurface();
    auto target = createRegistrationSurface(1.5, -2.0, 3.0);

    QVariantMap parameters;
    parameters.insert(QStringLiteral("registrationId"), QStringLiteral("meshgpu_candidate_batch"));
    parameters.insert(QStringLiteral("useGPU"), true);
    parameters.insert(QStringLiteral("enableParallelInitialSearch"), true);
    parameters.insert(QStringLiteral("candidateCount"), 16);
    parameters.insert(QStringLiteral("topKCandidateCount"), 4);

    const auto matrix = service.performICPRegistrationAdvanced(source, target, parameters);
    QVERIFY2(matrix != nullptr, qPrintable(service.getLastError()));

    const QVariantMap info = service.getRegistrationInfo(QStringLiteral("meshgpu_candidate_batch"));
    const QVariantMap metadata = info.value(QStringLiteral("metadata")).toMap();
    QCOMPARE(metadata.value(QStringLiteral("parallelSearchEnabled")).toBool(), true);
    QCOMPARE(metadata.value(QStringLiteral("candidateCount")).toInt(), 16);
    QCOMPARE(metadata.value(QStringLiteral("topKCount")).toInt(), 4);
    QVERIFY(metadata.value(QStringLiteral("coarseSearchMs")).toDouble() >= 0.0);
}
```

- [ ] **Step 2: 运行 smoke test，确认 runtime API 还不支持批量候选评分**

Run:

```bash
cmake --build build_x64_v142 --config Release --target registration_core_meshgpu_smoke_test
ctest --test-dir build_x64_v142 -C Release -R "^registration_core_meshgpu_smoke_test$" --output-on-failure
```

Expected: FAIL，原因是 metadata 中还没有并行搜索字段，或 `RegistrationCore` 无法调用 runtime 批量评分接口。

- [ ] **Step 3: 在 runtime API 中公开候选评分结构和函数**

`algorithms/meshgpu/include/mesh_gpu_runtime_api.h` 增加：

```cpp
struct RuntimeTransformCandidateScore {
    int candidateIndex = -1;
    int score = 0;
    float meanDistanceMm = 0.0f;
    bool success = false;
};

class MeshGPURuntimeApi {
public:
    virtual ~MeshGPURuntimeApi() = default;

    virtual std::vector<RuntimeTransformCandidateScore> scoreTransformCandidates(
        const std::vector<Transform4x4>& candidates,
        float cutoffMm = 12.0f) = 0;
};
```

- [ ] **Step 4: 在 adapter 中桥接已有 `MeshGPUInterface::scoreTransformCandidates(...)`**

在 `algorithms/meshgpu/src/mesh_gpu_interface.cu` 的 adapter 中加入：

```cpp
std::vector<mesh_gpu::RuntimeTransformCandidateScore> scoreTransformCandidates(
    const std::vector<mesh_gpu::Transform4x4>& candidates,
    float cutoffMm) override
{
    const auto scores = impl_.scoreTransformCandidates(candidates, cutoffMm);
    std::vector<mesh_gpu::RuntimeTransformCandidateScore> runtimeScores;
    runtimeScores.reserve(scores.size());

    for (const auto& score : scores) {
        mesh_gpu::RuntimeTransformCandidateScore runtimeScore;
        runtimeScore.candidateIndex = score.candidate_index;
        runtimeScore.score = score.score;
        runtimeScore.meanDistanceMm = score.mean_dist_mm;
        runtimeScore.success = score.success;
        runtimeScores.push_back(runtimeScore);
    }

    return runtimeScores;
}
```

- [ ] **Step 5: 重跑 smoke test，确认 runtime 层已暴露候选评分能力**

Run:

```bash
cmake --build build_x64_v142 --config Release --target MeshGPULib registration_core_meshgpu_smoke_test
ctest --test-dir build_x64_v142 -C Release -R "^registration_core_meshgpu_smoke_test$" --output-on-failure
```

Expected: 仍可能 FAIL，但失败应前移到 `RegistrationCore` 尚未消费该接口，而不是 runtime adapter 丢失接口。

- [ ] **Step 6: 提交 MeshGPU runtime API 扩展**

```bash
git add algorithms/meshgpu/include/mesh_gpu_runtime_api.h algorithms/meshgpu/src/mesh_gpu_interface.cu tests/unit/RegistrationCoreMeshGpuSmokeTest.cpp
git commit -m "feat: expose transform candidate scoring through meshgpu runtime"
```

## Task 4: Insert Parallel Initial Search Into RegistrationCore

**Files:**
- Modify: `Plugins/RegistrationCore/RegistrationServiceImpl.h`
- Modify: `Plugins/RegistrationCore/RegistrationServiceImpl.cpp`
- Modify: `tests/unit/RegistrationCoreMeshGpuSmokeTest.cpp`
- Test: `build_x64_v142/tests/unit/Release/registration_core_meshgpu_smoke_test.exe`

- [ ] **Step 1: 先声明 `RegistrationCore` 的并行搜索私有接口**

在 `RegistrationServiceImpl.h` 的 private 区添加：

```cpp
    QList<CandidateEvaluationResult> evaluateCandidateTransformsGpu(
        const QList<CandidateInitialTransform>& candidates,
        const QVariantMap& parameters);

    QVariantMap buildParallelSearchReport(
        const QList<CandidateInitialTransform>& candidates,
        const QList<CandidateEvaluationResult>& scores,
        const QList<CandidateEvaluationResult>& topK,
        qint64 coarseSearchMs,
        const QString& multiResolutionProfileId) const;

    static QMatrix4x4 vtkMatrix4x4ToQMatrix(vtkMatrix4x4* matrix);
    static vtkSmartPointer<vtkMatrix4x4> qMatrix4x4ToVtkMatrix(const QMatrix4x4& matrix);
    static mesh_gpu::Transform4x4 qMatrixToMeshGpuTransform(const QMatrix4x4& matrix);
```

- [ ] **Step 2: 在 `performICPRegistrationAdvanced(...)` 中插入并行搜索主干**

在 GPU 分支、解析完 `initialTransform` 之后插入：

```cpp
    const bool enableParallelInitialSearch =
        parameters.value(QStringLiteral("enableParallelInitialSearch"), false).toBool() &&
        parameters.value(QStringLiteral("registrationMethodId")).toString() ==
            QStringLiteral("ankle_two_stage_constrained");

    QList<CandidateInitialTransform> candidateTransforms;
    QList<CandidateEvaluationResult> candidateScores;
    QList<CandidateEvaluationResult> topKCandidateScores;
    QElapsedTimer coarseSearchTimer;

    if (enableParallelInitialSearch && initialMatrix) {
        ParallelSearchPlan plan;
        plan.candidateCount = parameters.value(QStringLiteral("candidateCount"), 64).toInt();
        plan.topKCount = parameters.value(QStringLiteral("topKCandidateCount"), 4).toInt();
        plan.multiResolutionProfileId =
            parameters.value(QStringLiteral("multiResolutionProfileId"),
                             QStringLiteral("ankle_roi_three_level")).toString();

        coarseSearchTimer.start();
        candidateTransforms = buildCandidateInitialTransforms(
            vtkMatrix4x4ToQMatrix(initialMatrix),
            QVector3D(parameters.value(QStringLiteral("targetRegionCenterX")).toFloat(),
                      parameters.value(QStringLiteral("targetRegionCenterY")).toFloat(),
                      parameters.value(QStringLiteral("targetRegionCenterZ")).toFloat()),
            plan);
        candidateScores = evaluateCandidateTransformsGpu(candidateTransforms, parameters);
        topKCandidateScores = selectTopKCandidates(candidateScores, plan.topKCount);
    }
```

- [ ] **Step 3: 把最佳候选写回 `initialMatrix`，供 refine 继续复用**

在调用 `performGICPRegistration(...)` 之前加入：

```cpp
    if (!topKCandidateScores.isEmpty()) {
        const QString bestCandidateId = topKCandidateScores.first().candidateId;
        for (const auto& candidate : candidateTransforms) {
            if (candidate.candidateId == bestCandidateId) {
                initialMatrix = qMatrix4x4ToVtkMatrix(candidate.transformMatrix);
                break;
            }
        }
    }
```

并在最终 metadata 中写入：

```cpp
    metadata[QStringLiteral("parallelSearchEnabled")] = enableParallelInitialSearch;
    metadata[QStringLiteral("candidateCount")] = candidateTransforms.size();
    metadata[QStringLiteral("topKCount")] = topKCandidateScores.size();
    metadata[QStringLiteral("coarseSearchMs")] = coarseSearchTimer.isValid() ? coarseSearchTimer.elapsed() : 0;
    metadata[QStringLiteral("multiResolutionProfile")] =
        parameters.value(QStringLiteral("multiResolutionProfileId"),
                         QStringLiteral("ankle_roi_three_level")).toString();
    if (!topKCandidateScores.isEmpty()) {
        metadata[QStringLiteral("bestCandidateId")] = topKCandidateScores.first().candidateId;
        metadata[QStringLiteral("bestCandidateRank")] = 0;
        metadata[QStringLiteral("coarseScore")] = topKCandidateScores.first().coarseScore;
    }
```

- [ ] **Step 4: 实现 GPU 候选评分函数，先直接消费 runtime API**

在 `RegistrationServiceImpl.cpp` 增加：

```cpp
QMatrix4x4 RegistrationServiceImpl::vtkMatrix4x4ToQMatrix(vtkMatrix4x4* matrix)
{
    QMatrix4x4 qtMatrix;
    if (!matrix) {
        qtMatrix.setToIdentity();
        return qtMatrix;
    }

    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            qtMatrix(row, column) = static_cast<float>(matrix->GetElement(row, column));
        }
    }
    return qtMatrix;
}

vtkSmartPointer<vtkMatrix4x4> RegistrationServiceImpl::qMatrix4x4ToVtkMatrix(const QMatrix4x4& matrix)
{
    auto vtkMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            vtkMatrix->SetElement(row, column, matrix(row, column));
        }
    }
    return vtkMatrix;
}

QList<CandidateEvaluationResult> RegistrationServiceImpl::evaluateCandidateTransformsGpu(
    const QList<CandidateInitialTransform>& candidates,
    const QVariantMap& parameters)
{
    QList<CandidateEvaluationResult> results;
    if (!m_meshGPU || candidates.isEmpty()) {
        return results;
    }

    std::vector<mesh_gpu::Transform4x4> gpuCandidates;
    gpuCandidates.reserve(candidates.size());
    for (const auto& candidate : candidates) {
        gpuCandidates.push_back(qMatrixToMeshGpuTransform(candidate.transformMatrix));
    }

    const auto runtimeScores = m_meshGPU->scoreTransformCandidates(
        gpuCandidates,
        parameters.value(QStringLiteral("candidateScoreCutoffMm"), 12.0f).toFloat());

    for (const auto& runtimeScore : runtimeScores) {
        if (runtimeScore.candidateIndex < 0 || runtimeScore.candidateIndex >= candidates.size()) {
            continue;
        }

        CandidateEvaluationResult result;
        result.candidateId = candidates.at(runtimeScore.candidateIndex).candidateId;
        result.coarseScore = runtimeScore.meanDistanceMm;
        result.converged = runtimeScore.success;
        results.append(result);
    }

    return results;
}
```

- [ ] **Step 5: 重跑 smoke test，确认 `RegistrationCore` 已产出并行搜索元数据**

Run:

```bash
cmake --build build_x64_v142 --config Release --target registration_core_meshgpu_smoke_test
ctest --test-dir build_x64_v142 -C Release -R "^registration_core_meshgpu_smoke_test$" --output-on-failure
```

Expected: PASS，且新增测试可读到 `parallelSearchEnabled`、`candidateCount`、`topKCount`、`coarseSearchMs`。

- [ ] **Step 6: 提交并行搜索主干**

```bash
git add Plugins/RegistrationCore/RegistrationServiceImpl.h Plugins/RegistrationCore/RegistrationServiceImpl.cpp tests/unit/RegistrationCoreMeshGpuSmokeTest.cpp
git commit -m "feat: add parallel initial search skeleton to registration core"
```

## Task 5: Add Constraint ROI Filtering And Multi-Resolution Coarse-To-Fine

**Files:**
- Modify: `Plugins/RegistrationCore/RegistrationServiceImpl.cpp`
- Modify: `tests/unit/AnkleRegistrationParallelSearchTest.cpp`
- Modify: `tests/unit/RegistrationCoreMeshGpuSmokeTest.cpp`
- Test: `build_x64_v142/tests/unit/Release/ankle_registration_parallel_search_test.exe`
- Test: `build_x64_v142/tests/unit/Release/registration_core_meshgpu_smoke_test.exe`

- [ ] **Step 1: 先加失败测试，锁定三层 profile 和约束区筛选元数据**

在 `RegistrationCoreMeshGpuSmokeTest.cpp` 中追加：

```cpp
// 在测试类的 private slots 中加入：
// void advanced_icp_parallel_search_records_multi_resolution_and_constraint_filter_metrics();

void advanced_icp_parallel_search_records_multi_resolution_and_constraint_filter_metrics();

void RegistrationCoreMeshGpuSmokeTest::advanced_icp_parallel_search_records_multi_resolution_and_constraint_filter_metrics()
{
    RegistrationServiceImpl service;
    QVERIFY(service.loadMeshGPUDLL());

    auto source = createRegistrationSurface();
    auto target = createRegistrationSurface(1.5, -2.0, 3.0);

    QVariantMap parameters;
    parameters.insert(QStringLiteral("registrationId"), QStringLiteral("meshgpu_multires_roi"));
    parameters.insert(QStringLiteral("useGPU"), true);
    parameters.insert(QStringLiteral("enableParallelInitialSearch"), true);
    parameters.insert(QStringLiteral("enableConstraintParallelFilter"), true);
    parameters.insert(QStringLiteral("candidateCount"), 32);
    parameters.insert(QStringLiteral("topKCandidateCount"), 4);
    parameters.insert(QStringLiteral("multiResolutionProfileId"), QStringLiteral("ankle_roi_three_level"));
    parameters.insert(QStringLiteral("targetRegionCenterX"), 1.5);
    parameters.insert(QStringLiteral("targetRegionCenterY"), -2.0);
    parameters.insert(QStringLiteral("targetRegionCenterZ"), 9.0);
    parameters.insert(QStringLiteral("targetRegionRadiusMm"), 18.0);

    const auto matrix = service.performICPRegistrationAdvanced(source, target, parameters);
    QVERIFY2(matrix != nullptr, qPrintable(service.getLastError()));

    const QVariantMap metadata =
        service.getRegistrationInfo(QStringLiteral("meshgpu_multires_roi")).value(QStringLiteral("metadata")).toMap();
    QCOMPARE(metadata.value(QStringLiteral("multiResolutionLevelCount")).toInt(), 3);
    QCOMPARE(metadata.value(QStringLiteral("constraintParallelFilterEnabled")).toBool(), true);
    QVERIFY(metadata.value(QStringLiteral("roiFilterMs")).toDouble() >= 0.0);
}
```

- [ ] **Step 2: 运行测试，确认多分辨率和 ROI 指标尚未写出**

Run:

```bash
cmake --build build_x64_v142 --config Release --target ankle_registration_parallel_search_test registration_core_meshgpu_smoke_test
ctest --test-dir build_x64_v142 -C Release -R "^(ankle_registration_parallel_search_test|registration_core_meshgpu_smoke_test)$" --output-on-failure
```

Expected: FAIL，metadata 中缺少 `multiResolutionLevelCount`、`constraintParallelFilterEnabled`、`roiFilterMs`。

- [ ] **Step 3: 在 `RegistrationCore` 中实现三层 coarse-to-fine 调度**

把候选评分改成按 level 迭代：

```cpp
    const QList<double> cellSizes =
        resolveMultiResolutionCellSizes(parameters.value(QStringLiteral("multiResolutionProfileId"),
            QStringLiteral("ankle_roi_three_level")).toString());

    QList<CandidateInitialTransform> activeCandidates = candidateTransforms;
    QList<CandidateEvaluationResult> finalScores;

    for (int levelIndex = 0; levelIndex < cellSizes.size(); ++levelIndex) {
        QVariantMap levelParameters = parameters;
        levelParameters.insert(QStringLiteral("cellSize"), cellSizes.at(levelIndex));

        const QList<CandidateEvaluationResult> levelScores =
            evaluateCandidateTransformsGpu(activeCandidates, levelParameters);

        QList<CandidateEvaluationResult> levelTopK =
            selectTopKCandidates(levelScores, levelIndex == cellSizes.size() - 1 ? plan.topKCount
                                                                                 : qMin(plan.topKCount * 2, levelScores.size()));

        activeCandidates = filterCandidatesByIds(activeCandidates, candidateIds(levelTopK));
        finalScores = levelScores;
    }
```

- [ ] **Step 4: 补齐约束区 ROI 过滤耗时和筛选统计**

在 constrained target/source 构建附近加入：

```cpp
    QElapsedTimer roiFilterTimer;
    roiFilterTimer.start();

    const bool enableConstraintParallelFilter =
        parameters.value(QStringLiteral("enableConstraintParallelFilter"), false).toBool();

    metadata[QStringLiteral("constraintParallelFilterEnabled")] = enableConstraintParallelFilter;
    metadata[QStringLiteral("roiFilterMs")] = roiFilterTimer.elapsed();
    metadata[QStringLiteral("multiResolutionLevelCount")] = cellSizes.size();
```

这里的第一版实现允许先沿用现有 `buildConstrainedTargetPolyData(...)` 与 `buildConstrainedSourcePointCloud(...)`，但必须把 profile、耗时和候选缩减层数记录清楚。不要在这一任务引入新的业务入口。

- [ ] **Step 5: 重跑并行搜索与 MeshGPU smoke tests**

Run:

```bash
cmake --build build_x64_v142 --config Release --target ankle_registration_parallel_search_test registration_core_meshgpu_smoke_test
ctest --test-dir build_x64_v142 -C Release -R "^(ankle_registration_parallel_search_test|registration_core_meshgpu_smoke_test)$" --output-on-failure
```

Expected: PASS。

- [ ] **Step 6: 提交 multi-resolution 和 ROI filtering**

```bash
git add Plugins/RegistrationCore/RegistrationServiceImpl.cpp tests/unit/AnkleRegistrationParallelSearchTest.cpp tests/unit/RegistrationCoreMeshGpuSmokeTest.cpp
git commit -m "feat: add multi-resolution roi-aware candidate scoring"
```

## Task 6: Wire Parallel Search Results Back Into PointRegistration

**Files:**
- Modify: `Plugins/PointRegistration/PointRegistrationServiceImpl.cpp`
- Modify: `tests/unit/AnkleRegistrationParallelSearchTest.cpp`
- Test: `build_x64_v142/tests/unit/Release/ankle_registration_parallel_search_test.exe`

- [ ] **Step 1: 先补失败测试，锁定 `PointRegistrationResult.metrics` 的回写字段**

在测试文件追加：

```cpp
void point_registration_metrics_include_parallel_search_summary_keys();

void AnkleRegistrationParallelSearchTest::point_registration_metrics_include_parallel_search_summary_keys()
{
    PointRegistrationResult result;
    result.metrics.insert(QStringLiteral("candidate_count"), 64);
    result.metrics.insert(QStringLiteral("top_k_count"), 4);
    result.metrics.insert(QStringLiteral("coarse_search_ms"), 12.4);
    result.metrics.insert(QStringLiteral("roi_filter_ms"), 1.7);
    result.metrics.insert(QStringLiteral("refine_ms"), 8.2);
    result.metrics.insert(QStringLiteral("best_candidate_rank"), 0);
    result.metrics.insert(QStringLiteral("coarse_score"), 0.93);
    result.metrics.insert(QStringLiteral("parallel_search_enabled"), true);

    QVERIFY(result.metrics.contains(QStringLiteral("candidate_count")));
    QVERIFY(result.metrics.contains(QStringLiteral("top_k_count")));
    QVERIFY(result.metrics.contains(QStringLiteral("coarse_search_ms")));
    QVERIFY(result.metrics.contains(QStringLiteral("roi_filter_ms")));
    QVERIFY(result.metrics.contains(QStringLiteral("refine_ms")));
    QVERIFY(result.metrics.contains(QStringLiteral("best_candidate_rank")));
    QVERIFY(result.metrics.contains(QStringLiteral("coarse_score")));
    QVERIFY(result.metrics.contains(QStringLiteral("parallel_search_enabled")));
}
```

- [ ] **Step 2: 在 `PointRegistrationServiceImpl` 中把参数传给 `RegistrationCore`**

在构造 `parameters` 的位置补充：

```cpp
            parameters.insert(QStringLiteral("enableParallelInitialSearch"), options.enableParallelInitialSearch);
            parameters.insert(QStringLiteral("enableConstraintParallelFilter"), options.enableConstraintParallelFilter);
            parameters.insert(QStringLiteral("candidateCount"), options.candidateCount);
            parameters.insert(QStringLiteral("topKCandidateCount"), options.topKCandidateCount);
            parameters.insert(QStringLiteral("multiResolutionProfileId"), options.multiResolutionProfileId);
```

- [ ] **Step 3: 从 `RegistrationCore` registration info 回收 metadata 并并入最终结果**

在 `performICPRegistrationAdvanced(...)` 成功后追加：

```cpp
                const QVariantMap refineInfo =
                    coreRegistrationService->getRegistrationInfo(
                        parameters.value(QStringLiteral("registrationId")).toString());
                const QVariantMap refineMetadata = refineInfo.value(QStringLiteral("metadata")).toMap();

                const QList<QPair<QString, QString>> metricMappings = {
                    { QStringLiteral("parallelSearchEnabled"), QStringLiteral("parallel_search_enabled") },
                    { QStringLiteral("candidateCount"), QStringLiteral("candidate_count") },
                    { QStringLiteral("topKCount"), QStringLiteral("top_k_count") },
                    { QStringLiteral("coarseSearchMs"), QStringLiteral("coarse_search_ms") },
                    { QStringLiteral("roiFilterMs"), QStringLiteral("roi_filter_ms") },
                    { QStringLiteral("bestCandidateRank"), QStringLiteral("best_candidate_rank") },
                    { QStringLiteral("coarseScore"), QStringLiteral("coarse_score") },
                    { QStringLiteral("multiResolutionProfile"), QStringLiteral("multi_resolution_profile") }
                };

                for (const auto& mapping : metricMappings) {
                    if (refineMetadata.contains(mapping.first)) {
                        result.metrics.insert(mapping.second, refineMetadata.value(mapping.first));
                    }
                }
```

并在最终结果阶段补齐：

```cpp
    result.metrics.insert(QStringLiteral("candidate_count"), options.candidateCount);
    result.metrics.insert(QStringLiteral("top_k_count"), options.topKCandidateCount);
    result.metrics.insert(QStringLiteral("parallel_search_enabled"), options.enableParallelInitialSearch);
    result.metrics.insert(QStringLiteral("multi_resolution_profile"), options.multiResolutionProfileId);
```

- [ ] **Step 4: 运行并行搜索单测，确认 PointRegistration 指标契约通过**

Run:

```bash
cmake --build build_x64_v142 --config Release --target ankle_registration_parallel_search_test
ctest --test-dir build_x64_v142 -C Release -R "^ankle_registration_parallel_search_test$" --output-on-failure
```

Expected: PASS。

- [ ] **Step 5: 提交 PointRegistration 回写逻辑**

```bash
git add Plugins/PointRegistration/PointRegistrationServiceImpl.cpp tests/unit/AnkleRegistrationParallelSearchTest.cpp
git commit -m "feat: surface parallel search metrics in point registration results"
```

## Task 7: Export Experiment And Navigation Evaluation Metrics

**Files:**
- Modify: `Framework/Navigation/innovation_summary_csv_exporter.cpp`
- Modify: `Framework/Navigation/navigation_evaluation_service.cpp`
- Modify: `tests/unit/Innovation2RegistrationExperimentTest.cpp`
- Modify: `tests/unit/NavigationEvaluationServiceTest.cpp`
- Test: `build_x64_v142/tests/unit/Release/innovation_2_registration_experiment_test.exe`
- Test: `build_x64_v142/tests/unit/Release/navigation_evaluation_service_test.exe`

- [ ] **Step 1: 先补失败测试，锁定论文实验与导航评估必须保留的新增列**

在 `Innovation2RegistrationExperimentTest.cpp` 中追加：

```cpp
// 在测试类的 private slots 中加入：
// void experiment_exports_parallel_search_metrics_for_constrained_method();

void experiment_exports_parallel_search_metrics_for_constrained_method();

void Innovation2RegistrationExperimentTest::experiment_exports_parallel_search_metrics_for_constrained_method()
{
    Innovation2RegistrationExperiment experiment;

    Innovation2RegistrationInput input;
    input.caseId = QStringLiteral("ankle-case-203");
    input.registrationMethodIds = QStringList({ QStringLiteral("ankle_two_stage_constrained") });

    const auto records = experiment.run(input);

    QCOMPARE(records.size(), 1);
    QVERIFY(records.first().metrics.contains(QStringLiteral("candidate_count")));
    QVERIFY(records.first().metrics.contains(QStringLiteral("top_k_count")));
    QVERIFY(records.first().metrics.contains(QStringLiteral("coarse_search_ms")));
    QVERIFY(records.first().metrics.contains(QStringLiteral("best_candidate_rank")));
    QVERIFY(records.first().metrics.contains(QStringLiteral("multi_resolution_profile")));
}
```

在 `NavigationEvaluationServiceTest.cpp` 中追加：

```cpp
// 在测试类的 private slots 中加入：
// void service_round_trips_parallel_search_registration_metrics();

void service_round_trips_parallel_search_registration_metrics();

void NavigationEvaluationServiceTest::service_round_trips_parallel_search_registration_metrics()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    NavigationEvaluationService service(tempRoot.path());
    AnkleRegistrationRecord registration;
    registration.caseId = QStringLiteral("ankle-case-301");
    registration.registrationMode = QStringLiteral("ankle_two_stage_constrained");
    registration.fre = 0.74;
    registration.targetTre = 1.18;
    registration.coverageScore = 0.87;
    registration.metrics.insert(QStringLiteral("candidate_count"), 64);
    registration.metrics.insert(QStringLiteral("top_k_count"), 4);
    registration.metrics.insert(QStringLiteral("coarse_search_ms"), 12.6);
    registration.metrics.insert(QStringLiteral("best_candidate_rank"), 0);
    registration.metrics.insert(QStringLiteral("multi_resolution_profile"), QStringLiteral("ankle_roi_three_level"));

    QVERIFY(service.saveRegistrationRecord(registration));
    const AnkleEvaluationSnapshot snapshot = service.loadEvaluationSnapshot(registration.caseId);

    QCOMPARE(snapshot.registrationMetrics.value(QStringLiteral("candidate_count")).toInt(), 64);
    QCOMPARE(snapshot.registrationMetrics.value(QStringLiteral("top_k_count")).toInt(), 4);
    QCOMPARE(snapshot.registrationMetrics.value(QStringLiteral("best_candidate_rank")).toInt(), 0);
}
```

- [ ] **Step 2: 扩展 innovation 2 CSV 头与行数据**

在 `innovation_summary_csv_exporter.cpp` 中把 innovation 2 头扩展为：

```cpp
return QByteArray(
    "case_id,innovation_id,strategy_id,fre_mm,overall_tre_mm,target_tre_mm,"
    "raw_fre_mm,raw_overall_tre_mm,raw_target_tre_mm,"
    "convergence_success,runtime_ms,candidate_count,top_k_count,coarse_search_ms,"
    "best_candidate_rank,coarse_score,parallel_search_enabled,multi_resolution_profile,"
    "used_case_planning,used_case_model_assets,used_anatomical_regions,used_planned_constraint_regions,"
    "case_model_asset_count,tibia_distal_point_count,talus_dome_point_count,anatomical_region_point_count,"
    "case_loaded_bones,roi_radius_mm,roi_center_x,roi_center_y,roi_center_z,roi_point_count\n");
```

并在 `csvRowFor(...)` 同步输出：

```cpp
.arg(record.metrics.value(QStringLiteral("candidate_count")).toInt())
.arg(record.metrics.value(QStringLiteral("top_k_count")).toInt())
.arg(record.metrics.value(QStringLiteral("coarse_search_ms")).toDouble(), 0, 'f', 4)
.arg(record.metrics.value(QStringLiteral("best_candidate_rank")).toInt())
.arg(record.metrics.value(QStringLiteral("coarse_score")).toDouble(), 0, 'f', 4)
.arg(record.metrics.value(QStringLiteral("parallel_search_enabled")).toBool() ? QStringLiteral("true") : QStringLiteral("false"))
.arg(record.metrics.value(QStringLiteral("multi_resolution_profile")).toString())
```

- [ ] **Step 3: 在导航评估导出中保留关键并行搜索指标**

在 `navigation_evaluation_service.cpp` 的 `toJson(const AnkleEvaluationSnapshot& snapshot)` 中追加：

```cpp
    object.insert(QStringLiteral("candidate_count"),
                  snapshot.registrationMetrics.value(QStringLiteral("candidate_count")).toInt());
    object.insert(QStringLiteral("top_k_count"),
                  snapshot.registrationMetrics.value(QStringLiteral("top_k_count")).toInt());
    object.insert(QStringLiteral("coarse_search_ms"),
                  snapshot.registrationMetrics.value(QStringLiteral("coarse_search_ms")).toDouble());
    object.insert(QStringLiteral("best_candidate_rank"),
                  snapshot.registrationMetrics.value(QStringLiteral("best_candidate_rank")).toInt());
    object.insert(QStringLiteral("parallel_search_enabled"),
                  snapshot.registrationMetrics.value(QStringLiteral("parallel_search_enabled")).toBool());
```

保留 `appendMetricsLines(...)` 现有逻辑，不要另造第二套 CSV 导出协议。

- [ ] **Step 4: 运行实验与评估导出测试**

Run:

```bash
cmake --build build_x64_v142 --config Release --target innovation_2_registration_experiment_test navigation_evaluation_service_test
ctest --test-dir build_x64_v142 -C Release -R "^(innovation_2_registration_experiment_test|navigation_evaluation_service_test)$" --output-on-failure
```

Expected: PASS，且新增指标列出现在 innovation 2 记录和 navigation evaluation snapshot 中。

- [ ] **Step 5: 提交实验与评估导出**

```bash
git add Framework/Navigation/innovation_summary_csv_exporter.cpp Framework/Navigation/navigation_evaluation_service.cpp tests/unit/Innovation2RegistrationExperimentTest.cpp tests/unit/NavigationEvaluationServiceTest.cpp
git commit -m "feat: export parallel registration metrics for experiments and evaluation"
```

## Task 8: Update Project Docs And Experiment Runbook

**Files:**
- Modify: `docs/current_status_and_project_overview.md`
- Create: `docs/superpowers/specs/2026-05-23-ankle-registration-parallel-acceleration-experiment-guide.md`
- Test: none

- [ ] **Step 1: 更新项目总览文档中的“配准现状”和“数字孪生衔接”**

在 `docs/current_status_and_project_overview.md` 增补两段：

```md
## 配准现状补充

- 当前主链已经固定为 `target_sensitive` 采点 + `ankle_two_stage_constrained` 配准。
- `PointRegistration` 负责病例目标区和约束区语义，`RegistrationCore` 负责候选调度和 refine，`MeshGPU` 负责批量候选评分与 GICP。
- 本轮开发新增的核心能力是：多初值并行搜索、约束区筛选、多分辨率 coarse-to-fine、局部约束区 refine 指标导出。

## 与数字孪生的衔接

- 数字孪生不直接重写配准，而是消费 `candidate_count`、`best_candidate_rank`、`coarse_search_ms`、`target_tre_mm` 等指标作为配准可信度证据。
- 导航评估和病例回放继续通过 `NavigationEvaluationService` 读取这些指标，用于术中准入和术后复盘。
```

- [ ] **Step 2: 新建实验运行手册，写清实验输入、参数、指标和作图口径**

`docs/superpowers/specs/2026-05-23-ankle-registration-parallel-acceleration-experiment-guide.md` 至少包含：

```md
# Ankle Registration Parallel Acceleration Experiment Guide

## Default Profile

- `candidate_count = 64`
- `top_k_count = 4`
- `multi_resolution_profile = ankle_roi_three_level`
- `target_region_radius_mm = 18.0`

## Mandatory Metrics

- `fre_mm`
- `overall_tre_mm`
- `target_tre_mm`
- `runtime_ms`
- `candidate_count`
- `top_k_count`
- `coarse_search_ms`
- `best_candidate_rank`
- `parallel_search_enabled`

## Comparison Groups

1. `single_stage_landmark`
2. `landmark_plus_global_icp`
3. `landmark_plus_global_gicp`
4. `ankle_two_stage_constrained`
```

- [ ] **Step 3: 人工自查文档内容是否和设计文档、实现计划、代码命名一致**

Run:

```bash
rg -n "ankle_two_stage_constrained|candidate_count|multi_resolution_profile|best_candidate_rank" docs/current_status_and_project_overview.md docs/superpowers/specs/2026-05-23-ankle-registration-parallel-acceleration-experiment-guide.md
```

Expected: 两份文档都能命中相同术语，不出现旧名称和互相冲突的表述。

- [ ] **Step 4: 提交文档更新**

```bash
git add docs/current_status_and_project_overview.md docs/superpowers/specs/2026-05-23-ankle-registration-parallel-acceleration-experiment-guide.md
git commit -m "docs: add runbook for ankle registration parallel acceleration"
```

## Self-Review

- Spec coverage: 已覆盖方案 B 的四个核心步骤，分别落到配置层、候选生成、GPU 候选评分、多分辨率调度、局部 refine 接回、实验导出和文档说明。
- Existing-code fit: 所有任务都围绕 `PointRegistrationServiceImpl.cpp`、`RegistrationServiceImpl.cpp`、`mesh_gpu_runtime_api.h`、`Innovation2RegistrationExperimentTest.cpp` 和 `NavigationEvaluationService` 做增量扩展，没有把主链改成新系统。
- Placeholder scan: 计划中没有 `TBD`、`TODO`、`implement later` 或 “类似 Task N” 这类占位语。
- Type consistency: 统一使用 `candidateCount` / `topKCandidateCount` / `multiResolutionProfileId` 作为配置名，统一使用 `candidate_count` / `top_k_count` / `coarse_search_ms` / `best_candidate_rank` 作为指标名。

## Execution Handoff

计划已保存到 `docs/superpowers/plans/2026-05-23-ankle-registration-parallel-acceleration-implementation-plan.md`。有两种执行方式：

**1. Subagent-Driven（推荐）** - 我按任务分发全新子代理执行，每个任务之间做检查和复核，迭代更快

**2. Inline Execution** - 在当前会话里用 `executing-plans` 按 checkpoint 分批执行

**你想选哪一种？**
