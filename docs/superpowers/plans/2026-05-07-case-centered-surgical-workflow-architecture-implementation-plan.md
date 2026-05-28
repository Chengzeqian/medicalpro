# Case-Centered Surgical Workflow Architecture Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 建立病例中心化的数据关系，把资产主库、病例绑定、工作区运行结果和页面快照分层落到现有工程中。

**Architecture:** 这一轮实现先不重写整个中台 UI，而是在现有 `Framework/Navigation` 持久化层里补齐 `case_asset_bindings.json` 及相关类型，再让 `DashboardPage` 明确消费病例工作包，给导航工作区提供稳定输入。真实病例引导链路继续沿用 `real_case_asset_bootstrapper` 和 `real_case_workspace_seed_coordinator`，但产物从只写 `case_manifest.json` 扩展为同时写病例绑定文件。

**Tech Stack:** C++20, Qt Core/Widgets, JSON persistence, existing `Framework` library, `NewPagesLib`, QtTest

---

### Task 1: 定义病例绑定数据模型与仓储接口

**Files:**
- Modify: `Framework/Navigation/ankle_navigation_types.h`
- Modify: `Framework/Navigation/ankle_case_workspace_repository.h`
- Test: `tests/unit/AnkleCaseWorkspaceRepositoryTest.cpp`

- [ ] **Step 1: 先写失败测试，锁定 `case_asset_bindings.json` 的基本契约**

```cpp
void AnkleCaseWorkspaceRepositoryTest::case_workspace_persists_case_asset_bindings_for_multiple_bones_and_instruments()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    AnkleCaseWorkspaceRepository repo(tempRoot.path());

    AnkleCaseManifest manifest;
    manifest.caseId = QStringLiteral("ankle-case-bindings-001");
    manifest.patientId = QStringLiteral("patient-001");
    manifest.patientName = QStringLiteral("Patient 001");
    QVERIFY(repo.createCaseWorkspace(manifest));

    AnkleCaseAssetBindings bindings;
    bindings.caseId = manifest.caseId;
    bindings.boundBoneAssetIds = { QStringLiteral("bone-tibia"), QStringLiteral("bone-talus") };
    bindings.activeBoneAssetIds = bindings.boundBoneAssetIds;
    bindings.boundInstrumentAssetIds = { QStringLiteral("probe-main"), QStringLiteral("tool-guide") };
    bindings.instrumentGeometryBindings = {
        AnkleInstrumentGeometryBinding { QStringLiteral("probe-main"), QStringLiteral("geometry-probe"), QStringLiteral("geometry/probe.ini") },
        AnkleInstrumentGeometryBinding { QStringLiteral("tool-guide"), QStringLiteral("geometry-guide"), QStringLiteral("geometry/guide.ini") }
    };

    QVERIFY(repo.saveCaseAssetBindings(bindings));

    const AnkleCaseAssetBindings restored = repo.loadCaseAssetBindings(manifest.caseId);
    QCOMPARE(restored.boundBoneAssetIds, bindings.boundBoneAssetIds);
    QCOMPARE(restored.boundInstrumentAssetIds, bindings.boundInstrumentAssetIds);
    QCOMPARE(restored.instrumentGeometryBindings.size(), 2);
}
```

- [ ] **Step 2: 运行测试确认当前失败**

Run: `ctest --test-dir build_x64_v142 -C Release -R ankle_case_workspace_repository_test --output-on-failure`
Expected: FAIL，提示 `AnkleCaseAssetBindings` 或 `saveCaseAssetBindings/loadCaseAssetBindings` 尚不存在

- [ ] **Step 3: 在 `ankle_navigation_types.h` 中补齐病例绑定类型**

```cpp
struct AnkleInstrumentGeometryBinding
{
    QString instrumentAssetId;
    QString geometryAssetId;
    QString geometryFilePath;
};

struct AnkleCaseAssetBindings
{
    QString caseId;
    QStringList boundBoneAssetIds;
    QStringList activeBoneAssetIds;
    QStringList boundInstrumentAssetIds;
    QStringList activeInstrumentAssetIds;
    QList<AnkleInstrumentGeometryBinding> instrumentGeometryBindings;
    QString createdAtIso;
    QString updatedAtIso;
};
```

- [ ] **Step 4: 在仓储头文件里声明绑定文件读写与路径接口**

