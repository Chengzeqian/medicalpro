#include <QtTest/QtTest>

#include <QFile>
#include <QString>

class AnkleNavigationWorkflowContractTest : public QObject
{
    Q_OBJECT

private slots:
    void workflow_pages_thread_case_id_and_stage_structure();
    void navigation_workspace_application_service_becomes_workspace_truth_source();
    void navigation_workspace_snapshot_captures_stage_gate_state();
    void navigation_page_stage_gate_ui_reads_workspace_snapshot_truth_source();
    void navigation_workflow_coordinator_defers_stage_entry_to_workspace_gate();
    void navigation_page_tracker_connection_bridges_selected_instrument_tracking_configuration();
    void navigation_page_calibration_button_runs_active_tool_probe_calibration();
    void navigation_page_exposes_stepwise_probe_calibration_flow();
    void navigation_page_refreshes_navigation_confidence_after_probe_calibration();
    void navigation_page_persists_probe_calibration_evidence_into_evaluation_report();
    void navigation_page_records_workspace_snapshot_at_runtime_transitions();
    void navigation_page_evaluation_summary_prefers_workspace_snapshot_truth_source();
    void navigation_page_defers_runtime_status_summary_text_to_workspace_service();
    void navigation_page_refreshes_navigation_confidence_after_registration_state_changes();
    void navigation_page_subscribes_runtime_status_changes_for_gate_refresh();
    void navigation_page_enforces_runtime_navigation_gate_while_active();
    void navigation_page_persists_navigation_gate_evidence_into_evaluation_report();
    void navigation_page_recomputes_gate_inside_navigation_start_flow();
    void navigation_page_resets_runtime_snapshots_when_case_context_changes();
    void navigation_page_clears_tracking_session_and_tool_when_case_context_changes();
    void navigation_page_resets_gate_ui_when_registration_becomes_unavailable();
    void navigation_page_restores_calibration_snapshot_without_overwriting_it_on_case_reentry();
    void navigation_page_restores_registration_snapshot_into_runtime_state_on_case_reentry();
    void navigation_page_start_flow_accepts_restored_registration_snapshot_without_live_registration_service();
    void navigation_page_restores_navigation_snapshot_into_runtime_controls_on_case_reentry();
    void navigation_page_exposes_batch_evaluation_summary_export_entry();
    void navigation_page_exports_innovation_summaries_with_data_root_above_cases_directory();
    void navigation_page_uses_data_root_above_cases_directory_for_case_workspace_repository();
    void navigation_page_uses_three_panel_workspace_shell();
    void navigation_page_theme_includes_navigation_selectors();
    void navigation_page_wires_runtime_transform_chain_from_calibration_and_registration();
    void navigation_page_selects_bone_models_by_case_asset_id_and_resolves_case_paths();
    void navigation_page_selects_instrument_stl_from_current_selected_instrument();
    void navigation_page_persists_tracking_latency_in_navigation_run();
    void navigation_page_routes_single_virtual_space_pose_updates_through_runtime_coordinator_and_vtk_bridge();
    void navigation_page_exposes_error_aware_digital_twin_hud();
    void navigation_page_removes_legacy_import_and_segmentation_primary_actions();

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

void AnkleNavigationWorkflowContractTest::navigation_workspace_snapshot_captures_stage_gate_state()
{
    const QString navigationHeader = readFile(QStringLiteral("UI/NewPages/NavigationPage.h"));

    QVERIFY2(navigationHeader.contains(QStringLiteral("NavigationWorkspaceApplicationService")),
        "navigation page must declare workspace truth source");
    QVERIFY2(navigationHeader.contains(QStringLiteral("refreshStageGateUi")),
        "navigation page must declare a stage gate refresh hook");
}

void AnkleNavigationWorkflowContractTest::navigation_workspace_application_service_becomes_workspace_truth_source()
{
    const QString serviceHeader =
        readFile(QStringLiteral("UI/NewPages/Navigation/navigation_workspace_application_service.h"));
    const QString serviceSource =
        readFile(QStringLiteral("UI/NewPages/Navigation/navigation_workspace_application_service.cpp"));

    QVERIFY2(serviceHeader.contains(QStringLiteral("class NavigationWorkspaceApplicationService")),
        "workspace truth source must introduce NavigationWorkspaceApplicationService");
    QVERIFY2(serviceHeader.contains(QStringLiteral("NavigationWorkspaceSnapshot currentSnapshot() const")),
        "workspace application service must expose a current snapshot reader");
    QVERIFY2(serviceHeader.contains(QStringLiteral("NavigationStageGate evaluateStageGate(AnkleWorkflowStage stage)")),
        "workspace application service must expose stage gate evaluation by workflow stage");
    QVERIFY2(serviceSource.contains(QStringLiteral("buildSnapshot")),
        "workspace application service must assemble workspace snapshots");
}

void AnkleNavigationWorkflowContractTest::navigation_page_stage_gate_ui_reads_workspace_snapshot_truth_source()
{
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));
    const QString navigationHeader = readFile(QStringLiteral("UI/NewPages/NavigationPage.h"));
    const QString binderHeader =
        readFile(QStringLiteral("UI/NewPages/Navigation/navigation_workspace_ui_binder.h"));
    const QString binderSource =
        readFile(QStringLiteral("UI/NewPages/Navigation/navigation_workspace_ui_binder.cpp"));

    QVERIFY2(navigationSource.contains(QStringLiteral("refreshStageGateUi(")),
        "navigation page must route gate rendering through refreshStageGateUi()");
    QVERIFY2(navigationSource.contains(QStringLiteral("currentSnapshot()")),
        "navigation page must read the latest workspace snapshot");
    QVERIFY2(navigationHeader.contains(QStringLiteral("NavigationWorkspaceUiBinder")),
        "navigation page must declare a workspace UI binder collaborator");
    QVERIFY2(binderHeader.contains(QStringLiteral("class NavigationWorkspaceUiBinder")),
        "workspace UI binder must be introduced");
    QVERIFY2(binderSource.contains(QStringLiteral("refreshFromSnapshot")),
        "workspace UI binder must expose snapshot-driven UI refresh");
    QVERIFY2(binderSource.contains(QStringLiteral("applyWorkspaceSummary")),
        "workspace UI binder must own workspace summary rendering");
    QVERIFY2(binderSource.contains(QStringLiteral("button->setEnabled(")),
        "workspace UI binder must drive workflow rail button enablement");
    QVERIFY2(binderSource.contains(QStringLiteral("navigationConfidenceLabel")),
        "workspace UI binder must own navigation confidence rendering");
    QVERIFY2(binderSource.contains(QStringLiteral("calibrationStatusLabel")),
        "workspace UI binder must own calibration status rendering");
    QVERIFY2(navigationSource.contains(QStringLiteral("m_workspaceUiBinder->refreshFromSnapshot(")),
        "navigation page must delegate stage gate UI refresh to workspace UI binder");
    QVERIFY2(navigationSource.contains(QStringLiteral("m_workspaceUiBinder->applyNavigationConfidence(")),
        "navigation page must delegate runtime navigation confidence rendering to workspace UI binder");
    QVERIFY2(navigationSource.contains(QStringLiteral("m_workspaceUiBinder->applyCalibrationSummary(")),
        "navigation page must delegate probe calibration status rendering to workspace UI binder");
    QVERIFY2(!navigationSource.contains(QStringLiteral("m_navigationCaseStatusLabel->setText(")),
        "navigation page should not directly render case summary after binder extraction");
    QVERIFY2(!navigationSource.contains(QStringLiteral("navigationReadinessLabel->setText(")),
        "navigation page should not directly render navigation readiness after binder extraction");
    QVERIFY2(!navigationSource.contains(QStringLiteral("navigationConfidenceLabel->setText(")),
        "navigation page should not directly render navigation confidence after binder extraction");
    QVERIFY2(!navigationSource.contains(QStringLiteral("calibrationStatusLabel->setText(")),
        "navigation page should not directly render calibration status after binder extraction");
}

