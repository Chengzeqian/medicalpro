#pragma once

#include "UI/NewPages/NavigationPage.h"

#include <QString>

class NavigationWorkflowContext
{
public:
    void clearCaseIdentity();
    void setCaseIdentity(const QString& caseId, int patientId, const QString& patientName);
    void setPatientId(int patientId);
    void setPatientName(const QString& patientName);
    void setCurrentStage(AnkleWorkflowStage stage);
    void setCasesRoot(const QString& root);

    QString caseId() const;
    int patientId() const;
    QString patientName() const;
    AnkleWorkflowStage currentStage() const;
    QString casesRoot() const;
    QString caseRoot() const;
    QString patientSummary() const;

private:
    QString m_caseId;
    int m_patientId = -1;
    QString m_patientName;
    QString m_casesRoot;
    AnkleWorkflowStage m_currentStage = AnkleWorkflowStage::Preparation;
};
