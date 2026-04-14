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

QString buildDashboardDicomSummary(int studyCount)
{
    return QStringLiteral("当前病例包含 %1 组 DICOM 检查。").arg(studyCount);
}

QString formatModuleTimestamp(const QDateTime& dateTime)
{
    return dateTime.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

} // namespace ThreePagePresentationUtils
