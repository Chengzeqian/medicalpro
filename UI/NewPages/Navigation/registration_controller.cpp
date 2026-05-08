#include "UI/NewPages/Navigation/registration_controller.h"

#include "UI/NewPages/Navigation/navigation_runtime_coordinator.h"

#include <QDateTime>

namespace
{
QString serializeMatrix(const QMatrix4x4& matrix)
{
    QStringList rows;
    for (int row = 0; row < 4; ++row) {
        QStringList values;
        for (int column = 0; column < 4; ++column) {
            values.append(QString::number(matrix(row, column), 'f', 6));
        }
        rows.append(values.join(QStringLiteral(",")));
    }
    return rows.join(QStringLiteral(";"));
}
}

RegistrationController::RegistrationController(
    Actions actions,
    NavigationRuntimeCoordinator* runtimeCoordinator)
    : m_actions(std::move(actions))
    , m_runtimeCoordinator(runtimeCoordinator)
{
}

void RegistrationController::computeRegistration() const
{
    if (m_actions.computeRegistration) {
        m_actions.computeRegistration();
    }
}

void RegistrationController::handleRegistrationCompleted(const PointRegistrationResult& result) const
{
    if (m_runtimeCoordinator) {
        m_runtimeCoordinator->handleRegistrationResult(result);
    }
}

NavigationWorkspaceRegistrationState RegistrationController::computePerBoneRegistration() const
{
    if (m_actions.computeRegistration) {
        m_actions.computeRegistration();
    }

    return buildRegistrationWorkspaceState(
        m_actions.resolvePerBoneRegistrationResults
            ? m_actions.resolvePerBoneRegistrationResults()
            : QList<PointRegistrationResult>(),
        m_actions.resolveFusedNavigationSpacePath
            ? m_actions.resolveFusedNavigationSpacePath()
            : QString(),
        m_actions.resolveFusedCoverageScore
            ? m_actions.resolveFusedCoverageScore()
            : 0.0);
}

NavigationWorkspaceRegistrationState RegistrationController::buildRegistrationWorkspaceState(
    const QList<PointRegistrationResult>& perBoneResults,
    const QString& fusedSpacePath,
    double fusedCoverageScore) const
{
    NavigationWorkspaceRegistrationState state;
    for (const PointRegistrationResult& result : perBoneResults) {
        NavigationPerBoneRegistrationState perBoneState;
        perBoneState.boneAssetId =
            result.metrics.value(QStringLiteral("bone_asset_id")).toString();
        perBoneState.boneRegionId =
            result.metrics.value(QStringLiteral("bone_region_id")).toString();
        perBoneState.pointCount = result.pointCount;
        perBoneState.success = result.success;
        perBoneState.fre = result.rmsError;
        perBoneState.targetTre = result.targetRegionTre;
        perBoneState.coverageScore = result.coverageScore;
        perBoneState.transformMatrix = serializeMatrix(result.transformMatrix);
        perBoneState.completedAt = QDateTime::currentDateTimeUtc();
        state.perBoneResults.append(perBoneState);
    }

    state.pointCount = 0;
    state.success = !state.perBoneResults.isEmpty();
    state.fre = 0.0;
    state.targetTre = 0.0;
    state.coverageScore = 0.0;
    for (const PointRegistrationResult& result : perBoneResults) {
        state.pointCount += result.pointCount;
        state.fre += result.rmsError;
        state.targetTre += result.targetRegionTre;
        state.coverageScore += result.coverageScore;
    }
    if (!perBoneResults.isEmpty()) {
        state.fre /= perBoneResults.size();
        state.targetTre /= perBoneResults.size();
        state.coverageScore /= perBoneResults.size();
    }

    if (!perBoneResults.isEmpty()) {
        const PointRegistrationResult& lastResult = perBoneResults.constLast();
        state.translationX = lastResult.translationX;
        state.translationY = lastResult.translationY;
        state.translationZ = lastResult.translationZ;
        state.rotationX = lastResult.rotationX;
        state.rotationY = lastResult.rotationY;
        state.rotationZ = lastResult.rotationZ;
        state.transformMatrix = serializeMatrix(lastResult.transformMatrix);
    }

    state.fusedNavigationSpaceReady = !fusedSpacePath.isEmpty() && !state.perBoneResults.isEmpty();
    state.fusedNavigationSpacePath = fusedSpacePath;
    state.fusedCoverageScore = fusedCoverageScore;
    if (!state.fusedNavigationSpaceReady) {
        state.fusionBlockingReasons.append(QStringLiteral("融合导航空间尚未生成"));
    }
    state.completedAt = QDateTime::currentDateTimeUtc();
    return state;
}
