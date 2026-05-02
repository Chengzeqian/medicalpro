#include <QtTest/QtTest>

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QString>

class MeshGpuSubprojectContractTest : public QObject
{
    Q_OBJECT

private slots:
    void meshgpu_subproject_uses_in_tree_cmake_contract();

private:
    QString readSource(const QString& relativePath) const
    {
        QFile file(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTest::qFail(qPrintable(relativePath), __FILE__, __LINE__);
            return {};
        }

        return QString::fromUtf8(file.readAll());
    }
};

void MeshGpuSubprojectContractTest::meshgpu_subproject_uses_in_tree_cmake_contract()
{
    const QFileInfo meshgpuCmake(QStringLiteral(MEDICALPRO_SOURCE_DIR "/algorithms/meshgpu/CMakeLists.txt"));
    QVERIFY2(meshgpuCmake.exists(), "algorithms/meshgpu/CMakeLists.txt must exist");
    QVERIFY2(meshgpuCmake.isReadable(), "algorithms/meshgpu/CMakeLists.txt must be readable");

    const QString cmakeSource = readSource(QStringLiteral("algorithms/meshgpu/CMakeLists.txt"));
    QVERIFY2(cmakeSource.contains(QStringLiteral("project(MeshGPU")),
        "MeshGPU subproject must declare project(MeshGPU");
    QVERIFY2(cmakeSource.contains(QStringLiteral("add_library(MeshGPULib SHARED")),
        "MeshGPU subproject must declare add_library(MeshGPULib SHARED");
    QVERIFY2(cmakeSource.contains(QStringLiteral("MEDICALPRO_EIGEN_ROOT")),
        "MeshGPU subproject must consume MEDICALPRO_EIGEN_ROOT");
    QVERIFY2(cmakeSource.contains(QStringLiteral("option(MESHGPU_BUILD_TOOLS \"Build MeshGPU CLI tools and demos\" OFF)")),
        "MeshGPU tools must stay OFF by default");
    QVERIFY2(cmakeSource.contains(QStringLiteral("option(BUILD_ASCEND_STUB_PLUGIN \"Build sample Ascend backend plugin template\" OFF)")),
        "MeshGPU ascend stub plugin must stay OFF by default");
    QVERIFY2(cmakeSource.contains(QStringLiteral("option(BUILD_ASCEND_ALGO_SAMPLE \"Build sample Ascend algorithm hook plugin\" OFF)")),
        "MeshGPU ascend algorithm sample must stay OFF by default");
    QVERIFY2(cmakeSource.contains(QStringLiteral("option(BUILD_ASCEND_CANN_KERNEL_SAMPLE \"Build sample external CANN kernel hook plugin\" OFF)")),
        "MeshGPU ascend CANN sample must stay OFF by default");
    QVERIFY2(cmakeSource.contains(QStringLiteral("message(STATUS \"MeshGPU core build skipped because MESHGPU_ENABLE_CUDA=OFF\")")),
        "MeshGPU subproject must explicitly report safe skip when MESHGPU_ENABLE_CUDA=OFF");
    QVERIFY2(cmakeSource.contains(QStringLiteral("return()")),
        "MeshGPU subproject must return early when MESHGPU_ENABLE_CUDA=OFF");
    QVERIFY2(!cmakeSource.contains(QStringLiteral("message(FATAL_ERROR \"MeshGPU core library currently requires CUDA.")),
        "MeshGPU subproject must not fatal when MESHGPU_ENABLE_CUDA=OFF");
    QVERIFY2(!cmakeSource.contains(QStringLiteral("../eigen")),
        "MeshGPU subproject must not reference ../eigen");
    QVERIFY2(!cmakeSource.contains(QStringLiteral("E:/ICPtry")),
        "MeshGPU subproject must not reference E:/ICPtry");

    const QStringList forbiddenDirectories = {
        QStringLiteral("algorithms/meshgpu/scripts"),
        QStringLiteral("algorithms/meshgpu/build"),
        QStringLiteral("algorithms/meshgpu/build_ascend_only"),
        QStringLiteral("algorithms/meshgpu/logs"),
        QStringLiteral("algorithms/meshgpu/benchmark_results"),
        QStringLiteral("algorithms/meshgpu/visualization_output")
    };

    for (const QString& relativePath : forbiddenDirectories) {
        const QDir forbiddenDirectory(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
        QVERIFY2(!forbiddenDirectory.exists(), qPrintable(relativePath + QStringLiteral(" must not exist")));
    }
}

QTEST_APPLESS_MAIN(MeshGpuSubprojectContractTest)
#include "MeshGpuSubprojectContractTest.moc"
