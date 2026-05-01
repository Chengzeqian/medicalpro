#pragma once

#include "Framework/Navigation/navigation_confidence_evaluator.h"

#include <QString>

class NavigationGateStrategy
{
public:
    virtual ~NavigationGateStrategy() = default;

    virtual QString id() const = 0;
    virtual NavigationConfidenceResult evaluate(const NavigationConfidenceInputs& inputs) const = 0;
};
