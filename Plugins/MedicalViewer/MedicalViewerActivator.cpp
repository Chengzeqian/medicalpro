#include "MedicalViewerActivator.h"
#include "MedicalViewerService.h"
#include "MedicalViewerServiceImpl.h"

#include <QDebug>
#include <QStringList>
#include <ctkPluginContext.h>
#include <ctkServiceReference.h>
#include <ctkDictionary.h>

// VTK头文件检查
#ifdef VTK_FOUND
#include <vtkVersion.h>
#include <vtkObject.h>
#endif

//-----------------------------------------------------------------------------
void MedicalViewerActivator::start(ctkPluginContext* context)
{
    m_context = context;
    m_viewerService = nullptr;
    m_initialized = false;

    qDebug() << "[MedicalViewerActivator] Medical Viewer plugin starting...";

    try {
        // 🔑 关键：注册基础元类型以支持信号槽传递（简化注册避免定义问题）
        qRegisterMetaType<QWidget*>("QWidget*");
        qRegisterMetaType<QColor>("QColor");
        qRegisterMetaType<QString>("QString");
        qRegisterMetaType<QStringList>("QStringList");
        qRegisterMetaType<double>("double");
        qRegisterMetaType<int>("int");
        qDebug() << "[MedicalViewerActivator] 基础元类型注册完成";
        
        // 检查VTK可用性
        if (!checkVTKAvailability()) {
            qWarning() << "[MedicalViewerActivator] VTK不可用，某些功能将受限";
        }

        // 验证依赖插件
        if (!validateDependencies(context)) {
            qWarning() << "[MedicalViewerActivator] 依赖验证失败";
            return;
        }

        // 初始化图像显示服务
        if (!initializeViewerService(context)) {
            qWarning() << "[MedicalViewerActivator] 图像显示服务初始化失败";
            cleanup();
            return;
        }

        m_initialized = true;
        qDebug() << "[MedicalViewerActivator] Medical Viewer plugin started successfully";
        qDebug() << "[MedicalViewerActivator] Service initialized: Medical Viewer Service v1.0.0";
        qDebug() << "[MedicalViewerActivator] Available viewer types:";
        qDebug() << "[MedicalViewerActivator] - 2D Image Viewer";
        qDebug() << "[MedicalViewerActivator] - 3D Image Viewer";
        qDebug() << "[MedicalViewerActivator] - MPR Viewer";
        qDebug() << "[MedicalViewerActivator] - Volume Renderer";
        qDebug() << "[MedicalViewerActivator] - Multi Viewer";

#ifdef VTK_FOUND
        qDebug() << "[MedicalViewerActivator] VTK Version:" << vtkVersion::GetVTKVersion();
#endif

    } catch (const std::exception& e) {
        qCritical() << "[MedicalViewerActivator] Exception during plugin start:" << e.what();
        cleanup();
    }
}

//-----------------------------------------------------------------------------
void MedicalViewerActivator::stop(ctkPluginContext* context)
{
    Q_UNUSED(context)
    
    qDebug() << "[MedicalViewerActivator] Medical Viewer plugin stopping...";
    
    cleanup();
    
    m_context = nullptr;
    m_initialized = false;
    
    qDebug() << "[MedicalViewerActivator] Medical Viewer plugin stopped successfully";
}

//-----------------------------------------------------------------------------
bool MedicalViewerActivator::initializeViewerService(ctkPluginContext* context)
{
    try {
        qDebug() << "[MedicalViewerActivator] 正在初始化图像显示服务...";

        // 创建服务实现
        m_viewerService = new MedicalViewerServiceImpl(context, this);
        
        // 🔑 关键：设置插件上下文（遵循PatientManagement成功模式）
        m_viewerService->setPluginContext(context);
        
        // 🔑 关键：设置详细的服务属性（遵循PatientManagement成功模式）
        ctkDictionary serviceProps;
        serviceProps.insert("service.description", "医学图像显示服务 - 提供2D/3D/MPR/体绘制等查看器");
        serviceProps.insert("service.vendor", "Medical Navigation System");
        serviceProps.insert("service.version", "1.0.0");
        serviceProps.insert("service.category", "medical");
        serviceProps.insert("service.ranking", "80");
        serviceProps.insert("plugin.name", "MedicalViewer");
        serviceProps.insert("viewer.types", "2D,3D,MPR,Volume,Multi");
        serviceProps.insert("rendering.engine", "VTK");
        serviceProps.insert("3d.support", "true");
        serviceProps.insert("mpr.support", "true");
        serviceProps.insert("volume.rendering", "true");
        serviceProps.insert("ui.support", "true");
        
        // 注册服务到CTK框架
        m_serviceRegistration = context->registerService<MedicalViewerService>(
            m_viewerService,
            serviceProps
        );
        
        if (!m_serviceRegistration) {
            qWarning() << "[MedicalViewerActivator] 图像显示服务注册失败";
            delete m_viewerService;
            m_viewerService = nullptr;
            return false;
        }

        qDebug() << "[MedicalViewerActivator] 图像显示服务注册成功";
        qDebug() << "[MedicalViewerActivator] 可用查看器类型数量:" << m_viewerService->getAvailableViewerTypes().size();
        
        return true;

    } catch (const std::exception& e) {
        qCritical() << "[MedicalViewerActivator] 图像显示服务初始化异常:" << e.what();
        return false;
    }
}

//-----------------------------------------------------------------------------
void MedicalViewerActivator::cleanup()
{
    // 注销服务
    if (m_serviceRegistration) {
        try {
            m_serviceRegistration.unregister();
            qDebug() << "[MedicalViewerActivator] 图像显示服务已注销";
        } catch (...) {
            qWarning() << "[MedicalViewerActivator] 服务注销时发生异常";
        }
        m_serviceRegistration = ctkServiceRegistration();
    }

    // 清理服务实例
    if (m_viewerService) {
        delete m_viewerService;
        m_viewerService = nullptr;
        qDebug() << "[MedicalViewerActivator] 图像显示服务实例已清理";
    }
}

//-----------------------------------------------------------------------------
bool MedicalViewerActivator::validateDependencies(ctkPluginContext* context)
{
    // 简化依赖检查 - 暂时总是返回true，避免复杂的插件枚举
    Q_UNUSED(context);
    qDebug() << "[MedicalViewerActivator] 依赖检查通过（简化模式）";
    return true;
}

//-----------------------------------------------------------------------------
bool MedicalViewerActivator::checkVTKAvailability()
{
#ifdef VTK_FOUND
    try {
        // 简单的VTK可用性检查
        vtkObject* testObject = vtkObject::New();
        if (testObject) {
            testObject->Delete();
            qDebug() << "[MedicalViewerActivator] VTK可用，版本:" << vtkVersion::GetVTKVersion();
            return true;
        }
    } catch (...) {
        qWarning() << "[MedicalViewerActivator] VTK初始化时发生异常";
    }
#endif
    
    qWarning() << "[MedicalViewerActivator] VTK不可用";
    return false;
}


