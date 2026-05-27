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
    QVariantMap registrationInfo;
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
    QVariantMap getRegistrationInfo(const QString&) const override { return registrationInfo; }
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
    void execute_registration_respects_single_stage_landmark_method();
    void execute_registration_respects_global_icp_method();
    void execute_registration_respects_global_gicp_method();
    void execute_registration_respects_two_stage_constrained_method();
    void execute_registration_uses_planned_constraint_regions_to_limit_two_stage_refinement();
    void execute_registration_emits_planning_constraint_context_metrics();
    void execute_registration_surfaces_parallel_search_metrics_from_registration_core();
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

void PointRegistrationRegistrationCoreIntegrationTest::execute_registration_respects_single_stage_landmark_method()
{
    PlatformServiceRegistry registry;
    FakeRegistrationService registrationService;
    registry.registerService(QStringLiteral("RegistrationCore"), QStringLiteral("RegistrationService"), &registrationService);

    PointRegistrationServiceImpl service;
    service.setServiceRegistry(&registry);
    RegistrationWorkflow workflow(&service);

    PointRegistrationExecutionOptions options;
    options.registrationMethodId = QStringLiteral("single_stage_landmark");
    workflow.setExecutionOptions(options);

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

    QCOMPARE(registrationService.advancedCallCount, 0);
    QCOMPARE(workflow.getLastResult().metrics.value(QStringLiteral("registration_mode")).toString(),
             QStringLiteral("single_stage_landmark"));
    QCOMPARE(workflow.getLastResult().metrics.value(QStringLiteral("refine_method")).toString(),
             QStringLiteral("weighted_landmark_only"));
}

void PointRegistrationRegistrationCoreIntegrationTest::execute_registration_respects_global_icp_method()
{
    PlatformServiceRegistry registry;
    FakeRegistrationService registrationService;
    registry.registerService(QStringLiteral("RegistrationCore"), QStringLiteral("RegistrationService"), &registrationService);

    PointRegistrationServiceImpl service;
    service.setServiceRegistry(&registry);
    RegistrationWorkflow workflow(&service);

    PointRegistrationExecutionOptions options;
    options.registrationMethodId = QStringLiteral("landmark_plus_global_icp");
    workflow.setExecutionOptions(options);

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

    QCOMPARE(registrationService.advancedCallCount, 1);
    QCOMPARE(registrationService.lastParameters.value(QStringLiteral("useGPU")).toBool(), false);
    QCOMPARE(workflow.getLastResult().metrics.value(QStringLiteral("registration_mode")).toString(),
             QStringLiteral("landmark_plus_global_icp"));
    QCOMPARE(workflow.getLastResult().metrics.value(QStringLiteral("refine_method")).toString(),
             QStringLiteral("registration_core_cpu_icp"));
}

void PointRegistrationRegistrationCoreIntegrationTest::execute_registration_respects_global_gicp_method()
{
    PlatformServiceRegistry registry;
    FakeRegistrationService registrationService;
    registry.registerService(QStringLiteral("RegistrationCore"), QStringLiteral("RegistrationService"), &registrationService);

    PointRegistrationServiceImpl service;
    service.setServiceRegistry(&registry);
    RegistrationWorkflow workflow(&service);

    PointRegistrationExecutionOptions options;
    options.registrationMethodId = QStringLiteral("landmark_plus_global_gicp");
    workflow.setExecutionOptions(options);

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

    QCOMPARE(registrationService.advancedCallCount, 1);
    QCOMPARE(registrationService.lastParameters.value(QStringLiteral("useGPU")).toBool(), true);
    QCOMPARE(registrationService.lastParameters.value(QStringLiteral("registrationMethodId")).toString(),
             QStringLiteral("landmark_plus_global_gicp"));
    QCOMPARE(workflow.getLastResult().metrics.value(QStringLiteral("registration_mode")).toString(),
             QStringLiteral("landmark_plus_global_gicp"));
    QCOMPARE(workflow.getLastResult().metrics.value(QStringLiteral("refine_method")).toString(),
             QStringLiteral("registration_core_gpu_gicp"));
}

void PointRegistrationRegistrationCoreIntegrationTest::execute_registration_respects_two_stage_constrained_method()
{
    PlatformServiceRegistry registry;
    FakeRegistrationService registrationService;
    registry.registerService(QStringLiteral("RegistrationCore"), QStringLiteral("RegistrationService"), &registrationService);

    PointRegistrationServiceImpl service;
    service.setServiceRegistry(&registry);
    RegistrationWorkflow workflow(&service);

    PointRegistrationExecutionOptions options;
    options.registrationMethodId = QStringLiteral("ankle_two_stage_constrained");
    workflow.setExecutionOptions(options);

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

    QCOMPARE(registrationService.advancedCallCount, 1);
    QCOMPARE(registrationService.lastParameters.value(QStringLiteral("useGPU")).toBool(), true);
    QCOMPARE(registrationService.lastParameters.value(QStringLiteral("registrationMethodId")).toString(),
             QStringLiteral("ankle_two_stage_constrained"));
    QCOMPARE(workflow.getLastResult().metrics.value(QStringLiteral("registration_mode")).toString(),
             QStringLiteral("ankle_two_stage_constrained"));
    QCOMPARE(workflow.getLastResult().metrics.value(QStringLiteral("refine_method")).toString(),
             QStringLiteral("registration_core_gpu_gicp"));
}

