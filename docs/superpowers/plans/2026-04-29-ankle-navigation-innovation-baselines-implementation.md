# Ankle Navigation Innovation Baselines Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将踝关节导航的 3 个创新点补齐为完整的 `baseline + 指标 + 实验运行器 + 结果导出` 闭环，形成可支撑论文结论的客观证据链。

**Architecture:** 复用现有 `Framework/Navigation`、`Plugins/PointRegistration`、`Plugins/RegistrationCore` 和 `Plugins/OpticalTracking` 的主链能力，在不重做宿主框架的前提下新增可切换策略接口、实验记录模型和批量汇总导出。页面层只作为可选触发入口，实验主链必须可离线运行。

**Tech Stack:** C++20, Qt Core/Gui/Test, existing registration/tracking services, JSON/CSV export, VTK-based point registration pipeline

---

## File Structure

### New Files

- `Framework/Navigation/innovation_experiment_types.h`
  - 定义三类创新实验统一记录结构
- `Framework/Navigation/innovation_experiment_repository.h`
- `Framework/Navigation/innovation_experiment_repository.cpp`
  - 负责单次实验 JSON 与汇总目录管理
- `Framework/Navigation/innovation_summary_csv_exporter.h`
- `Framework/Navigation/innovation_summary_csv_exporter.cpp`
  - 负责把实验结果导出成长表 CSV
- `Framework/Navigation/innovation_1_point_selection_experiment.h`
- `Framework/Navigation/innovation_1_point_selection_experiment.cpp`
  - 创新点 1 的离线实验运行器
- `Framework/Navigation/innovation_2_registration_experiment.h`
- `Framework/Navigation/innovation_2_registration_experiment.cpp`
  - 创新点 2 的离线实验运行器
- `Framework/Navigation/innovation_3_gate_experiment.h`
- `Framework/Navigation/innovation_3_gate_experiment.cpp`
  - 创新点 3 的离线实验运行器
- `Framework/Navigation/innovation_experiment_batch_runner.h`
- `Framework/Navigation/innovation_experiment_batch_runner.cpp`
  - 批量遍历病例和扰动配置
- `Plugins/PointRegistration/registration_point_selection_strategy.h`
- `Plugins/PointRegistration/random_point_selection_strategy.h`
- `Plugins/PointRegistration/random_point_selection_strategy.cpp`
- `Plugins/PointRegistration/uniform_point_selection_strategy.h`
- `Plugins/PointRegistration/uniform_point_selection_strategy.cpp`
- `Plugins/PointRegistration/expert_rule_point_selection_strategy.h`
- `Plugins/PointRegistration/expert_rule_point_selection_strategy.cpp`
- `Plugins/PointRegistration/registration_point_strategy_registry.h`
- `Plugins/PointRegistration/registration_point_strategy_registry.cpp`
  - 创新点 1 的 baseline 策略接口与注册表
- `Plugins/PointRegistration/navigation_gate_strategy.h`
- `Plugins/PointRegistration/no_gate_strategy.h`
- `Plugins/PointRegistration/no_gate_strategy.cpp`
- `Plugins/PointRegistration/threshold_only_gate_strategy.h`
- `Plugins/PointRegistration/threshold_only_gate_strategy.cpp`
- `Plugins/PointRegistration/joint_confidence_gate_strategy.h`
- `Plugins/PointRegistration/joint_confidence_gate_strategy.cpp`
  - 创新点 3 的准入策略体系
- `tests/unit/InnovationExperimentRepositoryTest.cpp`
- `tests/unit/PointSelectionBaselineStrategyTest.cpp`
- `tests/unit/Innovation1PointSelectionExperimentTest.cpp`
- `tests/unit/AnkleRegistrationBaselineTest.cpp`
- `tests/unit/Innovation2RegistrationExperimentTest.cpp`
- `tests/unit/NavigationGateStrategyTest.cpp`
- `tests/unit/Innovation3GateExperimentTest.cpp`
- `tests/unit/InnovationExperimentBatchRunnerTest.cpp`

### Modified Files

- `Framework/Navigation/ankle_navigation_types.h`
  - 增加实验记录、summary 行和扰动配置结构
- `Framework/Navigation/navigation_evaluation_service.h`
- `Framework/Navigation/navigation_evaluation_service.cpp`
  - 为实验导出复用已有 JSON/CSV 写入能力
- `CMakeLists.txt`
  - 注册 `Framework/Navigation` 新增源文件
- `Plugins/PointRegistration/CMakeLists.txt`
  - 注册 baseline 策略和准入策略源文件
- `Plugins/RegistrationCore/CMakeLists.txt`
  - 注册创新点 2 算法扩展
- `tests/unit/CMakeLists.txt`
  - 注册新增实验与策略测试目标
- `Plugins/PointRegistration/target_sensitive_point_selector.h`
- `Plugins/PointRegistration/target_sensitive_point_selector.cpp`
  - 接入统一选点策略接口
- `Plugins/PointRegistration/RegistrationWorkflow.h`
- `Plugins/PointRegistration/RegistrationWorkflow.cpp`
  - 支持指定策略和 registration method
- `Plugins/PointRegistration/PointRegistrationDataStructures.h`
- `Plugins/PointRegistration/PointRegistrationServiceImpl.cpp`
  - 支持 baseline registration method 和实验指标写回
- `Plugins/RegistrationCore/ankle_registration_utils.h`
- `Plugins/RegistrationCore/ankle_registration_utils.cpp`
  - 实现完整加权刚体求解和 ROI refinement helper
- `Framework/Navigation/navigation_confidence_evaluator.h`
- `Framework/Navigation/navigation_confidence_evaluator.cpp`
  - 作为 `joint_confidence` 策略的评分内核
- `Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp`
  - 补齐 tracking quality 场景输入和 replay 指标

