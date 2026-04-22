#include "Framework/Platform/Kernel/PlatformCtkPolicyBridge.h"

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
    const QString& ctkSymbolicName)
{
    const QString trimmed = ctkSymbolicName.trimmed();

    for (const auto& descriptor : descriptors) {
        if (descriptor.runtime.ctkSymbolicName.trimmed() == trimmed) return &descriptor;
    }

    for (const auto& descriptor : descriptors) {
        if (descriptor.runtime.ctkSymbolicName.trimmed().compare(trimmed, Qt::CaseInsensitive) == 0) return &descriptor;
    }

    const QString normalized = normalizedLookupKey(trimmed);
    for (const auto& descriptor : descriptors) {
        if (normalizedLookupKey(descriptor.runtime.ctkSymbolicName) == normalized) return &descriptor;
    }

    return nullptr;
}

PlatformCtkLoadBucket resolveLoadBucket(
    const PlatformRuntimeConfig& runtimeConfig,
    const PlatformPluginDescriptor& descriptor)
{
    if (descriptor.runtime.startupPolicy == PlatformStartupPolicy::OnDemand) return PlatformCtkLoadBucket::OnDemand;
    if (runtimeConfig.corePluginIds.contains(descriptor.id)) return PlatformCtkLoadBucket::Immediate;
    return PlatformCtkLoadBucket::Deferred;
}
}

PlatformCtkPolicyBridgeResult PlatformCtkPolicyBridge::resolve(
    const PlatformRuntimeConfig& runtimeConfig,
    const QVector<PlatformPluginDescriptor>& descriptors,
    const QString& ctkSymbolicName)
{
    PlatformCtkPolicyBridgeResult result;
    result.ctkSymbolicName = ctkSymbolicName.trimmed();

    const auto* descriptor = findDescriptorBySymbolicName(descriptors, ctkSymbolicName);
    if (!descriptor) {
        result.diagnosticCode = QStringLiteral("descriptor_missing_for_ctk_policy_bridge");
        return result;
    }

    result.resolvedPluginId = descriptor->id;
    result.ctkSymbolicName = descriptor->runtime.ctkSymbolicName.trimmed();
    result.loadBucket = resolveLoadBucket(runtimeConfig, *descriptor);
    result.isCritical = runtimeConfig.corePluginIds.contains(descriptor->id);
    result.resolutionStatus = PlatformCtkPolicyResolutionStatus::ResolvedFromDescriptor;
    return result;
}
