#include "FourViewDisplayServiceImpl.h"
#include "FourViewVTKWidget.h"
#include <QDebug>
#include <QFileInfo>
#include <QCoreApplication>
#include <algorithm>
#include <cstring>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Windows API for dynamic function loading
#ifdef _WIN32
#include <Windows.h>
#endif

// ITK includes for reading nii.gz files
#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkImageToVTKImageFilter.h>
#include <itkImageIOFactory.h>
#include <itkMultiThreaderBase.h>
#include <itkImageIOBase.h>

// VTK includes
#include <vtkImageData.h>
#include <vtkXMLImageDataReader.h>
#include <vtkXMLImageDataWriter.h>
#include <vtkXMLPolyDataReader.h>
#include <vtkXMLPolyDataWriter.h>
#include <vtkNIFTIImageReader.h>
#include <vtkRenderer.h>
#include <vtkCamera.h>
#include <vtkMarchingCubes.h>
#include <vtkFlyingEdges3D.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkSmartVolumeMapper.h>
#include <vtkVolumeProperty.h>
#include <vtkPiecewiseFunction.h>
#include <vtkColorTransferFunction.h>
#include <vtkVolume.h>
#include <vtkImageProperty.h>
#include <vtkSmoothPolyDataFilter.h>
#include <vtkDecimatePro.h>
#include <vtkWindowedSincPolyDataFilter.h>
#include <vtkPolyDataNormals.h>
#include <vtkImageShrink3D.h>
#include <vtkRenderWindow.h>
#include <vtkSTLReader.h>
#include <vtkNew.h>
#include <QElapsedTimer>
#include <QDateTime>
#include <QApplication>
#include <QDir>
#include <QCoreApplication>

FourViewDisplayServiceImpl::FourViewDisplayServiceImpl(QObject* parent)
    : FourViewDisplayService(parent)
    , m_imageData(nullptr)
    , m_3dRenderer(nullptr)
    , m_3dActor(nullptr)
    , m_3dVolume(nullptr)
    , m_windowWidth(2000.0)
    , m_windowLevel(400.0)
    , m_3dRenderMode("surface")
    , m_3dOpacity(1.0)
    , m_3dColorR(255)
    , m_3dColorG(228)
    , m_3dColorB(196)
    , m_initialized(false)
    , m_axialSlice(0)
    , m_sagittalSlice(0)
    , m_coronalSlice(0)
    , m_context(nullptr)
    , m_vtkWidget(nullptr)
    , m_toolActor(nullptr)
    , m_toolPolyData(nullptr)
    , m_toolSphere(nullptr)
    , m_toolModelVisible(false)
{
    m_toolPosition[0] = m_toolPosition[1] = m_toolPosition[2] = 0.0;
    qDebug() << "[FourViewDisplayServiceImpl] 创建四视图显示服务";
}

FourViewDisplayServiceImpl::~FourViewDisplayServiceImpl()
{
    qDebug() << "[FourViewDisplayServiceImpl] 销毁四视图显示服务";
}

void FourViewDisplayServiceImpl::setPluginContext(ctkPluginContext* context)
{
    m_context = context;
    qDebug() << "[FourViewDisplayServiceImpl] CTK Context 已设置";
}

void FourViewDisplayServiceImpl::ensureInitialized()
{
    if (m_initialized) {
        return; // 已经初始化过了
    }
    
    qDebug() << "[FourViewDisplayServiceImpl] 延迟初始化VTK组件";
    initializeVTKComponents();
}

void FourViewDisplayServiceImpl::initializeVTKComponents()
{
    qDebug() << "[FourViewDisplayServiceImpl] 初始化VTK组件";
    
    // 创建一个占位符图像（1x1x1的空白图像）
    vtkSmartPointer<vtkImageData> placeholderImage = vtkSmartPointer<vtkImageData>::New();
    placeholderImage->SetDimensions(1, 1, 1);
    placeholderImage->AllocateScalars(VTK_UNSIGNED_CHAR, 1);
    unsigned char* ptr = static_cast<unsigned char*>(placeholderImage->GetScalarPointer());
    *ptr = 0;
    
    // 初始化3D渲染器
    m_3dRenderer = vtkSmartPointer<vtkRenderer>::New();
    m_3dRenderer->SetBackground(0.0, 0.0, 0.0);  // 设置黑色背景
    
    m_initialized = true;
    qDebug() << "[FourViewDisplayServiceImpl] VTK组件初始化完成（包含占位符图像）";
}

bool FourViewDisplayServiceImpl::loadImageFile(const QString& filePath)
{
    qDebug() << "[FourViewDisplayServiceImpl] 加载影像文件:" << filePath;
    
    if (filePath.isEmpty()) {
        m_lastError = "文件路径为空";
        qWarning() << "[FourViewDisplayServiceImpl]" << m_lastError;
        emit imageLoadFailed(filePath, m_lastError);
        emit serviceError(m_lastError);
        return false;
    }
    
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        m_lastError = "文件不存在";
        qWarning() << "[FourViewDisplayServiceImpl]" << m_lastError;
        emit imageLoadFailed(filePath, m_lastError);
        emit serviceError(m_lastError);
        return false;
    }
    
    try {
        // 使用ITK读取nii.gz文件
        typedef itk::Image<short, 3> ImageType;
        typedef itk::ImageFileReader<ImageType> ReaderType;
        typedef itk::ImageToVTKImageFilter<ImageType> ConnectorType;
        
        ReaderType::Pointer reader = ReaderType::New();
        reader->SetFileName(filePath.toStdString());
        
        ConnectorType::Pointer connector = ConnectorType::New();
        connector->SetInput(reader->GetOutput());
        
        try {
            connector->Update();
        } catch (const itk::ExceptionObject& e) {
            m_lastError = QString("ITK读取错误: %1").arg(e.GetDescription());
            qWarning() << "[FourViewDisplayServiceImpl]" << m_lastError;
            emit imageLoadFailed(filePath, m_lastError);
            emit serviceError(m_lastError);
            return false;
        }
        
        // 获取VTK影像数据
        m_imageData = vtkSmartPointer<vtkImageData>::New();
        m_imageData->DeepCopy(connector->GetOutput());
        
        m_currentFilePath = filePath;
        m_lastError.clear();
        
        updateAllViews();
        
        qDebug() << "[FourViewDisplayServiceImpl] 影像加载成功";
        emit imageLoaded(filePath);
        emit viewsUpdated();
        return true;
        
    } catch (const std::exception& e) {
        m_lastError = QString("加载异常: %1").arg(e.what());
        qWarning() << "[FourViewDisplayServiceImpl]" << m_lastError;
        emit imageLoadFailed(filePath, m_lastError);
        emit serviceError(m_lastError);
        return false;
    } catch (...) {
        m_lastError = "未知错误";
        qWarning() << "[FourViewDisplayServiceImpl]" << m_lastError;
        emit imageLoadFailed(filePath, m_lastError);
        emit serviceError(m_lastError);
        return false;
    }
}

bool FourViewDisplayServiceImpl::warmUpImageIOForFile(const QString& filePath)
{
    if (filePath.isEmpty()) {
        return false;
    }
    QFileInfo fi(filePath);
    if (!fi.exists()) {
        return false;
    }
    try {
        itk::ImageIOBase::Pointer imageIO = itk::ImageIOFactory::CreateImageIO(
            filePath.toStdString().c_str(), itk::ImageIOFactory::ReadMode);
        if (!imageIO) {
            return false;
        }
        imageIO->SetFileName(filePath.toStdString());
        imageIO->ReadImageInformation(); // 只读头信息
        return true;
    } catch (...) {
        return false;
    }
}

QString FourViewDisplayServiceImpl::extractPatientId(const QString& filePath) const
{
    QString norm = QDir::fromNativeSeparators(filePath);
    int idx = norm.indexOf("/patient_data/");
    if (idx < 0) return QString();
    int start = idx + QString("/patient_data/").length();
    int end = norm.indexOf('/', start);
    if (end < 0) return QString();
    return norm.mid(start, end - start);
}

QString FourViewDisplayServiceImpl::cacheDirForPatient(const QString& patientId) const
{
    QString base = QCoreApplication::applicationDirPath() + "/cache/images";
    return base + "/" + patientId;
}

