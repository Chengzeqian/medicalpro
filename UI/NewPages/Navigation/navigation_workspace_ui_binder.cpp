#include "UI/NewPages/Navigation/navigation_workspace_ui_binder.h"

#include <QLabel>
#include <QPushButton>
#include <QVariant>
#include <QWidget>

namespace {

const QString formatTwinScalar(const QVariantMap& metrics, const QString& key)
{
    return QString::number(metrics.value(key).toDouble(), 'f', 2);
}

const QString digitalTwinSummaryOrEmpty(const QVariantMap& metrics)
{
    if (!metrics.contains(QStringLiteral("twin_confidence_score"))) {
        return QString();
    }

    const QString dominantRiskSource =
        metrics.value(QStringLiteral("dominant_risk_source")).toString();
    const QString reRegisterRecommended =
        metrics.value(QStringLiteral("re_register_recommended")).toBool() ? QStringLiteral("yes")
                                                                          : QStringLiteral("no");

    return QStringLiteral(
               "Twin: confidence=%1 | local_risk=%2 | target_distance=%3 mm | dominant_risk=%4 | "
               "re_register=%5")
        .arg(formatTwinScalar(metrics, QStringLiteral("twin_confidence_score")))
        .arg(formatTwinScalar(metrics, QStringLiteral("local_risk_score")))
        .arg(formatTwinScalar(metrics, QStringLiteral("target_region_distance_mm")))
        .arg(dominantRiskSource.isEmpty() ? QStringLiteral("unknown") : dominantRiskSource)
        .arg(reRegisterRecommended);
}

}

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
    if (!snapshot.preparationState.instrumentCalibrationStates.isEmpty()
        || !snapshot.preparationState.blockingReasons.isEmpty()
        || snapshot.preparationState.allRequiredInstrumentsCalibrated) {
        applyPreparationSummary(snapshot.preparationState);
    } else {
        applyCalibrationSummary(snapshot.calibrationState);
    }
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

void NavigationWorkspaceUiBinder::applyPreparationSummary(
    const NavigationWorkspacePreparationState& state) const
{
    if (!m_bindings.calibrationStatusLabel) {
        return;
    }

    if (state.allRequiredInstrumentsCalibrated) {
        m_bindings.calibrationStatusLabel->setText(QStringLiteral("标定状态：所有导航器械均已标定完成"));
        applyTone(m_bindings.calibrationStatusLabel, QStringLiteral("ok"));
        return;
    }

    const QString message = state.blockingReasons.isEmpty()
        ? QStringLiteral("标定状态：仍有器械未完成标定")
        : QStringLiteral("标定状态：%1").arg(state.blockingReasons.join(QStringLiteral("；")));
    m_bindings.calibrationStatusLabel->setText(message);
    applyTone(m_bindings.calibrationStatusLabel, QStringLiteral("warning"));
}

void NavigationWorkspaceUiBinder::applyPlanningSummary(
    const NavigationWorkspacePlanningState& planningState,
    const NavigationWorkspaceAssetState& assetState) const
{
    if (!m_bindings.planningSummaryLabel) {
        return;
    }

    if (!planningState.hasPlanning) {
        m_bindings.planningSummaryLabel->setText(
            QStringLiteral("规划摘要：尚未生成手术规划，请在病例工作包中完成规划"));
        applyTone(m_bindings.planningSummaryLabel, QStringLiteral("warning"));
        return;
    }

    QStringList lines;
    lines.append(QStringLiteral("目标骨：%1")
                     .arg(planningState.targetBone.isEmpty() ? QStringLiteral("未指定") : planningState.targetBone));
    lines.append(QStringLiteral("目标区域：%1")
                     .arg(planningState.targetRegion.isEmpty() ? QStringLiteral("未指定") : planningState.targetRegion));
    lines.append(QStringLiteral("解剖约束区：%1")
                     .arg(planningState.constraintRegions.isEmpty()
                              ? QStringLiteral("无")
                              : planningState.constraintRegions.join(QStringLiteral("、"))));
    lines.append(QStringLiteral("推荐配准点顺序：%1")
                     .arg(planningState.recommendedPointOrder.isEmpty()
                              ? QStringLiteral("未指定")
                              : planningState.recommendedPointOrder.join(QStringLiteral(" → "))));
    lines.append(QStringLiteral("已绑定骨模型：%1")
                     .arg(assetState.boundBoneAssets.isEmpty()
                              ? QStringLiteral("无")
                              : assetState.boundBoneAssets.join(QStringLiteral("、"))));
    lines.append(QStringLiteral("DICOM 影像：%1")
                     .arg(assetState.dicomReady ? QStringLiteral("已绑定") : QStringLiteral("未绑定")));

    m_bindings.planningSummaryLabel->setText(lines.join(QStringLiteral("\n")));
    applyTone(m_bindings.planningSummaryLabel, planningState.targetRegionReady ? QStringLiteral("ok") : QStringLiteral("warning"));
}