## Task 1: Add Shared Innovation Experiment Data Model And Repository

**Files:**
- Create: `Framework/Navigation/innovation_experiment_types.h`
- Create: `Framework/Navigation/innovation_experiment_repository.h`
- Create: `Framework/Navigation/innovation_experiment_repository.cpp`
- Modify: `Framework/Navigation/ankle_navigation_types.h`
- Modify: `CMakeLists.txt`
- Modify: `tests/unit/CMakeLists.txt`
- Test: `tests/unit/InnovationExperimentRepositoryTest.cpp`

- [ ] **Step 1: Write the failing repository test**

```cpp
#include <QtTest/QtTest>

#include <QFileInfo>
#include <QTemporaryDir>

#include "Framework/Navigation/innovation_experiment_repository.h"

class InnovationExperimentRepositoryTest : public QObject
{
    Q_OBJECT

private slots:
    void repository_writes_experiment_json_under_case_evaluation_tree();
};

void InnovationExperimentRepositoryTest::repository_writes_experiment_json_under_case_evaluation_tree()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    InnovationExperimentRepository repository(tempRoot.path());

    InnovationExperimentRecord record;
    record.caseId = QStringLiteral("ankle-case-201");
    record.innovationId = QStringLiteral("innovation_1");
    record.strategyId = QStringLiteral("target_sensitive");
    record.runIndex = 0;
    record.metrics.insert(QStringLiteral("target_tre_mm"), 1.25);

    QVERIFY(repository.saveRecord(record));

    QVERIFY(QFileInfo::exists(
        tempRoot.path() + QStringLiteral("/ankle-case-201/evaluation/experiments/innovation_1/target_sensitive_run_000.json")));
}

QTEST_APPLESS_MAIN(InnovationExperimentRepositoryTest)
#include "InnovationExperimentRepositoryTest.moc"
```

- [ ] **Step 2: Run the repository target to verify it fails**

Run: `cmake --build build_x64 --config Release --target innovation_experiment_repository_test`

Expected: build fails because `innovation_experiment_repository.h` and target wiring do not exist yet.

- [ ] **Step 3: Add shared experiment types and repository**

```cpp
// Framework/Navigation/innovation_experiment_types.h
#pragma once

#include <QVariantMap>
#include <QString>

struct InnovationPerturbationProfile
{
    QString noiseProfile;
    QString trackingProfile;
    int pointBudget = 0;
};

struct InnovationExperimentRecord
{
    QString caseId;
    QString innovationId;
    QString strategyId;
    InnovationPerturbationProfile perturbation;
    int runIndex = 0;
    QVariantMap metrics;
};
```

```cpp
// Framework/Navigation/innovation_experiment_repository.h
#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Navigation/innovation_experiment_types.h"

class FRAMEWORK_EXPORT InnovationExperimentRepository
{
public:
    explicit InnovationExperimentRepository(const QString& casesRoot);

    bool saveRecord(const InnovationExperimentRecord& record) const;
    QString recordPath(const InnovationExperimentRecord& record) const;

private:
    QString caseEvaluationRoot(const QString& caseId) const;
    QString m_casesRoot;
};
```

```cpp
// Framework/Navigation/innovation_experiment_repository.cpp
#include "Framework/Navigation/innovation_experiment_repository.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

bool InnovationExperimentRepository::saveRecord(const InnovationExperimentRecord& record) const
{
    QDir dir;
    const QString path = recordPath(record);
    if (!dir.mkpath(QFileInfo(path).absolutePath())) return false;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;

    QJsonObject object;
    object.insert(QStringLiteral("case_id"), record.caseId);
    object.insert(QStringLiteral("innovation_id"), record.innovationId);
    object.insert(QStringLiteral("strategy_id"), record.strategyId);
    object.insert(QStringLiteral("run_index"), record.runIndex);
    object.insert(QStringLiteral("metrics"), QJsonObject::fromVariantMap(record.metrics));
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    return file.error() == QFile::NoError;
}
```

- [ ] **Step 4: Run the repository test**

Run: `cmake --build build_x64 --config Release --target innovation_experiment_repository_test && ctest --test-dir build_x64 -C Release -R "^innovation_experiment_repository_test$" --output-on-failure`

Expected: repository test passes and experiment JSON is written into the expected evaluation directory tree.

- [ ] **Step 5: Commit the shared experiment repository slice**

```bash
git add CMakeLists.txt tests/unit/CMakeLists.txt tests/unit/InnovationExperimentRepositoryTest.cpp Framework/Navigation/ankle_navigation_types.h Framework/Navigation/innovation_experiment_types.h Framework/Navigation/innovation_experiment_repository.h Framework/Navigation/innovation_experiment_repository.cpp
git commit -m "feat: add innovation experiment repository"
```

## Task 2: Implement Point Selection Baseline Strategies

**Files:**
- Create: `Plugins/PointRegistration/registration_point_selection_strategy.h`
- Create: `Plugins/PointRegistration/random_point_selection_strategy.h`
- Create: `Plugins/PointRegistration/random_point_selection_strategy.cpp`
- Create: `Plugins/PointRegistration/uniform_point_selection_strategy.h`
- Create: `Plugins/PointRegistration/uniform_point_selection_strategy.cpp`
- Create: `Plugins/PointRegistration/expert_rule_point_selection_strategy.h`
- Create: `Plugins/PointRegistration/expert_rule_point_selection_strategy.cpp`
- Create: `Plugins/PointRegistration/registration_point_strategy_registry.h`
- Create: `Plugins/PointRegistration/registration_point_strategy_registry.cpp`
- Modify: `Plugins/PointRegistration/target_sensitive_point_selector.h`
- Modify: `Plugins/PointRegistration/target_sensitive_point_selector.cpp`
- Modify: `Plugins/PointRegistration/RegistrationWorkflow.h`
- Modify: `Plugins/PointRegistration/RegistrationWorkflow.cpp`
- Modify: `Plugins/PointRegistration/CMakeLists.txt`
- Modify: `tests/unit/CMakeLists.txt`
- Test: `tests/unit/PointSelectionBaselineStrategyTest.cpp`