QString FourViewDisplayServiceImpl::cacheFilePathForImage(const QString& filePath) const
{
    QString patientId = extractPatientId(filePath);
    if (patientId.isEmpty()) return QString();
    QFileInfo fi(filePath);
    QString stem = fi.completeBaseName();
    stem.replace(".nii", "");
    return cacheDirForPatient(patientId) + "/" + stem + ".vti";
}

vtkImageData* FourViewDisplayServiceImpl::tryReadDiskCache(const QString& filePath)
{
    QString vti = cacheFilePathForImage(filePath);
    if (vti.isEmpty() || !QFileInfo::exists(vti)) return nullptr;
    vtkXMLImageDataReader* reader = vtkXMLImageDataReader::New();
    reader->SetFileName(vti.toUtf8().constData());
    reader->Update();
    vtkImageData* data = vtkImageData::New();
    data->DeepCopy(reader->GetOutput());
    reader->Delete();
    return data;
}

void FourViewDisplayServiceImpl::writeDiskCache(const QString& filePath, vtkImageData* imageData)
{
    QString patientId = extractPatientId(filePath);
    if (patientId.isEmpty() || !imageData) return;
    QString dir = cacheDirForPatient(patientId);
    QDir().mkpath(dir);
    QString vti = cacheFilePathForImage(filePath);
    if (vti.isEmpty()) return;
    vtkXMLImageDataWriter* writer = vtkXMLImageDataWriter::New();
    writer->SetFileName(vti.toUtf8().constData());
    writer->SetInputData(imageData);
    writer->SetDataModeToAppended();
    writer->EncodeAppendedDataOff();
    writer->Write();
    writer->Delete();
}

// ==================== 3D Mesh 缓存 ====================
QString FourViewDisplayServiceImpl::meshCacheFilePath(const QString& imagePath) const
{
    QString patientId = extractPatientId(imagePath);
    if (patientId.isEmpty()) return QString();
    
    QString cacheDir = cacheDirForPatient(patientId);
    QFileInfo fi(imagePath);
    QString baseName = fi.completeBaseName(); // 去掉所有扩展名
    return cacheDir + "/" + baseName + "_mesh_cache.vtp";
}

vtkPolyData* FourViewDisplayServiceImpl::tryReadMeshCache(const QString& imagePath)
{
    QString cachePath = meshCacheFilePath(imagePath);
    if (cachePath.isEmpty()) return nullptr;
    
    QFileInfo cacheInfo(cachePath);
    if (!cacheInfo.exists()) {
        return nullptr;
    }
    
    // 检查缓存是否比原文件新
    QFileInfo origInfo(imagePath);
    if (origInfo.lastModified() > cacheInfo.lastModified()) {
        qDebug() << "[FourViewDisplayServiceImpl] Mesh 缓存过期，将重新生成";
        return nullptr;
    }
    
    try {
        vtkNew<vtkXMLPolyDataReader> reader;
        reader->SetFileName(cachePath.toStdString().c_str());
        reader->Update();
        
        vtkPolyData* polyData = vtkPolyData::New();
        polyData->DeepCopy(reader->GetOutput());
        
        qDebug() << "[FourViewDisplayServiceImpl] ✓ Mesh 缓存读取成功:" << cachePath;
        return polyData;
    } catch (...) {
        qWarning() << "[FourViewDisplayServiceImpl] Mesh 缓存读取失败:" << cachePath;
        return nullptr;
    }
}

void FourViewDisplayServiceImpl::writeMeshCache(const QString& imagePath, vtkPolyData* polyData)
{
    if (!polyData) return;
    
    QString cachePath = meshCacheFilePath(imagePath);
    if (cachePath.isEmpty()) return;
    
    QFileInfo fi(cachePath);
    QDir dir = fi.absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    try {
        vtkNew<vtkXMLPolyDataWriter> writer;
        writer->SetFileName(cachePath.toStdString().c_str());
        writer->SetInputData(polyData);
        writer->SetDataModeToBinary(); // 二进制模式更快
        writer->SetCompressorTypeToZLib(); // 压缩节省空间
        writer->Write();
        
        qDebug() << "[FourViewDisplayServiceImpl] ✓ Mesh 缓存写入成功:" << cachePath;
    } catch (...) {
        qWarning() << "[FourViewDisplayServiceImpl] Mesh 缓存写入失败:" << cachePath;
    }
}

