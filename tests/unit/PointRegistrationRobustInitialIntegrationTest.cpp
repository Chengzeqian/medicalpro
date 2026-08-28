#include <QtTest/QtTest>

#include <vtkCellArray.h>
#include <vtkMatrix4x4.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkTriangle.h>

#include "Framework/Platform/Kernel/platform_service_registry.h"
#include "Plugins/OpticalTracking/OpticalTrackingService.h"
#include "Plugins/PointRegistration/PointRegistrationServiceImpl.h"
#include "Plugins/RegistrationCore/RegistrationService.h"

class RobustInitialFakeRegistrationService final : public registration_core::RegistrationService
{
public:
    int advancedCallCount = 0;
    QVariantMap lastParameters;

    vtkSmartPointer<vtkMatrix4x4> performLandmarkRegistration(
        vtkPoints*,
        vtkPoints*,
        const QString&) override
    {
        return nullptr;
    }

    QList<double> performLandmarkRegistrationList(
        const QList<QList<double>>&,
        const QList<QList<double>>&) override
    {
        return {};
    }

    vtkSmartPointer<vtkMatrix4x4> performICPRegistration(
        vtkPolyData*,
        vtkPolyData*,
        vtkMatrix4x4*,
        int,
        const QString&) override
    {
        return nullptr;
    }

    vtkSmartPointer<vtkMatrix4x4> performICPRegistrationAdvanced(
        vtkPolyData*,
        vtkPolyData*,
        const QVariantMap& parameters) override
    {
        ++advancedCallCount;
        lastParameters = parameters;

        auto refinedTrackerToCt = vtkSmartPointer<vtkMatrix4x4>::New();
        refinedTrackerToCt->Identity();
        refinedTrackerToCt->SetElement(0, 3, -12.0);
        refinedTrackerToCt->SetElement(1, 3, 7.0);
        refinedTrackerToCt->SetElement(2, 3, -4.0);
        return refinedTrackerToCt;
    }

    double computeRegistrationError(vtkPoints*, vtkPoints*) override { return 0.0; }
    double computeRegistrationErrorList(const QList<QList<double>>&, const QList<QList<double>>&, const QList<double>&) override { return 0.0; }
    double computeFRE(const QString&) override { return 0.0; }
    double computeTRE(const QString&, const QList<double>&) override { return 0.0; }
    QVariantMap evaluateRegistrationQuality(const QString&) override { return {}; }
    bool saveRegistrationResult(const QString&, vtkMatrix4x4*, const QVariantMap&) override { return true; }
    vtkSmartPointer<vtkMatrix4x4> loadRegistrationResult(const QString&) override { return nullptr; }
    QStringList getRegistrationList() const override { return {}; }
    QVariantMap getRegistrationInfo(const QString&) const override { return {}; }
    bool deleteRegistration(const QString&) override { return false; }
    vtkSmartPointer<vtkMatrix4x4> invertMatrix(vtkMatrix4x4*) override { return nullptr; }
    vtkSmartPointer<vtkMatrix4x4> multiplyMatrix(vtkMatrix4x4*, vtkMatrix4x4*) override { return nullptr; }
    QList<double> transformPoint(const QList<double>&, vtkMatrix4x4*) override { return {}; }
    vtkSmartPointer<vtkPoints> transformPoints(vtkPoints*, vtkMatrix4x4*) override { return nullptr; }
    vtkSmartPointer<vtkMatrix4x4> perform2D3DRegistration(
        const QString&,
        vtkPolyData*,
        vtkMatrix4x4*,
        const QVariantMap&) override
    {
        return nullptr;
    }
    QList<double> matrixToList(vtkMatrix4x4*) override { return {}; }
    vtkSmartPointer<vtkMatrix4x4> listToMatrix(const QList<double>&) override { return nullptr; }
    bool exportMatrix(vtkMatrix4x4*, const QString&, const QString&) override { return false; }
    vtkSmartPointer<vtkMatrix4x4> importMatrix(const QString&) override { return nullptr; }
    QString getLastError() const override { return {}; }
};

