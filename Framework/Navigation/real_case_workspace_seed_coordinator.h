#pragma once

#include "Framework/FrameworkExport.h"

#include <QString>
#include <QVector3D>

struct RealCaseWorkspaceSeed
{
    bool enabled = false;
    QString caseId;
    QString patientId;
    QString patientName;
    QString surgeryId;
    QString tibiaModelPath;
    QString talusModelPath;
    QString primaryInstrumentAssetId;
    QString primaryInstrumentDisplayName;
    QString primaryInstrumentModelPath;
    QString primaryInstrumentTrackingMarkerId;
    QString primaryInstrumentGeometryFilePath;
    QString primaryInstrumentGeometryAssetId;
    QVector3D targetRegionCenter;
    double targetRegionRadiusMm = 0.0;
};

class FRAMEWORK_EXPORT RealCaseWorkspaceSeedCoordinator
{
public:
    bool ensureWorkspace(const RealCaseWorkspaceSeed& seed, const QString& casesRoot) const;
};
