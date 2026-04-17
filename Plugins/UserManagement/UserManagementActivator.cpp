#include "UserManagementActivator.h"
#include "UserManagementServiceImpl.h"
#include "UserManagementService.h"
#include <QDebug>
#include <QCoreApplication>
#include <ctkPluginContext.h>
#include <service/event/ctkEventAdmin.h>
#include <stdexcept>

UserManagementActivator::UserManagementActivator()
    : m_context(nullptr)
    , m_userService(nullptr)
    , m_servicesRegistered(false)
{
    qDebug() << "[UserManagementActivator] UserManagement activator created";
}

UserManagementActivator::~UserManagementActivator()
{
    qDebug() << "[UserManagementActivator] UserManagement activator destroyed";
}

void UserManagementActivator::start(ctkPluginContext* context)
{
    qDebug() << "[UserManagementActivator] Starting UserManagement plugin";

    if (!context) {
        logMessage("ERROR", "Plugin context is null, cannot start UserManagement plugin");
        return;
    }

    m_context = context;

    // 【修复】改为同步执行，确保服务在插件启动完成时已经注册
    // 原来使用 Qt::QueuedConnection 导致服务注册是异步的，
    // 当 StartupOrchestrator 认为插件启动完成时，服务实际上还没有注册
    try {
        if (!initializeUserService()) {
            logMessage("ERROR", "Failed to initialize UserManagement service");
            throw std::runtime_error("Failed to initialize UserManagement service");
        }

        if (!registerServices()) {
            logMessage("ERROR", "Failed to register UserManagement services");
            throw std::runtime_error("Failed to register UserManagement services");
        }

        logMessage("INFO", "UserManagement plugin started successfully");
    } catch (const std::exception& e) {
        // 不再向外重新抛出异常，避免 CTK/Qt 捕获到异常后导致应用退出
        logMessage("ERROR", QString("Plugin start exception: %1").arg(e.what()));

        // 清理可能已部分初始化的资源
        if (m_userService) {
            disconnect(m_userService, nullptr, this, nullptr);
            delete m_userService;
            m_userService = nullptr;
        }

        m_context = nullptr;
    } catch (...) {
        // 捕获所有未知异常，防止继续向上抛出
        logMessage("ERROR", "Unknown exception occurred while starting plugin");

        if (m_userService) {
            disconnect(m_userService, nullptr, this, nullptr);
            delete m_userService;
            m_userService = nullptr;
        }

        m_context = nullptr;
    }
}

void UserManagementActivator::stop(ctkPluginContext* context)
{
    qDebug() << "[UserManagementActivator] Stopping UserManagement plugin";
    
    Q_UNUSED(context)
    
    try {
        // 注销所有服务
        unregisterServices();
        
        // 清理用户管理服务
        if (m_userService) {
            // 断开信号连接
            disconnect(m_userService, nullptr, this, nullptr);
            
            // 删除服务实例
            m_userService->deleteLater();
            m_userService = nullptr;
        }
        
        m_context = nullptr;
        
        logMessage("INFO", "UserManagement plugin stopped");
        
    } catch (const std::exception& e) {
        logMessage("ERROR", QString("Plugin stop exception: %1").arg(e.what()));
    } catch (...) {
        logMessage("ERROR", "Unknown exception occurred while stopping plugin");
    }
}

void UserManagementActivator::onDatabaseInitialized()
{
    qDebug() << "[UserManagementActivator] User database initialized";
    logMessage("INFO", "User database initialized");
}

void UserManagementActivator::onDatabaseError(const QString& error)
{
    qWarning() << "[UserManagementActivator] User database error:" << error;
    logMessage("ERROR", QString("User database error: %1").arg(error));
}

