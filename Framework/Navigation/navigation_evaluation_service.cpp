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
    return object;
}

QJsonObject toJson(const AnkleEvaluationReport& report)
{
    QJsonObject object;
    object.insert(QStringLiteral("case_id"), report.caseId);
    object.insert(QStringLiteral("translation_error_mm"), report.translationErrorMm);
    object.insert(QStringLiteral("rotation_error_deg"), report.rotationErrorDeg);
    object.insert(QStringLiteral("allow_navigation"), report.allowNavigation);
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

    QDir dir;
    if (!dir.mkpath(QFileInfo(metricsCsvPath(caseId)).absolutePath())) {
        return false;
    }

    QFile file(metricsCsvPath(caseId));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }

    const QStringList lines = {
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
        QStringLiteral("allow_navigation,%1").arg(evaluation.value(QStringLiteral("allow_navigation")).toBool() ? QStringLiteral("true") : QStringLiteral("false"))
    };

    file.write(lines.join(QStringLiteral("\n")).toUtf8());
    return file.error() == QFile::NoError;
}

QString NavigationEvaluationService::caseRoot(const QString& caseId) const
{
    return m_casesRoot + QStringLiteral("/") + caseId;
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
    return caseRoot(caseId) + QStringLiteral("/evaluation/evaluation_report.json");
}

QString NavigationEvaluationService::metricsCsvPath(const QString& caseId) const
{
    return caseRoot(caseId) + QStringLiteral("/evaluation/evaluation_metrics.csv");
}
