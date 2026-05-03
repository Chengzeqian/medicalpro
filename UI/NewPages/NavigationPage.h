#ifndef NAVIGATIONPAGE_NEW_H
#define NAVIGATIONPAGE_NEW_H

#include "BasePage.h"
#include "PageIndex.h"
#include "Framework/Navigation/navigation_confidence_evaluator.h"
#include "Plugins/PointRegistration/PointRegistrationDataStructures.h"

#include <QEvent>
#include <QMatrix4x4>
#include <QPointer>
#include <QTimer>
#include <memory>

namespace Ui {
class NavigationPage;
}

class BoneSurfaceMotionSimulator;
class BoneSegmentationService;
class DicomViewerService;
class EmbeddedVtkViewHost;
class FourViewDisplayService;
class InstrumentManagementService;
class LegacyNavigationPageServiceAdapter;
class NavigationEvaluationController;
class Navigation3DViewWidget;
class NavigationPageServiceAccess;
class NavigationServiceBundle;
class NavigationVtkBridge;
class NavigationWorkflowCoordinator;
class NavigationWorkflowContext;
class OpticalTrackingService;
class PointRegistrationService;
class PreparationPlanningController;
class RegistrationWorkflow;
class RegistrationController;

enum class AnkleWorkflowStage
{
    Preparation,
    Planning,
    Registration,
    Navigation,
    Evaluation
};

class NavigationPageNew : public BasePage
{
    Q_OBJECT

public:
    explicit NavigationPageNew(QWidget* parent = nullptr, NavigationPageServiceAccess* serviceAccess = nullptr);
    ~NavigationPageNew();

    void onActivated() override;
    void onDeactivated() override;

    void setCaseContext(const QString& caseId, int patientId, const QString& patientName);
    void setPatientId(int patientId);
    void setPatientName(const QString& name);

signals:
    void backToMainRequested();

private slots:
    void on_backButton_clicked();
    void on_importInstrumentButton_clicked();
    void on_deleteInstrumentButton_clicked();
    void on_refreshInstrumentButton_clicked();
    void on_clearAllInstrumentButton_clicked();
    void on_generateThumbnailButton_clicked();
    void on_loadDicomButton_clicked();
    void on_autoSegmentButton_clicked();
    void on_manualSegmentButton_clicked();
    void on_exportSTLButton_clicked();
    void on_selectProsthesisButton_clicked();
    void on_adjustProsthesisButton_clicked();
    void on_loadModelButton_clicked();
    void on_toggleModelButton_clicked();
    void on_load2DImageButton_clicked();
    void on_start2D3DRegButton_clicked();
    void on_collectPointButton_clicked();
    void on_computeRegButton_clicked();
    void on_calibrateButton_clicked();
    void on_deletePointButton_clicked();
    void on_clearAllPointsButton_clicked();
    void on_connectTrackerButton_clicked();
    void on_disconnectTrackerButton_clicked();
    void on_startNavigationButton_clicked();
    void on_pauseNavigationButton_clicked();
    void on_exportEvaluationSummaryButton_clicked();
    void on_resetViewButton_clicked();
    void onTrackerDataReceived();
    void onNavigationTimerUpdate();
    void onNavigation3DBoneLoaded(bool success, const QVector3D& center, const QVector3D& size);
    void onInstrumentCardClicked(int instrumentId);
    void onSegmentationProgress(const QString& taskId, int progress, const QString& message);
    void onSegmentationCompleted(const QString& taskId, const QVariantMap& result);
    void onSegmentationFailed(const QString& taskId, const QString& error);
    void onRegistrationStateChanged(RegistrationSessionState state);
    void onRegistrationProgressUpdated(int progress, const QString& message);
    void onRegistrationModelLoaded(bool success, const QString& info);
    void onRegistrationPointAdded(int index, const QVector3D& position);
    void onRegistrationProbePointCaptured(int index, const QVector3D& position);
    void onRegistrationCompleted(const PointRegistrationResult& result);
    void onRegistrationFailed(const QString& error);
    void onRegistrationPointPicked(double x, double y, double z);

public:
    void beginTransitionMask();
    void endTransitionMask();
    void resetPage();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void setWorkflowStage(AnkleWorkflowStage stage);
    QString evaluationCasesRoot() const;
    void refreshEvaluationSummary();
    void loadInstruments();
    void setupVTKViews();
    void cleanupVTKViews();
    void embedFourViewWidget();
    void updateFourViewWidgetPlacement();
    void updateTrackerStatus(bool connected);
    void updatePositionDisplay(double x, double y, double z);
    void updateAccuracyDisplay(double accuracy);
    void setupRegistration();
    void embedRegistrationVTKWidget();
    void updateRegistrationPointsList();
    void updateRegistrationResultDisplay(const PointRegistrationResult& result);
    bool ensurePointRegistrationService(bool tryStartPlugin = true);
    void refreshPatientInfoLabel();
    InstrumentManagementService* instrumentManagementService() const;
    DicomViewerService* dicomViewerService() const;
    BoneSegmentationService* segmentationService() const;
    FourViewDisplayService* fourViewDisplayService() const;
    OpticalTrackingService* opticalTrackingService() const;
    PointRegistrationService* pointRegistrationService(bool tryStartPlugin = false) const;
    void clearTrackingRuntimeState(bool disconnectDevice);
    void resetProbeCalibrationState();
    void startProbeCalibration();
    void captureProbeCalibrationPoint();
    void finishProbeCalibration();
    void cancelProbeCalibration();
    void updateProbeCalibrationUi();
    bool tryBuildNavigationConfidenceInputs(NavigationConfidenceInputs& inputs) const;
    void refreshNavigationConfidenceState(bool showWarnings = false);
    void persistEvaluationReportSnapshot(const QVariantMap& trackingQuality = {}, bool exportMetricsCsv = false);
    void performLoadDicom();
    void performComputeRegistration();
    void performStartNavigation();

