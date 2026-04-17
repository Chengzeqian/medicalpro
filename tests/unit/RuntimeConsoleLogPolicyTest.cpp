#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>

class RuntimeConsoleLogPolicyTest : public QObject
{
    Q_OBJECT

private slots:
    void runtimeLogFiles_useAsciiOnlyMessages();
};

namespace
{

QString readSourceFile(const QString& relativePath)
{
    QFile file(QFINDTESTDATA(relativePath));

    if (!file.exists()) {
        const QStringList candidatePaths = {
            QDir::current().absoluteFilePath(relativePath),
            QDir::current().absoluteFilePath(QStringLiteral("../../../%1").arg(relativePath)),
            QDir::current().absoluteFilePath(QStringLiteral("../../../../%1").arg(relativePath)),
        };

        for (const QString& candidatePath : candidatePaths) {
            if (QFileInfo::exists(candidatePath)) {
                file.setFileName(candidatePath);
                break;
            }
        }
    }

    if (!file.exists() && QCoreApplication::instance()) {
        const QString projectRootCandidate =
            QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(
                QStringLiteral("../../../../%1").arg(relativePath));
        if (QFileInfo::exists(projectRootCandidate)) {
            file.setFileName(projectRootCandidate);
        }
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }

    return QString::fromUtf8(file.readAll());
}

bool containsLogCall(const QString& line)
{
    return line.contains(QStringLiteral("qDebug()"))
        || line.contains(QStringLiteral("qWarning()"))
        || line.contains(QStringLiteral("qCritical()"))
        || line.contains(QStringLiteral("qInfo()"))
        || line.contains(QStringLiteral("logMessage("))
        || line.contains(QStringLiteral("logDatabaseStatus("))
        || line.contains(QStringLiteral("logSecurityEvent("))
        || line.contains(QStringLiteral("logOperation("))
        || line.contains(QStringLiteral("LOG_DEBUG"))
        || line.contains(QStringLiteral("LOG_INFO"))
        || line.contains(QStringLiteral("LOG_WARNING"))
        || line.contains(QStringLiteral("LOG_ERROR"))
        || line.contains(QStringLiteral("LOG_CRITICAL"));
}

QStringList collectNonAsciiLogLines(const QString& source)
{
    QStringList linesWithNonAscii;
    const QStringList lines = source.split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        if (!containsLogCall(line)) {
            continue;
        }

        bool hasNonAscii = false;
        for (const QChar ch : line) {
            if (ch.unicode() > 127) {
                hasNonAscii = true;
                break;
            }
        }

        if (hasNonAscii) {
            linesWithNonAscii.append(line.trimmed());
        }
    }
    return linesWithNonAscii;
}

void verifyRuntimeLogFile(const QString& relativePath, const QStringList& expectedMarkers)
{
    const QString source = readSourceFile(relativePath);
    QVERIFY2(!source.isEmpty(), qPrintable(QStringLiteral("Failed to load %1").arg(relativePath)));

    const QStringList nonAsciiLogLines = collectNonAsciiLogLines(source);
    QVERIFY2(
        nonAsciiLogLines.isEmpty(),
        qPrintable(QStringLiteral("Non-ASCII runtime log lines remain in %1:\n%2")
                       .arg(relativePath, nonAsciiLogLines.join(QStringLiteral("\n")))));

    for (const QString& marker : expectedMarkers) {
        QVERIFY2(
            source.contains(marker),
            qPrintable(QStringLiteral("Expected marker not found in %1: %2").arg(relativePath, marker)));
    }
}

} // namespace

void RuntimeConsoleLogPolicyTest::runtimeLogFiles_useAsciiOnlyMessages()
{
    verifyRuntimeLogFile(
        QStringLiteral("Plugins/UserManagement/UserManagementActivator.cpp"),
        {QStringLiteral("[UserManagementActivator] Starting UserManagement plugin")});

    verifyRuntimeLogFile(
        QStringLiteral("Plugins/UserManagement/UserManagementServiceImpl.cpp"),
        {QStringLiteral("[UserManagementServiceImpl] UserManagement service instance created")});

    verifyRuntimeLogFile(
        QStringLiteral("Plugins/Registration2D3D/Registration2D3DServiceImpl.cpp"),
        {QStringLiteral("[Registration2D3D] Service instance created")});

    verifyRuntimeLogFile(
        QStringLiteral("Plugins/Registration2D3D/Registration2D3DWidget.cpp"),
        {QStringLiteral("[Registration2D3DWidget] Enhanced plugin widget created")});

    verifyRuntimeLogFile(
        QStringLiteral("Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp"),
        {QStringLiteral("[OpticalTrackingServiceImpl] Creating optical tracking service implementation (full CTK architecture)")});

    verifyRuntimeLogFile(
        QStringLiteral("UI/Widgets/Instrument3DPreviewWidget.cpp"),
        {QStringLiteral("[Instrument3DPreviewWidget] ========== Constructor begin ==========")});

    verifyRuntimeLogFile(
        QStringLiteral("Plugins/OpticalRegistration/widgets/OpticalRegistrationWidget.cpp"),
        {QStringLiteral("[OpticalRegistrationWidget] Widget created")});

    verifyRuntimeLogFile(
        QStringLiteral("Plugins/OpticalRegistration/internal/OpticalRegistrationVTKWidget.cpp"),
        {QStringLiteral("[OpticalRegistrationVTKWidget] VTK Widget created")});
}

QTEST_APPLESS_MAIN(RuntimeConsoleLogPolicyTest)

#include "RuntimeConsoleLogPolicyTest.moc"
