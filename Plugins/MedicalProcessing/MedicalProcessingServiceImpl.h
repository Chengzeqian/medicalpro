#ifndef MEDICAL_PROCESSING_SERVICE_IMPL_H
#define MEDICAL_PROCESSING_SERVICE_IMPL_H

#include "MedicalProcessingService.h"
#include <QObject>
#include <QMutex>
#include <QAtomicInt>
#include <QMap>
#include <QVariant>
#include <QUuid>
#include <QTimer>

// CTK框架
#include <ctkPluginContext.h>
#include <ctkServiceReference.h>

// 前向声明（遵循完全CTK架构）
// 使用QObject*以符合CTK架构原则

/**
 * @brief Medical Processing Service Implementation (完全CTK架构)
 * 
 * MedicalProcessingService接口的具体实现，采用完全CTK架构设计：
 * - 通过CTK服务框架获取MedicalImageCoreService
 * - 所有图像操作通过服务接口完成
 * - 不直接依赖MedicalImageData类
 * - 支持异步处理和进度报告
 * - 完全解耦的插件间通信
 */
class MedicalProcessingServiceImpl : public MedicalProcessingService
{
    Q_OBJECT
    Q_INTERFACES(MedicalProcessingService)

public:
    /**
     * @brief 构造函数
     * @param context CTK插件上下文
     * @param parent 父对象
     */
    explicit MedicalProcessingServiceImpl(ctkPluginContext* context, QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~MedicalProcessingServiceImpl() override;
    
    /**
     * @brief 设置CTK插件上下文（关键方法，遵循PatientManagement成功模式）
     * @param context CTK插件上下文
     */
    void setPluginContext(ctkPluginContext* context);

    // ==================== 图像分割算法实现 ====================
    
    QString thresholdSegmentation(
        const QString& imageId,
        double lowerThreshold,
        double upperThreshold) override;
    
    QString regionGrowingSegmentation(
        const QString& imageId,
        const QString& seedPoints,
        double tolerance) override;
    
    QString watershedSegmentation(
        const QString& imageId,
        const QString& markersImageId = QString()) override;

    // ==================== 图像滤波和增强实现 ====================
    
    QString gaussianFilter(
        const QString& imageId,
        double sigma) override;
    
    QString medianFilter(
        const QString& imageId,
        int radius) override;
    
    QString bilateralFilter(
        const QString& imageId,
        double domainSigma,
        double rangeSigma) override;

    // ==================== 边缘检测实现 ====================
    
    QString cannyEdgeDetection(
        const QString& imageId,
        double lowerThreshold,
        double upperThreshold) override;
    
    QString gradientMagnitudeEdgeDetection(
        const QString& imageId,
        double sigma) override;

    // ==================== 形态学操作实现 ====================
    
    QString morphologicalErosion(
        const QString& imageId,
        int structuringElementRadius) override;
    
    QString morphologicalDilation(
        const QString& imageId,
        int structuringElementRadius) override;
    
    QString morphologicalOpening(
        const QString& imageId,
        int structuringElementRadius) override;
    
    QString morphologicalClosing(
        const QString& imageId,
        int structuringElementRadius) override;

    // ==================== 批处理和高级功能实现 ====================
    
    QStringList batchProcess(
        const QStringList& imageIds,
        const QString& operation,
        const QVariantMap& parameters) override;
    
    QStringList getSupportedOperations() const override;
    
    QStringList getAvailableAlgorithms() const override;
    
    QVariantMap getDefaultParameters(const QString& operation) const override;
    
    void cancelCurrentOperation() override;
    
    QString getProcessingStatus() const override;
    
    QString getLastError() const override;

    // ==================== UI显示管理（遵循PatientManagement成功模式） ====================
    
    bool showProcessingDialog(QWidget* parent = nullptr) override;
    bool showBatchProcessingDialog(QWidget* parent = nullptr) override;
    bool showAlgorithmConfigDialog(QWidget* parent = nullptr) override;

private slots:
    /**
     * @brief 处理异步操作超时
     */
    void onOperationTimeout();
    
    /**
     * @brief 处理服务可用性变化
     * @param available 服务是否可用
     */
    void onImageServiceAvailabilityChanged(bool available);

private:
    /**
     * @brief 初始化图像服务连接
     */
    void initializeImageServiceConnection();
    
    /**
     * @brief 初始化支持的操作列表
     */
    void initializeSupportedOperations();
    
    /**
     * @brief 验证图像ID有效性
     * @param imageId 图像ID
     * @return 是否有效
     */
    bool validateImageId(const QString& imageId) const;
    
    /**
     * @brief 生成操作ID
     * @return 唯一操作ID
     */
    QString generateOperationId() const;
    
    /**
     * @brief 设置错误信息
     * @param error 错误描述
     */
    void setError(const QString& error);
    
    /**
     * @brief 执行通用图像处理操作
     * @param imageId 输入图像ID
     * @param operation 操作名称
     * @param parameters 操作参数
     * @return 结果图像ID
     */
    QString executeImageOperation(
        const QString& imageId,
        const QString& operation,
        const QVariantMap& parameters);
    
    /**
     * @brief 更新处理进度
     * @param operationId 操作ID
     * @param progress 进度值
     */
    void updateProgress(const QString& operationId, int progress);
    
    /**
     * @brief 检查操作是否被取消
     * @param operationId 操作ID
     * @return 是否被取消
     */
    bool isOperationCancelled(const QString& operationId) const;
    
    /**
     * @brief 执行具体的图像处理算法
     * @param imageId 输入图像ID
     * @param operation 操作名称
     * @param parameters 操作参数
     * @return 处理结果图像ID
     */
    QString performImageProcessing(const QString& imageId, const QString& operation, const QVariantMap& parameters);
    
    // 具体图像处理算法方法
    bool performThresholdSegmentation(const QString& inputId, const QString& outputId, const QVariantMap& parameters);
    bool performBinaryThreshold(const QString& inputId, const QString& outputId, const QVariantMap& parameters);
    bool performOtsuThreshold(const QString& inputId, const QString& outputId, const QVariantMap& parameters);
    bool performRegionGrowing(const QString& inputId, const QString& outputId, const QVariantMap& parameters);
    bool performWatershedSegmentation(const QString& inputId, const QString& outputId, const QVariantMap& parameters);
    bool performGaussianFilter(const QString& inputId, const QString& outputId, const QVariantMap& parameters);
    bool performMedianFilter(const QString& inputId, const QString& outputId, const QVariantMap& parameters);
    bool performBilateralFilter(const QString& inputId, const QString& outputId, const QVariantMap& parameters);
    bool performCannyEdgeDetection(const QString& inputId, const QString& outputId, const QVariantMap& parameters);
    bool performGradientMagnitudeEdgeDetection(const QString& inputId, const QString& outputId, const QVariantMap& parameters);
    bool performMorphologicalErosion(const QString& inputId, const QString& outputId, const QVariantMap& parameters);
    bool performMorphologicalDilation(const QString& inputId, const QString& outputId, const QVariantMap& parameters);
    bool performMorphologicalOpening(const QString& inputId, const QString& outputId, const QVariantMap& parameters);
    bool performMorphologicalClosing(const QString& inputId, const QString& outputId, const QVariantMap& parameters);
    
    /**
     * @brief 创建处理后的图像
     * @param inputId 输入图像ID
     * @param outputId 输出图像ID
     * @param algorithm 算法名称
     * @param metadata 元数据
     * @return 是否创建成功
     */
    bool createProcessedImage(const QString& inputId, const QString& outputId, const QString& algorithm, const QVariantMap& metadata);

    /**
     * @brief 对浮点数据执行阈值分割
     * @param data 图像数据指针
     * @param totalPixels 总像素数
     * @param lowerThreshold 下阈值
     * @param upperThreshold 上阈值
     * @param inputId 输入图像ID
     * @param outputId 输出图像ID
     * @return 是否成功
     */
    bool performFloatThresholdSegmentation(float* data, qint64 totalPixels, double lowerThreshold, double upperThreshold, const QString& inputId, const QString& outputId);

    /**
     * @brief 对整数数据执行阈值分割
     * @param data 图像数据指针
     * @param totalPixels 总像素数
     * @param lowerThreshold 下阈值
     * @param upperThreshold 上阈值
     * @param inputId 输入图像ID
     * @param outputId 输出图像ID
     * @return 是否成功
     */
    bool performIntThresholdSegmentation(short* data, qint64 totalPixels, double lowerThreshold, double upperThreshold, const QString& inputId, const QString& outputId);

private:
    /// CTK插件上下文
    ctkPluginContext* m_pluginContext;
    
    /// 医学图像服务引用（CTK服务框架）
    ctkServiceReference m_imageServiceRef;
    QObject* m_imageService;
    
    /// 当前操作状态
    QString m_currentOperationId;
    QString m_currentOperation;
    QAtomicInt m_operationCancelled;
    QString m_processingStatus;
    
    /// 错误信息
    QString m_lastError;
    
    /// 线程安全
    mutable QMutex m_mutex;
    
    /// 操作超时定时器
    QTimer* m_operationTimer;
    
    /// 活动操作映射
    QMap<QString, QVariantMap> m_activeOperations;
    
    /// 服务连接状态
    bool m_serviceConnected;
    
    /// 组件初始化状态
    bool m_componentsInitialized;
    
    /// 支持的操作列表
    QStringList m_supportedOperations;
    
    /// 操作默认参数
    QMap<QString, QVariantMap> m_defaultParameters;
};

#endif // MEDICAL_PROCESSING_SERVICE_IMPL_H