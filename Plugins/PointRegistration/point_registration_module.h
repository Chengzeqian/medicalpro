#pragma once

#include "Framework/Platform/Contracts/platform_module_ports.h"
#include "PointRegistrationServiceImpl.h"

#include <memory>

class PointRegistrationModule final : public IPlatformModuleActivator
{
public:
    ~PointRegistrationModule() override;
    QString pluginId() const override;
    bool start(PlatformModuleContext& context) override;
    void stop(PlatformModuleContext& context) override;

private:
    std::unique_ptr<PointRegistrationServiceImpl> m_service;
};
