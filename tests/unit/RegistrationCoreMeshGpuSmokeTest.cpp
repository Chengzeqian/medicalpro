#include <QtTest/QtTest>

#include <QApplication>
#include <QCoreApplication>
#include <QFileInfo>
#include <QLibrary>
#include <QVariantMap>

#include <vtkCell.h>
#include <vtkCubeSource.h>
#include <vtkDataArray.h>
#include <vtkMatrix4x4.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataNormals.h>
#include <vtkSmartPointer.h>
#include <vtkTriangleFilter.h>

#include "algorithms/meshgpu/include/mesh_gpu_runtime_api.h"
#include "Plugins/RegistrationCore/RegistrationServiceImpl.h"

class RegistrationCoreMeshGpuSmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void runtime_output_contains_meshgpu_dll();
    void registration_service_loads_meshgpu_dll_from_runtime_output();
    void advanced_icp_with_gpu_records_gicp_registration_when_runtime_is_available();
    void advanced_icp_with_constraint_payload_records_core_constraint_usage();
    void advanced_icp_parallel_search_records_parallel_search_metadata();
    void advanced_icp_parallel_search_with_constraints_records_parallel_search_metadata();
    void advanced_icp_parallel_search_records_multi_resolution_and_constraint_filter_metrics();
    void candidate_batch_scoring_returns_ranked_scores_from_runtime_api();

private:
    using CreateRuntimeApiFn = mesh_gpu::MeshGPURuntimeApi* (*)();
    using DestroyRuntimeApiFn = void (*)(mesh_gpu::MeshGPURuntimeApi*);

    static vtkSmartPointer<vtkPolyData> createRegistrationSurface(double tx = 0.0, double ty = 0.0, double tz = 0.0)
    {
        auto cube = vtkSmartPointer<vtkCubeSource>::New();
        cube->SetCenter(tx, ty, tz);
        cube->SetXLength(18.0);
        cube->SetYLength(24.0);
        cube->SetZLength(12.0);
        cube->Update();

        auto triangleFilter = vtkSmartPointer<vtkTriangleFilter>::New();
        triangleFilter->SetInputConnection(cube->GetOutputPort());
        triangleFilter->Update();

        auto normals = vtkSmartPointer<vtkPolyDataNormals>::New();
        normals->SetInputConnection(triangleFilter->GetOutputPort());
        normals->ComputePointNormalsOn();
        normals->ComputeCellNormalsOn();
        normals->SplittingOff();
        normals->ConsistencyOn();
        normals->Update();

        auto surface = vtkSmartPointer<vtkPolyData>::New();
        surface->DeepCopy(normals->GetOutput());
        return surface;
    }

    static std::vector<mesh_gpu::Point3D> extractPoints(vtkPolyData* surface)
    {
        std::vector<mesh_gpu::Point3D> points;
        points.reserve(static_cast<size_t>(surface->GetNumberOfPoints()));

        for (vtkIdType i = 0; i < surface->GetNumberOfPoints(); ++i) {
            double point[3];
            surface->GetPoint(i, point);
            points.emplace_back(
                static_cast<float>(point[0]),
                static_cast<float>(point[1]),
                static_cast<float>(point[2]));
        }

        return points;
    }

    static std::vector<mesh_gpu::Normal3D> extractNormals(vtkPolyData* surface)
    {
        std::vector<mesh_gpu::Normal3D> normals;
        vtkDataArray* normalArray = surface->GetPointData()->GetNormals();
        if (normalArray == nullptr) {
            return normals;
        }

        normals.reserve(static_cast<size_t>(surface->GetNumberOfPoints()));
        for (vtkIdType i = 0; i < surface->GetNumberOfPoints(); ++i) {
            double normal[3];
            normalArray->GetTuple(i, normal);
            normals.emplace_back(
                static_cast<float>(normal[0]),
                static_cast<float>(normal[1]),
                static_cast<float>(normal[2]));
        }

        return normals;
    }

    static std::vector<std::array<int, 3>> extractTriangles(vtkPolyData* surface)
    {
        std::vector<std::array<int, 3>> triangles;
        triangles.reserve(static_cast<size_t>(surface->GetNumberOfCells()));

        for (vtkIdType i = 0; i < surface->GetNumberOfCells(); ++i) {
            vtkCell* cell = surface->GetCell(i);
            if (cell == nullptr || cell->GetNumberOfPoints() != 3) {
                continue;
            }

            triangles.push_back({
                static_cast<int>(cell->GetPointId(0)),
                static_cast<int>(cell->GetPointId(1)),
                static_cast<int>(cell->GetPointId(2))
            });
        }

        return triangles;
    }

    static mesh_gpu::Transform4x4 createTranslationTransform(float tx, float ty, float tz)
    {
        mesh_gpu::Transform4x4 transform;
        transform(0, 3) = tx;
        transform(1, 3) = ty;
        transform(2, 3) = tz;
        return transform;
    }
};

