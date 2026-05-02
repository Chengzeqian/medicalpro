#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Navigation/innovation_experiment_types.h"

#include <QStringList>

struct Innovation3GateInput
{
    QString caseId;
    QStringList gateStrategyIds;
};

class FRAMEWORK_EXPORT Innovation3GateExperiment
{
public:
    QList<InnovationExperimentRecord> run(const Innovation3GateInput& input) const;
};
