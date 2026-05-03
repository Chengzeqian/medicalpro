#pragma once

#include "Plugins/PointRegistration/PointRegistrationDataStructures.h"

#include <functional>

class NavigationRuntimeCoordinator;

class RegistrationController
{
public:
    struct Actions
    {
        std::function<void()> computeRegistration;
    };

    explicit RegistrationController(
        Actions actions = {},
        NavigationRuntimeCoordinator* runtimeCoordinator = nullptr);

    void computeRegistration() const;
    void handleRegistrationCompleted(const PointRegistrationResult& result) const;

private:
    Actions m_actions;
    NavigationRuntimeCoordinator* m_runtimeCoordinator = nullptr;
};
