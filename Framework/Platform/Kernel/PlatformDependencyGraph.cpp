#include "Framework/Platform/Kernel/PlatformDependencyGraph.h"

namespace
{
void appendEdge(
    PlatformDependencyGraphResult* result,
    QHash<QString, int>* indegree,
    const QString& from,
    const QString& to)
{
    auto edges = result->outgoingEdges.value(from);
    if (edges.contains(to)) return;

    edges.append(to);
    result->outgoingEdges.insert(from, edges);
    indegree->insert(to, indegree->value(to) + 1);
}
}

PlatformDependencyGraphResult PlatformDependencyGraph::build(const QVector<PlatformPluginDescriptor>& descriptors)
{
    PlatformDependencyGraphResult result;
    QHash<QString, PlatformPluginDescriptor> descriptorsById;
    QHash<QString, QString> capabilityProviders;
    QHash<QString, int> indegree;
    int corePluginCount = 0;

    for (const auto& descriptor : descriptors) {
        descriptorsById.insert(descriptor.id, descriptor);
        indegree.insert(descriptor.id, 0);
        result.outgoingEdges.insert(descriptor.id, {});

        if (descriptor.runtime.bootstrapLevel == PlatformBootstrapLevel::Core) ++corePluginCount;

        for (const auto& capability : descriptor.provides.capabilities) {
            if (!capabilityProviders.contains(capability)) capabilityProviders.insert(capability, descriptor.id);
        }
    }

    for (const auto& descriptor : descriptors) {
        for (const auto& requiredPluginId : descriptor.required.plugins) {
            if (!descriptorsById.contains(requiredPluginId)) {
                result.errors.append(QStringLiteral("Missing required plugin: %1").arg(requiredPluginId));
                continue;
            }

            const auto provider = descriptorsById.value(requiredPluginId);
            if (descriptor.runtime.bootstrapLevel == PlatformBootstrapLevel::Core
                && provider.runtime.startupPolicy == PlatformStartupPolicy::OnDemand) {
                result.errors.append(
                    QStringLiteral("core plugin %1 cannot require on_demand plugin %2")
                        .arg(descriptor.id, requiredPluginId));
            }

            appendEdge(&result, &indegree, requiredPluginId, descriptor.id);
        }

        for (const auto& capability : descriptor.required.capabilities) {
            const auto providerId = capabilityProviders.value(capability);
            if (providerId.isEmpty()) {
                result.errors.append(QStringLiteral("Missing capability provider: %1").arg(capability));
                continue;
            }

            const auto provider = descriptorsById.value(providerId);
            if (descriptor.runtime.bootstrapLevel == PlatformBootstrapLevel::Core
                && provider.runtime.startupPolicy == PlatformStartupPolicy::OnDemand) {
                result.errors.append(
                    QStringLiteral("core plugin %1 cannot require on_demand capability %2")
                        .arg(descriptor.id, capability));
            }

            appendEdge(&result, &indegree, providerId, descriptor.id);
        }
    }

    QStringList queue;
    for (const auto& descriptor : descriptors) {
        if (descriptor.runtime.bootstrapLevel != PlatformBootstrapLevel::Core) continue;
        if (indegree.value(descriptor.id) == 0) queue.append(descriptor.id);
    }

    while (!queue.isEmpty()) {
        const auto current = queue.takeFirst();
        result.coreStartupOrder.append(current);

        for (const auto& next : result.outgoingEdges.value(current)) {
            indegree.insert(next, indegree.value(next) - 1);
            if (indegree.value(next) != 0) continue;

            const auto nextDescriptor = descriptorsById.value(next);
            if (nextDescriptor.runtime.bootstrapLevel == PlatformBootstrapLevel::Core) queue.append(next);
        }
    }

    if (result.errors.isEmpty() && result.coreStartupOrder.size() != corePluginCount) {
        result.errors.append(QStringLiteral("core dependency cycle detected"));
    }

    return result;
}
