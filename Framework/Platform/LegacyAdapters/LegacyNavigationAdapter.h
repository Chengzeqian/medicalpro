#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/PlatformFacadePorts.h"

#include <functional>

class FRAMEWORK_EXPORT LegacyNavigationAdapter : public INavigationFacadePort
{
public:
    using EnsureReadyFn = std::function<bool(const QString&)>;

    explicit LegacyNavigationAdapter(EnsureReadyFn ensureReadyFn = {});

    bool ensureReady(const QString& pluginId) override;

private:
    EnsureReadyFn m_ensureReadyFn;
};