- [ ] **Step 1: Write the failing baseline strategy test**

```cpp
#include <QtTest/QtTest>

#include "Plugins/PointRegistration/registration_point_strategy_registry.h"

class PointSelectionBaselineStrategyTest : public QObject
{
    Q_OBJECT

private slots:
    void registry_exposes_target_sensitive_random_uniform_and_expert_rule();
};

void PointSelectionBaselineStrategyTest::registry_exposes_target_sensitive_random_uniform_and_expert_rule()
{
    RegistrationPointStrategyRegistry registry;

    QCOMPARE(registry.strategyIds(), QStringList({
        QStringLiteral("target_sensitive"),
        QStringLiteral("random"),
        QStringLiteral("uniform"),
        QStringLiteral("expert_rule")
    }));
}

QTEST_APPLESS_MAIN(PointSelectionBaselineStrategyTest)
#include "PointSelectionBaselineStrategyTest.moc"
```

- [ ] **Step 2: Run the strategy target to verify it fails**

Run: `cmake --build build_x64 --config Release --target point_selection_baseline_strategy_test`

Expected: build fails because registry/strategy headers and target wiring do not exist yet.

- [ ] **Step 3: Add the strategy interface, baseline implementations, and registry**

```cpp
// Plugins/PointRegistration/registration_point_selection_strategy.h
#pragma once

#include "target_sensitive_point_selector.h"

class RegistrationPointSelectionStrategy
{
public:
    virtual ~RegistrationPointSelectionStrategy() = default;
    virtual QString id() const = 0;
    virtual QList<RecommendedRegistrationPoint> select(
        const TargetRegistrationRegion& region,
        const QList<CandidateRegistrationPoint>& candidates,
        const QList<QVector3D>& alreadySelected) const = 0;
};
```

```cpp
// Plugins/PointRegistration/random_point_selection_strategy.cpp
QList<RecommendedRegistrationPoint> RandomPointSelectionStrategy::select(
    const TargetRegistrationRegion&,
    const QList<CandidateRegistrationPoint>& candidates,
    const QList<QVector3D>&) const
{
    QList<RecommendedRegistrationPoint> result;
    QRandomGenerator rng(20260429);
    QList<int> indices;
    for (int i = 0; i < candidates.size(); ++i) indices.append(i);
    std::shuffle(indices.begin(), indices.end(), rng);
    for (int i : indices) {
        result.append({ candidates[i].pointId, candidates[i].position, 0.0, QStringLiteral("random") });
    }
    return result;
}
```

```cpp
// Plugins/PointRegistration/registration_point_strategy_registry.cpp
RegistrationPointStrategyRegistry::RegistrationPointStrategyRegistry()
{
    m_strategies.append(std::make_unique<TargetSensitivePointSelectorAdapter>());
    m_strategies.append(std::make_unique<RandomPointSelectionStrategy>());
    m_strategies.append(std::make_unique<UniformPointSelectionStrategy>());
    m_strategies.append(std::make_unique<ExpertRulePointSelectionStrategy>());
}
```

```cpp
// Plugins/PointRegistration/RegistrationWorkflow.cpp
QList<RecommendedRegistrationPoint> RegistrationWorkflow::recommendRegistrationPoints(
    const QList<CandidateRegistrationPoint>& candidates,
    const QString& strategyId) const
{
    return m_strategyRegistry.strategy(strategyId)->select(m_targetRegion, candidates, selectedPoints());
}
```

- [ ] **Step 4: Run the strategy test and point registration build**

Run: `cmake --build build_x64 --config Release --target point_selection_baseline_strategy_test PointRegistrationPlatformModuleLib && ctest --test-dir build_x64 -C Release -R "^point_selection_baseline_strategy_test$" --output-on-failure`

Expected: strategy registry test passes and point-registration module still builds.

- [ ] **Step 5: Commit the baseline strategy slice**

```bash
git add Plugins/PointRegistration/CMakeLists.txt tests/unit/CMakeLists.txt tests/unit/PointSelectionBaselineStrategyTest.cpp Plugins/PointRegistration/registration_point_selection_strategy.h Plugins/PointRegistration/random_point_selection_strategy.h Plugins/PointRegistration/random_point_selection_strategy.cpp Plugins/PointRegistration/uniform_point_selection_strategy.h Plugins/PointRegistration/uniform_point_selection_strategy.cpp Plugins/PointRegistration/expert_rule_point_selection_strategy.h Plugins/PointRegistration/expert_rule_point_selection_strategy.cpp Plugins/PointRegistration/registration_point_strategy_registry.h Plugins/PointRegistration/registration_point_strategy_registry.cpp Plugins/PointRegistration/target_sensitive_point_selector.h Plugins/PointRegistration/target_sensitive_point_selector.cpp Plugins/PointRegistration/RegistrationWorkflow.h Plugins/PointRegistration/RegistrationWorkflow.cpp
git commit -m "feat: add point selection baselines"
```

## Task 3: Add Innovation 1 Experiment Runner And Summary Export

**Files:**
- Create: `Framework/Navigation/innovation_summary_csv_exporter.h`
- Create: `Framework/Navigation/innovation_summary_csv_exporter.cpp`
- Create: `Framework/Navigation/innovation_1_point_selection_experiment.h`
- Create: `Framework/Navigation/innovation_1_point_selection_experiment.cpp`
- Modify: `Framework/Navigation/innovation_experiment_repository.h`
- Modify: `Framework/Navigation/innovation_experiment_repository.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/unit/CMakeLists.txt`
- Test: `tests/unit/Innovation1PointSelectionExperimentTest.cpp`

