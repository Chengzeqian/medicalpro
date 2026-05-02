#include "ankle_registration_utils.h"

#include <QtMath>
#include <vtkMath.h>

namespace
{
QVector3D rotateVector(const double rotation[3][3], const QVector3D& vector)
{
    return QVector3D(
        static_cast<float>(rotation[0][0] * vector.x() + rotation[0][1] * vector.y() + rotation[0][2] * vector.z()),
        static_cast<float>(rotation[1][0] * vector.x() + rotation[1][1] * vector.y() + rotation[1][2] * vector.z()),
        static_cast<float>(rotation[2][0] * vector.x() + rotation[2][1] * vector.y() + rotation[2][2] * vector.z()));
}

void transpose3x3(const double input[3][3], double output[3][3])
{
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            output[row][column] = input[column][row];
        }
    }
}

void multiply3x3(const double left[3][3], const double right[3][3], double output[3][3])
{
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            output[row][column] = 0.0;
            for (int pivot = 0; pivot < 3; ++pivot) {
                output[row][column] += left[row][pivot] * right[pivot][column];
            }
        }
    }
}

double determinant3x3(const double matrix[3][3])
{
    return matrix[0][0] * (matrix[1][1] * matrix[2][2] - matrix[1][2] * matrix[2][1])
        - matrix[0][1] * (matrix[1][0] * matrix[2][2] - matrix[1][2] * matrix[2][0])
        + matrix[0][2] * (matrix[1][0] * matrix[2][1] - matrix[1][1] * matrix[2][0]);
}
}

WeightedRigidRegistrationResult AnkleRegistrationUtils::solveWeightedRigid(
    const QList<QVector3D>& source,
    const QList<QVector3D>& target,
    const QList<double>& weights)
{
    WeightedRigidRegistrationResult result;
    result.transform.setToIdentity();

    if (source.size() < 3 || source.size() != target.size() || source.size() != weights.size()) {
        return result;
    }

    double totalWeight = 0.0;
    QVector3D weightedSourceCentroid;
    QVector3D weightedTargetCentroid;

    for (int i = 0; i < source.size(); ++i) {
        const double weight = weights[i];
        if (weight <= 0.0) {
            continue;
        }

        totalWeight += weight;
        weightedSourceCentroid += static_cast<float>(weight) * source[i];
        weightedTargetCentroid += static_cast<float>(weight) * target[i];
    }

    if (totalWeight <= 0.0) {
        return result;
    }

    weightedSourceCentroid /= static_cast<float>(totalWeight);
    weightedTargetCentroid /= static_cast<float>(totalWeight);

    double covariance[3][3] = {};
    for (int i = 0; i < source.size(); ++i) {
        const double weight = weights[i];
        if (weight <= 0.0) {
            continue;
        }

        const QVector3D centeredSource = source[i] - weightedSourceCentroid;
        const QVector3D centeredTarget = target[i] - weightedTargetCentroid;
        covariance[0][0] += weight * centeredSource.x() * centeredTarget.x();
        covariance[0][1] += weight * centeredSource.x() * centeredTarget.y();
        covariance[0][2] += weight * centeredSource.x() * centeredTarget.z();
        covariance[1][0] += weight * centeredSource.y() * centeredTarget.x();
        covariance[1][1] += weight * centeredSource.y() * centeredTarget.y();
        covariance[1][2] += weight * centeredSource.y() * centeredTarget.z();
        covariance[2][0] += weight * centeredSource.z() * centeredTarget.x();
        covariance[2][1] += weight * centeredSource.z() * centeredTarget.y();
        covariance[2][2] += weight * centeredSource.z() * centeredTarget.z();
    }

    double u[3][3] = {};
    double singularValues[3] = {};
    double vt[3][3] = {};
    vtkMath::SingularValueDecomposition3x3(covariance, u, singularValues, vt);

    double v[3][3] = {};
    transpose3x3(vt, v);

    double uTranspose[3][3] = {};
    transpose3x3(u, uTranspose);

    double rotation[3][3] = {};
    multiply3x3(v, uTranspose, rotation);

    if (determinant3x3(rotation) < 0.0) {
        for (int row = 0; row < 3; ++row) {
            v[row][2] *= -1.0;
        }
        multiply3x3(v, uTranspose, rotation);
    }

    result.translation = weightedTargetCentroid - rotateVector(rotation, weightedSourceCentroid);
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            result.transform(row, column) = static_cast<float>(rotation[row][column]);
        }
    }
    result.transform(0, 3) = result.translation.x();
    result.transform(1, 3) = result.translation.y();
    result.transform(2, 3) = result.translation.z();
    result.rotation = QQuaternion::fromRotationMatrix(result.transform.normalMatrix());

    double weightedSquaredError = 0.0;
    for (int i = 0; i < source.size(); ++i) {
        const double weight = weights[i];
        if (weight <= 0.0) {
            continue;
        }

        const QVector3D delta = rotateVector(rotation, source[i]) + result.translation - target[i];
        weightedSquaredError += weight * static_cast<double>(QVector3D::dotProduct(delta, delta));
    }

    result.weightedRmsError = qSqrt(weightedSquaredError / totalWeight);
    result.success = true;
    return result;
}

QList<int> AnkleRegistrationUtils::selectRoiPointIndices(
    const QList<QVector3D>& modelPoints,
    const QVector3D& roiCenter,
    double roiRadiusMm)
{
    QList<int> indices;
    if (roiRadiusMm <= 0.0) {
        return indices;
    }

    const float radiusSquared = static_cast<float>(roiRadiusMm * roiRadiusMm);
    for (int i = 0; i < modelPoints.size(); ++i) {
        const QVector3D delta = modelPoints[i] - roiCenter;
        if (QVector3D::dotProduct(delta, delta) <= radiusSquared) {
            indices.append(i);
        }
    }

    return indices;
}