void AnkleNavigationWorkflowContractTest::navigation_workflow_coordinator_defers_stage_entry_to_workspace_gate()
{
    const QString coordinatorHeader =
        readFile(QStringLiteral("UI/NewPages/Navigation/navigation_workflow_coordinator.h"));
    const QString coordinatorSource =
        readFile(QStringLiteral("UI/NewPages/Navigation/navigation_workflow_coordinator.cpp"));

    QVERIFY2(coordinatorHeader.contains(QStringLiteral("NavigationWorkspaceApplicationService")),
        "workflow coordinator must accept workspace truth source for gate checks");
    QVERIFY2(coordinatorHeader.contains(QStringLiteral("bool tryEnterStage(AnkleWorkflowStage stage) const")),
        "workflow coordinator must expose gate-aware stage entry");
    QVERIFY2(coordinatorSource.contains(QStringLiteral("tryEnterStage(AnkleWorkflowStage::Registration)")),
        "workflow coordinator must defer registration stage entry to unified workspace gate");
    QVERIFY2(coordinatorSource.contains(QStringLiteral("tryEnterStage(AnkleWorkflowStage::Planning)")),
        "workflow coordinator must defer planning stage entry to unified workspace gate");
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
    QVERIFY2(navigationHeader.contains(QStringLiteral("class NavigationRuntimeCoordinator;")),
        "navigation page must forward declare NavigationRuntimeCoordinator");
    QVERIFY2(navigationHeader.contains(QStringLiteral("std::unique_ptr<NavigationRuntimeCoordinator> m_runtimeCoordinator;")),
        "navigation page must own a dedicated runtime coordinator");
    QVERIFY2(!navigationHeader.contains(QStringLiteral("bool tryBuildNavigationConfidenceInputs(NavigationConfidenceInputs& inputs) const;")),
        "navigation page must not keep a full confidence input builder after runtime coordinator extraction");
    QVERIFY2(navigationSource.contains(QStringLiteral("refreshNavigationConfidenceState(true);")),
        "navigation start must evaluate navigation confidence through the shared refresh helper");
    QVERIFY2(navigationSource.contains(QStringLiteral("m_runtimeCoordinator->handleTrackingQuality(")),
        "navigation page must hand tracking quality snapshots to runtime coordinator");
    QVERIFY2(navigationSource.contains(QStringLiteral("m_runtimeCoordinator->handleCalibrationCompleted(")),
        "navigation page must hand calibration completion snapshots to runtime coordinator");
    QVERIFY2(navigationSource.contains(QStringLiteral("m_runtimeCoordinator->recomputeConfidence();")),
        "navigation page must delegate confidence recompute to runtime coordinator");
    QVERIFY2(navigationSource.contains(QStringLiteral("m_lastConfidence = m_runtimeCoordinator->runtimeState()->confidenceResult();")),
        "navigation page must read the latest confidence snapshot back from runtime coordinator");
    QVERIFY2(navigationSource.contains(QStringLiteral("applyCalibrationResult(m_trackingSessionId, m_navigationToolId, calibrationResult)")),
        "probe calibration completion must still apply calibration result to the active tool");
    QVERIFY2(navigationSource.contains(QStringLiteral("refreshNavigationConfidenceState();")),
        "probe calibration completion must refresh navigation confidence state after applying calibration");
    QVERIFY2(!navigationSource.contains(QStringLiteral("inputs.toolCalibrated")),
        "navigation page must not assemble calibrated state directly after runtime coordinator extraction");
    QVERIFY2(!navigationSource.contains(QStringLiteral("inputs.calibrationAccuracy")),
        "navigation page must not assemble calibration accuracy directly after runtime coordinator extraction");
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

    const QString helperStart = QStringLiteral("void NavigationPageNew::persistEvaluationReportSnapshot(bool exportMetricsCsv)");
    const QString helperNextStart = QStringLiteral("void NavigationPageNew::updateProbeCalibrationUi()");
    const int helperStartIndex = navigationSource.indexOf(helperStart);
    QVERIFY2(helperStartIndex >= 0, "navigation page must define persistEvaluationReportSnapshot()");

    const int helperEndIndex = navigationSource.indexOf(helperNextStart, helperStartIndex);
    QVERIFY2(helperEndIndex > helperStartIndex, "navigation page must keep updateProbeCalibrationUi() after persistEvaluationReportSnapshot()");

    const QString helperBody = navigationSource.mid(helperStartIndex, helperEndIndex - helperStartIndex);

    QVERIFY2(navigationHeader.contains(QStringLiteral("void persistEvaluationReportSnapshot(bool exportMetricsCsv = false);")),
        "navigation page must declare a shared evaluation report persistence helper");
    QVERIFY2(helperBody.contains(QStringLiteral("m_runtimeCoordinator->persistEvaluationReportSnapshot(exportMetricsCsv);")),
        "shared evaluation report helper must delegate evaluation report persistence to runtime coordinator");
    QVERIFY2(helperBody.contains(QStringLiteral("refreshEvaluationSummary();")),
        "shared evaluation report helper must refresh the evaluation tab summary in-page");
    QVERIFY2(!helperBody.contains(QStringLiteral("NavigationEvaluationService evaluationService(evaluationCasesRoot());")),
        "shared evaluation report helper must not open evaluation service directly after runtime coordinator extraction");
    QVERIFY2(!helperBody.contains(QStringLiteral("AnkleEvaluationReport report;")),
        "shared evaluation report helper must not assemble evaluation reports directly after runtime coordinator extraction");
    QVERIFY2(body.contains(QStringLiteral("persistEvaluationReportSnapshot();")),
        "probe calibration completion must hand off evaluation persistence to the shared helper");
}

