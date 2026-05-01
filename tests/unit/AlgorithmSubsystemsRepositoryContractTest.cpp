#include <QtTest/QtTest>

#include <QFile>
#include <QFileInfo>
#include <QString>

class AlgorithmSubsystemsRepositoryContractTest : public QObject
{
    Q_OBJECT

private slots:
    void repository_exposes_in_tree_dependency_roots_for_algorithm_subsystems();

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

void AlgorithmSubsystemsRepositoryContractTest::repository_exposes_in_tree_dependency_roots_for_algorithm_subsystems()
{
    const QFileInfo eigenCore(QStringLiteral(MEDICALPRO_SOURCE_DIR "/ThirdParty/eigen/Eigen/Core"));
    QVERIFY2(eigenCore.exists(), "ThirdParty/eigen/Eigen/Core must exist in repository");

    const QString rootCmake = readSource(QStringLiteral("CMakeLists.txt"));
    QVERIFY2(rootCmake.contains(QStringLiteral("MEDICALPRO_EIGEN_ROOT")),
        "Root CMakeLists.txt must declare MEDICALPRO_EIGEN_ROOT");
    QVERIFY2(rootCmake.contains(QStringLiteral("MEDICALPRO_ATRACSYS_SDK_DIR")),
        "Root CMakeLists.txt must declare MEDICALPRO_ATRACSYS_SDK_DIR");
    QVERIFY2(rootCmake.contains(QStringLiteral("ThirdParty/eigen")),
        "Root CMakeLists.txt must reference ThirdParty/eigen as an in-tree dependency root");
    QVERIFY2(rootCmake.contains(QStringLiteral("if(NOT EXISTS \"${MEDICALPRO_EIGEN_ROOT}/Eigen/Core\")")),
        "Root CMakeLists.txt must guard MEDICALPRO_EIGEN_ROOT with an Eigen/Core existence check");
    QVERIFY2(rootCmake.contains(QStringLiteral("message(FATAL_ERROR \"Eigen not found at ${MEDICALPRO_EIGEN_ROOT}\")")),
        "Root CMakeLists.txt must fail fast when MEDICALPRO_EIGEN_ROOT is missing Eigen/Core");
}

QTEST_APPLESS_MAIN(AlgorithmSubsystemsRepositoryContractTest)
#include "AlgorithmSubsystemsRepositoryContractTest.moc"
