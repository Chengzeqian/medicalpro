#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Navigation/ankle_navigation_types.h"

class FRAMEWORK_EXPORT NavigationEvaluationService
{
public:
    explicit NavigationEvaluationService(const QString& casesRoot);

    bool saveRegistrationRecord(const AnkleRegistrationRecord& record) const;
    bool saveNavigationRun(const AnkleNavigationRunRecord& record) const;
    bool saveEvaluationReport(const AnkleEvaluationReport& report) const;
    bool exportMetricsCsv(const QString& caseId) const;

private:
    QString caseRoot(const QString& caseId) const;
    QString evaluationRoot(const QString& caseId) const;
    QString registrationPath(const QString& caseId) const;
    QString navigationPath(const QString& caseId) const;
    QString evaluationPath(const QString& caseId) const;
    QString metricsCsvPath(const QString& caseId) const;

    QString m_casesRoot;
};
