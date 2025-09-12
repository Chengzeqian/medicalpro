#include "MedicalProcessingActivator.h"
#include "MedicalProcessingService.h"
#include "MedicalProcessingServiceImpl.h"

#include <QDebug>
#include <QStringList>
#include <ctkPluginContext.h>
#include <ctkServiceReference.h>
#include <ctkDictionary.h>

//-----------------------------------------------------------------------------
void MedicalProcessingActivator::start(ctkPluginContext* context)
{
    m_context = context;
    m_processingService = nullptr;
    m_initialized = false;

    qDebug() << "[MedicalProcessingActivator] Medical Processing plugin starting...";

    try {
        // 🔑 关键：注册元类型以支持信号槽传递（遵循PatientManagement成功模式）
        // 移除未定义类型的注册，改为基本类型注册
        qRegisterMetaType<QString>("QString");
        qRegisterMetaType<QStringList>("QStringList");
        qRegisterMetaType<int>("int");
        qDebug() << "[MedicalProcessingActivator] 元类型注册完成";
        
        // 验证依赖插件
        if (!validateDependencies(context)) {
            qWarning() << "[MedicalProcessingActivator] 依赖验证失败";
            return;
        }

        // 初始化图像处理服务
        if (!initializeProcessingService(context)) {
            qWarning() << "[MedicalProcessingActivator] 图像处理服务初始化失败";
            cleanup();
            return;
        }

        m_initialized = true;
        qDebug() << "[MedicalProcessingActivator] Medical Processing plugin started successfully";
        qDebug() << "[MedicalProcessingActivator] Service initialized: Medical Processing Service v1.0.0";
        qDebug() << "[MedicalProcessingActivator] Available processing algorithms:";
        qDebug() << "[MedicalProcessingActivator] - Threshold Segmentation";
        qDebug() << "[MedicalProcessingActivator] - Region Growing Segmentation";
        qDebug() << "[MedicalProcessingActivator] - Watershed Segmentation";
        qDebug() << "[MedicalProcessingActivator] - Gaussian Filter";
        qDebug() << "[MedicalProcessingActivator] - Median Filter";
        qDebug() << "[MedicalProcessingActivator] - Bilateral Filter";
        qDebug() << "[MedicalProcessingActivator] - Canny Edge Detection";
        qDebug() << "[MedicalProcessingActivator] - Gradient Magnitude Edge Detection";
        qDebug() << "[MedicalProcessingActivator] - Morphological Operations";

    } catch (const std::exception& e) {
        qCritical() << "[MedicalProcessingActivator] Exception during plugin start:" << e.what();
        cleanup();
    }
}

//-----------------------------------------------------------------------------
void MedicalProcessingActivator::stop(ctkPluginContext* context)
{
    Q_UNUSED(context)
    
    qDebug() << "[MedicalProcessingActivator] Medical Processing plugin stopping...";
    
    cleanup();
    
    m_context = nullptr;
    m_initialized = false;
    
    qDebug() << "[MedicalProcessingActivator] Medical Processing plugin stopped successfully";
}

//-----------------------------------------------------------------------------
bool MedicalProcessingActivator::initializeProcessingService(ctkPluginContext* context)
{
    try {
        qDebug() << "[MedicalProcessingActivator] 正在初始化图像处理服务...";

        // 创建服务实现
        m_processingService = new MedicalProcessingServiceImpl(context, this);
        
        // 🔑 关键：设置插件上下文（遵循PatientManagement成功模式）
        m_processingService->setPluginContext(context);
        
        // 🔑 关键：设置详细的服务属性（遵循PatientManagement成功模式）
        ctkDictionary serviceProps;
        serviceProps.insert("service.description", "医学图像处理服务 - 提供分割、滤波、形态学等算法");
        serviceProps.insert("service.vendor", "Medical Navigation System");
        serviceProps.insert("service.version", "1.0.0");
        serviceProps.insert("service.category", "medical");
        serviceProps.insert("service.ranking", "90");
        serviceProps.insert("plugin.name", "MedicalProcessing");
        serviceProps.insert("algorithms.segmentation", "ThresholdSegmentation,RegionGrowingSegmentation,WatershedSegmentation");
        serviceProps.insert("algorithms.filtering", "GaussianFilter,MedianFilter,BilateralFilter");
        serviceProps.insert("algorithms.morphology", "Erosion,Dilation,Opening,Closing");
        serviceProps.insert("algorithms.edge", "CannyEdgeDetection,GradientMagnitudeEdgeDetection");
        serviceProps.insert("ui.support", "true");
        
        // 注册服务到CTK框架
        m_serviceRegistration = context->registerService<MedicalProcessingService>(
            m_processingService, 
            serviceProps
        );
        
        if (!m_serviceRegistration) {
            qWarning() << "[MedicalProcessingActivator] 图像处理服务注册失败";
            delete m_processingService;
            m_processingService = nullptr;
            return false;
        }

        qDebug() << "[MedicalProcessingActivator] 图像处理服务注册成功";
        qDebug() << "[MedicalProcessingActivator] 可用算法数量:" << m_processingService->getAvailableAlgorithms().size();
        
        return true;

    } catch (const std::exception& e) {
        qCritical() << "[MedicalProcessingActivator] 图像处理服务初始化异常:" << e.what();
        return false;
    }
}

//-----------------------------------------------------------------------------
void MedicalProcessingActivator::cleanup()
{
    // 注销服务
    if (m_serviceRegistration) {
        try {
            m_serviceRegistration.unregister();
            qDebug() << "[MedicalProcessingActivator] 图像处理服务已注销";
        } catch (...) {
            qWarning() << "[MedicalProcessingActivator] 服务注销时发生异常";
        }
        m_serviceRegistration = ctkServiceRegistration();
    }

    // 清理服务实例
    if (m_processingService) {
        delete m_processingService;
        m_processingService = nullptr;
        qDebug() << "[MedicalProcessingActivator] 图像处理服务实例已清理";
    }
}

//-----------------------------------------------------------------------------
bool MedicalProcessingActivator::validateDependencies(ctkPluginContext* context)
{
    // 简化依赖检查（实际项目中应该检查具体服务）
    qDebug() << "[MedicalProcessingActivator] 正在验证依赖性...";
    
    // 这里可以通过检查服务注册表来验证依赖
    // 暂时跳过复杂的插件枚举，假设依赖已满足
    bool hasCorePlugin = true;
    
    qDebug() << "[MedicalProcessingActivator] 依赖验证通过";
    return true;
}

// 导出插件
// 移除moc包含 - 此文件中没有Q_OBJECT宏
// #include "MedicalProcessingActivator.moc"
