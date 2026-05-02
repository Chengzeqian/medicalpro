#pragma once

#include "Framework/FrameworkExport.h"

#include <QtGlobal>
#include <QStringList>

struct NavigationConfidenceInputs
{
    double fre = 0.0;
    double targetTre = 0.0;
    double coverageScore = 0.0;
    double surfaceResidual = 0.0;
    double trackingJitter = 0.0;
    double visibleFrameRatio = 1.0;
    bool toolCalibrated = false;
    double calibrationAccuracy = 0.0;
};

struct NavigationConfidenceResult
{
    double score = 0.0;
    bool allowNavigation = false;
    QStringList recommendations;
};

namespace NavigationConfidenceDetail
{
inline double clamp01(double value)
{
    return qBound(0.0, value, 1.0);
}

inline double inverseScore(double value, double threshold)
{
    if (threshold <= 0.0) {
        return 0.0;
    }

    return clamp01(1.0 - (value / threshold));
}

inline QString collectMorePointsText()
{
    return QString::fromUtf8("\u8865\u91c7\u70b9");
}

inline QString recheckTargetRegionText()
{
    return QString::fromUtf8("\u91cd\u65b0\u68c0\u67e5\u76ee\u6807\u533a\u914d\u51c6");
}

inline QString reviewSurfaceResidualText()
{
    return QString::fromUtf8("\u590d\u6838\u8868\u9762\u6b8b\u5dee");
}

inline QString checkTrackingVisibilityText()
{
    return QString::fromUtf8("\u68c0\u67e5\u8ffd\u8e2a\u5668\u89c6\u91ce\u548c\u906e\u6321");
}

inline QString navigationAllowedText()
{
    return QString::fromUtf8("\u53ef\u8fdb\u5165\u5bfc\u822a");
}

inline QString calibrateProbeFirstText()
{
    return QString::fromUtf8("\u5148\u5b8c\u6210\u63a2\u9488\u6807\u5b9a");
}

inline QString recalibrateProbeText()
{
    return QString::fromUtf8("\u91cd\u65b0\u6267\u884c\u63a2\u9488\u6807\u5b9a");
}
}

inline NavigationConfidenceResult evaluateNavigationConfidence(const NavigationConfidenceInputs& inputs)
{
    NavigationConfidenceResult result;

    const double freScore = NavigationConfidenceDetail::inverseScore(inputs.fre, 2.0);
    const double treScore = NavigationConfidenceDetail::inverseScore(inputs.targetTre, 3.0);
    const double coverageScore = NavigationConfidenceDetail::clamp01(inputs.coverageScore);
    const double surfaceResidualScore = NavigationConfidenceDetail::inverseScore(inputs.surfaceResidual, 3.0);
    const double trackingJitterScore = NavigationConfidenceDetail::inverseScore(inputs.trackingJitter, 2.0);
    const double visibleScore = NavigationConfidenceDetail::clamp01(inputs.visibleFrameRatio);
    const double calibrationScore = inputs.toolCalibrated
        ? NavigationConfidenceDetail::inverseScore(inputs.calibrationAccuracy, 2.0)
        : 0.0;

    result.score =
        (freScore * 0.18) +
        (treScore * 0.22) +
        (coverageScore * 0.15) +
        (surfaceResidualScore * 0.12) +
        (trackingJitterScore * 0.13) +
        (visibleScore * 0.08) +
        (calibrationScore * 0.12);

    if (inputs.coverageScore < 0.5) {
        result.recommendations.append(NavigationConfidenceDetail::collectMorePointsText());
    }
    if (inputs.targetTre > 2.5) {
        result.recommendations.append(NavigationConfidenceDetail::recheckTargetRegionText());
    }
    if (inputs.surfaceResidual > 2.0) {
        result.recommendations.append(NavigationConfidenceDetail::reviewSurfaceResidualText());
    }
    if (inputs.trackingJitter > 1.0 || inputs.visibleFrameRatio < 0.85) {
        result.recommendations.append(NavigationConfidenceDetail::checkTrackingVisibilityText());
    }
    if (!inputs.toolCalibrated) {
        result.recommendations.append(NavigationConfidenceDetail::calibrateProbeFirstText());
    } else if (inputs.calibrationAccuracy > 1.5) {
        result.recommendations.append(NavigationConfidenceDetail::recalibrateProbeText());
    }

    result.allowNavigation =
        result.score >= 0.6 &&
        inputs.targetTre <= 2.5 &&
        inputs.coverageScore >= 0.5 &&
        inputs.trackingJitter <= 1.0 &&
        inputs.visibleFrameRatio >= 0.85 &&
        inputs.toolCalibrated &&
        inputs.calibrationAccuracy <= 1.5;

    if (result.allowNavigation && result.recommendations.isEmpty()) {
        result.recommendations.append(NavigationConfidenceDetail::navigationAllowedText());
    }

    return result;
}

class FRAMEWORK_EXPORT NavigationConfidenceEvaluator
{
public:
    NavigationConfidenceResult evaluate(const NavigationConfidenceInputs& inputs) const
    {
        return evaluateNavigationConfidence(inputs);
    }
};
