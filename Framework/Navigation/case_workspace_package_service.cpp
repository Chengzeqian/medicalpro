#include "Framework/Navigation/case_workspace_package_service.h"

CaseWorkspacePackageService::CaseWorkspacePackageService(const QString& dataRoot)
    : m_repository(dataRoot)
{
}

CaseWorkspacePackageSummary CaseWorkspacePackageService::loadSummary(const QString& caseId) const
{
    if (caseId.isEmpty()) {
        return {};
    }

    const AnkleCaseManifest manifest = m_repository.loadManifest(caseId);
    const AnkleCaseAssetBindings bindings = m_repository.loadCaseAssetBindings(caseId);

    CaseWorkspacePackageSummary summary;
    summary.caseId = caseId;
    summary.boundBoneCount = bindings.boundBoneAssetIds.size();
    summary.activeBoneCount = bindings.activeBoneAssetIds.size();
    summary.boundInstrumentCount = bindings.boundInstrumentAssetIds.size();
    summary.geometryBindingCount = bindings.instrumentGeometryBindings.size();
    summary.readyForNavigation = !manifest.caseId.isEmpty()
        && summary.boundBoneCount > 0
        && summary.boundInstrumentCount > 0
        && summary.geometryBindingCount > 0;
    return summary;
}