```cpp
class FRAMEWORK_EXPORT AnkleCaseWorkspaceRepository
{
public:
    explicit AnkleCaseWorkspaceRepository(const QString& dataRoot);

    bool createCaseWorkspace(AnkleCaseManifest& manifest) const;
    bool saveManifest(const AnkleCaseManifest& manifest) const;
    AnkleCaseManifest loadManifest(const QString& caseId) const;
    bool saveCaseAssetBindings(const AnkleCaseAssetBindings& bindings) const;
    AnkleCaseAssetBindings loadCaseAssetBindings(const QString& caseId) const;

    QString caseRoot(const QString& caseId) const;
    QString manifestPath(const QString& caseId) const;
    QString caseAssetBindingsPath(const QString& caseId) const;
    QString stagePath(const QString& caseId, const QString& stageName) const;
};
```

- [ ] **Step 5: 重新构建相关测试目标，确认声明阶段可编译**

Run: `cmake --build build_x64_v142 --config Release --target ankle_case_workspace_repository_test`
Expected: BUILD SUCCESS

- [ ] **Step 6: Commit**

```bash
git add Framework/Navigation/ankle_navigation_types.h Framework/Navigation/ankle_case_workspace_repository.h tests/unit/AnkleCaseWorkspaceRepositoryTest.cpp
git commit -m "feat: define case asset binding domain types"
```

---

### Task 2: 实现 `case_asset_bindings.json` 持久化

**Files:**
- Modify: `Framework/Navigation/ankle_case_workspace_repository.cpp`
- Modify: `tests/unit/AnkleCaseWorkspaceRepositoryTest.cpp`

- [ ] **Step 1: 扩展失败测试，验证绑定文件路径和 round-trip**

```cpp
void AnkleCaseWorkspaceRepositoryTest::case_workspace_binding_file_round_trips_as_json_truth_source()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    AnkleCaseWorkspaceRepository repo(tempRoot.path());
    AnkleCaseManifest manifest;
    manifest.caseId = QStringLiteral("ankle-case-bindings-002");
    QVERIFY(repo.createCaseWorkspace(manifest));

    AnkleCaseAssetBindings bindings;
    bindings.caseId = manifest.caseId;
    bindings.boundBoneAssetIds = { QStringLiteral("bone-tibia") };
    QVERIFY(repo.saveCaseAssetBindings(bindings));

    QVERIFY(QFileInfo::exists(repo.caseAssetBindingsPath(manifest.caseId)));
    const AnkleCaseAssetBindings restored = repo.loadCaseAssetBindings(manifest.caseId);
    QCOMPARE(restored.caseId, manifest.caseId);
    QCOMPARE(restored.boundBoneAssetIds, QStringList({ QStringLiteral("bone-tibia") }));
}
```

- [ ] **Step 2: 运行测试确认实现前失败**

Run: `ctest --test-dir build_x64_v142 -C Release -R ankle_case_workspace_repository_test --output-on-failure`
Expected: FAIL，提示 `case_asset_bindings.json` 未写入或读取为空

- [ ] **Step 3: 在仓储实现里加入 JSON 编解码与路径实现**

```cpp
QString AnkleCaseWorkspaceRepository::caseAssetBindingsPath(const QString& caseId) const
{
    return caseRoot(caseId) + QStringLiteral("/case_asset_bindings.json");
}

bool AnkleCaseWorkspaceRepository::saveCaseAssetBindings(const AnkleCaseAssetBindings& bindings) const
{
    if (bindings.caseId.isEmpty() || !ensureDir(caseRoot(bindings.caseId))) {
        return false;
    }

    QJsonArray geometryBindings;
    for (const AnkleInstrumentGeometryBinding& binding : bindings.instrumentGeometryBindings) {
        QJsonObject object;
        object.insert(QStringLiteral("instrument_asset_id"), binding.instrumentAssetId);
        object.insert(QStringLiteral("geometry_asset_id"), binding.geometryAssetId);
        object.insert(QStringLiteral("geometry_file_path"), binding.geometryFilePath);
        geometryBindings.append(object);
    }

    QJsonObject root;
    root.insert(QStringLiteral("case_id"), bindings.caseId);
    root.insert(QStringLiteral("bound_bone_asset_ids"), QJsonArray::fromStringList(bindings.boundBoneAssetIds));
    root.insert(QStringLiteral("active_bone_asset_ids"), QJsonArray::fromStringList(bindings.activeBoneAssetIds));
    root.insert(QStringLiteral("bound_instrument_asset_ids"), QJsonArray::fromStringList(bindings.boundInstrumentAssetIds));
    root.insert(QStringLiteral("active_instrument_asset_ids"), QJsonArray::fromStringList(bindings.activeInstrumentAssetIds));
    root.insert(QStringLiteral("instrument_geometry_bindings"), geometryBindings);
    root.insert(QStringLiteral("created_at_iso"), bindings.createdAtIso);
    root.insert(QStringLiteral("updated_at_iso"), bindings.updatedAtIso);

    QFile file(caseAssetBindingsPath(bindings.caseId));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return file.error() == QFile::NoError;
}
```

