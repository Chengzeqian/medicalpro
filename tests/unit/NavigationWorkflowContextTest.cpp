#include <QtTest/QtTest>

#include "UI/NewPages/Navigation/navigation_workflow_context.h"

class NavigationWorkflowContextTest : public QObject
{
    Q_OBJECT

private slots:
    void context_tracks_case_identity_stage_and_workspace_paths();
};

void NavigationWorkflowContextTest::context_tracks_case_identity_stage_and_workspace_paths()
{
    NavigationWorkflowContext context;
    context.setCaseIdentity(QStringLiteral("ankle-case-101"), 12, QStringLiteral("Patient Z"));
    context.setCurrentStage(AnkleWorkflowStage::Registration);
    context.setCasesRoot(QStringLiteral("D:/Qtproject/medicalpro/data/cases"));

    QCOMPARE(context.caseId(), QStringLiteral("ankle-case-101"));
    QCOMPARE(context.patientId(), 12);
    QCOMPARE(context.patientName(), QStringLiteral("Patient Z"));
    QCOMPARE(context.currentStage(), AnkleWorkflowStage::Registration);
    QCOMPARE(context.caseRoot(), QStringLiteral("D:/Qtproject/medicalpro/data/cases/ankle-case-101"));
}

QTEST_APPLESS_MAIN(NavigationWorkflowContextTest)
#include "NavigationWorkflowContextTest.moc"
