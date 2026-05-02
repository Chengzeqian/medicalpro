#include "Framework/Navigation/innovation_1_point_selection_experiment.h"

QList<InnovationExperimentRecord> Innovation1PointSelectionExperiment::run(
    const Innovation1PointSelectionInput& input) const
{
    QList<InnovationExperimentRecord> records;
    records.reserve(input.strategyIds.size());

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
