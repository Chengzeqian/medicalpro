#include "FourViewDisplayActivator.h"
#include "FourViewDisplayServiceImpl.h"
#include "FourViewDisplayService.h"
#include <QDebug>
#include <QCoreApplication>
#include <QDateTime>
#include <QPointer>
#include <QThread>
#include <ctkPluginContext.h>
#include <service/event/ctkEventAdmin.h>

// VTK includes for factory initialization
#include <vtkImageData.h>

FourViewDisplayActivator::FourViewDisplayActivator()
    : m_context(nullptr)
    , m_fourViewService(nullptr)
    , m_servicesRegistered(false)
{
    qDebug() << "[FourViewDisplayActivator] 四视图显示插件激活器创建";
}

FourViewDisplayActivator::~FourViewDisplayActivator()
{
    qDebug() << "[FourViewDisplayActivator] 四视图显示插件激活器销毁";
}

void FourViewDisplayActivator::start(ctkPluginContext* context)
{
    qDebug() << "[FourViewDisplayActivator] 启动四视图显示插件";

    if (!context) {
        logMessage("ERROR", "插件上下文为空，无法启动");
        return;
    }

    m_context = context;

    // 同步注册服务（与 Registration2D3D 保持一致）
    try {
        qDebug() << "[FourViewDisplayActivator] 当前线程:" << QThread::currentThread();
        qDebug() << "[FourViewDisplayActivator] 主线程:" << QCoreApplication::instance()->thread();
        qDebug() << "[FourViewDisplayActivator] VTK 初始化将延迟到首次使用时";

        if (!initializeFourViewService()) {
            logMessage("ERROR", "四视图显示服务初始化失败");
            return;
        }

        if (!registerServices()) {
            logMessage("ERROR", "服务注册失败");
            return;
        }

        logMessage("INFO", "四视图显示插件启动成功");

    } catch (const std::exception& e) {
        logMessage("ERROR", QString("插件启动异常: %1").arg(e.what()));
    } catch (...) {
        logMessage("ERROR", "插件启动时发生未知异常");
    }
}

void FourViewDisplayActivator::stop(ctkPluginContext* context)
{
    qDebug() << "[FourViewDisplayActivator] 停止四视图显示插件";
    
    Q_UNUSED(context)
    
    try {
        // 注销所有服务
        unregisterServices();
        
        // 清理四视图显示服务
        if (m_fourViewService) {
            // 断开信号连接
            disconnect(m_fourViewService, nullptr, this, nullptr);
            
            // 删除服务实例
            m_fourViewService->deleteLater();
            m_fourViewService = nullptr;
        }
        
        m_context = nullptr;
        
        logMessage("INFO", "四视图显示插件停止完成");
        
    } catch (const std::exception& e) {
        logMessage("ERROR", QString("插件停止异常: %1").arg(e.what()));
    } catch (...) {
        logMessage("ERROR", "插件停止时发生未知异常");
    }
}

bool FourViewDisplayActivator::initializeFourViewService()
{
    qDebug() << "[FourViewDisplayActivator] 初始化四视图显示服务";
    
    try {
        // 创建四视图显示服务实例（不设置父对象，避免线程问题）
        m_fourViewService = new FourViewDisplayServiceImpl(nullptr);

        // 将服务移动到主线程，避免跨线程问题
        QThread* mainThread = QCoreApplication::instance()->thread();
        if (QThread::currentThread() != mainThread) {
            qDebug() << "[FourViewDisplayActivator] 将服务移动到主线程";
            m_fourViewService->moveToThread(mainThread);
        }

        // 设置 CTK Context（使服务能够创建和初始化 FourViewWidget）
        m_fourViewService->setPluginContext(m_context);

        // 连接信号槽（使用 Qt::QueuedConnection 确保跨线程安全）
        connect(m_fourViewService, &FourViewDisplayServiceImpl::serviceError,
                this, [this](const QString& error) {
                    logMessage("ERROR", QString("服务错误: %1").arg(error));
                }, Qt::QueuedConnection);
        
        // 获取EventAdmin服务引用
        ctkServiceReference eventAdminRef = m_context->getServiceReference<ctkEventAdmin>();
        if (eventAdminRef) {
            ctkEventAdmin* eventAdmin = qobject_cast<ctkEventAdmin*>(
                m_context->getService(eventAdminRef));
            if (eventAdmin) {
                qDebug() << "[FourViewDisplayActivator] EventAdmin服务已连接";
            }
        }
        
        qDebug() << "[FourViewDisplayActivator] 四视图显示服务初始化成功";
        return true;
        
    } catch (const std::exception& e) {
        logMessage("ERROR", QString("四视图显示服务初始化异常: %1").arg(e.what()));
        return false;
    } catch (...) {
        logMessage("ERROR", "四视图显示服务初始化时发生未知异常");
        return false;
    }
}