class PointRegistrationRobustInitialIntegrationTest : public QObject
{
    Q_OBJECT

private slots:
    void execute_registration_passes_robust_initial_confidence_to_registration_core();
    void optical_tracking_capture_accepts_stable_probe_window();
    void optical_tracking_capture_rejects_unstable_probe_window();

private:
    static vtkSmartPointer<vtkPolyData> createTriangleModel()
    {
        auto points = vtkSmartPointer<vtkPoints>::New();
        points->InsertNextPoint(0.0, 0.0, 0.0);
        points->InsertNextPoint(10.0, 0.0, 0.0);
        points->InsertNextPoint(0.0, 10.0, 0.0);

        auto triangle = vtkSmartPointer<vtkTriangle>::New();
        triangle->GetPointIds()->SetId(0, 0);
        triangle->GetPointIds()->SetId(1, 1);
        triangle->GetPointIds()->SetId(2, 2);

        auto triangles = vtkSmartPointer<vtkCellArray>::New();
        triangles->InsertNextCell(triangle);

        auto polyData = vtkSmartPointer<vtkPolyData>::New();
        polyData->SetPoints(points);
        polyData->SetPolys(triangles);
        return polyData;
    }
};

class StableCaptureFakeTrackingService final : public OpticalTrackingService
{
public:
    struct Frame
    {
        QList<double> position;
        QVariantMap status;
    };

    QVector<Frame> frames;
    int positionCallCount = 0;
    mutable int statusCallCount = 0;

    QList<double> getToolPosition(const QString&, const QString&) override
    {
        if (frames.isEmpty()) {
            return {};
        }

        const int frameIndex = qMin(positionCallCount, frames.size() - 1);
        ++positionCallCount;
        return frames.at(frameIndex).position;
    }

    QVariantMap getToolStatus(const QString&, const QString&) const override
    {
        if (frames.isEmpty()) {
            return {};
        }

        const int frameIndex = qMin(statusCallCount, frames.size() - 1);
        ++statusCallCount;
        return frames.at(frameIndex).status;
    }

    QStringList scanAvailableDevices() override { return {}; }
    bool connectToDevice(const QString&) override { return false; }
    bool disconnectDevice(const QString&) override { return false; }
    bool isDeviceConnected(const QString&) const override { return false; }
    QVariantMap getDeviceInfo(const QString&) const override { return {}; }
    QStringList getConnectedDevices() const override { return {}; }
    bool setDeviceParameters(const QString&, const QVariantMap&) override { return false; }
    QVariantMap getDeviceParameters(const QString&) const override { return {}; }
    QString createTrackingSession(const QString&, const QString&) override { return {}; }
    bool startTracking(const QString&) override { return false; }
    bool stopTracking(const QString&) override { return false; }
    bool pauseTracking(const QString&, bool) override { return false; }
    QString getTrackingStatus(const QString&) const override { return {}; }
    bool closeTrackingSession(const QString&) override { return false; }
    QStringList getActiveSessions() const override { return {}; }
    QVariantMap getSessionInfo(const QString&) const override { return {}; }
    QString addTrackingTool(const QString&, const QString&, const QVariantMap&) override { return {}; }
    bool removeTrackingTool(const QString&, const QString&) override { return false; }
    QStringList getTrackingTools(const QString&) const override { return {}; }
    bool setToolParameters(const QString&, const QString&, const QVariantMap&) override { return false; }
    QString startToolCalibration(const QString&, const QString&, const QString&) override { return {}; }
    bool addCalibrationPoint(const QString&) override { return false; }
    QVariantMap finishCalibration(const QString&) override { return {}; }
    bool cancelCalibration(const QString&) override { return false; }
    QVariantMap getCalibrationStatus(const QString&) const override { return {}; }
    bool applyCalibrationResult(const QString&, const QString&, const QVariantMap&) override { return false; }
    QString startDataRecording(const QString&, const QString&, const QString&) override { return {}; }
    bool stopDataRecording(const QString&) override { return false; }
    bool pauseDataRecording(const QString&, bool) override { return false; }
    QVariantMap getRecordingStatus(const QString&) const override { return {}; }
    QString loadRecordedData(const QString&) override { return {}; }
    bool playbackData(const QString&, qint64) override { return false; }
    bool setReferenceCoordinateSystem(const QString&, const QString&) override { return false; }
    QList<double> getTransformMatrix(const QString&, const QString&, const QString&) override { return {}; }
    QList<double> transformPoint(const QString&, const QList<double>&, const QString&, const QString&) override { return {}; }
    bool enableRealTimeStreaming(const QString&, double) override { return false; }
    bool disableRealTimeStreaming(const QString&) override { return false; }
    QMap<QString, QList<double>> getRealTimeData(const QString&) override { return {}; }
    QVariantMap checkTrackingQuality(const QString&, const QString&) override { return {}; }
    QVariantMap validateToolAccuracy(const QString&, const QString&, const QList<QList<double>>&) override { return {}; }
    QVariantMap getSystemStatusReport(const QString&) override { return {}; }
    QWidget* createTrackingWidget(QWidget*) override { return nullptr; }
    bool showTrackingControlPanel(QWidget*) override { return false; }
    bool showDeviceConfigDialog(QWidget*) override { return false; }
    bool showCalibrationWizardDialog(QWidget*) override { return false; }
    bool showDataRecordingDialog(QWidget*) override { return false; }
    QWidget* createTrackingControlInterface(QWidget*) override { return nullptr; }
    QString exportRecordingData(const QString&, const QString&, const QString&) override { return {}; }
    void pauseRendering() override {}
    void resumeRendering() override {}
    QString getLastError() const override { return {}; }
};

