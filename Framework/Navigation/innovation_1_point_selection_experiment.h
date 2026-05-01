#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Navigation/innovation_experiment_types.h"

#include <QStringList>

struct Innovation1PointSelectionInput
{
    QString caseId;
    QStringList strategyIds;
    int pointBudget = 0;
};

class FRAMEWORK_EXPORT Innovation1PointSelectionExperiment
{
public:
    QList<InnovationExperimentRecord> run(const Innovation1PointSelectionInput& input) const;
};