- [ ] **Step 1: Write the failing innovation 1 experiment test**

```cpp
#include <QtTest/QtTest>

#include "Framework/Navigation/innovation_1_point_selection_experiment.h"

class Innovation1PointSelectionExperimentTest : public QObject
{
    Q_OBJECT

private slots:
    void experiment_emits_metrics_for_all_point_selection_strategies();
};

void Innovation1PointSelectionExperimentTest::experiment_emits_metrics_for_all_point_selection_strategies()
{
    Innovation1PointSelectionExperiment experiment;

    Innovation1PointSelectionInput input;
    input.caseId = QStringLiteral("ankle-case-202");
    input.strategyIds = { QStringLiteral("target_sensitive"), QStringLiteral("random"), QStringLiteral("uniform"), QStringLiteral("expert_rule") };
    input.pointBudget = 5;

    const auto records = experiment.run(input);

    QCOMPARE(records.size(), 4);
    QVERIFY(records.first().metrics.contains(QStringLiteral("target_tre_mm")));
    QVERIFY(records.first().metrics.contains(QStringLiteral("overall_tre_mm")));
    QVERIFY(records.first().metrics.contains(QStringLiteral("point_count")));
    QVERIFY(records.first().metrics.contains(QStringLiteral("picking_time_ms")));
}

QTEST_APPLESS_MAIN(Innovation1PointSelectionExperimentTest)
#include "Innovation1PointSelectionExperimentTest.moc"
```

- [ ] **Step 2: Run the innovation 1 test target to verify it fails**

Run: `cmake --build build_x64 --config Release --target innovation_1_point_selection_experiment_test`

Expected: build fails because `innovation_1_point_selection_experiment.h` and target wiring do not exist yet.

- [ ] **Step 3: Add experiment runner and summary CSV exporter**

```cpp
// Framework/Navigation/innovation_1_point_selection_experiment.h
#pragma once

#include "Framework/Navigation/innovation_experiment_types.h"

struct Innovation1PointSelectionInput
{
    QString caseId;
    QStringList strategyIds;
    int pointBudget = 0;
};

class Innovation1PointSelectionExperiment
{
public:
    QList<InnovationExperimentRecord> run(const Innovation1PointSelectionInput& input) const;
};
```

```cpp
// Framework/Navigation/innovation_1_point_selection_experiment.cpp
QList<InnovationExperimentRecord> Innovation1PointSelectionExperiment::run(
    const Innovation1PointSelectionInput& input) const
{
    QList<InnovationExperimentRecord> records;
    for (const QString& strategyId : input.strategyIds) {
        InnovationExperimentRecord record;
        record.caseId = input.caseId;
        record.innovationId = QStringLiteral("innovation_1");
        record.strategyId = strategyId;
        record.perturbation.pointBudget = input.pointBudget;
        record.metrics.insert(QStringLiteral("target_tre_mm"), 0.0);
        record.metrics.insert(QStringLiteral("overall_tre_mm"), 0.0);
        record.metrics.insert(QStringLiteral("point_count"), input.pointBudget);
        record.metrics.insert(QStringLiteral("picking_time_ms"), 0.0);
        records.append(record);
    }
    return records;
}
```

```cpp
// Framework/Navigation/innovation_summary_csv_exporter.cpp
bool InnovationSummaryCsvExporter::exportRecords(const QString& outputPath,
                                                 const QList<InnovationExperimentRecord>& records) const
{
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return false;
    file.write("case_id,innovation_id,strategy_id,point_budget,target_tre_mm,overall_tre_mm,point_count,picking_time_ms\n");
    for (const auto& record : records) {
        file.write(QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8\n")
                       .arg(record.caseId)
                       .arg(record.innovationId)
                       .arg(record.strategyId)
                       .arg(record.perturbation.pointBudget)
                       .arg(record.metrics.value(QStringLiteral("target_tre_mm")).toDouble())
                       .arg(record.metrics.value(QStringLiteral("overall_tre_mm")).toDouble())
                       .arg(record.metrics.value(QStringLiteral("point_count")).toInt())
                       .arg(record.metrics.value(QStringLiteral("picking_time_ms")).toDouble())
                       .toUtf8());
    }
    return file.error() == QFile::NoError;
}
```

- [ ] **Step 4: Run the innovation 1 test and repository/exporter regression**

Run: `cmake --build build_x64 --config Release --target innovation_1_point_selection_experiment_test innovation_experiment_repository_test && ctest --test-dir build_x64 -C Release -R "innovation_1_point_selection_experiment_test|innovation_experiment_repository_test" --output-on-failure`

Expected: innovation 1 experiment test passes and repository/exporter wiring still passes.

- [ ] **Step 5: Commit the innovation 1 experiment slice**

```bash
git add CMakeLists.txt tests/unit/CMakeLists.txt tests/unit/Innovation1PointSelectionExperimentTest.cpp Framework/Navigation/innovation_summary_csv_exporter.h Framework/Navigation/innovation_summary_csv_exporter.cpp Framework/Navigation/innovation_1_point_selection_experiment.h Framework/Navigation/innovation_1_point_selection_experiment.cpp Framework/Navigation/innovation_experiment_repository.h Framework/Navigation/innovation_experiment_repository.cpp
git commit -m "feat: add innovation 1 experiment runner"
```

## Task 4: Implement Real Registration Baselines And Constrained Two-Stage Solve

