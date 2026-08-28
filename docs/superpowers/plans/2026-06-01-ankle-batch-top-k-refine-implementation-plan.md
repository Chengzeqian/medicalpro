# Ankle Batch Top-K Refine Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 `ankle_two_stage_constrained` 主链新增 `MeshGPU` 批量 `top-K refine` runtime API，并让 `RegistrationCore` 用这条批量路径替代当前上层串行 refine 循环，同时保留旧 DLL 兼容回退。

**Architecture:** 先用测试锁定新的 runtime contract 和 metadata 行为，再在 `mesh_gpu_runtime_api.h` 中新增批量 refine 数据结构与接口。`MeshGPU` runtime 先通过批量入口复用现有单候选 refine 能力返回每个候选的结果，`RegistrationCore` 再将 `top-K` 候选映射到批量请求、按批量结果选择最佳候选，并对 legacy runtime 保持显式 fallback。

**Tech Stack:** C++17, Qt / QTest, VTK, CUDA MeshGPU runtime API, CMake, MSBuild / CTest

---

## File Map

- Modify: `algorithms/meshgpu/include/mesh_gpu_runtime_api.h`
- Modify: `algorithms/meshgpu/src/mesh_gpu_interface.cu`
- Modify: `Plugins/RegistrationCore/RegistrationServiceImpl.h`
- Modify: `Plugins/RegistrationCore/RegistrationServiceImpl.cpp`
- Modify: `tests/unit/RegistrationCoreMeshGpuSmokeTest.cpp`
- Modify: `tests/unit/RegistrationCoreMeshGpuCompatibilityTest.cpp`

说明：

- `mesh_gpu_runtime_api.h` 负责新增批量 refine contract
- `mesh_gpu_interface.cu` 负责 runtime adapter 和底层批量执行入口
- `RegistrationServiceImpl.*` 负责批量请求组装、最佳候选选择和 fallback metadata
- `RegistrationCoreMeshGpuSmokeTest.cpp` 负责新 runtime 与新集成路径的主验证
- `RegistrationCoreMeshGpuCompatibilityTest.cpp` 负责 legacy runtime 回退锁定

### Task 1: 锁定批量 refine contract 的失败测试

**Files:**
- Modify: `tests/unit/RegistrationCoreMeshGpuSmokeTest.cpp`
- Modify: `tests/unit/RegistrationCoreMeshGpuCompatibilityTest.cpp`
- Test: `tests/unit/RegistrationCoreMeshGpuSmokeTest.cpp`
- Test: `tests/unit/RegistrationCoreMeshGpuCompatibilityTest.cpp`

- [ ] **Step 1: 在 smoke test 中先写批量 refine runtime 直测**

在 `RegistrationCoreMeshGpuSmokeTest` 的 `private slots:` 中加入新测试声明：

```cpp
void runtime_batch_refine_returns_result_per_candidate();
void advanced_icp_parallel_search_records_batch_refine_metadata();
```

并先补最小失败测试体：

```cpp
void RegistrationCoreMeshGpuSmokeTest::runtime_batch_refine_returns_result_per_candidate()
{
    const QString runtimeDllPath = QCoreApplication::applicationDirPath() + QStringLiteral("/MeshGPULib.dll");
    QLibrary runtimeLibrary(runtimeDllPath);
    QVERIFY2(runtimeLibrary.load(), qPrintable(runtimeLibrary.errorString()));

    const auto createRuntimeApi =
        reinterpret_cast<CreateRuntimeApiFn>(runtimeLibrary.resolve("CreateMeshGPURuntimeApi"));
    const auto destroyRuntimeApi =
        reinterpret_cast<DestroyRuntimeApiFn>(runtimeLibrary.resolve("DestroyMeshGPURuntimeApi"));
    QVERIFY(createRuntimeApi != nullptr);
    QVERIFY(destroyRuntimeApi != nullptr);

    mesh_gpu::MeshGPURuntimeApi* runtimeApi = createRuntimeApi();
    QVERIFY(runtimeApi != nullptr);

    auto target = createRegistrationSurface();
    auto source = createRegistrationSurface(1.5, -2.0, 3.0);

    QVERIFY(runtimeApi->setTargetMesh(extractPoints(target), extractNormals(target), extractTriangles(target), 1.0f));
    QVERIFY(runtimeApi->setSourcePointCloud(extractPoints(source)));

    std::vector<mesh_gpu::RuntimeRefineCandidateRequest> candidates {
        { 0, createTranslationTransform(0.0f, 0.0f, 0.0f) },
        { 1, createTranslationTransform(-1.5f, 2.0f, -3.0f) }
    };

    mesh_gpu::RegistrationParams params;
    params.max_iterations = 10;
    params.distance_threshold = 30.0f;
    params.use_point_to_plane = true;

    const auto results = runtimeApi->refineTransformCandidates(candidates, params);

    destroyRuntimeApi(runtimeApi);
    runtimeApi = nullptr;
    runtimeLibrary.unload();

    QCOMPARE(static_cast<int>(results.size()), 2);
    QCOMPARE(results.at(0).candidateIndex, 0);
    QCOMPARE(results.at(1).candidateIndex, 1);
    QVERIFY(results.at(0).rmse >= 0.0f);
    QVERIFY(results.at(1).rmse >= 0.0f);
}
```

