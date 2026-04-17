#include "ConsoleLogBridge.h"

#include <QDateTime>
#include <QFileInfo>

#include <cstdlib>
#include <cstdio>

#ifdef _WIN32
#  include <windows.h>
#endif

namespace
{

QString buildLevelTag(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return QStringLiteral("[DEBUG]");
    case QtInfoMsg:
        return QStringLiteral("[INFO]");
    case QtWarningMsg:
        return QStringLiteral("[WARNING]");
    case QtCriticalMsg:
        return QStringLiteral("[CRITICAL]");
    case QtFatalMsg:
        return QStringLiteral("[FATAL]");
    }

    return QStringLiteral("[UNKNOWN]");
}

QString buildContextSuffix(const QMessageLogContext& context)
{
    if (!context.file && context.line <= 0) {
        return QString();
    }

    QString locationText;
    if (context.file) {
        locationText = QFileInfo(QString::fromUtf8(context.file)).fileName();
    }
    if (context.line > 0) {
        locationText += QStringLiteral(":%1").arg(context.line);
    }

    return locationText.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(locationText);
}

#ifdef _WIN32
bool isConsoleHandle(HANDLE handle)
{
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD consoleMode = 0;
    return GetConsoleMode(handle, &consoleMode) != 0;
}

bool isDiskHandle(HANDLE handle)
{
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    return GetFileType(handle) == FILE_TYPE_DISK;
}

HANDLE openConsoleOutputHandle()
{
    return CreateFileW(
        L"CONOUT$",
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);
}

void writeUtf16LineToConsole(HANDLE handle, const QString& formattedLine)
{
    const std::wstring wideLine = (formattedLine + QStringLiteral("\r\n")).toStdWString();
    DWORD written = 0;
    WriteConsoleW(handle, wideLine.c_str(), static_cast<DWORD>(wideLine.size()), &written, nullptr);
}

void writeUtf8Bytes(HANDLE handle, const QByteArray& bytes)
{
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD totalWritten = 0;
    while (totalWritten < static_cast<DWORD>(bytes.size())) {
        DWORD written = 0;
        const DWORD remaining = static_cast<DWORD>(bytes.size()) - totalWritten;
        const char* data = bytes.constData() + totalWritten;
        if (!WriteFile(handle, data, remaining, &written, nullptr) || written == 0) {
            break;
        }
        totalWritten += written;
    }
}

unsigned int resolveConsoleCodePage()
{
    const UINT consoleCodePage = GetConsoleOutputCP();
    return consoleCodePage != 0 ? consoleCodePage : static_cast<unsigned int>(GetACP());
}
#endif

void writeFormattedLine(const QString& formattedLine)
{
#ifdef _WIN32
    HANDLE stdHandle = GetStdHandle(STD_ERROR_HANDLE);
    const bool stdHandleIsConsole = isConsoleHandle(stdHandle);
    const bool stdHandleIsDisk = isDiskHandle(stdHandle);

    HANDLE consoleHandle = INVALID_HANDLE_VALUE;
    if (!stdHandleIsConsole && !stdHandleIsDisk) {
        consoleHandle = openConsoleOutputHandle();
    }

    const bool attachedConsoleAvailable = isConsoleHandle(consoleHandle);
    const ConsoleLogBridge::OutputMode outputMode = ConsoleLogBridge::resolveOutputMode(
        stdHandleIsConsole,
        stdHandleIsDisk,
        attachedConsoleAvailable);

    if (outputMode == ConsoleLogBridge::OutputMode::Console) {
        HANDLE targetHandle = stdHandleIsConsole ? stdHandle : consoleHandle;
        if (isConsoleHandle(targetHandle)) {
            writeUtf16LineToConsole(targetHandle, formattedLine);
        } else {
            writeUtf8Bytes(
                stdHandle,
                ConsoleLogBridge::buildConsolePipeOutputBytes(formattedLine, resolveConsoleCodePage()));
        }
    } else {
        writeUtf8Bytes(stdHandle, ConsoleLogBridge::buildRedirectedOutputBytes(formattedLine));
    }

    if (consoleHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(consoleHandle);
    }
#else
    const QByteArray redirectedBytes = ConsoleLogBridge::buildRedirectedOutputBytes(formattedLine);
    std::fwrite(redirectedBytes.constData(), 1, static_cast<size_t>(redirectedBytes.size()), stderr);
    std::fflush(stderr);
#endif
}

void qtMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    const QString formattedLine = ConsoleLogBridge::formatLogLine(type, context, message);
    writeFormattedLine(formattedLine);

    if (type == QtFatalMsg) {
        std::abort();
    }
}

} // namespace

namespace ConsoleLogBridge
{

OutputMode resolveOutputMode(bool stdHandleIsConsole, bool stdHandleIsDisk, bool attachedConsoleAvailable)
{
    if (stdHandleIsConsole) {
        return OutputMode::Console;
    }
    if (stdHandleIsDisk) {
        return OutputMode::Redirected;
    }
    return attachedConsoleAvailable ? OutputMode::Console : OutputMode::Redirected;
}

QString formatLogLine(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    return QStringLiteral("[%1] %2 %3%4")
        .arg(timestamp, buildLevelTag(type), message, buildContextSuffix(context));
}

QByteArray buildRedirectedOutputBytes(const QString& formattedLine)
{
    return (formattedLine + QStringLiteral("\n")).toUtf8();
}

QByteArray buildConsolePipeOutputBytes(const QString& formattedLine, unsigned int codePage)
{
#ifdef _WIN32
    const QString lineWithNewline = formattedLine + QStringLiteral("\n");
    const std::wstring wideLine = lineWithNewline.toStdWString();
    const int requiredSize = WideCharToMultiByte(
        codePage,
        0,
        wideLine.c_str(),
        static_cast<int>(wideLine.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (requiredSize <= 0) {
        return lineWithNewline.toUtf8();
    }

    QByteArray encoded(requiredSize, Qt::Uninitialized);
    const int written = WideCharToMultiByte(
        codePage,
        0,
        wideLine.c_str(),
        static_cast<int>(wideLine.size()),
        encoded.data(),
        requiredSize,
        nullptr,
        nullptr);
    if (written <= 0) {
        return lineWithNewline.toUtf8();
    }

    encoded.truncate(written);
    return encoded;
#else
    Q_UNUSED(codePage);
    return buildRedirectedOutputBytes(formattedLine);
#endif
}

void installMessageHandler()
{
    static bool installed = false;
    if (installed) {
        return;
    }

    qInstallMessageHandler(qtMessageHandler);
    installed = true;
}

} // namespace ConsoleLogBridge