- [ ] **Step 4: 实现加载逻辑并在缺失文件时返回空绑定对象**

```cpp
AnkleCaseAssetBindings AnkleCaseWorkspaceRepository::loadCaseAssetBindings(const QString& caseId) const
{
    QFile file(caseAssetBindingsPath(caseId));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return {};
    }

    const QJsonObject root = document.object();
    AnkleCaseAssetBindings bindings;
    bindings.caseId = root.value(QStringLiteral("case_id")).toString();
    bindings.boundBoneAssetIds = toStringList(root.value(QStringLiteral("bound_bone_asset_ids")).toArray());
    bindings.activeBoneAssetIds = toStringList(root.value(QStringLiteral("active_bone_asset_ids")).toArray());
    bindings.boundInstrumentAssetIds = toStringList(root.value(QStringLiteral("bound_instrument_asset_ids")).toArray());
    bindings.activeInstrumentAssetIds = toStringList(root.value(QStringLiteral("active_instrument_asset_ids")).toArray());
    return bindings;
}
```

- [ ] **Step 5: 运行仓储测试**

Run: `ctest --test-dir build_x64_v142 -C Release -R ankle_case_workspace_repository_test --output-on-failure`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add Framework/Navigation/ankle_case_workspace_repository.cpp tests/unit/AnkleCaseWorkspaceRepositoryTest.cpp
git commit -m "feat: persist case asset bindings in repository"
```

---

### Task 3: 让真实病例引导链路写出病例工作包

**Files:**
- Modify: `Framework/Navigation/real_case_asset_bootstrapper.h`
- Modify: `Framework/Navigation/real_case_asset_bootstrapper.cpp`
- Modify: `Framework/Navigation/real_case_workspace_seed_coordinator.cpp`
- Modify: `tests/unit/RealCaseAssetBootstrapperTest.cpp`
- Modify: `tests/unit/RealCaseWorkspaceSeedCoordinatorTest.cpp`

- [ ] **Step 1: 先写失败测试，要求 bootstrap 同时写绑定文件**

```cpp
void RealCaseAssetBootstrapperTest::bootstrap_writes_case_asset_bindings_for_seeded_bones()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    RealCaseAssetBootstrapRequest request;
    request.dataRoot = tempRoot.path();
    request.caseId = QStringLiteral("ankle-case-real-import-002");
    request.patientId = QStringLiteral("45971129749");
    request.patientName = QStringLiteral("Real Case 45971129749");
    request.surgeryId = QStringLiteral("ankle-navigation-real-import-002");
    request.tibiaModelPath = externalTibiaPath;
    request.talusModelPath = externalTalusPath;

    RealCaseAssetBootstrapper bootstrapper;
    QVERIFY(bootstrapper.bootstrap(request));

    AnkleCaseWorkspaceRepository repository(tempRoot.path());
    const AnkleCaseAssetBindings bindings = repository.loadCaseAssetBindings(request.caseId);
    QCOMPARE(bindings.boundBoneAssetIds, QStringList({ QStringLiteral("bone:tibia"), QStringLiteral("bone:talus") }));
    QCOMPARE(bindings.activeBoneAssetIds, bindings.boundBoneAssetIds);
}
```

- [ ] **Step 2: 运行 bootstrap/seed 测试确认失败**

Run: `ctest --test-dir build_x64_v142 -C Release -R "real_case_asset_bootstrapper_test|real_case_workspace_seed_coordinator_test" --output-on-failure`
Expected: FAIL，提示没有生成病例绑定文件

- [ ] **Step 3: 扩展 bootstrap 请求，允许写入默认探针/器械绑定**

```cpp
struct RealCaseAssetBootstrapRequest
{
    QString dataRoot;
    QString caseId;
    QString patientId;
    QString patientName;
    QString surgeryId;
    QString tibiaModelPath;
    QString talusModelPath;
    QStringList defaultInstrumentAssetIds;
    QList<AnkleInstrumentGeometryBinding> defaultInstrumentGeometryBindings;
    QVector3D targetRegionCenter;
    double targetRegionRadiusMm = 0.0;
};
```

- [ ] **Step 4: 在 bootstrap 实现里保存绑定文件**

```cpp
AnkleCaseAssetBindings bindings;
bindings.caseId = request.caseId;
bindings.boundBoneAssetIds = { QStringLiteral("bone:tibia"), QStringLiteral("bone:talus") };
bindings.activeBoneAssetIds = bindings.boundBoneAssetIds;
bindings.boundInstrumentAssetIds = request.defaultInstrumentAssetIds;
bindings.activeInstrumentAssetIds = request.defaultInstrumentAssetIds;
bindings.instrumentGeometryBindings = request.defaultInstrumentGeometryBindings;
bindings.createdAtIso = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
bindings.updatedAtIso = bindings.createdAtIso;

