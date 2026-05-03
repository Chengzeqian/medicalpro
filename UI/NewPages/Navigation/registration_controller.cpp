#include "UI/NewPages/Navigation/registration_controller.h"

#include "UI/NewPages/Navigation/navigation_runtime_coordinator.h"

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
