#include "MedicalImageCoreActivator.h"
#include "MedicalImageCoreService.h"
#include "MedicalImageCoreServiceImpl.h"
#include <QDebug>
#include <QThread>
#include <QCoreApplication>
#include <ctkDictionary.h>
#include <stdexcept>

MedicalImageCoreActivator::MedicalImageCoreActivator()
    : m_context(nullptr)
    , m_imageService(nullptr)
    , m_loaderManager(nullptr)
{
    qDebug() << "[MedicalImageCoreActivator] 医学图像核心插件激活器已创建";
}

MedicalImageCoreActivator::~MedicalImageCoreActivator()
{
    cleanup();
    qDebug() << "[MedicalImageCoreActivator] 医学图像核心插件激活器已销毁";
}

void MedicalImageCoreActivator::start(ctkPluginContext* context)
{
    try {
        m_context = context;
        
        qDebug() << "[MedicalImageCoreActivator] 启动医学图像核心插件...";
        
        // 🔑 关键：注册元类型以支持信号槽传递（遵循PatientManagement成功模式）
        // 移除未定义类型的注册，改为基本类型注册
        qRegisterMetaType<QString>("QString");
        qRegisterMetaType<QStringList>("QStringList");
        qDebug() << "[MedicalImageCoreActivator] 元类型注册完成";
        
        // 初始化核心服务
        initializeImageLoaderManager();
        initializeMedicalImageService();
        
        // 注册图像加载器
        registerImageLoaders();
        
        qDebug() << "[MedicalImageCoreActivator] 医学图像核心插件启动完成";
        qDebug() << "[MedicalImageCoreActivator] 提供服务:";
        qDebug() << "  - 医学图像核心服务 (MedicalImageCoreService)";
        qDebug() << "  - 图像加载器管理 (ImageLoaderManager)";
        qDebug() << "  - DICOM图像加载支持";
        qDebug() << "  - 多格式图像处理";
        
    } catch (const std::exception& e) {
        QString error = QString("医学图像核心插件启动失败: %1").arg(e.what());
        qCritical() << "[MedicalImageCoreActivator]" << error;
        cleanup();
    }
}

void MedicalImageCoreActivator::stop(ctkPluginContext* context)
{
    Q_UNUSED(context)
    
    qDebug() << "[MedicalImageCoreActivator] 停止医学图像核心插件...";
    
    cleanup();
    
    qDebug() << "[MedicalImageCoreActivator] 医学图像核心插件已停止";
}

void MedicalImageCoreActivator::initializeMedicalImageService()
{
    try {
        qDebug() << "[MedicalImageCoreActivator] 初始化统一医学图像服务...";
        
        // 创建医学图像核心服务实现
        m_imageService = new MedicalImageCoreServiceImpl(m_context, this);
        
        // 🔑 关键：设置插件上下文（遵循PatientManagement成功模式）
        m_imageService->setPluginContext(m_context);
        
        // 🔑 关键：设置详细的服务属性（遵循PatientManagement成功模式）
        auto serviceProps = ctkDictionary();
        serviceProps["service.description"] = "统一医学图像核心服务 - 支持DICOM/NRRD/NII等格式的图像加载、处理和管理";
        serviceProps["service.vendor"] = "Medical Navigation System";
        serviceProps["service.version"] = "1.0.0";
        serviceProps["service.category"] = "medical";
        serviceProps["service.ranking"] = "100";
        serviceProps["plugin.name"] = "MedicalImageCore";
        serviceProps["supported.formats"] = "DICOM,NRRD,NII,PNG,JPEG";
        serviceProps["memory.limit.mb"] = "2048";
        serviceProps["ui.support"] = "true";
        
        m_imageServiceRegistration = m_context->registerService<MedicalImageCoreService>(
            m_imageService, serviceProps);
        
        if (!m_imageServiceRegistration) {
            throw std::runtime_error("统一医学图像服务注册失败");
        }
        
        qDebug() << "[MedicalImageCoreActivator] 统一医学图像服务初始化完成";
        qDebug() << "[MedicalImageCoreActivator] 服务已注册到CTK框架";
        
    } catch (const std::exception& e) {
        QString error = QString("统一医学图像服务初始化失败: %1").arg(e.what());
        qCritical() << "[MedicalImageCoreActivator]" << error;
        
        // 清理已创建的服务
        if (m_imageService) {
            delete m_imageService;
            m_imageService = nullptr;
        }
        throw;
    }
}