void RegistrationCoreMeshGpuSmokeTest::runtime_output_contains_meshgpu_dll()
{
    const QString runtimeDllPath = QCoreApplication::applicationDirPath() + QStringLiteral("/MeshGPULib.dll");
    QVERIFY2(QFileInfo::exists(runtimeDllPath), qPrintable(runtimeDllPath));
}

void RegistrationCoreMeshGpuSmokeTest::registration_service_loads_meshgpu_dll_from_runtime_output()
{
    RegistrationServiceImpl service;
    QVERIFY(service.loadMeshGPUDLL());
}

void RegistrationCoreMeshGpuSmokeTest::advanced_icp_with_gpu_records_gicp_registration_when_runtime_is_available()
{
    RegistrationServiceImpl service;
    QVERIFY(service.loadMeshGPUDLL());

    auto source = createRegistrationSurface();
    auto target = createRegistrationSurface(1.5, -2.0, 3.0);

    QVariantMap parameters;
    parameters.insert(QStringLiteral("registrationId"), QStringLiteral("meshgpu_smoke"));
    parameters.insert(QStringLiteral("useGPU"), true);
    parameters.insert(QStringLiteral("maxIterations"), 10);
    parameters.insert(QStringLiteral("distanceThreshold"), 30.0);
    parameters.insert(QStringLiteral("usePointToPlane"), true);
    parameters.insert(QStringLiteral("verbose"), false);

    const auto matrix = service.performICPRegistrationAdvanced(source, target, parameters);

    QVERIFY2(matrix != nullptr, qPrintable(service.getLastError()));

    const QStringList registrationIds = service.getRegistrationList();
    QVERIFY(registrationIds.contains(QStringLiteral("meshgpu_smoke")));

    const QVariantMap info = service.getRegistrationInfo(QStringLiteral("meshgpu_smoke"));
    QCOMPARE(info.value(QStringLiteral("type")).toString(), QStringLiteral("gicp"));

    const QVariantMap metadata = info.value(QStringLiteral("metadata")).toMap();
    QCOMPARE(metadata.value(QStringLiteral("algorithm")).toString(), QStringLiteral("GPU-GICP"));
    QVERIFY(metadata.contains(QStringLiteral("elapsedMs")));
    QVERIFY(metadata.contains(QStringLiteral("converged")));
}

