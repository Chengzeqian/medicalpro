#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Navigation/innovation_experiment_types.h"

#include <QStringList>

struct Innovation2RegistrationInput
{
    QString caseId;
    QStringList registrationMethodIds;
};

class FRAMEWORK_EXPORT Innovation2RegistrationExperiment
{
public:
    QList<InnovationExperimentRecord> run(const Innovation2RegistrationInput& input) const;
};