- [ ] **Step 2: 在 smoke test 中先写批量 refine metadata 集成断言**

把现有 `advanced_icp_parallel_search_refines_top_k_and_records_non_zero_best_candidate_rank()` 扩展为先失败的断言：

```cpp
QCOMPARE(metadata.value(QStringLiteral("batchRefineRequested")).toBool(), true);
QCOMPARE(metadata.value(QStringLiteral("batchRefineEnabled")).toBool(), true);
QCOMPARE(metadata.value(QStringLiteral("batchRefineFallback")).toString(), QString());
QCOMPARE(metadata.value(QStringLiteral("refineCandidateCount")).toInt(), 2);
QVERIFY(metadata.value(QStringLiteral("refineMs")).toLongLong() >= 0);
```

- [ ] **Step 3: 在 compatibility test 中先写 legacy 回退断言**

在 `RegistrationCoreMeshGpuCompatibilityTest` 中补充：

```cpp
if (hasRuntimeApi) {
    QCOMPARE(metadata.value(QStringLiteral("batchRefineRequested")).toBool(), true);
    QCOMPARE(metadata.value(QStringLiteral("batchRefineEnabled")).toBool(), true);
    QCOMPARE(metadata.value(QStringLiteral("batchRefineFallback")).toString(), QString());
} else {
    QCOMPARE(metadata.value(QStringLiteral("batchRefineRequested")).toBool(), true);
    QCOMPARE(metadata.value(QStringLiteral("batchRefineEnabled")).toBool(), false);
    QCOMPARE(
        metadata.value(QStringLiteral("batchRefineFallback")).toString(),
        QStringLiteral("legacy_runtime_without_batch_refine"));
}
```

- [ ] **Step 4: 运行测试目标，确认因为缺少新类型/新接口而失败**

Run:

```bash
cmake --build build_x64 --config Release --target registration_core_meshgpu_smoke_test registration_core_meshgpu_compatibility_test
```

Expected:

```text
FAIL，编译器报错 `RuntimeRefineCandidateRequest` 或 `refineTransformCandidates` 未定义
```

- [ ] **Step 5: 提交失败测试**

```bash
git add tests/unit/RegistrationCoreMeshGpuSmokeTest.cpp tests/unit/RegistrationCoreMeshGpuCompatibilityTest.cpp
git commit -m "test: lock batch refine runtime contract"
```

### Task 2: 增加 runtime API contract 并让编译继续前进

**Files:**
- Modify: `algorithms/meshgpu/include/mesh_gpu_runtime_api.h`
- Test: `tests/unit/RegistrationCoreMeshGpuSmokeTest.cpp`

- [ ] **Step 1: 在 runtime header 中新增请求/结果结构**

在 `mesh_gpu_runtime_api.h` 的 `RuntimeConstraintFilterResult` 后新增：

```cpp
struct RuntimeRefineCandidateRequest {
    int candidateIndex = -1;
    Transform4x4 initialTransform;
};

struct RuntimeRefineCandidateResult {
    int candidateIndex = -1;
    Transform4x4 transform;
    float rmse = 0.0f;
    int iterations = 0;
    bool converged = false;
    bool success = false;
};
```

- [ ] **Step 2: 在 `MeshGPURuntimeApi` 中新增纯虚接口**

加入新签名：

```cpp
virtual std::vector<RuntimeRefineCandidateResult> refineTransformCandidates(
    const std::vector<RuntimeRefineCandidateRequest>& candidates,
    const RegistrationParams& params) = 0;
```

