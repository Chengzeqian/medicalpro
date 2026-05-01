#pragma once

#include "registration_point_selection_strategy.h"

#include <memory>
#include <vector>

class RegistrationPointStrategyRegistry
{
public:
    RegistrationPointStrategyRegistry();

    QStringList strategyIds() const;
    const RegistrationPointSelectionStrategy* strategy(const QString& id) const;

private:
    std::vector<std::unique_ptr<RegistrationPointSelectionStrategy>> m_strategies;
};