void AnkleNavigationWorkflowContractTest::navigation_page_records_workspace_snapshot_at_runtime_transitions()
{
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));

    const QString calibrationStart = QStringLiteral("void NavigationPageNew::finishProbeCalibration()");
    const QString calibrationNext = QStringLiteral("void NavigationPageNew::cancelProbeCalibration()");
    const int calibrationStartIndex = navigationSource.indexOf(calibrationStart);
    QVERIFY2(calibrationStartIndex >= 0, "navigation page must define finishProbeCalibration()");
    const int calibrationEndIndex = navigationSource.indexOf(calibrationNext, calibrationStartIndex);
    QVERIFY2(calibrationEndIndex > calibrationStartIndex, "navigation page must keep cancelProbeCalibration() after finishProbeCalibration()");
    const QString calibrationBody = navigationSource.mid(calibrationStartIndex, calibrationEndIndex - calibrationStartIndex);

    const QString registrationStart =
        QStringLiteral("void NavigationPageNew::onRegistrationCompleted(const PointRegistrationResult& result)");
    const QString registrationNext = QStringLiteral("void NavigationPageNew::onRegistrationFailed(const QString& error)");
    const int registrationStartIndex = navigationSource.indexOf(registrationStart);
    QVERIFY2(registrationStartIndex >= 0, "navigation page must define onRegistrationCompleted()");
    const int registrationEndIndex = navigationSource.indexOf(registrationNext, registrationStartIndex);
    QVERIFY2(registrationEndIndex > registrationStartIndex, "navigation page must keep onRegistrationFailed() after onRegistrationCompleted()");
    const QString registrationBody = navigationSource.mid(registrationStartIndex, registrationEndIndex - registrationStartIndex);

    const QString startNavigationStart = QStringLiteral("void NavigationPageNew::performStartNavigation()");
    const QString startNavigationNext = QStringLiteral("void NavigationPageNew::on_pauseNavigationButton_clicked()");
    const int startNavigationStartIndex = navigationSource.indexOf(startNavigationStart);
    QVERIFY2(startNavigationStartIndex >= 0, "navigation page must define performStartNavigation()");
    const int startNavigationEndIndex = navigationSource.indexOf(startNavigationNext, startNavigationStartIndex);
    QVERIFY2(startNavigationEndIndex > startNavigationStartIndex, "navigation page must keep on_pauseNavigationButton_clicked() after performStartNavigation()");
    const QString startNavigationBody = navigationSource.mid(startNavigationStartIndex, startNavigationEndIndex - startNavigationStartIndex);

    const QString pauseNavigationStart = QStringLiteral("void NavigationPageNew::on_pauseNavigationButton_clicked()");
    const QString pauseNavigationNext = QStringLiteral("void NavigationPageNew::on_resetViewButton_clicked()");
    const int pauseNavigationStartIndex = navigationSource.indexOf(pauseNavigationStart);
    QVERIFY2(pauseNavigationStartIndex >= 0, "navigation page must define on_pauseNavigationButton_clicked()");
    const int pauseNavigationEndIndex = navigationSource.indexOf(pauseNavigationNext, pauseNavigationStartIndex);
    QVERIFY2(pauseNavigationEndIndex > pauseNavigationStartIndex, "navigation page must keep on_resetViewButton_clicked() after on_pauseNavigationButton_clicked()");
    const QString pauseNavigationBody = navigationSource.mid(pauseNavigationStartIndex, pauseNavigationEndIndex - pauseNavigationStartIndex);

    QVERIFY2(calibrationBody.contains(QStringLiteral("m_workspaceApplicationService->recordCalibrationState(calibrationState);")),
        "probe calibration completion must write calibration state into workspace snapshot");
    QVERIFY2(calibrationBody.contains(QStringLiteral("m_workspaceApplicationService->persistSnapshot();")),
        "probe calibration completion must persist workspace snapshot after calibration writeback");
    QVERIFY2(registrationBody.contains(QStringLiteral("m_workspaceApplicationService->recordRegistrationState(registrationState);")),
        "registration completion must write registration state into workspace snapshot");
    QVERIFY2(!registrationBody.contains(QStringLiteral("QStringLiteral(\"registration_applied\")")),
        "registration completion must not keep a placeholder transform summary in workspace snapshot");
    QVERIFY2(registrationBody.contains(QStringLiteral("result.translationX")),
        "registration completion must persist a real registration transform summary");
    QVERIFY2(startNavigationBody.contains(QStringLiteral("m_workspaceApplicationService->recordNavigationState(navigationState);")),
        "navigation start must write navigation runtime state into workspace snapshot");
    QVERIFY2(pauseNavigationBody.contains(QStringLiteral("m_workspaceApplicationService->recordNavigationState(navigationState);")),
        "navigation pause must write navigation runtime state into workspace snapshot");
}

void AnkleNavigationWorkflowContractTest::navigation_page_evaluation_summary_prefers_workspace_snapshot_truth_source()
{
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));
    const QString formatterHeader =
        readFile(QStringLiteral("UI/NewPages/Navigation/navigation_evaluation_summary_formatter.h"));
    const QString formatterSource =
        readFile(QStringLiteral("UI/NewPages/Navigation/navigation_evaluation_summary_formatter.cpp"));
    const QString functionStart = QStringLiteral("void NavigationPageNew::refreshEvaluationSummary()");
    const QString nextFunctionStart = QStringLiteral("void NavigationPageNew::loadInstruments()");
    const int startIndex = navigationSource.indexOf(functionStart);
    QVERIFY2(startIndex >= 0, "navigation page must define refreshEvaluationSummary()");
    const int endIndex = navigationSource.indexOf(nextFunctionStart, startIndex);
    QVERIFY2(endIndex > startIndex, "navigation page must keep loadInstruments() after refreshEvaluationSummary()");
    const QString body = navigationSource.mid(startIndex, endIndex - startIndex);

    QVERIFY2(formatterHeader.contains(QStringLiteral("buildNavigationEvaluationSummary(const NavigationWorkspaceSnapshot& snapshot)")),
        "evaluation summary formatter must expose a workspace snapshot overload");
    QVERIFY2(formatterSource.contains(QStringLiteral("NavigationWorkspaceSnapshot")),
        "evaluation summary formatter must know how to summarize workspace snapshot truth");
    QVERIFY2(body.contains(QStringLiteral("m_workspaceApplicationService->currentSnapshot()")),
        "evaluation summary refresh must prefer workspace snapshot as summary truth source");
    QVERIFY2(body.contains(QStringLiteral("buildNavigationEvaluationSummary(")),
        "evaluation summary refresh must delegate summary rendering to the formatter");
}

