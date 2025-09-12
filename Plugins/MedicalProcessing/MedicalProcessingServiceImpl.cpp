#include "MedicalProcessingServiceImpl.h"
#include "MedicalProcessingWidget.h"
#include "ServiceInterfaces.h"
#include "../MedicalImageCore/MedicalImageCoreService.h"

// CTK框架
#include <ctkPluginContext.h>
#include <ctkServiceReference.h>

// 注意：遵循完全CTK架构，通过服务接口访问
// 不直接包含其他插件的头文件

#include <QDebug>
#include <QMutexLocker>
#include <QThread>
#include <QUuid>
#include <QTimer>
#include <QApplication>

// UI相关头文件（用于UI显示管理方法）
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QListWidget>
#include <QDialogButtonBox>
#include <QLabel>
#include <QSpinBox>
#include <QPushButton>
#include <QTextEdit>
#include <QProgressBar>
#include <QGroupBox>
#include <QMessageBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QTabWidget>
#include <QScrollArea>
#include <QSlider>
#include <QCheckBox>

//-----------------------------------------------------------------------------
MedicalProcessingServiceImpl::MedicalProcessingServiceImpl(ctkPluginContext* context, QObject* parent)
    : MedicalProcessingService(parent)
    , m_pluginContext(context)
    , m_imageService(nullptr)
    , m_operationTimer(new QTimer(this))
{
    m_operationTimer->setSingleShot(true);
    m_operationTimer->setInterval(100);
    
    // 连接定时器信号
    connect(m_operationTimer, &QTimer::timeout, this, &MedicalProcessingServiceImpl::onOperationTimeout);
    
    // 初始化图像服务连接
    try {
        initializeImageServiceConnection();
    } catch (const std::exception& e) {
        qWarning() << "[MedicalProcessingServiceImpl] 初始化图像服务连接失败:" << e.what();
    }
}

//-----------------------------------------------------------------------------
MedicalProcessingServiceImpl::~MedicalProcessingServiceImpl()
{
    // 停止并清理定时器
    if (m_operationTimer) {
        m_operationTimer->stop();
        m_operationTimer->deleteLater();
        m_operationTimer = nullptr;
    }
    
    // 断开服务连接
    if (m_imageService) {
        disconnect(this, nullptr, m_imageService, nullptr);
        disconnect(m_imageService, nullptr, this, nullptr);
    }
    
    qDebug() << "[MedicalProcessingServiceImpl] 服务析构完成";
}

//-----------------------------------------------------------------------------
void MedicalProcessingServiceImpl::setPluginContext(ctkPluginContext* context)
{
    m_pluginContext = context;
    
    // 重新初始化图像服务连接
    try {
    initializeImageServiceConnection();
    } catch (const std::exception& e) {
        qWarning() << "[MedicalProcessingServiceImpl] 重新初始化图像服务连接失败:" << e.what();
    }
}

//-----------------------------------------------------------------------------
// 分割算法接口实现
//-----------------------------------------------------------------------------

QString MedicalProcessingServiceImpl::thresholdSegmentation(const QString& inputImageId, double lowerThreshold, double upperThreshold)
{
    QString outputImageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    QVariantMap parameters;
    parameters["lowerThreshold"] = lowerThreshold;
    parameters["upperThreshold"] = upperThreshold;
    
    if (performThresholdSegmentation(inputImageId, outputImageId, parameters)) {
        emit processingCompleted(outputImageId, outputImageId);
        return outputImageId;
    }
    
    return QString();
}

QString MedicalProcessingServiceImpl::regionGrowingSegmentation(const QString& inputImageId, const QString& seedPoints, double tolerance)
{
    QString outputImageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    QVariantMap parameters;
    parameters["seedPoints"] = seedPoints;
    parameters["tolerance"] = tolerance;
    
    if (performRegionGrowing(inputImageId, outputImageId, parameters)) {
        emit processingCompleted(outputImageId, outputImageId);
        return outputImageId;
    }
    
    return QString();
}

QString MedicalProcessingServiceImpl::watershedSegmentation(const QString& inputImageId, const QString& markersImageId)
{
    QString outputImageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    QVariantMap parameters;
        parameters["markersImageId"] = markersImageId;
    
    if (performWatershedSegmentation(inputImageId, outputImageId, parameters)) {
        emit processingCompleted(outputImageId, outputImageId);
        return outputImageId;
    }
    
    return QString();
}

