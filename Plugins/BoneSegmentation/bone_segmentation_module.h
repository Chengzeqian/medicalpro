#pragma once

#include "Framework/Platform/Contracts/platform_module_ports.h"
#include "SegmentationServiceImpl.h"

#include <memory>

class BoneSegmentationModule final : public IPlatformModuleActivator
{
public:
    ~BoneSegmentationModule() override;
    QString pluginId() const override;
    bool start(PlatformModuleContext& context) override;
    void stop(PlatformModuleContext& context) override;

private:
    std::unique_ptr<SegmentationServiceImpl> m_service;
};
