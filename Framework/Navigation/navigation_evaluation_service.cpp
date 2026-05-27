#include "Framework/Navigation/navigation_evaluation_service.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace
{
QJsonArray toJsonArray(const QStringList& values)
{
    QJsonArray array;
    for (const QString& value : values) {
        array.append(value);
    }
    return array;
}

QStringList toStringList(const QJsonArray& values)
{
    QStringList result;
    for (const QJsonValue& value : values) {
        result.append(value.toString());
    }
    return result;
}

QString csvValue(const QVariant& value)
{
    if (value.type() == QVariant::Bool) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }

    switch (value.type()) {
        case QVariant::Double:
            return QString::number(value.toDouble(), 'f', 4);
        case QVariant::Int:
        case QVariant::UInt:
        case QVariant::LongLong:
        case QVariant::ULongLong:
            return value.toString();
        default:
            break;
    }

    if (value.canConvert<QStringList>()) {
        const QString joined = value.toStringList().join(QStringLiteral("|"));
        return QStringLiteral("\"%1\"").arg(joined);
    }

    const QString text = value.toString();
    if (text.contains(QLatin1Char(',')) || text.contains(QLatin1Char('"')) || text.contains(QLatin1Char('|'))) {
        QString escaped = text;
        escaped.replace(QStringLiteral("\""), QStringLiteral("\"\""));
        return QStringLiteral("\"%1\"").arg(escaped);
    }

    return text;
}

QStringList appendMetricsLines(const QVariantMap& metrics)
{
    QStringList lines;
    for (auto it = metrics.cbegin(); it != metrics.cend(); ++it) {
        lines.append(QStringLiteral("%1,%2").arg(it.key(), csvValue(it.value())));
    }
    return lines;
}

QJsonObject toJson(const AnkleRegistrationRecord& record)
{
    QJsonObject object;
    object.insert(QStringLiteral("case_id"), record.caseId);
    object.insert(QStringLiteral("registration_mode"), record.registrationMode);
    object.insert(QStringLiteral("fre"), record.fre);
    object.insert(QStringLiteral("target_tre"), record.targetTre);
    object.insert(QStringLiteral("coverage_score"), record.coverageScore);
    object.insert(QStringLiteral("metrics"), QJsonObject::fromVariantMap(record.metrics));
    return object;
}

QJsonObject toJson(const AnkleNavigationRunRecord& record)
{
    QJsonObject object;
    object.insert(QStringLiteral("case_id"), record.caseId);
    object.insert(QStringLiteral("navigation_mode"), record.navigationMode);
    object.insert(QStringLiteral("confidence_score"), record.confidenceScore);
    object.insert(QStringLiteral("warnings"), toJsonArray(record.warnings));
    object.insert(QStringLiteral("metrics"), QJsonObject::fromVariantMap(record.metrics));
    return object;
}

QJsonObject toJson(const AnkleEvaluationReport& report)
{
    QJsonObject object;
    object.insert(QStringLiteral("case_id"), report.caseId);
    object.insert(QStringLiteral("translation_error_mm"), report.translationErrorMm);
    object.insert(QStringLiteral("rotation_error_deg"), report.rotationErrorDeg);
    object.insert(QStringLiteral("allow_navigation"), report.allowNavigation);
    object.insert(QStringLiteral("confidence_score"), report.confidenceScore);
    object.insert(QStringLiteral("gate_reasons"), toJsonArray(report.gateReasons));
    object.insert(QStringLiteral("calibrated"), report.calibrated);
    object.insert(QStringLiteral("calibration_accuracy_mm"), report.calibrationAccuracyMm);
    object.insert(QStringLiteral("metrics"), QJsonObject::fromVariantMap(report.metrics));
    return object;
}