vtkImageData* FourViewDisplayServiceImpl::readImageDataFromFile(const QString& filePath)
{
    qDebug() << "[FourViewDisplayServiceImpl] ========== readImageDataFromFile 开始 ==========" << filePath;
    
    try {
        qDebug() << "[FourViewDisplayServiceImpl] Step A: 文件路径检查";
    if (filePath.isEmpty()) {
        qWarning() << "[FourViewDisplayServiceImpl] 文件路径为空";
        return nullptr;
    }
    
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        qWarning() << "[FourViewDisplayServiceImpl] 文件不存在:" << filePath;
        return nullptr;
    }
        qDebug() << "[FourViewDisplayServiceImpl] Step B: 文件存在，检查内存缓存";

        // 先尝试内存缓存
        vtkImageData* cached = nullptr;
        try {
            qDebug() << "[FourViewDisplayServiceImpl] Step B.1: 调用 getCachedImageClone";
            cached = getCachedImageClone(filePath);
            qDebug() << "[FourViewDisplayServiceImpl] Step B.2: getCachedImageClone 返回:" << (cached ? "有缓存" : "无缓存");
        } catch (const std::exception& e) {
            qWarning() << "[FourViewDisplayServiceImpl] 内存缓存检查异常:" << e.what();
        } catch (...) {
            qWarning() << "[FourViewDisplayServiceImpl] 内存缓存检查未知异常";
        }
        
        if (cached) {
            qDebug() << "[FourViewDisplayServiceImpl] ✓ 命中内存缓存，直接返回";
            return cached;
        }
        qDebug() << "[FourViewDisplayServiceImpl] Step C: 内存缓存未命中，检查磁盘缓存";

        // 再尝试磁盘缓存（暂时禁用，可能导致崩溃）
        qDebug() << "[FourViewDisplayServiceImpl] Step C.1: 跳过磁盘缓存检查（调试）";
        /*
        vtkImageData* diskCached = nullptr;
        try {
            diskCached = tryReadDiskCache(filePath);
        } catch (const std::exception& e) {
            qWarning() << "[FourViewDisplayServiceImpl] 磁盘缓存读取异常:" << e.what();
        } catch (...) {
            qWarning() << "[FourViewDisplayServiceImpl] 磁盘缓存读取未知异常";
        }
        
        if (diskCached) {
            qDebug() << "[FourViewDisplayServiceImpl] ✓ 命中磁盘缓存(.vti)";
            try {
                storeImageInCache(filePath, diskCached);
            } catch (...) {
                qWarning() << "[FourViewDisplayServiceImpl] 存储到内存缓存失败（忽略）";
            }
            return diskCached;
        }
        */
        
        qDebug() << "[FourViewDisplayServiceImpl] Step D: 缓存未命中，开始 ITK 读取...";
    } catch (const std::exception& e) {
        qCritical() << "[FourViewDisplayServiceImpl] 读取前置检查异常:" << e.what();
        return nullptr;
    } catch (...) {
        qCritical() << "[FourViewDisplayServiceImpl] 读取前置检查未知异常";
        return nullptr;
    }

    // 使用 ITK ImageFileReader + ImageToVTKImageFilter（自动检测数据类型）
    qDebug() << "[FourViewDisplayServiceImpl] Step D: 使用 ITK Reader + Filter 方案";
    
    // Step D.0: 先检测图像的实际数据类型
    qDebug() << "[FourViewDisplayServiceImpl] Step D.0: 检测图像数据类型";
    itk::ImageIOBase::Pointer imageIO = nullptr;
    try {
        imageIO = itk::ImageIOFactory::CreateImageIO(
            filePath.toStdString().c_str(), itk::ImageIOFactory::ReadMode);
        if (!imageIO) {
            qCritical() << "[FourViewDisplayServiceImpl] 无法创建 ImageIO 对象";
            return nullptr;
        }
        imageIO->SetFileName(filePath.toStdString());
        imageIO->ReadImageInformation();
        
        itk::ImageIOBase::IOComponentType componentType = imageIO->GetComponentType();
        unsigned int numDimensions = imageIO->GetNumberOfDimensions();
        
        qDebug() << "[FourViewDisplayServiceImpl] 图像维度:" << numDimensions;
        qDebug() << "[FourViewDisplayServiceImpl] 数据类型:" << QString::fromStdString(imageIO->GetComponentTypeAsString(componentType));
        
        if (numDimensions != 3) {
            qCritical() << "[FourViewDisplayServiceImpl] 不支持的图像维度:" << numDimensions;
            return nullptr;
        }
        
        // 根据数据类型使用不同的模板实例化
        vtkImageData* resultImage = nullptr;
        
        switch (componentType) {
            case itk::ImageIOBase::UCHAR: {
                qDebug() << "[FourViewDisplayServiceImpl] 使用 unsigned char 类型读取";
                using ImageType = itk::Image<unsigned char, 3>;
                using ReaderType = itk::ImageFileReader<ImageType>;
                using FilterType = itk::ImageToVTKImageFilter<ImageType>;
        
        ReaderType::Pointer reader = ReaderType::New();
        reader->SetFileName(filePath.toStdString());
        
                const unsigned int prevThreads = itk::MultiThreaderBase::GetGlobalDefaultNumberOfThreads();
                itk::MultiThreaderBase::SetGlobalDefaultNumberOfThreads(1);
                
                reader->Update();
                
                FilterType::Pointer filter = FilterType::New();
                filter->SetInput(reader->GetOutput());
                filter->Update();
                
                itk::MultiThreaderBase::SetGlobalDefaultNumberOfThreads(prevThreads);
                
                resultImage = filter->GetOutput();
                if (resultImage) {
                    resultImage->Register(nullptr);
                }
                break;
            }
            case itk::ImageIOBase::SHORT: {
                qDebug() << "[FourViewDisplayServiceImpl] 使用 short 类型读取";
                using ImageType = itk::Image<short, 3>;
                using ReaderType = itk::ImageFileReader<ImageType>;
                using FilterType = itk::ImageToVTKImageFilter<ImageType>;
                
                ReaderType::Pointer reader = ReaderType::New();
                reader->SetFileName(filePath.toStdString());
                
                const unsigned int prevThreads = itk::MultiThreaderBase::GetGlobalDefaultNumberOfThreads();
                itk::MultiThreaderBase::SetGlobalDefaultNumberOfThreads(1);
                
                reader->Update();
                
                FilterType::Pointer filter = FilterType::New();
                filter->SetInput(reader->GetOutput());
                filter->Update();
                
                itk::MultiThreaderBase::SetGlobalDefaultNumberOfThreads(prevThreads);
                
                resultImage = filter->GetOutput();
                if (resultImage) {
                    resultImage->Register(nullptr);
                }
                break;
            }
            case itk::ImageIOBase::USHORT: {
                qDebug() << "[FourViewDisplayServiceImpl] 使用 unsigned short 类型读取";
                using ImageType = itk::Image<unsigned short, 3>;
                using ReaderType = itk::ImageFileReader<ImageType>;
                using FilterType = itk::ImageToVTKImageFilter<ImageType>;
                
                ReaderType::Pointer reader = ReaderType::New();
                reader->SetFileName(filePath.toStdString());
                
                const unsigned int prevThreads = itk::MultiThreaderBase::GetGlobalDefaultNumberOfThreads();
                itk::MultiThreaderBase::SetGlobalDefaultNumberOfThreads(1);
                
                reader->Update();
                
                FilterType::Pointer filter = FilterType::New();
                filter->SetInput(reader->GetOutput());
                filter->Update();
                
                itk::MultiThreaderBase::SetGlobalDefaultNumberOfThreads(prevThreads);
                
                resultImage = filter->GetOutput();
                if (resultImage) {
                    resultImage->Register(nullptr);
                }
                break;
            }
            case itk::ImageIOBase::INT: {
                qDebug() << "[FourViewDisplayServiceImpl] 使用 int 类型读取";
                using ImageType = itk::Image<int, 3>;
                using ReaderType = itk::ImageFileReader<ImageType>;
                using FilterType = itk::ImageToVTKImageFilter<ImageType>;
                
                ReaderType::Pointer reader = ReaderType::New();
                reader->SetFileName(filePath.toStdString());
                
                const unsigned int prevThreads = itk::MultiThreaderBase::GetGlobalDefaultNumberOfThreads();
                itk::MultiThreaderBase::SetGlobalDefaultNumberOfThreads(1);
                
                reader->Update();
                
                FilterType::Pointer filter = FilterType::New();
                filter->SetInput(reader->GetOutput());
                filter->Update();
                
                itk::MultiThreaderBase::SetGlobalDefaultNumberOfThreads(prevThreads);
                
                resultImage = filter->GetOutput();
                if (resultImage) {
                    resultImage->Register(nullptr);
                }
                break;
            }
            case itk::ImageIOBase::FLOAT: {
                qDebug() << "[FourViewDisplayServiceImpl] 使用 float 类型读取";
                using ImageType = itk::Image<float, 3>;
                using ReaderType = itk::ImageFileReader<ImageType>;
                using FilterType = itk::ImageToVTKImageFilter<ImageType>;
                
                ReaderType::Pointer reader = ReaderType::New();
                reader->SetFileName(filePath.toStdString());
                
                const unsigned int prevThreads = itk::MultiThreaderBase::GetGlobalDefaultNumberOfThreads();
                itk::MultiThreaderBase::SetGlobalDefaultNumberOfThreads(1);
                
                reader->Update();
                
                FilterType::Pointer filter = FilterType::New();
                filter->SetInput(reader->GetOutput());
                filter->Update();
                
                itk::MultiThreaderBase::SetGlobalDefaultNumberOfThreads(prevThreads);
                
                resultImage = filter->GetOutput();
                if (resultImage) {
                    resultImage->Register(nullptr);
                }
                break;
            }
            default:
                qCritical() << "[FourViewDisplayServiceImpl] 不支持的数据类型:" 
                           << QString::fromStdString(imageIO->GetComponentTypeAsString(componentType));
                return nullptr;
        }
        
        if (resultImage) {
            qDebug() << "[FourViewDisplayServiceImpl] ✓ ITK Reader+Filter 读取成功";
            storeImageInCache(filePath, resultImage);
            qDebug() << "[FourViewDisplayServiceImpl] ✓ 已存储到内存缓存";
            return resultImage;
        }
        
    } catch (const itk::ExceptionObject& e) {
        qCritical() << "[FourViewDisplayServiceImpl] ITK Reader 异常:" << e.GetDescription();
    } catch (const std::exception& e) {
        qCritical() << "[FourViewDisplayServiceImpl] 标准异常:" << e.what();
    } catch (...) {
        qCritical() << "[FourViewDisplayServiceImpl] 未知异常";
    }
    
    // 回退方案：ITK ImageIO 直接读取
    qDebug() << "[FourViewDisplayServiceImpl] 回退到 ImageIO 方案";
    try {
        itk::ImageIOBase::Pointer imageIO = itk::ImageIOFactory::CreateImageIO(
            filePath.toStdString().c_str(), itk::ImageIOFactory::ReadMode);
        if (imageIO) {
            imageIO->SetFileName(filePath.toStdString());
            imageIO->ReadImageInformation();
            const unsigned int nd = imageIO->GetNumberOfDimensions();
            if (nd == 3) {
                int width  = static_cast<int>(imageIO->GetDimensions(0));
                int height = static_cast<int>(imageIO->GetDimensions(1));
                int depth  = static_cast<int>(imageIO->GetDimensions(2));
                qDebug() << "[FourViewDisplayServiceImpl] ITK 读取尺寸:" << width << "x" << height << "x" << depth;
                if (width > 0 && height > 0 && depth > 0) {
                    qDebug() << "[FourViewDisplayServiceImpl] Step D.1: 计算缓冲区大小";
                    const size_t voxelCount = static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth);
                    const size_t bytesPerComponent = imageIO->GetComponentSize();
                    const size_t numComponents = imageIO->GetNumberOfComponents();
                    const size_t totalBytes = voxelCount * bytesPerComponent * numComponents;
                    qDebug() << "[FourViewDisplayServiceImpl] Step D.2: 分配" << totalBytes << "字节缓冲区";
                    std::vector<unsigned char> raw(totalBytes);
                    qDebug() << "[FourViewDisplayServiceImpl] Step D.3: 缓冲区分配完成";

                    // 单线程读取避免 gzip 解压不稳定
                    qDebug() << "[FourViewDisplayServiceImpl] Step D.4: 设置单线程读取";
                    const unsigned int prevThreads = itk::MultiThreaderBase::GetGlobalDefaultNumberOfThreads();
                    itk::MultiThreaderBase::SetGlobalDefaultNumberOfThreads(1);
                    qDebug() << "[FourViewDisplayServiceImpl] Step D.5: 开始读取数据";
                    try {
                        using RegionType = itk::ImageIORegion;
                        RegionType region(nd);
                        for (unsigned int i = 0; i < nd; ++i) {
                            region.SetSize(i, imageIO->GetDimensions(i));
                            region.SetIndex(i, 0);
                        }
                        imageIO->SetIORegion(region);
                        qDebug() << "[FourViewDisplayServiceImpl] Step D.6: 调用 imageIO->Read()";
                        imageIO->Read(raw.data());
                        qDebug() << "[FourViewDisplayServiceImpl] Step D.7: imageIO->Read() 完成";
        } catch (const itk::ExceptionObject& e) {
                        itk::MultiThreaderBase::SetGlobalDefaultNumberOfThreads(prevThreads);
                        qCritical() << "[FourViewDisplayServiceImpl] ITK读取异常:" << e.GetDescription();
                        throw;
                    } catch (const std::exception& e) {
                        itk::MultiThreaderBase::SetGlobalDefaultNumberOfThreads(prevThreads);
                        qCritical() << "[FourViewDisplayServiceImpl] 标准异常:" << e.what();
                        throw;
                    } catch (...) {
                        itk::MultiThreaderBase::SetGlobalDefaultNumberOfThreads(prevThreads);
                        qCritical() << "[FourViewDisplayServiceImpl] 未知异常";
                        throw;
                    }
                    itk::MultiThreaderBase::SetGlobalDefaultNumberOfThreads(prevThreads);
                    qDebug() << "[FourViewDisplayServiceImpl] Step D.8: 线程数已恢复";

                    // 转换为 VTK 格式
                    qDebug() << "[FourViewDisplayServiceImpl] Step D.9: 创建 vtkImageData";
                    vtkImageData* imageData = vtkImageData::New();
                    qDebug() << "[FourViewDisplayServiceImpl] Step D.10: 设置 Extent";
                    imageData->SetExtent(0, width - 1, 0, height - 1, 0, depth - 1);
                    qDebug() << "[FourViewDisplayServiceImpl] Step D.11: 分配 VTK 标量";
                    imageData->AllocateScalars(VTK_SHORT, 1);
                    qDebug() << "[FourViewDisplayServiceImpl] Step D.12: VTK 标量分配完成";

                    short* dst = static_cast<short*>(imageData->GetScalarPointer());
                    const itk::ImageIOBase::IOComponentType ct = imageIO->GetComponentType();
                    qDebug() << "[FourViewDisplayServiceImpl] Step D.13: 开始数据复制，类型=" << static_cast<int>(ct);
                    if (ct == itk::ImageIOBase::SHORT) {
                        std::memcpy(dst, raw.data(), voxelCount * sizeof(short));
                        qDebug() << "[FourViewDisplayServiceImpl] Step D.14: memcpy 完成";
                    } else if (ct == itk::ImageIOBase::USHORT) {
                        const unsigned short* src = reinterpret_cast<const unsigned short*>(raw.data());
                        for (size_t i = 0; i < voxelCount; ++i) dst[i] = static_cast<short>(src[i]);
                    } else if (ct == itk::ImageIOBase::CHAR) {
                        const char* src = reinterpret_cast<const char*>(raw.data());
                        for (size_t i = 0; i < voxelCount; ++i) dst[i] = static_cast<short>(src[i]);
                    } else if (ct == itk::ImageIOBase::UCHAR) {
                        const unsigned char* src = reinterpret_cast<const unsigned char*>(raw.data());
                        for (size_t i = 0; i < voxelCount; ++i) dst[i] = static_cast<short>(src[i]);
                    } else if (ct == itk::ImageIOBase::INT) {
                        const int* src = reinterpret_cast<const int*>(raw.data());
                        for (size_t i = 0; i < voxelCount; ++i) dst[i] = static_cast<short>(src[i]);
                    } else if (ct == itk::ImageIOBase::FLOAT) {
                        const float* src = reinterpret_cast<const float*>(raw.data());
                        for (size_t i = 0; i < voxelCount; ++i) dst[i] = static_cast<short>(src[i]);
                    } else if (ct == itk::ImageIOBase::DOUBLE) {
                        const double* src = reinterpret_cast<const double*>(raw.data());
                        for (size_t i = 0; i < voxelCount; ++i) dst[i] = static_cast<short>(src[i]);
                    } else {
                        imageData->Delete();
                        throw std::runtime_error("不支持的像素类型");
                    }

                    qDebug() << "[FourViewDisplayServiceImpl] Step D.15: 设置 Spacing/Origin";
                    // 设置Spacing/Origin
                    double spacing[3] = {1.0, 1.0, 1.0};
                    double origin[3] = {0.0, 0.0, 0.0};
                    for (unsigned int i = 0; i < 3; ++i) spacing[i] = imageIO->GetSpacing(i);
                    for (unsigned int i = 0; i < 3; ++i) origin[i] = imageIO->GetOrigin(i);
                    imageData->SetSpacing(spacing);
                    imageData->SetOrigin(origin);
                    qDebug() << "[FourViewDisplayServiceImpl] Step D.16: Spacing/Origin 设置完成";

                    qDebug() << "[FourViewDisplayServiceImpl] Step D.17: 存储到内存缓存";
                    // 缓存（暂时禁用磁盘缓存写入）
                    storeImageInCache(filePath, imageData);
                    qDebug() << "[FourViewDisplayServiceImpl] Step D.18: 内存缓存完成";
                    // writeDiskCache(filePath, imageData);  // 暂时禁用
                    qDebug() << "[FourViewDisplayServiceImpl] ✓ ITK 读取成功并已缓存";
                    return imageData;
                }
            }
        }
    } catch (const std::exception& e) {
        qWarning() << "[FourViewDisplayServiceImpl] ITK ImageIO 直接读取失败:" << e.what();
    } catch (...) {
        qWarning() << "[FourViewDisplayServiceImpl] ITK ImageIO 直接读取未知失败";
    }

    // 低层次ITK ImageIO读取（不走ImageFileReader管线），单线程，尽量避免不稳定因素
    try {
        itk::ImageIOBase::Pointer imageIO = itk::ImageIOFactory::CreateImageIO(
            filePath.toStdString().c_str(), itk::ImageIOFactory::ReadMode);
        if (imageIO) {
            imageIO->SetFileName(filePath.toStdString());
            imageIO->ReadImageInformation();
            const unsigned int nd = imageIO->GetNumberOfDimensions();
            if (nd != 3) {
                throw std::runtime_error("仅支持3D影像");
            }
            int width  = static_cast<int>(imageIO->GetDimensions(0));
            int height = static_cast<int>(imageIO->GetDimensions(1));
            int depth  = static_cast<int>(imageIO->GetDimensions(2));
            if (width <= 0 || height <= 0 || depth <= 0) {
                throw std::runtime_error("非法尺寸");
            }

            // 读取原始组件到临时缓冲
            const size_t bytesPerComponent = imageIO->GetComponentSize();
            const size_t numComponents = imageIO->GetNumberOfComponents();
            const size_t voxelCount = static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth);
            const size_t totalBytes = voxelCount * bytesPerComponent * numComponents;
            std::vector<unsigned char> raw(totalBytes);

            // 单线程读取
            const unsigned int prevThreads = itk::MultiThreaderBase::GetGlobalDefaultNumberOfThreads();
            itk::MultiThreaderBase::SetGlobalDefaultNumberOfThreads(1);
            try {
                using RegionType = itk::ImageIORegion;
                RegionType region(nd);
                for (unsigned int i = 0; i < nd; ++i) {
                    region.SetSize(i, imageIO->GetDimensions(i));
                    region.SetIndex(i, 0);
                }
                imageIO->SetIORegion(region);
                imageIO->Read(raw.data());
            } catch (...) {
                itk::MultiThreaderBase::SetGlobalDefaultNumberOfThreads(prevThreads);
                throw;
            }
            itk::MultiThreaderBase::SetGlobalDefaultNumberOfThreads(prevThreads);

            // 转为short（必要时做组件类型转换）
            vtkImageData* imageData = vtkImageData::New();
            imageData->SetExtent(0, width - 1, 0, height - 1, 0, depth - 1);
            imageData->AllocateScalars(VTK_SHORT, 1);
            short* dst = static_cast<short*>(imageData->GetScalarPointer());

            const itk::ImageIOBase::IOComponentType ct = imageIO->GetComponentType();
            if (ct == itk::ImageIOBase::SHORT) {
                std::memcpy(dst, raw.data(), voxelCount * sizeof(short));
            } else if (ct == itk::ImageIOBase::USHORT) {
                const unsigned short* src = reinterpret_cast<const unsigned short*>(raw.data());
                for (size_t i = 0; i < voxelCount; ++i) dst[i] = static_cast<short>(src[i]);
            } else if (ct == itk::ImageIOBase::CHAR) {
                const char* src = reinterpret_cast<const char*>(raw.data());
                for (size_t i = 0; i < voxelCount; ++i) dst[i] = static_cast<short>(src[i]);
            } else if (ct == itk::ImageIOBase::UCHAR) {
                const unsigned char* src = reinterpret_cast<const unsigned char*>(raw.data());
                for (size_t i = 0; i < voxelCount; ++i) dst[i] = static_cast<short>(src[i]);
            } else if (ct == itk::ImageIOBase::INT) {
                const int* src = reinterpret_cast<const int*>(raw.data());
                for (size_t i = 0; i < voxelCount; ++i) dst[i] = static_cast<short>(src[i]);
            } else if (ct == itk::ImageIOBase::FLOAT) {
                const float* src = reinterpret_cast<const float*>(raw.data());
                for (size_t i = 0; i < voxelCount; ++i) dst[i] = static_cast<short>(src[i]);
            } else if (ct == itk::ImageIOBase::DOUBLE) {
                const double* src = reinterpret_cast<const double*>(raw.data());
                for (size_t i = 0; i < voxelCount; ++i) dst[i] = static_cast<short>(src[i]);
            } else {
                imageData->Delete();
                throw std::runtime_error("不支持的像素类型");
            }

            // 间距/原点
            double spacing[3] = {1.0, 1.0, 1.0};
            double origin[3] = {0.0, 0.0, 0.0};
            for (unsigned int i = 0; i < 3; ++i) spacing[i] = imageIO->GetSpacing(i);
            for (unsigned int i = 0; i < 3; ++i) origin[i] = imageIO->GetOrigin(i);
            imageData->SetSpacing(spacing);
            imageData->SetOrigin(origin);

            storeImageInCache(filePath, imageData);
            writeDiskCache(filePath, imageData);
            qDebug() << "[FourViewDisplayServiceImpl] ITK ImageIO 直接读取成功";
            return imageData;
        }
    } catch (const std::exception& e) {
        qWarning() << "[FourViewDisplayServiceImpl] ITK ImageIO 直接读取失败:" << e.what();
    } catch (...) {
        qWarning() << "[FourViewDisplayServiceImpl] ITK ImageIO 直接读取未知失败";
    }

    try {
        // 使用ITK读取nii.gz文件（纯ITK读取，不使用VTK桥接过滤器，避免在后台线程初始化VTK管线）
        typedef itk::Image<short, 3> ImageType;
        typedef itk::ImageFileReader<ImageType> ReaderType;
        
        qDebug() << "[FourViewDisplayServiceImpl] 使用ITK Reader读取:" << filePath;
        ReaderType::Pointer reader = ReaderType::New();
        reader->SetFileName(filePath.toStdString());
        
        // 为避免首次解压/IO初始化中的潜在多线程不稳定，强制单线程读取
        const unsigned int prevThreads = itk::MultiThreaderBase::GetGlobalDefaultNumberOfThreads();
        itk::MultiThreaderBase::SetGlobalDefaultNumberOfThreads(1);
        try {
            reader->Update();
        } catch (...) {
            itk::MultiThreaderBase::SetGlobalDefaultNumberOfThreads(prevThreads);
            throw;
        }
        itk::MultiThreaderBase::SetGlobalDefaultNumberOfThreads(prevThreads);
        ImageType::Pointer itkImage = reader->GetOutput();
        
        ImageType::RegionType region = itkImage->GetLargestPossibleRegion();
        ImageType::SizeType size = region.GetSize();
        ImageType::SpacingType spacing = itkImage->GetSpacing();
        ImageType::PointType origin = itkImage->GetOrigin();
        
        const int width = static_cast<int>(size[0]);
        const int height = static_cast<int>(size[1]);
        const int depth = static_cast<int>(size[2]);
        
        if (width <= 0 || height <= 0 || depth <= 0) {
            qWarning() << "[FourViewDisplayServiceImpl] 非法图像尺寸:" << width << height << depth;
            return nullptr;
        }
        
        vtkImageData* imageData = vtkImageData::New();
        imageData->SetExtent(0, width - 1, 0, height - 1, 0, depth - 1);
        imageData->SetSpacing(spacing[0], spacing[1], spacing[2]);
        imageData->SetOrigin(origin[0], origin[1], origin[2]);
        imageData->AllocateScalars(VTK_SHORT, 1);
        
        const size_t voxelCount = static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth);
        void* dstPtr = imageData->GetScalarPointer();
        const void* srcPtr = static_cast<const void*>(itkImage->GetBufferPointer());
        
        if (!dstPtr || !srcPtr) {
            qWarning() << "[FourViewDisplayServiceImpl] 缓冲区指针为空，无法拷贝数据";
            imageData->Delete();
            return nullptr;
        }
        
        std::memcpy(dstPtr, srcPtr, voxelCount * sizeof(short));

        // 写入缓存（内存+磁盘）
        storeImageInCache(filePath, imageData);
        writeDiskCache(filePath, imageData);

        qDebug() << "[FourViewDisplayServiceImpl] 影像数据读取成功（ITK完成，已复制到VTK）";
        return imageData;
        
    } catch (const itk::ExceptionObject& e) {
        qWarning() << "[FourViewDisplayServiceImpl] ITK读取错误:" << e.GetDescription();
        return nullptr;
    } catch (const std::exception& e) {
        qWarning() << "[FourViewDisplayServiceImpl] 读取异常:" << e.what();
        return nullptr;
    } catch (...) {
        qWarning() << "[FourViewDisplayServiceImpl] 未知读取错误";
        return nullptr;
    }
}

