#pragma once

#include "registration_point_selection_strategy.h"

class ExpertRulePointSelectionStrategy : public RegistrationPointSelectionStrategy
{
public:
    QString id() const override;
    QList<RecommendedRegistrationPoint> select(
        const TargetRegistrationRegion& region,
        const QList<CandidateRegistrationPoint>& candidates,
        const QList<QVector3D>& alreadySelected) const override;
};