QJsonObject toJson(const AnkleEvaluationSnapshot& snapshot)
{
    QJsonObject object;
    object.insert(QStringLiteral("case_id"), snapshot.caseId);
    object.insert(QStringLiteral("registration_mode"), snapshot.registrationMode);
    object.insert(QStringLiteral("fre"), snapshot.fre);
    object.insert(QStringLiteral("target_tre"), snapshot.targetTre);
    object.insert(QStringLiteral("coverage_score"), snapshot.coverageScore);
    object.insert(QStringLiteral("navigation_mode"), snapshot.navigationMode);
    object.insert(QStringLiteral("navigation_confidence_score"), snapshot.navigationConfidenceScore);
    object.insert(QStringLiteral("translation_error_mm"), snapshot.translationErrorMm);
    object.insert(QStringLiteral("rotation_error_deg"), snapshot.rotationErrorDeg);
    object.insert(QStringLiteral("allow_navigation"), snapshot.allowNavigation);
    object.insert(QStringLiteral("evaluation_confidence_score"), snapshot.evaluationConfidenceScore);
    object.insert(QStringLiteral("calibrated"), snapshot.calibrated);
    object.insert(QStringLiteral("calibration_accuracy_mm"), snapshot.calibrationAccuracyMm);
    object.insert(QStringLiteral("gate_reasons"), toJsonArray(snapshot.gateReasons));
    object.insert(QStringLiteral("gate_reason_count"), snapshot.evaluationMetrics.value(QStringLiteral("gate_reason_count")).toInt());
    object.insert(QStringLiteral("candidate_count"), snapshot.registrationMetrics.value(QStringLiteral("candidate_count")).toInt());
    object.insert(QStringLiteral("top_k_count"), snapshot.registrationMetrics.value(QStringLiteral("top_k_count")).toInt());
    object.insert(QStringLiteral("coarse_search_ms"), snapshot.registrationMetrics.value(QStringLiteral("coarse_search_ms")).toDouble());
    object.insert(QStringLiteral("best_candidate_rank"), snapshot.registrationMetrics.value(QStringLiteral("best_candidate_rank")).toInt());
    object.insert(QStringLiteral("parallel_search_enabled"), snapshot.registrationMetrics.value(QStringLiteral("parallel_search_enabled")).toBool());
    object.insert(QStringLiteral("target_region_radius_mm"), snapshot.registrationMetrics.value(QStringLiteral("target_region_radius_mm")).toDouble());
    object.insert(QStringLiteral("constraint_region_count"), snapshot.registrationMetrics.value(QStringLiteral("constraint_region_count")).toInt());
    object.insert(QStringLiteral("tracking_jitter_mm"), snapshot.navigationMetrics.value(QStringLiteral("tracking_jitter_mm")).toDouble());
    object.insert(QStringLiteral("visible_frame_ratio"), snapshot.navigationMetrics.value(QStringLiteral("visible_frame_ratio")).toDouble());
    object.insert(QStringLiteral("tracking_confidence_score"), snapshot.navigationMetrics.value(QStringLiteral("tracking_confidence_score")).toDouble());
    object.insert(QStringLiteral("twin_confidence_score"), snapshot.evaluationMetrics.value(QStringLiteral("twin_confidence_score")).toDouble());
    object.insert(QStringLiteral("local_risk_score"), snapshot.evaluationMetrics.value(QStringLiteral("local_risk_score")).toDouble());
    object.insert(QStringLiteral("target_region_distance_mm"), snapshot.evaluationMetrics.value(QStringLiteral("target_region_distance_mm")).toDouble());
    object.insert(QStringLiteral("target_region_angle_error_deg"), snapshot.evaluationMetrics.value(QStringLiteral("target_region_angle_error_deg")).toDouble());
    object.insert(QStringLiteral("dominant_risk_source"), snapshot.evaluationMetrics.value(QStringLiteral("dominant_risk_source")).toString());
    object.insert(QStringLiteral("re_register_recommended"), snapshot.evaluationMetrics.value(QStringLiteral("re_register_recommended")).toBool());
    object.insert(QStringLiteral("tracking_degradation_detected"), snapshot.evaluationMetrics.value(QStringLiteral("tracking_degradation_detected")).toBool());
    object.insert(QStringLiteral("registration_metrics"), QJsonObject::fromVariantMap(snapshot.registrationMetrics));
    object.insert(QStringLiteral("navigation_metrics"), QJsonObject::fromVariantMap(snapshot.navigationMetrics));
    object.insert(QStringLiteral("evaluation_metrics"), QJsonObject::fromVariantMap(snapshot.evaluationMetrics));
    return object;
}

