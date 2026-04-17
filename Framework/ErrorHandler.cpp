#include "ErrorHandler.h"

#include <QCoreApplication>
#include <QDebug>
#include <QLoggingCategory>
#include <QMessageBox>

namespace
{
    QLoggingCategory errorHandlerLog("framework.errorhandler");

    QString levelToString(ErrorHandler::ErrorLevel level)
    {
        switch (level) {
        case ErrorHandler::ErrorLevel::Info:
            return QStringLiteral("Info");
        case ErrorHandler::ErrorLevel::Warning:
            return QStringLiteral("Warning");
        case ErrorHandler::ErrorLevel::Error:
            return QStringLiteral("Error");
        case ErrorHandler::ErrorLevel::Critical:
            return QStringLiteral("Critical");
        }
        return QStringLiteral("Unknown");
    }
}

ErrorHandler::ErrorHandler(QObject* parent)
    : QObject(parent)
{
}

void ErrorHandler::handleStartupError(ErrorLevel level,
                                      const QString& message,
                                      const QVariantMap& context)
{
    logError(level, message, context);
    showErrorDialog(level, message, context);
}

void ErrorHandler::showErrorDialog(ErrorLevel level,
                                   const QString& message,
                                   const QVariantMap& context)
{
    Q_UNUSED(context);

    QMetaObject::invokeMethod(QCoreApplication::instance(), [level, message]() {
        QMessageBox::Icon icon = QMessageBox::Information;
        QString title = QObject::tr("系统提示");

        switch (level) {
        case ErrorLevel::Info:
            icon = QMessageBox::Information;
            title = QObject::tr("信息");
            break;
        case ErrorLevel::Warning:
            icon = QMessageBox::Warning;
            title = QObject::tr("警告");
            break;
        case ErrorLevel::Error:
            icon = QMessageBox::Critical;
            title = QObject::tr("错误");
            break;
        case ErrorLevel::Critical:
            icon = QMessageBox::Critical;
            title = QObject::tr("严重错误");
            break;
        }

        QMessageBox box(icon, title, message, QMessageBox::Ok);
        box.setModal(true);
        box.exec();
    }, Qt::QueuedConnection);
}

void ErrorHandler::logError(ErrorLevel level,
                            const QString& message,
                            const QVariantMap& context)
{
    QStringList contextPairs;
    for (auto it = context.cbegin(); it != context.cend(); ++it) {
        contextPairs << QStringLiteral("%1=%2").arg(it.key(), it.value().toString());
    }

    const QString formatted = contextPairs.isEmpty()
        ? message
        : QStringLiteral("%1 | %2").arg(message, contextPairs.join(", "));

    switch (level) {
    case ErrorLevel::Info:
        qCInfo(errorHandlerLog) << formatted;
        break;
    case ErrorLevel::Warning:
        qCWarning(errorHandlerLog) << formatted;
        break;
    case ErrorLevel::Error:
        qCCritical(errorHandlerLog) << formatted;
        break;
    case ErrorLevel::Critical:
        qFatal("%s", formatted.toUtf8().constData());
        break;
    }
}
