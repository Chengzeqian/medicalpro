#pragma once

#include "DicomViewerServiceImpl.h"
#include "Framework/Platform/Contracts/platform_module_ports.h"

#include <memory>

class DicomViewerModule final : public IPlatformModuleActivator
{
public:
    ~DicomViewerModule() override;
    QString pluginId() const override;
    bool start(PlatformModuleContext& context) override;
    void stop(PlatformModuleContext& context) override;

private:
    std::unique_ptr<DicomViewerServiceImpl> m_service;
};
