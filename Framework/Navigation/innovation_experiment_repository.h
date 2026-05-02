#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Navigation/innovation_experiment_types.h"

class FRAMEWORK_EXPORT InnovationExperimentRepository
{
public:
    explicit InnovationExperimentRepository(const QString& casesRoot);

    bool saveRecord(const InnovationExperimentRecord& record) const;
    QString recordPath(const InnovationExperimentRecord& record) const;

private:
    QString caseEvaluationRoot(const QString& caseId) const;

    QString m_casesRoot;
};
