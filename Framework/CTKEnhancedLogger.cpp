#include "CTKEnhancedLogger.h"
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QCoreApplication>
#include <QThread>
#include <QFileInfo>
#include <QDateTime>

// 静态成员初始化
CTKEnhancedLogger* CTKEnhancedLogger::s_instance = nullptr;
QMutex CTKEnhancedLogger::s_mutex;

CTKEnhancedLogger* CTKEnhancedLogger::instance()
{
    if (!s_instance) {
        QMutexLocker locker(&s_mutex);
        if (!s_instance) {
            s_instance = new CTKEnhancedLogger();
        }
    }
    return s_instance;
}

CTKEnhancedLogger::CTKEnhancedLogger(QObject *parent)
    : QObject(parent)
    , m_currentLevel(INFO)
    , m_logFileHandle(nullptr)
    , m_logStream(nullptr)
    , m_maxFileSize(10 * 1024 * 1024)  // 10MB
    , m_maxFiles(5)
    , m_consoleOutput(true)
    , m_fileRotation(true)
{
    // 设置默认日志文件路径
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(logDir);
    
    QString defaultLogFile = logDir + "/medicalpro.log";
    setLogFile(defaultLogFile);
}

CTKEnhancedLogger::~CTKEnhancedLogger()
{
    if (m_logStream) {
        delete m_logStream;
    }
    if (m_logFileHandle) {
        m_logFileHandle->close();
        delete m_logFileHandle;
    }
}

void CTKEnhancedLogger::logMessage(LogLevel level, LogCategory category, 
                                  const QString& message, const QString& module)
{
    if (!shouldLog(level, category)) {
        return;
    }
    
    QMutexLocker locker(&m_writeMutex);
    
    QString formattedMessage = formatMessage(level, category, message, module);
    
    // 输出到控制台
    if (m_consoleOutput) {
        writeToConsole(formattedMessage, level);
    }
    
    // 输出到文件
    if (m_logFileHandle && m_logFileHandle->isOpen()) {
        writeToFile(formattedMessage);
    }
    
    // 发送信号
    emit logMessageEmitted(level, category, message, QDateTime::currentDateTime().toString());
}

void CTKEnhancedLogger::debug(const QString& message, const QString& module)
{
    logMessage(DEBUG, GENERAL, message, module);
}

void CTKEnhancedLogger::info(const QString& message, const QString& module)
{
    logMessage(INFO, GENERAL, message, module);
}

void CTKEnhancedLogger::warn(const QString& message, const QString& module)
{
    logMessage(WARN, GENERAL, message, module);
}

void CTKEnhancedLogger::error(const QString& message, const QString& module)
{
    logMessage(ERROR, GENERAL, message, module);
}

void CTKEnhancedLogger::fatal(const QString& message, const QString& module)
{
    logMessage(FATAL, GENERAL, message, module);
}

void CTKEnhancedLogger::auditLog(const QString& action, const QString& patientId, const QString& userId)
{
    QString auditMessage = QString("ACTION=%1").arg(action);
    if (!patientId.isEmpty()) {
        auditMessage += QString(" PATIENT_ID=%1").arg(patientId);
    }
    if (!userId.isEmpty()) {
        auditMessage += QString(" USER_ID=%1").arg(userId);
    }
    
    logMessage(INFO, MEDICAL_AUDIT, auditMessage, "AUDIT");
}

void CTKEnhancedLogger::pluginLog(const QString& pluginName, const QString& message, LogLevel level)
{
    QString pluginMessage = QString("[%1] %2").arg(pluginName, message);
    logMessage(level, PLUGIN_SYSTEM, pluginMessage, pluginName);
}

void CTKEnhancedLogger::trackingLog(const QString& event, const QVariantMap& data)
{
    QString trackingMessage = QString("EVENT=%1").arg(event);
    
    for (auto it = data.begin(); it != data.end(); ++it) {
        trackingMessage += QString(" %1=%2").arg(it.key(), it.value().toString());
    }
    
    logMessage(INFO, OPTICAL_TRACKING, trackingMessage, "TRACKING");
}

void CTKEnhancedLogger::setLogLevel(LogLevel level)
{
    m_currentLevel = level;
    info(QString("日志级别设置为: %1").arg(levelToString(level)), "Logger");
}

void CTKEnhancedLogger::setLogFile(const QString& filePath)
{
    QMutexLocker locker(&m_writeMutex);
    
    // 关闭现有文件
    if (m_logStream) {
        delete m_logStream;
        m_logStream = nullptr;
    }
    if (m_logFileHandle) {
        m_logFileHandle->close();
        delete m_logFileHandle;
        m_logFileHandle = nullptr;
    }
    
    m_logFile = filePath;
    
    // 确保目录存在
    QFileInfo fileInfo(filePath);
    QDir().mkpath(fileInfo.absolutePath());
    
    // 打开新文件
    m_logFileHandle = new QFile(filePath);
    if (m_logFileHandle->open(QIODevice::WriteOnly | QIODevice::Append)) {
        m_logStream = new QTextStream(m_logFileHandle);
        m_logStream->setCodec("UTF-8");
        
        // 写入启动标记
        QString startMessage = QString("=== MedicalPro 日志启动 [%1] ===")
                              .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
        *m_logStream << startMessage << Qt::endl;
        m_logStream->flush();
    } else {
        error(QString("无法打开日志文件: %1").arg(filePath), "Logger");
        delete m_logFileHandle;
        m_logFileHandle = nullptr;
    }
}

