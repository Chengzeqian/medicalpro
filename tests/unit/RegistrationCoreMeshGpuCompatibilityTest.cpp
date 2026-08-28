#include <QtTest/QtTest>

#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QCoreApplication>
#include <QVariantMap>

#include <vtkCell.h>
#include <vtkCubeSource.h>
#include <vtkMatrix4x4.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataNormals.h>
#include <vtkSmartPointer.h>
#include <vtkTriangleFilter.h>

#include "Plugins/RegistrationCore/RegistrationServiceImpl.h"

class RegistrationCoreMeshGpuCompatibilityTest : public QObject
{
    Q_OBJECT

private slots:
    void legacy_meshgpu_runtime_still_supports_parallel_search_registration();

private:
    static QString resolveRuntimeDllPath()
    {
        const QDir executableDir(QCoreApplication::applicationDirPath());
        const QStringList candidates {
            executableDir.absoluteFilePath(QStringLiteral("MeshGPULib.dll")),
            executableDir.absoluteFilePath(QStringLiteral("../MeshGPULib.dll")),
            executableDir.absoluteFilePath(QStringLiteral("../../Release/MeshGPULib.dll"))
        };

        for (const QString& candidate : candidates) {
            if (QFileInfo::exists(candidate)) {
                return candidate;
            }
        }

        return candidates.front();
    }

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

void RegistrationCoreMeshGpuCompatibilityTest::legacy_meshgpu_runtime_still_supports_parallel_search_registration()
{
    const QString runtimeDllPath = resolveRuntimeDllPath();
    QVERIFY2(QFileInfo::exists(runtimeDllPath), qPrintable(runtimeDllPath));

    QLibrary runtimeLibrary(runtimeDllPath);
    QVERIFY2(runtimeLibrary.load(), qPrintable(runtimeLibrary.errorString()));
    const bool hasRuntimeApi =
        runtimeLibrary.resolve("CreateMeshGPURuntimeApi") != nullptr
        && runtimeLibrary.resolve("DestroyMeshGPURuntimeApi") != nullptr;
    runtimeLibrary.unload();

    RegistrationServiceImpl service;
    QVERIFY2(service.loadMeshGPUDLL(runtimeDllPath), qPrintable(runtimeDllPath));

    auto source = createRegistrationSurface();
    auto target = createRegistrationSurface(1.5, -2.0, 3.0);

    QVariantMap parameters;
    parameters.insert(QStringLiteral("registrationId"), QStringLiteral("meshgpu_legacy_parallel_search"));
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

    const QVariantMap info = service.getRegistrationInfo(QStringLiteral("meshgpu_legacy_parallel_search"));
    const QVariantMap metadata = info.value(QStringLiteral("metadata")).toMap();
    if (hasRuntimeApi) {
        QCOMPARE(info.value(QStringLiteral("type")).toString(), QStringLiteral("gicp"));
        QCOMPARE(metadata.value(QStringLiteral("parallelSearchEnabled")).toBool(), true);
        QCOMPARE(metadata.value(QStringLiteral("candidateCount")).toInt(), 4);
        QCOMPARE(metadata.value(QStringLiteral("topKCount")).toInt(), 2);
        QVERIFY(!metadata.value(QStringLiteral("bestCandidateId")).toString().isEmpty());
        QCOMPARE(metadata.value(QStringLiteral("batchRefineRequested")).toBool(), true);
        QCOMPARE(metadata.value(QStringLiteral("batchRefineEnabled")).toBool(), true);
        QVERIFY(metadata.value(QStringLiteral("batchRefineFallback")).toString().isEmpty());
    } else {
        QCOMPARE(metadata.value(QStringLiteral("parallelSearchRequested")).toBool(), true);
        QCOMPARE(metadata.value(QStringLiteral("parallelSearchEnabled")).toBool(), false);
        QCOMPARE(
            metadata.value(QStringLiteral("parallelSearchFallback")).toString(),
            QStringLiteral("legacy_runtime_without_candidate_scoring"));
        QCOMPARE(metadata.value(QStringLiteral("legacyRuntimeCompatibility")).toBool(), true);
        QCOMPARE(metadata.value(QStringLiteral("candidateCountRequested")).toInt(), 4);
        QCOMPARE(metadata.value(QStringLiteral("topKCountRequested")).toInt(), 2);
        QCOMPARE(metadata.value(QStringLiteral("batchRefineRequested")).toBool(), true);
        QCOMPARE(metadata.value(QStringLiteral("batchRefineEnabled")).toBool(), false);
        QCOMPARE(
            metadata.value(QStringLiteral("batchRefineFallback")).toString(),
            QStringLiteral("legacy_runtime_without_batch_refine"));
    }
}

QTEST_GUILESS_MAIN(RegistrationCoreMeshGpuCompatibilityTest)

#include "RegistrationCoreMeshGpuCompatibilityTest.moc"