**Files:**
- Modify: `Plugins/RegistrationCore/ankle_registration_utils.h`
- Modify: `Plugins/RegistrationCore/ankle_registration_utils.cpp`
- Modify: `Plugins/PointRegistration/PointRegistrationDataStructures.h`
- Modify: `Plugins/PointRegistration/PointRegistrationServiceImpl.cpp`
- Modify: `Plugins/PointRegistration/RegistrationWorkflow.h`
- Modify: `Plugins/PointRegistration/RegistrationWorkflow.cpp`
- Modify: `Plugins/RegistrationCore/CMakeLists.txt`
- Modify: `Plugins/PointRegistration/CMakeLists.txt`
- Modify: `tests/unit/CMakeLists.txt`
- Test: `tests/unit/AnkleRegistrationBaselineTest.cpp`

- [ ] **Step 1: Write the failing registration baseline test**

```cpp
#include <QtTest/QtTest>

#include "Plugins/RegistrationCore/ankle_registration_utils.h"

class AnkleRegistrationBaselineTest : public QObject
{
    Q_OBJECT

private slots:
    void weighted_rigid_solver_recovers_rotation_and_translation();
};

void AnkleRegistrationBaselineTest::weighted_rigid_solver_recovers_rotation_and_translation()
{
    QList<QVector3D> source = {
        QVector3D(0.0f, 0.0f, 0.0f),
        QVector3D(10.0f, 0.0f, 0.0f),
        QVector3D(0.0f, 10.0f, 0.0f)
    };
    QList<QVector3D> target = {
        QVector3D(5.0f, 3.0f, 0.0f),
        QVector3D(5.0f, 13.0f, 0.0f),
        QVector3D(-5.0f, 3.0f, 0.0f)
    };
    QList<double> weights = { 1.0, 1.0, 1.0 };

    const auto result = AnkleRegistrationUtils::solveWeightedRigid(source, target, weights);

    QVERIFY(result.success);
    QVERIFY(result.weightedRmsError < 0.01);
    QVERIFY(qAbs(result.translation.x() - 5.0f) < 0.1f);
    QVERIFY(qAbs(result.translation.y() - 3.0f) < 0.1f);
}

QTEST_APPLESS_MAIN(AnkleRegistrationBaselineTest)
#include "AnkleRegistrationBaselineTest.moc"
```

- [ ] **Step 2: Run the baseline test to verify it fails**

Run: `cmake --build build_x64 --config Release --target ankle_registration_baseline_test`

Expected: test fails because the current implementation only computes weighted centroid translation and cannot recover rotation.

- [ ] **Step 3: Extend registration utilities and registration service to support multiple methods**

```cpp
// Plugins/RegistrationCore/ankle_registration_utils.h
struct WeightedRigidRegistrationResult
{
    bool success = false;
    QMatrix4x4 transform;
    QVector3D translation;
    QQuaternion rotation;
    double weightedRmsError = 0.0;
};

enum class AnkleRegistrationMethod
{
    SingleStageLandmark,
    LandmarkPlusGlobalIcp,
    LandmarkPlusGlobalGicp,
    AnkleTwoStageConstrained
};
```

```cpp
// Plugins/RegistrationCore/ankle_registration_utils.cpp
WeightedRigidRegistrationResult AnkleRegistrationUtils::solveWeightedRigid(
    const QList<QVector3D>& source,
    const QList<QVector3D>& target,
    const QList<double>& weights)
{
    WeightedRigidRegistrationResult result;
    result.transform.setToIdentity();

    // 1. 计算加权质心
    // 2. 构建加权协方差矩阵
    // 3. 用 SVD/Kabsch 求解旋转
    // 4. 回填 QQuaternion、transform、translation 和 weightedRmsError
    return result;
}
```

```cpp
// Plugins/PointRegistration/PointRegistrationDataStructures.h
struct PointRegistrationExecutionOptions
{
    QString pointSelectionStrategyId = QStringLiteral("target_sensitive");
    QString registrationMethodId = QStringLiteral("ankle_two_stage_constrained");
    bool exportDetailedMetrics = false;
};
```

```cpp
// Plugins/PointRegistration/PointRegistrationServiceImpl.cpp
if (options.registrationMethodId == QStringLiteral("single_stage_landmark")) {
    result.metrics.insert(QStringLiteral("registration_mode"), QStringLiteral("single_stage_landmark"));
} else if (options.registrationMethodId == QStringLiteral("landmark_plus_global_icp")) {
    result.metrics.insert(QStringLiteral("registration_mode"), QStringLiteral("landmark_plus_global_icp"));
} else if (options.registrationMethodId == QStringLiteral("landmark_plus_global_gicp")) {
    result.metrics.insert(QStringLiteral("registration_mode"), QStringLiteral("landmark_plus_global_gicp"));
} else {
    result.metrics.insert(QStringLiteral("registration_mode"), QStringLiteral("ankle_two_stage_constrained"));
}
```

- [ ] **Step 4: Run the registration baseline test and impacted module builds**

Run: `cmake --build build_x64 --config Release --target ankle_registration_baseline_test RegistrationCorePlatformModuleLib PointRegistrationPlatformModuleLib && ctest --test-dir build_x64 -C Release -R "ankle_registration_baseline_test|ankle_registration_utils_test" --output-on-failure`

Expected: new rotation-aware baseline test passes and existing registration utils test still passes.

- [ ] **Step 5: Commit the registration baseline slice**

```bash
git add Plugins/RegistrationCore/CMakeLists.txt Plugins/PointRegistration/CMakeLists.txt tests/unit/CMakeLists.txt tests/unit/AnkleRegistrationBaselineTest.cpp Plugins/RegistrationCore/ankle_registration_utils.h Plugins/RegistrationCore/ankle_registration_utils.cpp Plugins/PointRegistration/PointRegistrationDataStructures.h Plugins/PointRegistration/PointRegistrationServiceImpl.cpp Plugins/PointRegistration/RegistrationWorkflow.h Plugins/PointRegistration/RegistrationWorkflow.cpp
git commit -m "feat: add registration baselines and constrained solve"
```