void AnkleNavigationWorkflowContractTest::navigation_page_defers_runtime_status_summary_text_to_workspace_service()
{
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));
    const QString serviceHeader =
        readFile(QStringLiteral("UI/NewPages/Navigation/navigation_workspace_application_service.h"));

    QVERIFY2(serviceHeader.contains(QStringLiteral("recordCalibrationState")),
        "workspace application service must own calibration runtime state recording");
    QVERIFY2(serviceHeader.contains(QStringLiteral("recordNavigationState")),
        "workspace application service must own navigation runtime state recording");
    QVERIFY2(!navigationSource.contains(QStringLiteral("QStringLiteral(\"navigation_active\")")),
        "navigation page must not hardcode active navigation summary text after workspace summary extraction");
    QVERIFY2(!navigationSource.contains(QStringLiteral("QStringLiteral(\"navigation_paused\")")),
        "navigation page must not hardcode paused navigation summary text after workspace summary extraction");
    QVERIFY2(!navigationSource.contains(QStringLiteral("QStringLiteral(\"completed\")")),
        "navigation page must not hardcode calibration completion summary text after workspace summary extraction");
    QVERIFY2(!navigationSource.contains(QStringLiteral("const NavigationWorkspaceNavigationState previousState =")),
        "navigation page must not preserve prior navigation summary state by hand after workspace summary extraction");
    QVERIFY2(serviceHeader.contains(QStringLiteral("NavigationWorkspaceNavigationState buildNavigationSummary")),
        "workspace application service must own navigation summary composition");
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

    QVERIFY2(pauseBody.contains(QStringLiteral("persistEvaluationReportSnapshot(true);")),
        "navigation pause must hand off evaluation persistence to the shared helper and export metrics");
    QVERIFY2(registrationBody.contains(QStringLiteral("evaluationService.saveRegistrationRecord(record);")),
        "navigation page must persist registration record after registration completes");
    QVERIFY2(registrationBody.contains(QStringLiteral("persistEvaluationReportSnapshot();")),
        "registration completion must refresh shared evaluation report state after persisting registration record");
}

void AnkleNavigationWorkflowContractTest::navigation_page_recomputes_gate_inside_navigation_start_flow()
{
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));
    const QString workflowCoordinatorSource = readFile(QStringLiteral("UI/NewPages/Navigation/navigation_workflow_coordinator.cpp"));
    const QString evaluationControllerSource = readFile(QStringLiteral("UI/NewPages/Navigation/navigation_evaluation_controller.cpp"));

    const QString startFunctionStart = QStringLiteral("void NavigationPageNew::performStartNavigation()");
    const QString startFunctionNext = QStringLiteral("void NavigationPageNew::on_pauseNavigationButton_clicked()");
    const int startIndex = navigationSource.indexOf(startFunctionStart);
    QVERIFY2(startIndex >= 0, "navigation page must define performStartNavigation()");

    const int endIndex = navigationSource.indexOf(startFunctionNext, startIndex);
    QVERIFY2(endIndex > startIndex, "navigation page must keep pause handler after performStartNavigation()");

    const QString startBody = navigationSource.mid(startIndex, endIndex - startIndex);

    QVERIFY2(startBody.contains(QStringLiteral("refreshNavigationConfidenceState(true);")),
        "navigation start flow must recompute runtime gate inside performStartNavigation()");
    QVERIFY2(startBody.contains(QStringLiteral("setWorkflowStage(AnkleWorkflowStage::Navigation);")),
        "navigation start flow must enter navigation stage only after performStartNavigation() passes the runtime gate");
    QVERIFY2(!workflowCoordinatorSource.contains(QStringLiteral("!m_navigationEvaluationController->canStartNavigation()")),
        "workflow coordinator must not preemptively block navigation based on a potentially stale cached gate");
    QVERIFY2(!workflowCoordinatorSource.contains(QStringLiteral("enterStage(AnkleWorkflowStage::Navigation);")),
        "workflow coordinator must not enter navigation stage before page-level start flow confirms navigation can begin");
    QVERIFY2(evaluationControllerSource.contains(QStringLiteral("return confidenceResult().allowNavigation;")),
        "navigation evaluation controller may still expose the current cached gate, but start flow must not preemptively block on it");
}

void AnkleNavigationWorkflowContractTest::navigation_page_resets_runtime_snapshots_when_case_context_changes()
{
    const QString runtimeStateSource = readFile(QStringLiteral("UI/NewPages/Navigation/navigation_runtime_state.cpp"));

    QVERIFY2(runtimeStateSource.contains(QStringLiteral("m_trackingQuality.clear();")),
        "runtime state must clear tracking quality when case context changes");
    QVERIFY2(runtimeStateSource.contains(QStringLiteral("m_registrationResult = PointRegistrationResult();")),
        "runtime state must clear registration result when case context changes");
    QVERIFY2(runtimeStateSource.contains(QStringLiteral("m_confidenceResult = NavigationConfidenceResult();")),
        "runtime state must clear confidence result when case context changes");
    QVERIFY2(runtimeStateSource.contains(QStringLiteral("m_hasTrackingQuality = false;")),
        "runtime state must reset tracking snapshot presence when case context changes");
    QVERIFY2(runtimeStateSource.contains(QStringLiteral("m_hasRegistrationResult = false;")),
        "runtime state must reset registration snapshot presence when case context changes");
    QVERIFY2(runtimeStateSource.contains(QStringLiteral("m_hasConfidenceResult = false;")),
        "runtime state must reset confidence snapshot presence when case context changes");
}