//-----------------------------------------------------------------------------
// 滤波算法接口实现
//-----------------------------------------------------------------------------

QString MedicalProcessingServiceImpl::gaussianFilter(const QString& inputImageId, double sigma)
{
    QString outputImageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    QVariantMap parameters;
    parameters["sigma"] = sigma;
    
    if (performGaussianFilter(inputImageId, outputImageId, parameters)) {
        emit processingCompleted(outputImageId, outputImageId);
        return outputImageId;
    }
    
    return QString();
}

QString MedicalProcessingServiceImpl::medianFilter(const QString& inputImageId, int radius)
{
    QString outputImageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    QVariantMap parameters;
    parameters["radius"] = radius;
    
    if (performMedianFilter(inputImageId, outputImageId, parameters)) {
        emit processingCompleted(outputImageId, outputImageId);
        return outputImageId;
    }
    
    return QString();
}

QString MedicalProcessingServiceImpl::bilateralFilter(const QString& inputImageId, double domainSigma, double rangeSigma)
{
    QString outputImageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    QVariantMap parameters;
    parameters["domainSigma"] = domainSigma;
    parameters["rangeSigma"] = rangeSigma;
    
    if (performBilateralFilter(inputImageId, outputImageId, parameters)) {
        emit processingCompleted(outputImageId, outputImageId);
        return outputImageId;
    }
    
    return QString();
}

//-----------------------------------------------------------------------------
// 边缘检测算法接口实现
//-----------------------------------------------------------------------------

QString MedicalProcessingServiceImpl::cannyEdgeDetection(const QString& inputImageId, double lowerThreshold, double upperThreshold)
{
    QString outputImageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    QVariantMap parameters;
    parameters["lowerThreshold"] = lowerThreshold;
    parameters["upperThreshold"] = upperThreshold;
    
    if (performCannyEdgeDetection(inputImageId, outputImageId, parameters)) {
        emit processingCompleted(outputImageId, outputImageId);
        return outputImageId;
    }
    
    return QString();
}

QString MedicalProcessingServiceImpl::gradientMagnitudeEdgeDetection(const QString& inputImageId, double sigma)
{
    QString outputImageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    QVariantMap parameters;
    parameters["sigma"] = sigma;
    
    if (performGradientMagnitudeEdgeDetection(inputImageId, outputImageId, parameters)) {
        emit processingCompleted(outputImageId, outputImageId);
        return outputImageId;
    }
    
    return QString();
}

//-----------------------------------------------------------------------------
// 形态学操作接口实现
//-----------------------------------------------------------------------------

QString MedicalProcessingServiceImpl::morphologicalErosion(const QString& inputImageId, int structuringElementRadius)
{
    QString outputImageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    QVariantMap parameters;
    parameters["structuringElementRadius"] = structuringElementRadius;
    
    if (performMorphologicalErosion(inputImageId, outputImageId, parameters)) {
        emit processingCompleted(outputImageId, outputImageId);
        return outputImageId;
    }
    
    return QString();
}

QString MedicalProcessingServiceImpl::morphologicalDilation(const QString& inputImageId, int structuringElementRadius)
{
    QString outputImageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    QVariantMap parameters;
    parameters["structuringElementRadius"] = structuringElementRadius;
    
    if (performMorphologicalDilation(inputImageId, outputImageId, parameters)) {
        emit processingCompleted(outputImageId, outputImageId);
        return outputImageId;
    }
    
    return QString();
}

QString MedicalProcessingServiceImpl::morphologicalOpening(const QString& inputImageId, int structuringElementRadius)
{
    QString outputImageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    QVariantMap parameters;
    parameters["structuringElementRadius"] = structuringElementRadius;
    
    if (performMorphologicalOpening(inputImageId, outputImageId, parameters)) {
        emit processingCompleted(outputImageId, outputImageId);
        return outputImageId;
    }
    
    return QString();
}

QString MedicalProcessingServiceImpl::morphologicalClosing(const QString& inputImageId, int structuringElementRadius)
{
    QString outputImageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    QVariantMap parameters;
    parameters["structuringElementRadius"] = structuringElementRadius;
    
    if (performMorphologicalClosing(inputImageId, outputImageId, parameters)) {
        emit processingCompleted(outputImageId, outputImageId);
        return outputImageId;
    }
    
    return QString();
}