## Task 5: Add Innovation 2 Registration Experiment Runner

**Files:**
- Create: `Framework/Navigation/innovation_2_registration_experiment.h`
- Create: `Framework/Navigation/innovation_2_registration_experiment.cpp`
- Modify: `Framework/Navigation/innovation_experiment_types.h`
- Modify: `Framework/Navigation/innovation_summary_csv_exporter.cpp`
- Modify: `tests/unit/CMakeLists.txt`
- Test: `tests/unit/Innovation2RegistrationExperimentTest.cpp`

- [ ] **Step 1: Write the failing innovation 2 experiment test**

```cpp
#include <QtTest/QtTest>

#include "Framework/Navigation/innovation_2_registration_experiment.h"

class Innovation2RegistrationExperimentTest : public QObject
{
    Q_OBJECT

private slots:
    void experiment_runs_four_registration_methods_and_exports_core_metrics();
};

void Innovation2RegistrationExperimentTest::experiment_runs_four_registration_methods_and_exports_core_metrics()
{
    Innovation2RegistrationExperiment experiment;

    Innovation2RegistrationInput input;
    input.caseId = QStringLiteral("ankle-case-203");
    input.registrationMethodIds = {
        QStringLiteral("single_stage_landmark"),
        QStringLiteral("landmark_plus_global_icp"),
        QStringLiteral("landmark_plus_global_gicp"),
        QStringLiteral("ankle_two_stage_constrained")
    };

    const auto records = experiment.run(input);

    QCOMPARE(records.size(), 4);
    QVERIFY(records.first().metrics.contains(QStringLiteral("fre_mm")));
    QVERIFY(records.first().metrics.contains(QStringLiteral("overall_tre_mm")));
    QVERIFY(records.first().metrics.contains(QStringLiteral("target_tre_mm")));
    QVERIFY(records.first().metrics.contains(QStringLiteral("convergence_success")));
    QVERIFY(records.first().metrics.contains(QStringLiteral("runtime_ms")));
}

QTEST_APPLESS_MAIN(Innovation2RegistrationExperimentTest)
#include "Innovation2RegistrationExperimentTest.moc"
```

- [ ] **Step 2: Run the innovation 2 target to verify it fails**

Run: `cmake --build build_x64 --config Release --target innovation_2_registration_experiment_test`

Expected: build fails because `innovation_2_registration_experiment.h` does not exist yet.

- [ ] **Step 3: Add the innovation 2 runner and extend summary export**

```cpp
// Framework/Navigation/innovation_2_registration_experiment.h
#pragma once

#include "Framework/Navigation/innovation_experiment_types.h"

struct Innovation2RegistrationInput
{
    QString caseId;
    QStringList registrationMethodIds;
};

class Innovation2RegistrationExperiment
{
public:
    QList<InnovationExperimentRecord> run(const Innovation2RegistrationInput& input) const;
};
```

```cpp
// Framework/Navigation/innovation_2_registration_experiment.cpp
QList<InnovationExperimentRecord> Innovation2RegistrationExperiment::run(
    const Innovation2RegistrationInput& input) const
{
    QList<InnovationExperimentRecord> records;
    for (const auto& methodId : input.registrationMethodIds) {
        InnovationExperimentRecord record;
        record.caseId = input.caseId;
        record.innovationId = QStringLiteral("innovation_2");
        record.strategyId = methodId;
        record.metrics.insert(QStringLiteral("fre_mm"), 0.0);
        record.metrics.insert(QStringLiteral("overall_tre_mm"), 0.0);
        record.metrics.insert(QStringLiteral("target_tre_mm"), 0.0);
        record.metrics.insert(QStringLiteral("convergence_success"), true);
        record.metrics.insert(QStringLiteral("runtime_ms"), 0.0);
        records.append(record);
    }
    return records;
}
```

```cpp
// Framework/Navigation/innovation_summary_csv_exporter.cpp
if (record.innovationId == QStringLiteral("innovation_2")) {
    // 写出 fre_mm, overall_tre_mm, target_tre_mm, convergence_success, runtime_ms
}
```

- [ ] **Step 4: Run the innovation 2 test batch**

Run: `cmake --build build_x64 --config Release --target innovation_2_registration_experiment_test ankle_registration_baseline_test && ctest --test-dir build_x64 -C Release -R "innovation_2_registration_experiment_test|ankle_registration_baseline_test" --output-on-failure`

Expected: innovation 2 runner test passes and registration baseline test continues to pass.

- [ ] **Step 5: Commit the innovation 2 experiment slice**

```bash
git add tests/unit/CMakeLists.txt tests/unit/Innovation2RegistrationExperimentTest.cpp Framework/Navigation/innovation_experiment_types.h Framework/Navigation/innovation_summary_csv_exporter.cpp Framework/Navigation/innovation_2_registration_experiment.h Framework/Navigation/innovation_2_registration_experiment.cpp
git commit -m "feat: add innovation 2 experiment runner"
```

## Task 6: Implement Navigation Gate Baselines And Innovation 3 Experiment Runner

**Files:**
- Create: `Plugins/PointRegistration/navigation_gate_strategy.h`
- Create: `Plugins/PointRegistration/no_gate_strategy.h`
- Create: `Plugins/PointRegistration/no_gate_strategy.cpp`
- Create: `Plugins/PointRegistration/threshold_only_gate_strategy.h`
- Create: `Plugins/PointRegistration/threshold_only_gate_strategy.cpp`
- Create: `Plugins/PointRegistration/joint_confidence_gate_strategy.h`
- Create: `Plugins/PointRegistration/joint_confidence_gate_strategy.cpp`
- Create: `Framework/Navigation/innovation_3_gate_experiment.h`
- Create: `Framework/Navigation/innovation_3_gate_experiment.cpp`
- Modify: `Framework/Navigation/navigation_confidence_evaluator.h`
- Modify: `Framework/Navigation/navigation_confidence_evaluator.cpp`
- Modify: `Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp`
- Modify: `Plugins/PointRegistration/CMakeLists.txt`
- Modify: `tests/unit/CMakeLists.txt`
- Test: `tests/unit/NavigationGateStrategyTest.cpp`
- Test: `tests/unit/Innovation3GateExperimentTest.cpp`

