#include "registration_point_strategy_registry.h"

#include "expert_rule_point_selection_strategy.h"
#include "random_point_selection_strategy.h"
#include "uniform_point_selection_strategy.h"

namespace
{
class TargetSensitivePointSelectionStrategy : public RegistrationPointSelectionStrategy
{
public:
    QString id() const override
    {
        return QStringLiteral("target_sensitive");
    }

    QList<RecommendedRegistrationPoint> select(
        const TargetRegistrationRegion& region,
        const QList<CandidateRegistrationPoint>& candidates,
        const QList<QVector3D>& alreadySelected) const override
    {
        return m_selector.rankCandidates(region, candidates, alreadySelected);
    }

private:
    TargetSensitivePointSelector m_selector;
};
}

RegistrationPointStrategyRegistry::RegistrationPointStrategyRegistry()
{
    m_strategies.emplace_back(std::make_unique<TargetSensitivePointSelectionStrategy>());
    m_strategies.emplace_back(std::make_unique<RandomPointSelectionStrategy>());
    m_strategies.emplace_back(std::make_unique<UniformPointSelectionStrategy>());
    m_strategies.emplace_back(std::make_unique<ExpertRulePointSelectionStrategy>());
}

QStringList RegistrationPointStrategyRegistry::strategyIds() const
{
    QStringList ids;
    ids.reserve(static_cast<qsizetype>(m_strategies.size()));
    for (const auto& strategy : m_strategies) {
        ids.append(strategy->id());
    }
    return ids;
}

const RegistrationPointSelectionStrategy* RegistrationPointStrategyRegistry::strategy(const QString& id) const
{
    for (const auto& strategy : m_strategies) {
        if (strategy->id() == id) {
            return strategy.get();
        }
    }
    return nullptr;
}
