#ifndef MEDICAL_PROCESSING_SERVICE_H
#define MEDICAL_PROCESSING_SERVICE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QVariant>
#include <QMetaType>

/**
 * @brief Medical Processing Service Interface (完全CTK架构)
 * 
 * 提供医学图像处理的标准接口，采用完全CTK架构设计：
 * - 通过图像ID引用图像数据
 * - 所有操作通过CTK服务接口完成
 * - 不直接依赖MedicalImageData类
 * - 支持异步处理和进度报告
 * 
 * 核心设计原则：
 * 1. 输入：图像ID (QString)
 * 2. 输出：处理后的图像ID (QString)
 * 3. 参数：处理参数 (QVariantMap)
 * 4. 通信：完全通过CTK服务框架
 */
class MedicalProcessingService : public QObject
{
    Q_OBJECT

public:
    explicit MedicalProcessingService(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~MedicalProcessingService() = default;

    // ==================== 图像分割算法 ====================
    
    /**
     * @brief 阈值分割
     * @param imageId 输入图像ID
     * @param lowerThreshold 下阈值
     * @param upperThreshold 上阈值
     * @return 分割结果图像ID
     */
    virtual QString thresholdSegmentation(
        const QString& imageId,
        double lowerThreshold,
        double upperThreshold) = 0;
    
    /**
     * @brief 区域生长分割
     * @param imageId 输入图像ID
     * @param seedPoints 种子点坐标列表 (格式："x1,y1,z1;x2,y2,z2;...")
     * @param tolerance 容忍度
     * @return 分割结果图像ID
     */
    virtual QString regionGrowingSegmentation(
        const QString& imageId,
        const QString& seedPoints,
        double tolerance) = 0;
    
    /**
     * @brief 分水岭分割
     * @param imageId 输入图像ID
     * @param markersImageId 标记图像ID (可选)
     * @return 分割结果图像ID
     */
    virtual QString watershedSegmentation(
        const QString& imageId,
        const QString& markersImageId = QString()) = 0;

    // ==================== 图像滤波和增强 ====================
    
    /**
     * @brief 高斯滤波
     * @param imageId 输入图像ID
     * @param sigma 标准差
     * @return 滤波结果图像ID
     */
    virtual QString gaussianFilter(
        const QString& imageId,
        double sigma) = 0;
    
    /**
     * @brief 中值滤波
     * @param imageId 输入图像ID
     * @param radius 滤波半径
     * @return 滤波结果图像ID
     */
    virtual QString medianFilter(
        const QString& imageId,
        int radius) = 0;
    
    /**
     * @brief 双边滤波
     * @param imageId 输入图像ID
     * @param domainSigma 空间域标准差
     * @param rangeSigma 灰度域标准差
     * @return 滤波结果图像ID
     */
    virtual QString bilateralFilter(
        const QString& imageId,
        double domainSigma,
        double rangeSigma) = 0;

    // ==================== 边缘检测 ====================
    
    /**
     * @brief Canny边缘检测
     * @param imageId 输入图像ID
     * @param lowerThreshold 低阈值
     * @param upperThreshold 高阈值
     * @return 边缘检测结果图像ID
     */
    virtual QString cannyEdgeDetection(
        const QString& imageId,
        double lowerThreshold,
        double upperThreshold) = 0;
    
    /**
     * @brief 梯度幅度边缘检测
     * @param imageId 输入图像ID
     * @param sigma 高斯滤波标准差
     * @return 边缘检测结果图像ID
     */
    virtual QString gradientMagnitudeEdgeDetection(
        const QString& imageId,
        double sigma) = 0;

    // ==================== 形态学操作 ====================
    
    /**
     * @brief 形态学腐蚀
     * @param imageId 输入图像ID
     * @param structuringElementRadius 结构元素半径
     * @return 腐蚀结果图像ID
     */
    virtual QString morphologicalErosion(
        const QString& imageId,
        int structuringElementRadius) = 0;
    
    /**
     * @brief 形态学膨胀
     * @param imageId 输入图像ID
     * @param structuringElementRadius 结构元素半径
     * @return 膨胀结果图像ID
     */
    virtual QString morphologicalDilation(
        const QString& imageId,
        int structuringElementRadius) = 0;
    
    /**
     * @brief 形态学开运算
     * @param imageId 输入图像ID
     * @param structuringElementRadius 结构元素半径
     * @return 开运算结果图像ID
     */
    virtual QString morphologicalOpening(
        const QString& imageId,
        int structuringElementRadius) = 0;
    
    /**
     * @brief 形态学闭运算
     * @param imageId 输入图像ID
     * @param structuringElementRadius 结构元素半径
     * @return 闭运算结果图像ID
     */
    virtual QString morphologicalClosing(
        const QString& imageId,
        int structuringElementRadius) = 0;

    // ==================== 批处理和高级功能 ====================
    
    /**
     * @brief 批量处理
     * @param imageIds 输入图像ID列表
     * @param operation 操作名称
     * @param parameters 操作参数
     * @return 处理结果图像ID列表
     */
    virtual QStringList batchProcess(
        const QStringList& imageIds,
        const QString& operation,
        const QVariantMap& parameters) = 0;
    
    /**
     * @brief 获取支持的处理操作列表
     * @return 操作名称列表
     */
    virtual QStringList getSupportedOperations() const = 0;
    
    /**
     * @brief 获取可用算法列表
     * @return 可用算法名称列表
     */
    virtual QStringList getAvailableAlgorithms() const = 0;
    
    /**
     * @brief 获取操作的默认参数
     * @param operation 操作名称
     * @return 默认参数映射
     */
    virtual QVariantMap getDefaultParameters(const QString& operation) const = 0;
    
    /**
     * @brief 取消当前处理操作
     */
    virtual void cancelCurrentOperation() = 0;
    
    /**
     * @brief 获取处理状态
     * @return 状态信息
     */
    virtual QString getProcessingStatus() const = 0;
    
    /**
     * @brief 获取最后的错误信息
     * @return 错误信息
     */
    virtual QString getLastError() const = 0;

    // ==================== UI显示管理（遵循PatientManagement成功模式） ====================
    
    /**
     * @brief 显示图像处理界面
     * @param parent 父窗口（可选）
     * @return 成功返回true，失败返回false
     */
    virtual bool showProcessingDialog(QWidget* parent = nullptr) = 0;
    
    /**
     * @brief 显示批量处理界面
     * @param parent 父窗口（可选）
     * @return 成功返回true，失败返回false
     */
    virtual bool showBatchProcessingDialog(QWidget* parent = nullptr) = 0;
    
    /**
     * @brief 显示算法配置界面
     * @param parent 父窗口（可选）
     * @return 成功返回true，失败返回false
     */
    virtual bool showAlgorithmConfigDialog(QWidget* parent = nullptr) = 0;

signals:
    /**
     * @brief 处理开始信号
     * @param operationId 操作ID
     * @param operation 操作名称
     * @param imageId 图像ID
     */
    void processingStarted(const QString& operationId, const QString& operation, const QString& imageId);
    
    /**
     * @brief 处理进度信号
     * @param operationId 操作ID
     * @param progress 进度百分比 (0-100)
     */
    void processingProgress(const QString& operationId, int progress);
    
    /**
     * @brief 处理完成信号
     * @param operationId 操作ID
     * @param resultImageId 结果图像ID
     */
    void processingCompleted(const QString& operationId, const QString& resultImageId);
    
    /**
     * @brief 处理失败信号
     * @param operationId 操作ID
     * @param error 错误信息
     */
    void processingFailed(const QString& operationId, const QString& error);
    
    /**
     * @brief 批量处理完成信号
     * @param operationId 操作ID
     * @param resultImageIds 结果图像ID列表
     */
    void batchProcessingCompleted(const QString& operationId, const QStringList& resultImageIds);
    
    /**
     * @brief 操作取消信号
     * @param operationId 操作ID
     */
    void operationCancelled(const QString& operationId);
};

// Qt接口声明
Q_DECLARE_INTERFACE(MedicalProcessingService, "medical.MedicalProcessingService")

#endif // MEDICAL_PROCESSING_SERVICE_H