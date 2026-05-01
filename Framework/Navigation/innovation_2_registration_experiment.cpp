#include "Framework/Navigation/innovation_2_registration_experiment.h"

QList<InnovationExperimentRecord> Innovation2RegistrationExperiment::run(
    const Innovation2RegistrationInput& input) const
{
    QList<InnovationExperimentRecord> records;
    records.reserve(input.registrationMethodIds.size());

    for (const QString& methodId : input.registrationMethodIds) {
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
