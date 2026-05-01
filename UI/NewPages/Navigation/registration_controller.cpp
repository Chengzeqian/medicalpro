#include "UI/NewPages/Navigation/registration_controller.h"

RegistrationController::RegistrationController(Actions actions)
    : m_actions(std::move(actions))
{
}

void RegistrationController::computeRegistration() const
{
    if (m_actions.computeRegistration) {
        m_actions.computeRegistration();
    }
}
