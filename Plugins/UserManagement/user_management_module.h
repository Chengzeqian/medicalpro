#pragma once

#include "Framework/Platform/Contracts/platform_module_ports.h"
#include "UserManagementServiceImpl.h"

#include <memory>

class UserManagementModule final : public IPlatformModuleActivator
{
public:
    ~UserManagementModule() override;
    QString pluginId() const override;
    bool start(PlatformModuleContext& context) override;
    void stop(PlatformModuleContext& context) override;

private:
    std::unique_ptr<UserManagementServiceImpl> m_service;
};
