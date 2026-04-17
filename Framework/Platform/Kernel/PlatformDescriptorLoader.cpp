#include "Framework/Platform/Kernel/PlatformDescriptorLoader.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace
{
QStringList toStringList(const QJsonValue& value)
{
    QStringList output;
    for (const auto& item : value.toArray()) output.append(item.toString());
    return output;
}

PlatformStartupPolicy toStartupPolicy(const QString& rawValue, bool* ok)
{
    if (rawValue == QStringLiteral("eager")) {
        *ok = true;
        return PlatformStartupPolicy::Eager;
    }
    if (rawValue == QStringLiteral("on_demand")) {
        *ok = true;
        return PlatformStartupPolicy::OnDemand;
    }
    if (rawValue == QStringLiteral("disabled")) {
        *ok = true;
        return PlatformStartupPolicy::Disabled;
    }

    *ok = false;
    return PlatformStartupPolicy::Disabled;
}

PlatformBootstrapLevel toBootstrapLevel(const QString& rawValue, bool* ok)
{
    if (rawValue == QStringLiteral("core")) {
        *ok = true;
        return PlatformBootstrapLevel::Core;
    }
    if (rawValue == QStringLiteral("deferred")) {
        *ok = true;
        return PlatformBootstrapLevel::Deferred;
    }

    *ok = false;
    return PlatformBootstrapLevel::Deferred;
}
}

PlatformPluginDescriptor PlatformDescriptorLoader::loadFromFile(const QString& filePath, QString* error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Cannot open descriptor: %1").arg(filePath);
        return {};
    }

    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        if (error) *error = QStringLiteral("Descriptor root must be a JSON object: %1").arg(filePath);
        return {};
    }

    const auto root = document.object();
    PlatformPluginDescriptor descriptor;
    descriptor.id = root.value(QStringLiteral("id")).toString();
    descriptor.version = root.value(QStringLiteral("version")).toString();
    descriptor.displayName = root.value(QStringLiteral("display_name")).toString();
    descriptor.domain = root.value(QStringLiteral("domain")).toString();
    descriptor.enabled = root.value(QStringLiteral("enabled")).toBool(true);

    const auto runtimeObject = root.value(QStringLiteral("runtime")).toObject();
    descriptor.runtime.ctkSymbolicName = runtimeObject.value(QStringLiteral("ctk_symbolic_name")).toString();
    descriptor.runtime.entryCapability = runtimeObject.value(QStringLiteral("entry_capability")).toString();

    bool startupPolicyOk = false;
    descriptor.runtime.startupPolicy = toStartupPolicy(
        runtimeObject.value(QStringLiteral("startup_policy")).toString(),
        &startupPolicyOk);
    if (!startupPolicyOk) {
        if (error) *error = QStringLiteral("Unsupported startup_policy in descriptor: %1").arg(filePath);
        return {};
    }

    bool bootstrapLevelOk = false;
    descriptor.runtime.bootstrapLevel = toBootstrapLevel(
        runtimeObject.value(QStringLiteral("bootstrap_level")).toString(),
        &bootstrapLevelOk);
    if (!bootstrapLevelOk) {
        if (error) *error = QStringLiteral("Unsupported bootstrap_level in descriptor: %1").arg(filePath);
        return {};
    }

    const auto providesObject = root.value(QStringLiteral("provides")).toObject();
    descriptor.provides.services = toStringList(providesObject.value(QStringLiteral("services")));
    descriptor.provides.capabilities = toStringList(providesObject.value(QStringLiteral("capabilities")));
    descriptor.provides.plugins = toStringList(providesObject.value(QStringLiteral("plugins")));

    const auto requiresObject = root.value(QStringLiteral("requires")).toObject();
    descriptor.required.services = toStringList(requiresObject.value(QStringLiteral("services")));
    descriptor.required.capabilities = toStringList(requiresObject.value(QStringLiteral("capabilities")));
    descriptor.required.plugins = toStringList(requiresObject.value(QStringLiteral("plugins")));

    const auto optionalObject = root.value(QStringLiteral("optional")).toObject();
    descriptor.optional.services = toStringList(optionalObject.value(QStringLiteral("services")));
    descriptor.optional.capabilities = toStringList(optionalObject.value(QStringLiteral("capabilities")));
    descriptor.optional.plugins = toStringList(optionalObject.value(QStringLiteral("plugins")));

    descriptor.healthChecks = toStringList(root.value(QStringLiteral("health_checks")));

    if (descriptor.id.isEmpty() || descriptor.version.isEmpty() || descriptor.displayName.isEmpty() || descriptor.domain.isEmpty()) {
        if (error) *error = QStringLiteral("Descriptor missing required fields: %1").arg(filePath);
        return {};
    }

    if (error) error->clear();
    return descriptor;
}

QVector<PlatformPluginDescriptor> PlatformDescriptorLoader::loadFromDirectory(const QString& directoryPath, QStringList* errors)
{
    QVector<PlatformPluginDescriptor> descriptors;
    const QDir directory(directoryPath);
    const auto files = directory.entryList({QStringLiteral("*.json")}, QDir::Files | QDir::Readable, QDir::Name);

    for (const auto& fileName : files) {
        QString error;
        const auto descriptor = loadFromFile(directory.filePath(fileName), &error);
        if (!error.isEmpty()) {
            if (errors) errors->append(error);
            continue;
        }

        descriptors.append(descriptor);
    }

    return descriptors;
}
