#include "MedicalImageData.h"
#include <QDebug>
#include <QUuid>
#include <QFileInfo>
#include <QDateTime>

MedicalImageData::MedicalImageData(QObject* parent)
    : QObject(parent)
    , m_dataType(DataType::Unknown)
#ifdef ITK_FOUND
    , m_itkImage(nullptr)
#endif
    , m_pixelData(nullptr)
    , m_pixelDataSize(0)
    , m_ownsPixelData(false)
{
    initializeDefaults();
}

MedicalImageData::~MedicalImageData()
{
    cleanup();
}

void MedicalImageData::initializeDefaults()
{
    m_imageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_loadTime = QDateTime::currentDateTime();
    m_dataType = DataType::Unknown;
    
    qDebug() << "[MedicalImageData] 医学图像数据对象已创建, ID:" << m_imageId;
}

void MedicalImageData::cleanup()
{
    if (m_ownsPixelData && m_pixelData) {
        delete[] static_cast<char*>(m_pixelData);
        m_pixelData = nullptr;
    }
    
#ifdef ITK_FOUND
    if (m_itkImage) {
        // ITK智能指针会自动清理
        m_itkImage = nullptr;
    }
#endif

    qDebug() << "[MedicalImageData] 图像数据已清理:" << m_imageId;
}

QString MedicalImageData::getImageId() const
{
    return m_imageId;
}

void MedicalImageData::setImageId(const QString& id)
{
    if (m_imageId != id) {
        m_imageId = id;
        emit imageDataChanged();
    }
}

QString MedicalImageData::getFilePath() const
{
    return m_filePath;
}

void MedicalImageData::setFilePath(const QString& path)
{
    if (m_filePath != path) {
        m_filePath = path;
        
        // 自动设置格式（基于文件扩展名）
        if (!path.isEmpty()) {
            QFileInfo fileInfo(path);
            QString extension = fileInfo.suffix().toLower();
            
            if (extension == "dcm" || extension == "ima" || extension == "dicom") {
                setImageFormat("DICOM");
            } else if (extension == "nrrd" || extension == "nhdr") {
                setImageFormat("NRRD");
            } else if (extension == "nii" || extension == "nii.gz") {
                setImageFormat("NIfTI");
            } else if (extension == "mhd" || extension == "mha") {
                setImageFormat("MetaImage");
            } else {
                setImageFormat(extension.toUpper());
            }
        }
        
        emit imageDataChanged();
    }
}

QString MedicalImageData::getImageFormat() const
{
    return m_imageFormat;
}

void MedicalImageData::setImageFormat(const QString& format)
{
    if (m_imageFormat != format) {
        m_imageFormat = format;
        emit imageDataChanged();
    }
}

QDateTime MedicalImageData::getLoadTime() const
{
    return m_loadTime;
}

int MedicalImageData::getDimensionCount() const
{
    return m_dimensions.size();
}

QList<int> MedicalImageData::getDimensions() const
{
    return m_dimensions;
}

void MedicalImageData::setDimensions(const QList<int>& dimensions)
{
    if (m_dimensions != dimensions) {
        m_dimensions = dimensions;
        emit imageDataChanged();
    }
}

QList<double> MedicalImageData::getSpacing() const
{
    return m_spacing;
}

void MedicalImageData::setSpacing(const QList<double>& spacing)
{
    if (m_spacing != spacing) {
        m_spacing = spacing;
        emit imageDataChanged();
    }
}

QList<double> MedicalImageData::getOrigin() const
{
    return m_origin;
}

void MedicalImageData::setOrigin(const QList<double>& origin)
{
    if (m_origin != origin) {
        m_origin = origin;
        emit imageDataChanged();
    }
}

MedicalImageData::DataType MedicalImageData::getDataType() const
{
    return m_dataType;
}

void MedicalImageData::setDataType(DataType type)
{
    if (m_dataType != type) {
        m_dataType = type;
        emit imageDataChanged();
    }
}

qint64 MedicalImageData::getPixelCount() const
{
    qint64 count = 1;
    for (int dim : m_dimensions) {
        count *= dim;
    }
    return count;
}

qint64 MedicalImageData::getMemorySize() const
{
    qint64 pixelCount = getPixelCount();
    if (pixelCount == 0) return 0;
    
    int bytesPerPixel = 1;
    switch (m_dataType) {
        case DataType::UChar:
            bytesPerPixel = 1;
            break;
        case DataType::Short:
        case DataType::UShort:
            bytesPerPixel = 2;
            break;
        case DataType::Int:
        case DataType::UInt:
        case DataType::Float:
            bytesPerPixel = 4;
            break;
        case DataType::Double:
            bytesPerPixel = 8;
            break;
        default:
            bytesPerPixel = 1;
    }
    
    return pixelCount * bytesPerPixel;
}

QMap<QString, QVariant> MedicalImageData::getMetadata() const
{
    return m_metadata;
}

QVariant MedicalImageData::getMetadata(const QString& key, const QVariant& defaultValue) const
{
    return m_metadata.value(key, defaultValue);
}

void MedicalImageData::setMetadata(const QString& key, const QVariant& value)
{
    if (m_metadata.value(key) != value) {
        m_metadata[key] = value;
        emit metadataChanged(key, value);
    }
}

void MedicalImageData::removeMetadata(const QString& key)
{
    if (m_metadata.contains(key)) {
        m_metadata.remove(key);
        emit metadataChanged(key, QVariant());
    }
}

void MedicalImageData::clearMetadata()
{
    if (!m_metadata.isEmpty()) {
        m_metadata.clear();
        emit imageDataChanged();
    }
}

