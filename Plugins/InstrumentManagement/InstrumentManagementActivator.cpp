#include <stdexcept>
#include "InstrumentManagementActivator.h"
#include "InstrumentManagementServiceImpl.h"
#include <QDebug>

InstrumentManagementActivator::InstrumentManagementActivator()
    : m_context(nullptr)
    , m_service(nullptr)
{
    qDebug() << "[InstrumentManagement] 激活器创建";
}

InstrumentManagementActivator::~InstrumentManagementActivator()
{
    qDebug() << "[InstrumentManagement] 激活器销毁";
}

void InstrumentManagementActivator::start(ctkPluginContext* context)
{
    qDebug() << "[InstrumentManagement] 插件启动";

    if (!context) {
        qCritical() << "[InstrumentManagement] CTK 上下文为空，无法初始化";
        throw std::runtime_error("CTK 上下文为空");
    }

    m_context = context;

    // 【修复】改为同步执行，确保服务在插件启动完成时已注册
    // 原来使用 QTimer::singleShot 导致服务注册是异步的
    try {
        m_service = new InstrumentManagementServiceImpl();
        qDebug() << "[InstrumentManagement] 服务实例创建成功";

        m_context->registerService<InstrumentManagementService>(m_service);
        qDebug() << "[InstrumentManagement] 服务已注册";

    } catch (const std::exception& e) {
        // 仅记录错误并清理资源，避免异常向外传播导致 CTK/Qt 崩溃
        qCritical() << "[InstrumentManagement] 标准异常:" << e.what();
        if (m_service) {
            delete m_service;
            m_service = nullptr;
        }
        m_context = nullptr;
    } catch (...) {
        qCritical() << "[InstrumentManagement] 未知异常";
        if (m_service) {
            delete m_service;
            m_service = nullptr;
        }
        m_context = nullptr;
    }
}

void InstrumentManagementActivator::stop(ctkPluginContext* context)
{
    Q_UNUSED(context);
    qDebug() << "[InstrumentManagement] 插件停止";
    
    // 服务将由CTK框架自动注销
    if (m_service) {
        delete m_service;
        m_service = nullptr;
    }
    m_context = nullptr;
}

