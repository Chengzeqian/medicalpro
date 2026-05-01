#include "Framework/Navigation/innovation_experiment_batch_runner.h"

#include "Framework/Navigation/innovation_1_point_selection_experiment.h"
#include "Framework/Navigation/innovation_2_registration_experiment.h"
#include "Framework/Navigation/innovation_3_gate_experiment.h"
#include "Framework/Navigation/innovation_summary_csv_exporter.h"

#include <QDir>

namespace
{
QStringList innovation1StrategyIds()
{
    return QStringList({
        QStringLiteral("target_sensitive"),
        QStringLiteral("random"),
        QStringLiteral("uniform"),
        QStringLiteral("expert_rule")
    });
}

QStringList innovation2MethodIds()
{
    return QStringList({
        QStringLiteral("single_stage_landmark"),
        QStringLiteral("landmark_plus_global_icp"),
        QStringLiteral("landmark_plus_global_gicp"),
        QStringLiteral("ankle_two_stage_constrained")
    });
}

QStringList innovation3GateIds()
{
    return QStringList({
        QStringLiteral("no_gate"),
        QStringLiteral("threshold_only"),
        QStringLiteral("joint_confidence")
    });
}
}

InnovationBatchOutput InnovationExperimentBatchRunner::run(const InnovationBatchInput& input) const
{
    InnovationBatchOutput output;

    Innovation1PointSelectionExperiment innovation1;
    Innovation2RegistrationExperiment innovation2;
    Innovation3GateExperiment innovation3;
    InnovationSummaryCsvExporter exporter;

    QList<InnovationExperimentRecord> innovation1Records;
    QList<InnovationExperimentRecord> innovation2Records;
    QList<InnovationExperimentRecord> innovation3Records;

    for (const QString& caseId : input.caseIds) {
        Innovation1PointSelectionInput innovation1Input;
        innovation1Input.caseId = caseId;
        innovation1Input.strategyIds = innovation1StrategyIds();
        innovation1Input.pointBudget = 5;
        innovation1Records.append(innovation1.run(innovation1Input));

        Innovation2RegistrationInput innovation2Input;
        innovation2Input.caseId = caseId;
        innovation2Input.registrationMethodIds = innovation2MethodIds();
        innovation2Records.append(innovation2.run(innovation2Input));

        Innovation3GateInput innovation3Input;
        innovation3Input.caseId = caseId;
        innovation3Input.gateStrategyIds = innovation3GateIds();
        innovation3Records.append(innovation3.run(innovation3Input));
    }

    const QString outputRoot = QDir::currentPath() + QStringLiteral("/summaries");
    const QString innovation1File = exporter.defaultFileName(QStringLiteral("innovation_1"));
    const QString innovation2File = exporter.defaultFileName(QStringLiteral("innovation_2"));
    const QString innovation3File = exporter.defaultFileName(QStringLiteral("innovation_3"));

    if (exporter.exportRecords(outputRoot + QStringLiteral("/") + innovation1File, innovation1Records)) {
        output.summaryFiles.append(innovation1File);
    }
    if (exporter.exportRecords(outputRoot + QStringLiteral("/") + innovation2File, innovation2Records)) {
        output.summaryFiles.append(innovation2File);
    }
    if (exporter.exportRecords(outputRoot + QStringLiteral("/") + innovation3File, innovation3Records)) {
        output.summaryFiles.append(innovation3File);
    }

    return output;
}
