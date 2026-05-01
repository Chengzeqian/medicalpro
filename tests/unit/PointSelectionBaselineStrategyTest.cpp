#include <QtTest/QtTest>

#include "Plugins/PointRegistration/registration_point_strategy_registry.h"

class PointSelectionBaselineStrategyTest : public QObject
{
    Q_OBJECT

private slots:
    void registry_exposes_target_sensitive_random_uniform_and_expert_rule();
};

void PointSelectionBaselineStrategyTest::registry_exposes_target_sensitive_random_uniform_and_expert_rule()
{
    RegistrationPointStrategyRegistry registry;

    QCOMPARE(registry.strategyIds(), QStringList({
        QStringLiteral("target_sensitive"),
        QStringLiteral("random"),
        QStringLiteral("uniform"),
        QStringLiteral("expert_rule")
    }));
}

QTEST_APPLESS_MAIN(PointSelectionBaselineStrategyTest)
#include "PointSelectionBaselineStrategyTest.moc"
