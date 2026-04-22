#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/PlatformPluginDescriptor.h"
#include "Framework/Platform/Kernel/PlatformRuntimeConfig.h"

#include <QString>
#include <QVector>

enum class PlatformCtkLoadBucket
{
    Immediate,
    Deferred,
    OnDemand
};

enum class PlatformCtkPolicyResolutionStatus
{
    ResolvedFromDescriptor,
    DescriptorMissingFallback
};

struct FRAMEWORK_EXPORT PlatformCtkPolicyBridgeResult
{
    QString resolvedPluginId;
    QString ctkSymbolicName;
    PlatformCtkLoadBucket loadBucket = PlatformCtkLoadBucket::OnDemand;
    bool isCritical = false;
    PlatformCtkPolicyResolutionStatus resolutionStatus = PlatformCtkPolicyResolutionStatus::DescriptorMissingFallback;
    QString diagnosticCode;
};

class FRAMEWORK_EXPORT PlatformCtkPolicyBridge
{
public:
    static PlatformCtkPolicyBridgeResult resolve(
        const PlatformRuntimeConfig& runtimeConfig,
        const QVector<PlatformPluginDescriptor>& descriptors,
        const QString& ctkSymbolicName);
};