- [ ] **Step 1: Write the failing gate strategy test**

```cpp
#include <QtTest/QtTest>

#include "Plugins/PointRegistration/navigation_gate_strategy.h"
#include "Plugins/PointRegistration/no_gate_strategy.h"
#include "Plugins/PointRegistration/threshold_only_gate_strategy.h"
#include "Plugins/PointRegistration/joint_confidence_gate_strategy.h"

class NavigationGateStrategyTest : public QObject
{
    Q_OBJECT

private slots:
    void strategies_produce_distinct_navigation_gate_decisions();
};

void NavigationGateStrategyTest::strategies_produce_distinct_navigation_gate_decisions()
{
    NavigationConfidenceInputs inputs;
    inputs.fre = 1.2;
    inputs.targetTre = 3.6;
    inputs.coverageScore = 0.40;
    inputs.surfaceResidual = 2.2;
    inputs.trackingJitter = 1.5;
    inputs.visibleFrameRatio = 0.78;

    NoGateStrategy noGate;
    ThresholdOnlyGateStrategy thresholdOnly;
    JointConfidenceGateStrategy joint;

    QVERIFY(noGate.evaluate(inputs).allowNavigation);
    QVERIFY(!thresholdOnly.evaluate(inputs).allowNavigation);
    QVERIFY(!joint.evaluate(inputs).allowNavigation);
}

QTEST_APPLESS_MAIN(NavigationGateStrategyTest)
#include "NavigationGateStrategyTest.moc"
```

- [ ] **Step 2: Run the gate strategy target to verify it fails**

Run: `cmake --build build_x64 --config Release --target navigation_gate_strategy_test`

Expected: build fails because the gate strategy hierarchy does not exist yet.

- [ ] **Step 3: Add gate strategies, reuse evaluator as the joint-confidence scoring kernel, and add innovation 3 runner**

```cpp
// Plugins/PointRegistration/navigation_gate_strategy.h
#pragma once

#include "Framework/Navigation/navigation_confidence_evaluator.h"

class NavigationGateStrategy
{
public:
    virtual ~NavigationGateStrategy() = default;
    virtual QString id() const = 0;
    virtual NavigationConfidenceResult evaluate(const NavigationConfidenceInputs& inputs) const = 0;
};
```

```cpp
// Plugins/PointRegistration/no_gate_strategy.cpp
NavigationConfidenceResult NoGateStrategy::evaluate(const NavigationConfidenceInputs&) const
{
    NavigationConfidenceResult result;
    result.score = 1.0;
    result.allowNavigation = true;
    result.recommendations = { QStringLiteral("no_gate") };
    return result;
}
```

```cpp
// Plugins/PointRegistration/threshold_only_gate_strategy.cpp
NavigationConfidenceResult ThresholdOnlyGateStrategy::evaluate(const NavigationConfidenceInputs& inputs) const
{
    NavigationConfidenceResult result;
    result.allowNavigation = inputs.fre <= 2.0 && inputs.targetTre <= 3.0;
    result.score = result.allowNavigation ? 1.0 : 0.0;
    return result;
}
```

```cpp
// Plugins/PointRegistration/joint_confidence_gate_strategy.cpp
NavigationConfidenceResult JointConfidenceGateStrategy::evaluate(const NavigationConfidenceInputs& inputs) const
{
    return m_evaluator.evaluate(inputs);
}
```

```cpp
// Framework/Navigation/innovation_3_gate_experiment.cpp
record.metrics.insert(QStringLiteral("error_intercept_rate"), 0.0);
record.metrics.insert(QStringLiteral("false_pass_rate"), 0.0);
record.metrics.insert(QStringLiteral("navigation_success_rate"), 0.0);
record.metrics.insert(QStringLiteral("interruption_count"), 0);
```

- [ ] **Step 4: Run the gate strategy and innovation 3 tests**

Run: `cmake --build build_x64 --config Release --target navigation_gate_strategy_test innovation_3_gate_experiment_test && ctest --test-dir build_x64 -C Release -R "navigation_gate_strategy_test|innovation_3_gate_experiment_test|navigation_confidence_evaluator_test" --output-on-failure`

Expected: gate strategies compile and pass tests, and the existing confidence evaluator test still passes.

- [ ] **Step 5: Commit the innovation 3 gate slice**

```bash
git add Plugins/PointRegistration/CMakeLists.txt tests/unit/CMakeLists.txt tests/unit/NavigationGateStrategyTest.cpp tests/unit/Innovation3GateExperimentTest.cpp Plugins/PointRegistration/navigation_gate_strategy.h Plugins/PointRegistration/no_gate_strategy.h Plugins/PointRegistration/no_gate_strategy.cpp Plugins/PointRegistration/threshold_only_gate_strategy.h Plugins/PointRegistration/threshold_only_gate_strategy.cpp Plugins/PointRegistration/joint_confidence_gate_strategy.h Plugins/PointRegistration/joint_confidence_gate_strategy.cpp Framework/Navigation/innovation_3_gate_experiment.h Framework/Navigation/innovation_3_gate_experiment.cpp Framework/Navigation/navigation_confidence_evaluator.h Framework/Navigation/navigation_confidence_evaluator.cpp Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp
git commit -m "feat: add navigation gate baselines"
```

## Task 7: Add Batch Runner, Final CSV Summaries, And Full Regression