void PointRegistrationRegistrationCoreIntegrationTest::execute_registration_uses_planned_constraint_regions_to_limit_two_stage_refinement()
{
    PlatformServiceRegistry registry;
    FakeRegistrationService registrationService;
    registry.registerService(QStringLiteral("RegistrationCore"), QStringLiteral("RegistrationService"), &registrationService);

    PointRegistrationServiceImpl service;
    service.setServiceRegistry(&registry);
    RegistrationWorkflow workflow(&service);

    TargetRegistrationRegion region;
    region.origin = QVector3D(5.0f, 5.0f, 0.0f);
    region.primaryAxis = QVector3D(0.0f, 0.0f, 1.0f);
    region.radiusMm = 12.0;
    workflow.setTargetRegistrationRegion(region);

    QMap<QString, QList<QVector3D>> constraintRegions;
    constraintRegions.insert(
        QStringLiteral("tibia_distal_region"),
        QList<QVector3D> {
            QVector3D(0.0f, 0.0f, 0.0f),
            QVector3D(10.0f, 0.0f, 0.0f),
            QVector3D(0.0f, 10.0f, 0.0f)
        });
    workflow.setPlanningConstraintRegions(constraintRegions);

    QVariantMap metadata;
    metadata.insert(QStringLiteral("constraint_region_count"), 1);
    metadata.insert(QStringLiteral("constraint_region_keys"), QStringLiteral("tibia_distal_region"));
    workflow.setPlanningConstraintContext(metadata);

    PointRegistrationExecutionOptions options;
    options.registrationMethodId = QStringLiteral("ankle_two_stage_constrained");
    workflow.setExecutionOptions(options);

    QVERIFY(service.loadModelFromPolyData(createTriangleModel(), QStringLiteral("ankle-model")));

    const int point0 = service.addPoint(QStringLiteral("p0"));
    const int point1 = service.addPoint(QStringLiteral("p1"));
    const int point2 = service.addPoint(QStringLiteral("p2"));
    const int point3 = service.addPoint(QStringLiteral("p3"));

    QVERIFY(service.setSourcePosition(point0, QVector3D(0.0f, 0.0f, 0.0f)));
    QVERIFY(service.setSourcePosition(point1, QVector3D(10.0f, 0.0f, 0.0f)));
    QVERIFY(service.setSourcePosition(point2, QVector3D(0.0f, 10.0f, 0.0f)));
    QVERIFY(service.setSourcePosition(point3, QVector3D(60.0f, 60.0f, 0.0f)));

    QVERIFY(service.setTargetPosition(point0, QVector3D(5.0f, 3.0f, 0.0f)));
    QVERIFY(service.setTargetPosition(point1, QVector3D(15.0f, 3.0f, 0.0f)));
    QVERIFY(service.setTargetPosition(point2, QVector3D(5.0f, 13.0f, 0.0f)));
    QVERIFY(service.setTargetPosition(point3, QVector3D(65.0f, 63.0f, 0.0f)));

    QVERIFY(workflow.executeRegistration());

    QCOMPARE(registrationService.advancedCallCount, 1);
    QCOMPARE(registrationService.lastSourcePointCount, vtkIdType(3));
    QCOMPARE(registrationService.lastParameters.value(QStringLiteral("constrainedPointCount")).toInt(), 3);
    QCOMPARE(registrationService.lastParameters.value(QStringLiteral("constraintRegionCount")).toInt(), 1);
    QCOMPARE(registrationService.lastParameters.value(QStringLiteral("curvatureWeightMode")).toInt(), 4);
    QCOMPARE(workflow.getLastResult().metrics.value(QStringLiteral("constraint_refine_pair_count")).toInt(), 3);
    QCOMPARE(workflow.getLastResult().metrics.value(QStringLiteral("constraint_refine_used")).toBool(), true);
}

