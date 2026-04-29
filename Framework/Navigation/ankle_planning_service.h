#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Navigation/ankle_case_workspace_repository.h"

#include <QJsonObject>
#include <QVariantMap>

class FRAMEWORK_EXPORT AnklePlanningService
{
public:
    explicit AnklePlanningService(const AnkleCaseWorkspaceRepository& repository);

    AnklePlanningData createDefaultPlanning(const QString& caseId) const;
    bool savePlanning(const QString& caseId, const AnklePlanningData& planning) const;
    AnklePlanningData loadPlanning(const QString& caseId) const;
    QVariantMap buildDashboardReadiness(const QString& caseId) const;

private:
    QString planningPath(const QString& caseId) const;
    QJsonObject toJson(const AnklePlanningData& planning) const;
    AnklePlanningData fromJson(const QJsonObject& object) const;

    AnkleCaseWorkspaceRepository m_repository;
};
