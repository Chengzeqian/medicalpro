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
    void navigation_page_refreshes_navigation_confidence_after_probe_calibration();
    void navigation_page_persists_probe_calibration_evidence_into_evaluation_report();
    void navigation_page_refreshes_navigation_confidence_after_registration_state_changes();
    void navigation_page_subscribes_runtime_status_changes_for_gate_refresh();
    void navigation_page_enforces_runtime_navigation_gate_while_active();
    void navigation_page_persists_navigation_gate_evidence_into_evaluation_report();
    void navigation_page_exposes_batch_evaluation_summary_export_entry();
    void navigation_page_exports_innovation_summaries_with_data_root_above_cases_directory();
    void navigation_page_uses_data_root_above_cases_directory_for_case_workspace_repository();

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

void AnkleNavigationWorkflowContractTest::navigation_page_refreshes_navigation_confidence_after_probe_calibration()
{
    const QString navigationHeader = readFile(QStringLiteral("UI/NewPages/NavigationPage.h"));
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));

    QVERIFY2(navigationHeader.contains(QStringLiteral("void refreshNavigationConfidenceState(bool showWarnings = false);")),
        "navigation page must expose a shared navigation confidence refresh helper");
    QVERIFY2(navigationHeader.contains(QStringLiteral("bool tryBuildNavigationConfidenceInputs(NavigationConfidenceInputs& inputs) const;")),
        "navigation page must expose a shared navigation confidence input builder");
    QVERIFY2(navigationSource.contains(QStringLiteral("refreshNavigationConfidenceState(true);")),
        "navigation start must evaluate navigation confidence through the shared refresh helper");
    QVERIFY2(navigationSource.contains(QStringLiteral("applyCalibrationResult(m_trackingSessionId, m_navigationToolId, calibrationResult)")),
        "probe calibration completion must still apply calibration result to the active tool");
    QVERIFY2(navigationSource.contains(QStringLiteral("refreshNavigationConfidenceState();")),
        "probe calibration completion must refresh navigation confidence state after applying calibration");
    QVERIFY2(navigationSource.contains(QStringLiteral("inputs.toolCalibrated")),
        "navigation confidence inputs must include probe calibration completion state");
    QVERIFY2(navigationSource.contains(QStringLiteral("inputs.calibrationAccuracy")),
        "navigation confidence inputs must include probe calibration accuracy");
    QVERIFY2(navigationSource.contains(QStringLiteral("navigationReadinessLabel")),
        "navigation page must expose readiness status text");
    QVERIFY2(navigationSource.contains(QStringLiteral("navigationConfidenceLabel")),
        "navigation page must expose confidence score text");
}

void AnkleNavigationWorkflowContractTest::navigation_page_persists_probe_calibration_evidence_into_evaluation_report()
{
    const QString navigationHeader = readFile(QStringLiteral("UI/NewPages/NavigationPage.h"));
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));
    const QString functionStart = QStringLiteral("void NavigationPageNew::finishProbeCalibration()");
    const QString nextFunctionStart = QStringLiteral("void NavigationPageNew::cancelProbeCalibration()");
    const int startIndex = navigationSource.indexOf(functionStart);
    QVERIFY2(startIndex >= 0, "navigation page must define finishProbeCalibration()");

    const int endIndex = navigationSource.indexOf(nextFunctionStart, startIndex);
    QVERIFY2(endIndex > startIndex, "navigation page must keep cancelProbeCalibration() after finishProbeCalibration()");

    const QString body = navigationSource.mid(startIndex, endIndex - startIndex);

    const QString helperStart = QStringLiteral("void NavigationPageNew::persistEvaluationReportSnapshot(const QVariantMap& trackingQuality, bool exportMetricsCsv)");
    const QString helperNextStart = QStringLiteral("void NavigationPageNew::updateProbeCalibrationUi()");
    const int helperStartIndex = navigationSource.indexOf(helperStart);
    QVERIFY2(helperStartIndex >= 0, "navigation page must define persistEvaluationReportSnapshot()");

    const int helperEndIndex = navigationSource.indexOf(helperNextStart, helperStartIndex);
    QVERIFY2(helperEndIndex > helperStartIndex, "navigation page must keep updateProbeCalibrationUi() after persistEvaluationReportSnapshot()");

    const QString helperBody = navigationSource.mid(helperStartIndex, helperEndIndex - helperStartIndex);

    QVERIFY2(navigationHeader.contains(QStringLiteral("void persistEvaluationReportSnapshot(const QVariantMap& trackingQuality = {}, bool exportMetricsCsv = false);")),
        "navigation page must declare a shared evaluation report persistence helper");
    QVERIFY2(helperBody.contains(QStringLiteral("NavigationEvaluationService evaluationService(evaluationCasesRoot());")),
        "shared evaluation report helper must open evaluation service for the active case");
    QVERIFY2(helperBody.contains(QStringLiteral("AnkleEvaluationReport report;")),
        "shared evaluation report helper must construct an evaluation report snapshot");
    QVERIFY2(helperBody.contains(QStringLiteral("report.allowNavigation = m_lastConfidence.allowNavigation;")),
        "shared evaluation report helper must persist the latest navigation gate decision");
    QVERIFY2(helperBody.contains(QStringLiteral("report.confidenceScore = m_lastConfidence.score;")),
        "shared evaluation report helper must persist the latest navigation confidence score");
    QVERIFY2(helperBody.contains(QStringLiteral("report.gateReasons = m_lastConfidence.recommendations;")),
        "shared evaluation report helper must persist latest gate reasons");
    QVERIFY2(helperBody.contains(QStringLiteral("evaluationService.saveEvaluationReport(report);")),
        "shared evaluation report helper must write the updated evaluation report");
    QVERIFY2(helperBody.contains(QStringLiteral("evaluationService.exportCaseSummary(caseId);")),
        "shared evaluation report helper must refresh the persisted case evaluation summary");
    QVERIFY2(helperBody.contains(QStringLiteral("refreshEvaluationSummary();")),
        "shared evaluation report helper must refresh the evaluation tab summary in-page");
    QVERIFY2(body.contains(QStringLiteral("persistEvaluationReportSnapshot(trackingQuality);")),
        "probe calibration completion must hand off evaluation persistence to the shared helper");
}

