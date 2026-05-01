#pragma once

#include "target_sensitive_point_selector.h"

class RegistrationPointSelectionStrategy
{
public:
    virtual ~RegistrationPointSelectionStrategy() = default;

    virtual QString id() const = 0;
    virtual QList<RecommendedRegistrationPoint> select(
        const TargetRegistrationRegion& region,
        const QList<CandidateRegistrationPoint>& candidates,
        const QList<QVector3D>& alreadySelected) const = 0;
};