namespace {
QVariantMap goodTrackingStatus(double trackingErrorMm = 0.12)
{
    QVariantMap status;
    status.insert(QStringLiteral("visible"), true);
    status.insert(QStringLiteral("calibrated"), true);
    status.insert(QStringLiteral("quality"), 0.96);
    status.insert(QStringLiteral("trackingErrorMm"), trackingErrorMm);
    status.insert(QStringLiteral("calibrationAccuracy"), trackingErrorMm);
    return status;
}
}

void PointRegistrationRobustInitialIntegrationTest::
    execute_registration_passes_robust_initial_confidence_to_registration_core()
{
    PlatformServiceRegistry registry;
    RobustInitialFakeRegistrationService registrationService;
    registry.registerService(QStringLiteral("RegistrationCore"), QStringLiteral("RegistrationService"), &registrationService);

    PointRegistrationServiceImpl service;
    service.setServiceRegistry(&registry);

    PointRegistrationExecutionOptions options;
    options.registrationMethodId = QStringLiteral("ankle_two_stage_constrained");
    service.setExecutionOptions(options);

    QVERIFY(service.loadModelFromPolyData(createTriangleModel(), QStringLiteral("ankle-model")));

    const int point0 = service.addPoint(QStringLiteral("p0"));
    const int point1 = service.addPoint(QStringLiteral("p1"));
    const int point2 = service.addPoint(QStringLiteral("p2"));
    const int point3 = service.addPoint(QStringLiteral("p3"));

    QVERIFY(service.setSourcePosition(point0, QVector3D(0.0f, 0.0f, 0.0f)));
    QVERIFY(service.setSourcePosition(point1, QVector3D(40.0f, 0.0f, 0.0f)));
    QVERIFY(service.setSourcePosition(point2, QVector3D(0.0f, 35.0f, 0.0f)));
    QVERIFY(service.setSourcePosition(point3, QVector3D(0.0f, 0.0f, 30.0f)));

    QVERIFY(service.setTargetPosition(point0, QVector3D(12.0f, -7.0f, 4.0f)));
    QVERIFY(service.setTargetPosition(point1, QVector3D(52.0f, -7.0f, 4.0f)));
    QVERIFY(service.setTargetPosition(point2, QVector3D(12.0f, 28.0f, 4.0f)));
    QVERIFY(service.setTargetPosition(point3, QVector3D(30.0f, -29.0f, 45.0f)));

    const PointRegistrationResult result = service.executeRegistration();

    QVERIFY(result.success);
    QCOMPARE(registrationService.advancedCallCount, 1);
    QCOMPARE(registrationService.lastParameters.value(QStringLiteral("robustInitialInlierCount")).toInt(), 3);
    QCOMPARE(registrationService.lastParameters.value(QStringLiteral("robustInitialOutlierCount")).toInt(), 1);
    QVERIFY(registrationService.lastParameters.value(QStringLiteral("robustInitialConfidence")).toDouble() > 0.70);
    QVERIFY(registrationService.lastParameters.value(QStringLiteral("robustInitialRmsMm")).toDouble() < 0.05);
    QCOMPARE(result.metrics.value(QStringLiteral("initial_inlier_count")).toInt(), 3);
    QCOMPARE(result.metrics.value(QStringLiteral("initial_outlier_count")).toInt(), 1);
    QVERIFY(result.metrics.value(QStringLiteral("initial_confidence")).toDouble() > 0.70);
    QVERIFY(result.metrics.value(QStringLiteral("initial_rms_mm")).toDouble() < 0.05);
}

