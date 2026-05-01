#include "Framework/Navigation/innovation_summary_csv_exporter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace
{
QByteArray csvHeaderFor(const QString& innovationId)
{
    if (innovationId == QStringLiteral("innovation_2")) {
        return QByteArray("case_id,innovation_id,strategy_id,fre_mm,overall_tre_mm,target_tre_mm,convergence_success,runtime_ms\n");
    }
    if (innovationId == QStringLiteral("innovation_3")) {
        return QByteArray("case_id,innovation_id,strategy_id,error_intercept_rate,false_pass_rate,navigation_success_rate,interruption_count,confidence_score,allow_navigation,calibrated,calibration_accuracy_mm,gate_reasons\n");
    }

    return QByteArray("case_id,innovation_id,strategy_id,point_budget,target_tre_mm,overall_tre_mm,point_count,picking_time_ms\n");
}

QByteArray csvRowFor(const InnovationExperimentRecord& record)
{
    if (record.innovationId == QStringLiteral("innovation_2")) {
        return QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8\n")
            .arg(record.caseId)
            .arg(record.innovationId)
            .arg(record.strategyId)
            .arg(record.metrics.value(QStringLiteral("fre_mm")).toDouble(), 0, 'f', 4)
            .arg(record.metrics.value(QStringLiteral("overall_tre_mm")).toDouble(), 0, 'f', 4)
            .arg(record.metrics.value(QStringLiteral("target_tre_mm")).toDouble(), 0, 'f', 4)
            .arg(record.metrics.value(QStringLiteral("convergence_success")).toBool() ? QStringLiteral("true") : QStringLiteral("false"))
            .arg(record.metrics.value(QStringLiteral("runtime_ms")).toDouble(), 0, 'f', 4)
            .toUtf8();
    }
    if (record.innovationId == QStringLiteral("innovation_3")) {
        return QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,\"%12\"\n")
            .arg(record.caseId)
            .arg(record.innovationId)
            .arg(record.strategyId)
            .arg(record.metrics.value(QStringLiteral("error_intercept_rate")).toDouble(), 0, 'f', 4)
            .arg(record.metrics.value(QStringLiteral("false_pass_rate")).toDouble(), 0, 'f', 4)
            .arg(record.metrics.value(QStringLiteral("navigation_success_rate")).toDouble(), 0, 'f', 4)
            .arg(record.metrics.value(QStringLiteral("interruption_count")).toInt())
            .arg(record.metrics.value(QStringLiteral("confidence_score")).toDouble(), 0, 'f', 4)
            .arg(record.metrics.value(QStringLiteral("allow_navigation")).toBool() ? QStringLiteral("true") : QStringLiteral("false"))
            .arg(record.metrics.value(QStringLiteral("calibrated")).toBool() ? QStringLiteral("true") : QStringLiteral("false"))
            .arg(record.metrics.value(QStringLiteral("calibration_accuracy_mm")).toDouble(), 0, 'f', 4)
            .arg(record.metrics.value(QStringLiteral("gate_reasons")).toString())
            .toUtf8();
    }

    return QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8\n")
        .arg(record.caseId)
        .arg(record.innovationId)
        .arg(record.strategyId)
        .arg(record.perturbation.pointBudget)
        .arg(record.metrics.value(QStringLiteral("target_tre_mm")).toDouble(), 0, 'f', 4)
        .arg(record.metrics.value(QStringLiteral("overall_tre_mm")).toDouble(), 0, 'f', 4)
        .arg(record.metrics.value(QStringLiteral("point_count")).toInt())
        .arg(record.metrics.value(QStringLiteral("picking_time_ms")).toDouble(), 0, 'f', 4)
        .toUtf8();
}
}

QString InnovationSummaryCsvExporter::defaultFileName(const QString& innovationId) const
{
    return innovationId + QStringLiteral("_summary.csv");
}

bool InnovationSummaryCsvExporter::exportRecords(
    const QString& outputPath,
    const QList<InnovationExperimentRecord>& records) const
{
    QDir dir;
    if (!dir.mkpath(QFileInfo(outputPath).absolutePath())) {
        return false;
    }

    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }

    file.write(csvHeaderFor(records.isEmpty() ? QStringLiteral("innovation_1") : records.first().innovationId));
    for (const InnovationExperimentRecord& record : records) {
        file.write(csvRowFor(record));
    }

    return file.error() == QFile::NoError;
}
