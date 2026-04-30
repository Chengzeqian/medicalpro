#ifndef STARTUP_UI_COORDINATOR_H
#define STARTUP_UI_COORDINATOR_H

#include "FrameworkExport.h"

#include <functional>
#include <QString>

class QApplication;

class FRAMEWORK_EXPORT StartupUiCoordinator
{
public:
    using ReportHandler = std::function<void(const QString&)>;

    StartupUiCoordinator(ReportHandler failureHandler, ReportHandler safeModeHandler, bool safeMode);

    void bindToStartupCompletion(QApplication* app);

private:
    ReportHandler m_failureHandler;
    ReportHandler m_safeModeHandler;
    bool m_safeMode = false;
};

#endif // STARTUP_UI_COORDINATOR_H
