#include <QtTest/QtTest>

#include <vtkCellArray.h>
#include <vtkMatrix4x4.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkTriangle.h>

#include "Framework/Platform/Kernel/platform_service_registry.h"
#include "Plugins/PointRegistration/RegistrationWorkflow.h"
#include "Plugins/PointRegistration/PointRegistrationServiceImpl.h"
#include "Plugins/RegistrationCore/RegistrationService.h"

class FakeRegistrationService final : public registration_core::RegistrationService
{
    Q_OBJECT

public:
    int advancedCallCount = 0;
    QVariantMap lastParameters;
    vtkIdType lastSourcePointCount = 0;
    vtkIdType lastTargetPointCount = 0;

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
        vtkPolyData* source,
        vtkPolyData* target,
        const QVariantMap& parameters) override
    {
        ++advancedCallCount;
        lastSourcePointCount = source ? source->GetNumberOfPoints() : 0;
        lastTargetPointCount = target ? target->GetNumberOfPoints() : 0;
        lastParameters = parameters;

        auto refinedTrackerToCt = vtkSmartPointer<vtkMatrix4x4>::New();
        refinedTrackerToCt->Identity();
        refinedTrackerToCt->SetElement(0, 3, -5.0);
        refinedTrackerToCt->SetElement(1, 3, -3.0);
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
    vtkSmartPointer<vtkMatrix4x4> invertMatrix(vtkMatrix4x4* matrix) override
    {
        auto inverted = vtkSmartPointer<vtkMatrix4x4>::New();
        vtkMatrix4x4::Invert(matrix, inverted);
        return inverted;
    }
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

class PointRegistrationRegistrationCoreIntegrationTest : public QObject
{
    Q_OBJECT

private slots:
    void execute_registration_delegates_gpu_refinement_to_registration_core();
    void mutating_points_invalidates_previous_registration_state();
    void failed_reregistration_clears_previous_registration_state();

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

void PointRegistrationRegistrationCoreIntegrationTest::execute_registration_delegates_gpu_refinement_to_registration_core()
{
    PlatformServiceRegistry registry;
    FakeRegistrationService registrationService;
    registry.registerService(QStringLiteral("RegistrationCore"), QStringLiteral("RegistrationService"), &registrationService);

    PointRegistrationServiceImpl service;
    service.setServiceRegistry(&registry);

    QVERIFY(service.loadModelFromPolyData(createTriangleModel(), QStringLiteral("ankle-model")));

    const int point0 = service.addPoint(QStringLiteral("p0"));
    const int point1 = service.addPoint(QStringLiteral("p1"));
    const int point2 = service.addPoint(QStringLiteral("p2"));

    QVERIFY(service.setSourcePosition(point0, QVector3D(0.0f, 0.0f, 0.0f)));
    QVERIFY(service.setSourcePosition(point1, QVector3D(10.0f, 0.0f, 0.0f)));
    QVERIFY(service.setSourcePosition(point2, QVector3D(0.0f, 10.0f, 0.0f)));

    QVERIFY(service.setTargetPosition(point0, QVector3D(5.0f, 3.0f, 0.0f)));
    QVERIFY(service.setTargetPosition(point1, QVector3D(15.0f, 3.0f, 0.0f)));
    QVERIFY(service.setTargetPosition(point2, QVector3D(5.0f, 13.0f, 0.0f)));

    const PointRegistrationResult result = service.executeRegistration();

    QVERIFY(result.success);
    QCOMPARE(registrationService.advancedCallCount, 1);
    QCOMPARE(registrationService.lastSourcePointCount, vtkIdType(3));
    QCOMPARE(registrationService.lastTargetPointCount, vtkIdType(3));
    QCOMPARE(registrationService.lastParameters.value(QStringLiteral("useGPU")).toBool(), true);
    QVERIFY(registrationService.lastParameters.contains(QStringLiteral("initialTransform")));
    QCOMPARE(result.metrics.value(QStringLiteral("refine_method")).toString(),
             QStringLiteral("registration_core_gpu_gicp"));
    QVERIFY(qAbs(result.translationX - 5.0) < 0.1);
    QVERIFY(qAbs(result.translationY - 3.0) < 0.1);
}

void PointRegistrationRegistrationCoreIntegrationTest::mutating_points_invalidates_previous_registration_state()
{
    PlatformServiceRegistry registry;
    FakeRegistrationService registrationService;
    registry.registerService(QStringLiteral("RegistrationCore"), QStringLiteral("RegistrationService"), &registrationService);

    PointRegistrationServiceImpl service;
    service.setServiceRegistry(&registry);
    RegistrationWorkflow workflow(&service);

    QVERIFY(service.loadModelFromPolyData(createTriangleModel(), QStringLiteral("ankle-model")));

    const int point0 = service.addPoint(QStringLiteral("p0"));
    const int point1 = service.addPoint(QStringLiteral("p1"));
    const int point2 = service.addPoint(QStringLiteral("p2"));

    QVERIFY(service.setSourcePosition(point0, QVector3D(0.0f, 0.0f, 0.0f)));
    QVERIFY(service.setSourcePosition(point1, QVector3D(10.0f, 0.0f, 0.0f)));
    QVERIFY(service.setSourcePosition(point2, QVector3D(0.0f, 10.0f, 0.0f)));

    QVERIFY(service.setTargetPosition(point0, QVector3D(5.0f, 3.0f, 0.0f)));
    QVERIFY(service.setTargetPosition(point1, QVector3D(15.0f, 3.0f, 0.0f)));
    QVERIFY(service.setTargetPosition(point2, QVector3D(5.0f, 13.0f, 0.0f)));

    QVERIFY(workflow.executeRegistration());
    QVERIFY(workflow.getLastResult().success);
    QVERIFY(service.getLastResult().success);
    QVERIFY(!service.getTransformMatrix().isIdentity());

    QVERIFY(service.setSourcePosition(point0, QVector3D(1.0f, 0.0f, 0.0f)));

    QVERIFY(!workflow.getLastResult().success);
    QVERIFY(!service.getLastResult().success);
    QVERIFY(service.getTransformMatrix().isIdentity());
}

void PointRegistrationRegistrationCoreIntegrationTest::failed_reregistration_clears_previous_registration_state()
{
    PlatformServiceRegistry registry;
    FakeRegistrationService registrationService;
    registry.registerService(QStringLiteral("RegistrationCore"), QStringLiteral("RegistrationService"), &registrationService);

    PointRegistrationServiceImpl service;
    service.setServiceRegistry(&registry);
    RegistrationWorkflow workflow(&service);

    QVERIFY(service.loadModelFromPolyData(createTriangleModel(), QStringLiteral("ankle-model")));

    const int point0 = service.addPoint(QStringLiteral("p0"));
    const int point1 = service.addPoint(QStringLiteral("p1"));
    const int point2 = service.addPoint(QStringLiteral("p2"));

    QVERIFY(service.setSourcePosition(point0, QVector3D(0.0f, 0.0f, 0.0f)));
    QVERIFY(service.setSourcePosition(point1, QVector3D(10.0f, 0.0f, 0.0f)));
    QVERIFY(service.setSourcePosition(point2, QVector3D(0.0f, 10.0f, 0.0f)));

    QVERIFY(service.setTargetPosition(point0, QVector3D(5.0f, 3.0f, 0.0f)));
    QVERIFY(service.setTargetPosition(point1, QVector3D(15.0f, 3.0f, 0.0f)));
    QVERIFY(service.setTargetPosition(point2, QVector3D(5.0f, 13.0f, 0.0f)));

    QVERIFY(workflow.executeRegistration());
    QVERIFY(workflow.getLastResult().success);
    QVERIFY(service.getLastResult().success);
    QVERIFY(!service.getTransformMatrix().isIdentity());

    QVERIFY(service.removePoint(point2));
    QVERIFY(!workflow.executeRegistration());

    QVERIFY(!workflow.getLastResult().success);
    QVERIFY(!service.getLastResult().success);
    QVERIFY(service.getTransformMatrix().isIdentity());
}

QTEST_MAIN(PointRegistrationRegistrationCoreIntegrationTest)
#include "PointRegistrationRegistrationCoreIntegrationTest.moc"