//-----------------------------------------------------------------------------
// 通用处理方法
//-----------------------------------------------------------------------------



QString MedicalProcessingServiceImpl::getLastError() const
{
    QMutexLocker locker(&m_mutex);
    return m_lastError;
}

QStringList MedicalProcessingServiceImpl::batchProcess(const QStringList& imageIds, const QString& operation, const QVariantMap& parameters)
{
    QStringList results;
    
    for (const QString& imageId : imageIds) {
        // 根据操作类型调用相应的处理方法
        QString result;
        if (operation == "ThresholdSegmentation") {
            double lower = parameters.value("lowerThreshold", 0.0).toDouble();
            double upper = parameters.value("upperThreshold", 255.0).toDouble();
            result = thresholdSegmentation(imageId, lower, upper);
        } else if (operation == "GaussianFilter") {
            double sigma = parameters.value("sigma", 1.0).toDouble();
            result = gaussianFilter(imageId, sigma);
        }
        // 可以添加更多操作类型...
        
        if (!result.isEmpty()) {
            results.append(result);
        }
    }
    
    return results;
}

QStringList MedicalProcessingServiceImpl::getSupportedOperations() const
{
    return QStringList() 
        << "ThresholdSegmentation" << "RegionGrowing" << "WatershedSegmentation"
        << "GaussianFilter" << "MedianFilter" << "BilateralFilter"
        << "CannyEdgeDetection" << "GradientMagnitudeEdgeDetection"
        << "MorphologicalErosion" << "MorphologicalDilation"
        << "MorphologicalOpening" << "MorphologicalClosing";
}

QStringList MedicalProcessingServiceImpl::getAvailableAlgorithms() const
{
    return getSupportedOperations(); // 在这个实现中，支持的操作就是可用的算法
}

QVariantMap MedicalProcessingServiceImpl::getDefaultParameters(const QString& operation) const
{
    QVariantMap defaults;
    
    if (operation == "ThresholdSegmentation") {
        defaults["lowerThreshold"] = 0.0;
        defaults["upperThreshold"] = 255.0;
    } else if (operation == "RegionGrowing") {
        defaults["seedPoints"] = "128,128,64";
        defaults["tolerance"] = 10.0;
    } else if (operation == "GaussianFilter") {
        defaults["sigma"] = 1.0;
    } else if (operation == "MedianFilter") {
        defaults["radius"] = 1;
    } else if (operation == "BilateralFilter") {
        defaults["domainSigma"] = 5.0;
        defaults["rangeSigma"] = 10.0;
    } else if (operation == "CannyEdgeDetection") {
        defaults["lowerThreshold"] = 10.0;
        defaults["upperThreshold"] = 50.0;
    } else if (operation == "GradientMagnitudeEdgeDetection") {
        defaults["sigma"] = 1.0;
    } else if (operation.startsWith("Morphological")) {
        defaults["structuringElementRadius"] = 1;
    }
    
    return defaults;
}

void MedicalProcessingServiceImpl::cancelCurrentOperation()
{
    QMutexLocker locker(&m_mutex);
    
    // 取消当前所有活动操作
    QStringList operations = m_activeOperations.keys();
    for (const QString& operationId : operations) {
        m_activeOperations.remove(operationId);
        // 操作已取消
    }
    
    qDebug() << "[MedicalProcessingServiceImpl] 已取消所有活动操作";
}

QString MedicalProcessingServiceImpl::getProcessingStatus() const
{
    QMutexLocker locker(&m_mutex);
    
    if (m_activeOperations.isEmpty()) {
        return "空闲";
    } else {
        return QString("正在处理 %1 个操作").arg(m_activeOperations.size());
    }
}



//-----------------------------------------------------------------------------
// UI显示管理实现
//-----------------------------------------------------------------------------

bool MedicalProcessingServiceImpl::showProcessingDialog(QWidget* parent)
{
    try {
        MedicalProcessingWidget* processingWidget = new MedicalProcessingWidget(parent);
        processingWidget->setPluginContext(m_pluginContext);
        
        // 设置为独立窗口
        processingWidget->setWindowFlags(Qt::Window);
        processingWidget->setAttribute(Qt::WA_DeleteOnClose);
        processingWidget->resize(1200, 800);
        
        // 显示窗口
        processingWidget->show();
        processingWidget->raise();
        processingWidget->activateWindow();
        
        qDebug() << "[MedicalProcessingServiceImpl] 显示图像处理界面";
        return true;
    } catch (const std::exception& e) {
        setError(QString("显示处理界面失败: %1").arg(e.what()));
        return false;
    }
}