bool writeJsonFile(const QString& path, const QJsonObject& object)
{
    QDir dir;
    if (!dir.mkpath(QFileInfo(path).absolutePath())) {
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    return file.error() == QFile::NoError;
}

QJsonObject readJsonFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object() : QJsonObject();
}
}

NavigationEvaluationService::NavigationEvaluationService(const QString& casesRoot)
    : m_casesRoot(casesRoot)
{
}

bool NavigationEvaluationService::saveRegistrationRecord(const AnkleRegistrationRecord& record) const
{
    return writeJsonFile(registrationPath(record.caseId), toJson(record));
}

bool NavigationEvaluationService::saveNavigationRun(const AnkleNavigationRunRecord& record) const
{
    return writeJsonFile(navigationPath(record.caseId), toJson(record));
}

bool NavigationEvaluationService::saveEvaluationReport(const AnkleEvaluationReport& report) const
{
    return writeJsonFile(evaluationPath(report.caseId), toJson(report));
}

bool NavigationEvaluationService::exportMetricsCsv(const QString& caseId) const
{
    const QJsonObject registration = readJsonFile(registrationPath(caseId));
    const QJsonObject navigation = readJsonFile(navigationPath(caseId));
    const QJsonObject evaluation = readJsonFile(evaluationPath(caseId));
    const QVariantMap registrationMetrics = registration.value(QStringLiteral("metrics")).toObject().toVariantMap();
    const QVariantMap navigationMetrics = navigation.value(QStringLiteral("metrics")).toObject().toVariantMap();
    const QVariantMap evaluationMetrics = evaluation.value(QStringLiteral("metrics")).toObject().toVariantMap();
    const QString gateReasons =
        toStringList(evaluation.value(QStringLiteral("gate_reasons")).toArray()).join(QStringLiteral("; "));

    QDir dir;
    if (!dir.mkpath(QFileInfo(metricsCsvPath(caseId)).absolutePath())) {
        return false;
    }

    QFile file(metricsCsvPath(caseId));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }

    QStringList lines = {
        QStringLiteral("metric,value"),
        QStringLiteral("case_id,%1").arg(caseId),
        QStringLiteral("registration_mode,%1").arg(registration.value(QStringLiteral("registration_mode")).toString()),
        QStringLiteral("fre,%1").arg(registration.value(QStringLiteral("fre")).toDouble(), 0, 'f', 4),
        QStringLiteral("target_tre,%1").arg(registration.value(QStringLiteral("target_tre")).toDouble(), 0, 'f', 4),
        QStringLiteral("coverage_score,%1").arg(registration.value(QStringLiteral("coverage_score")).toDouble(), 0, 'f', 4),
        QStringLiteral("navigation_mode,%1").arg(navigation.value(QStringLiteral("navigation_mode")).toString()),
        QStringLiteral("confidence_score,%1").arg(navigation.value(QStringLiteral("confidence_score")).toDouble(), 0, 'f', 4),
        QStringLiteral("translation_error_mm,%1").arg(evaluation.value(QStringLiteral("translation_error_mm")).toDouble(), 0, 'f', 4),
        QStringLiteral("rotation_error_deg,%1").arg(evaluation.value(QStringLiteral("rotation_error_deg")).toDouble(), 0, 'f', 4),
        QStringLiteral("allow_navigation,%1").arg(evaluation.value(QStringLiteral("allow_navigation")).toBool() ? QStringLiteral("true") : QStringLiteral("false")),
        QStringLiteral("evaluation_confidence_score,%1").arg(evaluation.value(QStringLiteral("confidence_score")).toDouble(), 0, 'f', 4),
        QStringLiteral("gate_reasons,\"%1\"").arg(gateReasons),
        QStringLiteral("calibrated,%1").arg(evaluation.value(QStringLiteral("calibrated")).toBool() ? QStringLiteral("true") : QStringLiteral("false")),
        QStringLiteral("calibration_accuracy_mm,%1").arg(evaluation.value(QStringLiteral("calibration_accuracy_mm")).toDouble(), 0, 'f', 4)
    };

    lines.append(appendMetricsLines(registrationMetrics));
    lines.append(appendMetricsLines(navigationMetrics));
    lines.append(appendMetricsLines(evaluationMetrics));

    file.write(lines.join(QStringLiteral("\n")).toUtf8());
    return file.error() == QFile::NoError;
}

