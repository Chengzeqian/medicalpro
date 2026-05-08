#pragma once

#include "Plugins/PointRegistration/PointRegistrationDataStructures.h"
#include "UI/NewPages/Navigation/navigation_workspace_types.h"

#include <functional>

class NavigationRuntimeCoordinator;

class RegistrationController
{
public:
    struct Actions
    {
        std::function<void()> computeRegistration;
        std::function<QList<PointRegistrationResult>()> resolvePerBoneRegistrationResults;
        std::function<QString()> resolveFusedNavigationSpacePath;
        std::function<double()> resolveFusedCoverageScore;
    };

    explicit RegistrationController(
        Actions actions = {},
        NavigationRuntimeCoordinator* runtimeCoordinator = nullptr);

    void computeRegistration() const;
    void handleRegistrationCompleted(const PointRegistrationResult& result) const;
    NavigationWorkspaceRegistrationState computePerBoneRegistration() const;
    NavigationWorkspaceRegistrationState buildRegistrationWorkspaceState(
        const QList<PointRegistrationResult>& perBoneResults,
        const QString& fusedSpacePath,
        double fusedCoverageScore) const;

private:
    Actions m_actions;
    NavigationRuntimeCoordinator* m_runtimeCoordinator = nullptr;
};
