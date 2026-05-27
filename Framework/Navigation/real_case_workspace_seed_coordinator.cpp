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
    const QString bindingsPath = repository.caseAssetBindingsPath(seed.caseId);
    const QString modelsRoot = repository.stagePath(seed.caseId, QStringLiteral("models"));
    const QString tibiaPath = modelsRoot + QStringLiteral("/tibia.stl");
    const QString talusPath = modelsRoot + QStringLiteral("/talus.stl");
    const QString instrumentModelPath = seed.primaryInstrumentModelPath.isEmpty()
        ? QString()
        : repository.stagePath(seed.caseId, QStringLiteral("instruments"))
            + QStringLiteral("/")
            + QFileInfo(seed.primaryInstrumentModelPath).fileName();
    const QString instrumentGeometryPath = seed.primaryInstrumentGeometryFilePath.isEmpty()
        ? QString()
        : repository.stagePath(seed.caseId, QStringLiteral("geometry"))
            + QStringLiteral("/")
            + QFileInfo(seed.primaryInstrumentGeometryFilePath).fileName();
    const bool instrumentAssetsReady =
        (seed.primaryInstrumentModelPath.isEmpty() || QFileInfo::exists(instrumentModelPath))
        && (seed.primaryInstrumentGeometryFilePath.isEmpty() || QFileInfo::exists(instrumentGeometryPath));

    if (QFileInfo::exists(manifestPath) &&
        QFileInfo::exists(bindingsPath) &&
        QFileInfo::exists(tibiaPath) &&
        QFileInfo::exists(talusPath) &&
        instrumentAssetsReady) {
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
    request.primaryInstrumentAssetId = seed.primaryInstrumentAssetId.isEmpty()
        ? QStringLiteral("instrument:probe-main")
        : seed.primaryInstrumentAssetId;
    request.primaryInstrumentDisplayName = seed.primaryInstrumentDisplayName.isEmpty()
        ? QStringLiteral("主探针")
        : seed.primaryInstrumentDisplayName;
    request.primaryInstrumentModelPath = seed.primaryInstrumentModelPath;
    request.primaryInstrumentTrackingMarkerId = seed.primaryInstrumentTrackingMarkerId;
    request.primaryInstrumentGeometryFilePath = seed.primaryInstrumentGeometryFilePath;
    request.primaryInstrumentGeometryAssetId = seed.primaryInstrumentGeometryAssetId.isEmpty()
        ? QStringLiteral("geometry:probe-main")
        : seed.primaryInstrumentGeometryAssetId;
    request.defaultInstrumentAssetIds = QStringList { request.primaryInstrumentAssetId };
    request.defaultInstrumentGeometryBindings = {
        AnkleInstrumentGeometryBinding {
            request.primaryInstrumentAssetId,
            request.primaryInstrumentGeometryAssetId,
            seed.primaryInstrumentGeometryFilePath.isEmpty()
                ? QStringLiteral("geometry/probe-main.ini")
                : QStringLiteral("geometry/%1").arg(QFileInfo(seed.primaryInstrumentGeometryFilePath).fileName())
        }
    };
    request.targetRegionCenter = seed.targetRegionCenter;
    request.targetRegionRadiusMm = seed.targetRegionRadiusMm;

    RealCaseAssetBootstrapper bootstrapper;
    return bootstrapper.bootstrap(request);
}