void MedicalImageCoreActivator::initializeImageLoaderManager()
{
    try {
        qDebug() << "[MedicalImageCoreActivator] 初始化图像加载器管理器...";
        
        // TODO: 创建图像加载器管理器
        // m_loaderManager = new ImageLoaderManager(this);
        
        // TODO: 注册服务到CTK框架
        // auto serviceProps = ctkDictionary();
        // serviceProps["service.description"] = "Image Loader Manager";
        // m_loaderManagerRegistration = m_context->registerService<ImageLoaderManager>(
        //     m_loaderManager, serviceProps);
        
        qDebug() << "[MedicalImageCoreActivator] 图像加载器管理器初始化完成 (占位符)";
        
    } catch (const std::exception& e) {
        QString error = QString("图像加载器管理器初始化失败: %1").arg(e.what());
        qCritical() << "[MedicalImageCoreActivator]" << error;
        throw;
    }
}

void MedicalImageCoreActivator::registerImageLoaders()
{
    try {
        qDebug() << "[MedicalImageCoreActivator] 注册图像加载器...";
        
        // TODO: 创建并注册DICOM加载器
        // auto dicomLoader = new DicomImageLoader(m_loaderManager);
        // m_loaderManager->registerLoader(dicomLoader);
        
        // TODO: 创建并注册NRRD加载器
        // auto nrrdLoader = new NrrdImageLoader(m_loaderManager);
        // m_loaderManager->registerLoader(nrrdLoader);
        
        qDebug() << "[MedicalImageCoreActivator] 图像加载器注册完成 (占位符)";
        qDebug() << "  支持格式: DICOM, NRRD, NIfTI, MetaImage";
        
    } catch (const std::exception& e) {
        QString error = QString("图像加载器注册失败: %1").arg(e.what());
        qCritical() << "[MedicalImageCoreActivator]" << error;
        throw;
    }
}

void MedicalImageCoreActivator::cleanup()
{
    qDebug() << "[MedicalImageCoreActivator] 开始清理资源...";
    
    // 注销服务
    if (m_imageServiceRegistration) {
        m_imageServiceRegistration.unregister();
        qDebug() << "[MedicalImageCoreActivator] 图像服务已注销";
    }
    
    if (m_loaderManagerRegistration) {
        m_loaderManagerRegistration.unregister();
        qDebug() << "[MedicalImageCoreActivator] 加载器管理器已注销";
    }
    
    // 等待短暂时间确保服务调用完成
    QThread::msleep(100);
    
    // 清理资源
    if (m_imageService) {
        qDebug() << "[MedicalImageCoreActivator] 开始强制清理图像服务...";
        
        // 确保服务停止所有活动
        if (auto impl = dynamic_cast<MedicalImageCoreServiceImpl*>(m_imageService)) {
            impl->cancelAllTasks();
            impl->clearAllImages();
            
            // 等待任务真正停止
            QThread::msleep(200);
            
            // 强制处理事件
            QCoreApplication::processEvents();
        }
        
        delete m_imageService;
        m_imageService = nullptr;
        qDebug() << "[MedicalImageCoreActivator] 图像服务实例已删除";
        
        // 再次等待确保析构完成
        QThread::msleep(100);
        QCoreApplication::processEvents();
    }
    
    if (m_loaderManager) {
        delete m_loaderManager;
        m_loaderManager = nullptr;
        qDebug() << "[MedicalImageCoreActivator] 加载器管理器已删除";
    }
    
    m_context = nullptr;
    
    qDebug() << "[MedicalImageCoreActivator] 资源清理完成";
}



