#ifndef IMAGE_WRAPPER_H
#define IMAGE_WRAPPER_H

#include <QByteArray>
#include <QSize>
#include <QString>
#include <QSharedPointer>
#include <QMetaType>

#include <memory>
#include <atomic>

/**
 * @brief 医学影像数据包装类
 * 
 * 统一封装各种影像数据格式，支持：
 * - 原始像素数据（通过 QByteArray 管理）
 * - OpenCV cv::Mat（通过 void* 存储）
 * - VTK vtkImageData（通过 void* 存储）
 * 
 * 设计要点：
 * - 使用 QByteArray 管理原始像素数据，利用其内置内存池
 * - 支持跨线程传递（通过 QSharedPointer 或 std::shared_ptr）
 * - 引用计数管理，自动释放资源
 * - 线程安全的访问接口
 * 
 * 使用示例：
 * @code
 * // 创建影像包装器
 * auto wrapper = std::make_shared<ImageWrapper>(width, height, channels);
 * wrapper->setPixelData(rawData, dataSize);
 * 
 * // 跨线程传递
 * emit imageReady(wrapper);
 * 
 * // 接收端
 * void onImageReady(std::shared_ptr<ImageWrapper> image) {
 *     const uchar* data = image->pixelData();
 *     // 处理影像...
 * }
 * @endcode
 */
class ImageWrapper
{
public:
    /**
     * @brief 影像数据类型
     */
    enum class DataType {
        Unknown,
        RawPixels,      ///< 原始像素数据（QByteArray）
        OpenCVMat,      ///< OpenCV cv::Mat
        VTKImageData    ///< VTK vtkImageData
    };

    /**
     * @brief 像素格式
     */
    enum class PixelFormat {
        Unknown,
        Grayscale8,     ///< 8位灰度
        Grayscale16,    ///< 16位灰度
        RGB24,          ///< 24位RGB
        RGBA32,         ///< 32位RGBA
        Float32,        ///< 32位浮点
        Float64         ///< 64位浮点
    };

    /**
     * @brief 默认构造函数
     */
    ImageWrapper();

    /**
     * @brief 带尺寸构造函数
     */
    ImageWrapper(int width, int height, int channels = 1, PixelFormat format = PixelFormat::Grayscale8);

    /**
     * @brief 析构函数
     */
    ~ImageWrapper();

    // 禁止拷贝，允许移动
    ImageWrapper(const ImageWrapper&) = delete;
    ImageWrapper& operator=(const ImageWrapper&) = delete;
    ImageWrapper(ImageWrapper&& other) noexcept;
    ImageWrapper& operator=(ImageWrapper&& other) noexcept;

    // 尺寸和格式
    int width() const { return m_width; }
    int height() const { return m_height; }
    int channels() const { return m_channels; }
    QSize size() const { return QSize(m_width, m_height); }
    PixelFormat pixelFormat() const { return m_pixelFormat; }
    DataType dataType() const { return m_dataType; }

    // 像素数据访问
    const uchar* pixelData() const;
    uchar* pixelData();
    qint64 pixelDataSize() const { return m_pixelData.size(); }
    bool isEmpty() const { return m_pixelData.isEmpty() && !m_externalData; }

    // 设置像素数据
    void setPixelData(const uchar* data, qint64 size);
    void setPixelData(const QByteArray& data);
    void allocate(int width, int height, int channels, PixelFormat format = PixelFormat::Grayscale8);

    // 外部数据引用（不拥有所有权）
    void setExternalData(void* data, DataType type);
    void* externalData() const { return m_externalData; }

    // 元数据
    void setMetadata(const QString& key, const QString& value);
    QString metadata(const QString& key) const;

    // 深拷贝
    std::shared_ptr<ImageWrapper> clone() const;

    // 线程安全的引用计数（用于调试）
    int refCount() const { return m_refCount.load(); }

private:
    int m_width;
    int m_height;
    int m_channels;
    PixelFormat m_pixelFormat;
    DataType m_dataType;

    QByteArray m_pixelData;         ///< 原始像素数据（使用 QByteArray 内置内存池）
    void* m_externalData;           ///< 外部数据指针（cv::Mat 或 vtkImageData）
    QMap<QString, QString> m_metadata;

    mutable std::atomic<int> m_refCount{0};
};

// 注册到 Qt 元类型系统，支持信号槽传递
Q_DECLARE_METATYPE(std::shared_ptr<ImageWrapper>)
Q_DECLARE_METATYPE(QSharedPointer<ImageWrapper>)

#endif // IMAGE_WRAPPER_H