void AnkleNavigationWorkflowContractTest::navigation_page_clears_tracking_session_and_tool_when_case_context_changes()
{
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));
    const QString setCaseContextStart =
        QStringLiteral("void NavigationPageNew::setCaseContext(const QString& caseId, int patientId, const QString& patientName)");
    const QString setCaseContextNext = QStringLiteral("void NavigationPageNew::setPatientId(int patientId)");
    const int setCaseContextStartIndex = navigationSource.indexOf(setCaseContextStart);
    QVERIFY2(setCaseContextStartIndex >= 0, "navigation page must define setCaseContext()");

    const int setCaseContextEndIndex = navigationSource.indexOf(setCaseContextNext, setCaseContextStartIndex);
    QVERIFY2(setCaseContextEndIndex > setCaseContextStartIndex, "navigation page must keep setPatientId() after setCaseContext()");

    const QString setCaseContextBody = navigationSource.mid(
        setCaseContextStartIndex,
        setCaseContextEndIndex - setCaseContextStartIndex);

    QVERIFY2(setCaseContextBody.contains(QStringLiteral("m_runtimeState->setCaseContext(caseId, QString(), QString());")),
        "case context switch must not carry old tracking session and tool ids into the new case");
}

void AnkleNavigationWorkflowContractTest::navigation_page_resets_gate_ui_when_registration_becomes_unavailable()
{
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));
    const QString functionStart = QStringLiteral("void NavigationPageNew::refreshNavigationConfidenceState(bool showWarnings)");
    const QString nextFunctionStart = QStringLiteral("void NavigationPageNew::persistEvaluationReportSnapshot(bool exportMetricsCsv)");
    const int startIndex = navigationSource.indexOf(functionStart);
    QVERIFY2(startIndex >= 0, "navigation page must define refreshNavigationConfidenceState()");

    const int endIndex = navigationSource.indexOf(nextFunctionStart, startIndex);
    QVERIFY2(endIndex > startIndex, "navigation page must keep persistEvaluationReportSnapshot() after refreshNavigationConfidenceState()");

    const QString body = navigationSource.mid(startIndex, endIndex - startIndex);

    QVERIFY2(body.contains(QStringLiteral("registrationTransform.isIdentity() || !registrationResult.success")),
        "navigation confidence refresh must detect unavailable registration state");
    QVERIFY2(body.contains(QStringLiteral("m_workspaceUiBinder->applyStageGate(")),
        "navigation confidence refresh must route readiness reset through workspace UI binder");
    QVERIFY2(body.contains(QStringLiteral("m_workspaceUiBinder->applyNavigationConfidence(")),
        "navigation confidence refresh must route confidence reset through workspace UI binder");
    QVERIFY2(!body.contains(QStringLiteral("navigationReadinessLabel->setText(")),
        "navigation confidence refresh must not directly reset readiness text");
    QVERIFY2(!body.contains(QStringLiteral("navigationConfidenceLabel->setText(")),
        "navigation confidence refresh must not directly reset confidence text");
    QVERIFY2(body.contains(QStringLiteral("resetGateUi(false);")),
        "navigation confidence refresh must disable the start button when registration becomes unavailable");
}

void AnkleNavigationWorkflowContractTest::navigation_page_restores_calibration_snapshot_without_overwriting_it_on_case_reentry()
{
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));
    const QString setCaseContextStart =
        QStringLiteral("void NavigationPageNew::setCaseContext(const QString& caseId, int patientId, const QString& patientName)");
    const QString setCaseContextNext = QStringLiteral("void NavigationPageNew::setPatientId(int patientId)");
    const int setCaseContextStartIndex = navigationSource.indexOf(setCaseContextStart);
    QVERIFY2(setCaseContextStartIndex >= 0, "navigation page must define setCaseContext()");

    const int setCaseContextEndIndex = navigationSource.indexOf(setCaseContextNext, setCaseContextStartIndex);
    QVERIFY2(setCaseContextEndIndex > setCaseContextStartIndex, "navigation page must keep setPatientId() after setCaseContext()");

    const QString setCaseContextBody = navigationSource.mid(
        setCaseContextStartIndex,
        setCaseContextEndIndex - setCaseContextStartIndex);

    const QString calibrationUiStart = QStringLiteral("void NavigationPageNew::updateProbeCalibrationUi()");
    const QString calibrationUiNext = QStringLiteral("void NavigationPageNew::updatePositionDisplay(double x, double y, double z)");
    const int calibrationUiStartIndex = navigationSource.indexOf(calibrationUiStart);
    QVERIFY2(calibrationUiStartIndex >= 0, "navigation page must define updateProbeCalibrationUi()");

    const int calibrationUiEndIndex = navigationSource.indexOf(calibrationUiNext, calibrationUiStartIndex);
    QVERIFY2(calibrationUiEndIndex > calibrationUiStartIndex, "navigation page must keep updatePositionDisplay() after updateProbeCalibrationUi()");

    const QString calibrationUiBody = navigationSource.mid(
        calibrationUiStartIndex,
        calibrationUiEndIndex - calibrationUiStartIndex);

    QVERIFY2(setCaseContextBody.contains(QStringLiteral("updateProbeCalibrationUi();")),
        "case reentry must refresh probe calibration UI from restored workspace snapshot");
    QVERIFY2(calibrationUiBody.contains(QStringLiteral("const NavigationWorkspaceCalibrationState snapshotCalibrationState =")),
        "probe calibration UI refresh must read restored calibration snapshot before overwriting runtime state");
    QVERIFY2(calibrationUiBody.contains(QStringLiteral("if (!trackingReady && snapshotCalibrationState.completed)")),
        "probe calibration UI refresh must preserve restored completed calibration state when tracking is not yet reconnected");
    QVERIFY2(calibrationUiBody.contains(QStringLiteral("if (!hasActiveCalibration && snapshotCalibrationState.completed)")),
        "probe calibration UI refresh must preserve restored completed calibration state when no live calibration session exists");
}

