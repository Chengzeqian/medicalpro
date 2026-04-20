#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/PlatformFacadePorts.h"

class FRAMEWORK_EXPORT LegacyNavigationAdapter : public INavigationFacadePort
{
public:
    bool ensureReady(const QString& pluginId) override;
};
