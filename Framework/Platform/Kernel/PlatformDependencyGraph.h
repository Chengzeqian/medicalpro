#pragma once

#include "Framework/Platform/Contracts/PlatformPluginDescriptor.h"

#include <QHash>
#include <QStringList>
#include <QVector>

struct PlatformDependencyGraphResult
{
    QHash<QString, QStringList> outgoingEdges;
    QStringList coreStartupOrder;
    QStringList errors;
};

class PlatformDependencyGraph
{
public:
    static PlatformDependencyGraphResult build(const QVector<PlatformPluginDescriptor>& descriptors);
};
