#include <QtTest/QtTest>

#include <QFile>
#include <QString>

class AnkleNavigationWorkflowContractTest : public QObject
{
    Q_OBJECT

private slots:
    void workflow_pages_thread_case_id_and_stage_structure();
    void navigation_page_tracker_connection_bridges_selected_instrument_tracking_configuration();
    void navigation_page_calibration_button_runs_active_tool_probe_calibration();
    void navigation_page_exposes_stepwise_probe_calibration_flow();

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
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));
    const QString mainInterfaceSource = readFile(QStringLiteral("UI/MainInterfaceWidget.cpp"));

    QVERIFY2(managementHeader.contains(QStringLiteral("enterCaseWorkspaceRequested")),
        "management page must emit case workspace signal");
    QVERIFY2(dashboardHeader.contains(QStringLiteral("setCurrentCaseId")),
        "dashboard must accept current case id");
    QVERIFY2(navigationHeader.contains(QStringLiteral("setCaseContext")),
        "navigation page must accept full case context");
    QVERIFY2(navigationHeader.contains(QStringLiteral("enum class AnkleWorkflowStage")),
        "navigation page must declare workflow stages");
    QVERIFY2(!navigationSource.contains(QStringLiteral("fourViewLayout->addWidget(m_navigation3DView")),
        "navigation page must not directly embed Navigation3DViewWidget into fourViewLayout");
    QVERIFY2(navigationSource.contains(QStringLiteral("showNavigationContent(m_navigation3DView)")),
        "navigation page must route navigation 3D view placement through NavigationVtkBridge");
    QVERIFY2(mainInterfaceSource.contains(QStringLiteral("enterCaseWorkspaceRequested")),
        "main interface must wire management -> dashboard case signal");
}

void AnkleNavigationWorkflowContractTest::navigation_page_tracker_connection_bridges_selected_instrument_tracking_configuration()
{
    const QString navigationHeader = readFile(QStringLiteral("UI/NewPages/NavigationPage.h"));
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));

    QVERIFY2(navigationHeader.contains(QStringLiteral("int m_selectedInstrumentId")),
        "navigation page must keep the selected instrument id for tracking setup");
    QVERIFY2(navigationHeader.contains(QStringLiteral("QString m_trackingSessionId")),
        "navigation page must keep the active tracking session id");
    QVERIFY2(navigationHeader.contains(QStringLiteral("QString m_navigationToolId")),
        "navigation page must keep the active navigation tool id");

    QVERIFY2(navigationSource.contains(QStringLiteral("m_selectedInstrumentId = instrumentId")),
        "instrument click handler must persist the selected instrument id");
    QVERIFY2(navigationSource.contains(QStringLiteral("scanAvailableDevices()")),
        "tracker connection must scan available devices through OpticalTrackingService");
    QVERIFY2(navigationSource.contains(QStringLiteral("createTrackingSession(")),
        "tracker connection must create a tracking session");
    QVERIFY2(navigationSource.contains(QStringLiteral("addTrackingTool(")),
        "tracker connection must register a tracking tool");
    QVERIFY2(navigationSource.contains(QStringLiteral("trackingMarkerId")),
        "tracker connection must bridge instrument trackingMarkerId into tool config");
    QVERIFY2(navigationSource.contains(QStringLiteral("geometryFilePath")),
        "tracker connection must bridge instrument geometryFilePath into tool config");
    QVERIFY2(navigationSource.contains(QStringLiteral("geometryFile")),
        "tracker connection must populate tool geometryFile for ProbeCalibration");
    QVERIFY2(navigationSource.contains(QStringLiteral("geometryId")),
        "tracker connection must populate tool geometryId for ProbeCalibration");
    QVERIFY2(!navigationSource.contains(QStringLiteral("checkTrackingQuality(QString(), QString())")),
        "navigation start must query tracking quality with the active session/tool instead of empty ids");
}