bool FourViewDisplayActivator::registerServices()
{
    qDebug() << "[FourViewDisplayActivator] 注册四视图显示服务";
    
    if (!m_context || !m_fourViewService) {
        logMessage("ERROR", "插件上下文或服务实例为空，无法注册服务");
        return false;
    }
    
    try {
        // 准备服务属性
        ctkDictionary serviceProperties;
        serviceProperties.insert("service.description", "四视图显示服务");
        serviceProperties.insert("service.vendor", "MedicalPro");
        serviceProperties.insert("service.version", "1.0.0");
        serviceProperties.insert(ctkPluginConstants::SERVICE_RANKING, 100);
        
        // 注册FourViewDisplayService服务
        m_serviceRegistration = m_context->registerService<FourViewDisplayService>(
            m_fourViewService, serviceProperties);
        
        if (!m_serviceRegistration) {
            logMessage("ERROR", "FourViewDisplayService服务注册失败");
            return false;
        }
        
        m_servicesRegistered = true;
        
        qDebug() << "[FourViewDisplayActivator] 服务注册成功:"
                << "FourViewDisplayService - ID:" << m_serviceRegistration.getReference().getProperty(ctkPluginConstants::SERVICE_ID).toLongLong();
        
        logMessage("INFO", "四视图显示服务注册成功");
        return true;
        
    } catch (const std::exception& e) {
        logMessage("ERROR", QString("服务注册异常: %1").arg(e.what()));
        return false;
    } catch (...) {
        logMessage("ERROR", "服务注册时发生未知异常");
        return false;
    }
}

void FourViewDisplayActivator::unregisterServices()
{
    qDebug() << "[FourViewDisplayActivator] 注销四视图显示服务";
    
    if (!m_servicesRegistered) {
        qDebug() << "[FourViewDisplayActivator] 没有已注册的服务需要注销";
        return;
    }
    
    try {
        // 注销FourViewDisplayService服务
        if (m_serviceRegistration) {
            qDebug() << "[FourViewDisplayActivator] 注销FourViewDisplayService服务";
            m_serviceRegistration.unregister();
            m_serviceRegistration = ctkServiceRegistration();
        }
        
        m_servicesRegistered = false;
        
        logMessage("INFO", "四视图显示服务注销完成");
        
    } catch (const std::exception& e) {
        logMessage("ERROR", QString("服务注销异常: %1").arg(e.what()));
    } catch (...) {
        logMessage("ERROR", "服务注销时发生未知异常");
    }
}

void FourViewDisplayActivator::logMessage(const QString& level, const QString& message) const
{
    QString logMsg = QString("[FourViewDisplayActivator][%1] %2").arg(level).arg(message);
    
    if (level == "ERROR") {
        qCritical() << logMsg;
    } else if (level == "WARN") {
        qWarning() << logMsg;
    } else {
        qDebug() << logMsg;
    }
    
    // 如果有EventAdmin服务，可以发送日志事件
    if (m_context) {
        ctkServiceReference eventAdminRef = m_context->getServiceReference<ctkEventAdmin>();
        if (eventAdminRef) {
            ctkEventAdmin* eventAdmin = qobject_cast<ctkEventAdmin*>(
                m_context->getService(eventAdminRef));
            if (eventAdmin) {
                ctkDictionary eventProperties;
                eventProperties.insert("level", level);
                eventProperties.insert("message", message);
                eventProperties.insert("source", "FourViewDisplayActivator");
                eventProperties.insert("timestamp", QDateTime::currentDateTime().toString(Qt::ISODate));
                
                ctkEvent logEvent("log/four_view_display", eventProperties);
                eventAdmin->postEvent(logEvent);
            }
        }
    }
}