    Ui::NavigationPage* ui;
    NavigationPageServiceAccess* m_serviceAccess;
    LegacyNavigationPageServiceAdapter* m_ownedServiceAdapter;
    std::unique_ptr<NavigationWorkflowContext> m_workflowContext;
    std::unique_ptr<NavigationServiceBundle> m_serviceBundle;
    std::unique_ptr<PreparationPlanningController> m_preparationPlanningController;
    std::unique_ptr<RegistrationController> m_registrationController;
    std::unique_ptr<NavigationEvaluationController> m_navigationEvaluationController;
    std::unique_ptr<NavigationWorkflowCoordinator> m_workflowCoordinator;
    std::unique_ptr<EmbeddedVtkViewHost> m_planningVtkHost;
    std::unique_ptr<EmbeddedVtkViewHost> m_navigationVtkHost;
    std::unique_ptr<EmbeddedVtkViewHost> m_registrationVtkHost;
    std::unique_ptr<NavigationVtkBridge> m_navigationVtkBridge;
    bool m_trackerConnected;
    bool m_navigationActive;
    QString m_lastDicomDirPath;
    QPointer<QWidget> m_fourViewWidget;
    QTimer* m_trackerTimer;
    QString m_currentSegmentationTaskId;
    QString m_lastSegmentationTaskId;
    QString m_lastSegmentationOutputDir;
    bool m_modelVisible;
    QString m_lastLoadedModelPath;
    QPointer<QWidget> m_registrationVTKWidget;
    int m_selectedPointIndex;
    int m_selectedInstrumentId;
    Navigation3DViewWidget* m_navigation3DView;
    BoneSurfaceMotionSimulator* m_motionSimulator;
    QTimer* m_navigationTimer;
    QMatrix4x4 m_registrationTransform;
    RegistrationWorkflow* m_registrationWorkflow;
    NavigationConfidenceEvaluator m_confidenceEvaluator;
    NavigationConfidenceResult m_lastConfidence;
    QString m_trackingSessionId;
    QString m_navigationToolId;
    QString m_activeCalibrationId;
    int m_activeCalibrationRequiredPoints;
    int m_activeCalibrationCollectedPoints;
};

#endif // NAVIGATIONPAGE_NEW_H