void CTKEnhancedLogger::setMaxFileSize(qint64 maxSize)
{
    m_maxFileSize = maxSize;
}

void CTKEnhancedLogger::setMaxFiles(int maxFiles)
{
    m_maxFiles = maxFiles;
}

void CTKEnhancedLogger::enableConsoleOutput(bool enable)
{
    m_consoleOutput = enable;
}

void CTKEnhancedLogger::enableFileRotation(bool enable)
{
    m_fileRotation = enable;
}

void CTKEnhancedLogger::writeToFile(const QString& formattedMessage)
{
    if (!m_logStream) return;
    
    *m_logStream << formattedMessage << Qt::endl;
    m_logStream->flush();
    
    // 检查文件大小，必要时轮转
    if (m_fileRotation && m_logFileHandle->size() > m_maxFileSize) {
        rotateLogFile();
    }
}

void CTKEnhancedLogger::writeToConsole(const QString& formattedMessage, LogLevel level)
{
    // 根据日志级别选择输出方式
    switch (level) {
    case DEBUG:
        qDebug().noquote() << formattedMessage;
        break;
    case INFO:
        qInfo().noquote() << formattedMessage;
        break;
    case WARN:
        qWarning().noquote() << formattedMessage;
        break;
    case ERROR:
    case FATAL:
        qCritical().noquote() << formattedMessage;
        break;
    }
}

QString CTKEnhancedLogger::formatMessage(LogLevel level, LogCategory category, 
                                        const QString& message, const QString& module)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString threadId = QString("0x%1").arg((quintptr)QThread::currentThreadId(), 0, 16);
    
    QString formattedMessage = QString("[%1] [%2] [%3] [TID:%4]")
                              .arg(timestamp)
                              .arg(levelToString(level))
                              .arg(categoryToString(category))
                              .arg(threadId);
    
    if (!module.isEmpty()) {
        formattedMessage += QString(" [%1]").arg(module);
    }
    
    formattedMessage += QString(" %1").arg(message);
    
    return formattedMessage;
}

QString CTKEnhancedLogger::levelToString(LogLevel level)
{
    switch (level) {
    case DEBUG: return "DEBUG";
    case INFO:  return "INFO ";
    case WARN:  return "WARN ";
    case ERROR: return "ERROR";
    case FATAL: return "FATAL";
    default:    return "UNKNW";
    }
}

QString CTKEnhancedLogger::categoryToString(LogCategory category)
{
    switch (category) {
    case GENERAL:         return "GENERAL";
    case PLUGIN_SYSTEM:   return "PLUGIN ";
    case OPTICAL_TRACKING: return "OPTICAL";
    case PATIENT_DATA:    return "PATIENT";
    case DICOM_PROCESSING: return "DICOM  ";
    case CALIBRATION:     return "CALIBR ";
    case SYSTEM_ERROR:    return "SYSERR ";
    case MEDICAL_AUDIT:   return "AUDIT  ";
    default:              return "UNKNOWN";
    }
}

void CTKEnhancedLogger::rotateLogFile()
{
    if (!m_logFileHandle || m_logFile.isEmpty()) return;
    
    // 关闭当前文件
    delete m_logStream;
    m_logStream = nullptr;
    m_logFileHandle->close();
    delete m_logFileHandle;
    m_logFileHandle = nullptr;
    
    // 轮转文件
    QFileInfo fileInfo(m_logFile);
    QString baseName = fileInfo.completeBaseName();
    QString suffix = fileInfo.suffix();
    QString dir = fileInfo.absolutePath();
    
    // 删除最老的文件
    QString oldestFile = QString("%1/%2.%3.%4")
                        .arg(dir, baseName, QString::number(m_maxFiles - 1), suffix);
    QFile::remove(oldestFile);
    
    // 重命名文件
    for (int i = m_maxFiles - 2; i >= 0; --i) {
        QString oldName = (i == 0) ? m_logFile : 
                         QString("%1/%2.%3.%4").arg(dir, baseName, QString::number(i), suffix);
        QString newName = QString("%1/%2.%3.%4")
                         .arg(dir, baseName, QString::number(i + 1), suffix);
        
        QFile::rename(oldName, newName);
    }
    
    // 重新打开日志文件
    setLogFile(m_logFile);
}

bool CTKEnhancedLogger::shouldLog(LogLevel level, LogCategory category)
{
    Q_UNUSED(category)  // 目前不根据类别过滤，可以扩展
    return level >= m_currentLevel;
}
