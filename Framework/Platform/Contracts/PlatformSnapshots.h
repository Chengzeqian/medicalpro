#pragma once

#include "Framework/Platform/Contracts/PlatformRuntimeTypes.h"

#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

struct PlatformStartupTraceEntry
{
    QString phaseKey;
    QString phaseLabel;
    bool success = false;
    qint64 elapsedMs = 0;
    QString detail;
};

struct PlatformPluginRuntimeSnapshot
{
    QString pluginId;
    QString ctkSymbolicName;
    PlatformPluginState state = PlatformPluginState::Discovered;
    QStringList missingRequiredServices;
    QStringList missingRequiredCapabilities;
    QStringList missingRequiredPlugins;
};

struct PlatformCapabilitySnapshot
{
    PlatformRuntimeMode runtimeMode = PlatformRuntimeMode::ObserveOnly;
    bool platformReady = false;
    QStringList unlockedCapabilities;
    QStringList lockedCapabilities;
    QStringList degradedPlugins;
};

struct PlatformDiagnosticSnapshot
{
    bool frameworkReady = false;
    PlatformRuntimeMode runtimeMode = PlatformRuntimeMode::ObserveOnly;
    QVector<PlatformPluginRuntimeSnapshot> plugins;
    QVector<PlatformStartupTraceEntry> startupTrace;
    PlatformCapabilitySnapshot capabilitySnapshot;
    QStringList recoveryHints;
};
