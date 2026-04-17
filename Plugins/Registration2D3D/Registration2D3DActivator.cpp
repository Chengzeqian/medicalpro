#include "Registration2D3DActivator.h"
#include "Registration2D3DServiceImpl.h"
#include <QDebug>
#include <QCoreApplication>
#include <QDir>

Registration2D3DActivator::Registration2D3DActivator()
    : m_serviceImpl(nullptr)
    , m_service(nullptr)
{
    qDebug() << "[Registration2D3D] 激活器创建";
}

Registration2D3DActivator::~Registration2D3DActivator()
{
    qDebug() << "[Registration2D3D] 激活器销毁";
}

void Registration2D3DActivator::start(ctkPluginContext* context)
{
    qDebug() << "[Registration2D3D] 插件启动";

    try {
        // 创建服务实例（不设置父对象）
        m_serviceImpl = new Registration2D3DServiceImpl(nullptr);
        m_service = m_serviceImpl;  // 服务接口指针指向实现类

        // 将服务移动到主线程，避免跨线程问题
        QThread* mainThread = QCoreApplication::instance()->thread();
        if (QThread::currentThread() != mainThread) {
            qDebug() << "[Registration2D3D] 将服务移动到主线程";
            m_serviceImpl->moveToThread(mainThread);
        }
        qDebug() << "[Registration2D3D] 服务实例创建成功";

        // 设置CTK上下文（用于Widget工厂）
        m_serviceImpl->setContext(context);
        qDebug() << "[Registration2D3D] CTK上下文已设置";

        // 初始化Python环境
        QString appDir = QCoreApplication::applicationDirPath();
        QString pythonHome;  // 留空使用系统Python
        QString scriptsPath;

        // 搜索Regi脚本目录的候选路径
        QStringList searchPaths = {
            appDir + "/Regi",                           // 1. 可执行文件同级目录
            appDir + "/../Regi",                        // 2. 上级目录
            appDir + "/../../Regi",                     // 3. 上两级目录（开发环境）
            QDir::currentPath() + "/Regi",              // 4. 当前工作目录
            "D:/Qtproject/medicalpro/Regi"              // 5. 源代码目录（开发时硬编码）
        };

        for (const QString& path : searchPaths) {
            QDir dir(path);
            if (dir.exists() && dir.exists("2D3DRegistration.py")) {
                scriptsPath = dir.absolutePath();
                qDebug() << "[Registration2D3D] 找到Python脚本目录:" << scriptsPath;
                break;
            }
        }

        if (!scriptsPath.isEmpty()) {
            // 保存路径供后续使用（启动预热阶段会进行初始化）
            qDebug() << "[Registration2D3D] Python脚本目录已找到:" << scriptsPath;
            m_serviceImpl->setConfiguration("pythonHome", pythonHome);
            m_serviceImpl->setConfiguration("scriptsPath", scriptsPath);
            m_serviceImpl->setConfiguration("pythonInitDeferred", true);
            qDebug() << "[Registration2D3D] Python初始化将在启动预热阶段完成";
        } else {
            qWarning() << "[Registration2D3D] Python脚本目录不存在，已搜索以下路径:";
            for (const QString& path : searchPaths) {
                qWarning() << "  -" << path;
            }
            qWarning() << "[Registration2D3D] 配准功能将不可用，请确保Regi目录已正确部署";
        }

        // 注册服务到CTK服务注册表
        context->registerService<Registration2D3DService>(m_service);
        qDebug() << "[Registration2D3D] 服务已注册到CTK框架";

    } catch (const std::exception& e) {
        qCritical() << "[Registration2D3D] 标准异常:" << e.what();
        // 不重新抛出异常，避免CTK框架崩溃
    } catch (...) {
        qCritical() << "[Registration2D3D] 未知异常";
        // 不重新抛出异常，避免CTK框架崩溃
    }
}

void Registration2D3DActivator::stop(ctkPluginContext* context)
{
    Q_UNUSED(context);
    qDebug() << "[Registration2D3D] 插件停止";
    
    // 清理Python环境
    if (m_service) {
        m_service->finalizePythonEnvironment();
    }
    
    // 服务将由CTK框架自动注销
    if (m_service) {
        delete m_service;
        m_service = nullptr;
    }
    
    qDebug() << "[Registration2D3D] 插件已停止";
}

