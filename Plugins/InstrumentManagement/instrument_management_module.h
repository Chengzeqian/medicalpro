#pragma once

#include "Framework/Platform/Contracts/platform_module_ports.h"
#include "InstrumentManagementServiceImpl.h"

#include <memory>

class InstrumentManagementModule final : public IPlatformModuleActivator
{
public:
    ~InstrumentManagementModule() override;
    QString pluginId() const override;
    bool start(PlatformModuleContext& context) override;
    void stop(PlatformModuleContext& context) override;

private:
    std::unique_ptr<InstrumentManagementServiceImpl> m_service;
};
