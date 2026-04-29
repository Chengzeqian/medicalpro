#include "Framework/Platform/Kernel/PlatformPluginPolicyBridge.h"

namespace
{
QString trimAndLower(const QString& value)
{
    return value.trimmed().toLower();
}

QString normalizedLookupKey(const QString& value)
{
    QString normalized = trimAndLower(value);
    if (normalized.startsWith(QStringLiteral("lib"))) normalized.remove(0, 3);
    return normalized;
}

const PlatformPluginDescriptor* findDescriptorBySymbolicName(
    const QVector<PlatformPluginDescriptor>& descriptors,
    const QString& symbolicName)
{
    const QString trimmed = symbolicName.trimmed();

    for (const auto& descriptor : descriptors) {
        if (descriptor.runtime.symbolicName.trimmed() == trimmed) return &descriptor;
    }

    for (const auto& descriptor : descriptors) {
        if (descriptor.runtime.symbolicName.trimmed().compare(trimmed, Qt::CaseInsensitive) == 0) return &descriptor;
    }

    const QString normalized = normalizedLookupKey(trimmed);
    for (const auto& descriptor : descriptors) {
        if (normalizedLookupKey(descriptor.runtime.symbolicName) == normalized) return &descriptor;
    }

    return nullptr;
}

PlatformPluginLoadBucket resolveLoadBucket(
    const PlatformRuntimeConfig& runtimeConfig,
    const PlatformPluginDescriptor& descriptor)
{
    if (descriptor.runtime.startupPolicy == PlatformStartupPolicy::OnDemand) return PlatformPluginLoadBucket::OnDemand;
    if (runtimeConfig.corePluginIds.contains(descriptor.id)) return PlatformPluginLoadBucket::Immediate;
    return PlatformPluginLoadBucket::Deferred;
}
}

PlatformPluginPolicyBridgeResult PlatformPluginPolicyBridge::resolve(
    const PlatformRuntimeConfig& runtimeConfig,
    const QVector<PlatformPluginDescriptor>& descriptors,
    const QString& symbolicName)
{
    PlatformPluginPolicyBridgeResult result;
    result.symbolicName = symbolicName.trimmed();

    const auto* descriptor = findDescriptorBySymbolicName(descriptors, symbolicName);
    if (!descriptor) {
        result.diagnosticCode = QStringLiteral("descriptor_missing_for_plugin_policy_bridge");
        return result;
    }

    result.resolvedPluginId = descriptor->id;
    result.symbolicName = descriptor->runtime.symbolicName.trimmed();
    result.loadBucket = resolveLoadBucket(runtimeConfig, *descriptor);
    result.isCritical = runtimeConfig.corePluginIds.contains(descriptor->id);
    result.resolutionStatus = PlatformPluginPolicyResolutionStatus::ResolvedFromDescriptor;
    return result;
}
