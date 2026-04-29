#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/PlatformRuntimeTypes.h"

#include <QString>
#include <QStringList>

struct FRAMEWORK_EXPORT PlatformRuntimeConfig
{
    PlatformRuntimeMode runtimeMode = PlatformRuntimeMode::ObserveOnly;
    QString descriptorDirectory;
    QStringList corePluginIds;

    static PlatformRuntimeConfig loadFromFile(const QString& filePath, QString* error = nullptr);
    QStringList resolveCoreSymbolicNames(const QString& descriptorDirectoryPath, QString* error = nullptr) const;
};