void RegistrationCoreMeshGpuSmokeTest::advanced_icp_with_constraint_payload_records_core_constraint_usage()
{
    RegistrationServiceImpl service;
    QVERIFY(service.loadMeshGPUDLL());

    auto source = createRegistrationSurface();
    auto target = createRegistrationSurface(1.5, -2.0, 3.0);

    QVariantMap parameters;
    parameters.insert(QStringLiteral("registrationId"), QStringLiteral("meshgpu_constraint_payload"));
    parameters.insert(QStringLiteral("useGPU"), true);
    parameters.insert(QStringLiteral("maxIterations"), 10);
    parameters.insert(QStringLiteral("distanceThreshold"), 30.0);
    parameters.insert(QStringLiteral("usePointToPlane"), true);
    parameters.insert(QStringLiteral("verbose"), false);
    parameters.insert(QStringLiteral("targetRegionCenterX"), 1.5);
    parameters.insert(QStringLiteral("targetRegionCenterY"), -2.0);
    parameters.insert(QStringLiteral("targetRegionCenterZ"), 9.0);
    parameters.insert(QStringLiteral("targetRegionRadiusMm"), 0.0);
    parameters.insert(QStringLiteral("initialTransform"), QVariantList {
        1.0, 0.0, 0.0, 1.5,
        0.0, 1.0, 0.0, -2.0,
        0.0, 0.0, 1.0, 3.0,
        0.0, 0.0, 0.0, 1.0
    });
    parameters.insert(QStringLiteral("constraintRegionCount"), 2);
    parameters.insert(QStringLiteral("constraintRegionKeys"), QStringLiteral("tibia_distal_region|talus_dome_region"));

    QVariantMap constraintRegions;
    constraintRegions.insert(
        QStringLiteral("tibia_distal_region"),
        QVariantList {
            QVariantList { -7.5, -14.0, 9.0 },
            QVariantList { -7.5, 10.0, 9.0 },
            QVariantList { 10.5, -14.0, 9.0 }
        });
    constraintRegions.insert(
        QStringLiteral("talus_dome_region"),
        QVariantList {
            QVariantList { -7.5, 10.0, 9.0 },
            QVariantList { 10.5, -14.0, 9.0 },
            QVariantList { 10.5, 10.0, 9.0 }
        });
    parameters.insert(QStringLiteral("constraintRegions"), constraintRegions);

    const auto matrix = service.performICPRegistrationAdvanced(source, target, parameters);

    QVERIFY2(matrix != nullptr, qPrintable(service.getLastError()));

    const QVariantMap info = service.getRegistrationInfo(QStringLiteral("meshgpu_constraint_payload"));
    const QVariantMap metadata = info.value(QStringLiteral("metadata")).toMap();
    QCOMPARE(metadata.value(QStringLiteral("constraintRegionCount")).toInt(), 2);
    QCOMPARE(metadata.value(QStringLiteral("constraintRegionKeys")).toString(), QStringLiteral("tibia_distal_region|talus_dome_region"));
    QCOMPARE(metadata.value(QStringLiteral("coreConstraintApplied")).toBool(), true);
    QVERIFY(metadata.value(QStringLiteral("coreConstraintSourcePointCount")).toInt() >= 3);
    QVERIFY(metadata.value(QStringLiteral("coreConstraintSourcePointCount")).toInt() < source->GetNumberOfPoints());
    QVERIFY(metadata.value(QStringLiteral("coreConstraintTargetPointCount")).toInt() >= 3);
    QVERIFY(metadata.value(QStringLiteral("coreConstraintTargetPointCount")).toInt() < target->GetNumberOfPoints());
    QVERIFY(metadata.value(QStringLiteral("coreConstraintTargetTriangleCount")).toInt() > 0);
    QVERIFY(metadata.value(QStringLiteral("coreConstraintTargetTriangleCount")).toInt() < target->GetNumberOfCells());
}