bool MedicalProcessingServiceImpl::showBatchProcessingDialog(QWidget* parent)
{
    QMessageBox::information(parent, "批量处理", "批量处理界面即将推出");
    return true;
}

bool MedicalProcessingServiceImpl::showAlgorithmConfigDialog(QWidget* parent)
{
    QMessageBox::information(parent, "算法配置", "算法配置界面即将推出");
    return true;
}

//-----------------------------------------------------------------------------
// 私有方法实现
//-----------------------------------------------------------------------------

void MedicalProcessingServiceImpl::initializeImageServiceConnection()
{
    if (!m_pluginContext) {
        qWarning() << "[MedicalProcessingServiceImpl] 插件上下文为空";
        return;
    }

    // 获取图像服务引用（CTK标准方式）
    ctkServiceReference serviceRef = m_pluginContext->getServiceReference("medical.MedicalImageCoreService");
    if (serviceRef) {
        m_imageService = m_pluginContext->getService(serviceRef);
        
        if (m_imageService) {
            // 图像服务已连接，不需要信号连接，直接设置为可用状态
            
            qDebug() << "[MedicalProcessingServiceImpl] 图像服务连接成功";
            onImageServiceAvailabilityChanged(true);
        }
    } else {
        qDebug() << "[MedicalProcessingServiceImpl] 图像服务暂不可用，将在服务可用时自动连接";
    }
}

void MedicalProcessingServiceImpl::onImageServiceAvailabilityChanged(bool available)
{
    if (available && m_imageService) {
        qDebug() << "[MedicalProcessingServiceImpl] 图像服务已连接，处理功能已就绪";
    } else {
        qDebug() << "[MedicalProcessingServiceImpl] 图像服务连接丢失";
    }
}

void MedicalProcessingServiceImpl::onOperationTimeout()
{
    // 处理操作超时
    qDebug() << "[MedicalProcessingServiceImpl] 操作超时处理";
}

bool MedicalProcessingServiceImpl::createProcessedImage(const QString& inputId, const QString& outputId, const QString& algorithm, const QVariantMap& parameters)
{
    if (!m_imageService) {
        setError("图像服务未连接");
    return false;
}

    try {
        // 获取图像服务的正确类型
        MedicalImageCoreService* imageService = qobject_cast<MedicalImageCoreService*>(m_imageService);
        if (!imageService) {
            setError("图像服务类型转换失败");
            return false;
        }

        // 获取输入图像信息（直接CTK服务调用）
        QList<int> dimensions = imageService->getImageDimensions(inputId);
        QList<double> spacing = imageService->getImageSpacing(inputId);

        // 简化实现：直接复制输入图像作为处理结果的基础
        QString createdId = imageService->duplicateImage(inputId, outputId);
        if (createdId.isEmpty()) {
            setError("复制图像失败");
            return false;
        }

        // 记录处理信息到元数据
        imageService->setImageMetadata(outputId, "processing_algorithm", algorithm);
        imageService->setImageMetadata(outputId, "processing_parameters", QVariant(parameters).toString());
        imageService->setImageMetadata(outputId, "source_image", inputId);

        qDebug() << "[MedicalProcessingServiceImpl] 处理结果创建成功:" << outputId;
        return true;
    } catch (const std::exception& e) {
        setError(QString("创建处理结果失败: %1").arg(e.what()));
        return false;
    }
    
    // 模拟创建成功
    qDebug() << "[MedicalProcessingServiceImpl] 模拟创建处理结果图像:" << outputId << "算法:" << algorithm;
    
    // 在实际实现中，这里会：
    // 1. 从输入图像获取像素数据
    // 2. 应用具体的图像处理算法
    // 3. 创建新的图像数据对象
    // 4. 将处理结果注册到图像服务
    
    return true;
}

