#include "threshold_only_gate_strategy.h"

namespace
{
QString navigationBlockedText()
{
    return QString::fromUtf8("\u7981\u6b62\u8fdb\u5165\u5bfc\u822a");
}

QString navigationAllowedText()
{
    return QString::fromUtf8("\u53ef\u8fdb\u5165\u5bfc\u822a");
}
}

QString ThresholdOnlyGateStrategy::id() const
{
    return QStringLiteral("threshold_only");
}

NavigationConfidenceResult ThresholdOnlyGateStrategy::evaluate(const NavigationConfidenceInputs& inputs) const
{
    NavigationConfidenceResult result;
    result.allowNavigation = inputs.fre <= 2.0 && inputs.targetTre <= 3.0;
    result.score = result.allowNavigation ? 1.0 : 0.0;
    result.recommendations.append(result.allowNavigation ? navigationAllowedText() : navigationBlockedText());
    return result;
}
