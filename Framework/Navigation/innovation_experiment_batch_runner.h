#pragma once

#include "Framework/FrameworkExport.h"

#include <QStringList>

struct InnovationBatchInput
{
    QStringList caseIds;
    QString caseDataRoot;
};

struct InnovationBatchOutput
{
    QStringList summaryFiles;
};

class FRAMEWORK_EXPORT InnovationExperimentBatchRunner
{
public:
    InnovationBatchOutput run(const InnovationBatchInput& input) const;
};
