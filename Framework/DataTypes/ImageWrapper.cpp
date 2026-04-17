#include "ImageWrapper.h"
#include <QDebug>

ImageWrapper::ImageWrapper()
    : m_width(0)
    , m_height(0)
    , m_channels(1)
    , m_pixelFormat(PixelFormat::Unknown)
    , m_dataType(DataType::Unknown)
    , m_externalData(nullptr)
{
}

ImageWrapper::ImageWrapper(int width, int height, int channels, PixelFormat format)
    : m_width(width)
    , m_height(height)
    , m_channels(channels)
    , m_pixelFormat(format)
    , m_dataType(DataType::RawPixels)
    , m_externalData(nullptr)
{
    allocate(width, height, channels, format);
}

ImageWrapper::~ImageWrapper()
{
    // QByteArray 自动释放内存
    // 外部数据不由此类管理
    m_externalData = nullptr;
}

ImageWrapper::ImageWrapper(ImageWrapper&& other) noexcept
    : m_width(other.m_width)
    , m_height(other.m_height)
    , m_channels(other.m_channels)
    , m_pixelFormat(other.m_pixelFormat)
    , m_dataType(other.m_dataType)
    , m_pixelData(std::move(other.m_pixelData))
    , m_externalData(other.m_externalData)
    , m_metadata(std::move(other.m_metadata))
{
    other.m_width = 0;
    other.m_height = 0;
    other.m_externalData = nullptr;
}

ImageWrapper& ImageWrapper::operator=(ImageWrapper&& other) noexcept
{
    if (this != &other) {
        m_width = other.m_width;
        m_height = other.m_height;
        m_channels = other.m_channels;
        m_pixelFormat = other.m_pixelFormat;
        m_dataType = other.m_dataType;
        m_pixelData = std::move(other.m_pixelData);
        m_externalData = other.m_externalData;
        m_metadata = std::move(other.m_metadata);

        other.m_width = 0;
        other.m_height = 0;
        other.m_externalData = nullptr;
    }
    return *this;
}

const uchar* ImageWrapper::pixelData() const
{
    if (!m_pixelData.isEmpty()) {
        return reinterpret_cast<const uchar*>(m_pixelData.constData());
    }
    return nullptr;
}

uchar* ImageWrapper::pixelData()
{
    if (!m_pixelData.isEmpty()) {
        return reinterpret_cast<uchar*>(m_pixelData.data());
    }
    return nullptr;
}

void ImageWrapper::setPixelData(const uchar* data, qint64 size)
{
    if (data && size > 0) {
        m_pixelData = QByteArray(reinterpret_cast<const char*>(data), static_cast<int>(size));
        m_dataType = DataType::RawPixels;
    }
}

void ImageWrapper::setPixelData(const QByteArray& data)
{
    m_pixelData = data;
    m_dataType = DataType::RawPixels;
}

void ImageWrapper::allocate(int width, int height, int channels, PixelFormat format)
{
    m_width = width;
    m_height = height;
    m_channels = channels;
    m_pixelFormat = format;
    m_dataType = DataType::RawPixels;

    // 计算每像素字节数
    int bytesPerPixel = channels;
    switch (format) {
        case PixelFormat::Grayscale16:
            bytesPerPixel = 2;
            break;
        case PixelFormat::Float32:
            bytesPerPixel = 4 * channels;
            break;
        case PixelFormat::Float64:
            bytesPerPixel = 8 * channels;
            break;
        default:
            bytesPerPixel = channels;
            break;
    }

    qint64 totalSize = static_cast<qint64>(width) * height * bytesPerPixel;
    m_pixelData.resize(static_cast<int>(totalSize));
    m_pixelData.fill(0);
}

void ImageWrapper::setExternalData(void* data, DataType type)
{
    m_externalData = data;
    m_dataType = type;
}

void ImageWrapper::setMetadata(const QString& key, const QString& value)
{
    m_metadata[key] = value;
}

QString ImageWrapper::metadata(const QString& key) const
{
    return m_metadata.value(key);
}

std::shared_ptr<ImageWrapper> ImageWrapper::clone() const
{
    auto cloned = std::make_shared<ImageWrapper>();
    cloned->m_width = m_width;
    cloned->m_height = m_height;
    cloned->m_channels = m_channels;
    cloned->m_pixelFormat = m_pixelFormat;
    cloned->m_dataType = m_dataType;
    cloned->m_pixelData = m_pixelData;  // QByteArray 使用 COW
    cloned->m_metadata = m_metadata;
    // 外部数据不复制
    return cloned;
}

