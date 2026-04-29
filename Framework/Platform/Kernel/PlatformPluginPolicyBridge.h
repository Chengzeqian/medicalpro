#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/PlatformPluginDescriptor.h"
#include "Framework/Platform/Kernel/PlatformRuntimeConfig.h"

#include <QString>
#include <QVector>

enum class PlatformPluginLoadBucket
{
    Immediate,
    Deferred,
    OnDemand
};

enum class PlatformPluginPolicyResolutionStatus
{
    ResolvedFromDescriptor,
    DescriptorMissingFallback
};

struct FRAMEWORK_EXPORT PlatformPluginPolicyBridgeResult
{
    QString resolvedPluginId;
    QString symbolicName;
    PlatformPluginLoadBucket loadBucket = PlatformPluginLoadBucket::OnDemand;
    bool isCritical = false;
    PlatformPluginPolicyResolutionStatus resolutionStatus = PlatformPluginPolicyResolutionStatus::DescriptorMissingFallback;
    QString diagnosticCode;
};

class FRAMEWORK_EXPORT PlatformPluginPolicyBridge
{
public:
    static PlatformPluginPolicyBridgeResult resolve(
        const PlatformRuntimeConfig& runtimeConfig,
        const QVector<PlatformPluginDescriptor>& descriptors,
        const QString& symbolicName);
};
