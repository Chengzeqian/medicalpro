#include "Framework/Platform/Kernel/PlatformManagedPluginPlan.h"

#include "Framework/Platform/Kernel/PlatformDependencyGraph.h"

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

bool validateManagedDescriptor(const PlatformPluginDescriptor& descriptor, QString* error)
{
    QStringList missingFields;

    if (descriptor.runtime.ctkSymbolicName.trimmed().isEmpty()) {
        missingFields.append(QStringLiteral("runtime.ctk_symbolic_name"));
    }
    if (descriptor.diagnostics.requiredServices.isEmpty()) {
        missingFields.append(QStringLiteral("diagnostics.required_services"));
    }
    if (descriptor.diagnostics.serviceReadyTimeoutMs <= 0) {
        missingFields.append(QStringLiteral("diagnostics.service_ready_timeout_ms"));
    }
    if (descriptor.healthChecks.isEmpty()) {
        missingFields.append(QStringLiteral("health_checks"));
    }

    if (missingFields.isEmpty()) return true;

    assignError(
        error,
        QStringLiteral("descriptor missing phase1 diagnostics contract fields: %1 | plugin_id=%2")
            .arg(missingFields.join(QStringLiteral(", ")), descriptor.id));
    return false;
}

void appendManagedDescriptorRecursively(
    const QString& pluginId,
    const QHash<QString, PlatformPluginDescriptor>& descriptorsById,
    const QHash<QString, QString>& capabilityProviders,
    QSet<QString>& visitedPluginIds,
    QVector<PlatformPluginDescriptor>& managedDescriptors,
    QString* error)
{
    if (pluginId.trimmed().isEmpty()) {
        assignError(error, QStringLiteral("managed plugin id is empty"));
        return;
    }

    if (visitedPluginIds.contains(pluginId)) return;
    if (!descriptorsById.contains(pluginId)) {
        assignError(error, QStringLiteral("missing descriptor for managed plugin: %1").arg(pluginId));
        return;
    }

    const auto descriptor = descriptorsById.value(pluginId);
    visitedPluginIds.insert(pluginId);

    for (const auto& requiredPluginId : descriptor.required.plugins) {
        appendManagedDescriptorRecursively(
            requiredPluginId,
            descriptorsById,
            capabilityProviders,
            visitedPluginIds,
            managedDescriptors,
            error);
        if (error && !error->isEmpty()) return;
    }

    for (const auto& requiredCapability : descriptor.required.capabilities) {
        const auto providerPluginId = capabilityProviders.value(requiredCapability);
        if (providerPluginId.isEmpty()) {
            assignError(error, QStringLiteral("missing provider for managed capability: %1").arg(requiredCapability));
            return;
        }

        appendManagedDescriptorRecursively(
            providerPluginId,
            descriptorsById,
            capabilityProviders,
            visitedPluginIds,
            managedDescriptors,
            error);
        if (error && !error->isEmpty()) return;
    }

    managedDescriptors.append(descriptor);
}
}

PlatformManagedPluginPlan PlatformManagedPluginPlanBuilder::build(
    const PlatformRuntimeConfig& runtimeConfig,
    const QVector<PlatformPluginDescriptor>& descriptors,
    const QString& pluginDirectory,
    QString* error)
{
    if (error) error->clear();

    QHash<QString, PlatformPluginDescriptor> descriptorsById;
    QHash<QString, QString> capabilityProviders;
    descriptorsById.reserve(descriptors.size());
    capabilityProviders.reserve(descriptors.size());

    for (const auto& descriptor : descriptors) {
        descriptorsById.insert(descriptor.id, descriptor);
        for (const auto& capability : descriptor.provides.capabilities) {
            if (!capabilityProviders.contains(capability)) capabilityProviders.insert(capability, descriptor.id);
        }
    }

    QVector<PlatformPluginDescriptor> managedDescriptors;
    managedDescriptors.reserve(runtimeConfig.corePluginIds.size());

    QSet<QString> visitedPluginIds;
    for (const auto& corePluginId : runtimeConfig.corePluginIds) {
        appendManagedDescriptorRecursively(
            corePluginId,
            descriptorsById,
            capabilityProviders,
            visitedPluginIds,
            managedDescriptors,
            error);
        if (error && !error->isEmpty()) return {};
    }

    const auto dependencyGraph = PlatformDependencyGraph::build(managedDescriptors);
    if (!dependencyGraph.errors.isEmpty()) {
        assignError(error, dependencyGraph.errors.join(QStringLiteral("; ")));
        return {};
    }

    PlatformManagedPluginPlan plan;
    plan.runtimeMode = runtimeConfig.runtimeMode;
    plan.managedPluginIds = dependencyGraph.coreStartupOrder;
    plan.corePluginIds = dependencyGraph.coreStartupOrder;
    plan.installEntries.reserve(dependencyGraph.coreStartupOrder.size());

    for (const auto& pluginId : dependencyGraph.coreStartupOrder) {
        if (!descriptorsById.contains(pluginId)) {
            assignError(error, QStringLiteral("missing descriptor for startup plan entry: %1").arg(pluginId));
            return {};
        }

        const auto descriptor = descriptorsById.value(pluginId);
        if (!validateManagedDescriptor(descriptor, error)) return {};

        const auto bundleFilePath = resolveBundlePath(pluginDirectory, descriptor.runtime.ctkSymbolicName.trimmed());
        if (bundleFilePath.isEmpty()) {
            assignError(
                error,
                QStringLiteral("managed bundle path not found for ctk_symbolic_name: %1")
                    .arg(descriptor.runtime.ctkSymbolicName));
            return {};
        }

        PlatformManagedPluginPlanEntry entry;
        entry.pluginId = descriptor.id;
        entry.displayName = descriptor.displayName;
        entry.ctkSymbolicName = descriptor.runtime.ctkSymbolicName.trimmed();
        entry.bundleFilePath = bundleFilePath;
        entry.bootstrapLevel = descriptor.runtime.bootstrapLevel;
        entry.startupPolicy = descriptor.runtime.startupPolicy;
        entry.requiredPlugins = descriptor.required.plugins;
        entry.requiredCapabilities = descriptor.required.capabilities;
        entry.requiredServices = descriptor.diagnostics.requiredServices;
        entry.healthChecks = descriptor.healthChecks;
        entry.serviceReadyTimeoutMs = descriptor.diagnostics.serviceReadyTimeoutMs;
        plan.installEntries.append(entry);
    }

    return plan;
}