void PointRegistrationRegistrationCoreIntegrationTest::execute_registration_emits_planning_constraint_context_metrics()
{
    PlatformServiceRegistry registry;
    FakeRegistrationService registrationService;
    registry.registerService(QStringLiteral("RegistrationCore"), QStringLiteral("RegistrationService"), &registrationService);

    PointRegistrationServiceImpl service;
    service.setServiceRegistry(&registry);
    RegistrationWorkflow workflow(&service);

    TargetRegistrationRegion region;
    region.origin = QVector3D(35.0f, 5.0f, 5.0f);
    region.primaryAxis = QVector3D(0.0f, 0.0f, 1.0f);
    region.radiusMm = 15.0;
    workflow.setTargetRegistrationRegion(region);

    QVariantMap metadata;
    metadata.insert(QStringLiteral("constraint_region_count"), 2);
    metadata.insert(QStringLiteral("constraint_region_keys"), QStringLiteral("tibia_distal_region|talus_dome_region"));
    metadata.insert(QStringLiteral("constraint_region_bones"), QStringLiteral("tibia|talus"));
    metadata.insert(QStringLiteral("constraint_region_roles"), QStringLiteral("distal_region|dome_region"));
    metadata.insert(QStringLiteral("constraint_region_source"), QStringLiteral("planning_json"));
    metadata.insert(QStringLiteral("constraint_region_version"), QStringLiteral("1.1"));
    workflow.setPlanningConstraintContext(metadata);

    PointRegistrationExecutionOptions options;
    options.registrationMethodId = QStringLiteral("ankle_two_stage_constrained");
    workflow.setExecutionOptions(options);

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

    const QVariantMap metrics = workflow.getLastResult().metrics;
    QCOMPARE(metrics.value(QStringLiteral("target_region_radius_mm")).toDouble(), 15.0);
    QCOMPARE(metrics.value(QStringLiteral("target_region_origin_x")).toDouble(), 35.0);
    QCOMPARE(metrics.value(QStringLiteral("constraint_region_count")).toInt(), 2);
    QCOMPARE(metrics.value(QStringLiteral("constraint_region_keys")).toString(), QStringLiteral("tibia_distal_region|talus_dome_region"));
    QCOMPARE(metrics.value(QStringLiteral("constraint_region_bones")).toString(), QStringLiteral("tibia|talus"));
    QCOMPARE(metrics.value(QStringLiteral("constraint_region_roles")).toString(), QStringLiteral("distal_region|dome_region"));
    QCOMPARE(metrics.value(QStringLiteral("constraint_region_source")).toString(), QStringLiteral("planning_json"));
    QCOMPARE(metrics.value(QStringLiteral("constraint_region_version")).toString(), QStringLiteral("1.1"));
}

void PointRegistrationRegistrationCoreIntegrationTest::execute_registration_surfaces_parallel_search_metrics_from_registration_core()
{
    PlatformServiceRegistry registry;
    FakeRegistrationService registrationService;
    registrationService.registrationInfo.insert(
        QStringLiteral("metadata"),
        QVariantMap{
            { QStringLiteral("parallelSearchEnabled"), true },
            { QStringLiteral("candidateCount"), 48 },
            { QStringLiteral("topKCount"), 6 },
            { QStringLiteral("coarseSearchMs"), 12.5 },
            { QStringLiteral("roiFilterMs"), 1.75 },
            { QStringLiteral("elapsedMs"), 8.5 },
            { QStringLiteral("bestCandidateRank"), 2 },
            { QStringLiteral("coarseScore"), 0.91 },
            { QStringLiteral("multiResolutionProfile"), QStringLiteral("ankle_roi_two_level") }
        });
    registry.registerService(QStringLiteral("RegistrationCore"), QStringLiteral("RegistrationService"), &registrationService);

    PointRegistrationServiceImpl service;
    service.setServiceRegistry(&registry);
    RegistrationWorkflow workflow(&service);

    PointRegistrationExecutionOptions options;
    options.registrationMethodId = QStringLiteral("ankle_two_stage_constrained");
    options.candidateCount = 48;
    options.topKCandidateCount = 6;
    options.enableParallelInitialSearch = true;
    options.enableConstraintParallelFilter = true;
    options.multiResolutionProfileId = QStringLiteral("ankle_roi_two_level");
    workflow.setExecutionOptions(options);

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

    QCOMPARE(registrationService.lastParameters.value(QStringLiteral("enableParallelInitialSearch")).toBool(), true);
    QCOMPARE(registrationService.lastParameters.value(QStringLiteral("enableConstraintParallelFilter")).toBool(), true);
    QCOMPARE(registrationService.lastParameters.value(QStringLiteral("candidateCount")).toInt(), 48);
    QCOMPARE(registrationService.lastParameters.value(QStringLiteral("topKCandidateCount")).toInt(), 6);
    QCOMPARE(registrationService.lastParameters.value(QStringLiteral("multiResolutionProfileId")).toString(),
             QStringLiteral("ankle_roi_two_level"));

    const QVariantMap metrics = workflow.getLastResult().metrics;
    QCOMPARE(metrics.value(QStringLiteral("candidate_count")).toInt(), 48);
    QCOMPARE(metrics.value(QStringLiteral("top_k_count")).toInt(), 6);
    QCOMPARE(metrics.value(QStringLiteral("coarse_search_ms")).toDouble(), 12.5);
    QCOMPARE(metrics.value(QStringLiteral("roi_filter_ms")).toDouble(), 1.75);
    QCOMPARE(metrics.value(QStringLiteral("refine_ms")).toDouble(), 8.5);
    QCOMPARE(metrics.value(QStringLiteral("best_candidate_rank")).toInt(), 2);
    QCOMPARE(metrics.value(QStringLiteral("coarse_score")).toDouble(), 0.91);
    QCOMPARE(metrics.value(QStringLiteral("parallel_search_enabled")).toBool(), true);
    QCOMPARE(metrics.value(QStringLiteral("multi_resolution_profile")).toString(),
             QStringLiteral("ankle_roi_two_level"));
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
