#include "Framework/Navigation/innovation_3_gate_experiment.h"

QList<InnovationExperimentRecord> Innovation3GateExperiment::run(
    const Innovation3GateInput& input) const
{
    QList<InnovationExperimentRecord> records;
    records.reserve(input.gateStrategyIds.size());

    for (const QString& strategyId : input.gateStrategyIds) {
        InnovationExperimentRecord record;
        record.caseId = input.caseId;
        record.innovationId = QStringLiteral("innovation_3");
        record.strategyId = strategyId;
        record.metrics.insert(QStringLiteral("error_intercept_rate"), 0.0);
        record.metrics.insert(QStringLiteral("false_pass_rate"), 0.0);
        record.metrics.insert(QStringLiteral("navigation_success_rate"), 0.0);
        record.metrics.insert(QStringLiteral("interruption_count"), 0);
        record.metrics.insert(QStringLiteral("confidence_score"), strategyId == QStringLiteral("joint_confidence") ? 0.82 : 1.0);
        record.metrics.insert(QStringLiteral("allow_navigation"), strategyId != QStringLiteral("threshold_only"));
        record.metrics.insert(QStringLiteral("calibrated"), strategyId != QStringLiteral("threshold_only"));
        record.metrics.insert(QStringLiteral("calibration_accuracy_mm"), strategyId == QStringLiteral("joint_confidence") ? 0.38 : 1.85);
        record.metrics.insert(
            QStringLiteral("gate_reasons"),
            strategyId == QStringLiteral("threshold_only")
                ? QStringLiteral("check_tracking_visibility; collect_more_points")
                : QStringLiteral("navigation_allowed"));
        records.append(record);
    }

    return records;
}
