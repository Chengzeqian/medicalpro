#include "Framework/Navigation/navigation_confidence_evaluator.h"

#include <QtGlobal>

namespace
{
QString collectMorePointsText()
{
    return QString::fromUtf8("\u8865\u91c7\u70b9");
}

QString recheckTargetRegionText()
{
    return QString::fromUtf8("\u91cd\u65b0\u68c0\u67e5\u76ee\u6807\u533a\u914d\u51c6");
}

QString reviewSurfaceResidualText()
{
    return QString::fromUtf8("\u590d\u6838\u8868\u9762\u6b8b\u5dee");
}

QString checkTrackingVisibilityText()
{
    return QString::fromUtf8("\u68c0\u67e5\u8ffd\u8e2a\u5668\u89c6\u91ce\u548c\u906e\u6321");
}

QString navigationAllowedText()
{
    return QString::fromUtf8("\u53ef\u8fdb\u5165\u5bfc\u822a");
}

double clamp01(double value)
{
    return qBound(0.0, value, 1.0);
}

double inverseScore(double value, double threshold)
{
    if (threshold <= 0.0) {
        return 0.0;
    }

    return clamp01(1.0 - (value / threshold));
}
}

NavigationConfidenceResult NavigationConfidenceEvaluator::evaluate(const NavigationConfidenceInputs& inputs) const
{
    NavigationConfidenceResult result;

    const double freScore = inverseScore(inputs.fre, 2.0);
    const double treScore = inverseScore(inputs.targetTre, 3.0);
    const double coverageScore = clamp01(inputs.coverageScore);
    const double surfaceResidualScore = inverseScore(inputs.surfaceResidual, 3.0);
    const double trackingJitterScore = inverseScore(inputs.trackingJitter, 2.0);
    const double visibleScore = clamp01(inputs.visibleFrameRatio);

    result.score =
        (freScore * 0.20) +
        (treScore * 0.25) +
        (coverageScore * 0.15) +
        (surfaceResidualScore * 0.15) +
        (trackingJitterScore * 0.15) +
        (visibleScore * 0.10);

    if (inputs.coverageScore < 0.5) {
        result.recommendations.append(collectMorePointsText());
    }
    if (inputs.targetTre > 2.5) {
        result.recommendations.append(recheckTargetRegionText());
    }
    if (inputs.surfaceResidual > 2.0) {
        result.recommendations.append(reviewSurfaceResidualText());
    }
    if (inputs.trackingJitter > 1.0 || inputs.visibleFrameRatio < 0.85) {
        result.recommendations.append(checkTrackingVisibilityText());
    }

    result.allowNavigation =
        result.score >= 0.6 &&
        inputs.targetTre <= 2.5 &&
        inputs.coverageScore >= 0.5 &&
        inputs.trackingJitter <= 1.0 &&
        inputs.visibleFrameRatio >= 0.85;

    if (result.allowNavigation && result.recommendations.isEmpty()) {
        result.recommendations.append(navigationAllowedText());
    }

    return result;
}
