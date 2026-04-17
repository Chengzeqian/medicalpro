#ifndef CONSOLELOGBRIDGE_H
#define CONSOLELOGBRIDGE_H

#include "FrameworkExport.h"

#include <QByteArray>
#include <QString>
#include <QtGlobal>

namespace ConsoleLogBridge
{

enum class OutputMode
{
    Console,
    Redirected
};

FRAMEWORK_EXPORT OutputMode resolveOutputMode(
    bool stdHandleIsConsole,
    bool stdHandleIsDisk,
    bool attachedConsoleAvailable);
FRAMEWORK_EXPORT QString formatLogLine(
    QtMsgType type,
    const QMessageLogContext& context,
    const QString& message);
FRAMEWORK_EXPORT QByteArray buildRedirectedOutputBytes(const QString& formattedLine);
FRAMEWORK_EXPORT QByteArray buildConsolePipeOutputBytes(const QString& formattedLine, unsigned int codePage);
FRAMEWORK_EXPORT void installMessageHandler();

} // namespace ConsoleLogBridge

#endif // CONSOLELOGBRIDGE_H
