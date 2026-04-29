#pragma once

#include "OpticalTrackingServiceImpl.h"
#include "Framework/Platform/Contracts/platform_module_ports.h"

#include <memory>

class OpticalTrackingModule final : public IPlatformModuleActivator
{
public:
    ~OpticalTrackingModule() override;
    QString pluginId() const override;
    bool start(PlatformModuleContext& context) override;
    void stop(PlatformModuleContext& context) override;

private:
    std::unique_ptr<OpticalTrackingServiceImpl> m_service;
};
