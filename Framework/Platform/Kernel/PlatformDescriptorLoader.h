#pragma once

#include "Framework/Platform/Contracts/PlatformPluginDescriptor.h"

#include <QStringList>
#include <QVector>

class PlatformDescriptorLoader
{
public:
    static PlatformPluginDescriptor loadFromFile(const QString& filePath, QString* error = nullptr);
    static QVector<PlatformPluginDescriptor> loadFromDirectory(const QString& directoryPath, QStringList* errors = nullptr);
};
