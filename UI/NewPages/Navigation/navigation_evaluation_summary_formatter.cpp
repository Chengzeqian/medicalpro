#include "UI/NewPages/Navigation/navigation_evaluation_summary_formatter.h"

namespace
{
QString joinOrDash(const QStringList& values)
{
    return values.isEmpty() ? QStringLiteral("-") : values.join(QStringLiteral("\uff1b"));
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
        ? QStringLiteral("\u75c5\u4f8b\uff1a%1").arg(snapshot.caseId)
        : QStringLiteral("\u5f53\u524d\u75c5\u4f8b\u6682\u65e0\u8bc4\u4f30\u7ed3\u679c");

    if (!summary.hasData) {
        summary.registrationText = QStringLiteral("\u5c1a\u672a\u751f\u6210\u914d\u51c6\u8bc4\u4f30\u8bb0\u5f55");
        summary.constraintText = QStringLiteral("\u5c1a\u672a\u751f\u6210\u76ee\u6807\u533a/\u7ea6\u675f\u533a\u8bc1\u636e");
        summary.trackingText = QStringLiteral("\u5c1a\u672a\u751f\u6210\u8ddf\u8e2a\u8d28\u91cf\u8bc1\u636e");
        summary.gateText = QStringLiteral("\u5c1a\u672a\u751f\u6210\u5bfc\u822a\u51c6\u5165\u8bc1\u636e");
        return summary;
    }

    summary.registrationText = QStringLiteral(
        "\u914d\u51c6\u6a21\u5f0f\uff1a%1\nFRE\uff1a%2 mm\n\u76ee\u6807\u533a TRE\uff1a%3 mm\n\u8986\u76d6\u8bc4\u5206\uff1a%4")
        .arg(snapshot.registrationMode.isEmpty() ? QStringLiteral("-") : snapshot.registrationMode)
        .arg(snapshot.fre, 0, 'f', 2)
        .arg(snapshot.targetTre, 0, 'f', 2)
        .arg(snapshot.coverageScore, 0, 'f', 2);

    summary.constraintText = QStringLiteral(
        "\u76ee\u6807\u533a\u534a\u5f84\uff1a%1 mm\n\u7ea6\u675f\u533a\u6570\u91cf\uff1a%2\n\u7ea6\u675f\u533a\u952e\uff1a%3\n\u7ea6\u675f\u9aa8\uff1a%4\n\u7ea6\u675f\u89d2\u8272\uff1a%5")
        .arg(snapshot.registrationMetrics.value(QStringLiteral("target_region_radius_mm")).toDouble(), 0, 'f', 2)
        .arg(snapshot.registrationMetrics.value(QStringLiteral("constraint_region_count")).toInt())
        .arg(valueOrDash(snapshot.registrationMetrics, QStringLiteral("constraint_region_keys")))
        .arg(valueOrDash(snapshot.registrationMetrics, QStringLiteral("constraint_region_bones")))
        .arg(valueOrDash(snapshot.registrationMetrics, QStringLiteral("constraint_region_roles")));

    summary.trackingText = QStringLiteral(
        "\u8ddf\u8e2a\u6a21\u5f0f\uff1a%1\n\u6296\u52a8\uff1a%2 mm\n\u53ef\u89c1\u5e27\u6bd4\u4f8b\uff1a%3%\n\u8ddf\u8e2a\u53ef\u4fe1\u5ea6\uff1a%4")
        .arg(valueOrDash(snapshot.navigationMetrics, QStringLiteral("tracking_profile")))
        .arg(snapshot.navigationMetrics.value(QStringLiteral("tracking_jitter_mm")).toDouble(), 0, 'f', 2)
        .arg(snapshot.navigationMetrics.value(QStringLiteral("visible_frame_ratio")).toDouble() * 100.0, 0, 'f', 2)
        .arg(snapshot.navigationMetrics.value(QStringLiteral("tracking_confidence_score")).toDouble(), 0, 'f', 2);

    summary.gateText = QStringLiteral(
        "\u5141\u8bb8\u5bfc\u822a\uff1a%1\n\u51c6\u5165\u8bc4\u5206\uff1a%2\n\u95e8\u63a7\u539f\u56e0\u6570\uff1a%3\n\u5df2\u6807\u5b9a\uff1a%4\n\u6807\u5b9a\u7cbe\u5ea6\uff1a%5 mm\n\u5efa\u8bae\uff1a%6")
        .arg(snapshot.allowNavigation ? QStringLiteral("\u662f") : QStringLiteral("\u5426"))
        .arg(snapshot.evaluationConfidenceScore, 0, 'f', 2)
        .arg(snapshot.evaluationMetrics.value(QStringLiteral("gate_reason_count")).toInt())
        .arg(snapshot.calibrated ? QStringLiteral("\u662f") : QStringLiteral("\u5426"))
        .arg(snapshot.calibrationAccuracyMm, 0, 'f', 2)
        .arg(joinOrDash(snapshot.gateReasons));

    if (snapshot.evaluationMetrics.contains(QStringLiteral("twin_confidence_score"))) {
        summary.gateText.append(QStringLiteral(
            "\nTwin可信度：%1\n局部风险：%2\n目标距离：%3 mm\n主风险：%4\n建议重配准：%5")
                .arg(snapshot.evaluationMetrics.value(QStringLiteral("twin_confidence_score")).toDouble(), 0, 'f', 2)
                .arg(snapshot.evaluationMetrics.value(QStringLiteral("local_risk_score")).toDouble(), 0, 'f', 2)
                .arg(snapshot.evaluationMetrics.value(QStringLiteral("target_region_distance_mm")).toDouble(), 0, 'f', 2)
                .arg(valueOrDash(snapshot.evaluationMetrics, QStringLiteral("dominant_risk_source")))
                .arg(snapshot.evaluationMetrics.value(QStringLiteral("re_register_recommended")).toBool()
                    ? QStringLiteral("是")
                    : QStringLiteral("否")));
    }

    return summary;
}

NavigationEvaluationSummary buildNavigationEvaluationSummary(const NavigationWorkspaceSnapshot& snapshot)
{
    NavigationEvaluationSummary summary;
    summary.hasData = snapshot.registrationState.success
        || snapshot.navigationState.hasRunRecord
        || snapshot.navigationState.hasEvaluationReport
        || snapshot.calibrationState.completed
        || snapshot.navigationState.running;
    summary.headerText = summary.hasData
        ? QStringLiteral("\u75c5\u4f8b\uff1a%1 | \u60a3\u8005\uff1a%2")
              .arg(snapshot.caseId, snapshot.caseContext.patientName.isEmpty()
                  ? QStringLiteral("-")
                  : snapshot.caseContext.patientName)
        : QStringLiteral("\u5f53\u524d\u75c5\u4f8b\u6682\u65e0\u8bc4\u4f30\u7ed3\u679c");

    if (!summary.hasData) {
        summary.registrationText = QStringLiteral("\u5c1a\u672a\u751f\u6210\u914d\u51c6\u8bc4\u4f30\u8bb0\u5f55");
        summary.constraintText = QStringLiteral("\u5c1a\u672a\u751f\u6210\u76ee\u6807\u533a/\u7ea6\u675f\u533a\u8bc1\u636e");
        summary.trackingText = QStringLiteral("\u5c1a\u672a\u751f\u6210\u8ddf\u8e2a/\u6807\u5b9a\u8bc1\u636e");
        summary.gateText = QStringLiteral("\u5c1a\u672a\u751f\u6210\u5bfc\u822a\u51c6\u5165\u8bc1\u636e");
        return summary;
    }

    summary.registrationText = QStringLiteral(
        "\u914d\u51c6\u6210\u529f\uff1a%1\n\u914d\u51c6\u70b9\u6570\uff1a%2\nFRE\uff1a%3 mm\n\u76ee\u6807\u533a TRE\uff1a%4 mm\n\u8986\u76d6\u8bc4\u5206\uff1a%5\n\u53d8\u6362\u6458\u8981\uff1a%6")
        .arg(snapshot.registrationState.success ? QStringLiteral("\u662f") : QStringLiteral("\u5426"))
        .arg(snapshot.registrationState.pointCount)
        .arg(snapshot.registrationState.fre, 0, 'f', 2)
        .arg(snapshot.registrationState.targetTre, 0, 'f', 2)
        .arg(snapshot.registrationState.coverageScore, 0, 'f', 2)
        .arg(snapshot.registrationState.transformMatrix.isEmpty()
            ? QStringLiteral("-")
            : snapshot.registrationState.transformMatrix);

    summary.constraintText = QStringLiteral(
        "\u76ee\u6807\u533a\u5c31\u7eea\uff1a%1\n\u53c2\u8003\u9aa8\uff1a%2\n\u7ea6\u675f\u533a\uff1a%3\n\u63a8\u8350\u70b9\u987a\u5e8f\uff1a%4")
        .arg(snapshot.planningState.targetRegionReady ? QStringLiteral("\u662f") : QStringLiteral("\u5426"))
        .arg(joinOrDash(snapshot.planningState.referenceBones))
        .arg(joinOrDash(snapshot.planningState.constraintRegions))
        .arg(joinOrDash(snapshot.planningState.recommendedPointOrder));

    summary.trackingText = QStringLiteral(
        "\u8ffd\u8e2a\u8fde\u63a5\uff1a%1\n\u5de5\u5177\u53ef\u89c1\uff1a%2\n\u6807\u5b9a\u5b8c\u6210\uff1a%3\n\u6807\u5b9a\u7cbe\u5ea6\uff1a%4 mm\n\u5bfc\u822a\u7f6e\u4fe1\u5ea6\uff1a%5\n\u6807\u5b9a\u72b6\u6001\uff1a%6")
        .arg(snapshot.navigationState.trackerConnected ? QStringLiteral("\u662f") : QStringLiteral("\u5426"))
        .arg(snapshot.navigationState.toolVisible ? QStringLiteral("\u662f") : QStringLiteral("\u5426"))
        .arg(snapshot.calibrationState.completed ? QStringLiteral("\u662f") : QStringLiteral("\u5426"))
        .arg(snapshot.calibrationState.accuracy, 0, 'f', 2)
        .arg(snapshot.navigationState.confidence, 0, 'f', 2)
        .arg(snapshot.calibrationState.statusText.isEmpty()
            ? QStringLiteral("-")
            : snapshot.calibrationState.statusText);

    summary.gateText = QStringLiteral(
        "\u8fd0\u884c\u4e2d\uff1a%1\n\u5141\u8bb8\u5bfc\u822a\uff1a%2\n\u7f6e\u4fe1\u5ea6\uff1a%3\n\u95e8\u7981\u539f\u56e0\uff1a%4\n\u8fd0\u884c\u6458\u8981\uff1a%5")
        .arg(snapshot.navigationState.running ? QStringLiteral("\u662f") : QStringLiteral("\u5426"))
        .arg(snapshot.navigationState.allowNavigation ? QStringLiteral("\u662f") : QStringLiteral("\u5426"))
        .arg(snapshot.navigationState.confidence, 0, 'f', 2)
        .arg(joinOrDash(snapshot.navigationState.blockReasons))
        .arg(snapshot.navigationState.summaryText.isEmpty()
            ? snapshot.stageGate.reasonText
            : snapshot.navigationState.summaryText);

    return summary;
}
