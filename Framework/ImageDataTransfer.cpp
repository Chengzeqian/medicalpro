#include "ImageDataTransfer.h"

#include "StartupOrchestrator.h"
#include "ErrorHandler.h"

#include <QBuffer>
#include <QDebug>
#include <QImageReader>
#include <QMutexLocker>

#ifdef VTK_FOUND
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>
#endif

ImageData::ImageData()
    : m_type(ImageDataType::Unknown)
{
}

ImageData::ImageDataType ImageData::type() const
{
    return m_type;
}

void ImageData::clear()
{
#ifdef VTK_FOUND
    m_vtkImage = nullptr;
#endif
    m_qImage = QImage();
    m_rawBuffer.clear();
    m_metadata.clear();
    m_type = ImageDataType::Unknown;
}

#ifdef VTK_FOUND
void ImageData::setVTKImage(const vtkSmartPointer<vtkImageData>& image)
{
    m_vtkImage = image;
    m_qImage = QImage();
    m_rawBuffer.clear();
    m_type = ImageDataType::VTKImage;
}

vtkSmartPointer<vtkImageData> ImageData::vtkImage() const
{
    return m_type == ImageDataType::VTKImage ? m_vtkImage : nullptr;
}
#endif

void ImageData::setQImage(const QImage& image)
{
    m_qImage = image;
#ifdef VTK_FOUND
    m_vtkImage = nullptr;
#endif
    m_rawBuffer.clear();
    m_type = ImageDataType::QImage;
}

QImage ImageData::qImage() const
{
    return m_type == ImageDataType::QImage ? m_qImage : QImage();
}

void ImageData::setRawBuffer(const QByteArray& buffer)
{
    m_rawBuffer = buffer;
#ifdef VTK_FOUND
    m_vtkImage = nullptr;
#endif
    m_qImage = QImage();
    m_type = ImageDataType::RawBuffer;
}

QByteArray ImageData::rawBuffer() const
{
    return m_type == ImageDataType::RawBuffer ? m_rawBuffer : QByteArray();
}

void ImageData::setMetadata(const QVariantMap& metadata)
{
    m_metadata = metadata;
}

QVariantMap ImageData::metadata() const
{
    return m_metadata;
}

void ImageData::addMetadata(const QString& key, const QVariant& value)
{
    m_metadata.insert(key, value);
}

bool ImageData::convertTo(ImageDataType targetType)
{
    if (m_type == targetType) {
        return true;
    }

#ifdef VTK_FOUND
    if (targetType == ImageDataType::QImage && m_type == ImageDataType::VTKImage) {
        return convertFromVTKToQImage();
    }
#endif

    if (targetType == ImageDataType::RawBuffer && m_type == ImageDataType::QImage) {
        return convertFromQImageToRaw();
    }

    if (targetType == ImageDataType::QImage && m_type == ImageDataType::RawBuffer) {
        return convertFromRawToQImage();
    }

    StartupOrchestrator::instance()->logDiagnostic(
        ErrorHandler::ErrorLevel::Warning,
        QStringLiteral("ImageData 无法转换类型：%1 -> %2")
            .arg(static_cast<int>(m_type))
            .arg(static_cast<int>(targetType)));
    return false;
}

ImageData ImageData::clone() const
{
    ImageData copy;
    copy.m_type = m_type;
    copy.m_metadata = m_metadata;
#ifdef VTK_FOUND
    if (m_type == ImageDataType::VTKImage && m_vtkImage) {
        copy.m_vtkImage = cloneVTKImage();
    }
#endif
    if (m_type == ImageDataType::QImage) {
        copy.m_qImage = m_qImage.copy();
    }
    if (m_type == ImageDataType::RawBuffer) {
        copy.m_rawBuffer = m_rawBuffer;
    }
    return copy;
}

#ifdef VTK_FOUND
bool ImageData::convertFromVTKToQImage()
{
    if (!m_vtkImage) {
        return false;
    }

    int dims[3] = {0};
    m_vtkImage->GetDimensions(dims);
    const int width = dims[0];
    const int height = dims[1];

    if (width <= 0 || height <= 0) {
        return false;
    }

    QImage image(width, height, QImage::Format_RGBA8888);
    if (image.isNull()) {
        return false;
    }

    void* voidPtr = m_vtkImage->GetScalarPointer();
    if (!voidPtr) {
        return false;
    }

    memcpy(image.bits(), voidPtr, width * height * 4);
    m_qImage = image;
    m_type = ImageDataType::QImage;
    return true;
}

vtkSmartPointer<vtkImageData> ImageData::cloneVTKImage() const
{
    if (!m_vtkImage) {
        return nullptr;
    }

    vtkNew<vtkImageData> clone;
    clone->DeepCopy(m_vtkImage);
    return clone;
}
#endif

bool ImageData::convertFromQImageToRaw()
{
    if (m_qImage.isNull()) {
        return false;
    }

    QByteArray buffer;
    QBuffer qBuffer(&buffer);
    qBuffer.open(QIODevice::WriteOnly);
    m_qImage.save(&qBuffer, "PNG");
    qBuffer.close();

    m_rawBuffer = buffer;
    m_type = ImageDataType::RawBuffer;
    return true;
}

bool ImageData::convertFromRawToQImage()
{
    if (m_rawBuffer.isEmpty()) {
        return false;
    }

    QImage image;
    image.loadFromData(m_rawBuffer);
    if (image.isNull()) {
        return false;
    }

    m_qImage = image;
    m_type = ImageDataType::QImage;
    return true;
}