- [ ] **Step 3: 重新编译，确认失败推进到“实现类未实现纯虚函数”**

Run:

```bash
cmake --build build_x64 --config Release --target registration_core_meshgpu_smoke_test registration_core_meshgpu_compatibility_test
```

Expected:

```text
FAIL，错误推进为 `MeshGPURuntimeApiAdapter` / `LegacyMeshGpuRuntimeApiAdapter` 未实现 `refineTransformCandidates`
```

- [ ] **Step 4: 提交 contract 改动**

```bash
git add algorithms/meshgpu/include/mesh_gpu_runtime_api.h
git commit -m "feat: add batch refine runtime api contract"
```

### Task 3: 实现 MeshGPU runtime 批量 refine 入口

**Files:**
- Modify: `algorithms/meshgpu/src/mesh_gpu_interface.cu`
- Modify: `Plugins/RegistrationCore/RegistrationServiceImpl.cpp`
- Test: `tests/unit/RegistrationCoreMeshGpuSmokeTest.cpp`

- [ ] **Step 1: 在 `MeshGPURuntimeApiAdapter` 中实现新接口**

在 `algorithms/meshgpu/src/mesh_gpu_interface.cu` 的 runtime adapter 中新增：

```cpp
std::vector<mesh_gpu::RuntimeRefineCandidateResult> refineTransformCandidates(
    const std::vector<mesh_gpu::RuntimeRefineCandidateRequest>& candidates,
    const mesh_gpu::RegistrationParams& params) override {
    std::vector<mesh_gpu::RuntimeRefineCandidateResult> results;
    results.reserve(candidates.size());

    for (const auto& candidate : candidates) {
        const auto registrationResult = impl_.runRegistration(candidate.initialTransform, params);

        mesh_gpu::RuntimeRefineCandidateResult runtimeResult;
        runtimeResult.candidateIndex = candidate.candidateIndex;
        runtimeResult.transform = registrationResult.transform;
        runtimeResult.rmse = registrationResult.rmse;
        runtimeResult.iterations = registrationResult.iterations;
        runtimeResult.converged = registrationResult.converged;
        runtimeResult.success = registrationResult.converged || registrationResult.iterations > 0 || registrationResult.rmse >= 0.0f;
        results.push_back(runtimeResult);
    }

    return results;
}
```

实现时注意把 `success` 判定收敛到和现有 runtime 结果一致的最小语义，不要额外发明复杂状态。

- [ ] **Step 2: 在 legacy adapter 中实现“显式不支持”的批量接口**

在 `LegacyMeshGpuRuntimeApiAdapter` 中新增：

```cpp
std::vector<mesh_gpu::RuntimeRefineCandidateResult> refineTransformCandidates(
    const std::vector<mesh_gpu::RuntimeRefineCandidateRequest>&,
    const mesh_gpu::RegistrationParams&) override
{
    return {};
}
```

这里故意返回空结果，不在 legacy adapter 内偷偷模拟 batch refine，这样 `RegistrationCore` 可以明确记录 fallback。

- [ ] **Step 3: 重新编译并运行新的 runtime 直测**

Run:

```bash
cmake --build build_x64 --config Release --target registration_core_meshgpu_smoke_test
.\build_x64\Release\registration_core_meshgpu_smoke_test.exe -txt
```

Expected:

```text
`runtime_batch_refine_returns_result_per_candidate` 由编译失败推进到运行期断言；若实现正确，该测试应通过，其他新增 metadata 断言仍可能失败
```

- [ ] **Step 4: 提交 runtime 实现**

```bash
git add algorithms/meshgpu/src/mesh_gpu_interface.cu Plugins/RegistrationCore/RegistrationServiceImpl.cpp
git commit -m "feat: add meshgpu batch refine runtime entry"
```

### Task 4: 接通 RegistrationCore 的批量 refine 路径

**Files:**
- Modify: `Plugins/RegistrationCore/RegistrationServiceImpl.h`
- Modify: `Plugins/RegistrationCore/RegistrationServiceImpl.cpp`
- Test: `tests/unit/RegistrationCoreMeshGpuSmokeTest.cpp`
- Test: `tests/unit/RegistrationCoreMeshGpuCompatibilityTest.cpp`

- [ ] **Step 1: 在头文件中声明批量 refine 辅助函数和可用性标志**

在 `RegistrationServiceImpl.h` 中新增：