void AnkleNavigationWorkflowContractTest::navigation_page_restores_registration_snapshot_into_runtime_state_on_case_reentry()
{
    const QString navigationHeader = readFile(QStringLiteral("UI/NewPages/NavigationPage.h"));
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));

    const QString setCaseContextStart =
        QStringLiteral("void NavigationPageNew::setCaseContext(const QString& caseId, int patientId, const QString& patientName)");
    const QString setCaseContextNext = QStringLiteral("void NavigationPageNew::setPatientId(int patientId)");
    const int setCaseContextStartIndex = navigationSource.indexOf(setCaseContextStart);
    QVERIFY2(setCaseContextStartIndex >= 0, "navigation page must define setCaseContext()");

    const int setCaseContextEndIndex = navigationSource.indexOf(setCaseContextNext, setCaseContextStartIndex);
    QVERIFY2(setCaseContextEndIndex > setCaseContextStartIndex, "navigation page must keep setPatientId() after setCaseContext()");

    const QString setCaseContextBody = navigationSource.mid(
        setCaseContextStartIndex,
        setCaseContextEndIndex - setCaseContextStartIndex);

    const QString confidenceRefreshStart =
        QStringLiteral("void NavigationPageNew::refreshNavigationConfidenceState(bool showWarnings)");
    const QString confidenceRefreshNext =
        QStringLiteral("void NavigationPageNew::refreshStageGateUi()");
    const int confidenceRefreshStartIndex = navigationSource.indexOf(confidenceRefreshStart);
    QVERIFY2(confidenceRefreshStartIndex >= 0, "navigation page must define refreshNavigationConfidenceState()");

    const int confidenceRefreshEndIndex = navigationSource.indexOf(confidenceRefreshNext, confidenceRefreshStartIndex);
    QVERIFY2(confidenceRefreshEndIndex > confidenceRefreshStartIndex, "navigation page must keep refreshStageGateUi() after refreshNavigationConfidenceState()");

    const QString confidenceRefreshBody = navigationSource.mid(
        confidenceRefreshStartIndex,
        confidenceRefreshEndIndex - confidenceRefreshStartIndex);

    const QString restoreRegistrationStart = QStringLiteral("void NavigationPageNew::restoreRegistrationSnapshotState()");
    const QString restoreRegistrationNext = QStringLiteral("void NavigationPageNew::onRegistrationStateChanged(RegistrationSessionState state)");
    const int restoreRegistrationStartIndex = navigationSource.indexOf(restoreRegistrationStart);
    QVERIFY2(restoreRegistrationStartIndex >= 0, "navigation page must define restoreRegistrationSnapshotState()");

    const int restoreRegistrationEndIndex = navigationSource.indexOf(restoreRegistrationNext, restoreRegistrationStartIndex);
    QVERIFY2(restoreRegistrationEndIndex > restoreRegistrationStartIndex, "navigation page must keep ensurePointRegistrationService() after restoreRegistrationSnapshotState()");

    const QString restoreRegistrationBody = navigationSource.mid(
        restoreRegistrationStartIndex,
        restoreRegistrationEndIndex - restoreRegistrationStartIndex);

    QVERIFY2(navigationHeader.contains(QStringLiteral("void restoreRegistrationSnapshotState();")),
        "navigation page must declare a registration snapshot restore helper");
    QVERIFY2(setCaseContextBody.contains(QStringLiteral("restoreRegistrationSnapshotState();")),
        "case reentry must restore registration snapshot state before refreshing gate UI");
    QVERIFY2(restoreRegistrationBody.contains(QStringLiteral("updateRegistrationResultDisplay(")),
        "case reentry must restore registration result display from workspace snapshot");
    QVERIFY2(restoreRegistrationBody.contains(QStringLiteral("m_runtimeState->setRegistrationResult(")),
        "case reentry must hydrate runtime registration result from workspace snapshot");
    QVERIFY2(restoreRegistrationBody.contains(QStringLiteral("buildRegistrationResultFromWorkspaceState(")),
        "case reentry must rebuild registration result from persisted workspace transform data");
    QVERIFY2(!restoreRegistrationBody.contains(QStringLiteral("registrationState.pointCount > 0 ? 1.0f : 0.0f")),
        "case reentry must not synthesize a fake transform from point count");
    QVERIFY2(confidenceRefreshBody.contains(QStringLiteral("const PointRegistrationResult registrationResult = m_runtimeState->hasRegistrationResult()")),
        "navigation confidence refresh must fall back to restored runtime registration result");
    QVERIFY2(confidenceRefreshBody.contains(QStringLiteral("const QMatrix4x4 registrationTransform = m_registrationTransform.isIdentity()")),
        "navigation confidence refresh must fall back to restored registration transform when service transform is unavailable");
}

void AnkleNavigationWorkflowContractTest::navigation_page_start_flow_accepts_restored_registration_snapshot_without_live_registration_service()
{
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));
    const QString startFunctionStart = QStringLiteral("void NavigationPageNew::performStartNavigation()");
    const QString startFunctionNext = QStringLiteral("void NavigationPageNew::on_pauseNavigationButton_clicked()");
    const int startIndex = navigationSource.indexOf(startFunctionStart);
    QVERIFY2(startIndex >= 0, "navigation page must define performStartNavigation()");

    const int endIndex = navigationSource.indexOf(startFunctionNext, startIndex);
    QVERIFY2(endIndex > startIndex, "navigation page must keep pause handler after performStartNavigation()");

    const QString startBody = navigationSource.mid(startIndex, endIndex - startIndex);

    QVERIFY2(startBody.contains(QStringLiteral("const PointRegistrationResult registrationResult = m_runtimeState->hasRegistrationResult()")),
        "navigation start flow must consult restored runtime registration result before failing on missing live registration service");
    QVERIFY2(startBody.contains(QStringLiteral("const QMatrix4x4 registrationTransform = m_registrationTransform.isIdentity()")),
        "navigation start flow must consult restored registration transform before requiring live registration service");
    QVERIFY2(!startBody.contains(QStringLiteral("if (!registrationService) {\n        showWarning(\"导航\", \"配准服务不可用，请先完成配准。\")")),
        "navigation start flow must not hard-stop solely because the live registration service is unavailable");
    QVERIFY2(!startBody.contains(QStringLiteral("m_registrationTransform = registrationService->getTransformMatrix();")),
        "navigation start flow must not overwrite restored registration transform before checking whether it is usable");
}