void RegistrationCoreMeshGpuSmokeTest::advanced_icp_parallel_search_records_parallel_search_metadata()
{
    RegistrationServiceImpl service;
    QVERIFY(service.loadMeshGPUDLL());

    auto source = createRegistrationSurface();
    auto target = createRegistrationSurface(1.5, -2.0, 3.0);

    QVariantMap parameters;
    parameters.insert(QStringLiteral("registrationId"), QStringLiteral("meshgpu_parallel_search"));
    parameters.insert(QStringLiteral("useGPU"), true);
    parameters.insert(QStringLiteral("enableParallelInitialSearch"), true);
    parameters.insert(QStringLiteral("registrationMethodId"), QStringLiteral("ankle_two_stage_constrained"));
    parameters.insert(QStringLiteral("candidateCount"), 4);
    parameters.insert(QStringLiteral("topKCandidateCount"), 2);
    parameters.insert(QStringLiteral("multiResolutionProfileId"), QStringLiteral("ankle_roi_three_level"));
    parameters.insert(QStringLiteral("targetRegionCenterX"), 1.5);
    parameters.insert(QStringLiteral("targetRegionCenterY"), -2.0);
    parameters.insert(QStringLiteral("targetRegionCenterZ"), 3.0);
    parameters.insert(QStringLiteral("initialTransform"), QVariantList {
        1.0, 0.0, 0.0, 1.5,
        0.0, 1.0, 0.0, -2.0,
        0.0, 0.0, 1.0, 3.0,
        0.0, 0.0, 0.0, 1.0
    });
    parameters.insert(QStringLiteral("maxIterations"), 10);
    parameters.insert(QStringLiteral("distanceThreshold"), 30.0);
    parameters.insert(QStringLiteral("usePointToPlane"), true);
    parameters.insert(QStringLiteral("verbose"), false);

    const auto matrix = service.performICPRegistrationAdvanced(source, target, parameters);

    QVERIFY2(matrix != nullptr, qPrintable(service.getLastError()));

    const QVariantMap info = service.getRegistrationInfo(QStringLiteral("meshgpu_parallel_search"));
    const QVariantMap metadata = info.value(QStringLiteral("metadata")).toMap();
    QCOMPARE(metadata.value(QStringLiteral("parallelSearchEnabled")).toBool(), true);
    QCOMPARE(metadata.value(QStringLiteral("candidateCount")).toInt(), 4);
    QCOMPARE(metadata.value(QStringLiteral("topKCount")).toInt(), 2);
    QVERIFY(metadata.value(QStringLiteral("coarseSearchMs")).toLongLong() >= 0);
    QCOMPARE(metadata.value(QStringLiteral("multiResolutionProfile")).toString(), QStringLiteral("ankle_roi_three_level"));
    QVERIFY(!metadata.value(QStringLiteral("bestCandidateId")).toString().isEmpty());
    QCOMPARE(metadata.value(QStringLiteral("bestCandidateRank")).toInt(), 0);
    QVERIFY(metadata.value(QStringLiteral("coarseScore")).toDouble() >= 0.0);
}