AnkleEvaluationSnapshot NavigationEvaluationService::loadEvaluationSnapshot(const QString& caseId) const
{
    AnkleEvaluationSnapshot snapshot;
    snapshot.caseId = caseId;

    const QJsonObject registration = readJsonFile(registrationPath(caseId));
    const QJsonObject navigation = readJsonFile(navigationPath(caseId));
    const QJsonObject evaluation = readJsonFile(evaluationPath(caseId));

    snapshot.hasRegistration = !registration.isEmpty();
    snapshot.hasNavigationRun = !navigation.isEmpty();
    snapshot.hasEvaluationReport = !evaluation.isEmpty();

    if (snapshot.hasRegistration) {
        snapshot.registrationMode = registration.value(QStringLiteral("registration_mode")).toString();
        snapshot.fre = registration.value(QStringLiteral("fre")).toDouble();
        snapshot.targetTre = registration.value(QStringLiteral("target_tre")).toDouble();
        snapshot.coverageScore = registration.value(QStringLiteral("coverage_score")).toDouble();
        snapshot.registrationMetrics = registration.value(QStringLiteral("metrics")).toObject().toVariantMap();
    }

    if (snapshot.hasNavigationRun) {
        snapshot.navigationMode = navigation.value(QStringLiteral("navigation_mode")).toString();
        snapshot.navigationConfidenceScore = navigation.value(QStringLiteral("confidence_score")).toDouble();
        snapshot.navigationMetrics = navigation.value(QStringLiteral("metrics")).toObject().toVariantMap();
    }

    if (snapshot.hasEvaluationReport) {
        snapshot.translationErrorMm = evaluation.value(QStringLiteral("translation_error_mm")).toDouble();
        snapshot.rotationErrorDeg = evaluation.value(QStringLiteral("rotation_error_deg")).toDouble();
        snapshot.allowNavigation = evaluation.value(QStringLiteral("allow_navigation")).toBool();
        snapshot.evaluationConfidenceScore = evaluation.value(QStringLiteral("confidence_score")).toDouble();
        snapshot.gateReasons = toStringList(evaluation.value(QStringLiteral("gate_reasons")).toArray());
        snapshot.calibrated = evaluation.value(QStringLiteral("calibrated")).toBool();
        snapshot.calibrationAccuracyMm = evaluation.value(QStringLiteral("calibration_accuracy_mm")).toDouble();
        snapshot.evaluationMetrics = evaluation.value(QStringLiteral("metrics")).toObject().toVariantMap();
    }

    return snapshot;
}

bool NavigationEvaluationService::exportCaseSummary(const QString& caseId) const
{
    return writeJsonFile(caseSummaryPath(caseId), toJson(loadEvaluationSnapshot(caseId)));
}

QStringList NavigationEvaluationService::discoverExportableCaseIds() const
{
    const QDir casesDir(m_casesRoot);
    const QFileInfoList entries = casesDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    QStringList caseIds;
    for (const QFileInfo& entry : entries) {
        if (entry.fileName() == QStringLiteral("summaries")) {
            continue;
        }

        if (QFileInfo::exists(evaluationPath(entry.fileName()))) {
            caseIds.append(entry.fileName());
        }
    }

    return caseIds;
}

