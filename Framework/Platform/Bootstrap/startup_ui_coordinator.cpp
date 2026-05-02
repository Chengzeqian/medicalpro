#include "Framework/Platform/Bootstrap/startup_ui_coordinator.h"

#include "Framework/StartupOrchestrator.h"

#include <QApplication>

#include <utility>

StartupUiCoordinator::StartupUiCoordinator(
    ReportHandler failureHandler,
    ReportHandler safeModeHandler,
    bool safeMode)
    : m_failureHandler(std::move(failureHandler))
    , m_safeModeHandler(std::move(safeModeHandler))
    , m_safeMode(safeMode)
{
}

void StartupUiCoordinator::bindToStartupCompletion(QApplication* app)
{
    if (!app) {
        return;
    }

    const auto failureHandler = m_failureHandler;
    const auto safeModeHandler = m_safeModeHandler;
    const bool safeMode = m_safeMode;

    QObject::connect(
        StartupOrchestrator::instance(),
        &StartupOrchestrator::startupCompleted,
        app,
        [failureHandler, safeModeHandler, safeMode](bool success) {
            const QString reportText = StartupOrchestrator::instance()->getDiagnosticReport();
            if (!success) {
                if (failureHandler) {
                    failureHandler(reportText);
                }
                return;
            }

            if (safeMode && safeModeHandler) {
                safeModeHandler(reportText);
            }
        });
}
