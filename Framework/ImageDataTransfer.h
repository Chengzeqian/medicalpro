#ifndef IMAGEDATATRANSFER_H
#define IMAGEDATATRANSFER_H

#include "FrameworkExport.h"

#include <QByteArray>
#include <QImage>
#include <QVariantMap>

#ifdef VTK_FOUND
#include <vtkSmartPointer.h>
#include <vtkImageData.h>
#endif

class FRAMEWORK_EXPORT ImageData
{
public:
    enum class ImageDataType {
        Unknown,
        VTKImage,
        QImage,
        RawBuffer
    };

    ImageData();

    ImageDataType type() const;
    void clear();

#ifdef VTK_FOUND
    void setVTKImage(const vtkSmartPointer<vtkImageData>& image);
    vtkSmartPointer<vtkImageData> vtkImage() const;
#endif

    void setQImage(const QImage& image);
    QImage qImage() const;

    void setRawBuffer(const QByteArray& buffer);
    QByteArray rawBuffer() const;

    void setMetadata(const QVariantMap& metadata);
    QVariantMap metadata() const;
    void addMetadata(const QString& key, const QVariant& value);

    bool convertTo(ImageDataType targetType);
    ImageData clone() const;

private:
    bool convertFromVTKToQImage();
    bool convertFromQImageToRaw();
    bool convertFromRawToQImage();

#ifdef VTK_FOUND
    vtkSmartPointer<vtkImageData> cloneVTKImage() const;
#endif

private:
    ImageDataType m_type;
    QVariantMap m_metadata;
#ifdef VTK_FOUND
    vtkSmartPointer<vtkImageData> m_vtkImage;
#endif
    QImage m_qImage;
    QByteArray m_rawBuffer;
};

#endif // IMAGEDATATRANSFER_H
