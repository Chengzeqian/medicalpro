#ifndef STARTUP_UI_COORDINATOR_H
#define STARTUP_UI_COORDINATOR_H

#include "FrameworkExport.h"

#include <QPointer>
#include <QString>

class QApplication;
class MainInterfaceWidget;
class StartupBootstrapController;

class FRAMEWORK_EXPORT StartupUiCoordinator
{
public:
    StartupUiCoordinator(
        StartupBootstrapController* bootstrapController,
        QPointer<MainInterfaceWidget>* mainInterface,
        bool safeMode);

    void bindToStartupCompletion(QApplication* app);

private:
    void handleStartupFailure(const QString& reportText) const;
    void showSafeModeNotice(const QString& reportText) const;

    QPointer<MainInterfaceWidget>* m_mainInterface = nullptr;
    bool m_safeMode = false;
};

#endif // STARTUP_UI_COORDINATOR_H
