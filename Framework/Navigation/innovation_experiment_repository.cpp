#include "Framework/Navigation/innovation_experiment_repository.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

namespace
{
QJsonObject toJson(const InnovationPerturbationProfile& perturbation)
{
    QJsonObject object;
    object.insert(QStringLiteral("noise_profile"), perturbation.noiseProfile);
    object.insert(QStringLiteral("tracking_profile"), perturbation.trackingProfile);
    object.insert(QStringLiteral("point_budget"), perturbation.pointBudget);
    return object;
}

QJsonObject toJson(const InnovationExperimentRecord& record)
{
    QJsonObject object;
    object.insert(QStringLiteral("case_id"), record.caseId);
    object.insert(QStringLiteral("innovation_id"), record.innovationId);
    object.insert(QStringLiteral("strategy_id"), record.strategyId);
    object.insert(QStringLiteral("run_index"), record.runIndex);
    object.insert(QStringLiteral("perturbation"), toJson(record.perturbation));
    object.insert(QStringLiteral("metrics"), QJsonObject::fromVariantMap(record.metrics));
    return object;
}
}

InnovationExperimentRepository::InnovationExperimentRepository(const QString& casesRoot)
    : m_casesRoot(casesRoot)
{
}

bool InnovationExperimentRepository::saveRecord(const InnovationExperimentRecord& record) const
{
    const QString path = recordPath(record);
    QDir dir;
    if (!dir.mkpath(QFileInfo(path).absolutePath())) {
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    file.write(QJsonDocument(toJson(record)).toJson(QJsonDocument::Indented));
    return file.error() == QFile::NoError;
}

QString InnovationExperimentRepository::recordPath(const InnovationExperimentRecord& record) const
{
    return caseEvaluationRoot(record.caseId)
        + QStringLiteral("/experiments/")
        + record.innovationId
        + QStringLiteral("/")
        + record.strategyId
        + QStringLiteral("_run_")
        + QStringLiteral("%1").arg(record.runIndex, 3, 10, QLatin1Char('0'))
        + QStringLiteral(".json");
}

QString InnovationExperimentRepository::caseEvaluationRoot(const QString& caseId) const
{
    return m_casesRoot + QStringLiteral("/") + caseId + QStringLiteral("/evaluation");
}
