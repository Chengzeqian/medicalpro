#ifndef MEDICAL_IMAGE_CORE_ACTIVATOR_H
#define MEDICAL_IMAGE_CORE_ACTIVATOR_H

#include <ctkPluginActivator.h>
#include <ctkPluginContext.h>
#include <ctkServiceRegistration.h>

class MedicalImageCoreServiceImpl;
class ImageLoaderManager;

/**
 * @brief MedicalImageCore插件激活器
 * 
 * 负责启动和停止医学图像核心服务，包括：
 * - 统一医学图像服务
 * - 图像加载器管理
 * - DICOM/NRRD/其他格式支持
 */
class MedicalImageCoreActivator : public QObject, public ctkPluginActivator
{
    Q_OBJECT
    Q_INTERFACES(ctkPluginActivator)
    Q_PLUGIN_METADATA(IID "medical.imagecore")

public:
    MedicalImageCoreActivator();
    ~MedicalImageCoreActivator() override;

    /**
     * @brief 启动插件
     * @param context CTK插件上下文
     */
    void start(ctkPluginContext* context) override;

    /**
     * @brief 停止插件
     * @param context CTK插件上下文
     */
    void stop(ctkPluginContext* context) override;

private:
    /**
     * @brief 初始化医学图像服务
     */
    void initializeMedicalImageService();
    
    /**
     * @brief 初始化图像加载器管理器
     */
    void initializeImageLoaderManager();
    
    /**
     * @brief 注册图像加载器
     */
    void registerImageLoaders();
    
    /**
     * @brief 清理资源
     */
    void cleanup();

private:
    // CTK上下文
    ctkPluginContext* m_context;
    
    // 核心服务实例
    MedicalImageCoreServiceImpl* m_imageService;
    ImageLoaderManager* m_loaderManager;
    
    // 服务注册
    ctkServiceRegistration m_imageServiceRegistration;
    ctkServiceRegistration m_loaderManagerRegistration;
};

#endif // MEDICAL_IMAGE_CORE_ACTIVATOR_H