if (!repository.saveCaseAssetBindings(bindings)) {
    return false;
}
```

- [ ] **Step 5: 在 seed coordinator 中补默认绑定**

```cpp
request.defaultInstrumentAssetIds = {
    QStringLiteral("instrument:probe-main"),
    QStringLiteral("instrument:guide-default")
};
request.defaultInstrumentGeometryBindings = {
    AnkleInstrumentGeometryBinding {
        QStringLiteral("instrument:probe-main"),
        QStringLiteral("geometry:probe-main"),
        QStringLiteral("geometry/probe-main.ini")
    },
    AnkleInstrumentGeometryBinding {
        QStringLiteral("instrument:guide-default"),
        QStringLiteral("geometry:guide-default"),
        QStringLiteral("geometry/guide-default.ini")
    }
};
```

- [ ] **Step 6: 运行真实病例引导测试**

Run: `ctest --test-dir build_x64_v142 -C Release -R "real_case_asset_bootstrapper_test|real_case_workspace_seed_coordinator_test|ankle_case_workspace_repository_test" --output-on-failure`
Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add Framework/Navigation/real_case_asset_bootstrapper.h Framework/Navigation/real_case_asset_bootstrapper.cpp Framework/Navigation/real_case_workspace_seed_coordinator.cpp tests/unit/RealCaseAssetBootstrapperTest.cpp tests/unit/RealCaseWorkspaceSeedCoordinatorTest.cpp
git commit -m "feat: seed case workspace package bindings"
```

---

### Task 4: 让病例工作台消费病例工作包并约束导航入口

**Files:**
- Create: `Framework/Navigation/case_workspace_package_service.h`
- Create: `Framework/Navigation/case_workspace_package_service.cpp`
- Modify: `UI/NewPages/DashboardPage.h`
- Modify: `UI/NewPages/DashboardPage.cpp`
- Modify: `UI/MainInterfaceWidget.cpp`
- Modify: `UI/NewPages/CMakeLists.txt`
- Modify: `CMakeLists.txt`
- Modify: `tests/unit/CMakeLists.txt`
- Create: `tests/unit/CaseWorkspacePackageServiceTest.cpp`

- [ ] **Step 1: 写失败测试，要求服务能返回病例工作包摘要**

```cpp
void CaseWorkspacePackageServiceTest::service_loads_case_workspace_package_with_bound_bones_and_instruments()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    AnkleCaseWorkspaceRepository repository(tempRoot.path());
    AnkleCaseManifest manifest;
    manifest.caseId = QStringLiteral("ankle-case-package-001");
    QVERIFY(repository.createCaseWorkspace(manifest));

    AnkleCaseAssetBindings bindings;
    bindings.caseId = manifest.caseId;
    bindings.boundBoneAssetIds = { QStringLiteral("bone:tibia"), QStringLiteral("bone:talus") };
    bindings.boundInstrumentAssetIds = { QStringLiteral("instrument:probe-main") };
    QVERIFY(repository.saveCaseAssetBindings(bindings));

    CaseWorkspacePackageService service(tempRoot.path());
    const CaseWorkspacePackageSummary summary = service.loadSummary(manifest.caseId);
    QCOMPARE(summary.boundBoneCount, 2);
    QCOMPARE(summary.boundInstrumentCount, 1);
    QCOMPARE(summary.readyForNavigation, true);
}
```

- [ ] **Step 2: 运行测试确认服务不存在**

Run: `ctest --test-dir build_x64_v142 -C Release -R case_workspace_package_service_test --output-on-failure`
Expected: FAIL，提示 `CaseWorkspacePackageService` 目标未定义

- [ ] **Step 3: 新建服务文件并实现摘要聚合**