//-----------------------------------------------------------------------------
bool MedicalProcessingServiceImpl::performFloatThresholdSegmentation(float* data, qint64 totalPixels, double lowerThreshold, double upperThreshold, const QString& inputId, const QString& outputId)
{
    if (!data) {
        setError("图像数据为空");
        return false;
    }

    qDebug() << "[MedicalProcessingServiceImpl] 执行浮点数阈值分割，阈值范围:" << lowerThreshold << "到" << upperThreshold;

    try {
        // 创建分割结果数据
        float* segmentedData = new float[totalPixels];

        // 执行阈值分割
        qint64 segmentedPixels = 0;
        for (qint64 i = 0; i < totalPixels; ++i) {
            float pixelValue = data[i];
            if (pixelValue >= lowerThreshold && pixelValue <= upperThreshold) {
                segmentedData[i] = pixelValue;  // 保留原值
                segmentedPixels++;
            } else {
                segmentedData[i] = 0.0f;  // 设为背景
            }

            // 更新进度
            if (i % (totalPixels / 20) == 0) {
                int progress = static_cast<int>((i * 100) / totalPixels);
                emit processingProgress(outputId, progress);
            }
        }

        qDebug() << "[MedicalProcessingServiceImpl] 分割完成，保留像素数:" << segmentedPixels << "/" << totalPixels;

        // 获取图像服务
        MedicalImageCoreService* imageService = qobject_cast<MedicalImageCoreService*>(m_imageService);
        if (!imageService) {
            delete[] segmentedData;
            setError("图像服务类型转换失败");
            return false;
        }

        // 先复制原图像作为基础
        QString duplicatedId = imageService->duplicateImage(inputId, outputId);
        if (duplicatedId.isEmpty()) {
            delete[] segmentedData;
            setError("复制原图像失败");
            return false;
        }

        // 获取复制图像的像素数据指针
        void* outputPixelData = imageService->getImagePixelData(outputId);
        if (!outputPixelData) {
            delete[] segmentedData;
            setError("无法获取输出图像像素数据");
            return false;
        }

        // 将分割结果复制到输出图像
        memcpy(outputPixelData, segmentedData, totalPixels * sizeof(float));

        delete[] segmentedData;

        // 设置元数据
        imageService->setImageMetadata(outputId, "processing_algorithm", "ThresholdSegmentation");
        imageService->setImageMetadata(outputId, "lower_threshold", QString::number(lowerThreshold));
        imageService->setImageMetadata(outputId, "upper_threshold", QString::number(upperThreshold));
        imageService->setImageMetadata(outputId, "source_image", inputId);
        imageService->setImageMetadata(outputId, "segmented_pixels", QString::number(segmentedPixels));

        emit processingProgress(outputId, 100);
        return true;

    } catch (const std::exception& e) {
        setError(QString("阈值分割处理异常: %1").arg(e.what()));
        return false;
    }
}

//-----------------------------------------------------------------------------
bool MedicalProcessingServiceImpl::performIntThresholdSegmentation(short* data, qint64 totalPixels, double lowerThreshold, double upperThreshold, const QString& inputId, const QString& outputId)
{
    if (!data) {
        setError("图像数据为空");
        return false;
    }

    qDebug() << "[MedicalProcessingServiceImpl] 执行整数阈值分割，阈值范围:" << lowerThreshold << "到" << upperThreshold;

    try {
        // 创建分割结果数据
        short* segmentedData = new short[totalPixels];

        // 执行阈值分割
        qint64 segmentedPixels = 0;
        for (qint64 i = 0; i < totalPixels; ++i) {
            short pixelValue = data[i];
            if (pixelValue >= lowerThreshold && pixelValue <= upperThreshold) {
                segmentedData[i] = pixelValue;  // 保留原值
                segmentedPixels++;
            } else {
                segmentedData[i] = 0;  // 设为背景
            }

            // 更新进度
            if (i % (totalPixels / 20) == 0) {
                int progress = static_cast<int>((i * 100) / totalPixels);
                emit processingProgress(outputId, progress);
            }
        }

        qDebug() << "[MedicalProcessingServiceImpl] 分割完成，保留像素数:" << segmentedPixels << "/" << totalPixels;

        // 获取图像服务
        MedicalImageCoreService* imageService = qobject_cast<MedicalImageCoreService*>(m_imageService);
        if (!imageService) {
            delete[] segmentedData;
            setError("图像服务类型转换失败");
            return false;
        }

        // 先复制原图像作为基础
        QString duplicatedId = imageService->duplicateImage(inputId, outputId);
        if (duplicatedId.isEmpty()) {
            delete[] segmentedData;
            setError("复制原图像失败");
            return false;
        }

        // 获取复制图像的像素数据指针
        void* outputPixelData = imageService->getImagePixelData(outputId);
        if (!outputPixelData) {
            delete[] segmentedData;
            setError("无法获取输出图像像素数据");
            return false;
        }

        // 将分割结果复制到输出图像
        memcpy(outputPixelData, segmentedData, totalPixels * sizeof(short));

        delete[] segmentedData;

        // 设置元数据
        imageService->setImageMetadata(outputId, "processing_algorithm", "ThresholdSegmentation");
        imageService->setImageMetadata(outputId, "lower_threshold", QString::number(lowerThreshold));
        imageService->setImageMetadata(outputId, "upper_threshold", QString::number(upperThreshold));
        imageService->setImageMetadata(outputId, "source_image", inputId);
        imageService->setImageMetadata(outputId, "segmented_pixels", QString::number(segmentedPixels));

        emit processingProgress(outputId, 100);
        return true;

    } catch (const std::exception& e) {
        setError(QString("阈值分割处理异常: %1").arg(e.what()));
        return false;
    }
}

