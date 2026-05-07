#pragma once

#include "Plugins/PointRegistration/PointRegistrationDataStructures.h"
#include "UI/NewPages/Navigation/navigation_workspace_types.h"

namespace
{
inline bool hasRegistrationTransformComponents(const NavigationWorkspaceRegistrationState& registrationState)
{
    return registrationState.translationX != 0.0
        || registrationState.translationY != 0.0
        || registrationState.translationZ != 0.0
        || registrationState.rotationX != 0.0
        || registrationState.rotationY != 0.0
        || registrationState.rotationZ != 0.0;
}
}

inline QString summarizeRegistrationTransform(const PointRegistrationResult& result)
{
    return QStringLiteral("tx=%1,ty=%2,tz=%3,rx=%4,ry=%5,rz=%6")
        .arg(result.translationX, 0, 'f', 3)
        .arg(result.translationY, 0, 'f', 3)
        .arg(result.translationZ, 0, 'f', 3)
        .arg(result.rotationX, 0, 'f', 3)
        .arg(result.rotationY, 0, 'f', 3)
        .arg(result.rotationZ, 0, 'f', 3);
}

inline NavigationWorkspaceRegistrationState hydrateRegistrationTransformComponents(
    const NavigationWorkspaceRegistrationState& registrationState)
{
    if (hasRegistrationTransformComponents(registrationState) || registrationState.transformMatrix.isEmpty()) {
        return registrationState;
    }

    NavigationWorkspaceRegistrationState hydratedState = registrationState;
    const QStringList fields = registrationState.transformMatrix.split(QStringLiteral(","));
    for (const QString& field : fields) {
        const QStringList keyValue = field.split(QStringLiteral("="));
        if (keyValue.size() != 2) {
            continue;
        }

        const QString key = keyValue[0].trimmed();
        const double value = keyValue[1].trimmed().toDouble();
        if (key == QStringLiteral("tx")) {
            hydratedState.translationX = value;
        } else if (key == QStringLiteral("ty")) {
            hydratedState.translationY = value;
        } else if (key == QStringLiteral("tz")) {
            hydratedState.translationZ = value;
        } else if (key == QStringLiteral("rx")) {
            hydratedState.rotationX = value;
        } else if (key == QStringLiteral("ry")) {
            hydratedState.rotationY = value;
        } else if (key == QStringLiteral("rz")) {
            hydratedState.rotationZ = value;
        }
    }

    return hydratedState;
}

inline QMatrix4x4 buildRegistrationTransformMatrix(const NavigationWorkspaceRegistrationState& registrationState)
{
    const NavigationWorkspaceRegistrationState hydratedState =
        hydrateRegistrationTransformComponents(registrationState);

    QMatrix4x4 transformMatrix;
    transformMatrix.setToIdentity();
    transformMatrix.translate(
        static_cast<float>(hydratedState.translationX),
        static_cast<float>(hydratedState.translationY),
        static_cast<float>(hydratedState.translationZ));
    transformMatrix.rotate(static_cast<float>(hydratedState.rotationZ), 0.0f, 0.0f, 1.0f);
    transformMatrix.rotate(static_cast<float>(hydratedState.rotationY), 0.0f, 1.0f, 0.0f);
    transformMatrix.rotate(static_cast<float>(hydratedState.rotationX), 1.0f, 0.0f, 0.0f);
    return transformMatrix;
}

inline PointRegistrationResult buildRegistrationResultFromWorkspaceState(
    const NavigationWorkspaceRegistrationState& registrationState)
{
    const NavigationWorkspaceRegistrationState hydratedState =
        hydrateRegistrationTransformComponents(registrationState);

    PointRegistrationResult registrationResult;
    registrationResult.success = hydratedState.success;
    registrationResult.pointCount = hydratedState.pointCount;
    registrationResult.rmsError = hydratedState.fre;
    registrationResult.targetRegionTre = hydratedState.targetTre;
    registrationResult.coverageScore = hydratedState.coverageScore;
    registrationResult.translationX = hydratedState.translationX;
    registrationResult.translationY = hydratedState.translationY;
    registrationResult.translationZ = hydratedState.translationZ;
    registrationResult.rotationX = hydratedState.rotationX;
    registrationResult.rotationY = hydratedState.rotationY;
    registrationResult.rotationZ = hydratedState.rotationZ;
    registrationResult.transformMatrix = buildRegistrationTransformMatrix(hydratedState);
    registrationResult.metrics.insert(QStringLiteral("point_count"), hydratedState.pointCount);
    return registrationResult;
}
