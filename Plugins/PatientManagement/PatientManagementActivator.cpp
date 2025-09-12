#include "PatientManagementActivator.h"
#include "PatientDatabaseServiceImpl.h"
#include "PatientDatabaseService.h"
#include "SQLiteManager.h"

#include <ctkPluginContext.h>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>

PatientManagementActivator::PatientManagementActivator()
    : m_context(nullptr)
    , m_databaseService(nullptr)
    , m_servicesRegistered(false)
{
    qDebug() << "PatientManagementActivator created";
}

PatientManagementActivator::~PatientManagementActivator()
{
    // 确保资源被正确清理
    if (m_servicesRegistered) {
        unregisterServices();
    }
    
    if (m_databaseService) {
        delete m_databaseService;
        m_databaseService = nullptr;
    }
    
    qDebug() << "PatientManagementActivator destroyed";
}

void PatientManagementActivator::start(ctkPluginContext* context)
{
    if (!context) {
        qCritical() << "PatientManagementActivator::start - Invalid plugin context";
        return;
    }
    
    m_context = context;
    
    try {
        logMessage("INFO", "正在启动患者管理插件...");
        
        // 注册元类型以支持信号槽传递
        qRegisterMetaType<PatientInfo>("PatientInfo");
        qRegisterMetaType<PatientImageInfo>("PatientImageInfo");
        qRegisterMetaType<SurgeryRecord>("SurgeryRecord");
        qRegisterMetaType<PatientSearchCriteria>("PatientSearchCriteria");
        
        // 初始化数据库服务
        if (!initializeDatabaseService()) {
            logMessage("ERROR", "数据库服务初始化失败");
            return;
        }
        
        // 注册服务
        if (!registerServices()) {
            logMessage("ERROR", "服务注册失败");
            return;
        }
        
        logMessage("INFO", "患者管理插件启动成功");
        
    } catch (const std::exception& e) {
        QString error = QString("插件启动异常: %1").arg(e.what());
        logMessage("ERROR", error);
        qCritical() << "PatientManagementActivator::start -" << error;
    } catch (...) {
        logMessage("ERROR", "插件启动发生未知异常");
        qCritical() << "PatientManagementActivator::start - Unknown exception";
    }
}

void PatientManagementActivator::stop(ctkPluginContext* context)
{
    Q_UNUSED(context)
    
    try {
        logMessage("INFO", "正在停止患者管理插件...");
        
        // 注销服务
        unregisterServices();
        
        // 清理数据库服务
        if (m_databaseService) {
            // 断开信号连接
            disconnect(m_databaseService, nullptr, this, nullptr);
            
            // 删除服务实例
            delete m_databaseService;
            m_databaseService = nullptr;
        }
        
        // 清理上下文引用
        m_context = nullptr;
        
        logMessage("INFO", "患者管理插件停止成功");
        
    } catch (const std::exception& e) {
        QString error = QString("插件停止异常: %1").arg(e.what());
        logMessage("ERROR", error);
        qCritical() << "PatientManagementActivator::stop -" << error;
    } catch (...) {
        logMessage("ERROR", "插件停止发生未知异常");
        qCritical() << "PatientManagementActivator::stop - Unknown exception";
    }
}

bool PatientManagementActivator::initializeDatabaseService()
{
    try {
        // 创建数据库服务实例
        m_databaseService = new PatientDatabaseServiceImpl(this);
        
        // 设置插件上下文
        m_databaseService->setPluginContext(m_context);
        
        // 连接数据库信号
        connect(m_databaseService, &PatientDatabaseService::databaseError,
                this, &PatientManagementActivator::onDatabaseError);
        
        connect(m_databaseService, &PatientDatabaseService::databaseStatusChanged,
                this, [this](const QString& status) {
                    logMessage("INFO", QString("数据库状态变更: %1").arg(status));
                });
        
        // 初始化数据库
        logMessage("INFO", "开始初始化数据库...");
        if (!m_databaseService->initializeDatabase()) {
            logMessage("ERROR", "数据库初始化失败");
            delete m_databaseService;
            m_databaseService = nullptr;
            return false;
        }
        logMessage("INFO", "数据库初始化成功");
        
        // 发出初始化完成信号
        logMessage("INFO", "调用onDatabaseInitialized...");
        onDatabaseInitialized();
        logMessage("INFO", "onDatabaseInitialized调用完成");
        
        logMessage("INFO", "数据库服务初始化成功");
        return true;
        
    } catch (const std::exception& e) {
        QString error = QString("数据库服务初始化异常: %1").arg(e.what());
        logMessage("ERROR", error);
        
        if (m_databaseService) {
            delete m_databaseService;
            m_databaseService = nullptr;
        }
        
        return false;
    } catch (...) {
        logMessage("ERROR", "数据库服务初始化发生未知异常");
        
        if (m_databaseService) {
            delete m_databaseService;
            m_databaseService = nullptr;
        }
        
        return false;
    }
}

