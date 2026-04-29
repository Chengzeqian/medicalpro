#pragma once

#include "Registration2D3DServiceImpl.h"
#include "Framework/Platform/Contracts/platform_module_ports.h"

#include <memory>

class Registration2D3DModule final : public IPlatformModuleActivator
{
public:
    ~Registration2D3DModule() override;
    QString pluginId() const override;
    bool start(PlatformModuleContext& context) override;
    void stop(PlatformModuleContext& context) override;

private:
    std::unique_ptr<Registration2D3DServiceImpl> m_service;
};