void RegistrationCoreMeshGpuSmokeTest::advanced_icp_parallel_search_with_constraints_records_parallel_search_metadata()
{
    RegistrationServiceImpl service;
    QVERIFY(service.loadMeshGPUDLL());

    auto source = createRegistrationSurface();
    auto target = createRegistrationSurface(1.5, -2.0, 3.0);

    QVariantMap parameters;
    parameters.insert(QStringLiteral("registrationId"), QStringLiteral("meshgpu_parallel_search_constraints"));
    parameters.insert(QStringLiteral("useGPU"), true);
    parameters.insert(QStringLiteral("enableParallelInitialSearch"), true);
    parameters.insert(QStringLiteral("registrationMethodId"), QStringLiteral("ankle_two_stage_constrained"));
    parameters.insert(QStringLiteral("candidateCount"), 4);
    parameters.insert(QStringLiteral("topKCandidateCount"), 2);
    parameters.insert(QStringLiteral("multiResolutionProfileId"), QStringLiteral("ankle_roi_three_level"));
    parameters.insert(QStringLiteral("targetRegionCenterX"), 1.5);
    parameters.insert(QStringLiteral("targetRegionCenterY"), -2.0);
    parameters.insert(QStringLiteral("targetRegionCenterZ"), 9.0);
    parameters.insert(QStringLiteral("targetRegionRadiusMm"), 0.0);
    parameters.insert(QStringLiteral("initialTransform"), QVariantList {
        1.0, 0.0, 0.0, 1.5,
        0.0, 1.0, 0.0, -2.0,
        0.0, 0.0, 1.0, 3.0,
        0.0, 0.0, 0.0, 1.0
    });
    parameters.insert(QStringLiteral("maxIterations"), 10);
    parameters.insert(QStringLiteral("distanceThreshold"), 30.0);
    parameters.insert(QStringLiteral("usePointToPlane"), true);
    parameters.insert(QStringLiteral("verbose"), false);
    parameters.insert(QStringLiteral("constraintRegionCount"), 2);
    parameters.insert(QStringLiteral("constraintRegionKeys"), QStringLiteral("tibia_distal_region|talus_dome_region"));

    QVariantMap constraintRegions;
    constraintRegions.insert(
        QStringLiteral("tibia_distal_region"),
        QVariantList {
            QVariantList { -7.5, -14.0, 9.0 },
            QVariantList { -7.5, 10.0, 9.0 },
            QVariantList { 10.5, -14.0, 9.0 }
        });
    constraintRegions.insert(
        QStringLiteral("talus_dome_region"),
        QVariantList {
            QVariantList { -7.5, 10.0, 9.0 },
            QVariantList { 10.5, -14.0, 9.0 },
            QVariantList { 10.5, 10.0, 9.0 }
        });
    parameters.insert(QStringLiteral("constraintRegions"), constraintRegions);

    const auto matrix = service.performICPRegistrationAdvanced(source, target, parameters);

    QVERIFY2(matrix != nullptr, qPrintable(service.getLastError()));

    const QVariantMap info = service.getRegistrationInfo(QStringLiteral("meshgpu_parallel_search_constraints"));
    const QVariantMap metadata = info.value(QStringLiteral("metadata")).toMap();
    QCOMPARE(metadata.value(QStringLiteral("parallelSearchEnabled")).toBool(), true);
    QCOMPARE(metadata.value(QStringLiteral("candidateCount")).toInt(), 4);
    QCOMPARE(metadata.value(QStringLiteral("topKCount")).toInt(), 2);
    QVERIFY(metadata.value(QStringLiteral("coarseSearchMs")).toLongLong() >= 0);
    QVERIFY(!metadata.value(QStringLiteral("bestCandidateId")).toString().isEmpty());
    QCOMPARE(metadata.value(QStringLiteral("bestCandidateRank")).toInt(), 0);
}

void RegistrationCoreMeshGpuSmokeTest::advanced_icp_parallel_search_records_multi_resolution_and_constraint_filter_metrics()
{
    RegistrationServiceImpl service;
    QVERIFY(service.loadMeshGPUDLL());

    auto source = createRegistrationSurface();
    auto target = createRegistrationSurface(1.5, -2.0, 3.0);

    QVariantMap parameters;
    parameters.insert(QStringLiteral("registrationId"), QStringLiteral("meshgpu_multires_roi"));
    parameters.insert(QStringLiteral("useGPU"), true);
    parameters.insert(QStringLiteral("enableParallelInitialSearch"), true);
    parameters.insert(QStringLiteral("enableConstraintParallelFilter"), true);
    parameters.insert(QStringLiteral("registrationMethodId"), QStringLiteral("ankle_two_stage_constrained"));
    parameters.insert(QStringLiteral("candidateCount"), 32);
    parameters.insert(QStringLiteral("topKCandidateCount"), 4);
    parameters.insert(QStringLiteral("multiResolutionProfileId"), QStringLiteral("ankle_roi_three_level"));
    parameters.insert(QStringLiteral("targetRegionCenterX"), 1.5);
    parameters.insert(QStringLiteral("targetRegionCenterY"), -2.0);
    parameters.insert(QStringLiteral("targetRegionCenterZ"), 9.0);
    parameters.insert(QStringLiteral("targetRegionRadiusMm"), 18.0);
    parameters.insert(QStringLiteral("initialTransform"), QVariantList {
        1.0, 0.0, 0.0, 1.5,
        0.0, 1.0, 0.0, -2.0,
        0.0, 0.0, 1.0, 3.0,
        0.0, 0.0, 0.0, 1.0
    });
    parameters.insert(QStringLiteral("maxIterations"), 10);
    parameters.insert(QStringLiteral("distanceThreshold"), 30.0);
    parameters.insert(QStringLiteral("usePointToPlane"), true);
    parameters.insert(QStringLiteral("verbose"), false);

    const auto matrix = service.performICPRegistrationAdvanced(source, target, parameters);

    QVERIFY2(matrix != nullptr, qPrintable(service.getLastError()));

    const QVariantMap info = service.getRegistrationInfo(QStringLiteral("meshgpu_multires_roi"));
    const QVariantMap metadata = info.value(QStringLiteral("metadata")).toMap();
    QCOMPARE(metadata.value(QStringLiteral("multiResolutionLevelCount")).toInt(), 3);
    QCOMPARE(metadata.value(QStringLiteral("constraintParallelFilterEnabled")).toBool(), true);
    QVERIFY(metadata.value(QStringLiteral("roiFilterMs")).toLongLong() >= 0);
}