void PointRegistrationRobustInitialIntegrationTest::optical_tracking_capture_accepts_stable_probe_window()
{
    StableCaptureFakeTrackingService trackingService;
    trackingService.frames = {
        {{24.00, -13.00, 8.00, 0.0, 0.0, 0.0}, goodTrackingStatus()},
        {{24.08, -13.04, 8.02, 0.0, 0.0, 0.0}, goodTrackingStatus()},
        {{23.96, -12.98, 8.01, 0.0, 0.0, 0.0}, goodTrackingStatus()},
        {{24.02, -13.01, 7.98, 0.0, 0.0, 0.0}, goodTrackingStatus()},
        {{24.05, -12.95, 8.03, 0.0, 0.0, 0.0}, goodTrackingStatus()},
        {{23.99, -13.03, 7.99, 0.0, 0.0, 0.0}, goodTrackingStatus()}
    };

    PointRegistrationServiceImpl service;
    service.setTrackingService(&trackingService);
    service.setTrackingSession(QStringLiteral("session-1"), QStringLiteral("probe-1"));
    service.setProbePointSource(ProbePointSource::OpticalTracking);

    const int pointIndex = service.addPoint(QStringLiteral("probe-point"));
    QVERIFY(service.captureProbePoint(pointIndex));

    const RegistrationPoint point = service.getPoint(pointIndex);
    QVERIFY(point.hasTarget);
    QVERIFY(qAbs(point.targetPosition.x() - 24.01f) < 0.08f);
    QVERIFY(qAbs(point.targetPosition.y() + 13.005f) < 0.08f);
    QVERIFY(qAbs(point.targetPosition.z() - 8.005f) < 0.08f);
    QVERIFY(trackingService.positionCallCount >= 5);
    QVERIFY(trackingService.statusCallCount >= 5);
}

void PointRegistrationRobustInitialIntegrationTest::optical_tracking_capture_rejects_unstable_probe_window()
{
    StableCaptureFakeTrackingService trackingService;
    trackingService.frames = {
        {{24.0, -13.0, 8.0, 0.0, 0.0, 0.0}, goodTrackingStatus()},
        {{27.4, -11.8, 8.6, 0.0, 0.0, 0.0}, goodTrackingStatus()},
        {{21.2, -14.6, 7.4, 0.0, 0.0, 0.0}, goodTrackingStatus()},
        {{25.8, -10.9, 9.1, 0.0, 0.0, 0.0}, goodTrackingStatus()},
        {{22.7, -15.2, 7.7, 0.0, 0.0, 0.0}, goodTrackingStatus()},
        {{28.1, -12.2, 8.8, 0.0, 0.0, 0.0}, goodTrackingStatus()}
    };

    PointRegistrationServiceImpl service;
    service.setTrackingService(&trackingService);
    service.setTrackingSession(QStringLiteral("session-1"), QStringLiteral("probe-1"));
    service.setProbePointSource(ProbePointSource::OpticalTracking);

    const int pointIndex = service.addPoint(QStringLiteral("probe-point"));
    QVERIFY(!service.captureProbePoint(pointIndex));
    QVERIFY(service.getLastError().contains(QStringLiteral("jitter")));
    QVERIFY(!service.getPoint(pointIndex).hasTarget);
}

QTEST_APPLESS_MAIN(PointRegistrationRobustInitialIntegrationTest)
#include "PointRegistrationRobustInitialIntegrationTest.moc"
