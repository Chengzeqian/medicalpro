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
    void parallel_search_plan_spans_roll_and_three_axis_translation_perturbations();
    void parallel_search_plan_includes_medium_compound_pose_candidates_within_default_budget();
    void parallel_search_keeps_lowest_scored_top_k_candidates();
    void parallel_search_uses_hit_ratio_and_coverage_to_break_close_coarse_score_ties();
    void parallel_search_prefers_roi_stable_candidate_over_slightly_lower_distance();
    void parallel_search_prefers_geometrically_consistent_candidate_when_distance_is_close();
    void parallel_search_returns_empty_top_k_for_non_positive_count();
    void parallel_search_profile_resolves_three_level_cell_sizes();
    void parallel_search_profile_resolves_two_level_cell_sizes();
    void parallel_search_profile_falls_back_to_single_level_for_unknown_profile();
    void parallel_search_extracts_candidate_ids_in_order();
    void parallel_search_filters_candidates_by_requested_ids();
    void parallel_search_adaptive_refine_reduces_when_identity_is_best_strong_roi_score();
    void parallel_search_adaptive_refine_reduces_when_identity_is_close_to_best_strong_roi_score();
    void parallel_search_adaptive_refine_keeps_full_set_when_identity_is_not_close_to_best();
    void parallel_search_adaptive_refine_keeps_full_set_when_roi_score_is_uncertain();
    void parallel_search_adaptive_refine_can_be_disabled();
    void parallel_search_converts_candidate_evaluation_to_variant_map();
    void initial_admission_accepts_confident_identity_for_fast_path();
    void initial_admission_allows_refine_for_medium_quality_initial();
    void initial_admission_rejects_large_offset_initial_before_refine();
    void initial_admission_rejects_large_robust_residual_even_when_surface_score_is_good();
    void initial_admission_requires_refine_when_robust_residual_is_above_fast_path_limit();
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
    QCOMPARE(options.multiResolutionProfileId, QStringLiteral("ankle_roi_two_level"));
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

void AnkleRegistrationParallelSearchTest::parallel_search_plan_spans_roll_and_three_axis_translation_perturbations()
{
    ParallelSearchPlan plan;
    plan.candidateCount = 64;

    const QList<CandidateInitialTransform> candidates =
        buildCandidateInitialTransforms(QMatrix4x4(), QVector3D(12.0f, -8.0f, 4.0f), plan);

    bool hasRollPerturbation = false;
    bool hasPositiveXTranslation = false;
    bool hasNegativeXTranslation = false;
    bool hasPositiveYTranslation = false;
    bool hasNegativeYTranslation = false;
    bool hasPositiveZTranslation = false;
    bool hasNegativeZTranslation = false;

    for (int index = 1; index < candidates.size(); ++index) {
        const CandidateInitialTransform& candidate = candidates.at(index);
        hasRollPerturbation = hasRollPerturbation || !qFuzzyIsNull(candidate.rotationDeltaDeg.z());
        hasPositiveXTranslation = hasPositiveXTranslation || candidate.translationDeltaMm.x() > 0.0f;
        hasNegativeXTranslation = hasNegativeXTranslation || candidate.translationDeltaMm.x() < 0.0f;
        hasPositiveYTranslation = hasPositiveYTranslation || candidate.translationDeltaMm.y() > 0.0f;
        hasNegativeYTranslation = hasNegativeYTranslation || candidate.translationDeltaMm.y() < 0.0f;
        hasPositiveZTranslation = hasPositiveZTranslation || candidate.translationDeltaMm.z() > 0.0f;
        hasNegativeZTranslation = hasNegativeZTranslation || candidate.translationDeltaMm.z() < 0.0f;
    }

    QVERIFY(hasRollPerturbation);
    QVERIFY(hasPositiveXTranslation);
    QVERIFY(hasNegativeXTranslation);
    QVERIFY(hasPositiveYTranslation);
    QVERIFY(hasNegativeYTranslation);
    QVERIFY(hasPositiveZTranslation);
    QVERIFY(hasNegativeZTranslation);
}

