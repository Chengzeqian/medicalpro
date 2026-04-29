#pragma once

#include "Framework/FrameworkExport.h"

#include <QStringList>

struct NavigationConfidenceInputs
{
    double fre = 0.0;
    double targetTre = 0.0;
    double coverageScore = 0.0;
    double surfaceResidual = 0.0;
    double trackingJitter = 0.0;
    double visibleFrameRatio = 1.0;
};

struct NavigationConfidenceResult
{
    double score = 0.0;
    bool allowNavigation = false;
    QStringList recommendations;
};

class FRAMEWORK_EXPORT NavigationConfidenceEvaluator
{
public:
    NavigationConfidenceResult evaluate(const NavigationConfidenceInputs& inputs) const;
};