```cpp
QList<mesh_gpu::RuntimeRefineCandidateRequest> buildBatchRefineRequests(
    const QList<CandidateInitialTransform>& candidateTransforms,
    const QList<CandidateEvaluationResult>& topKCandidateScores) const;

bool m_meshGPUBatchRefineAvailable = false;
```

如果你更倾向于把 helper 做成 `static` 或匿名命名空间函数，也可以，但必须保证 `m_meshGPUBatchRefineAvailable` 被显式维护。

- [ ] **Step 2: 在 DLL 加载逻辑中维护 batch refine 能力标志**

在 `loadMeshGPUDLL(...)` 的 runtime 成功分支中加入：

```cpp
m_meshGPUCandidateScoringAvailable = true;
m_meshGPUBatchRefineAvailable = true;
```

在 legacy adapter 分支中加入：

```cpp
m_meshGPUCandidateScoringAvailable = false;
m_meshGPUBatchRefineAvailable = false;
```

并在析构或卸载重置处把该标志恢复为 `false`。

- [ ] **Step 3: 实现请求组装 helper**

在 `RegistrationServiceImpl.cpp` 中新增一个最小 helper：

```cpp
QList<mesh_gpu::RuntimeRefineCandidateRequest> RegistrationServiceImpl::buildBatchRefineRequests(
    const QList<CandidateInitialTransform>& candidateTransforms,
    const QList<CandidateEvaluationResult>& topKCandidateScores) const
{
    QList<mesh_gpu::RuntimeRefineCandidateRequest> requests;

    for (int scoreIndex = 0; scoreIndex < topKCandidateScores.size(); ++scoreIndex) {
        const QString candidateId = topKCandidateScores.at(scoreIndex).candidateId;
        for (const CandidateInitialTransform& candidateTransform : candidateTransforms) {
            if (candidateTransform.candidateId != candidateId) {
                continue;
            }

            mesh_gpu::RuntimeRefineCandidateRequest request;
            request.candidateIndex = scoreIndex;
            request.initialTransform = qMatrixToMeshGpuTransform(candidateTransform.transformMatrix);
            requests.append(request);
            break;
        }
    }

    return requests;
}
```

- [ ] **Step 4: 用批量 refine 替换现有 `top-K` 串行 refine 循环**

把当前 `for (int scoreIndex = 0; scoreIndex < topKCandidateScores.size(); ++scoreIndex)` 那段改成：

```cpp
const bool batchRefineRequested = !topKCandidateScores.isEmpty();
bool batchRefineEnabled = false;
QString batchRefineFallback;

if (batchRefineRequested && m_meshGPUBatchRefineAvailable) {
    const QList<mesh_gpu::RuntimeRefineCandidateRequest> requestList =
        buildBatchRefineRequests(candidateTransforms, topKCandidateScores);
    std::vector<mesh_gpu::RuntimeRefineCandidateRequest> requests(
        requestList.begin(),
        requestList.end());

    const mesh_gpu::RegistrationParams refineParams = toMeshGpuRegistrationParams(parameters);
    const std::vector<mesh_gpu::RuntimeRefineCandidateResult> refineResults =
        m_meshGPU->refineTransformCandidates(requests, refineParams);

    if (!refineResults.empty()) {
        batchRefineEnabled = true;
        for (const auto& refineResult : refineResults) {
            if (!refineResult.success) {
                continue;
            }

            if (refineResult.rmse < bestRefineScore) {
                bestRefineScore = refineResult.rmse;
                selectedInitialMatrix = meshGPUTransformToVTK(refineResult.transform.data);
                selectedRank = refineResult.candidateIndex;
            }
        }
    } else {
        batchRefineFallback = QStringLiteral("legacy_runtime_without_batch_refine");
    }
}

if (!batchRefineEnabled) {
    batchRefineFallback = batchRefineFallback.isEmpty()
        ? QStringLiteral("legacy_runtime_without_batch_refine")
        : batchRefineFallback;
    // 保留当前串行 refine 循环作为 fallback
}
```

这里的 `toMeshGpuRegistrationParams(parameters)` 如果项目里没有现成 helper，就在 `RegistrationServiceImpl.cpp` 的匿名命名空间里补一个最小转换函数，把现有 `maxIterations / distanceThreshold / usePointToPlane / verbose` 映射到 `mesh_gpu::RegistrationParams`。

- [ ] **Step 5: 把 batch refine 状态写入 metadata**