vtkImageData* FourViewDisplayServiceImpl::getCachedImageClone(const QString& filePath)
{
    QString patientId = extractPatientId(filePath);
    // 使用空串作为无患者ID时的默认分组键
    QHash<QString, vtkSmartPointer<vtkImageData>>& imageMap = m_patientToImages[patientId];
    auto it = imageMap.find(filePath);
    if (it == imageMap.end()) return nullptr;
    // 返回缓存对象并增加引用计数，调用方可安全Delete一次
    vtkImageData* ptr = it.value();
    if (ptr) {
        ptr->Register(nullptr);
    }
    // 更新LRU顺序
    QStringList& order = m_patientCacheOrder[patientId];
    order.removeAll(filePath);
    order.prepend(filePath);
    return ptr;
}

void FourViewDisplayServiceImpl::storeImageInCache(const QString& filePath, vtkImageData* imageData)
{
    if (!imageData) return;
    QString patientId = extractPatientId(filePath);
    QHash<QString, vtkSmartPointer<vtkImageData>>& imageMap = m_patientToImages[patientId];
    QStringList& order = m_patientCacheOrder[patientId];

    // 共享同一数据对象：缓存与调用方共享底层内存（通过SmartPointer引用计数管理）
    vtkSmartPointer<vtkImageData> owned = imageData; // 增加引用
    imageMap.insert(filePath, owned);
    order.removeAll(filePath);
    order.prepend(filePath);

    // 容量控制（LRU淘汰）
    while (order.size() > m_cacheCapacityPerPatient) {
        QString evictKey = order.takeLast();
        imageMap.remove(evictKey);
    }
}

