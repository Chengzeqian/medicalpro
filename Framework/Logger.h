#ifndef FRAMEWORK_LOGGER_H
#define FRAMEWORK_LOGGER_H

#include <QDebug>
#include <QString>

#define LOG_DEBUG(module, msg) qDebug() << "[" << module << "]" << msg
#define LOG_INFO(module, msg)  qInfo()  << "[" << module << "]" << msg
#define LOG_WARNING(module, msg) qWarning() << "[" << module << "]" << msg
#define LOG_ERROR(module, msg) qCritical() << "[" << module << "]" << msg
#define LOG_CRITICAL(module, msg) qCritical() << "[" << module << "]" << msg
#define LOG_FATAL(module, msg) { qCritical() << "[" << module << "]" << msg; abort(); }

template<typename... Args>
inline QString formatLogMessage(QString fmt, Args&&... args)
{
    using expander = int[];
    (void)expander{0, (fmt = fmt.arg(std::forward<Args>(args)), 0)...};
    return fmt;
}

inline QString formatLogMessage(QString fmt)
{
    return fmt;
}

#define LOG_DEBUG_F(module, fmt, ...) LOG_DEBUG(module, formatLogMessage(QString(fmt), ##__VA_ARGS__))
#define LOG_INFO_F(module, fmt, ...)  LOG_INFO(module, formatLogMessage(QString(fmt), ##__VA_ARGS__))
#define LOG_WARNING_F(module, fmt, ...) LOG_WARNING(module, formatLogMessage(QString(fmt), ##__VA_ARGS__))
#define LOG_ERROR_F(module, fmt, ...) LOG_ERROR(module, formatLogMessage(QString(fmt), ##__VA_ARGS__))
#define LOG_CRITICAL_F(module, fmt, ...) LOG_CRITICAL(module, formatLogMessage(QString(fmt), ##__VA_ARGS__))
#define LOG_FATAL_F(module, fmt, ...) LOG_FATAL(module, formatLogMessage(QString(fmt), ##__VA_ARGS__))

inline void initializeLogger() {}

#endif // FRAMEWORK_LOGGER_H