在构建 `parallelSearchReport` 之前或之后加入：

```cpp
gpuParameters.insert(QStringLiteral("parallelSearchReport"), buildParallelSearchReport(...));

QVariantMap parallelSearchReport = gpuParameters.value(QStringLiteral("parallelSearchReport")).toMap();
parallelSearchReport.insert(QStringLiteral("batchRefineRequested"), batchRefineRequested);
parallelSearchReport.insert(QStringLiteral("batchRefineEnabled"), batchRefineEnabled);
parallelSearchReport.insert(QStringLiteral("batchRefineFallback"), batchRefineFallback);
gpuParameters.insert(QStringLiteral("parallelSearchReport"), parallelSearchReport);
```

保证新 runtime 路径时：

```cpp
batchRefineRequested == true
batchRefineEnabled == true
batchRefineFallback == ""
```

legacy fallback 时：

```cpp
batchRefineRequested == true
batchRefineEnabled == false
batchRefineFallback == "legacy_runtime_without_batch_refine"
```

- [ ] **Step 6: 跑 smoke 与 compatibility 测试确认转绿**

Run:

```bash
cmake --build build_x64 --config Release --target registration_core_meshgpu_smoke_test registration_core_meshgpu_compatibility_test
ctest --test-dir build_x64 -C Release -R "^(registration_core_meshgpu_smoke_test|registration_core_meshgpu_compatibility_test)$" --output-on-failure
.\build_x64\Release\registration_core_meshgpu_smoke_test.exe -txt
```

Expected:

```text
两组测试通过，新增 runtime batch refine 断言和 metadata/fallback 断言全部为绿
```

- [ ] **Step 7: 提交接线与回退逻辑**

```bash
git add Plugins/RegistrationCore/RegistrationServiceImpl.h Plugins/RegistrationCore/RegistrationServiceImpl.cpp tests/unit/RegistrationCoreMeshGpuSmokeTest.cpp tests/unit/RegistrationCoreMeshGpuCompatibilityTest.cpp
git commit -m "feat: integrate batch top-k refine into registration core"
```

### Task 5: 完整回归与结果核对

**Files:**
- Modify: `algorithms/meshgpu/include/mesh_gpu_runtime_api.h`
- Modify: `algorithms/meshgpu/src/mesh_gpu_interface.cu`
- Modify: `Plugins/RegistrationCore/RegistrationServiceImpl.h`
- Modify: `Plugins/RegistrationCore/RegistrationServiceImpl.cpp`
- Modify: `tests/unit/RegistrationCoreMeshGpuSmokeTest.cpp`
- Modify: `tests/unit/RegistrationCoreMeshGpuCompatibilityTest.cpp`

- [ ] **Step 1: 做一次完整的 MeshGPU 相关 fresh 验证**

Run:

```bash
Copy-Item .\build_ninja_cuda124\Release\MeshGPULib.dll .\build_x64\Release\MeshGPULib.dll -Force
cmake --build build_x64 --config Release --target registration_core_meshgpu_smoke_test registration_core_meshgpu_compatibility_test
ctest --test-dir build_x64 -C Release -R "^(registration_core_meshgpu_smoke_test|registration_core_meshgpu_compatibility_test)$" --output-on-failure
.\build_x64\Release\registration_core_meshgpu_smoke_test.exe -txt
```

Expected:

```text
build 成功，ctest 通过，文本 smoke 输出中包含 runtime 初始化、candidate GPU scoring 和新增 batch refine 路径的正常执行
```

- [ ] **Step 2: 人工复核行为边界**

核对这 4 点：

```text
1. 上层不再默认持有 top-K 串行 refine 作为主路径
2. 新 runtime 走 batch refine，legacy runtime 明确 fallback
3. metadata 明确区分 requested / enabled / fallback
4. 没有把本轮实现表述成单 kernel batch GICP
```

- [ ] **Step 3: 提交最终验证状态**

```bash
git add algorithms/meshgpu/include/mesh_gpu_runtime_api.h algorithms/meshgpu/src/mesh_gpu_interface.cu Plugins/RegistrationCore/RegistrationServiceImpl.h Plugins/RegistrationCore/RegistrationServiceImpl.cpp tests/unit/RegistrationCoreMeshGpuSmokeTest.cpp tests/unit/RegistrationCoreMeshGpuCompatibilityTest.cpp
git commit -m "test: verify batch top-k refine runtime path"
```