```cpp
struct CaseWorkspacePackageSummary
{
    QString caseId;
    int boundBoneCount = 0;
    int activeBoneCount = 0;
    int boundInstrumentCount = 0;
    int geometryBindingCount = 0;
    bool readyForNavigation = false;
};

CaseWorkspacePackageSummary CaseWorkspacePackageService::loadSummary(const QString& caseId) const
{
    const AnkleCaseManifest manifest = m_repository.loadManifest(caseId);
    const AnkleCaseAssetBindings bindings = m_repository.loadCaseAssetBindings(caseId);

    CaseWorkspacePackageSummary summary;
    summary.caseId = caseId;
    summary.boundBoneCount = bindings.boundBoneAssetIds.size();
    summary.activeBoneCount = bindings.activeBoneAssetIds.size();
    summary.boundInstrumentCount = bindings.boundInstrumentAssetIds.size();
    summary.geometryBindingCount = bindings.instrumentGeometryBindings.size();
    summary.readyForNavigation = !manifest.caseId.isEmpty()
        && summary.boundBoneCount > 0
        && summary.boundInstrumentCount > 0
        && summary.geometryBindingCount > 0;
    return summary;
}
```

- [ ] **Step 4: 在 `DashboardPage` 中显示病例工作包摘要并用它收紧导航 CTA**

```cpp
void DashboardPageNew::updateNavigationCta(bool patientSelected)
{
    const CaseWorkspacePackageSummary summary =
        m_caseWorkspacePackageService.loadSummary(m_currentCaseId);
    const bool readyForNavigation = patientSelected
        && summary.readyForNavigation
        && m_currentDicomStudyCount >= 0;

    ui->navigationCtaTitleLabel->setText(
        readyForNavigation ? QStringLiteral("病例工作包已就绪") : QStringLiteral("先补齐病例工作包"));
    ui->navigationCtaHintLabel->setText(QStringLiteral(
        "骨数量：%1，器械数量：%2，几何绑定：%3。")
        .arg(summary.boundBoneCount)
        .arg(summary.boundInstrumentCount)
        .arg(summary.geometryBindingCount));
    ui->enterNavigationButton->setEnabled(readyForNavigation);
}
```

- [ ] **Step 5: 把新服务加入主工程和测试构建**

```cmake
set(FRAMEWORK_SOURCES
    Framework/Navigation/case_workspace_package_service.h
    Framework/Navigation/case_workspace_package_service.cpp
)
```

```cmake
add_executable(case_workspace_package_service_test
    CaseWorkspacePackageServiceTest.cpp
    ${CMAKE_SOURCE_DIR}/Framework/Navigation/ankle_case_workspace_repository.cpp
    ${CMAKE_SOURCE_DIR}/Framework/Navigation/case_workspace_package_service.cpp
)
```

- [ ] **Step 6: 运行服务与现有仓储测试**

Run: `ctest --test-dir build_x64_v142 -C Release -R "case_workspace_package_service_test|ankle_case_workspace_repository_test|real_case_asset_bootstrapper_test|real_case_workspace_seed_coordinator_test" --output-on-failure`
Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add Framework/Navigation/case_workspace_package_service.h Framework/Navigation/case_workspace_package_service.cpp UI/NewPages/DashboardPage.h UI/NewPages/DashboardPage.cpp UI/MainInterfaceWidget.cpp UI/NewPages/CMakeLists.txt CMakeLists.txt tests/unit/CMakeLists.txt tests/unit/CaseWorkspacePackageServiceTest.cpp
git commit -m "feat: surface case workspace package readiness in case workbench"
```

---

## Self-Review

### Spec coverage

- 资产主库与病例绑定分层：Task 1, Task 2
- `case_asset_bindings.json` 持久化：Task 1, Task 2
- 真实病例工作包生成：Task 3
- 病例工作台消费病例工作包：Task 4
- 导航入口只消费病例绑定结果：Task 4

### Placeholder scan

- 所有任务都给出了具体文件路径、测试名、命令和代码片段
- 未使用 `TODO`、`TBD`、`implement later`

### Type consistency

- `AnkleCaseAssetBindings`、`AnkleInstrumentGeometryBinding` 在所有任务中名称一致
- `case_asset_bindings.json` 在仓储、bootstrap、Dashboard 汇总中路径一致
- `CaseWorkspacePackageService` 作为病例工作包只读聚合入口保持命名一致

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-05-07-case-centered-surgical-workflow-architecture-implementation-plan.md`. Two execution options:

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

Which approach?
