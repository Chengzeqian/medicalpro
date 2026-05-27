#include <QtTest/QtTest>

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

#include "Plugins/PointRegistration/PointRegistrationService.h"
#include "Plugins/PointRegistration/widgets/PointRegistrationWidget.h"

class FakePointRegistrationWidgetService : public PointRegistrationService
{
    Q_OBJECT

public:
    explicit FakePointRegistrationWidgetService(QObject* parent = nullptr)
        : PointRegistrationService(parent)
    {
        m_options.candidateCount = 48;
        m_options.topKCandidateCount = 6;
        m_options.enableParallelInitialSearch = true;
        m_options.enableConstraintParallelFilter = false;
        m_options.multiResolutionProfileId = QStringLiteral("ankle_roi_two_level");
    }

    int addPoint(const QString& name = QString()) override
    {
        Q_UNUSED(name);
        return 0;
    }

    bool removePoint(int index) override
    {
        Q_UNUSED(index);
        return false;
    }

    void clearPoints() override {}

    int pointCount() const override
    {
        return 0;
    }

    RegistrationPoint getPoint(int index) const override
    {
        Q_UNUSED(index);
        return {};
    }

    QVector<RegistrationPoint> getAllPoints() const override
    {
        return {};
    }

    bool setSourcePosition(int index, const QVector3D& position) override
    {
        Q_UNUSED(index);
        Q_UNUSED(position);
        return false;
    }

    bool setTargetPosition(int index, const QVector3D& position) override
    {
        Q_UNUSED(index);
        Q_UNUSED(position);
        return false;
    }

    bool setPointName(int index, const QString& name) override
    {
        Q_UNUSED(index);
        Q_UNUSED(name);
        return false;
    }

    void setTransformMode(TransformMode mode) override
    {
        m_transformMode = mode;
    }

    TransformMode getTransformMode() const override
    {
        return m_transformMode;
    }

    PointRegistrationResult executeRegistration() override
    {
        m_lastResult.success = true;
        m_lastResult.rmsError = 0.42;
        m_lastResult.maxError = 0.88;
        m_lastResult.metrics.insert(QStringLiteral("candidate_count"), m_options.candidateCount);
        m_lastResult.metrics.insert(QStringLiteral("top_k_count"), m_options.topKCandidateCount);
        m_lastResult.metrics.insert(QStringLiteral("coarse_search_ms"), 12.5);
        m_lastResult.metrics.insert(QStringLiteral("best_candidate_rank"), 2);
        m_lastResult.metrics.insert(QStringLiteral("parallel_search_enabled"), m_options.enableParallelInitialSearch);
        m_lastResult.metrics.insert(QStringLiteral("multi_resolution_profile"), m_options.multiResolutionProfileId);
        emit registrationCompleted(m_lastResult);
        return m_lastResult;
    }

    bool canExecuteRegistration() const override
    {
        return true;
    }

    PointRegistrationResult getLastResult() const override
    {
        return m_lastResult;
    }

    QMatrix4x4 getTransformMatrix() const override
    {
        return {};
    }

    QVector3D transformPoint(const QVector3D& point) const override
    {
        return point;
    }

    bool loadModelFromSegmentation(const QString& segmentationTaskId, const QString& bodyPart = QString()) override
    {
        Q_UNUSED(segmentationTaskId);
        Q_UNUSED(bodyPart);
        return false;
    }

    bool loadModelFromPolyData(vtkPolyData* polyData, const QString& modelName = QString()) override
    {
        Q_UNUSED(polyData);
        Q_UNUSED(modelName);
        return false;
    }

    bool loadModelFromFile(const QString& filePath) override
    {
        Q_UNUSED(filePath);
        return false;
    }

    QString getModelInfo() const override
    {
        return {};
    }

    bool hasModel() const override
    {
        return false;
    }

    void setProbePointSource(ProbePointSource source) override
    {
        m_probePointSource = source;
    }

    ProbePointSource getProbePointSource() const override
    {
        return m_probePointSource;
    }

    bool captureProbePoint(int pointIndex) override
    {
        Q_UNUSED(pointIndex);
        return false;
    }

    void setTrackingSession(const QString& sessionId, const QString& probeToolId) override
    {
        Q_UNUSED(sessionId);
        Q_UNUSED(probeToolId);
    }

    QVector3D getCurrentProbePosition() const override
    {
        return {};
    }

    void setExecutionOptions(const PointRegistrationExecutionOptions& options) override
    {
        m_options = options;
    }

    PointRegistrationExecutionOptions executionOptions() const override
    {
        return m_options;
    }

    void setTargetRegistrationRegion(const TargetRegistrationRegion& region) override
    {
        Q_UNUSED(region);
    }

    void setPlanningConstraintContext(const QVariantMap& context) override
    {
        Q_UNUSED(context);
    }

    void setPlanningConstraintRegions(const QMap<QString, QList<QVector3D>>& regions) override
    {
        Q_UNUSED(regions);
    }

    QVector3D generateSimulatedProbePoint(int pointIndex, double noiseLevel = 0.5) override
    {
        Q_UNUSED(pointIndex);
        Q_UNUSED(noiseLevel);
        return {};
    }

    int generateAllSimulatedProbePoints(double noiseLevel = 0.5) override
    {
        Q_UNUSED(noiseLevel);
        return 0;
    }

    void setSimulationTransform(const QMatrix4x4& transform) override
    {
        m_simulationTransform = transform;
    }

