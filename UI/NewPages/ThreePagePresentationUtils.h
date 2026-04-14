#ifndef THREEPAGEPRESENTATIONUTILS_H
#define THREEPAGEPRESENTATIONUTILS_H

#include <QDateTime>
#include <QString>

namespace ThreePagePresentationUtils
{

QString buildFrameworkSummary(bool frameworkReady, int pluginCount);
QString buildServiceGroupSummary(int readyServices, int totalServices);
QString buildDirectorySummary(bool exists, bool readable, const QString& displayName);
QString buildToneName(int readyCount, int totalCount);
QString buildWelcomeRuntimeSummary(bool frameworkReady, int readyServices, int totalServices, bool dataDirectoryReadable);
QString buildWelcomeDecisionLabel(bool frameworkReady, int readyServices, int totalServices, bool dataDirectoryReadable);
QString buildWelcomeDecisionTone(bool frameworkReady, int readyServices, int totalServices, bool dataDirectoryReadable);
QString buildModuleStatusSummary(bool ready);
QString buildDashboardDicomSummary(int studyCount);
QString formatModuleTimestamp(const QDateTime& dateTime);

} // namespace ThreePagePresentationUtils

#endif // THREEPAGEPRESENTATIONUTILS_H