bool PatientManagementActivator::registerServices()
{
    if (!m_context || !m_databaseService) {
        logMessage("ERROR", "无法注册服务：上下文或服务实例无效");
        return false;
    }
    
    try {
        // 设置服务属性
        ctkDictionary serviceProps;
        serviceProps.insert("service.description", "患者数据管理服务");
        serviceProps.insert("service.vendor", "Medical Navigation System");
        serviceProps.insert("service.version", "1.0.0");
        serviceProps.insert("service.category", "medical");
        
        // 注册PatientDatabaseService服务
        m_serviceRegistration = m_context->registerService<PatientDatabaseService>(
            m_databaseService, 
            serviceProps
        );
        
        if (!m_serviceRegistration) {
            logMessage("ERROR", "服务注册失败：无法获取服务注册句柄");
            return false;
        }
        
        m_servicesRegistered = true;
        
        logMessage("INFO", QString("PatientDatabaseService服务注册成功，服务ID: %1")
                   .arg(m_serviceRegistration.getReference().getProperty("service.id").toString()));
        
        return true;
        
    } catch (const std::exception& e) {
        QString error = QString("服务注册异常: %1").arg(e.what());
        logMessage("ERROR", error);
        return false;
    } catch (...) {
        logMessage("ERROR", "服务注册发生未知异常");
        return false;
    }
}

void PatientManagementActivator::unregisterServices()
{
    try {
        if (m_servicesRegistered && m_serviceRegistration) {
            logMessage("INFO", "正在注销PatientDatabaseService服务...");
            
            m_serviceRegistration.unregister();
            m_serviceRegistration = ctkServiceRegistration();
            
            logMessage("INFO", "服务注销成功");
        }
        
        m_servicesRegistered = false;
        
    } catch (const std::exception& e) {
        QString error = QString("服务注销异常: %1").arg(e.what());
        logMessage("ERROR", error);
        qWarning() << "PatientManagementActivator::unregisterServices -" << error;
    } catch (...) {
        logMessage("ERROR", "服务注销发生未知异常");
        qWarning() << "PatientManagementActivator::unregisterServices - Unknown exception";
    }
}

void PatientManagementActivator::onDatabaseInitialized()
{
    logMessage("INFO", "数据库初始化完成，开始验证数据库状态...");
    
    if (m_databaseService) {
        try {
            // 首先检查数据库状态
            QString status = m_databaseService->getDatabaseStatus();
            logMessage("INFO", QString("数据库状态: %1").arg(status));
            
            // 只有在数据库已连接时才查询统计信息
            if (status.contains("已连接")) {
                int patientCount = m_databaseService->getPatientCount();
                int imageCount = m_databaseService->getImageCount();
                
                logMessage("INFO", QString("当前数据统计 - 患者: %1, 影像: %2")
                           .arg(patientCount).arg(imageCount));
            } else {
                logMessage("WARN", "数据库未连接，跳过统计信息查询");
            }
        } catch (const std::exception& e) {
            logMessage("ERROR", QString("数据库验证异常: %1").arg(e.what()));
        } catch (...) {
            logMessage("ERROR", "数据库验证发生未知异常");
        }
    }
}

void PatientManagementActivator::onDatabaseError(const QString& error)
{
    logMessage("ERROR", QString("数据库错误: %1").arg(error));
    
    // 可以在这里添加错误恢复逻辑
    // 例如：尝试重新连接数据库、发送错误通知等
}

void PatientManagementActivator::logMessage(const QString& level, const QString& message) const
{
    QString logEntry = QString("[PatientManagement] [%1] %2").arg(level, message);
    
    if (level == "ERROR") {
        qCritical() << logEntry;
    } else if (level == "WARN") {
        qWarning() << logEntry;
    } else {
        qDebug() << logEntry;
    }
    
    // 如果有全局日志服务，可以在这里发送日志
    // 例如：通过CTK事件系统或自定义日志服务
}


