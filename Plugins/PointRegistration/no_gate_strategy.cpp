#include "no_gate_strategy.h"

QString NoGateStrategy::id() const
{
    return QStringLiteral("no_gate");
}

NavigationConfidenceResult NoGateStrategy::evaluate(const NavigationConfidenceInputs&) const
{
    NavigationConfidenceResult result;
    result.score = 1.0;
    result.allowNavigation = true;
    result.recommendations = QStringList({ QStringLiteral("no_gate") });
    return result;
}