bool NavigationEvaluationService::exportBatchSummaryCsv(const QStringList& caseIds) const
{
    QDir dir;
    if (!dir.mkpath(QFileInfo(batchSummaryCsvPath()).absolutePath())) {
        return false;
    }

    QFile file(batchSummaryCsvPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }

    QStringList lines = {
        QStringLiteral(
            "case_id,registration_mode,navigation_mode,allow_navigation,fre,target_tre,coverage_score,"
            "tracking_jitter_mm,visible_frame_ratio,tracking_confidence_score,gate_reason_count,calibrated,"
            "calibration_accuracy_mm,twin_confidence_score,local_risk_score,target_region_distance_mm,"
            "target_region_angle_error_deg,dominant_risk_source,re_register_recommended,"
            "tracking_degradation_detected")
    };

    for (const QString& caseId : caseIds) {
        const AnkleEvaluationSnapshot snapshot = loadEvaluationSnapshot(caseId);
        lines.append(QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16,%17,%18,%19,%20")
            .arg(snapshot.caseId)
            .arg(snapshot.registrationMode)
            .arg(snapshot.navigationMode)
            .arg(snapshot.allowNavigation ? QStringLiteral("true") : QStringLiteral("false"))
            .arg(snapshot.fre, 0, 'f', 4)
            .arg(snapshot.targetTre, 0, 'f', 4)
            .arg(snapshot.coverageScore, 0, 'f', 4)
            .arg(snapshot.navigationMetrics.value(QStringLiteral("tracking_jitter_mm")).toDouble(), 0, 'f', 4)
            .arg(snapshot.navigationMetrics.value(QStringLiteral("visible_frame_ratio")).toDouble(), 0, 'f', 4)
            .arg(snapshot.navigationMetrics.value(QStringLiteral("tracking_confidence_score")).toDouble(), 0, 'f', 4)
            .arg(snapshot.evaluationMetrics.value(QStringLiteral("gate_reason_count")).toInt())
            .arg(snapshot.calibrated ? QStringLiteral("true") : QStringLiteral("false"))
            .arg(snapshot.calibrationAccuracyMm, 0, 'f', 4)
            .arg(snapshot.evaluationMetrics.value(QStringLiteral("twin_confidence_score")).toDouble(), 0, 'f', 4)
            .arg(snapshot.evaluationMetrics.value(QStringLiteral("local_risk_score")).toDouble(), 0, 'f', 4)
            .arg(snapshot.evaluationMetrics.value(QStringLiteral("target_region_distance_mm")).toDouble(), 0, 'f', 4)
            .arg(snapshot.evaluationMetrics.value(QStringLiteral("target_region_angle_error_deg")).toDouble(), 0, 'f', 4)
            .arg(snapshot.evaluationMetrics.value(QStringLiteral("dominant_risk_source")).toString())
            .arg(snapshot.evaluationMetrics.value(QStringLiteral("re_register_recommended")).toBool() ? QStringLiteral("true") : QStringLiteral("false"))
            .arg(snapshot.evaluationMetrics.value(QStringLiteral("tracking_degradation_detected")).toBool() ? QStringLiteral("true") : QStringLiteral("false")));
    }

    file.write(lines.join(QStringLiteral("\n")).toUtf8());
    return file.error() == QFile::NoError;
}

QString NavigationEvaluationService::caseRoot(const QString& caseId) const
{
    return m_casesRoot + QStringLiteral("/") + caseId;
}

QString NavigationEvaluationService::evaluationRoot(const QString& caseId) const
{
    return caseRoot(caseId) + QStringLiteral("/evaluation");
}

QString NavigationEvaluationService::registrationPath(const QString& caseId) const
{
    return caseRoot(caseId) + QStringLiteral("/registration/registration_result.json");
}

QString NavigationEvaluationService::navigationPath(const QString& caseId) const
{
    return caseRoot(caseId) + QStringLiteral("/navigation/navigation_run.json");
}

QString NavigationEvaluationService::evaluationPath(const QString& caseId) const
{
    return evaluationRoot(caseId) + QStringLiteral("/evaluation_report.json");
}

QString NavigationEvaluationService::metricsCsvPath(const QString& caseId) const
{
    return evaluationRoot(caseId) + QStringLiteral("/evaluation_metrics.csv");
}

QString NavigationEvaluationService::caseSummaryPath(const QString& caseId) const
{
    return evaluationRoot(caseId) + QStringLiteral("/case_evaluation_summary.json");
}

QString NavigationEvaluationService::batchSummaryCsvPath() const
{
    return m_casesRoot + QStringLiteral("/summaries/evaluation_case_summaries.csv");
}
