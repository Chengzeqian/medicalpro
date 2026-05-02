#include <QtTest/QtTest>

#include <QApplication>
#include <QCoreApplication>
#include <QFileInfo>
#include <QVariantMap>

#include <vtkCubeSource.h>
#include <vtkMatrix4x4.h>
#include <vtkPolyData.h>
#include <vtkPolyDataNormals.h>
#include <vtkSmartPointer.h>
#include <vtkTriangleFilter.h>

#include "Plugins/RegistrationCore/RegistrationServiceImpl.h"

class RegistrationCoreMeshGpuSmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void runtime_output_contains_meshgpu_dll();
    void registration_service_loads_meshgpu_dll_from_runtime_output();
    void advanced_icp_with_gpu_records_gicp_registration_when_runtime_is_available();
    void advanced_icp_with_constraint_payload_records_core_constraint_usage();

private:
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

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    RegistrationCoreMeshGpuSmokeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "RegistrationCoreMeshGpuSmokeTest.moc"