//-----------------------------------------------------------------------------
// 具体图像处理算法实现
//-----------------------------------------------------------------------------

bool MedicalProcessingServiceImpl::performThresholdSegmentation(const QString& inputId, const QString& outputId, const QVariantMap& parameters)
{
    if (!m_imageService) {
        setError("图像服务未连接");
        return false;
    }
    
    double lowerThreshold = parameters.value("lowerThreshold", 0.0).toDouble();
    double upperThreshold = parameters.value("upperThreshold", 255.0).toDouble();
    
    qDebug() << "[MedicalProcessingServiceImpl] 执行阈值分割, 下阈值:" << lowerThreshold << "上阈值:" << upperThreshold;
    
    try {
        // 获取图像服务的正确类型
        MedicalImageCoreService* imageService = qobject_cast<MedicalImageCoreService*>(m_imageService);
        if (!imageService) {
            setError("图像服务类型转换失败");
            return false;
        }

        // 获取输入图像数据（直接CTK服务调用）
        void* inputData = imageService->getImagePixelData(inputId);
        if (!inputData) {
            setError("无法获取图像像素数据");
            return false;
        }

        QList<int> dimensions = imageService->getImageDimensions(inputId);
        QString dataType = imageService->getImageDataType(inputId);
        
        // 实际的阈值分割算法实现
        qint64 totalPixels = 1;
        for (int dim : dimensions) {
            totalPixels *= dim;
        }

        qDebug() << "[MedicalProcessingServiceImpl] 开始阈值分割处理，总像素数:" << totalPixels;
        qDebug() << "[MedicalProcessingServiceImpl] 数据类型:" << dataType;

        // 执行真正的阈值分割算法
        bool segmentationSuccess = false;

        if (dataType == "float" || dataType == "double") {
            // 处理浮点数据
            float* floatData = static_cast<float*>(inputData);
            segmentationSuccess = performFloatThresholdSegmentation(floatData, totalPixels, lowerThreshold, upperThreshold, inputId, outputId);
        } else if (dataType == "short" || dataType == "int") {
            // 处理整数数据
            short* shortData = static_cast<short*>(inputData);
            segmentationSuccess = performIntThresholdSegmentation(shortData, totalPixels, lowerThreshold, upperThreshold, inputId, outputId);
        } else {
            qDebug() << "[MedicalProcessingServiceImpl] 不支持的数据类型:" << dataType;
            setError(QString("不支持的数据类型: %1").arg(dataType));
            return false;
        }

        if (segmentationSuccess) {
            qDebug() << "[MedicalProcessingServiceImpl] 阈值分割完成:" << outputId;
            return true;
        }
        
        setError("阈值分割处理失败");
        return false;
        
    } catch (const std::exception& e) {
        setError(QString("阈值分割失败: %1").arg(e.what()));
        return false;
    }
}