void AnkleNavigationWorkflowContractTest::navigation_page_refreshes_navigation_confidence_after_registration_state_changes()
{
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));

    QVERIFY2(navigationSource.contains(QStringLiteral("connect(registrationService, &PointRegistrationService::pointAdded")),
        "navigation page must observe registration point additions");
    QVERIFY2(navigationSource.contains(QStringLiteral("connect(registrationService, &PointRegistrationService::pointRemoved")),
        "navigation page must observe registration point removals");
    QVERIFY2(navigationSource.contains(QStringLiteral("connect(registrationService, &PointRegistrationService::pointsCleared")),
        "navigation page must observe registration point clearing");
    QVERIFY2(navigationSource.contains(QStringLiteral("connect(registrationService, &PointRegistrationService::pointUpdated")),
        "navigation page must observe registration point updates");
    QVERIFY2(navigationSource.contains(QStringLiteral("onRegistrationFailed")),
        "navigation page must still expose registration failure handling");
    QVERIFY2(navigationSource.contains(QStringLiteral("onNavigationTimerUpdate")),
        "navigation page must still expose runtime navigation updates");
    QVERIFY2(navigationSource.contains(QStringLiteral("refreshNavigationConfidenceState();")),
        "registration state changes must refresh navigation confidence state");
}

void AnkleNavigationWorkflowContractTest::navigation_page_subscribes_runtime_status_changes_for_gate_refresh()
{
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));

    QVERIFY2(navigationSource.contains(QStringLiteral("connect(trackingService, &OpticalTrackingService::toolStatusChanged")),
        "navigation page must subscribe to optical tracking tool status changes");
    QVERIFY2(navigationSource.contains(QStringLiteral("connect(trackingService, &OpticalTrackingService::calibrationCompleted")),
        "navigation page must subscribe to calibration completion events");
    QVERIFY2(navigationSource.contains(QStringLiteral("connect(registrationService, &PointRegistrationService::registrationApplied")),
        "navigation page must subscribe to registration application events");
    QVERIFY2(navigationSource.contains(QStringLiteral("connect(registrationService, &PointRegistrationService::sessionStateChanged")),
        "navigation page must subscribe to registration session state changes");
    QVERIFY2(navigationSource.contains(QStringLiteral("updateProbeCalibrationUi();")),
        "runtime tracking status changes must refresh the probe calibration UI");
    QVERIFY2(navigationSource.contains(QStringLiteral("refreshNavigationConfidenceState();")),
        "runtime tracking and registration status changes must refresh navigation confidence state");
}

void AnkleNavigationWorkflowContractTest::navigation_page_enforces_runtime_navigation_gate_while_active()
{
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));
    const QString functionStart = QStringLiteral("void NavigationPageNew::onNavigationTimerUpdate()");
    const QString nextFunctionStart = QStringLiteral("void NavigationPageNew::onNavigation3DBoneLoaded");
    const int startIndex = navigationSource.indexOf(functionStart);
    QVERIFY2(startIndex >= 0, "navigation page must define onNavigationTimerUpdate()");

    const int endIndex = navigationSource.indexOf(nextFunctionStart, startIndex);
    QVERIFY2(endIndex > startIndex, "navigation page must keep onNavigation3DBoneLoaded after onNavigationTimerUpdate()");

    const QString timerUpdateBody = navigationSource.mid(startIndex, endIndex - startIndex);

    QVERIFY2(timerUpdateBody.contains(QStringLiteral("refreshNavigationConfidenceState(true);")),
        "runtime navigation updates must reevaluate navigation confidence with warning output");
    QVERIFY2(timerUpdateBody.contains(QStringLiteral("on_pauseNavigationButton_clicked();")),
        "runtime navigation updates must auto-pause navigation when confidence gate fails");
    QVERIFY2(timerUpdateBody.contains(QStringLiteral("if (!m_lastConfidence.allowNavigation)")),
        "runtime navigation updates must branch on the latest confidence gate result");
}