bool FourViewDisplayServiceImpl::updateViewsWithImageData(vtkImageData* imageData, const QString& filePath)
{
    qDebug() << "[FourViewDisplayServiceImpl] 更新VTK视图（主线程）:" << filePath;
    
    if (!imageData) {
        m_lastError = "影像数据为空";
        qWarning() << "[FourViewDisplayServiceImpl]" << m_lastError;
        emit imageLoadFailed(filePath, m_lastError);
        emit serviceError(m_lastError);
        return false;
    }
    
    try {
        // 保存影像数据到成员变量
        m_imageData = vtkSmartPointer<vtkImageData>::New();
        m_imageData->DeepCopy(imageData);
        
        // 释放传入的临时数据
        imageData->Delete();
        
        m_currentFilePath = filePath;
        m_lastError.clear();
        
        // 更新所有视图（涉及OpenGL操作，必须在主线程）
        updateAllViews();
        
        qDebug() << "[FourViewDisplayServiceImpl] VTK视图更新完成";
        emit imageLoaded(filePath);
        emit viewsUpdated();
        return true;
        
    } catch (const std::exception& e) {
        m_lastError = QString("视图更新异常: %1").arg(e.what());
        qWarning() << "[FourViewDisplayServiceImpl]" << m_lastError;
        emit imageLoadFailed(filePath, m_lastError);
        emit serviceError(m_lastError);
        return false;
    } catch (...) {
        m_lastError = "未知视图更新错误";
        qWarning() << "[FourViewDisplayServiceImpl]" << m_lastError;
        emit imageLoadFailed(filePath, m_lastError);
        emit serviceError(m_lastError);
        return false;
    }
}

