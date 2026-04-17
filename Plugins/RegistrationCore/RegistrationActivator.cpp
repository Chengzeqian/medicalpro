#include "RegistrationActivator.h"
#include "RegistrationServiceImpl.h"
#include "RegistrationService.h"
#include <QDebug>

RegistrationActivator::RegistrationActivator()
    : m_serviceImpl(nullptr)
{
}

RegistrationActivator::~RegistrationActivator()
{
}

void RegistrationActivator::start(ctkPluginContext* context)
{
    qDebug() << "[RegistrationCore] Plugin starting...";

    try {
        // 创建服务实现
        m_serviceImpl = new RegistrationServiceImpl();

        // 设置插件上下文（用于访问其他服务，如 Registration2D3D）
        m_serviceImpl->setPluginContext(context);

        // 注册服务
        m_serviceRegistration = context->registerService<RegistrationService>(m_serviceImpl);

        qDebug() << "[RegistrationCore] Service registered successfully";
        qInfo() << "[RegistrationCore] Registration algorithms ready (Landmark + ICP + 2D-3D)";

    } catch (const std::exception& ex) {
        qCritical() << "[RegistrationCore] Failed to start plugin:" << ex.what();
        throw;
    }
}

void RegistrationActivator::stop(ctkPluginContext* context)
{
    Q_UNUSED(context);

    qDebug() << "[RegistrationCore] Plugin stopping...";

    // 注销服务
    m_serviceRegistration.unregister();

    // 删除服务实现
    if (m_serviceImpl) {
        delete m_serviceImpl;
        m_serviceImpl = nullptr;
    }

    qDebug() << "[RegistrationCore] Plugin stopped";
}