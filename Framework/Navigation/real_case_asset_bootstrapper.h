#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Navigation/ankle_navigation_types.h"

#include <QString>
#include <QStringList>
#include <QVector3D>

struct RealCaseAssetBootstrapRequest
{
    QString dataRoot;
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
    QStringList defaultInstrumentAssetIds;
    QList<AnkleInstrumentGeometryBinding> defaultInstrumentGeometryBindings;
    QVector3D targetRegionCenter;
    double targetRegionRadiusMm = 0.0;
};

class FRAMEWORK_EXPORT RealCaseAssetBootstrapper
{
public:
    bool bootstrap(const RealCaseAssetBootstrapRequest& request) const;
};