void FourViewDisplayServiceImpl::requestVolumeData(double windowWidth, double windowLevel)
{
    ensureInitialized();

    if (!m_imageData)
    {
        const QString error = QStringLiteral("四视图影像尚未加载，无法提供体数据");
        qWarning() << "[FourViewDisplayServiceImpl]" << error;
        emit serviceError(error);
        return;
    }

    if (windowWidth >= 0.0)
    {
        m_windowWidth = windowWidth;
    }
    if (windowLevel >= 0.0)
    {
        m_windowLevel = windowLevel;
    }

    ImageData imageData;
    imageData.setVTKImage(m_imageData);

    QVariantMap metadata;
    metadata.insert(QStringLiteral("filePath"), m_currentFilePath);
    metadata.insert(QStringLiteral("windowWidth"), m_windowWidth);
    metadata.insert(QStringLiteral("windowLevel"), m_windowLevel);
    metadata.insert(QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc());
    imageData.setMetadata(metadata);

    emit volumeDataReady(imageData);
}

void FourViewDisplayServiceImpl::updateAllViewsProgressive(vtkRenderWindow* renderWindow3D)
{
    Q_UNUSED(renderWindow3D);
}

int FourViewDisplayServiceImpl::getAxialSlice() const
{
    return m_axialSlice;
}

int FourViewDisplayServiceImpl::getSagittalSlice() const
{
    return m_sagittalSlice;
}

int FourViewDisplayServiceImpl::getCoronalSlice() const
{
    return m_coronalSlice;
}

void FourViewDisplayServiceImpl::printCurrentViewParameters() const
{
    if (!m_imageData) {
        qDebug() << "[FourViewDisplayServiceImpl] 未加载影像数据";
        return;
    }

    qDebug() << "\n=======================================================";
    qDebug() << "【四视图显示参数】";

    // 输出影像基本信息
    int* dims = m_imageData->GetDimensions();
    double* spacing = m_imageData->GetSpacing();
    double* origin = m_imageData->GetOrigin();
    double* range = m_imageData->GetScalarRange();

    qDebug() << "【影像信息】";
    qDebug() << "  文件路径:" << m_currentFilePath;
    qDebug() << "  影像尺寸:" << dims[0] << "x" << dims[1] << "x" << dims[2];
    qDebug() << "  体素间距:" << spacing[0] << "," << spacing[1] << "," << spacing[2] << "(mm)";
    qDebug() << "  影像原点:" << origin[0] << "," << origin[1] << "," << origin[2];
    qDebug() << "  数据范围:" << range[0] << "~" << range[1];

    // 输出窗宽窗位
    qDebug() << "\n【窗宽窗位】";
    qDebug() << "  窗宽(WindowWidth):" << m_windowWidth;
    qDebug() << "  窗位(WindowLevel):" << m_windowLevel;

    // 输出切片索引
    qDebug() << "\n【切片索引】";
    qDebug() << "  轴位切片:" << m_axialSlice << " / " << (dims[2] - 1);
    qDebug() << "  矢状位切片:" << m_sagittalSlice << " / " << (dims[0] - 1);
    qDebug() << "  冠状位切片:" << m_coronalSlice << " / " << (dims[1] - 1);

    // 输出3D视图参数
    if (m_3dRenderer) {
        qDebug() << "\n【3D视图】";
        qDebug() << "  渲染模式:" << m_3dRenderMode;
        qDebug() << "  透明度:" << m_3dOpacity;
        qDebug() << "  颜色(RGB):" << m_3dColorR << "," << m_3dColorG << "," << m_3dColorB;

        if (m_3dRenderer->GetActiveCamera()) {
            vtkCamera* cam = m_3dRenderer->GetActiveCamera();
            double* pos = cam->GetPosition();
            double* focal = cam->GetFocalPoint();
            double* viewUp = cam->GetViewUp();
            double zoom = cam->GetParallelScale();
            double viewAngle = cam->GetViewAngle();

            qDebug() << "  相机位置:" << pos[0] << "," << pos[1] << "," << pos[2];
            qDebug() << "  焦点位置:" << focal[0] << "," << focal[1] << "," << focal[2];
            qDebug() << "  向上方向:" << viewUp[0] << "," << viewUp[1] << "," << viewUp[2];
            qDebug() << "  视角(ViewAngle):" << viewAngle;
            qDebug() << "  缩放比例(ParallelScale):" << zoom;
        }
    }

    qDebug() << "=======================================================\n";
}

// ========== 缺失方法的实现 ==========

bool FourViewDisplayServiceImpl::loadImageFiles(const QStringList& filePaths)
{
    qDebug() << "[FourViewDisplayServiceImpl] 加载多个影像文件，数量:" << filePaths.size();

    if (filePaths.isEmpty()) {
        m_lastError = "文件列表为空";
        qWarning() << "[FourViewDisplayServiceImpl]" << m_lastError;
        return false;
    }

    // 暂时只加载第一个文件
    return loadImageFile(filePaths.first());
}

bool FourViewDisplayServiceImpl::clearImage()
{
    qDebug() << "[FourViewDisplayServiceImpl] 清除影像";

    m_imageData = nullptr;
    m_currentFilePath.clear();
    m_lastError.clear();

    emit viewsUpdated();
    return true;
}

void FourViewDisplayServiceImpl::setWindowLevel(double windowWidth, double windowLevel)
{
    m_windowWidth = windowWidth;
    m_windowLevel = windowLevel;

    qDebug() << "[FourViewDisplayServiceImpl] 设置窗宽窗位:" << windowWidth << "/" << windowLevel;

    updateAllViews();
}

double FourViewDisplayServiceImpl::getWindowWidth() const
{
    return m_windowWidth;
}

double FourViewDisplayServiceImpl::getWindowLevel() const
{
    return m_windowLevel;
}

QString FourViewDisplayServiceImpl::getCurrentFilePath() const
{
    return m_currentFilePath;
}

bool FourViewDisplayServiceImpl::getImageDimensions(int& width, int& height, int& depth) const
{
    if (!m_imageData) {
        return false;
    }

    int* dims = m_imageData->GetDimensions();
    width = dims[0];
    height = dims[1];
    depth = dims[2];

    return true;
}

bool FourViewDisplayServiceImpl::getImageSpacing(double& spacingX, double& spacingY, double& spacingZ) const
{
    if (!m_imageData) {
        return false;
    }

    double* spacing = m_imageData->GetSpacing();
    spacingX = spacing[0];
    spacingY = spacing[1];
    spacingZ = spacing[2];

    return true;
}

bool FourViewDisplayServiceImpl::isImageLoaded() const
{
    return m_imageData != nullptr;
}

QString FourViewDisplayServiceImpl::getLastError() const
{
    return m_lastError;
}

void FourViewDisplayServiceImpl::updateAllViews()
{
    qDebug() << "[FourViewDisplayServiceImpl] 更新所有视图";

    if (!m_imageData) {
        qDebug() << "[FourViewDisplayServiceImpl] 未加载影像数据，跳过更新";
        return;
    }

    // 将影像数据传递给VTK Widget（如果已创建）
    if (m_vtkWidget) {
        qDebug() << "[FourViewDisplayServiceImpl] 将影像数据传递给VTK Widget";
        m_vtkWidget->loadImageData(m_imageData);
    }

    // 更新3D视图
    update3DView();

    emit viewsUpdated();
}

