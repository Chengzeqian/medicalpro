#pragma once

#include "navigation_gate_strategy.h"

class JointConfidenceGateStrategy : public NavigationGateStrategy
{
public:
    QString id() const override;
    NavigationConfidenceResult evaluate(const NavigationConfidenceInputs& inputs) const override;
};
