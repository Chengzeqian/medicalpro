#include "UI/NewPages/Navigation/navigation_workspace_ui_binder.h"

#include <QLabel>
#include <QPushButton>
#include <QVariant>
#include <QWidget>

NavigationWorkspaceUiBinder::NavigationWorkspaceUiBinder(Bindings bindings)
    : m_bindings(std::move(bindings))
{
}

void NavigationWorkspaceUiBinder::refreshFromSnapshot(const NavigationWorkspaceSnapshot& snapshot) const
{
    applyWorkspaceSummary(snapshot);

    for (auto it = m_bindings.workflowRailButtons.cbegin(); it != m_bindings.workflowRailButtons.cend(); ++it) {
        auto* button = it.value().data();
        if (!button) {
            continue;
        }

        const bool active = it.key() == snapshot.caseContext.currentStage;
        button->setChecked(active);
        button->setProperty("workflowState", active ? QStringLiteral("active") : QStringLiteral("idle"));
        button->setEnabled(snapshot.stageGate.allowed || active || it.key() == AnkleWorkflowStage::Preparation);
        if (m_bindings.widgetPolisher) {
            m_bindings.widgetPolisher(button);
        }
    }

    applyStageGate(snapshot.stageGate);
    applyNavigationConfidence(snapshot.navigationState, snapshot.stageGate);
    applyCalibrationSummary(snapshot.calibrationState);
}

void NavigationWorkspaceUiBinder::applyWorkspaceSummary(const NavigationWorkspaceSnapshot& snapshot) const
{
    if (m_bindings.caseStatusLabel) {
        const QString summary = snapshot.caseContext.patientName.isEmpty()
            ? snapshot.caseId
            : QStringLiteral("%1 | \u60a3\u8005\uff1a%2").arg(snapshot.caseId, snapshot.caseContext.patientName);
        m_bindings.caseStatusLabel->setText(summary);
        applyTone(m_bindings.caseStatusLabel, snapshot.caseId.isEmpty() ? QStringLiteral("warning") : QStringLiteral("ok"));
    }

    if (m_bindings.stageSummaryLabel) {
        const QString summary = snapshot.stageGate.allowed
            ? QStringLiteral("\u5f53\u524d\u9636\u6bb5\uff1a%1 | \u95e8\u7981\uff1a\u5df2\u6ee1\u8db3")
                  .arg(stageTitle(snapshot.caseContext.currentStage))
            : QStringLiteral("\u5f53\u524d\u9636\u6bb5\uff1a%1 | \u95e8\u7981\uff1a%2")
                  .arg(stageTitle(snapshot.caseContext.currentStage), snapshot.stageGate.reasonText);
        m_bindings.stageSummaryLabel->setText(summary);
        applyTone(m_bindings.stageSummaryLabel, snapshot.stageGate.allowed ? QStringLiteral("ok") : snapshot.stageGate.severity);
    }
}

void NavigationWorkspaceUiBinder::applyStageGate(const NavigationStageGate& gate) const
{
    if (m_bindings.navigationReadinessLabel) {
        m_bindings.navigationReadinessLabel->setText(QStringLiteral("\u5bfc\u822a\u51c6\u5165\uff1a%1")
                                                         .arg(gate.allowed ? QStringLiteral("\u5df2\u6ee1\u8db3") : gate.reasonText));
        applyTone(m_bindings.navigationReadinessLabel, gate.allowed ? QStringLiteral("ok") : gate.severity);
    }

    if (m_bindings.startNavigationButton && gate.requestedStage == AnkleWorkflowStage::Navigation) {
        m_bindings.startNavigationButton->setEnabled(gate.allowed);
    }
}