void FourViewDisplayServiceImpl::update3DView()
{
    qDebug() << "[FourViewDisplayServiceImpl] 更新3D视图";

    if (!m_imageData) {
        return;
    }

    ensureInitialized();

    // 根据渲染模式创建3D渲染
    if (m_3dRenderMode == "surface") {
        create3DSurfaceRendering();
    } else if (m_3dRenderMode == "volume") {
        create3DVolumeRendering();
    }

    // 触发3D视图渲染
    if (m_vtkWidget) {
        m_vtkWidget->render3DView();
    }
}

void FourViewDisplayServiceImpl::create3DSurfaceRendering()
{
    qDebug() << "[FourViewDisplayServiceImpl] 创建3D表面渲染";

    if (!m_imageData) {
        return;
    }

    // 获取要使用的渲染器：优先使用widget的渲染器，否则使用服务自己的渲染器
    vtkRenderer* renderer = nullptr;
    if (m_vtkWidget) {
        renderer = m_vtkWidget->get3DRenderer();
        qDebug() << "[FourViewDisplayServiceImpl] 使用Widget的3D渲染器";
    }
    if (!renderer) {
        renderer = m_3dRenderer;
        qDebug() << "[FourViewDisplayServiceImpl] 使用服务自己的3D渲染器";
    }
    if (!renderer) {
        qWarning() << "[FourViewDisplayServiceImpl] 没有可用的3D渲染器";
        return;
    }

    // 切换渲染模式时不要清空所有ViewProps，否则会把工具/分割模型也一起清掉
    if (m_3dVolume) {
        renderer->RemoveVolume(m_3dVolume);
        m_3dVolume = nullptr;
    }
    if (m_3dActor) {
        renderer->RemoveActor(m_3dActor);
    }

    // 使用Marching Cubes算法提取表面
    vtkSmartPointer<vtkMarchingCubes> surface = vtkSmartPointer<vtkMarchingCubes>::New();
    surface->SetInputData(m_imageData);
    surface->SetValue(0, 300);  // 设置阈值
    surface->Update();

    // 创建Mapper和Actor
    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(surface->GetOutputPort());
    mapper->ScalarVisibilityOff();

    m_3dActor = vtkSmartPointer<vtkActor>::New();
    m_3dActor->SetMapper(mapper);
    m_3dActor->GetProperty()->SetColor(m_3dColorR / 255.0, m_3dColorG / 255.0, m_3dColorB / 255.0);
    m_3dActor->GetProperty()->SetOpacity(m_3dOpacity);

    // 添加到渲染器
    renderer->AddActor(m_3dActor);
    if (m_toolActor && m_toolModelVisible && !renderer->HasViewProp(m_toolActor)) {
        renderer->AddActor(m_toolActor);
    }
    renderer->ResetCamera();
    renderer->ResetCameraClippingRange();
}

void FourViewDisplayServiceImpl::create3DVolumeRendering()
{
    qDebug() << "[FourViewDisplayServiceImpl] 创建3D体渲染";

    if (!m_imageData) {
        return;
    }

    // 获取要使用的渲染器：优先使用widget的渲染器，否则使用服务自己的渲染器
    vtkRenderer* renderer = nullptr;
    if (m_vtkWidget) {
        renderer = m_vtkWidget->get3DRenderer();
    }
    if (!renderer) {
        renderer = m_3dRenderer;
    }
    if (!renderer) {
        qWarning() << "[FourViewDisplayServiceImpl] 没有可用的3D渲染器";
        return;
    }

    // 切换渲染模式时不要清空所有ViewProps，否则会把工具/分割模型也一起清掉
    if (m_3dActor) {
        renderer->RemoveActor(m_3dActor);
        m_3dActor = nullptr;
    }
    if (m_3dVolume) {
        renderer->RemoveVolume(m_3dVolume);
    }

    // 创建体渲染Mapper
    vtkSmartPointer<vtkSmartVolumeMapper> volumeMapper = vtkSmartPointer<vtkSmartVolumeMapper>::New();
    volumeMapper->SetInputData(m_imageData);

    // 创建传输函数
    vtkSmartPointer<vtkPiecewiseFunction> opacityTransferFunction = vtkSmartPointer<vtkPiecewiseFunction>::New();
    opacityTransferFunction->AddPoint(0, 0.0);
    opacityTransferFunction->AddPoint(500, 0.15);
    opacityTransferFunction->AddPoint(1000, 0.85);
    opacityTransferFunction->AddPoint(1150, 1.0);

    vtkSmartPointer<vtkColorTransferFunction> colorTransferFunction = vtkSmartPointer<vtkColorTransferFunction>::New();
    colorTransferFunction->AddRGBPoint(0.0, 0.0, 0.0, 0.0);
    colorTransferFunction->AddRGBPoint(500.0, 1.0, 0.5, 0.3);
    colorTransferFunction->AddRGBPoint(1000.0, 1.0, 0.9, 0.8);
    colorTransferFunction->AddRGBPoint(1150.0, 1.0, 1.0, 1.0);

    // 创建体属性
    vtkSmartPointer<vtkVolumeProperty> volumeProperty = vtkSmartPointer<vtkVolumeProperty>::New();
    volumeProperty->SetColor(colorTransferFunction);
    volumeProperty->SetScalarOpacity(opacityTransferFunction);
    volumeProperty->ShadeOn();
    volumeProperty->SetInterpolationTypeToLinear();

    // 创建Volume
    m_3dVolume = vtkSmartPointer<vtkVolume>::New();
    m_3dVolume->SetMapper(volumeMapper);
    m_3dVolume->SetProperty(volumeProperty);

    // 添加到渲染器
    renderer->AddVolume(m_3dVolume);
    if (m_toolActor && m_toolModelVisible && !renderer->HasViewProp(m_toolActor)) {
        renderer->AddActor(m_toolActor);
    }
    renderer->ResetCamera();
    renderer->ResetCameraClippingRange();
}

void FourViewDisplayServiceImpl::set3DRenderMode(const QString& mode)
{
    m_3dRenderMode = mode;
    qDebug() << "[FourViewDisplayServiceImpl] 设置3D渲染模式:" << mode;
    update3DView();
}

void FourViewDisplayServiceImpl::set3DOpacity(double opacity)
{
    m_3dOpacity = opacity;
    qDebug() << "[FourViewDisplayServiceImpl] 设置3D透明度:" << opacity;

    if (m_3dActor) {
        m_3dActor->GetProperty()->SetOpacity(opacity);
    }

    // 同时更新纯VTK Widget
    if (m_vtkWidget) {
        m_vtkWidget->set3DOpacity(opacity);
    }
}

void FourViewDisplayServiceImpl::set3DColor(int r, int g, int b)
{
    m_3dColorR = r;
    m_3dColorG = g;
    m_3dColorB = b;

    qDebug() << "[FourViewDisplayServiceImpl] 设置3D颜色:" << r << "," << g << "," << b;

    if (m_3dActor) {
        m_3dActor->GetProperty()->SetColor(r / 255.0, g / 255.0, b / 255.0);
    }
}

void FourViewDisplayServiceImpl::create3DSurfaceRenderingProgressive(bool isPreview)
{
    Q_UNUSED(isPreview);
    create3DSurfaceRendering();
}

QWidget* FourViewDisplayServiceImpl::createFourViewVTKWidget(QWidget* parent)
{
    qDebug() << "[FourViewDisplayService] 创建纯VTK Widget（无控制UI）";
    try {
        m_vtkWidget = new FourViewVTKWidget(this, parent);
        return m_vtkWidget;
    } catch (const std::exception& e) {
        qCritical() << "[FourViewDisplayService] 创建VTK Widget异常:" << e.what();
        return nullptr;
    } catch (...) {
        qCritical() << "[FourViewDisplayService] 创建VTK Widget未知异常";
        return nullptr;
    }
}

void FourViewDisplayServiceImpl::setAxialSlice(int slice)
{
    m_axialSlice = slice;
    if (m_vtkWidget) {
        m_vtkWidget->setAxialSlice(slice);
    }
}

void FourViewDisplayServiceImpl::setSagittalSlice(int slice)
{
    m_sagittalSlice = slice;
    if (m_vtkWidget) {
        m_vtkWidget->setSagittalSlice(slice);
    }
}