bool UserManagementActivator::initializeUserService()
{
    qDebug() << "[UserManagementActivator] Initializing UserManagement service";
    
    try {
        // 创建用户管理服务实例
        m_userService = new UserManagementServiceImpl(this);
        
        // 连接信号槽
        connect(m_userService, &UserManagementServiceImpl::databaseError,
                this, &UserManagementActivator::onDatabaseError);
        
        // 获取EventAdmin服务引用
        ctkServiceReference eventAdminRef = m_context->getServiceReference<ctkEventAdmin>();
        if (eventAdminRef) {
            ctkEventAdmin* eventAdmin = qobject_cast<ctkEventAdmin*>(
                m_context->getService(eventAdminRef));
            if (eventAdmin) {
                // 设置EventAdmin引用（需要在UserManagementServiceImpl中添加setter方法）
                // m_userService->setEventAdmin(eventAdmin);
                qDebug() << "[UserManagementActivator] EventAdmin service connected";
            }
        }
        
        // 初始化数据库
        if (!m_userService->initializeDatabase()) {
            logMessage("ERROR", "Failed to initialize user database");
            delete m_userService;
            m_userService = nullptr;
            return false;
        }
        
        onDatabaseInitialized();
        
        qDebug() << "[UserManagementActivator] UserManagement service initialized";
        return true;
        
    } catch (const std::exception& e) {
        logMessage("ERROR", QString("UserManagement service init exception: %1").arg(e.what()));
        return false;
    } catch (...) {
        logMessage("ERROR", "Unknown exception occurred while initializing UserManagement service");
        return false;
    }
}

bool UserManagementActivator::registerServices()
{
    qDebug() << "[UserManagementActivator] Registering UserManagement services";
    
    if (!m_context || !m_userService) {
        logMessage("ERROR", "Plugin context or service instance is null, cannot register services");
        return false;
    }
    
    try {
        // 准备服务属性
        ctkDictionary serviceProperties;
        serviceProperties.insert("service.description", "用户管理服务");
        serviceProperties.insert("service.vendor", "MedicalPro");
        serviceProperties.insert("service.version", "1.0.0");
        serviceProperties.insert(ctkPluginConstants::SERVICE_RANKING, 100);
        
        // 注册UserManagementService服务
        m_serviceRegistration = m_context->registerService<UserManagementService>(
            m_userService, serviceProperties);
        
        if (!m_serviceRegistration) {
            logMessage("ERROR", "Failed to register UserManagementService");
            return false;
        }
        
        m_servicesRegistered = true;

        qDebug() << "[UserManagementActivator] Service registration succeeded:"
                << "UserManagementService - ID:" << m_serviceRegistration.getReference().getProperty(ctkPluginConstants::SERVICE_ID).toLongLong();

        logMessage("INFO", "UserManagement services registered");
        return true;
        
    } catch (const std::exception& e) {
        logMessage("ERROR", QString("Service registration exception: %1").arg(e.what()));
        return false;
    } catch (...) {
        logMessage("ERROR", "Unknown exception occurred while registering services");
        return false;
    }
}

void UserManagementActivator::unregisterServices()
{
    qDebug() << "[UserManagementActivator] Unregistering UserManagement services";
    
    if (!m_servicesRegistered) {
        qDebug() << "[UserManagementActivator] No registered services to unregister";
        return;
    }
    
    try {
        // 注销UserManagementService服务
        if (m_serviceRegistration) {
            qDebug() << "[UserManagementActivator] Unregistering UserManagementService";
            m_serviceRegistration.unregister();
            m_serviceRegistration = ctkServiceRegistration();
        }
        
        m_servicesRegistered = false;

        logMessage("INFO", "UserManagement services unregistered");
        
    } catch (const std::exception& e) {
        logMessage("ERROR", QString("Service unregister exception: %1").arg(e.what()));
    } catch (...) {
        logMessage("ERROR", "Unknown exception occurred while unregistering services");
    }
}

void UserManagementActivator::logMessage(const QString& level, const QString& message) const
{
    QString logMsg = QString("[UserManagementActivator][%1] %2").arg(level).arg(message);
    
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
                eventProperties.insert("source", "UserManagementActivator");
                eventProperties.insert("timestamp", QDateTime::currentDateTime().toString(Qt::ISODate));
                
                ctkEvent logEvent("log/user_management", eventProperties);
                eventAdmin->postEvent(logEvent);
            }
        }
    }
}

// #include "UserManagementActivator.moc" // 移除MOC包含，因为该类没有Q_OBJECT宏
