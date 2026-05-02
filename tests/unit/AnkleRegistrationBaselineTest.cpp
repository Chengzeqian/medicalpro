#include <QtTest/QtTest>

#include "Plugins/RegistrationCore/ankle_registration_utils.h"

class AnkleRegistrationBaselineTest : public QObject
{
    Q_OBJECT

private slots:
    void weighted_rigid_solver_recovers_rotation_and_translation();
};

void AnkleRegistrationBaselineTest::weighted_rigid_solver_recovers_rotation_and_translation()
{
    QList<QVector3D> source = {
        QVector3D(0.0f, 0.0f, 0.0f),
        QVector3D(10.0f, 0.0f, 0.0f),
        QVector3D(0.0f, 10.0f, 0.0f)
    };
    QList<QVector3D> target = {
        QVector3D(5.0f, 3.0f, 0.0f),
        QVector3D(5.0f, 13.0f, 0.0f),
        QVector3D(-5.0f, 3.0f, 0.0f)
    };
    QList<double> weights = { 1.0, 1.0, 1.0 };

    const WeightedRigidRegistrationResult result =
        AnkleRegistrationUtils::solveWeightedRigid(source, target, weights);

    QVERIFY(result.success);
    QVERIFY(result.weightedRmsError < 0.01);
    QVERIFY(qAbs(result.translation.x() - 5.0f) < 0.1f);
    QVERIFY(qAbs(result.translation.y() - 3.0f) < 0.1f);
}

QTEST_APPLESS_MAIN(AnkleRegistrationBaselineTest)
#include "AnkleRegistrationBaselineTest.moc"