void AnkleNavigationWorkflowContractTest::navigation_page_restores_navigation_snapshot_into_runtime_controls_on_case_reentry()
{
    const QString navigationHeader = readFile(QStringLiteral("UI/NewPages/NavigationPage.h"));
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));

    const QString setCaseContextStart =
        QStringLiteral("void NavigationPageNew::setCaseContext(const QString& caseId, int patientId, const QString& patientName)");
    const QString setCaseContextNext = QStringLiteral("void NavigationPageNew::setPatientId(int patientId)");
    const int setCaseContextStartIndex = navigationSource.indexOf(setCaseContextStart);
    QVERIFY2(setCaseContextStartIndex >= 0, "navigation page must define setCaseContext()");

    const int setCaseContextEndIndex = navigationSource.indexOf(setCaseContextNext, setCaseContextStartIndex);
    QVERIFY2(setCaseContextEndIndex > setCaseContextStartIndex, "navigation page must keep setPatientId() after setCaseContext()");

    const QString setCaseContextBody = navigationSource.mid(
        setCaseContextStartIndex,
        setCaseContextEndIndex - setCaseContextStartIndex);

    const QString restoreNavigationStart = QStringLiteral("void NavigationPageNew::restoreNavigationSnapshotState()");
    const QString restoreNavigationNext = QStringLiteral("void NavigationPageNew::onRegistrationStateChanged(RegistrationSessionState state)");
    const int restoreNavigationStartIndex = navigationSource.indexOf(restoreNavigationStart);
    QVERIFY2(restoreNavigationStartIndex >= 0, "navigation page must define restoreNavigationSnapshotState()");

    const int restoreNavigationEndIndex = navigationSource.indexOf(restoreNavigationNext, restoreNavigationStartIndex);
    QVERIFY2(restoreNavigationEndIndex > restoreNavigationStartIndex, "navigation page must keep onRegistrationStateChanged() after restoreNavigationSnapshotState()");

    const QString restoreNavigationBody = navigationSource.mid(
        restoreNavigationStartIndex,
        restoreNavigationEndIndex - restoreNavigationStartIndex);

    QVERIFY2(navigationHeader.contains(QStringLiteral("void restoreNavigationSnapshotState();")),
        "navigation page must declare a navigation snapshot restore helper");
    QVERIFY2(setCaseContextBody.contains(QStringLiteral("restoreNavigationSnapshotState();")),
        "case reentry must restore navigation runtime controls from workspace snapshot");
    QVERIFY2(restoreNavigationBody.contains(QStringLiteral("const NavigationWorkspaceNavigationState navigationState =")),
        "navigation restore helper must read navigation snapshot state");
    QVERIFY2(restoreNavigationBody.contains(QStringLiteral("m_navigationActive = navigationState.running;")),
        "navigation restore helper must restore navigation active flag from workspace snapshot");
    QVERIFY2(restoreNavigationBody.contains(QStringLiteral("m_navigationTimer->stop();")),
        "navigation restore helper must not auto-restart the navigation timer on case reentry");
    QVERIFY2(restoreNavigationBody.contains(QStringLiteral("m_motionSimulator->setPaused(!navigationState.running);")),
        "navigation restore helper must restore motion simulator pause state from workspace snapshot");
    QVERIFY2(restoreNavigationBody.contains(QStringLiteral("m_navigation3DView->setProbeVisible(")),
        "navigation restore helper must restore probe visibility from workspace snapshot");
    QVERIFY2(restoreNavigationBody.contains(QStringLiteral("ui->startNavigationButton->setEnabled(")),
        "navigation restore helper must restore start button state");
    QVERIFY2(restoreNavigationBody.contains(QStringLiteral("ui->pauseNavigationButton->setEnabled(")),
        "navigation restore helper must restore pause button state");
    QVERIFY2(restoreNavigationBody.contains(QStringLiteral("updateFourViewWidgetPlacement();")),
        "navigation restore helper must refresh runtime layout after applying snapshot state");
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

void AnkleNavigationWorkflowContractTest::navigation_page_uses_three_panel_workspace_shell()
{
    const QString navigationHeader = readFile(QStringLiteral("UI/NewPages/NavigationPage.h"));
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));

    QVERIFY2(navigationHeader.contains(QStringLiteral("setupNavigationWorkspaceShell")),
        "NavigationPage must declare a setup hook for the modern workspace shell");
    QVERIFY2(navigationHeader.contains(QStringLiteral("syncWorkflowRailState")),
        "NavigationPage must declare a sync hook for the left workflow rail");
    QVERIFY2(navigationSource.contains(QStringLiteral("navigationWorkflowRailFrame")),
        "NavigationPage must create a left workflow rail frame");
    QVERIFY2(navigationSource.contains(QStringLiteral("navigationWorkspaceFrame")),
        "NavigationPage must create a central workspace frame");
    QVERIFY2(navigationSource.contains(QStringLiteral("navigationStatusRailFrame")),
        "NavigationPage must create a right status rail frame");
    QVERIFY2(navigationSource.contains(QStringLiteral("ui->tabWidget->tabBar()->hide()")),
        "NavigationPage must hide the old top tab bar after adding the left workflow rail");
}

void AnkleNavigationWorkflowContractTest::navigation_page_theme_includes_navigation_selectors()
{
    const QString theme = readFile(QStringLiteral("UI/styles/three_pages_theme.qss"));

    QVERIFY2(theme.contains(QStringLiteral("QWidget#NavigationPage")),
        "three page theme must include NavigationPage in the shared page selectors");
    QVERIFY2(theme.contains(QStringLiteral("QFrame#navigationWorkflowRailFrame")),
        "theme must style the navigation workflow rail");
    QVERIFY2(theme.contains(QStringLiteral("QFrame#navigationStatusRailFrame")),
        "theme must style the navigation status rail");
}

void AnkleNavigationWorkflowContractTest::navigation_page_wires_runtime_transform_chain_from_calibration_and_registration()
{
    const QString navigationHeader = readFile(QStringLiteral("UI/NewPages/NavigationPage.h"));
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));
    const QString coordinatorHeader =
        readFile(QStringLiteral("UI/NewPages/Navigation/navigation_runtime_coordinator.h"));

    QVERIFY2(coordinatorHeader.contains(QStringLiteral("void clearPoseTrackingState();")),
        "runtime coordinator must expose a pose-pipeline reset entry for case/tool switches");
    QVERIFY2(coordinatorHeader.contains(QStringLiteral("void clearRegistrationTransform();")),
        "runtime coordinator must expose a registration transform reset entry");
    QVERIFY2(navigationHeader.contains(QStringLiteral("void pushSimulatedPoseFrameToRuntime(")),
        "navigation page must accept realtime pose input when pushing runtime pose frames");
    QVERIFY2(navigationSource.contains(QStringLiteral("m_runtimeCoordinator->handleCalibrationTransform(")),
        "navigation page must forward calibration transform into runtime coordinator");
    QVERIFY2(navigationSource.contains(QStringLiteral("m_runtimeCoordinator->handleRegistrationTransform(")),
        "navigation page must forward registration transform into runtime coordinator");
    QVERIFY2(navigationSource.contains(QStringLiteral("m_runtimeCoordinator->clearPoseTrackingState();")),
        "navigation page must clear realtime pose state when tracking runtime is reset");
    QVERIFY2(navigationSource.contains(QStringLiteral("m_runtimeCoordinator->clearRegistrationTransform();")),
        "navigation page must clear registration transform when registration becomes unavailable or case context changes");
}

