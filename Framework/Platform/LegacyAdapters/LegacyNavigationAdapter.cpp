#include "Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.h"

#include "Framework/CTKManager.h"
#include "Framework/Platform/Kernel/PlatformDescriptorLoader.h"
#include "Framework/Platform/Kernel/PlatformRuntimeConfig.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>

#include <utility>

LegacyNavigationAdapter::LegacyNavigationAdapter(
    StartPluginFn startPluginFn,
    const QHash<QString, QString>& platformPluginIdToCtkSymbolicName)
    : m_startPluginFn(std::move(startPluginFn))
    , m_platformPluginIdToCtkSymbolicName(platformPluginIdToCtkSymbolicName)
{
    if (!m_startPluginFn) {
        m_startPluginFn = [](const QString& ctkSymbolicName) {
            return CTKManager::instance()->startPlugin(ctkSymbolicName);
        };
    }

    if (m_platformPluginIdToCtkSymbolicName.isEmpty()) {
        m_platformPluginIdToCtkSymbolicName = loadPlatformPluginIdMapping();
    }
}

bool LegacyNavigationAdapter::ensureReady(const QString& pluginId)
{
    const auto trimmedPluginId = pluginId.trimmed();
    if (trimmedPluginId.isEmpty() || !m_startPluginFn) return false;

    const auto ctkSymbolicName = m_platformPluginIdToCtkSymbolicName.value(trimmedPluginId).trimmed();
    if (!ctkSymbolicName.isEmpty()) return m_startPluginFn(ctkSymbolicName);

    for (auto it = m_platformPluginIdToCtkSymbolicName.constBegin();
         it != m_platformPluginIdToCtkSymbolicName.constEnd();
         ++it) {
        if (it.value().compare(trimmedPluginId, Qt::CaseInsensitive) != 0) continue;
        qWarning() << "[LegacyNavigationAdapter] ensureReady received CTK symbolic name directly:" << trimmedPluginId;
        return m_startPluginFn(trimmedPluginId);
    }

    qWarning() << "[LegacyNavigationAdapter] Missing platform->CTK mapping for plugin id:" << trimmedPluginId;
    return false;
}

QHash<QString, QString> LegacyNavigationAdapter::loadPlatformPluginIdMapping()
{
    QHash<QString, QString> mapping;

    const QDir appDirectory(QCoreApplication::applicationDirPath());
    const auto runtimeConfigPath = appDirectory.filePath(QStringLiteral("config/platform_runtime.json"));

    QString runtimeConfigError;
    const auto runtimeConfig = PlatformRuntimeConfig::loadFromFile(runtimeConfigPath, &runtimeConfigError);
    if (!runtimeConfigError.isEmpty() || runtimeConfig.descriptorDirectory.isEmpty()) {
        return mapping;
    }

    QStringList descriptorErrors;
    const auto descriptors = PlatformDescriptorLoader::loadFromDirectory(
        appDirectory.filePath(runtimeConfig.descriptorDirectory),
        &descriptorErrors);
    Q_UNUSED(descriptorErrors);

    for (const auto& descriptor : descriptors) {
        const auto pluginId = descriptor.id.trimmed();
        const auto ctkSymbolicName = descriptor.runtime.ctkSymbolicName.trimmed();
        if (pluginId.isEmpty() || ctkSymbolicName.isEmpty()) continue;
        mapping.insert(pluginId, ctkSymbolicName);
    }

    return mapping;
}
