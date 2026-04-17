#include "ThreePagePresentationUtils.h"

namespace ThreePagePresentationUtils
{

QString buildFrameworkSummary(bool frameworkReady, int pluginCount)
{
    return frameworkReady
        ? QStringLiteral("插件框架已联通，当前识别到 %1 个插件。").arg(pluginCount)
        : QStringLiteral("插件框架尚未就绪，请先检查启动日志。");
}

QString buildServiceGroupSummary(int readyServices, int totalServices)
{
    return QStringLiteral("%1/%2 个关键服务已就绪。").arg(readyServices).arg(totalServices);
}

QString buildDirectorySummary(bool exists, bool readable, const QString& displayName)
{
    if (!exists) {
        return QStringLiteral("%1 目录缺失。").arg(displayName);
    }
    if (!readable) {
        return QStringLiteral("%1 目录存在但不可访问。").arg(displayName);
    }
    return QStringLiteral("%1 目录存在且可访问。").arg(displayName);
}

QString buildToneName(int readyCount, int totalCount)
{
    if (totalCount <= 0 || readyCount <= 0) {
        return QStringLiteral("danger");
    }
    if (readyCount == totalCount) {
        return QStringLiteral("ok");
    }
    return QStringLiteral("warning");
}

QString buildWelcomeRuntimeSummary(bool frameworkReady, int readyServices, int totalServices, bool dataDirectoryReadable)
{
    if (!frameworkReady || !dataDirectoryReadable || readyServices <= 0 || totalServices <= 0) {
        return QStringLiteral("插件框架或关键依赖仍在检查，当前暂不建议进入主流程。");
    }
    if (readyServices == totalServices) {
        return QStringLiteral("插件框架已联通，关键服务持续自检中，系统当前处于可进入状态。");
    }
    return QStringLiteral("插件框架已联通，部分关键服务仍在检查，系统当前可进入但建议先确认状态。");
}

QString buildWelcomeDecisionLabel(bool frameworkReady, int readyServices, int totalServices, bool dataDirectoryReadable)
{
    if (!frameworkReady || !dataDirectoryReadable || readyServices <= 0 || totalServices <= 0) {
        return QStringLiteral("暂不建议进入");
    }
    if (readyServices == totalServices) {
        return QStringLiteral("主流程可进入");
    }
    return QStringLiteral("可进入，建议检查");
}

QString buildWelcomeDecisionTone(bool frameworkReady, int readyServices, int totalServices, bool dataDirectoryReadable)
{
    if (!frameworkReady || !dataDirectoryReadable || readyServices <= 0 || totalServices <= 0) {
        return QStringLiteral("danger");
    }
    if (readyServices == totalServices) {
        return QStringLiteral("ok");
    }
    return QStringLiteral("warning");
}


QString buildModuleStatusSummary(bool ready)
{
    return ready ? QStringLiteral("主流程可进入") : QStringLiteral("部分依赖待确认");
}

QString buildModuleAccessHint(bool ready, const QStringList& missingServices)
{
    if (ready) {
        return QStringLiteral("关键服务已通过检查，可进入数据管理主链。");
    }
    if (missingServices.isEmpty()) {
        return QStringLiteral("当前仍在等待运行状态回传。");
    }
    return QStringLiteral("待确认服务：%1，建议先检查后再进入数据管理主链。")
        .arg(missingServices.join(QStringLiteral(" / ")));
}

QString buildDashboardDicomSummary(int studyCount)
{
    return QStringLiteral("当前病例包含 %1 组 DICOM 检查。").arg(studyCount);
}

QString buildDashboardNavigationHint(bool patientSelected, int studyCount)
{
    if (!patientSelected) {
        return QStringLiteral("请先在左侧选择病例，再进入导航流程。");
    }
    if (studyCount <= 0) {
        return QStringLiteral("已选病例暂无 DICOM 检查，可先核对资料后继续。");
    }
    return QStringLiteral("病例与影像已就绪，可进入导航流程。");
}

QString buildDashboardNavigationTone(bool patientSelected, int studyCount)
{
    if (!patientSelected) {
        return QStringLiteral("danger");
    }
    if (studyCount <= 0) {
        return QStringLiteral("warning");
    }
    return QStringLiteral("ok");
}

QString buildManagementOverviewValue(const QString& entityName, int count)
{
    if (entityName == QStringLiteral("医生数据")) {
        return QStringLiteral("%1 位医生").arg(count);
    }
    if (entityName == QStringLiteral("患者数据")) {
        return QStringLiteral("%1 位患者").arg(count);
    }
    return QStringLiteral("%1 台手术").arg(count);
}

QString buildManagementOverviewHint(const QString& entityName, int count)
{
    Q_UNUSED(count);

    if (entityName == QStringLiteral("医生数据")) {
        return QStringLiteral("当前可切换到医生视图继续查看、检索和维护术者资料。");
    }
    if (entityName == QStringLiteral("患者数据")) {
        return QStringLiteral("当前可切换到患者视图继续查看、检索和维护病例基础资料。");
    }
    return QStringLiteral("当前可切换到手术视图继续核对计划任务与流程状态。");
}

QString buildManagementEntryHint(bool readyToEnterDashboard)
{
    return readyToEnterDashboard
        ? QStringLiteral("基础数据已确认，可进入病例工作台继续病例与影像流程。")
        : QStringLiteral("建议先确认当前管理对象，再进入病例工作台。");
}

QString buildSystemSettingsPathSummary(bool dataReadable, bool dicomReadable)
{
    if (dataReadable && dicomReadable) {
        return QStringLiteral("data 与 DICOM 目录均可访问。");
    }
    if (dataReadable && !dicomReadable) {
        return QStringLiteral("data 目录可访问，但 DICOM 目录仍需检查。");
    }
    if (!dataReadable && dicomReadable) {
        return QStringLiteral("DICOM 目录可访问，但 data 目录仍需检查。");
    }
    return QStringLiteral("data 与 DICOM 目录当前均不可访问。");
}

QString buildSystemSettingsRecommendation(bool frameworkReady, int readyServices, int totalServices, bool dataReadable, bool dicomReadable)
{
    if (!frameworkReady || readyServices <= 0 || totalServices <= 0 || !dataReadable || !dicomReadable) {
        if (frameworkReady && readyServices > 0 && (dataReadable || dicomReadable)) {
            return QStringLiteral("建议先检查关键服务与路径状态，再继续主流程。");
        }
        return QStringLiteral("当前更适合先排查环境问题，不建议直接进入后续流程。");
    }
    if (readyServices == totalServices) {
        return QStringLiteral("当前环境可继续使用，如需微调参数可直接保存。");
    }
    return QStringLiteral("建议先检查关键服务与路径状态，再继续主流程。");
}

QString buildSystemSettingsRecommendationTone(bool frameworkReady, int readyServices, int totalServices, bool dataReadable, bool dicomReadable)
{
    if (!frameworkReady || readyServices <= 0 || totalServices <= 0 || (!dataReadable && !dicomReadable)) {
        return QStringLiteral("danger");
    }
    if (readyServices == totalServices && dataReadable && dicomReadable) {
        return QStringLiteral("ok");
    }
    return QStringLiteral("warning");
}

QString formatModuleTimestamp(const QDateTime& dateTime)
{
    return dateTime.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

} // namespace ThreePagePresentationUtils
