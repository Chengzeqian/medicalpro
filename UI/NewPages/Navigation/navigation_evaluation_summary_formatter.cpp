#include "UI/NewPages/Navigation/navigation_evaluation_summary_formatter.h"

namespace
{
QString joinOrDash(const QStringList& values)
{
    return values.isEmpty() ? QStringLiteral("-") : values.join(QStringLiteral("；"));
}

QString valueOrDash(const QVariantMap& values, const QString& key)
{
    const QString text = values.value(key).toString();
    return text.isEmpty() ? QStringLiteral("-") : text;
}
}

NavigationEvaluationSummary buildNavigationEvaluationSummary(const AnkleEvaluationSnapshot& snapshot)
{
    NavigationEvaluationSummary summary;
    summary.hasData = snapshot.hasRegistration || snapshot.hasNavigationRun || snapshot.hasEvaluationReport;
    summary.headerText = summary.hasData
        ? QStringLiteral("病例：%1").arg(snapshot.caseId)
        : QStringLiteral("当前病例暂无评估结果");

    if (!summary.hasData) {
        summary.registrationText = QStringLiteral("尚未生成注册评估记录");
        summary.constraintText = QStringLiteral("尚未生成目标区/约束区证据");
        summary.trackingText = QStringLiteral("尚未生成跟踪质量证据");
        summary.gateText = QStringLiteral("尚未生成导航准入证据");
        return summary;
    }

    summary.registrationText = QStringLiteral(
        "配准模式：%1\nFRE：%2 mm\n目标区 TRE：%3 mm\n覆盖评分：%4")
        .arg(snapshot.registrationMode.isEmpty() ? QStringLiteral("-") : snapshot.registrationMode)
        .arg(snapshot.fre, 0, 'f', 2)
        .arg(snapshot.targetTre, 0, 'f', 2)
        .arg(snapshot.coverageScore, 0, 'f', 2);

    summary.constraintText = QStringLiteral(
        "目标区半径：%1 mm\n约束区数量：%2\n约束区键：%3\n约束骨：%4\n约束角色：%5")
        .arg(snapshot.registrationMetrics.value(QStringLiteral("target_region_radius_mm")).toDouble(), 0, 'f', 2)
        .arg(snapshot.registrationMetrics.value(QStringLiteral("constraint_region_count")).toInt())
        .arg(valueOrDash(snapshot.registrationMetrics, QStringLiteral("constraint_region_keys")))
        .arg(valueOrDash(snapshot.registrationMetrics, QStringLiteral("constraint_region_bones")))
        .arg(valueOrDash(snapshot.registrationMetrics, QStringLiteral("constraint_region_roles")));

    summary.trackingText = QStringLiteral(
        "跟踪模式：%1\n抖动：%2 mm\n可见帧比例：%3%\n跟踪可信度：%4")
        .arg(valueOrDash(snapshot.navigationMetrics, QStringLiteral("tracking_profile")))
        .arg(snapshot.navigationMetrics.value(QStringLiteral("tracking_jitter_mm")).toDouble(), 0, 'f', 2)
        .arg(snapshot.navigationMetrics.value(QStringLiteral("visible_frame_ratio")).toDouble() * 100.0, 0, 'f', 2)
        .arg(snapshot.navigationMetrics.value(QStringLiteral("tracking_confidence_score")).toDouble(), 0, 'f', 2);

    summary.gateText = QStringLiteral(
        "允许导航：%1\n准入评分：%2\n门控原因数：%3\n已标定：%4\n标定精度：%5 mm\n建议：%6")
        .arg(snapshot.allowNavigation ? QStringLiteral("是") : QStringLiteral("否"))
        .arg(snapshot.evaluationConfidenceScore, 0, 'f', 2)
        .arg(snapshot.evaluationMetrics.value(QStringLiteral("gate_reason_count")).toInt())
        .arg(snapshot.calibrated ? QStringLiteral("是") : QStringLiteral("否"))
        .arg(snapshot.calibrationAccuracyMm, 0, 'f', 2)
        .arg(joinOrDash(snapshot.gateReasons));

    return summary;
}