void FourViewDisplayServiceImpl::setCoronalSlice(int slice)
{
    m_coronalSlice = slice;
    if (m_vtkWidget) {
        m_vtkWidget->setCoronalSlice(slice);
    }
}

void FourViewDisplayServiceImpl::resetViews()
{
    if (m_vtkWidget) {
        m_vtkWidget->resetViews();
    }
}

int FourViewDisplayServiceImpl::getAxialSliceMin() const
{
    return m_vtkWidget ? m_vtkWidget->getAxialSliceMin() : 0;
}

int FourViewDisplayServiceImpl::getAxialSliceMax() const
{
    return m_vtkWidget ? m_vtkWidget->getAxialSliceMax() : 0;
}

int FourViewDisplayServiceImpl::getSagittalSliceMin() const
{
    return m_vtkWidget ? m_vtkWidget->getSagittalSliceMin() : 0;
}

int FourViewDisplayServiceImpl::getSagittalSliceMax() const
{
    return m_vtkWidget ? m_vtkWidget->getSagittalSliceMax() : 0;
}

int FourViewDisplayServiceImpl::getCoronalSliceMin() const
{
    return m_vtkWidget ? m_vtkWidget->getCoronalSliceMin() : 0;
}

int FourViewDisplayServiceImpl::getCoronalSliceMax() const
{
    return m_vtkWidget ? m_vtkWidget->getCoronalSliceMax() : 0;
}

void FourViewDisplayServiceImpl::pauseRendering()
{
    if (m_vtkWidget) {
        m_vtkWidget->pauseVTKRendering();
    }
}

void FourViewDisplayServiceImpl::resumeRendering()
{
    if (m_vtkWidget) {
        m_vtkWidget->resumeVTKRendering();
        if (m_toolActor && m_toolModelVisible) {
            m_vtkWidget->ensureVTKInitialized();
            vtkRenderer* renderer = m_vtkWidget->get3DRenderer();
            if (renderer && !renderer->HasViewProp(m_toolActor)) {
                renderer->AddActor(m_toolActor);
                renderer->ResetCameraClippingRange();
            }
            m_vtkWidget->render3DView();
        }
    }
}

// ========== 导航工具位置叠加实现 ==========

void FourViewDisplayServiceImpl::updateToolPosition(double x, double y, double z)
{
    m_toolPosition[0] = x;
    m_toolPosition[1] = y;
    m_toolPosition[2] = z;

    vtkRenderer* renderer = nullptr;
    if (m_vtkWidget) {
        m_vtkWidget->ensureVTKInitialized();
        renderer = m_vtkWidget->get3DRenderer();
    }
    if (!renderer) {
        renderer = m_3dRenderer;
    }

    if (!m_toolActor || !renderer) {
        // 如果工具Actor不存在，创建默认的球形表示
        if (!m_toolSphere) {
            m_toolSphere = vtkSmartPointer<vtkSphereSource>::New();
            m_toolSphere->SetRadius(3.0);
            m_toolSphere->SetThetaResolution(16);
            m_toolSphere->SetPhiResolution(16);
        }
        m_toolSphere->SetCenter(x, y, z);
        m_toolSphere->Update();

        if (!m_toolActor) {
            vtkNew<vtkPolyDataMapper> mapper;
            mapper->SetInputConnection(m_toolSphere->GetOutputPort());

            m_toolActor = vtkSmartPointer<vtkActor>::New();
            m_toolActor->SetMapper(mapper);
            m_toolActor->GetProperty()->SetColor(1.0, 0.2, 0.2); // 红色
            m_toolActor->GetProperty()->SetOpacity(0.9);

            if (renderer && !renderer->HasViewProp(m_toolActor)) {
                renderer->AddActor(m_toolActor);
            }
        }
    } else {
        // 更新工具位置
        if (m_toolSphere) {
            m_toolSphere->SetCenter(x, y, z);
            m_toolSphere->Update();
        } else {
            m_toolActor->SetPosition(x, y, z);
        }
    }

    m_toolActor->SetVisibility(m_toolModelVisible);

    // 更新视图
    if (m_vtkWidget) {
        m_vtkWidget->updateToolCrosshair(x, y, z);
        m_vtkWidget->render3DView();
    }
}

void FourViewDisplayServiceImpl::setToolModelVisible(bool visible)
{
    m_toolModelVisible = visible;
    if (m_toolActor) {
        m_toolActor->SetVisibility(visible);
        if (m_vtkWidget) {
            m_vtkWidget->setToolCrosshairVisible(visible);
            m_vtkWidget->render3DView();
        }
    }
}

bool FourViewDisplayServiceImpl::loadToolModel(const QString& modelPath)
{
    QFileInfo fi(modelPath);
    if (!fi.exists()) {
        m_lastError = "工具模型文件不存在: " + modelPath;
        return false;
    }

    // 确保VTK组件已初始化
    ensureInitialized();

    // 加载STL模型
    vtkNew<vtkSTLReader> reader;
    reader->SetFileName(modelPath.toStdString().c_str());
    reader->Update();

    m_toolPolyData = reader->GetOutput();

    if (!m_toolPolyData || m_toolPolyData->GetNumberOfPoints() == 0) {
        m_lastError = "加载工具模型失败";
        return false;
    }

    // 创建Actor
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(m_toolPolyData);

    if (!m_toolActor) {
        m_toolActor = vtkSmartPointer<vtkActor>::New();
    }
    m_toolActor->SetMapper(mapper);
    m_toolActor->GetProperty()->SetColor(0.8, 0.9, 1.0); // 浅蓝色（骨骼颜色）
    m_toolActor->GetProperty()->SetOpacity(1.0);
    m_toolActor->GetProperty()->SetInterpolationToPhong();

    // 获取正确的渲染器：优先使用widget的渲染器
    vtkRenderer* renderer = nullptr;
    if (m_vtkWidget) {
        m_vtkWidget->ensureVTKInitialized();
        renderer = m_vtkWidget->get3DRenderer();
    }
    if (!renderer) {
        renderer = m_3dRenderer;
    }

    if (renderer) {
        renderer->AddActor(m_toolActor);
        // 重置相机以确保模型可见
        renderer->ResetCamera();
        renderer->ResetCameraClippingRange();
        qDebug() << "[FourViewDisplayServiceImpl] Actor已添加到渲染器";
    } else {
        qWarning() << "[FourViewDisplayServiceImpl] 无可用渲染器！";
    }

    // 清除球形表示
    m_toolSphere = nullptr;

    // 设置模型可见并触发渲染
    m_toolModelVisible = true;
    m_toolActor->SetVisibility(true);

    // 触发渲染更新
    if (m_vtkWidget) {
        m_vtkWidget->render3DView();
    }

    qDebug() << "[FourViewDisplayServiceImpl] 工具模型已加载:" << modelPath
             << "Points:" << m_toolPolyData->GetNumberOfPoints()
             << "Cells:" << m_toolPolyData->GetNumberOfCells();
    return true;
}

void FourViewDisplayServiceImpl::updateToolPose(const QList<double>& position, const QList<double>& orientation)
{
    if (position.size() < 3) return;

    m_toolPosition[0] = position[0];
    m_toolPosition[1] = position[1];
    m_toolPosition[2] = position[2];

    if (m_toolActor) {
        m_toolActor->SetPosition(position[0], position[1], position[2]);

        // 如果有方向信息，设置旋转
        if (orientation.size() >= 4) {
            // 四元数转欧拉角（简化实现）
            double qw = orientation[0];
            double qx = orientation[1];
            double qy = orientation[2];
            double qz = orientation[3];

            // 转换为度数
            double rx = atan2(2*(qw*qx + qy*qz), 1 - 2*(qx*qx + qy*qy)) * 180.0 / M_PI;
            double ry = asin(2*(qw*qy - qz*qx)) * 180.0 / M_PI;
            double rz = atan2(2*(qw*qz + qx*qy), 1 - 2*(qy*qy + qz*qz)) * 180.0 / M_PI;

            m_toolActor->SetOrientation(rx, ry, rz);
        }

        m_toolActor->SetVisibility(m_toolModelVisible);
    }

    if (m_vtkWidget) {
        m_vtkWidget->updateToolCrosshair(position[0], position[1], position[2]);
        m_vtkWidget->render3DView();
    }
}
