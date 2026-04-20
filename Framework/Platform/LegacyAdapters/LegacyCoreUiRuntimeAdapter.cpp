#include "Framework/Platform/LegacyAdapters/LegacyCoreUiRuntimeAdapter.h"

#include "Framework/CTKManager.h"

#include <QDir>
#include <QFileInfo>

bool LegacyCoreUiRuntimeAdapter::frameworkReady() const
{
    auto* ctkManager = CTKManager::instance();
    return ctkManager && ctkManager->isCTKAvailable();
}

QStringList LegacyCoreUiRuntimeAdapter::installedPlugins() const
{
    auto* ctkManager = CTKManager::instance();
    return ctkManager ? ctkManager->getInstalledPlugins() : QStringList {};
}

QStringList LegacyCoreUiRuntimeAdapter::startedPlugins() const
{
    auto* ctkManager = CTKManager::instance();
    return ctkManager ? ctkManager->getStartedPlugins() : QStringList {};
}

QStringList LegacyCoreUiRuntimeAdapter::loadedPlugins() const
{
    auto* ctkManager = CTKManager::instance();
    return ctkManager ? ctkManager->getLoadedPlugins() : QStringList {};
}

QStringList LegacyCoreUiRuntimeAdapter::missingServices(const QStringList& requiredServices) const
{
    auto* ctkManager = CTKManager::instance();
    return ctkManager ? ctkManager->getMissingServices(requiredServices) : requiredServices;
}

bool LegacyCoreUiRuntimeAdapter::directoryExists(const QString& directoryPath) const
{
    return QDir(directoryPath).exists();
}

bool LegacyCoreUiRuntimeAdapter::directoryReadable(const QString& directoryPath) const
{
    const QFileInfo info(QDir(directoryPath).absolutePath());
    return info.isReadable();
}
