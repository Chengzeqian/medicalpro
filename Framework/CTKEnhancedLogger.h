#ifndef CTKENHANCEDLOGGER_H
#define CTKENHANCEDLOGGER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QMutex>
#include <QTextStream>
#include <QFile>

#ifdef CTK_PLUGIN_FRAMEWORK
// 如果CTK有日志组件，可以使用
// #include <ctkLogger.h>  
#endif

/**
 * @brief CTK增强日志管理器
 * 
 * 为医疗软件提供专业级别的日志功能：
 * - 多级别日志 (DEBUG, INFO, WARN, ERROR, FATAL)
 * - 文件自动轮转
 * - 时间戳和模块标识
 * - 线程安全
 * - 医疗审计跟踪兼容
 */
class CTKEnhancedLogger : public QObject
{
    Q_OBJECT
    
public:
    enum LogLevel {
        DEBUG = 0,
        INFO = 1,
        WARN = 2,
        ERROR = 3,
        FATAL = 4
    };
    Q_ENUM(LogLevel)
    
    enum LogCategory {
        GENERAL = 0,
        PLUGIN_SYSTEM,
        OPTICAL_TRACKING, 
        PATIENT_DATA,
        DICOM_PROCESSING,
        CALIBRATION,
        SYSTEM_ERROR,
        MEDICAL_AUDIT     // 医疗审计日志
    };
    Q_ENUM(LogCategory)
    
    static CTKEnhancedLogger* instance();
    
    // 基础日志方法
    void logMessage(LogLevel level, LogCategory category, 
                   const QString& message, const QString& module = QString());
    
    // 便利方法
    void debug(const QString& message, const QString& module = QString());
    void info(const QString& message, const QString& module = QString());
    void warn(const QString& message, const QString& module = QString());
    void error(const QString& message, const QString& module = QString());
    void fatal(const QString& message, const QString& module = QString());
    
    // 医疗审计专用日志
    void auditLog(const QString& action, const QString& patientId = QString(), 
                  const QString& userId = QString());
    
    // 插件系统专用日志
    void pluginLog(const QString& pluginName, const QString& message, LogLevel level = INFO);
    
    // 光学追踪专用日志
    void trackingLog(const QString& event, const QVariantMap& data = QVariantMap());
    
    // 配置方法
    void setLogLevel(LogLevel level);
    void setLogFile(const QString& filePath);
    void setMaxFileSize(qint64 maxSize);  // 字节
    void setMaxFiles(int maxFiles);
    void enableConsoleOutput(bool enable);
    void enableFileRotation(bool enable);
    
    // 查询方法
    LogLevel currentLogLevel() const { return m_currentLevel; }
    QString currentLogFile() const { return m_logFile; }
    bool isConsoleOutputEnabled() const { return m_consoleOutput; }
    
signals:
    void logMessageEmitted(LogLevel level, LogCategory category, 
                          const QString& message, const QString& timestamp);
    void errorOccurred(const QString& errorMessage);

private:
    explicit CTKEnhancedLogger(QObject *parent = nullptr);
    ~CTKEnhancedLogger();
    
    // 内部方法
    void writeToFile(const QString& formattedMessage);
    void writeToConsole(const QString& formattedMessage, LogLevel level);
    QString formatMessage(LogLevel level, LogCategory category, 
                         const QString& message, const QString& module);
    QString levelToString(LogLevel level);
    QString categoryToString(LogCategory category);
    void rotateLogFile();
    bool shouldLog(LogLevel level, LogCategory category);
    
    // 成员变量
    static CTKEnhancedLogger* s_instance;
    static QMutex s_mutex;
    
    LogLevel m_currentLevel;
    QString m_logFile;
    QFile* m_logFileHandle;
    QTextStream* m_logStream;
    
    // 配置参数
    qint64 m_maxFileSize;    // 最大文件大小（字节）
    int m_maxFiles;          // 最大文件数量
    bool m_consoleOutput;    // 是否输出到控制台
    bool m_fileRotation;     // 是否启用文件轮转
    
    // 线程安全
    QMutex m_writeMutex;
    
    // 禁用复制和赋值
    Q_DISABLE_COPY(CTKEnhancedLogger)
};

// 全局便利宏定义
#define CTK_DEBUG(msg) CTKEnhancedLogger::instance()->debug(msg, Q_FUNC_INFO)
#define CTK_INFO(msg) CTKEnhancedLogger::instance()->info(msg, Q_FUNC_INFO)  
#define CTK_WARN(msg) CTKEnhancedLogger::instance()->warn(msg, Q_FUNC_INFO)
#define CTK_ERROR(msg) CTKEnhancedLogger::instance()->error(msg, Q_FUNC_INFO)
#define CTK_FATAL(msg) CTKEnhancedLogger::instance()->fatal(msg, Q_FUNC_INFO)

// 专用日志宏
#define CTK_AUDIT(action, patientId, userId) \
    CTKEnhancedLogger::instance()->auditLog(action, patientId, userId)

#define CTK_PLUGIN_LOG(plugin, msg, level) \
    CTKEnhancedLogger::instance()->pluginLog(plugin, msg, level)

#define CTK_TRACKING_LOG(event, data) \
    CTKEnhancedLogger::instance()->trackingLog(event, data)

#endif // CTKENHANCEDLOGGER_H
