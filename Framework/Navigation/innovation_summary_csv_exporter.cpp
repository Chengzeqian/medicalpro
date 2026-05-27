#include "Framework/Navigation/innovation_summary_csv_exporter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace
{
QString boolCsv(bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

QByteArray csvHeaderFor(const QString& innovationId)
{
    if (innovationId == QStringLiteral("innovation_2")) {
        return QByteArray(
            "case_id,innovation_id,strategy_id,fre_mm,overall_tre_mm,target_tre_mm,"
            "raw_fre_mm,raw_overall_tre_mm,raw_target_tre_mm,"
            "convergence_success,runtime_ms,candidate_count,top_k_count,coarse_search_ms,"
            "best_candidate_rank,coarse_score,parallel_search_enabled,multi_resolution_profile,"
            "used_case_planning,used_case_model_assets,used_anatomical_regions,used_planned_constraint_regions,"
            "case_model_asset_count,tibia_distal_point_count,talus_dome_point_count,anatomical_region_point_count,"
            "case_loaded_bones,roi_radius_mm,roi_center_x,roi_center_y,roi_center_z,roi_point_count\n");
    }
    if (innovationId == QStringLiteral("innovation_3")) {
        return QByteArray(
            "case_id,innovation_id,strategy_id,error_intercept_rate,false_pass_rate,navigation_success_rate,"
            "interruption_count,confidence_score,allow_navigation,calibrated,calibration_accuracy_mm,"
            "twin_confidence_score,local_risk_score,target_region_distance_mm,target_region_angle_error_deg,"
            "dominant_risk_source,re_register_recommended,tracking_degradation_detected,gate_reasons\n");
    }

    return QByteArray("case_id,innovation_id,strategy_id,point_budget,target_tre_mm,overall_tre_mm,point_count,picking_time_ms\n");
}

QByteArray csvRowFor(const InnovationExperimentRecord& record)
{
    if (record.innovationId == QStringLiteral("innovation_2")) {
        const QStringList columns = {
            record.caseId,
            record.innovationId,
            record.strategyId,
            QString::number(record.metrics.value(QStringLiteral("fre_mm")).toDouble(), 'f', 4),
            QString::number(record.metrics.value(QStringLiteral("overall_tre_mm")).toDouble(), 'f', 4),
            QString::number(record.metrics.value(QStringLiteral("target_tre_mm")).toDouble(), 'f', 4),
            QString::number(record.metrics.value(QStringLiteral("raw_fre_mm")).toDouble(), 'f', 4),
            QString::number(record.metrics.value(QStringLiteral("raw_overall_tre_mm")).toDouble(), 'f', 4),
            QString::number(record.metrics.value(QStringLiteral("raw_target_tre_mm")).toDouble(), 'f', 4),
            boolCsv(record.metrics.value(QStringLiteral("convergence_success")).toBool()),
            QString::number(record.metrics.value(QStringLiteral("runtime_ms")).toDouble(), 'f', 4),
            QString::number(record.metrics.value(QStringLiteral("candidate_count")).toInt()),
            QString::number(record.metrics.value(QStringLiteral("top_k_count")).toInt()),
            QString::number(record.metrics.value(QStringLiteral("coarse_search_ms")).toDouble(), 'f', 4),
            QString::number(record.metrics.value(QStringLiteral("best_candidate_rank")).toInt()),
            QString::number(record.metrics.value(QStringLiteral("coarse_score")).toDouble(), 'f', 4),
            boolCsv(record.metrics.value(QStringLiteral("parallel_search_enabled")).toBool()),
            record.metrics.value(QStringLiteral("multi_resolution_profile")).toString(),
            boolCsv(record.metrics.value(QStringLiteral("used_case_planning")).toBool()),
            boolCsv(record.metrics.value(QStringLiteral("used_case_model_assets")).toBool()),
            boolCsv(record.metrics.value(QStringLiteral("used_anatomical_regions")).toBool()),
            boolCsv(record.metrics.value(QStringLiteral("used_planned_constraint_regions")).toBool()),
            QString::number(record.metrics.value(QStringLiteral("case_model_asset_count")).toInt()),
            QString::number(record.metrics.value(QStringLiteral("tibia_distal_point_count")).toInt()),
            QString::number(record.metrics.value(QStringLiteral("talus_dome_point_count")).toInt()),
            QString::number(record.metrics.value(QStringLiteral("anatomical_region_point_count")).toInt()),
            record.metrics.value(QStringLiteral("case_loaded_bones")).toString(),
            QString::number(record.metrics.value(QStringLiteral("roi_radius_mm")).toDouble(), 'f', 4),
            QString::number(record.metrics.value(QStringLiteral("roi_center_x")).toDouble(), 'f', 4),
            QString::number(record.metrics.value(QStringLiteral("roi_center_y")).toDouble(), 'f', 4),
            QString::number(record.metrics.value(QStringLiteral("roi_center_z")).toDouble(), 'f', 4),
            QString::number(record.metrics.value(QStringLiteral("roi_point_count")).toInt())
        };
        return (columns.join(QStringLiteral(",")) + QStringLiteral("\n")).toUtf8();
    }
    if (record.innovationId == QStringLiteral("innovation_3")) {
        return QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16,%17,%18,\"%19\"\n")
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
            .arg(record.metrics.value(QStringLiteral("twin_confidence_score")).toDouble(), 0, 'f', 4)
            .arg(record.metrics.value(QStringLiteral("local_risk_score")).toDouble(), 0, 'f', 4)
            .arg(record.metrics.value(QStringLiteral("target_region_distance_mm")).toDouble(), 0, 'f', 4)
            .arg(record.metrics.value(QStringLiteral("target_region_angle_error_deg")).toDouble(), 0, 'f', 4)
            .arg(record.metrics.value(QStringLiteral("dominant_risk_source")).toString())
            .arg(record.metrics.value(QStringLiteral("re_register_recommended")).toBool() ? QStringLiteral("true") : QStringLiteral("false"))
            .arg(record.metrics.value(QStringLiteral("tracking_degradation_detected")).toBool() ? QStringLiteral("true") : QStringLiteral("false"))
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
