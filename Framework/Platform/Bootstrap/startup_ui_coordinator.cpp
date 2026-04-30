#include "Framework/Platform/Bootstrap/startup_ui_coordinator.h"

#include "Framework/StartupOrchestrator.h"
#include "UI/MainInterfaceWidget.h"

#include <QApplication>
#include <QMessageBox>
#include <QWidget>

StartupUiCoordinator::StartupUiCoordinator(
    StartupBootstrapController*,
    QPointer<MainInterfaceWidget>* mainInterface,
    bool safeMode)
    : m_mainInterface(mainInterface)
    , m_safeMode(safeMode)
{
}

void StartupUiCoordinator::bindToStartupCompletion(QApplication* app)
{
    if (!app) {
        return;
    }

    QObject::connect(StartupOrchestrator::instance(), &StartupOrchestrator::startupCompleted, app, [this](bool success) {
        const QString reportText = StartupOrchestrator::instance()->getDiagnosticReport();
        if (!success) {
            handleStartupFailure(reportText);
            return;
        }

        if (m_safeMode) {
            showSafeModeNotice(reportText);
        }
    });
}

void StartupUiCoordinator::handleStartupFailure(const QString& reportText) const
{
    qWarning() << "[Startup] Background startup reported failures; staying on in-app welcome page";
    qWarning().noquote() << reportText;
}

void StartupUiCoordinator::showSafeModeNotice(const QString& reportText) const
{
    if (!m_mainInterface || !*m_mainInterface) {
        return;
    }

    auto* messageHost = static_cast<QWidget*>(m_mainInterface->data());
    if (!messageHost) {
        return;
    }

    QMessageBox::information(
        messageHost,
        QObject::tr("安全模式"),
        QObject::tr("应用正在安全模式下运行，部分可选插件已被跳过。\n\n诊断摘要：\n%1").arg(reportText));
}
