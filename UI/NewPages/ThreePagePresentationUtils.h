#ifndef THREEPAGEPRESENTATIONUTILS_H
#define THREEPAGEPRESENTATIONUTILS_H

#include <QDateTime>
#include <QString>
#include <QStringList>

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
QString buildModuleAccessHint(bool ready, const QStringList& missingServices);
QString buildDashboardDicomSummary(int studyCount);
QString buildDashboardNavigationHint(bool patientSelected, int studyCount);
QString buildDashboardNavigationTone(bool patientSelected, int studyCount);
QString buildManagementOverviewValue(const QString& entityName, int count);
QString buildManagementOverviewHint(const QString& entityName, int count);
QString buildManagementEntryHint(bool readyToEnterDashboard);
QString buildSystemSettingsPathSummary(bool dataReadable, bool dicomReadable);
QString buildSystemSettingsRecommendation(bool frameworkReady, int readyServices, int totalServices, bool dataReadable, bool dicomReadable);
QString buildSystemSettingsRecommendationTone(bool frameworkReady, int readyServices, int totalServices, bool dataReadable, bool dicomReadable);
QString formatModuleTimestamp(const QDateTime& dateTime);

} // namespace ThreePagePresentationUtils

#endif // THREEPAGEPRESENTATIONUTILS_H
