#include "Framework/Platform/Kernel/PlatformRuntimeConfig.h"

#include "Framework/Platform/Kernel/PlatformDescriptorLoader.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace
{
bool assignRuntimeMode(const QString& runtimeMode, PlatformRuntimeMode& target)
{
    if (runtimeMode == QStringLiteral("observe_only")) {
        target = PlatformRuntimeMode::ObserveOnly;
        return true;
    }

    if (runtimeMode == QStringLiteral("facade_mode")) {
        target = PlatformRuntimeMode::FacadeMode;
        return true;
    }

    if (runtimeMode == QStringLiteral("orchestrate_core")) {
        target = PlatformRuntimeMode::OrchestrateCore;
        return true;
    }

    return false;
}
}

PlatformRuntimeConfig PlatformRuntimeConfig::loadFromFile(const QString& filePath, QString* error)
{
    if (error) error->clear();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Cannot open runtime config: %1").arg(filePath);
        return {};
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = QStringLiteral("Invalid runtime config json: %1").arg(filePath);
        return {};
    }

    const auto root = document.object();
    const auto runtimeModeValue = root.value(QStringLiteral("runtime_mode"));
    const auto descriptorDirectoryValue = root.value(QStringLiteral("descriptor_directory"));
    const auto corePluginIdsValue = root.value(QStringLiteral("core_plugin_ids"));

    if (!runtimeModeValue.isString()) {
        if (error) *error = QStringLiteral("runtime_mode must be a string");
        return {};
    }

    if (!descriptorDirectoryValue.isString()) {
        if (error) *error = QStringLiteral("descriptor_directory must be a string");
        return {};
    }

    if (!corePluginIdsValue.isArray()) {
        if (error) *error = QStringLiteral("core_plugin_ids must be an array");
        return {};
    }

    PlatformRuntimeConfig config;
    if (!assignRuntimeMode(runtimeModeValue.toString(), config.runtimeMode)) {
        if (error) *error = QStringLiteral("Unsupported runtime mode: %1").arg(runtimeModeValue.toString());
        return {};
    }

    config.descriptorDirectory = descriptorDirectoryValue.toString();
    const auto corePluginIds = corePluginIdsValue.toArray();
    config.corePluginIds.reserve(corePluginIds.size());

    for (const auto& item : corePluginIds) {
        if (!item.isString()) {
            if (error) *error = QStringLiteral("core_plugin_ids items must be strings");
            return {};
        }

        config.corePluginIds.append(item.toString());
    }

    return config;
}

QStringList PlatformRuntimeConfig::resolveCoreSymbolicNames(const QString& descriptorDirectoryPath, QString* error) const
{
    if (error) error->clear();
    if (descriptorDirectoryPath.isEmpty()) {
        if (error) *error = QStringLiteral("descriptor directory path is empty");
        return {};
    }

    const QDir descriptorDirectory(descriptorDirectoryPath);
    if (!descriptorDirectory.exists()) {
        if (error) *error = QStringLiteral("descriptor directory does not exist: %1").arg(descriptorDirectoryPath);
        return {};
    }

    QStringList loadErrors;
    const auto descriptors = PlatformDescriptorLoader::loadFromDirectory(descriptorDirectory.absolutePath(), &loadErrors);

    QHash<QString, QString> symbolicNamesById;
    symbolicNamesById.reserve(descriptors.size());
    for (const auto& descriptor : descriptors) {
        symbolicNamesById.insert(descriptor.id, descriptor.runtime.symbolicName);
    }

    QStringList resolvedNames;
    resolvedNames.reserve(corePluginIds.size());

    for (const auto& pluginId : corePluginIds) {
        if (!symbolicNamesById.contains(pluginId)) {
            if (error) {
                *error = QStringLiteral("missing descriptor for core plugin id: %1").arg(pluginId);
                if (!loadErrors.isEmpty()) *error += QStringLiteral(" | loader errors: %1").arg(loadErrors.join(QStringLiteral("; ")));
            }
            return {};
        }

        const auto symbolicName = symbolicNamesById.value(pluginId).trimmed();
        if (symbolicName.isEmpty()) {
            if (error) *error = QStringLiteral("descriptor missing runtime.symbolic_name for core plugin id: %1").arg(pluginId);
            return {};
        }

        resolvedNames.append(symbolicName);
    }

    return resolvedNames;
}
