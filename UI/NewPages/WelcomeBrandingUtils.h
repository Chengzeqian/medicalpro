#ifndef WELCOMEBRANDINGUTILS_H
#define WELCOMEBRANDINGUTILS_H

#include <QString>
#include <QStringList>

namespace WelcomeBrandingUtils
{

QStringList buildLogoCandidatePaths(const QString& applicationDirPath);
QString resolveLogoPath(const QString& applicationDirPath);
QString buildFallbackBrandText();

} // namespace WelcomeBrandingUtils

#endif // WELCOMEBRANDINGUTILS_H
