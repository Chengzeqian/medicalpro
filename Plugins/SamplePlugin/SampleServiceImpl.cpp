#include "SampleServiceImpl.h"
#include <QDebug>

SampleServiceImpl::SampleServiceImpl(QObject *parent)
    : QObject(parent)
#ifdef CTK_PLUGIN_FRAMEWORK
    , m_context(nullptr)
#endif
    , m_active(false)
    , m_operationCount(0)
{
    logMessage("SampleServiceImpl created");
}

#ifdef CTK_PLUGIN_FRAMEWORK
SampleServiceImpl::SampleServiceImpl(ctkPluginContext* context, QObject *parent)
    : QObject(parent)
    , m_context(context)
    , m_active(false)
    , m_operationCount(0)
{
    logMessage("SampleServiceImpl created with CTK context");
    
    // 自动注册服务到CTK框架
    if (m_context) {
        try {
            m_context->registerService<SampleService>(this);
            logMessage("SampleService registered to CTK framework");
        } catch (const std::exception& e) {
            logMessage(QString("Failed to register service: %1").arg(e.what()));
        }
    }
}
#endif

SampleServiceImpl::~SampleServiceImpl()
{
    stopService();
    logMessage("SampleServiceImpl destroyed");
}

QString SampleServiceImpl::getServiceName() const
{
    return "Sample Medical Plugin Service";
}

QString SampleServiceImpl::getServiceVersion() const
{
    return "1.0.0";
}

QString SampleServiceImpl::performOperation(const QString& input)
{
    if (!m_active) {
        QString error = "Service is not active";
        logMessage(error);
        return error;
    }
    
    m_operationCount++;
    QString result = QString("Processed input '%1' - Operation #%2 at %3")
                        .arg(input)
                        .arg(m_operationCount)
                        .arg(QDateTime::currentDateTime().toString());
    
    logMessage(QString("Performed operation: %1").arg(result));
    emit operationPerformed(input, result);
    
    return result;
}

bool SampleServiceImpl::isActive() const
{
    return m_active;
}

void SampleServiceImpl::startService()
{
    if (m_active) {
        logMessage("Service already active");
        return;
    }
    
    m_active = true;
    m_startTime = QDateTime::currentDateTime();
    m_operationCount = 0;
    
    logMessage(QString("SampleService started at %1").arg(m_startTime.toString()));
    emit serviceStatusChanged(true);
}

void SampleServiceImpl::stopService()
{
    if (!m_active) {
        return;
    }
    
    m_active = false;
    QDateTime stopTime = QDateTime::currentDateTime();
    qint64 runtime = m_startTime.secsTo(stopTime);
    
    logMessage(QString("SampleService stopped after %1 seconds, performed %2 operations")
               .arg(runtime)
               .arg(m_operationCount));
    
    emit serviceStatusChanged(false);
}

void SampleServiceImpl::logMessage(const QString& message)
{
    qDebug() << "[SampleServiceImpl]" << message;
}