void RegistrationCoreMeshGpuSmokeTest::candidate_batch_scoring_returns_ranked_scores_from_runtime_api()
{
    const QString runtimeDllPath = QCoreApplication::applicationDirPath() + QStringLiteral("/MeshGPULib.dll");
    QLibrary runtimeLibrary(runtimeDllPath);
    QVERIFY2(runtimeLibrary.load(), qPrintable(runtimeLibrary.errorString()));

    const auto createRuntimeApi =
        reinterpret_cast<CreateRuntimeApiFn>(runtimeLibrary.resolve("CreateMeshGPURuntimeApi"));
    const auto destroyRuntimeApi =
        reinterpret_cast<DestroyRuntimeApiFn>(runtimeLibrary.resolve("DestroyMeshGPURuntimeApi"));
    QVERIFY(createRuntimeApi != nullptr);
    QVERIFY(destroyRuntimeApi != nullptr);

    mesh_gpu::MeshGPURuntimeApi* runtimeApi = createRuntimeApi();
    QVERIFY(runtimeApi != nullptr);

    auto target = createRegistrationSurface();
    auto source = createRegistrationSurface(1.5, -2.0, 3.0);

    const auto targetVertices = extractPoints(target);
    const auto targetNormals = extractNormals(target);
    const auto targetTriangles = extractTriangles(target);
    const auto sourcePoints = extractPoints(source);

    QVERIFY(runtimeApi->setTargetMesh(targetVertices, targetNormals, targetTriangles, 1.0f));
    QVERIFY(runtimeApi->setSourcePointCloud(sourcePoints));

    const std::vector<mesh_gpu::Transform4x4> candidates {
        createTranslationTransform(0.0f, 0.0f, 0.0f),
        createTranslationTransform(-1.5f, 2.0f, -3.0f)
    };

    const auto scores = runtimeApi->scoreTransformCandidates(candidates, 20.0f);

    destroyRuntimeApi(runtimeApi);
    runtimeApi = nullptr;
    runtimeLibrary.unload();

    QCOMPARE(static_cast<int>(scores.size()), 2);
    QCOMPARE(scores.front().candidateIndex, 1);
    QCOMPARE(scores.back().candidateIndex, 0);
    QVERIFY(scores.front().score >= scores.back().score);
    QVERIFY(scores.front().meanDistanceMm >= 0.0f);
    QVERIFY(scores.back().meanDistanceMm >= 0.0f);
    QVERIFY(scores.front().meanDistanceMm <= scores.back().meanDistanceMm);
    QVERIFY(scores.front().success);
    QVERIFY(scores.back().success);
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    RegistrationCoreMeshGpuSmokeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "RegistrationCoreMeshGpuSmokeTest.moc"
