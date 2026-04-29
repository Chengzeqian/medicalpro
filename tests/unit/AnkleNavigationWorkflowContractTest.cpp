#include <QtTest/QtTest>

#include <QFile>
#include <QString>

class AnkleNavigationWorkflowContractTest : public QObject
{
    Q_OBJECT

private slots:
    void workflow_pages_thread_case_id_and_stage_structure();

private:
    QString readFile(const QString& relativePath) const;
};

QString AnkleNavigationWorkflowContractTest::readFile(const QString& relativePath) const
{
    QFile file(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTest::qFail(qPrintable(QStringLiteral("无法读取源文件: %1").arg(relativePath)), __FILE__, __LINE__);
        return {};
    }

    return QString::fromUtf8(file.readAll());
}

void AnkleNavigationWorkflowContractTest::workflow_pages_thread_case_id_and_stage_structure()
{
    const QString managementHeader = readFile(QStringLiteral("UI/NewPages/ManagementPage.h"));
    const QString dashboardHeader = readFile(QStringLiteral("UI/NewPages/DashboardPage.h"));
    const QString navigationHeader = readFile(QStringLiteral("UI/NewPages/NavigationPage.h"));
    const QString mainInterfaceSource = readFile(QStringLiteral("UI/MainInterfaceWidget.cpp"));

    QVERIFY2(managementHeader.contains(QStringLiteral("enterCaseWorkspaceRequested")),
        "management page must emit case workspace signal");
    QVERIFY2(dashboardHeader.contains(QStringLiteral("setCurrentCaseId")),
        "dashboard must accept current case id");
    QVERIFY2(navigationHeader.contains(QStringLiteral("setCaseContext")),
        "navigation page must accept full case context");
    QVERIFY2(navigationHeader.contains(QStringLiteral("enum class AnkleWorkflowStage")),
        "navigation page must declare workflow stages");
    QVERIFY2(mainInterfaceSource.contains(QStringLiteral("enterCaseWorkspaceRequested")),
        "main interface must wire management -> dashboard case signal");
}

QTEST_APPLESS_MAIN(AnkleNavigationWorkflowContractTest)
#include "AnkleNavigationWorkflowContractTest.moc"