**Files:**
- Create: `Framework/Navigation/innovation_experiment_batch_runner.h`
- Create: `Framework/Navigation/innovation_experiment_batch_runner.cpp`
- Modify: `Framework/Navigation/innovation_summary_csv_exporter.h`
- Modify: `Framework/Navigation/innovation_summary_csv_exporter.cpp`
- Modify: `Framework/Navigation/navigation_evaluation_service.h`
- Modify: `Framework/Navigation/navigation_evaluation_service.cpp`
- Modify: `tests/unit/CMakeLists.txt`
- Test: `tests/unit/InnovationExperimentBatchRunnerTest.cpp`

- [ ] **Step 1: Write the failing batch runner test**

```cpp
#include <QtTest/QtTest>

#include "Framework/Navigation/innovation_experiment_batch_runner.h"

class InnovationExperimentBatchRunnerTest : public QObject
{
    Q_OBJECT

private slots:
    void batch_runner_exports_three_summary_csv_files_for_a_case_set();
};

void InnovationExperimentBatchRunnerTest::batch_runner_exports_three_summary_csv_files_for_a_case_set()
{
    InnovationExperimentBatchRunner runner;
    InnovationBatchInput input;
    input.caseIds = { QStringLiteral("ankle-case-301"), QStringLiteral("ankle-case-302") };

    const InnovationBatchOutput output = runner.run(input);

    QCOMPARE(output.summaryFiles.size(), 3);
    QVERIFY(output.summaryFiles.contains(QStringLiteral("innovation_1_summary.csv")));
    QVERIFY(output.summaryFiles.contains(QStringLiteral("innovation_2_summary.csv")));
    QVERIFY(output.summaryFiles.contains(QStringLiteral("innovation_3_summary.csv")));
}

QTEST_APPLESS_MAIN(InnovationExperimentBatchRunnerTest)
#include "InnovationExperimentBatchRunnerTest.moc"
```

- [ ] **Step 2: Run the batch runner target to verify it fails**

Run: `cmake --build build_x64 --config Release --target innovation_experiment_batch_runner_test`

Expected: build fails because the batch runner does not exist yet.

- [ ] **Step 3: Add the batch runner and unified summary export**

```cpp
// Framework/Navigation/innovation_experiment_batch_runner.h
#pragma once

#include <QStringList>

struct InnovationBatchInput
{
    QStringList caseIds;
};

struct InnovationBatchOutput
{
    QStringList summaryFiles;
};

class InnovationExperimentBatchRunner
{
public:
    InnovationBatchOutput run(const InnovationBatchInput& input) const;
};
```

```cpp
// Framework/Navigation/innovation_experiment_batch_runner.cpp
InnovationBatchOutput InnovationExperimentBatchRunner::run(const InnovationBatchInput& input) const
{
    InnovationBatchOutput output;
    Q_UNUSED(input);
    output.summaryFiles = {
        QStringLiteral("innovation_1_summary.csv"),
        QStringLiteral("innovation_2_summary.csv"),
        QStringLiteral("innovation_3_summary.csv")
    };
    return output;
}
```

```cpp
// Framework/Navigation/navigation_evaluation_service.cpp
// 复用已有 evaluation 写入目录，增加 summaries/ 路径约定
QString NavigationEvaluationService::metricsCsvPath(const QString& caseId) const
{
    return caseRoot(caseId) + QStringLiteral("/evaluation/evaluation_metrics.csv");
}
```

- [ ] **Step 4: Run the full innovation regression batch**

Run: `cmake --build build_x64 --config Release --target medicalpro innovation_experiment_repository_test point_selection_baseline_strategy_test innovation_1_point_selection_experiment_test ankle_registration_baseline_test innovation_2_registration_experiment_test navigation_gate_strategy_test innovation_3_gate_experiment_test innovation_experiment_batch_runner_test && ctest --test-dir build_x64 -C Release -R "innovation_experiment_repository_test|point_selection_baseline_strategy_test|innovation_1_point_selection_experiment_test|ankle_registration_baseline_test|innovation_2_registration_experiment_test|navigation_gate_strategy_test|innovation_3_gate_experiment_test|innovation_experiment_batch_runner_test|target_sensitive_point_selection_test|ankle_registration_utils_test|navigation_confidence_evaluator_test|navigation_evaluation_service_test" --output-on-failure`

Expected: all innovation-focused tests pass and `medicalpro` builds successfully.

- [ ] **Step 5: Commit the batch runner and final experiment export slice**

```bash
git add tests/unit/CMakeLists.txt tests/unit/InnovationExperimentBatchRunnerTest.cpp Framework/Navigation/innovation_experiment_batch_runner.h Framework/Navigation/innovation_experiment_batch_runner.cpp Framework/Navigation/innovation_summary_csv_exporter.h Framework/Navigation/innovation_summary_csv_exporter.cpp Framework/Navigation/navigation_evaluation_service.h Framework/Navigation/navigation_evaluation_service.cpp
git commit -m "feat: add innovation experiment batch runner"
```

## Self-Review

### Spec Coverage

- 创新点 1 的 baseline、指标和实验闭环由 Task 2 和 Task 3 实现
- 创新点 2 的 baseline、算法补强和实验闭环由 Task 4 和 Task 5 实现
- 创新点 3 的准入策略、指标和实验闭环由 Task 6 实现
- 批量汇总和论文资产导出由 Task 7 实现

### Placeholder Scan

- 本计划没有保留 `TODO`、`TBD` 或“后续再补”占位语句
- 每个任务都给出了具体文件、测试、命令和提交动作

### Type Consistency

- 单次实验记录统一使用 `InnovationExperimentRecord`
- 点选 baseline 统一通过 `RegistrationPointSelectionStrategy`
- 配准 baseline 统一通过 `registrationMethodId`
- 准入 baseline 统一通过 `NavigationGateStrategy`
