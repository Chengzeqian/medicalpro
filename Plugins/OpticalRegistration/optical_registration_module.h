#pragma once

#include "Framework/Platform/Contracts/platform_module_ports.h"
#include "OpticalRegistrationServiceImpl.h"

#include <memory>

class OpticalRegistrationModule final : public IPlatformModuleActivator
{
public:
    ~OpticalRegistrationModule() override;
    QString pluginId() const override;
    bool start(PlatformModuleContext& context) override;
    void stop(PlatformModuleContext& context) override;

private:
    std::unique_ptr<OpticalRegistrationServiceImpl> m_service;
};