void AnkleNavigationWorkflowContractTest::navigation_page_calibration_button_runs_active_tool_probe_calibration()
{
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));

    QVERIFY2(navigationSource.contains(QStringLiteral("startToolCalibration(m_trackingSessionId, m_navigationToolId, QStringLiteral(\"pivot\"))")),
        "calibrate button must start pivot calibration for the active tracking session/tool");
    QVERIFY2(navigationSource.contains(QStringLiteral("getCalibrationStatus(calibrationId)")),
        "calibrate button must inspect calibration requirements/status");
    QVERIFY2(navigationSource.contains(QStringLiteral("addCalibrationPoint(calibrationId)")),
        "calibrate button must collect calibration samples");
    QVERIFY2(navigationSource.contains(QStringLiteral("finishCalibration(calibrationId)")),
        "calibrate button must finish the active calibration session");
    QVERIFY2(navigationSource.contains(QStringLiteral("applyCalibrationResult(m_trackingSessionId, m_navigationToolId, calibrationResult)")),
        "calibrate button must apply the calibration result back to the active tool");
}

void AnkleNavigationWorkflowContractTest::navigation_page_exposes_stepwise_probe_calibration_flow()
{
    const QString navigationHeader = readFile(QStringLiteral("UI/NewPages/NavigationPage.h"));
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));
    const QString navigationUi = readFile(QStringLiteral("UI/Forms/NavigationPage.ui"));

    QVERIFY2(navigationHeader.contains(QStringLiteral("QString m_activeCalibrationId")),
        "navigation page must keep the active calibration id");
    QVERIFY2(navigationHeader.contains(QStringLiteral("int m_activeCalibrationRequiredPoints")),
        "navigation page must keep the active calibration required point count");
    QVERIFY2(navigationHeader.contains(QStringLiteral("int m_activeCalibrationCollectedPoints")),
        "navigation page must keep the active calibration collected point count");
    QVERIFY2(navigationHeader.contains(QStringLiteral("void startProbeCalibration();")),
        "navigation page must expose startProbeCalibration()");
    QVERIFY2(navigationHeader.contains(QStringLiteral("void captureProbeCalibrationPoint();")),
        "navigation page must expose captureProbeCalibrationPoint()");
    QVERIFY2(navigationHeader.contains(QStringLiteral("void finishProbeCalibration();")),
        "navigation page must expose finishProbeCalibration()");
    QVERIFY2(navigationHeader.contains(QStringLiteral("void cancelProbeCalibration();")),
        "navigation page must expose cancelProbeCalibration()");
    QVERIFY2(navigationHeader.contains(QStringLiteral("void updateProbeCalibrationUi();")),
        "navigation page must expose updateProbeCalibrationUi()");

    QVERIFY2(navigationSource.contains(QStringLiteral("startProbeCalibration();")),
        "calibrate button must hand off to startProbeCalibration()");
    QVERIFY2(navigationSource.contains(QStringLiteral("captureProbeCalibrationPoint();")),
        "page must support explicit calibration point capture");
    QVERIFY2(navigationSource.contains(QStringLiteral("finishProbeCalibration();")),
        "page must support explicit calibration completion");
    QVERIFY2(navigationSource.contains(QStringLiteral("cancelProbeCalibration();")),
        "page must support calibration cancellation");
    QVERIFY2(navigationSource.contains(QStringLiteral("updateProbeCalibrationUi();")),
        "page must update probe calibration UI state");

    QVERIFY2(navigationUi.contains(QStringLiteral("captureCalibrationPointButton")),
        "navigation calibration UI must expose a capture point button");
    QVERIFY2(navigationUi.contains(QStringLiteral("finishCalibrationButton")),
        "navigation calibration UI must expose a finish calibration button");
    QVERIFY2(navigationUi.contains(QStringLiteral("cancelCalibrationButton")),
        "navigation calibration UI must expose a cancel calibration button");
    QVERIFY2(navigationUi.contains(QStringLiteral("calibrationStatusLabel")),
        "navigation calibration UI must expose calibration status text");
}

QTEST_APPLESS_MAIN(AnkleNavigationWorkflowContractTest)
#include "AnkleNavigationWorkflowContractTest.moc"
