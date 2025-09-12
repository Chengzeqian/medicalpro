#ifndef MEDICAL_IMAGE_DATA_H
#define MEDICAL_IMAGE_DATA_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QVariant>
#include <QDateTime>
#include <QUuid>

// 前向声明
#ifdef ITK_FOUND
#include <itkImage.h>
template<typename PixelType, unsigned int Dimension>
using ITKImageType = itk::Image<PixelType, Dimension>;
#endif

#ifdef VTK_FOUND
#include <vtkSmartPointer.h>
#include <vtkImageData.h>
#endif

// 注意：作为主程序共享数据结构，不需要DLL导出宏
// 所有插件都可以直接访问此类定义

/**
 * @brief 医学图像数据类 (共享数据结构)
 * 
 * 统一封装医学图像数据，支持多种后端：
 * - ITK图像数据 (用于图像处理)
 * - VTK图像数据 (用于可视化)
 * - Qt原生图像数据 (用于2D显示)
 * 
 * 提供统一的接口访问图像属性和数据
 * 
 * 注意：此类现在作为主程序的共享数据结构，
 * 所有插件通过CTK服务接口进行数据传递
 */
class MedicalImageData : public QObject
{
    Q_OBJECT
    
public:
    /**
     * @brief 图像数据类型
     */
    enum class DataType {
        Unknown = 0,
        UChar,      // unsigned char
        Short,      // short
        UShort,     // unsigned short
        Int,        // int
        UInt,       // unsigned int
        Float,      // float
        Double      // double
    };
    
