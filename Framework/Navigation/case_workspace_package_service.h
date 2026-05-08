#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Navigation/ankle_case_workspace_repository.h"

#include <QString>

struct CaseWorkspacePackageSummary
{
    QString caseId;
    int boundBoneCount = 0;
    int activeBoneCount = 0;
    int boundInstrumentCount = 0;
    int geometryBindingCount = 0;
    bool readyForNavigation = false;
};

class FRAMEWORK_EXPORT CaseWorkspacePackageService
{
public:
    explicit CaseWorkspacePackageService(const QString& dataRoot);

    CaseWorkspacePackageSummary loadSummary(const QString& caseId) const;

private:
    AnkleCaseWorkspaceRepository m_repository;
};
