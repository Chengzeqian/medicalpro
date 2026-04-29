#pragma once

#include "RegistrationServiceImpl.h"
#include "Framework/Platform/Contracts/platform_module_ports.h"

#include <memory>

class RegistrationCoreModule final : public IPlatformModuleActivator
{
public:
    ~RegistrationCoreModule() override;
    QString pluginId() const override;
    bool start(PlatformModuleContext& context) override;
    void stop(PlatformModuleContext& context) override;

private:
    std::unique_ptr<RegistrationServiceImpl> m_service;
};