void AnkleRegistrationParallelSearchTest::parallel_search_plan_includes_medium_compound_pose_candidates_within_default_budget()
{
    ParallelSearchPlan plan;
    plan.candidateCount = 64;

    const QList<CandidateInitialTransform> candidates =
        buildCandidateInitialTransforms(QMatrix4x4(), QVector3D(12.0f, -8.0f, 4.0f), plan);

    bool hasMediumCompoundCandidate = false;
    for (int index = 1; index < candidates.size(); ++index) {
        const CandidateInitialTransform& candidate = candidates.at(index);
        const QVector3D translation = candidate.translationDeltaMm;
        const QVector3D rotation = candidate.rotationDeltaDeg;
        const bool hasTranslation = !qFuzzyIsNull(translation.lengthSquared());
        const bool hasRotation = !qFuzzyIsNull(rotation.lengthSquared());
        const bool hasMediumTranslation = translation.length() >= 1.6f;
        const bool hasMediumRotation = rotation.length() >= 3.0f;
        if (hasTranslation && hasRotation && hasMediumTranslation && hasMediumRotation) {
            hasMediumCompoundCandidate = true;
            break;
        }
    }

    QVERIFY(hasMediumCompoundCandidate);
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

void AnkleRegistrationParallelSearchTest::parallel_search_uses_hit_ratio_and_coverage_to_break_close_coarse_score_ties()
{
    const QList<CandidateEvaluationResult> scores = {
        { QStringLiteral("candidate_001"), 1.00, 0.55, 0.48, true, 0 },
        { QStringLiteral("candidate_002"), 1.03, 0.93, 0.89, true, 0 },
        { QStringLiteral("candidate_003"), 1.32, 0.99, 0.96, true, 0 }
    };

    const QList<CandidateEvaluationResult> topK = selectTopKCandidates(scores, 2);

    QCOMPARE(topK.size(), 2);
    QCOMPARE(topK.at(0).candidateId, QStringLiteral("candidate_002"));
    QCOMPARE(topK.at(1).candidateId, QStringLiteral("candidate_001"));
}

void AnkleRegistrationParallelSearchTest::parallel_search_prefers_roi_stable_candidate_over_slightly_lower_distance()
{
    const QList<CandidateEvaluationResult> scores = {
        { QStringLiteral("candidate_distance_only"), 1.00, 0.20, 0.18, true, 0 },
        { QStringLiteral("candidate_roi_stable"), 1.18, 0.96, 0.92, true, 0 },
        { QStringLiteral("candidate_far"), 1.72, 0.99, 0.95, true, 0 }
    };

    const QList<CandidateEvaluationResult> topK = selectTopKCandidates(scores, 2);

    QCOMPARE(topK.size(), 2);
    QCOMPARE(topK.at(0).candidateId, QStringLiteral("candidate_roi_stable"));
    QCOMPARE(topK.at(1).candidateId, QStringLiteral("candidate_distance_only"));
}

void AnkleRegistrationParallelSearchTest::parallel_search_prefers_geometrically_consistent_candidate_when_distance_is_close()
{
    const QList<CandidateEvaluationResult> scores = {
        { QStringLiteral("candidate_distance_only"), 1.00, 0.90, 0.88, true, 0, 0.10, 0.15 },
        { QStringLiteral("candidate_geometry_stable"), 1.12, 0.91, 0.89, true, 0, 0.96, 0.92 },
        { QStringLiteral("candidate_far"), 1.60, 0.96, 0.94, true, 0, 0.98, 0.96 }
    };

    const QList<CandidateEvaluationResult> topK = selectTopKCandidates(scores, 2);

    QCOMPARE(topK.size(), 2);
    QCOMPARE(topK.at(0).candidateId, QStringLiteral("candidate_geometry_stable"));
    QCOMPARE(topK.at(1).candidateId, QStringLiteral("candidate_distance_only"));
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
    QCOMPARE(cellSizes.at(2), 1.0);
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

void AnkleRegistrationParallelSearchTest::parallel_search_adaptive_refine_reduces_when_identity_is_best_strong_roi_score()
{
    const QList<CandidateEvaluationResult> topK = {
        { QStringLiteral("candidate_000"), 1.70, 0.88, 0.80, true, 2 },
        { QStringLiteral("candidate_012"), 1.75, 0.83, 0.76, true, 2 },
        { QStringLiteral("candidate_003"), 1.82, 0.81, 0.72, true, 2 },
        { QStringLiteral("candidate_018"), 1.91, 0.80, 0.70, true, 2 }
    };

    const QList<CandidateEvaluationResult> selected =
        selectRefineCandidates(topK, 4, true);

    QCOMPARE(selected.size(), 2);
    QCOMPARE(selected.at(0).candidateId, QStringLiteral("candidate_000"));
    QCOMPARE(selected.at(1).candidateId, QStringLiteral("candidate_012"));
}

void AnkleRegistrationParallelSearchTest::parallel_search_adaptive_refine_reduces_when_identity_is_close_to_best_strong_roi_score()
{
    const QList<CandidateEvaluationResult> topK = {
        { QStringLiteral("candidate_003"), 1.70, 0.88, 0.80, true, 2 },
        { QStringLiteral("candidate_012"), 1.75, 0.83, 0.76, true, 2 },
        { QStringLiteral("candidate_000"), 1.82, 0.81, 0.72, true, 2 },
        { QStringLiteral("candidate_018"), 1.91, 0.80, 0.70, true, 2 }
    };

    const QList<CandidateEvaluationResult> selected =
        selectRefineCandidates(topK, 4, true);

    QCOMPARE(selected.size(), 2);
    QCOMPARE(selected.at(0).candidateId, QStringLiteral("candidate_003"));
    QCOMPARE(selected.at(1).candidateId, QStringLiteral("candidate_000"));
}

void AnkleRegistrationParallelSearchTest::parallel_search_adaptive_refine_keeps_full_set_when_identity_is_not_close_to_best()
{
    const QList<CandidateEvaluationResult> topK = {
        { QStringLiteral("candidate_003"), 1.70, 0.88, 0.80, true, 2 },
        { QStringLiteral("candidate_012"), 1.75, 0.83, 0.76, true, 2 },
        { QStringLiteral("candidate_018"), 1.91, 0.80, 0.70, true, 2 },
        { QStringLiteral("candidate_000"), 2.12, 0.75, 0.65, true, 2 }
    };

    const QList<CandidateEvaluationResult> selected =
        selectRefineCandidates(topK, 4, true);

    QCOMPARE(candidateIds(selected), candidateIds(topK));
}

void AnkleRegistrationParallelSearchTest::parallel_search_adaptive_refine_keeps_full_set_when_roi_score_is_uncertain()
{
    const QList<CandidateEvaluationResult> topK = {
        { QStringLiteral("candidate_003"), 2.35, 0.62, 0.58, true, 2 },
        { QStringLiteral("candidate_012"), 2.38, 0.61, 0.56, true, 2 },
        { QStringLiteral("candidate_000"), 2.43, 0.60, 0.55, true, 2 },
        { QStringLiteral("candidate_018"), 2.51, 0.58, 0.52, true, 2 }
    };

    const QList<CandidateEvaluationResult> selected =
        selectRefineCandidates(topK, 4, true);

    QCOMPARE(candidateIds(selected), candidateIds(topK));
}

void AnkleRegistrationParallelSearchTest::parallel_search_adaptive_refine_can_be_disabled()
{
    const QList<CandidateEvaluationResult> topK = {
        { QStringLiteral("candidate_003"), 1.70, 0.88, 0.80, true, 2 },
        { QStringLiteral("candidate_012"), 1.75, 0.83, 0.76, true, 2 },
        { QStringLiteral("candidate_000"), 1.82, 0.81, 0.72, true, 2 },
        { QStringLiteral("candidate_018"), 1.91, 0.80, 0.70, true, 2 }
    };

    const QList<CandidateEvaluationResult> selected =
        selectRefineCandidates(topK, 4, false);

    QCOMPARE(candidateIds(selected), candidateIds(topK));
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
    QVERIFY(map.contains(QStringLiteral("normal_consistency_score")));
    QVERIFY(map.contains(QStringLiteral("curvature_score")));
    QVERIFY(map.contains(QStringLiteral("geometry_score_available")));
    QVERIFY(map.contains(QStringLiteral("selection_score")));
    QCOMPARE(map.value(QStringLiteral("converged")).toBool(), true);
    QCOMPARE(map.value(QStringLiteral("multi_resolution_level")).toInt(), 2);
}

void AnkleRegistrationParallelSearchTest::initial_admission_accepts_confident_identity_for_fast_path()
{
    InitialAdmissionPolicy policy;
    const CandidateEvaluationResult identity {
        QStringLiteral("candidate_000"), 0.92, 0.93, 0.88, true, 0, 1.0, 1.0
    };
    const QList<CandidateEvaluationResult> topK { identity };

    const InitialAdmissionDecision decision =
        assessInitialAdmission(identity, topK, policy);

    QCOMPARE(decision.action, QStringLiteral("fast_path"));
    QVERIFY(decision.accepted);
    QCOMPARE(decision.reason, QStringLiteral("confident_identity_initial"));
}

void AnkleRegistrationParallelSearchTest::initial_admission_allows_refine_for_medium_quality_initial()
{
    InitialAdmissionPolicy policy;
    const CandidateEvaluationResult identity {
        QStringLiteral("candidate_000"), 2.35, 0.78, 0.71, true, 1, 0.82, 0.76
    };
    const QList<CandidateEvaluationResult> topK {
        { QStringLiteral("candidate_002"), 2.08, 0.81, 0.74, true, 1, 0.86, 0.79 },
        identity,
        { QStringLiteral("candidate_008"), 2.42, 0.76, 0.70, true, 1, 0.80, 0.72 }
    };

    const InitialAdmissionDecision decision =
        assessInitialAdmission(identity, topK, policy);

    QCOMPARE(decision.action, QStringLiteral("refine"));
    QVERIFY(decision.accepted);
    QCOMPARE(decision.reason, QStringLiteral("medium_quality_initial_requires_refine"));
}

void AnkleRegistrationParallelSearchTest::initial_admission_rejects_large_offset_initial_before_refine()
{
    InitialAdmissionPolicy policy;
    const CandidateEvaluationResult identity {
        QStringLiteral("candidate_000"), 11.24, 0.22, 0.18, true, 0, 0.10, 0.12
    };
    const QList<CandidateEvaluationResult> topK {
        { QStringLiteral("candidate_018"), 7.80, 0.31, 0.26, true, 1, 0.28, 0.25 },
        { QStringLiteral("candidate_044"), 8.15, 0.29, 0.25, true, 1, 0.23, 0.20 },
        identity
    };

    const InitialAdmissionDecision decision =
        assessInitialAdmission(identity, topK, policy);

    QCOMPARE(decision.action, QStringLiteral("reject"));
    QVERIFY(!decision.accepted);
    QCOMPARE(decision.reason, QStringLiteral("initial_score_exceeds_recovery_threshold"));
    QCOMPARE(decision.recoveryAction, QStringLiteral("resample_probe_points_or_check_probe_calibration"));
}

void AnkleRegistrationParallelSearchTest::initial_admission_rejects_large_robust_residual_even_when_surface_score_is_good()
{
    InitialAdmissionPolicy policy;
    const CandidateEvaluationResult identity {
        QStringLiteral("candidate_000"), 0.95, 0.92, 0.86, true, 0, 1.0, 1.0
    };
    InitialAdmissionEvidence evidence;
    evidence.hasRobustInitialMetrics = true;
    evidence.robustInitialRmsMm = 11.24;
    evidence.robustInitialConfidence = 0.88;
    evidence.robustInitialInlierCount = 6;

    const InitialAdmissionDecision decision =
        assessInitialAdmission(identity, { identity }, policy, evidence);

    QCOMPARE(decision.action, QStringLiteral("reject"));
    QVERIFY(!decision.accepted);
    QCOMPARE(decision.reason, QStringLiteral("robust_initial_residual_exceeds_recovery_threshold"));
    QCOMPARE(decision.recoveryAction, QStringLiteral("resample_probe_points_or_check_probe_calibration"));
}

void AnkleRegistrationParallelSearchTest::initial_admission_requires_refine_when_robust_residual_is_above_fast_path_limit()
{
    InitialAdmissionPolicy policy;
    const CandidateEvaluationResult identity {
        QStringLiteral("candidate_000"), 0.95, 0.92, 0.86, true, 0, 1.0, 1.0
    };
    InitialAdmissionEvidence evidence;
    evidence.hasRobustInitialMetrics = true;
    evidence.robustInitialRmsMm = 3.37;
    evidence.robustInitialConfidence = 0.85;
    evidence.robustInitialInlierCount = 240;

    const InitialAdmissionDecision decision =
        assessInitialAdmission(identity, { identity }, policy, evidence);

    QCOMPARE(decision.action, QStringLiteral("refine"));
    QVERIFY(decision.accepted);
    QCOMPARE(decision.reason, QStringLiteral("robust_initial_residual_requires_refine"));
}

QTEST_APPLESS_MAIN(AnkleRegistrationParallelSearchTest)
#include "AnkleRegistrationParallelSearchTest.moc"
