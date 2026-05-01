#pragma once

#include <functional>

class RegistrationController
{
public:
    struct Actions
    {
        std::function<void()> computeRegistration;
    };

    explicit RegistrationController(Actions actions = {});

    void computeRegistration() const;

private:
    Actions m_actions;
};
