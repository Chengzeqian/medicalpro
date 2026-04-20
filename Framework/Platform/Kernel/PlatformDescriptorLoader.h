#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/PlatformPluginDescriptor.h"

#include <QStringList>
#include <QVector>

class FRAMEWORK_EXPORT PlatformDescriptorLoader
{
public:
    static PlatformPluginDescriptor loadFromFile(const QString& filePath, QString* error = nullptr);
    static QVector<PlatformPluginDescriptor> loadFromDirectory(const QString& directoryPath, QStringList* errors = nullptr);
};
