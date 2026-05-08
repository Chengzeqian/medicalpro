#pragma once

#include "UI/NewPages/Navigation/navigation_workspace_types.h"
#include "UI/NewPages/Navigation/navigation_workflow_stage.h"

#include <QHash>
#include <QPointer>
#include <QString>
#include <functional>

class QLabel;
class QPushButton;
class QWidget;

class NavigationWorkspaceUiBinder
{
public:
    struct Bindings
    {
        QLabel* caseStatusLabel = nullptr;
        QLabel* stageSummaryLabel = nullptr;
        QLabel* navigationReadinessLabel = nullptr;
        QLabel* navigationConfidenceLabel = nullptr;
        QLabel* calibrationStatusLabel = nullptr;
        QPushButton* startNavigationButton = nullptr;
        QHash<AnkleWorkflowStage, QPointer<QPushButton>> workflowRailButtons;
        std::function<void(QWidget*, const QString&)> toneApplier;
        std::function<void(QPushButton*)> widgetPolisher;
    };

    explicit NavigationWorkspaceUiBinder(Bindings bindings = {});

    void refreshFromSnapshot(const NavigationWorkspaceSnapshot& snapshot) const;
    void applyWorkspaceSummary(const NavigationWorkspaceSnapshot& snapshot) const;
    void applyStageGate(const NavigationStageGate& gate) const;
    void applyCalibrationSummary(const NavigationWorkspaceCalibrationState& calibrationState) const;
    void applyPreparationSummary(const NavigationWorkspacePreparationState& state) const;
    void applyNavigationConfidence(
        const NavigationWorkspaceNavigationState& navigationState,
        const NavigationStageGate& gate,
        bool allowPendingState = true) const;

private:
    QString stageTitle(AnkleWorkflowStage stage) const;
    void applyTone(QWidget* widget, const QString& tone) const;

    Bindings m_bindings;
};
