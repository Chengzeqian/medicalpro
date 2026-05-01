#include "UI/NewPages/Navigation/navigation_workflow_context.h"

#include <QDir>

void NavigationWorkflowContext::clearCaseIdentity()
{
    m_caseId.clear();
    m_patientId = -1;
    m_patientName.clear();
}

void NavigationWorkflowContext::setCaseIdentity(const QString& caseId, int patientId, const QString& patientName)
{
    m_caseId = caseId;
    m_patientId = patientId;
    m_patientName = patientName;
}

void NavigationWorkflowContext::setPatientId(int patientId)
{
    m_patientId = patientId;
}

void NavigationWorkflowContext::setPatientName(const QString& patientName)
{
    m_patientName = patientName;
}

void NavigationWorkflowContext::setCurrentStage(AnkleWorkflowStage stage)
{
    m_currentStage = stage;
}

void NavigationWorkflowContext::setCasesRoot(const QString& root)
{
    m_casesRoot = root;
}

QString NavigationWorkflowContext::caseId() const
{
    return m_caseId;
}

int NavigationWorkflowContext::patientId() const
{
    return m_patientId;
}

QString NavigationWorkflowContext::patientName() const
{
    return m_patientName;
}

AnkleWorkflowStage NavigationWorkflowContext::currentStage() const
{
    return m_currentStage;
}

QString NavigationWorkflowContext::casesRoot() const
{
    return m_casesRoot;
}

QString NavigationWorkflowContext::caseRoot() const
{
    if (m_casesRoot.isEmpty() || m_caseId.isEmpty()) {
        return {};
    }

    return QDir(m_casesRoot).filePath(m_caseId);
}

QString NavigationWorkflowContext::patientSummary() const
{
    if (m_caseId.isEmpty() && m_patientName.isEmpty()) {
        return QStringLiteral("患者：-");
    }

    if (m_caseId.isEmpty()) {
        return QStringLiteral("患者：%1").arg(m_patientName);
    }

    if (m_patientName.isEmpty()) {
        return QStringLiteral("病例：%1").arg(m_caseId);
    }

    return QStringLiteral("病例：%1 | 患者：%2").arg(m_caseId, m_patientName);
}