void NavigationWorkspaceUiBinder::applyCalibrationSummary(
    const NavigationWorkspaceCalibrationState& calibrationState) const
{
    if (!m_bindings.calibrationStatusLabel) {
        return;
    }

    if (!calibrationState.trackingReady) {
        m_bindings.calibrationStatusLabel->setText(
            QStringLiteral("\u6807\u5b9a\u72b6\u6001\uff1a\u8bf7\u5148\u8fde\u63a5\u8ffd\u8e2a\u5668\u68b0\u3002"));
        applyTone(m_bindings.calibrationStatusLabel, QStringLiteral("warning"));
        return;
    }

    if (calibrationState.completed) {
        m_bindings.calibrationStatusLabel->setText(
            QStringLiteral("\u6807\u5b9a\u72b6\u6001\uff1a\u5df2\u5b8c\u6210 | \u7cbe\u5ea6\uff1a%1 mm")
                .arg(calibrationState.accuracy, 0, 'f', 3));
        applyTone(m_bindings.calibrationStatusLabel, QStringLiteral("ok"));
        return;
    }

    if (!calibrationState.started) {
        m_bindings.calibrationStatusLabel->setText(QStringLiteral("\u6807\u5b9a\u72b6\u6001\uff1a\u5f85\u5f00\u59cb"));
        applyTone(m_bindings.calibrationStatusLabel, QStringLiteral("warning"));
        return;
    }

    const QString statusText = calibrationState.statusText.isEmpty()
        ? QStringLiteral("\u8fdb\u884c\u4e2d")
        : calibrationState.statusText;
    m_bindings.calibrationStatusLabel->setText(QStringLiteral("\u6807\u5b9a\u72b6\u6001\uff1a%1\uff08%2/%3\uff09")
                                                   .arg(statusText)
                                                   .arg(calibrationState.collectedPoints)
                                                   .arg(calibrationState.requiredPoints));
    applyTone(
        m_bindings.calibrationStatusLabel,
        calibrationState.requiredPoints > 0 && calibrationState.collectedPoints >= calibrationState.requiredPoints
            ? QStringLiteral("ok")
            : QStringLiteral("warning"));
}

void NavigationWorkspaceUiBinder::applyNavigationConfidence(
    const NavigationWorkspaceNavigationState& navigationState,
    const NavigationStageGate& gate,
    bool allowPendingState) const
{
    if (!m_bindings.navigationConfidenceLabel) {
        return;
    }

    if (allowPendingState && navigationState.confidence <= 0.0 && !gate.allowed) {
        m_bindings.navigationConfidenceLabel->setText(QStringLiteral("\u53ef\u4fe1\u5ea6\u8bc4\u5206\uff1a\u5f85\u8bc4\u4f30"));
        applyTone(m_bindings.navigationConfidenceLabel, QStringLiteral("warning"));
        return;
    }

    const QString recommendationText = navigationState.blockReasons.isEmpty()
        ? QStringLiteral("\u65e0")
        : navigationState.blockReasons.join(QStringLiteral("\uff1b"));
    m_bindings.navigationConfidenceLabel->setText(QStringLiteral("\u53ef\u4fe1\u5ea6\u8bc4\u5206\uff1a%1 | \u5efa\u8bae\uff1a%2")
                                                      .arg(navigationState.confidence, 0, 'f', 2)
                                                      .arg(recommendationText));
    applyTone(m_bindings.navigationConfidenceLabel, navigationState.allowNavigation ? QStringLiteral("ok") : QStringLiteral("warning"));
}

QString NavigationWorkspaceUiBinder::stageTitle(AnkleWorkflowStage stage) const
{
    switch (stage) {
    case AnkleWorkflowStage::Preparation:
        return QStringLiteral("\u51c6\u5907");
    case AnkleWorkflowStage::Planning:
        return QStringLiteral("\u89c4\u5212");
    case AnkleWorkflowStage::Registration:
        return QStringLiteral("\u914d\u51c6");
    case AnkleWorkflowStage::Navigation:
        return QStringLiteral("\u5bfc\u822a");
    case AnkleWorkflowStage::Evaluation:
        return QStringLiteral("\u8bc4\u4f30");
    }

    return QStringLiteral("\u51c6\u5907");
}

void NavigationWorkspaceUiBinder::applyTone(QWidget* widget, const QString& tone) const
{
    if (!widget) {
        return;
    }

    if (m_bindings.toneApplier) {
        m_bindings.toneApplier(widget, tone);
    }
}