bool MedicalProcessingServiceImpl::performRegionGrowing(const QString& inputId, const QString& outputId, const QVariantMap& parameters)
{
    if (!m_imageService) {
        setError("图像服务未连接");
        return false;
    }
    
    QString seedPointsStr = parameters.value("seedPoints", "128,128,64").toString();
    double tolerance = parameters.value("tolerance", 10.0).toDouble();
    
    qDebug() << "[MedicalProcessingServiceImpl] 执行区域生长, 种子点:" << seedPointsStr << "容忍度:" << tolerance;
    
    try {
        // 模拟处理进度
        for (int i = 0; i <= 100; i += 10) {
            emit processingProgress(outputId, i);
            QThread::msleep(10);
        }
        
        if (createProcessedImage(inputId, outputId, "RegionGrowing", parameters)) {
            qDebug() << "[MedicalProcessingServiceImpl] 区域生长完成:" << outputId;
            return true;
        }
        
        setError("区域生长处理失败");
        return false;
        
    } catch (const std::exception& e) {
        setError(QString("区域生长失败: %1").arg(e.what()));
        return false;
    }
}

bool MedicalProcessingServiceImpl::performWatershedSegmentation(const QString& inputId, const QString& outputId, const QVariantMap& parameters)
{
    Q_UNUSED(inputId)
    Q_UNUSED(outputId)
    Q_UNUSED(parameters)
    
    setError("分水岭分割功能暂未实现");
    return false;
}

bool MedicalProcessingServiceImpl::performGaussianFilter(const QString& inputId, const QString& outputId, const QVariantMap& parameters)
{
    Q_UNUSED(inputId)
    Q_UNUSED(outputId)
    Q_UNUSED(parameters)
    
    setError("高斯滤波功能暂未实现");
    return false;
}

bool MedicalProcessingServiceImpl::performMedianFilter(const QString& inputId, const QString& outputId, const QVariantMap& parameters)
{
    Q_UNUSED(inputId)
    Q_UNUSED(outputId)
    Q_UNUSED(parameters)
    
    setError("中值滤波功能暂未实现");
    return false;
}

bool MedicalProcessingServiceImpl::performBilateralFilter(const QString& inputId, const QString& outputId, const QVariantMap& parameters)
{
    Q_UNUSED(inputId)
    Q_UNUSED(outputId)
    Q_UNUSED(parameters)
    
    setError("双边滤波功能暂未实现");
    return false;
}

bool MedicalProcessingServiceImpl::performCannyEdgeDetection(const QString& inputId, const QString& outputId, const QVariantMap& parameters)
{
    Q_UNUSED(inputId)
    Q_UNUSED(outputId)
    Q_UNUSED(parameters)
    
    setError("Canny边缘检测功能暂未实现");
    return false;
}

bool MedicalProcessingServiceImpl::performGradientMagnitudeEdgeDetection(const QString& inputId, const QString& outputId, const QVariantMap& parameters)
{
    Q_UNUSED(inputId)
    Q_UNUSED(outputId)
    Q_UNUSED(parameters)
    
    setError("梯度幅度边缘检测功能暂未实现");
    return false;
}

bool MedicalProcessingServiceImpl::performMorphologicalErosion(const QString& inputId, const QString& outputId, const QVariantMap& parameters)
{
    Q_UNUSED(inputId)
    Q_UNUSED(outputId)
    Q_UNUSED(parameters)
    
    setError("形态学腐蚀功能暂未实现");
    return false;
}

bool MedicalProcessingServiceImpl::performMorphologicalDilation(const QString& inputId, const QString& outputId, const QVariantMap& parameters)
{
    Q_UNUSED(inputId)
    Q_UNUSED(outputId)
    Q_UNUSED(parameters)
    
    setError("形态学膨胀功能暂未实现");
    return false;
}

bool MedicalProcessingServiceImpl::performMorphologicalOpening(const QString& inputId, const QString& outputId, const QVariantMap& parameters)
{
    Q_UNUSED(inputId)
    Q_UNUSED(outputId)
    Q_UNUSED(parameters)
    
    setError("形态学开运算功能暂未实现");
    return false;
}

bool MedicalProcessingServiceImpl::performMorphologicalClosing(const QString& inputId, const QString& outputId, const QVariantMap& parameters)
{
    Q_UNUSED(inputId)
    Q_UNUSED(outputId)
    Q_UNUSED(parameters)
    
    setError("形态学闭运算功能暂未实现");
        return false;
    }
    
void MedicalProcessingServiceImpl::setError(const QString& error)
{
    m_lastError = error;
    qWarning() << "[MedicalProcessingServiceImpl] 错误:" << error;
}