void AnkleNavigationWorkflowContractTest::navigation_page_selects_bone_models_by_case_asset_id_and_resolves_case_paths()
{
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));

    QVERIFY2(navigationSource.contains(QStringLiteral("const QString caseRoot = repository.caseRoot(caseId);")),
        "active bone model lookup must resolve paths relative to the case workspace root");
    QVERIFY2(navigationSource.contains(QStringLiteral("const QString assetBoneId = QStringLiteral(\"bone:%1\").arg(asset.boneName);")),
        "active bone model lookup must normalize manifest bone names into case asset ids");
    QVERIFY2(navigationSource.contains(QStringLiteral("QDir(caseRoot).filePath(asset.normalizedPath)")),
        "active bone model lookup must convert relative case asset paths into real file paths");
}

void AnkleNavigationWorkflowContractTest::navigation_page_selects_instrument_stl_from_current_selected_instrument()
{
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));
    const QString vtkBridgeHeader =
        readFile(QStringLiteral("UI/NewPages/Navigation/navigation_vtk_bridge.h"));
    const QString vtkBridgeSource =
        readFile(QStringLiteral("UI/NewPages/Navigation/navigation_vtk_bridge.cpp"));

    QVERIFY2(navigationSource.contains(QStringLiteral("instrument.modelFilePath")),
        "active navigation instrument model must come from the selected instrument STL/model path");
    QVERIFY2(!navigationSource.contains(QStringLiteral("return binding.geometryFilePath;")),
        "active navigation instrument model must not reuse tracking geometry ini path as the display model");
    QVERIFY2(vtkBridgeHeader.contains(QStringLiteral("void setNavigationViewWidget(")),
        "navigation vtk bridge must bind to the real single-window navigation render widget");
    QVERIFY2(vtkBridgeSource.contains(QStringLiteral("m_navigationViewWidget")),
        "navigation vtk bridge must forward digital twin updates into the real navigation render widget");
}

void AnkleNavigationWorkflowContractTest::navigation_page_persists_tracking_latency_in_navigation_run()
{
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));

    QVERIFY2(navigationSource.contains(QStringLiteral("run.metrics.insert(QStringLiteral(\"tracking_latency_ms\")")),
        "navigation run persistence must record tracking latency into evaluation metrics");
}

void AnkleNavigationWorkflowContractTest::navigation_page_routes_single_virtual_space_pose_updates_through_runtime_coordinator_and_vtk_bridge()
{
    const QString navigationSource = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));

    QVERIFY2(navigationSource.contains(QStringLiteral("m_runtimeCoordinator->handlePoseFrame(")),
        "navigation page must send realtime pose frames into NavigationRuntimeCoordinator");
    QVERIFY2(navigationSource.contains(QStringLiteral("m_navigationVtkBridge->loadBoneModels(")),
        "navigation page must preload active bone STL models into NavigationVtkBridge");
    QVERIFY2(navigationSource.contains(QStringLiteral("m_navigationVtkBridge->loadInstrumentModel(")),
        "navigation page must preload current instrument STL model into NavigationVtkBridge");
    QVERIFY2(navigationSource.contains(QStringLiteral("m_navigationVtkBridge->updateInstrumentPose(")),
        "navigation page must update instrument pose through NavigationVtkBridge");
    QVERIFY2(navigationSource.contains(QStringLiteral("m_navigationVtkBridge->setInstrumentVisible(")),
        "navigation page must update instrument visibility through NavigationVtkBridge");
}

void AnkleNavigationWorkflowContractTest::navigation_page_exposes_error_aware_digital_twin_hud()
{
    const QString pageHeader = readFile(QStringLiteral("UI/NewPages/NavigationPage.h"));
    const QString pageCode = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));
    const QString theme = readFile(QStringLiteral("UI/styles/three_pages_theme.qss"));

    QVERIFY2(pageHeader.contains(QStringLiteral("DigitalTwinTargetRegionDefinition currentTargetRegionDefinition() const;")),
        "navigation page must declare a target-region-to-digital-twin adapter helper");
    QVERIFY2(pageCode.contains(QStringLiteral("navigationHudFrame")),
        "navigation page must expose a digital twin hud frame");
    QVERIFY2(pageCode.contains(QStringLiteral("navigationHudRiskLabel")),
        "navigation page must expose digital twin dominant risk text");
    QVERIFY2(pageCode.contains(QStringLiteral("navigationHudTargetLabel")),
        "navigation page must expose target region navigation status text");
    QVERIFY2(pageCode.contains(QStringLiteral("m_runtimeCoordinator->setTargetRegionDefinition(")),
        "navigation page must push target region context into runtime coordinator");
    QVERIFY2(pageCode.contains(QStringLiteral("m_navigationVtkBridge->setTargetRegionDefinition(")),
        "navigation page must push target region context into vtk bridge");
    QVERIFY2(pageCode.contains(QStringLiteral("m_navigationVtkBridge->setTargetRegionRiskTone(")),
        "navigation page must project digital twin risk tone into the vtk bridge");
    QVERIFY2(theme.contains(QStringLiteral("QFrame#navigationHudFrame")),
        "theme must style the digital twin hud frame");
    QVERIFY2(theme.contains(QStringLiteral("QLabel#navigationHudRiskLabel")),
        "theme must style the digital twin risk label");
    QVERIFY2(theme.contains(QStringLiteral("QLabel#navigationHudTargetLabel")),
        "theme must style the digital twin target label");
}

void AnkleNavigationWorkflowContractTest::navigation_page_removes_legacy_import_and_segmentation_primary_actions()
{
    const QString pageCode = readFile(QStringLiteral("UI/NewPages/NavigationPage.cpp"));
    const QString pageUi = readFile(QStringLiteral("UI/Forms/NavigationPage.ui"));

    QVERIFY2(!pageCode.contains(QStringLiteral("器械导入功能将通过CTK服务实现")),
        "NavigationPage must not keep legacy instrument import placeholder in the primary workflow");
    QVERIFY2(!pageUi.contains(QStringLiteral("自动分割")),
        "NavigationPage.ui must not keep legacy auto segmentation as a primary planning action");
    QVERIFY2(!pageUi.contains(QStringLiteral("导出STL")),
        "NavigationPage.ui must not keep legacy STL export as a primary planning action");
    QVERIFY2(!pageUi.contains(QStringLiteral("加载2D图像")),
        "NavigationPage.ui must not keep legacy 2D-3D placeholder entry");
}

QTEST_APPLESS_MAIN(AnkleNavigationWorkflowContractTest)
#include "AnkleNavigationWorkflowContractTest.moc"
