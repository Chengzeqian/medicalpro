#include "joint_confidence_gate_strategy.h"

QString JointConfidenceGateStrategy::id() const
{
    return QStringLiteral("joint_confidence");
}

NavigationConfidenceResult JointConfidenceGateStrategy::evaluate(const NavigationConfidenceInputs& inputs) const
{
    return evaluateNavigationConfidence(inputs);
}
