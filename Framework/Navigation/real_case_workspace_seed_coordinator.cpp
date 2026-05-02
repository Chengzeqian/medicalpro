#include "Framework/Navigation/real_case_workspace_seed_coordinator.h"

#include "Framework/Navigation/ankle_case_workspace_repository.h"
#include "Framework/Navigation/real_case_asset_bootstrapper.h"

#include <QDir>
#include <QFileInfo>

bool RealCaseWorkspaceSeedCoordinator::ensureWorkspace(
    const RealCaseWorkspaceSeed& seed,
    const QString& casesRoot) const
{
    if (!seed.enabled) {
        return true;
    }

    if (seed.caseId.isEmpty() || casesRoot.isEmpty()) {
        return false;
    }

    const QString dataRoot = QFileInfo(casesRoot).dir().absolutePath();
    AnkleCaseWorkspaceRepository repository(dataRoot);

    const QString manifestPath = repository.manifestPath(seed.caseId);
    const QString modelsRoot = repository.stagePath(seed.caseId, QStringLiteral("models"));
    const QString tibiaPath = modelsRoot + QStringLiteral("/tibia.stl");
    const QString talusPath = modelsRoot + QStringLiteral("/talus.stl");

    if (QFileInfo::exists(manifestPath) &&
        QFileInfo::exists(tibiaPath) &&
        QFileInfo::exists(talusPath)) {
        return true;
    }

    RealCaseAssetBootstrapRequest request;
    request.dataRoot = dataRoot;
    request.caseId = seed.caseId;
    request.patientId = seed.patientId;
    request.patientName = seed.patientName;
    request.surgeryId = seed.surgeryId;
    request.tibiaModelPath = seed.tibiaModelPath;
    request.talusModelPath = seed.talusModelPath;
    request.targetRegionCenter = seed.targetRegionCenter;
    request.targetRegionRadiusMm = seed.targetRegionRadiusMm;

    RealCaseAssetBootstrapper bootstrapper;
    return bootstrapper.bootstrap(request);
}