#ifdef VTK_FOUND
vtkSmartPointer<vtkImageData> MedicalImageData::getVTKImage() const
{
    return m_vtkImage;
}

void MedicalImageData::setVTKImage(vtkSmartPointer<vtkImageData> image)
{
    if (m_vtkImage != image) {
        m_vtkImage = image;
        
        // 从VTK图像更新基本信息
        if (image) {
            int* dims = image->GetDimensions();
            setDimensions({dims[0], dims[1], dims[2]});
            
            double* spacing = image->GetSpacing();
            setSpacing({spacing[0], spacing[1], spacing[2]});
            
            double* origin = image->GetOrigin();
            setOrigin({origin[0], origin[1], origin[2]});
            
            // 设置数据类型
            int scalarType = image->GetScalarType();
            switch (scalarType) {
                case VTK_UNSIGNED_CHAR:
                    setDataType(DataType::UChar);
                    break;
                case VTK_SHORT:
                    setDataType(DataType::Short);
                    break;
                case VTK_UNSIGNED_SHORT:
                    setDataType(DataType::UShort);
                    break;
                case VTK_INT:
                    setDataType(DataType::Int);
                    break;
                case VTK_UNSIGNED_INT:
                    setDataType(DataType::UInt);
                    break;
                case VTK_FLOAT:
                    setDataType(DataType::Float);
                    break;
                case VTK_DOUBLE:
                    setDataType(DataType::Double);
                    break;
            }
        }
        
        emit imageDataChanged();
    }
}
#endif

void* MedicalImageData::getPixelData() const
{
#ifdef VTK_FOUND
    if (m_vtkImage) {
        return m_vtkImage->GetScalarPointer();
    }
#endif
    return m_pixelData;
}

void MedicalImageData::setPixelData(void* data, qint64 size, bool takeOwnership)
{
    if (m_ownsPixelData && m_pixelData) {
        delete[] static_cast<char*>(m_pixelData);
    }
    
    m_pixelData = data;
    m_pixelDataSize = size;
    m_ownsPixelData = takeOwnership;
    
    emit imageDataChanged();
}

bool MedicalImageData::isValid() const
{
    return !m_dimensions.isEmpty() && 
           m_dimensions[0] > 0 && 
           (getPixelData() != nullptr || 
#ifdef VTK_FOUND
            m_vtkImage != nullptr ||
#endif
#ifdef ITK_FOUND
            m_itkImage != nullptr ||
#endif
            false);
}

bool MedicalImageData::is3D() const
{
    return m_dimensions.size() >= 3 && m_dimensions[2] > 1;
}

QString MedicalImageData::getImageSummary() const
{
    QString summary;
    
    summary += QString("Image ID: %1\n").arg(m_imageId);
    summary += QString("Format: %1\n").arg(m_imageFormat);
    
    if (!m_dimensions.isEmpty()) {
        QStringList dimStrings;
        for (int dim : m_dimensions) {
            dimStrings << QString::number(dim);
        }
        summary += QString("Dimensions: %1\n").arg(dimStrings.join(" x "));
    }
    
    if (!m_spacing.isEmpty()) {
        QStringList spacingStrings;
        for (double sp : m_spacing) {
            spacingStrings << QString::number(sp, 'f', 3);
        }
        summary += QString("Spacing: %1\n").arg(spacingStrings.join(", "));
    }
    
    QString dataTypeStr;
    switch (m_dataType) {
        case DataType::UChar: dataTypeStr = "UChar"; break;
        case DataType::Short: dataTypeStr = "Short"; break;
        case DataType::UShort: dataTypeStr = "UShort"; break;
        case DataType::Int: dataTypeStr = "Int"; break;
        case DataType::UInt: dataTypeStr = "UInt"; break;
        case DataType::Float: dataTypeStr = "Float"; break;
        case DataType::Double: dataTypeStr = "Double"; break;
        default: dataTypeStr = "Unknown";
    }
    summary += QString("Data Type: %1\n").arg(dataTypeStr);
    
    summary += QString("Memory Size: %1 MB\n").arg(getMemorySize() / (1024.0 * 1024.0), 0, 'f', 2);
    summary += QString("Load Time: %1\n").arg(m_loadTime.toString());
    
    if (!m_filePath.isEmpty()) {
        summary += QString("File Path: %1\n").arg(m_filePath);
    }
    
    return summary.trimmed();
}

QMap<QString, double> MedicalImageData::getImageStatistics() const
{
    QMap<QString, double> stats;
    
    // 这里应该实现实际的图像统计计算
    // 简化实现，返回默认值
    stats["min"] = 0.0;
    stats["max"] = 1.0;
    stats["mean"] = 0.5;
    stats["std"] = 0.1;
    
    return stats;
}

MedicalImageData* MedicalImageData::clone() const
{
    auto* cloned = new MedicalImageData();
    
    cloned->m_filePath = m_filePath;
    cloned->m_imageFormat = m_imageFormat;
    cloned->m_dimensions = m_dimensions;
    cloned->m_spacing = m_spacing;
    cloned->m_origin = m_origin;
    cloned->m_dataType = m_dataType;
    cloned->m_metadata = m_metadata;
    
#ifdef VTK_FOUND
    if (m_vtkImage) {
        auto clonedVTK = vtkSmartPointer<vtkImageData>::New();
        clonedVTK->DeepCopy(m_vtkImage);
        cloned->setVTKImage(clonedVTK);
    }
#endif
    
    // 注意：克隆时不复制像素数据，需要调用者明确处理
    
    return cloned;
}
