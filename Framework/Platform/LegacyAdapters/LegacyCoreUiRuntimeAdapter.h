#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/PlatformUiPorts.h"

class FRAMEWORK_EXPORT LegacyCoreUiRuntimeAdapter : public ICoreUiRuntimeStatusPort
{
public:
    bool frameworkReady() const override;
    QStringList installedPlugins() const override;
    QStringList startedPlugins() const override;
    QStringList loadedPlugins() const override;
    QStringList missingServices(const QStringList& requiredServices) const override;
    bool directoryExists(const QString& directoryPath) const override;
    bool directoryReadable(const QString& directoryPath) const override;
};
