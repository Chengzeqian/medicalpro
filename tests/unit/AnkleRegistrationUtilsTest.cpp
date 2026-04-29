#include <QtTest/QtTest>

#include "Plugins/RegistrationCore/ankle_registration_utils.h"

class AnkleRegistrationUtilsTest : public QObject
{
    Q_OBJECT

private slots:
    void weighted_rigid_alignment_prefers_high_weight_landmarks();
};

void AnkleRegistrationUtilsTest::weighted_rigid_alignment_prefers_high_weight_landmarks()
{
    QList<QVector3D> source = {
        QVector3D(0.0f, 0.0f, 0.0f),
        QVector3D(10.0f, 0.0f, 0.0f),
        QVector3D(0.0f, 10.0f, 0.0f)
    };
    QList<QVector3D> target = {
        QVector3D(5.0f, 0.0f, 0.0f),
        QVector3D(15.0f, 0.0f, 0.0f),
        QVector3D(0.0f, 30.0f, 0.0f)
    };
    QList<double> weights = { 1.0, 1.0, 0.1 };

    const WeightedRigidRegistrationResult result =
        AnkleRegistrationUtils::solveWeightedRigid(source, target, weights);

    QVERIFY(result.success);
    QVERIFY(result.translation.x() > 4.0f);
    QVERIFY(result.weightedRmsError < 8.0);
}

QTEST_APPLESS_MAIN(AnkleRegistrationUtilsTest)
#include "AnkleRegistrationUtilsTest.moc"