void NavigationWorkspaceUiBinder::applyRegistrationSummary(
    const NavigationWorkspaceRegistrationState& registrationState) const
{
    if (!m_bindings.registrationSummaryLabel) {
        return;
    }

    QStringList lines;
    lines.append(QStringLiteral("分骨结果：%1 个骨位")
                     .arg(registrationState.perBoneResults.size()));
    if (!registrationState.perBoneResults.isEmpty()) {
        QStringList details;
        for (const auto& bone : registrationState.perBoneResults) {
            details.append(QStringLiteral("%1(fre=%2mm)")
                               .arg(bone.boneAssetId.isEmpty() ? QStringLiteral("未命名") : bone.boneAssetId)
                               .arg(bone.fre, 0, 'f', 2));
        }
        lines.append(QStringLiteral("  · %1").arg(details.join(QStringLiteral("，"))));
    }
    lines.append(QStringLiteral("融合导航空间：%1")
                     .arg(registrationState.fusedNavigationSpaceReady
                              ? (registrationState.fusedNavigationSpacePath.isEmpty()
                                     ? QStringLiteral("已就绪")
                                     : QStringLiteral("已就绪（%1）").arg(registrationState.fusedNavigationSpacePath))
                              : QStringLiteral("未就绪")));
    lines.append(QStringLiteral("覆盖评分：%1").arg(registrationState.fusedCoverageScore, 0, 'f', 2));
    if (!registrationState.fusionBlockingReasons.isEmpty()) {
        lines.append(QStringLiteral("阻塞原因：%1").arg(registrationState.fusionBlockingReasons.join(QStringLiteral("；"))));
    }

    m_bindings.registrationSummaryLabel->setText(lines.join(QStringLiteral("\n")));
    applyTone(m_bindings.registrationSummaryLabel,
              registrationState.fusedNavigationSpaceReady ? QStringLiteral("ok") : QStringLiteral("warning"));
}

void NavigationWorkspaceUiBinder::applyEvaluationSummary(
    const NavigationWorkspaceEvaluationState& evaluationState) const
{
    if (!m_bindings.evaluationSummaryLabel) {
        return;
    }

    if (!evaluationState.hasSummary) {
        m_bindings.evaluationSummaryLabel->setText(QStringLiteral("评估摘要：尚未生成"));
        applyTone(m_bindings.evaluationSummaryLabel, QStringLiteral("warning"));
        return;
    }

    QStringList lines;
    if (!evaluationState.summaryText.isEmpty()) {
        lines.append(evaluationState.summaryText);
    }

    const QString digitalTwinSummary = digitalTwinSummaryOrEmpty(evaluationState.errorMetrics);
    if (!digitalTwinSummary.isEmpty()) {
        lines.append(digitalTwinSummary);
    }

    if (!evaluationState.navigationProcessSummary.isEmpty()) {
        lines.append(QStringLiteral("导航过程：%1").arg(evaluationState.navigationProcessSummary));
    }
    if (!evaluationState.perBoneQualitySummary.isEmpty()) {
        lines.append(QStringLiteral("分骨质量：%1").arg(evaluationState.perBoneQualitySummary.join(QStringLiteral("；"))));
    }
    if (!evaluationState.errorMetrics.isEmpty()) {
        QStringList metricItems;
        for (auto it = evaluationState.errorMetrics.cbegin(); it != evaluationState.errorMetrics.cend(); ++it) {
            metricItems.append(QStringLiteral("%1=%2").arg(it.key(), it.value().toString()));
        }
        lines.append(QStringLiteral("指标：%1").arg(metricItems.join(QStringLiteral("，"))));
    }
    lines.append(QStringLiteral("报告导出：%1").arg(evaluationState.reportReady ? QStringLiteral("已就绪") : QStringLiteral("未就绪")));

    m_bindings.evaluationSummaryLabel->setText(lines.join(QStringLiteral("\n")));
    applyTone(m_bindings.evaluationSummaryLabel,
              evaluationState.reportReady ? QStringLiteral("ok") : QStringLiteral("warning"));
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
