#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Navigation/ankle_navigation_types.h"

#include <QJsonObject>
#include <QString>

class FRAMEWORK_EXPORT AnkleCaseWorkspaceRepository
{
public:
    explicit AnkleCaseWorkspaceRepository(const QString& dataRoot);

    bool createCaseWorkspace(AnkleCaseManifest& manifest) const;
    bool saveManifest(const AnkleCaseManifest& manifest) const;
    AnkleCaseManifest loadManifest(const QString& caseId) const;
    bool saveCaseAssetBindings(const AnkleCaseAssetBindings& bindings) const;
    AnkleCaseAssetBindings loadCaseAssetBindings(const QString& caseId) const;

    QString caseRoot(const QString& caseId) const;
    QString manifestPath(const QString& caseId) const;
    QString caseAssetBindingsPath(const QString& caseId) const;
    QString stagePath(const QString& caseId, const QString& stageName) const;

private:
    QJsonObject toJson(const AnkleCaseManifest& manifest) const;
    AnkleCaseManifest fromJson(const QJsonObject& object) const;
    bool ensureDir(const QString& path) const;

    QString m_dataRoot;
};
