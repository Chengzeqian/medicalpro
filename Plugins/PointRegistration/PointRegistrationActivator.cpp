#include "PointRegistrationActivator.h"
#include "PointRegistrationService.h"
#include "PointRegistrationServiceImpl.h"

#include <ctkPluginConstants.h>
#include <QDebug>

PointRegistrationActivator::PointRegistrationActivator()
    : m_context(nullptr)
    , m_service(nullptr)
    , m_serviceRegistered(false)
{
    logMessage("INFO", "激活器实例已创建");
}

PointRegistrationActivator::~PointRegistrationActivator()
{
    logMessage("INFO", "激活器实例已销毁");
}

void PointRegistrationActivator::start(ctkPluginContext* context)
{
	// 顶层异常保护：任何在插件启动过程中抛出的异常都不允许传播到 CTK / Qt
	// 这样可以避免 CTK 将其包装为 ctkRuntimeException: ctkPlugin start failed，
	// 保证应用整体稳定，只通过日志来暴露问题。
	try {
		logMessage("INFO", "========== 点配准插件启动 ==========");
		
		m_context = context;
		
		if (!m_context) {
			logMessage("ERROR", "无效的插件上下文");
			return;
		}
		
		// 1. 初始化服务
		if (!initializeService()) {
			logMessage("ERROR", "服务初始化失败");
			return;
		}
		
		// 2. 注册服务
		if (!registerService()) {
			logMessage("ERROR", "服务注册失败");
			return;
		}
		
		logMessage("INFO", "========== 点配准插件启动完成 ==========");
	} catch (const std::exception& e) {
		// 捕获标准异常（包括绝大多数第三方库抛出的异常）
		logMessage("ERROR", QString("插件启动异常: %1").arg(e.what()));
		// 不向外抛出，避免 CTK 认为插件启动失败而抛出 ctkRuntimeException
	} catch (...) {
		// 捕获所有未知异常（例如 CTK 自定义异常等）
		logMessage("ERROR", "插件启动发生未知异常（可能为 CTK 异常）");
		// 同样不再向外抛出
	}
}

void PointRegistrationActivator::stop(ctkPluginContext* context)
{
    Q_UNUSED(context);
    
    logMessage("INFO", "========== 点配准插件停止 ==========");
    
    // 1. 注销服务
    unregisterService();
    
    // 2. 清理服务实例
    if (m_service) {
        delete m_service;
        m_service = nullptr;
    }
    
    m_context = nullptr;
    
    logMessage("INFO", "========== 点配准插件停止完成 ==========");
}

bool PointRegistrationActivator::initializeService()
{
    logMessage("INFO", "初始化服务...");
    
    try {
        m_service = new PointRegistrationServiceImpl(this);
        logMessage("INFO", "服务初始化成功");
        return true;
    } catch (const std::exception& e) {
        logMessage("ERROR", QString("服务初始化异常: %1").arg(e.what()));
        return false;
    }
}

bool PointRegistrationActivator::registerService()
{
    logMessage("INFO", "注册服务...");
    
    if (!m_context || !m_service) {
        logMessage("ERROR", "无法注册服务：上下文或服务实例为空");
        return false;
    }
    
    try {
        // 准备服务属性
        ctkDictionary properties;
        properties.insert("service.description", "基于点的配准服务");
        properties.insert("service.vendor", "MedicalPro");
        properties.insert("service.version", "1.0.0");
        properties.insert(ctkPluginConstants::SERVICE_RANKING, 100);
        
        // 注册服务
        m_serviceRegistration = m_context->registerService<PointRegistrationService>(
            m_service, properties);
        
        if (!m_serviceRegistration) {
            logMessage("ERROR", "服务注册失败");
            return false;
        }
        
        m_serviceRegistered = true;
        
        auto serviceId = m_serviceRegistration.getReference()
            .getProperty(ctkPluginConstants::SERVICE_ID).toLongLong();
        logMessage("INFO", QString("服务注册成功，ID: %1").arg(serviceId));
        
        return true;
        
    } catch (const std::exception& e) {
        logMessage("ERROR", QString("服务注册异常: %1").arg(e.what()));
        return false;
    }
}

void PointRegistrationActivator::unregisterService()
{
    if (m_serviceRegistered && m_serviceRegistration) {
        logMessage("INFO", "注销服务...");
        
        try {
            m_serviceRegistration.unregister();
            m_serviceRegistered = false;
            logMessage("INFO", "服务注销成功");
        } catch (const std::exception& e) {
            logMessage("WARNING", QString("服务注销异常: %1").arg(e.what()));
        }
    }
}

void PointRegistrationActivator::logMessage(const QString& level, const QString& message) const
{
    QString formattedMsg = QString("[PointRegistrationPlugin][%1] %2").arg(level, message);
    
    if (level == "ERROR") {
        qCritical().noquote() << formattedMsg;
    } else if (level == "WARNING") {
        qWarning().noquote() << formattedMsg;
    } else {
        qDebug().noquote() << formattedMsg;
    }
}



