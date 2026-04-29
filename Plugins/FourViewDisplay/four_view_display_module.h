#pragma once

#include "FourViewDisplayServiceImpl.h"
#include "Framework/Platform/Contracts/platform_module_ports.h"

#include <memory>

class FourViewDisplayModule final : public IPlatformModuleActivator
{
public:
    ~FourViewDisplayModule() override;
    QString pluginId() const override;
    bool start(PlatformModuleContext& context) override;
    void stop(PlatformModuleContext& context) override;

private:
    std::unique_ptr<FourViewDisplayServiceImpl> m_service;
};
