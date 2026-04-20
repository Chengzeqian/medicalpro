#pragma once

#include "Framework/FrameworkExport.h"

#include <QString>
#include <QStringList>

class ICoreUiRuntimeStatusPort;

struct FRAMEWORK_EXPORT CoreUiRuntimeStatusSnapshot
{
    bool frameworkReady = false;
    bool workflowReady = false;
    int pluginCount = 0;
    int readyServices = 0;
    int totalServices = 0;
    bool dataDirectoryExists = false;
    bool dataDirectoryReadable = false;
    QStringList missingServices;
};

class FRAMEWORK_EXPORT CoreUiRuntimeStatusProvider
{
public:
    CoreUiRuntimeStatusProvider(ICoreUiRuntimeStatusPort* port, QString dataDirectoryPath);

    CoreUiRuntimeStatusSnapshot welcomeSnapshot() const;
    CoreUiRuntimeStatusSnapshot moduleSelectionSnapshot() const;
    CoreUiRuntimeStatusSnapshot systemSettingsSnapshot() const;

private:
    CoreUiRuntimeStatusSnapshot buildSnapshot(int pluginCount) const;

    ICoreUiRuntimeStatusPort* m_port;
    QString m_dataDirectoryPath;
};
