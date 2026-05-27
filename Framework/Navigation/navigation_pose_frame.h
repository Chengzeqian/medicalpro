#pragma once

#include "Framework/FrameworkExport.h"

#include <QDateTime>
#include <QList>
#include <QMatrix4x4>
#include <QString>

struct FRAMEWORK_EXPORT NavigationPoseFrame
{
    QString sourceId;
    QString toolId;
    QDateTime timestamp;
    bool trackingVisible = false;
    double trackingConfidence = 0.0;
    QMatrix4x4 trackingToMarker;
};

struct FRAMEWORK_EXPORT NavigationPoseSampleWindow
{
    QList<NavigationPoseFrame> recentFrames;
    int maxFrameCount = 0;
};
