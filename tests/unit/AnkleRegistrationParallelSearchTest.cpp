#include <QtTest/QtTest>

#include "Plugins/RegistrationCore/ankle_registration_parallel_search.h"
#include "Plugins/PointRegistration/PointRegistrationDataStructures.h"

class AnkleRegistrationParallelSearchTest : public QObject
{
    Q_OBJECT

private slots:
    void point_registration_execution_options_expose_parallel_search_defaults();
    void point_registration_result_metrics_can_store_parallel_search_contract_keys();
    void parallel_search_plan_generates_requested_candidates_and_identity_seed();
    void parallel_search_keeps_lowest_scored_top_k_candidates();
    void parallel_search_returns_empty_top_k_for_non_positive_count();
    void parallel_search_profile_resolves_three_level_cell_sizes();
    void parallel_search_profile_resolves_two_level_cell_sizes();
    void parallel_search_profile_falls_back_to_single_level_for_unknown_profile();
    void parallel_search_extracts_candidate_ids_in_order();
    void parallel_search_filters_candidates_by_requested_ids();
    void parallel_search_converts_candidate_evaluation_to_variant_map();
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

void AnkleRegistrationParallelSearchTest::point_registration_result_metrics_can_store_parallel_search_contract_keys()
{
    PointRegistrationResult result;

    // 锁定 PointRegistrationResult.metrics 的并行搜索契约键。
    result.metrics.insert(QStringLiteral("candidate_count"), 64);
    result.metrics.insert(QStringLiteral("top_k_count"), 4);
    result.metrics.insert(QStringLiteral("coarse_search_ms"), 12);
    result.metrics.insert(QStringLiteral("roi_filter_ms"), 3);
    result.metrics.insert(QStringLiteral("refine_ms"), 28);
    result.metrics.insert(QStringLiteral("best_candidate_rank"), 0);
    result.metrics.insert(QStringLiteral("coarse_score"), 0.91);
    result.metrics.insert(QStringLiteral("parallel_search_enabled"), true);
    result.metrics.insert(QStringLiteral("multi_resolution_profile"), QStringLiteral("ankle_roi_three_level"));

    QVERIFY(result.metrics.contains(QStringLiteral("candidate_count")));
    QVERIFY(result.metrics.contains(QStringLiteral("top_k_count")));
    QVERIFY(result.metrics.contains(QStringLiteral("coarse_search_ms")));
    QVERIFY(result.metrics.contains(QStringLiteral("roi_filter_ms")));
    QVERIFY(result.metrics.contains(QStringLiteral("refine_ms")));
    QVERIFY(result.metrics.contains(QStringLiteral("best_candidate_rank")));
    QVERIFY(result.metrics.contains(QStringLiteral("coarse_score")));
    QVERIFY(result.metrics.contains(QStringLiteral("parallel_search_enabled")));
    QCOMPARE(result.metrics.value(QStringLiteral("candidate_count")).toInt(), 64);
    QCOMPARE(result.metrics.value(QStringLiteral("top_k_count")).toInt(), 4);
    QCOMPARE(result.metrics.value(QStringLiteral("coarse_search_ms")).toInt(), 12);
    QCOMPARE(result.metrics.value(QStringLiteral("roi_filter_ms")).toInt(), 3);
    QCOMPARE(result.metrics.value(QStringLiteral("refine_ms")).toInt(), 28);
    QCOMPARE(result.metrics.value(QStringLiteral("best_candidate_rank")).toInt(), 0);
    QCOMPARE(result.metrics.value(QStringLiteral("coarse_score")).toDouble(), 0.91);
    QCOMPARE(result.metrics.value(QStringLiteral("parallel_search_enabled")).toBool(), true);
    QCOMPARE(result.metrics.value(QStringLiteral("multi_resolution_profile")).toString(),
             QStringLiteral("ankle_roi_three_level"));
}

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

void AnkleRegistrationParallelSearchTest::parallel_search_returns_empty_top_k_for_non_positive_count()
{
    const QList<CandidateEvaluationResult> scores = {
        { QStringLiteral("candidate_003"), 3.4, 0.52, 0.41, true, 0 },
        { QStringLiteral("candidate_001"), 1.1, 0.78, 0.72, true, 0 }
    };

    QCOMPARE(selectTopKCandidates(scores, 0).size(), 0);
    QCOMPARE(selectTopKCandidates(scores, -1).size(), 0);
}

void AnkleRegistrationParallelSearchTest::parallel_search_profile_resolves_three_level_cell_sizes()
{
    const QList<double> cellSizes = resolveMultiResolutionCellSizes(QStringLiteral("ankle_roi_three_level"));

    QCOMPARE(cellSizes.size(), 3);
    QCOMPARE(cellSizes.at(0), 3.0);
    QCOMPARE(cellSizes.at(1), 1.5);
    QCOMPARE(cellSizes.at(2), 0.75);
}

void AnkleRegistrationParallelSearchTest::parallel_search_profile_resolves_two_level_cell_sizes()
{
    const QList<double> cellSizes = resolveMultiResolutionCellSizes(QStringLiteral("ankle_roi_two_level"));

    QCOMPARE(cellSizes.size(), 2);
    QCOMPARE(cellSizes.at(0), 2.5);
    QCOMPARE(cellSizes.at(1), 1.0);
}

void AnkleRegistrationParallelSearchTest::parallel_search_profile_falls_back_to_single_level_for_unknown_profile()
{
    const QList<double> cellSizes = resolveMultiResolutionCellSizes(QStringLiteral("unknown_profile"));

    QCOMPARE(cellSizes.size(), 1);
    QCOMPARE(cellSizes.first(), 1.0);
}

void AnkleRegistrationParallelSearchTest::parallel_search_extracts_candidate_ids_in_order()
{
    const QList<CandidateEvaluationResult> scores = {
        { QStringLiteral("candidate_010"), 0.8, 0.61, 0.57, true, 1 },
        { QStringLiteral("candidate_002"), 0.9, 0.59, 0.54, false, 0 }
    };

    const QStringList ids = candidateIds(scores);

    QCOMPARE(ids.size(), 2);
    QCOMPARE(ids.at(0), QStringLiteral("candidate_010"));
    QCOMPARE(ids.at(1), QStringLiteral("candidate_002"));
}

void AnkleRegistrationParallelSearchTest::parallel_search_filters_candidates_by_requested_ids()
{
    QList<CandidateInitialTransform> candidates;
    candidates.append({ QStringLiteral("candidate_000"), QStringLiteral("landmark_identity_seed"), QMatrix4x4(), QVector3D(), QVector3D(), 0 });
    candidates.append({ QStringLiteral("candidate_001"), QStringLiteral("axis_perturbation_seed"), QMatrix4x4(), QVector3D(), QVector3D(), 1 });
    candidates.append({ QStringLiteral("candidate_002"), QStringLiteral("axis_perturbation_seed"), QMatrix4x4(), QVector3D(), QVector3D(), 2 });

    const QList<CandidateInitialTransform> filtered = filterCandidatesByIds(
        candidates,
        { QStringLiteral("candidate_002"), QStringLiteral("candidate_000") });

    QCOMPARE(filtered.size(), 2);
    QCOMPARE(filtered.at(0).candidateId, QStringLiteral("candidate_000"));
    QCOMPARE(filtered.at(1).candidateId, QStringLiteral("candidate_002"));
}

void AnkleRegistrationParallelSearchTest::parallel_search_converts_candidate_evaluation_to_variant_map()
{
    const CandidateEvaluationResult result = {
        QStringLiteral("candidate_007"), 1.25, 0.81, 0.77, true, 2
    };

    const QVariantMap map = candidateEvaluationToVariantMap(result);

    QCOMPARE(map.value(QStringLiteral("candidate_id")).toString(), QStringLiteral("candidate_007"));
    QCOMPARE(map.value(QStringLiteral("coarse_score")).toDouble(), 1.25);
    QCOMPARE(map.value(QStringLiteral("target_region_hit_ratio")).toDouble(), 0.81);
    QCOMPARE(map.value(QStringLiteral("coverage_score")).toDouble(), 0.77);
    QCOMPARE(map.value(QStringLiteral("converged")).toBool(), true);
    QCOMPARE(map.value(QStringLiteral("multi_resolution_level")).toInt(), 2);
}

QTEST_APPLESS_MAIN(AnkleRegistrationParallelSearchTest)
#include "AnkleRegistrationParallelSearchTest.moc"
