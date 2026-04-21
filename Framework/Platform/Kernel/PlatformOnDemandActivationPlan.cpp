#include "Framework/Platform/Kernel/PlatformOnDemandActivationPlan.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSet>

namespace
{
void assignError(QString* error, const QString& value)
{
    if (error) *error = value;
}

QString resolveBundlePath(const QString& pluginDirectory, const QString& ctkSymbolicName)
{
    if (pluginDirectory.trimmed().isEmpty() || ctkSymbolicName.trimmed().isEmpty()) return {};

    const QDir directory(pluginDirectory);
    const auto bundleFiles = directory.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const auto& bundleFile : bundleFiles) {
        const auto baseName = bundleFile.completeBaseName();
        if (baseName.compare(ctkSymbolicName, Qt::CaseInsensitive) == 0) return bundleFile.absoluteFilePath();
        if (baseName.compare(QStringLiteral("lib%1").arg(ctkSymbolicName), Qt::CaseInsensitive) == 0) {
            return bundleFile.absoluteFilePath();
        }
    }

    return {};
}

bool hasStrictOnDemandContract(const PlatformPluginDescriptor& descriptor, QString* error)
{
    if (descriptor.runtime.ctkSymbolicName.trimmed().isEmpty()) {
        assignError(error, QStringLiteral("descriptor_missing: runtime.ctk_symbolic_name"));
        return false;
    }
    if (descriptor.runtime.startupPolicy != PlatformStartupPolicy::OnDemand) {
        assignError(error, QStringLiteral("diagnostics_contract_missing: startup_policy must be on_demand"));
        return false;
    }
    if (descriptor.runtime.bootstrapLevel != PlatformBootstrapLevel::Deferred) {
        assignError(error, QStringLiteral("diagnostics_contract_missing: bootstrap_level must be deferred"));
        return false;
    }
    if (descriptor.diagnostics.requiredServices.isEmpty()) {
        assignError(error, QStringLiteral("diagnostics_contract_missing: required_services"));
        return false;
    }
    if (descriptor.diagnostics.serviceReadyTimeoutMs <= 0) {
        assignError(error, QStringLiteral("diagnostics_contract_missing: service_ready_timeout_ms"));
        return false;
    }
    if (descriptor.healthChecks.isEmpty()) {
        assignError(error, QStringLiteral("diagnostics_contract_missing: health_checks"));
        return false;
    }
    return true;
}

void appendDescriptorRecursively(
    const QString& pluginId,
    const QHash<QString, PlatformPluginDescriptor>& descriptorsById,
    QSet<QString>& visitedPluginIds,
    QVector<PlatformPluginDescriptor>& orderedDescriptors,
    QString* error)
{
    const auto trimmedPluginId = pluginId.trimmed();
    if (trimmedPluginId.isEmpty()) {
        assignError(error, QStringLiteral("on_demand plugin id is empty"));
        return;
    }

    if (visitedPluginIds.contains(trimmedPluginId)) return;
    if (!descriptorsById.contains(trimmedPluginId)) {
        assignError(error, QStringLiteral("missing descriptor for on_demand plugin: %1").arg(trimmedPluginId));
        return;
    }

    const auto descriptor = descriptorsById.value(trimmedPluginId);
    visitedPluginIds.insert(trimmedPluginId);

    for (const auto& requiredPluginId : descriptor.required.plugins) {
        appendDescriptorRecursively(requiredPluginId, descriptorsById, visitedPluginIds, orderedDescriptors, error);
        if (error && !error->isEmpty()) return;
    }

    orderedDescriptors.append(descriptor);
}
}

PlatformOnDemandActivationPlan PlatformOnDemandActivationPlanBuilder::build(
    const QString& targetPluginId,
    const QVector<PlatformPluginDescriptor>& descriptors,
    const QString& pluginDirectory,
    QString* error)
{
    if (error) error->clear();

    QHash<QString, PlatformPluginDescriptor> descriptorsById;
    descriptorsById.reserve(descriptors.size());
    for (const auto& descriptor : descriptors) {
        descriptorsById.insert(descriptor.id, descriptor);
    }

    QVector<PlatformPluginDescriptor> orderedDescriptors;
    QSet<QString> visitedPluginIds;
    appendDescriptorRecursively(targetPluginId, descriptorsById, visitedPluginIds, orderedDescriptors, error);
    if (error && !error->isEmpty()) return {};

    PlatformOnDemandActivationPlan plan;
    plan.targetPluginId = targetPluginId.trimmed();
    plan.activationEntries.reserve(orderedDescriptors.size());

    for (const auto& descriptor : orderedDescriptors) {
        if (!hasStrictOnDemandContract(descriptor, error)) return {};

        const auto bundleFilePath = resolveBundlePath(pluginDirectory, descriptor.runtime.ctkSymbolicName.trimmed());
        if (bundleFilePath.isEmpty()) {
            assignError(
                error,
                QStringLiteral("on_demand bundle path not found for ctk_symbolic_name: %1")
                    .arg(descriptor.runtime.ctkSymbolicName));
            return {};
        }

        PlatformOnDemandActivationPlanEntry entry;
        entry.pluginId = descriptor.id;
        entry.displayName = descriptor.displayName;
        entry.ctkSymbolicName = descriptor.runtime.ctkSymbolicName.trimmed();
        entry.bundleFilePath = bundleFilePath;
        entry.requiredPlugins = descriptor.required.plugins;
        entry.requiredCapabilities = descriptor.required.capabilities;
        entry.requiredServices = descriptor.diagnostics.requiredServices;
        entry.healthChecks = descriptor.healthChecks;
        entry.serviceReadyTimeoutMs = descriptor.diagnostics.serviceReadyTimeoutMs;
        entry.target = descriptor.id == plan.targetPluginId;
        plan.activationEntries.append(entry);
    }

    return plan;
}