    QMatrix4x4 getSimulationTransform() const override
    {
        return m_simulationTransform;
    }

    bool applyRegistrationToNavigation(const QString& registrationId) override
    {
        Q_UNUSED(registrationId);
        return false;
    }

    RegistrationSession getCurrentSession() const override
    {
        return {};
    }

    QWidget* createRegistrationWidget(QWidget* parent = nullptr) override
    {
        return new PointRegistrationWidget(this, parent);
    }

    QWidget* createVTKWidget(QWidget* parent = nullptr) override
    {
        Q_UNUSED(parent);
        return nullptr;
    }

    void pauseRendering() override {}

    void resumeRendering() override {}

    QString getLastError() const override
    {
        return {};
    }

private:
    PointRegistrationExecutionOptions m_options;
    PointRegistrationResult m_lastResult;
    TransformMode m_transformMode = TransformMode::RigidBody;
    ProbePointSource m_probePointSource = ProbePointSource::Manual;
    QMatrix4x4 m_simulationTransform;
};

class PointRegistrationWidgetContractTest : public QObject
{
    Q_OBJECT

private slots:
    void widget_loads_parallel_search_controls_from_service_execution_options();
    void widget_pushes_parallel_search_options_to_service_and_shows_parallel_metrics();
};

void PointRegistrationWidgetContractTest::widget_loads_parallel_search_controls_from_service_execution_options()
{
    FakePointRegistrationWidgetService service;
    PointRegistrationWidget widget(&service);

    auto* candidateSpin = widget.findChild<QSpinBox*>(QStringLiteral("candidate_count_spinbox"));
    auto* topKSpin = widget.findChild<QSpinBox*>(QStringLiteral("top_k_candidate_count_spinbox"));
    auto* parallelCheck = widget.findChild<QCheckBox*>(QStringLiteral("parallel_initial_search_checkbox"));
    auto* constraintCheck = widget.findChild<QCheckBox*>(QStringLiteral("constraint_parallel_filter_checkbox"));
    auto* profileCombo = widget.findChild<QComboBox*>(QStringLiteral("multi_resolution_profile_combo"));

    QVERIFY(candidateSpin != nullptr);
    QVERIFY(topKSpin != nullptr);
    QVERIFY(parallelCheck != nullptr);
    QVERIFY(constraintCheck != nullptr);
    QVERIFY(profileCombo != nullptr);

    QCOMPARE(candidateSpin->value(), 48);
    QCOMPARE(topKSpin->value(), 6);
    QCOMPARE(parallelCheck->isChecked(), true);
    QCOMPARE(constraintCheck->isChecked(), false);
    QCOMPARE(profileCombo->currentData().toString(), QStringLiteral("ankle_roi_two_level"));
}

void PointRegistrationWidgetContractTest::widget_pushes_parallel_search_options_to_service_and_shows_parallel_metrics()
{
    FakePointRegistrationWidgetService service;
    PointRegistrationWidget widget(&service);

    auto* candidateSpin = widget.findChild<QSpinBox*>(QStringLiteral("candidate_count_spinbox"));
    auto* topKSpin = widget.findChild<QSpinBox*>(QStringLiteral("top_k_candidate_count_spinbox"));
    auto* parallelCheck = widget.findChild<QCheckBox*>(QStringLiteral("parallel_initial_search_checkbox"));
    auto* constraintCheck = widget.findChild<QCheckBox*>(QStringLiteral("constraint_parallel_filter_checkbox"));
    auto* profileCombo = widget.findChild<QComboBox*>(QStringLiteral("multi_resolution_profile_combo"));
    auto* startButton = widget.findChild<QPushButton*>(QStringLiteral("start_registration_button"));
    auto* metricsLabel = widget.findChild<QLabel*>(QStringLiteral("parallel_metrics_summary_label"));

    QVERIFY(candidateSpin != nullptr);
    QVERIFY(topKSpin != nullptr);
    QVERIFY(parallelCheck != nullptr);
    QVERIFY(constraintCheck != nullptr);
    QVERIFY(profileCombo != nullptr);
    QVERIFY(startButton != nullptr);
    QVERIFY(metricsLabel != nullptr);

    candidateSpin->setValue(96);
    topKSpin->setValue(8);
    parallelCheck->setChecked(true);
    constraintCheck->setChecked(true);
    profileCombo->setCurrentIndex(profileCombo->findData(QStringLiteral("ankle_roi_three_level")));

    QTest::mouseClick(startButton, Qt::LeftButton);

    const PointRegistrationExecutionOptions options = service.executionOptions();
    QCOMPARE(options.candidateCount, 96);
    QCOMPARE(options.topKCandidateCount, 8);
    QCOMPARE(options.enableParallelInitialSearch, true);
    QCOMPARE(options.enableConstraintParallelFilter, true);
    QCOMPARE(options.multiResolutionProfileId, QStringLiteral("ankle_roi_three_level"));

    QVERIFY(metricsLabel->text().contains(QStringLiteral("96")));
    QVERIFY(metricsLabel->text().contains(QStringLiteral("8")));
    QVERIFY(metricsLabel->text().contains(QStringLiteral("12.5")));
    QVERIFY(metricsLabel->text().contains(QStringLiteral("2")));
    QVERIFY(metricsLabel->text().contains(QStringLiteral("ankle_roi_three_level")));
}

QTEST_MAIN(PointRegistrationWidgetContractTest)
#include "PointRegistrationWidgetContractTest.moc"