    /**
     * @brief 图像方向
     */
    enum class Orientation {
        Unknown = 0,
        Axial,      // 轴状面
        Sagittal,   // 矢状面
        Coronal     // 冠状面
    };

    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit MedicalImageData(QObject* parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~MedicalImageData() override;

    // ========== 基本属性 ==========
    
    /**
     * @brief 获取图像ID
     * @return 唯一图像标识符
     */
    QString getImageId() const;
    
    /**
     * @brief 设置图像ID
     * @param id 图像ID
     */
    void setImageId(const QString& id);
    
    /**
     * @brief 获取文件路径
     * @return 原始文件路径
     */
    QString getFilePath() const;
    
    /**
     * @brief 设置文件路径
     * @param path 文件路径
     */
    void setFilePath(const QString& path);
    
    /**
     * @brief 获取图像格式
     * @return 图像格式
     */
    QString getImageFormat() const;
    
    /**
     * @brief 设置图像格式
     * @param format 图像格式
     */
    void setImageFormat(const QString& format);
    
    /**
     * @brief 获取加载时间
     * @return 加载时间戳
     */
    QDateTime getLoadTime() const;
    
    // ========== 图像尺寸和属性 ==========
    
    /**
     * @brief 获取图像维度数量
     * @return 维度数量 (2D/3D/4D)
     */
    int getDimensionCount() const;
    
    /**
     * @brief 获取图像尺寸
     * @return 各维度尺寸 [width, height, depth, ...]
     */
    QList<int> getDimensions() const;
    
    /**
     * @brief 设置图像尺寸
     * @param dimensions 尺寸数组
     */
    void setDimensions(const QList<int>& dimensions);
    
    /**
     * @brief 获取像素间距
     * @return 各维度像素间距 [x, y, z, ...]
     */
    QList<double> getSpacing() const;
    
    /**
     * @brief 设置像素间距
     * @param spacing 像素间距数组
     */
    void setSpacing(const QList<double>& spacing);
    
    /**
     * @brief 获取图像原点
     * @return 图像原点坐标 [x, y, z, ...]
     */
    QList<double> getOrigin() const;
    
    /**
     * @brief 设置图像原点
     * @param origin 原点坐标数组
     */
    void setOrigin(const QList<double>& origin);
    
    /**
     * @brief 获取数据类型
     * @return 像素数据类型
     */
    DataType getDataType() const;
    
    /**
     * @brief 设置数据类型
     * @param type 数据类型
     */
    void setDataType(DataType type);
    
    /**
     * @brief 获取像素数量
     * @return 总像素数量
     */
    qint64 getPixelCount() const;
    
    /**
     * @brief 获取内存占用大小
     * @return 内存大小（字节）
     */
    qint64 getMemorySize() const;
    
    // ========== 元数据管理 ==========
    
    /**
     * @brief 获取所有元数据
     * @return 元数据键值对
     */
    QMap<QString, QVariant> getMetadata() const;
    
    /**
     * @brief 获取元数据值
     * @param key 元数据键
     * @param defaultValue 默认值
     * @return 元数据值
     */
    QVariant getMetadata(const QString& key, const QVariant& defaultValue = QVariant()) const;
    
    /**
     * @brief 设置元数据
     * @param key 元数据键
     * @param value 元数据值
     */
    void setMetadata(const QString& key, const QVariant& value);
    
    /**
     * @brief 移除元数据
     * @param key 元数据键
     */
    void removeMetadata(const QString& key);
    
    /**
     * @brief 清空所有元数据
     */
    void clearMetadata();
    
    // ========== 数据访问接口 ==========
    
#ifdef ITK_FOUND
    /**
     * @brief 获取ITK图像数据
     * @return ITK图像指针
     */
    template<typename PixelType, unsigned int Dimension>
    typename ITKImageType<PixelType, Dimension>::Pointer getITKImage() const;
    
    /**
     * @brief 设置ITK图像数据
     * @param image ITK图像指针
     */
    template<typename PixelType, unsigned int Dimension>
    void setITKImage(typename ITKImageType<PixelType, Dimension>::Pointer image);
#endif

#ifdef VTK_FOUND
    /**
     * @brief 获取VTK图像数据
     * @return VTK图像指针
     */
    vtkSmartPointer<vtkImageData> getVTKImage() const;
    
    /**
     * @brief 设置VTK图像数据
     * @param image VTK图像指针
     */
    void setVTKImage(vtkSmartPointer<vtkImageData> image);
#endif
    
    /**
     * @brief 获取原始像素数据指针
     * @return 像素数据指针
     */
    void* getPixelData() const;
    
    /**
     * @brief 设置原始像素数据
     * @param data 像素数据指针
     * @param size 数据大小
     * @param takeOwnership 是否接管内存管理
     */
    void setPixelData(void* data, qint64 size, bool takeOwnership = false);
    
    // ========== 实用工具 ==========
    
    /**
     * @brief 检查图像是否有效
     * @return 是否包含有效图像数据
     */
    bool isValid() const;
    
    /**
     * @brief 检查图像是否为3D
     * @return 是否为3D图像
     */
    bool is3D() const;
    
    /**
     * @brief 获取图像信息摘要
     * @return 图像信息字符串
     */
    QString getImageSummary() const;
    
    /**
     * @brief 计算图像统计信息
     * @return 统计信息 (min, max, mean, std)
     */
    QMap<QString, double> getImageStatistics() const;
    
    /**
     * @brief 克隆图像数据
     * @return 克隆的图像数据对象
     */
    MedicalImageData* clone() const;

signals:
    /**
     * @brief 图像数据变化信号
     */
    void imageDataChanged();
    
    /**
     * @brief 元数据变化信号
     * @param key 变化的键
     * @param value 新值
     */
    void metadataChanged(const QString& key, const QVariant& value);

private:
    // 基本属性
    QString m_imageId;
    QString m_filePath;
    QString m_imageFormat;
    QDateTime m_loadTime;
    
    // 图像属性
    QList<int> m_dimensions;
    QList<double> m_spacing;
    QList<double> m_origin;
    DataType m_dataType;
    
    // 元数据
    QMap<QString, QVariant> m_metadata;
    
    // 图像数据存储
#ifdef ITK_FOUND
    void* m_itkImage;  // 存储ITK图像的通用指针
#endif
#ifdef VTK_FOUND
    vtkSmartPointer<vtkImageData> m_vtkImage;
#endif
    void* m_pixelData;
    qint64 m_pixelDataSize;
    bool m_ownsPixelData;
    
    /**
     * @brief 初始化默认值
     */
    void initializeDefaults();
    
    /**
     * @brief 清理资源
     */
    void cleanup();
};

// 注册到Qt元类型系统，支持CTK服务接口中的参数传递
Q_DECLARE_METATYPE(MedicalImageData*)
Q_DECLARE_METATYPE(QSharedPointer<MedicalImageData>)

#endif // MEDICAL_IMAGE_DATA_H