void AnkleNavigationWorkflowContractTest::navigation_page_persists_navigation_gate_evidence_into_evaluation_report()
{
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));
    const QString pauseFunctionStart = QStringLiteral("void NavigationPageNew::on_pauseNavigationButton_clicked()");
    const QString pauseNextFunctionStart = QStringLiteral("void NavigationPageNew::on_resetViewButton_clicked()");
    const int pauseStartIndex = navigationSource.indexOf(pauseFunctionStart);
    QVERIFY2(pauseStartIndex >= 0, "navigation page must define on_pauseNavigationButton_clicked()");

    const int pauseEndIndex = navigationSource.indexOf(pauseNextFunctionStart, pauseStartIndex);
    QVERIFY2(pauseEndIndex > pauseStartIndex, "navigation page must keep on_resetViewButton_clicked after pause handler");

    const QString pauseBody = navigationSource.mid(pauseStartIndex, pauseEndIndex - pauseStartIndex);

    const QString registrationFunctionStart = QStringLiteral("void NavigationPageNew::onRegistrationCompleted(const PointRegistrationResult& result)");
    const QString registrationNextFunctionStart = QStringLiteral("void NavigationPageNew::onRegistrationFailed(const QString& error)");
    const int registrationStartIndex = navigationSource.indexOf(registrationFunctionStart);
    QVERIFY2(registrationStartIndex >= 0, "navigation page must define onRegistrationCompleted()");

    const int registrationEndIndex = navigationSource.indexOf(registrationNextFunctionStart, registrationStartIndex);
    QVERIFY2(registrationEndIndex > registrationStartIndex, "navigation page must keep onRegistrationFailed after registration completion handler");

    const QString registrationBody = navigationSource.mid(registrationStartIndex, registrationEndIndex - registrationStartIndex);

    QVERIFY2(pauseBody.contains(QStringLiteral("persistEvaluationReportSnapshot(trackingQuality, true);")),
        "navigation pause must hand off evaluation persistence to the shared helper and export metrics");
    QVERIFY2(registrationBody.contains(QStringLiteral("evaluationService.saveRegistrationRecord(record);")),
        "navigation page must persist registration record after registration completes");
    QVERIFY2(registrationBody.contains(QStringLiteral("persistEvaluationReportSnapshot();")),
        "registration completion must refresh shared evaluation report state after persisting registration record");
}

void AnkleNavigationWorkflowContractTest::navigation_page_exposes_batch_evaluation_summary_export_entry()
{
    const QString navigationHeader = readFile(QStringLiteral("UI/NewPages/NavigationPage.h"));
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));
    const QString navigationUi = readFile(QStringLiteral("UI/Forms/NavigationPage.ui"));

    QVERIFY2(navigationHeader.contains(QStringLiteral("void on_exportEvaluationSummaryButton_clicked();")),
        "navigation page must expose a batch evaluation summary export slot");
    QVERIFY2(navigationUi.contains(QStringLiteral("exportEvaluationSummaryButton")),
        "evaluation tab must expose a batch evaluation summary export button");
    QVERIFY2(navigationSource.contains(QStringLiteral("discoverExportableCaseIds()")),
        "navigation page must ask evaluation service for exportable case ids");
    QVERIFY2(navigationSource.contains(QStringLiteral("exportBatchSummaryCsv(caseIds)")),
        "navigation page must export batch evaluation summary csv from discovered case ids");
}

void AnkleNavigationWorkflowContractTest::navigation_page_exports_innovation_summaries_with_data_root_above_cases_directory()
{
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));

    QVERIFY2(navigationSource.contains(QStringLiteral("InnovationExperimentBatchRunner")),
        "navigation page must use InnovationExperimentBatchRunner for innovation summary export");
    QVERIFY2(navigationSource.contains(QStringLiteral("input.caseDataRoot = QFileInfo(evaluationCasesRoot()).dir().absolutePath();")),
        "navigation page must pass the data root above the cases directory to innovation batch runner");
    QVERIFY2(navigationSource.contains(QStringLiteral("innovation_1_summary.csv")),
        "navigation page must surface innovation_1 summary export");
    QVERIFY2(navigationSource.contains(QStringLiteral("innovation_2_summary.csv")),
        "navigation page must surface innovation_2 summary export");
    QVERIFY2(navigationSource.contains(QStringLiteral("innovation_3_summary.csv")),
        "navigation page must surface innovation_3 summary export");
}

void AnkleNavigationWorkflowContractTest::navigation_page_uses_data_root_above_cases_directory_for_case_workspace_repository()
{
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));

    QVERIFY2(navigationSource.contains(QStringLiteral("const QString dataRoot = QFileInfo(casesRoot).dir().absolutePath();")),
        "navigation page must derive case workspace data root above the cases directory");
    QVERIFY2(navigationSource.contains(QStringLiteral("const AnkleCaseWorkspaceRepository repository(dataRoot);")),
        "navigation page must construct AnkleCaseWorkspaceRepository with the data root, not the cases root");
}

QTEST_APPLESS_MAIN(AnkleNavigationWorkflowContractTest)
#include "AnkleNavigationWorkflowContractTest.moc"
