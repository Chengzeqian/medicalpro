#include "WelcomeBrandingUtils.h"

#include <QDir>
#include <QFile>

namespace WelcomeBrandingUtils
{

QStringList buildLogoCandidatePaths(const QString& applicationDirPath)
{
    const QString normalizedApplicationDir = QDir::cleanPath(applicationDirPath);
    return {
        normalizedApplicationDir + QStringLiteral("/data/branding/welcome_logo.png"),
        normalizedApplicationDir + QStringLiteral("/data/logo.png"),
        QStringLiteral(":/resoucce/logo.png")
    };
}

QString resolveLogoPath(const QString& applicationDirPath)
{
    const QStringList candidatePaths = buildLogoCandidatePaths(applicationDirPath);
    for (const QString& candidatePath : candidatePaths) {
        if (QFile::exists(candidatePath)) {
            return candidatePath;
        }
    }
    return QString();
}

QString buildFallbackBrandText()
{
    return QStringLiteral("MedicalPro");
}

} // namespace WelcomeBrandingUtils
