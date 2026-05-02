#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Navigation/innovation_experiment_types.h"

class FRAMEWORK_EXPORT InnovationSummaryCsvExporter
{
public:
    QString defaultFileName(const QString& innovationId) const;
    bool exportRecords(const QString& outputPath, const QList<InnovationExperimentRecord>& records) const;
};
