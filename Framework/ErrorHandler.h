#ifndef ERRORHANDLER_H
#define ERRORHANDLER_H

#include "FrameworkExport.h"

#include <QObject>
#include <QString>
#include <QVariantMap>

/**
 * @brief Centralised error handling utility for framework-level startup and service errors.
 *
 * Responsibility: provide a single place to route startup errors so implementation of
 * dialogs, logging, and future diagnostics can be managed consistently.
 */
class FRAMEWORK_EXPORT ErrorHandler : public QObject
{
    Q_OBJECT

public:
    enum class ErrorLevel {
        Info,
        Warning,
        Error,
        Critical
    };
    Q_ENUM(ErrorLevel)

    explicit ErrorHandler(QObject* parent = nullptr);

    static void handleStartupError(ErrorLevel level,
                                   const QString& message,
                                   const QVariantMap& context = {});
    static void showErrorDialog(ErrorLevel level,
                                const QString& message,
                                const QVariantMap& context = {});
    static void logError(ErrorLevel level,
                         const QString& message,
                         const QVariantMap& context = {});
};

#endif // ERRORHANDLER_H
