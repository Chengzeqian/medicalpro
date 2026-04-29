#include "SegmentationServiceImpl.h"
#include <QDebug>
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSettings>
#include <QTextStream>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSet>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QMetaType>
#include <QThread>
#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>  // For CREATE_NEW_CONSOLE
#endif

// 注册 QProcess::ExitStatus 以支持跨线程信号槽连接
static bool s_metaTypeRegistered = []() {
    qRegisterMetaType<QProcess::ExitStatus>("QProcess::ExitStatus");
    return true;
}();

static void appendToLogFileUtf8(const QString& filePath, const QString& text)
{
    if (filePath.isEmpty() || text.isEmpty()) {
        return;
    }

    // 将 tqdm 等使用的 '\r' 转成换行，避免 tail/ReadLine 长时间看不到输出
    QString normalized = text;
    normalized.replace("\r\n", "\n");
    normalized.replace('\r', '\n');

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }

    file.write(normalized.toUtf8());
}
#include <vtkPolyData.h>
#include <vtkImageData.h>
#include <vtkNIFTIImageReader.h>
#include <vtkMarchingCubes.h>
#include <vtkSmoothPolyDataFilter.h>
#include <vtkPolyDataNormals.h>
#include <vtkCleanPolyData.h>
#include <vtkSTLWriter.h>
#include <vtkImageCast.h>
#include <vtkImageGaussianSmooth.h>
#include <vtkImageThreshold.h>
#include <vtkTriangleFilter.h>
#include <vtkWindowedSincPolyDataFilter.h>
#include <vtkPointData.h>
#include <vtkDataArray.h>

// ITK includes for DICOM to NIfTI conversion and NIfTI reading
#include <itkImage.h>
#include <itkImageSeriesReader.h>
#include <itkImageFileReader.h>
#include <itkImageIOBase.h>
#include <itkImageIOFactory.h>
#include <itkGDCMImageIO.h>
#include <itkGDCMSeriesFileNames.h>
#include <itkImageFileWriter.h>
#include <itkNiftiImageIO.h>

namespace {
template <typename PixelType>
vtkSmartPointer<vtkImageData> readNiftiToVtkImage(const QString& filePath, int vtkScalarType)
{
    constexpr unsigned int Dimension = 3;
    using ImageType = itk::Image<PixelType, Dimension>;
    using ReaderType = itk::ImageFileReader<ImageType>;

    auto reader = ReaderType::New();
    reader->SetFileName(filePath.toStdString());

    auto niftiIO = itk::NiftiImageIO::New();
    reader->SetImageIO(niftiIO);

    reader->Update();

    typename ImageType::Pointer itkImage = reader->GetOutput();
    typename ImageType::RegionType region = itkImage->GetLargestPossibleRegion();
    typename ImageType::SizeType size = region.GetSize();
    typename ImageType::SpacingType spacing = itkImage->GetSpacing();
    typename ImageType::PointType origin = itkImage->GetOrigin();

    vtkSmartPointer<vtkImageData> vtkImage = vtkSmartPointer<vtkImageData>::New();
    vtkImage->SetDimensions(static_cast<int>(size[0]), static_cast<int>(size[1]), static_cast<int>(size[2]));
    vtkImage->SetExtent(0, static_cast<int>(size[0]) - 1,
                        0, static_cast<int>(size[1]) - 1,
                        0, static_cast<int>(size[2]) - 1);
    vtkImage->SetSpacing(spacing[0], spacing[1], spacing[2]);
    vtkImage->SetOrigin(origin[0], origin[1], origin[2]);
    vtkImage->AllocateScalars(vtkScalarType, 1);

    PixelType* vtkPtr = static_cast<PixelType*>(vtkImage->GetScalarPointer());
    const PixelType* itkPtr = itkImage->GetBufferPointer();
    const size_t totalVoxels = static_cast<size_t>(size[0]) * static_cast<size_t>(size[1]) * static_cast<size_t>(size[2]);
    std::memcpy(vtkPtr, itkPtr, totalVoxels * sizeof(PixelType));

    return vtkImage;
}
} // namespace

SegmentationServiceImpl::SegmentationServiceImpl(QObject* parent)
    : BoneSegmentationService(parent)
    , m_progressTimer(new QTimer(this))
{
    // 初始化默认参数
    m_parameters["fast"] = false;
    m_parameters["ml"] = true;  // 使用多标签输出模式
    m_parameters["smoothing"] = true;
    m_parameters["smoothing_iterations"] = 30;
    m_parameters["smoothing_passband"] = 0.1;
    m_parameters["mask_gaussian_sigma"] = 0.8;
    m_parameters["device"] = "gpu";  // gpu, cpu, or gpu:X
    m_parameters["task"] = "appendicular_bones";  // 默认任务：四肢/骨骼（包含胫骨、腓骨、足部等）
    // 注意：不再使用 quiet 参数，始终显示进度输出

    // 从配置加载 Python 路径和 TotalSegmentator 路径
    loadConfiguration();

    // 进度监控定时器 (每 1 秒检查一次)
    connect(m_progressTimer, &QTimer::timeout, this, &SegmentationServiceImpl::monitorProgress);
    m_progressTimer->start(1000);

    qDebug() << "[SegmentationService] Initialized";
}

SegmentationServiceImpl::~SegmentationServiceImpl()
{
    // 取消所有任务
    QMutexLocker locker(&m_mutex);
    for (auto it = m_tasks.begin(); it != m_tasks.end(); ++it) {
        if (it->process && it->process->state() != QProcess::NotRunning) {
            it->process->kill();
            it->process->waitForFinished(3000);
        }
        if (it->consoleProcess && it->consoleProcess->state() != QProcess::NotRunning) {
            it->consoleProcess->kill();
            it->consoleProcess->waitForFinished(1000);
        }
    }
    m_tasks.clear();
}

QString SegmentationServiceImpl::runBoneSegmentation(const QString& inputPath,
                                                     const QString& outputDir,
                                                     const QString& taskName)
{
    return runSegmentation(inputPath, "all", outputDir, taskName);
}

QString SegmentationServiceImpl::runSegmentation(const QString& inputPath,
                                                const QString& bodyPart,
                                                const QString& outputDir,
                                                const QString& taskName)
{
    QMutexLocker locker(&m_mutex);

    // 验证输入
    QFileInfo inputInfo(inputPath);
    if (!inputInfo.exists()) {
        m_lastError = QString("Input path does not exist: %1").arg(inputPath);
        qWarning() << "[SegmentationService]" << m_lastError;
        return QString();
    }

    // 确定实际输入路径 (TotalSegmentator 支持 DICOM 目录和 NIfTI 文件)
    QString actualInputPath = inputPath;
    bool needsConversion = false;

    // 检查输入类型
    if (inputInfo.isDir()) {
        // 检查是否是 DICOM 目录
        QDir inputDir(inputPath);
        QStringList dcmFiles = inputDir.entryList(QStringList() << "*.dcm" << "*.DCM", QDir::Files);
        if (dcmFiles.isEmpty()) {
            // 可能是没有 .dcm 扩展名的 DICOM 文件
            QStringList allFiles = inputDir.entryList(QDir::Files);
            if (allFiles.isEmpty()) {
                m_lastError = QString("Directory is empty: %1").arg(inputPath);
                qWarning() << "[SegmentationService]" << m_lastError;
                return QString();
            }
        }
        qDebug() << "[SegmentationService] Input is DICOM directory";
    } else if (inputPath.endsWith(".nii") || inputPath.endsWith(".nii.gz")) {
        qDebug() << "[SegmentationService] Input is NIfTI file";
    } else {
        m_lastError = QString("Unsupported input format: %1").arg(inputPath);
        qWarning() << "[SegmentationService]" << m_lastError;
        return QString();
    }

    // 创建任务
    SegmentationTask task;
    task.taskId = generateTaskId();
    task.taskName = taskName.isEmpty() ? QString("Segmentation_%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")) : taskName;
    task.inputPath = actualInputPath;
    task.bodyPart = bodyPart;
    task.outputDir = outputDir.isEmpty() ? createTempOutputDir() : outputDir;
    task.logFilePath = QDir(task.outputDir).filePath("totalsegmentator.log");
    task.status = "pending";
    task.progress = 0;
    task.startTime = QDateTime::currentMSecsSinceEpoch();

    // 确保输出目录存在
    QDir().mkpath(task.outputDir);

    // 保存任务
    m_tasks[task.taskId] = task;

    qDebug() << "[SegmentationService] Created task:" << task.taskId
             << "Input:" << actualInputPath
             << "Output:" << task.outputDir
             << "BodyPart:" << bodyPart;

    // 启动分割进程
    locker.unlock();  // 解锁，避免死锁
    if (!startSegmentationProcess(m_tasks[task.taskId])) {
        locker.relock();
        m_tasks.remove(task.taskId);
        return QString();
    }

    emit segmentationStarted(task.taskId, task.taskName);

    return task.taskId;
}

bool SegmentationServiceImpl::cancelTask(const QString& taskId)
{
    QMutexLocker locker(&m_mutex);

    SegmentationTask* task = findTask(taskId);
    if (!task) {
        m_lastError = QString("Task not found: %1").arg(taskId);
        return false;
    }

    if (task->status == "completed" || task->status == "failed" || task->status == "cancelled") {
        m_lastError = QString("Task already finished: %1").arg(task->status);
        return false;
    }

    // 终止进程
    if (task->process && task->process->state() != QProcess::NotRunning) {
        task->process->kill();
        task->process->waitForFinished(3000);
    }
    if (task->consoleProcess && task->consoleProcess->state() != QProcess::NotRunning) {
        task->consoleProcess->kill();
        task->consoleProcess->waitForFinished(1000);
    }

    task->status = "cancelled";
    task->endTime = QDateTime::currentMSecsSinceEpoch();

    emit segmentationCancelled(taskId);

    return true;
}

QString SegmentationServiceImpl::getTaskStatus(const QString& taskId) const
{
    QMutexLocker locker(&m_mutex);
    const SegmentationTask* task = findTask(taskId);
    return task ? task->status : QString();
}

int SegmentationServiceImpl::getTaskProgress(const QString& taskId) const
{
    QMutexLocker locker(&m_mutex);
    const SegmentationTask* task = findTask(taskId);
    return task ? task->progress : -1;
}

QStringList SegmentationServiceImpl::getActiveTasks() const
{
    QMutexLocker locker(&m_mutex);
    QStringList activeTasks;
    for (auto it = m_tasks.constBegin(); it != m_tasks.constEnd(); ++it) {
        if (it->status == "pending" || it->status == "running") {
            activeTasks.append(it->taskId);
        }
    }
    return activeTasks;
}

QVariantMap SegmentationServiceImpl::getTaskInfo(const QString& taskId) const
{
    QMutexLocker locker(&m_mutex);
    const SegmentationTask* task = findTask(taskId);
    if (!task) {
        return QVariantMap();
    }

    QVariantMap info;
    info["taskId"] = task->taskId;
    info["taskName"] = task->taskName;
    info["inputPath"] = task->inputPath;
    info["outputDir"] = task->outputDir;
    info["bodyPart"] = task->bodyPart;
    info["status"] = task->status;
    info["progress"] = task->progress;
    info["startTime"] = task->startTime;
    info["endTime"] = task->endTime;
    if (!task->errorMessage.isEmpty()) {
        info["error"] = task->errorMessage;
    }
    info["result"] = task->result;

    return info;
}

vtkSmartPointer<vtkPolyData> SegmentationServiceImpl::getSegmentationMesh(const QString& taskId,
                                                                          const QString& bodyPart)
{
    QMutexLocker locker(&m_mutex);

    // 检查缓存
    QString cacheKey = taskId + "_" + bodyPart;
    if (m_meshCache.contains(cacheKey)) {
        return m_meshCache[cacheKey];
    }

    const SegmentationTask* task = findTask(taskId);
    if (!task || task->status != "completed") {
        m_lastError = "Task not completed or not found";
        return nullptr;
    }

    // 查找输出文件
    QStringList files = getSegmentationFiles(taskId, "nii");
    if (files.isEmpty()) {
        m_lastError = "No segmentation output files found";
        return nullptr;
    }

    // 加载第一个文件并转换为 Mesh
    QString niftiFile = files.first();
    vtkSmartPointer<vtkImageData> mask = loadNiftiMask(niftiFile);
    if (!mask) {
        return nullptr;
    }

    vtkSmartPointer<vtkPolyData> mesh = generateMeshFromMask(mask, 0.5);
    if (mesh) {
        m_meshCache[cacheKey] = mesh;
    }

    return mesh;
}

vtkSmartPointer<vtkImageData> SegmentationServiceImpl::getSegmentationMask(const QString& taskId)
{
    QMutexLocker locker(&m_mutex);

    // 检查缓存
    if (m_maskCache.contains(taskId)) {
        return m_maskCache[taskId];
    }

    const SegmentationTask* task = findTask(taskId);
    if (!task || task->status != "completed") {
        m_lastError = "Task not completed or not found";
        return nullptr;
    }

    QStringList files = getSegmentationFiles(taskId, "nii");
    if (files.isEmpty()) {
        return nullptr;
    }

    vtkSmartPointer<vtkImageData> mask = loadNiftiMask(files.first());
    if (mask) {
        m_maskCache[taskId] = mask;
    }

    return mask;
}

QStringList SegmentationServiceImpl::getSegmentationFiles(const QString& taskId,
                                                         const QString& format)
{
    QMutexLocker locker(&m_mutex);

    SegmentationTask* task = findTask(taskId);
    if (!task) {
        return QStringList();
    }

    QDir outputDir(task->outputDir);
    if (!outputDir.exists()) {
        return QStringList();
    }

    QStringList nameFilters;
    if (format == "stl") {
        nameFilters << "*.stl";
    } else if (format == "nii") {
        nameFilters << "*.nii" << "*.nii.gz";
    } else if (format == "vtk") {
        nameFilters << "*.vtk";
    }

    QStringList files;
    for (const QFileInfo& fileInfo : outputDir.entryInfoList(nameFilters, QDir::Files)) {
        files.append(fileInfo.absoluteFilePath());
    }

    return files;
}

bool SegmentationServiceImpl::exportSegmentation(const QString& taskId,
                                                const QString& exportPath,
                                                const QString& format)
{
    vtkSmartPointer<vtkPolyData> mesh = getSegmentationMesh(taskId);
    if (!mesh) {
        return false;
    }

    if (format == "stl") {
        vtkSmartPointer<vtkSTLWriter> writer = vtkSmartPointer<vtkSTLWriter>::New();
        writer->SetFileName(exportPath.toStdString().c_str());
        writer->SetInputData(mesh);
        writer->Write();
        return true;
    }

    // TODO: 添加其他格式支持
    m_lastError = QString("Unsupported export format: %1").arg(format);
    return false;
}

bool SegmentationServiceImpl::setPythonEnvironment(const QString& pythonPath)
{
    m_pythonPath = pythonPath;

    // 保存配置
    if (!saveConfiguration()) {
        qWarning() << "[SegmentationService] Failed to save configuration";
    }

    return checkPythonEnvironment();
}

bool SegmentationServiceImpl::checkPythonEnvironment()
{
    if (m_pythonPath.isEmpty()) {
        m_pythonPath = "python";  // 使用系统 PATH 中的 python
    }

    // 测试 Python 环境
    QProcess process;
    process.start(m_pythonPath, QStringList() << "--version");
    if (!process.waitForFinished(5000)) {
        emit pythonEnvironmentChanged(false, "Python executable not found or timeout");
        return false;
    }

    QString output = process.readAllStandardOutput();
    qDebug() << "[SegmentationService] Python version:" << output.trimmed();

    // 测试 TotalSegmentator (正确的模块路径)
    process.start(m_pythonPath, QStringList() << "-m" << "totalsegmentator.bin.TotalSegmentator" << "--version");
    if (!process.waitForFinished(10000)) {
        emit pythonEnvironmentChanged(false, "TotalSegmentator not installed or timeout");
        return false;
    }

    // 检查是否成功执行
    if (process.exitCode() != 0) {
        QString errorOutput = process.readAllStandardError();
        qWarning() << "[SegmentationService] TotalSegmentator check failed:" << errorOutput;
        emit pythonEnvironmentChanged(false, "TotalSegmentator not properly installed");
        return false;
    }

    output = process.readAllStandardOutput();
    qDebug() << "[SegmentationService] TotalSegmentator version:" << output.trimmed();

    // 检查关键依赖：blosc2（nnUNetv2 -> acvl_utils 依赖）
    process.start(m_pythonPath, QStringList() << "-c" << "import blosc2; print(blosc2.__version__)");
    if (!process.waitForFinished(10000) || process.exitCode() != 0) {
        const QString errorOutput = process.readAllStandardError();
        qWarning() << "[SegmentationService] Dependency check failed: blosc2" << errorOutput.trimmed();
        emit pythonEnvironmentChanged(false, "Missing Python dependency: blosc2 (run: python -m pip install blosc2)");
        return false;
    }

    // 提示：模型权重会在首次运行时自动下载到用户目录
    qDebug() << "[SegmentationService] Note: Model weights will be downloaded to ~/.totalsegmentator on first run";

    emit pythonEnvironmentChanged(true, "Python environment ready");
    return true;
}

bool SegmentationServiceImpl::setSegmentationParameters(const QVariantMap& parameters)
{
    m_parameters = parameters;

    // 保存配置
    if (!saveConfiguration()) {
        qWarning() << "[SegmentationService] Failed to save configuration";
    }

    return true;
}

QVariantMap SegmentationServiceImpl::getSegmentationParameters() const
{
    return m_parameters;
}

QStringList SegmentationServiceImpl::getSupportedBodyParts() const
{
    // TotalSegmentator 支持的主要骨骼部位
    return QStringList() << "all" << "femur" << "tibia" << "pelvis" << "vertebrae"
                        << "ribs" << "skull" << "humerus" << "radius" << "ulna";
}

bool SegmentationServiceImpl::convertDicomToNifti(const QString& dicomPath, const QString& outputPath)
{
    qDebug() << "[SegmentationService] Converting DICOM to NIfTI:" << dicomPath << "->" << outputPath;

    try {
#if 0
        auto imageIO = itk::ImageIOFactory::CreateImageIO(filePath.toStdString().c_str(), itk::ImageIOFactory::ReadMode);
        if (!imageIO) {
            m_lastError = QString("Failed to create ImageIO for NIfTI: %1").arg(filePath);
            qWarning() << "[SegmentationService]" << m_lastError;
            return nullptr;
        }

        imageIO->SetFileName(filePath.toStdString());
        imageIO->ReadImageInformation();

        if (imageIO->GetNumberOfDimensions() != 3) {
            m_lastError = QString("Unsupported NIfTI dimension (%1), expected 3D: %2")
                              .arg(imageIO->GetNumberOfDimensions())
                              .arg(filePath);
            qWarning() << "[SegmentationService]" << m_lastError;
            return nullptr;
        }

        if (imageIO->GetNumberOfComponents() != 1) {
            m_lastError = QString("Unsupported NIfTI components (%1), expected scalar image: %2")
                              .arg(imageIO->GetNumberOfComponents())
                              .arg(filePath);
            qWarning() << "[SegmentationService]" << m_lastError;
            return nullptr;
        }

        const auto componentType = imageIO->GetComponentType();
        qDebug() << "[SegmentationService] NIfTI component type:"
                 << QString::fromStdString(itk::ImageIOBase::GetComponentTypeAsString(componentType));

        vtkSmartPointer<vtkImageData> vtkImage;
        switch (componentType) {
        case itk::ImageIOBase::UCHAR:
            vtkImage = readNiftiToVtkImage<unsigned char>(filePath, VTK_UNSIGNED_CHAR);
            break;
        case itk::ImageIOBase::CHAR:
            vtkImage = readNiftiToVtkImage<signed char>(filePath, VTK_SIGNED_CHAR);
            break;
        case itk::ImageIOBase::USHORT:
            vtkImage = readNiftiToVtkImage<unsigned short>(filePath, VTK_UNSIGNED_SHORT);
            break;
        case itk::ImageIOBase::SHORT:
            vtkImage = readNiftiToVtkImage<short>(filePath, VTK_SHORT);
            break;
        case itk::ImageIOBase::UINT:
            vtkImage = readNiftiToVtkImage<unsigned int>(filePath, VTK_UNSIGNED_INT);
            break;
        case itk::ImageIOBase::INT:
            vtkImage = readNiftiToVtkImage<int>(filePath, VTK_INT);
            break;
        case itk::ImageIOBase::FLOAT:
            vtkImage = readNiftiToVtkImage<float>(filePath, VTK_FLOAT);
            break;
        case itk::ImageIOBase::DOUBLE:
            vtkImage = readNiftiToVtkImage<double>(filePath, VTK_DOUBLE);
            break;
        default:
            qWarning() << "[SegmentationService] Unsupported component type, falling back to float:"
                       << QString::fromStdString(itk::ImageIOBase::GetComponentTypeAsString(componentType));
            vtkImage = readNiftiToVtkImage<float>(filePath, VTK_FLOAT);
            break;
        }

        if (!vtkImage) {
            m_lastError = QString("Failed to load NIfTI into VTK ImageData: %1").arg(filePath);
            qWarning() << "[SegmentationService]" << m_lastError;
            return nullptr;
        }

        int dims[3];
        vtkImage->GetDimensions(dims);
        double spacing[3];
        vtkImage->GetSpacing(spacing);
        qDebug() << "[SegmentationService] ITK loaded - Size:"
                 << dims[0] << "x" << dims[1] << "x" << dims[2]
                 << "Spacing:" << spacing[0] << spacing[1] << spacing[2];

        double range[2];
        vtkImage->GetScalarRange(range);
        qDebug() << "[SegmentationService] Scalar range:" << range[0] << "-" << range[1];

        if (range[0] == range[1]) {
            m_lastError = QString("NIfTI file has no valid data (constant value %1): %2")
                              .arg(range[0]).arg(filePath);
            qWarning() << "[SegmentationService]" << m_lastError;
            return nullptr;
        }

        qDebug() << "[SegmentationService] Successfully loaded NIfTI with ITK";
        return vtkImage;

#endif
        // 定义图像类型
        using PixelType = signed short;
        constexpr unsigned int Dimension = 3;
        using ImageType = itk::Image<PixelType, Dimension>;

        // 创建 DICOM 系列读取器
        using ReaderType = itk::ImageSeriesReader<ImageType>;
        auto reader = ReaderType::New();

        // 使用 GDCM IO
        using ImageIOType = itk::GDCMImageIO;
        auto dicomIO = ImageIOType::New();
        reader->SetImageIO(dicomIO);

        // 获取 DICOM 系列文件名
        using NamesGeneratorType = itk::GDCMSeriesFileNames;
        auto namesGenerator = NamesGeneratorType::New();
        namesGenerator->SetUseSeriesDetails(true);
        namesGenerator->AddSeriesRestriction("0008|0021"); // Series Date
        namesGenerator->SetDirectory(dicomPath.toStdString());

        // 获取系列 UIDs
        using SeriesIdContainer = std::vector<std::string>;
        const SeriesIdContainer& seriesUID = namesGenerator->GetSeriesUIDs();

        if (seriesUID.empty()) {
            m_lastError = "No DICOM series found in directory: " + dicomPath;
            qWarning() << "[SegmentationService]" << m_lastError;
            return false;
        }

        qDebug() << "[SegmentationService] Found" << seriesUID.size() << "DICOM series";

        // 使用第一个系列
        std::string seriesIdentifier = seriesUID.front();
        qDebug() << "[SegmentationService] Using series:" << QString::fromStdString(seriesIdentifier);

        // 获取该系列的文件名列表
        using FileNamesContainer = std::vector<std::string>;
        FileNamesContainer fileNames = namesGenerator->GetFileNames(seriesIdentifier);

        qDebug() << "[SegmentationService] Series contains" << fileNames.size() << "files";

        reader->SetFileNames(fileNames);

        // 读取 DICOM 系列
        reader->Update();

        // 写入 NIfTI 文件
        using WriterType = itk::ImageFileWriter<ImageType>;
        auto writer = WriterType::New();

        // 使用 NIfTI IO
        using NiftiIOType = itk::NiftiImageIO;
        auto niftiIO = NiftiIOType::New();
        writer->SetImageIO(niftiIO);

        writer->SetFileName(outputPath.toStdString());
        writer->SetInput(reader->GetOutput());
        writer->UseCompressionOn();  // 生成 .nii.gz

        writer->Update();

        qDebug() << "[SegmentationService] DICOM to NIfTI conversion completed:" << outputPath;
        return true;

    } catch (const itk::ExceptionObject& ex) {
        m_lastError = QString("ITK Exception: %1").arg(ex.GetDescription());
        qCritical() << "[SegmentationService]" << m_lastError;
        return false;
    } catch (const std::exception& ex) {
        m_lastError = QString("Exception: %1").arg(ex.what());
        qCritical() << "[SegmentationService]" << m_lastError;
        return false;
    }
}

vtkSmartPointer<vtkPolyData> SegmentationServiceImpl::convertMaskToMesh(const QString& niftiPath,
                                                                        double threshold)
{
    vtkSmartPointer<vtkImageData> mask = loadNiftiMask(niftiPath);
    if (!mask) {
        return nullptr;
    }

    return generateMeshFromMask(mask, threshold);
}

vtkSmartPointer<vtkPolyData> SegmentationServiceImpl::convertNiftiToMeshAuto(const QString& niftiPath)
{
    vtkSmartPointer<vtkImageData> image = loadNiftiMask(niftiPath);
    if (!image) {
        return nullptr;
    }

    double range[2];
    image->GetScalarRange(range);

    const bool labelMap = isLikelyLabelMap(image, range);
    if (labelMap) {
        qDebug() << "[SegmentationService] NIfTI detected as label map, generating mesh from nonzero mask";
        return generateMeshFromMask(image, 0.5);
    }

    double huThreshold = m_parameters.value("hu_threshold", 200.0).toDouble();
    qDebug() << "[SegmentationService] NIfTI detected as intensity volume, generating mesh with HU threshold:" << huThreshold;
    return generateMeshFromIntensity(image, huThreshold);
}

bool SegmentationServiceImpl::cleanupTempFiles(const QString& taskId)
{
    QMutexLocker locker(&m_mutex);

    if (taskId.isEmpty()) {
        // 清理所有临时文件
        for (const SegmentationTask& task : m_tasks) {
            if (task.outputDir.contains("temp")) {
                QDir(task.outputDir).removeRecursively();
            }
        }
        return true;
    }

    const SegmentationTask* task = findTask(taskId);
    if (!task) {
        return false;
    }

    if (task->outputDir.contains("temp")) {
        return QDir(task->outputDir).removeRecursively();
    }

    return true;
}

QString SegmentationServiceImpl::getLastError() const
{
    return m_lastError;
}

// ==================== Private Slots ====================

void SegmentationServiceImpl::handleProcessOutput()
{
    QProcess* process = qobject_cast<QProcess*>(sender());
    if (!process) return;

    QString taskId = findTaskIdByProcess(process);
    if (taskId.isEmpty()) return;

    // 读取标准输出
    QString output = QString::fromLocal8Bit(process->readAllStandardOutput());

    // 追加到日志文件（供命令行窗口实时显示）
    {
        QMutexLocker locker(&m_mutex);
        SegmentationTask* task = findTask(taskId);
        if (task) {
            appendToLogFileUtf8(task->logFilePath, output);
        }
    }

    // 打印完整输出到控制台（用户可见）
    if (!output.trimmed().isEmpty()) {
        qDebug() << "[TotalSegmentator]" << output.trimmed();
    }

    // 解析多种进度格式
    int progress = -1;

    // 格式1: "Progress: XX%" 或 "progress: XX%"
    QRegularExpression re1("[Pp]rogress:\\s*(\\d+)%");
    QRegularExpressionMatch match1 = re1.match(output);
    if (match1.hasMatch()) {
        progress = match1.captured(1).toInt();
    }

    // 格式2: tqdm 格式 "XX%|" 或 "100%|██████████|"
    if (progress < 0) {
        QRegularExpression re2("(\\d+)%\\|");
        QRegularExpressionMatch match2 = re2.match(output);
        if (match2.hasMatch()) {
            progress = match2.captured(1).toInt();
        }
    }

    // 格式3: 单独的百分比 "XX%" (不跟随 | 符号)
    if (progress < 0) {
        QRegularExpression re3("\\b(\\d{1,3})%\\b");
        QRegularExpressionMatch match3 = re3.match(output);
        if (match3.hasMatch()) {
            progress = match3.captured(1).toInt();
        }
    }

    // 格式4: "Processing: XX/YY" 或 "XX/YY [" (tqdm 风格)
    if (progress < 0) {
        QRegularExpression re4("(\\d+)/(\\d+)");
        QRegularExpressionMatch match4 = re4.match(output);
        if (match4.hasMatch()) {
            int current = match4.captured(1).toInt();
            int total = match4.captured(2).toInt();
            if (total > 0) {
                progress = (current * 100) / total;
            }
        }
    }

    if (progress >= 0) {
        updateTaskProgress(taskId, progress, output.trimmed());
    }
}

void SegmentationServiceImpl::handleProcessError()
{
    QProcess* process = qobject_cast<QProcess*>(sender());
    if (!process) return;

    QString taskId = findTaskIdByProcess(process);
    if (taskId.isEmpty()) return;

    // 读取错误输出
    QString error = QString::fromLocal8Bit(process->readAllStandardError());

    // 追加到日志文件（供命令行窗口实时显示）
    {
        QMutexLocker locker(&m_mutex);
        SegmentationTask* task = findTask(taskId);
        if (task) {
            appendToLogFileUtf8(task->logFilePath, error);
        }
    }

    // 打印到控制台（用户可见）
    // 注意：Python 的 tqdm 进度条通常输出到 stderr，所以这不一定是真正的错误
    if (!error.trimmed().isEmpty()) {
        // 检查是否是 tqdm 进度条输出（包含进度信息）
        if (error.contains("%|") || error.contains("it/s") || error.contains("B/s")) {
            // tqdm 输出，作为普通信息显示
            qDebug() << "[TotalSegmentator]" << error.trimmed();

            // 尝试解析进度
            QRegularExpression re("(\\d+)%\\|");
            QRegularExpressionMatch match = re.match(error);
            if (match.hasMatch()) {
                int progress = match.captured(1).toInt();
                updateTaskProgress(taskId, progress, error.trimmed());
            }
        } else {
            // 真正的错误信息
            qWarning() << "[TotalSegmentator Error]" << error.trimmed();
        }
    }
}

void SegmentationServiceImpl::handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    QProcess* process = qobject_cast<QProcess*>(sender());
    if (!process) return;

    QString taskId = findTaskIdByProcess(process);
    if (taskId.isEmpty()) return;

    qDebug() << "[SegmentationService]" << taskId
             << "Finished with exit code:" << exitCode
             << "Status:" << exitStatus;

    QString logFilePath;
    {
        QMutexLocker locker(&m_mutex);
        SegmentationTask* task = findTask(taskId);
        if (task) {
            logFilePath = task->logFilePath;
            task->process = nullptr;
        }
    }

    const QString statusText = (exitStatus == QProcess::NormalExit) ? "NormalExit" : "CrashExit";
    appendToLogFileUtf8(logFilePath,
                        QString("\n\n[Process finished] exitCode=%1 status=%2\n")
                            .arg(exitCode)
                            .arg(statusText));

    if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
        // 解析输出文件（避免在持锁状态下发射信号）
        SegmentationTask taskSnapshot;
        {
            QMutexLocker locker(&m_mutex);
            if (SegmentationTask* task = findTask(taskId)) {
                taskSnapshot = *task;
            }
        }

        if (!taskSnapshot.taskId.isEmpty() && parseSegmentationOutput(taskSnapshot)) {
            QVariantMap result;
            result["outputDir"] = taskSnapshot.outputDir;
            result["files"] = getSegmentationFiles(taskId);
            completeTask(taskId, result);
        } else {
            const QString errorText = m_lastError.isEmpty() ? QStringLiteral("Failed to parse segmentation output")
                                                           : m_lastError;
            failTask(taskId, errorText);
        }
    } else {
        QString error = QString::fromLocal8Bit(process->readAllStandardError());
        failTask(taskId, QString("Process failed: %1").arg(error));
    }

    // 清理进程对象
    process->deleteLater();
}

void SegmentationServiceImpl::monitorProgress()
{
    // 定期检查任务状态，更新进度
    // 简化版：实际可以通过文件系统监控输出目录的变化
}

// ==================== Private Methods ====================

QString SegmentationServiceImpl::generateTaskId()
{
    return QString("seg_%1_%2")
        .arg(QDateTime::currentDateTime().toString("yyyyMMddHHmmss"))
        .arg(qrand() % 10000, 4, 10, QChar('0'));
}

QString SegmentationServiceImpl::createTempOutputDir()
{
    // 默认输出放到项目目录下（便于调试与查看结果），而不是系统临时目录
    // 1) 若运行路径包含 "/build/"，取 build 之前的目录作为项目根目录
    // 2) 否则退化为可执行程序所在目录
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString normalizedAppDir = QDir::fromNativeSeparators(appDir);

    QString projectRoot = appDir;
    const int buildIndex = normalizedAppDir.indexOf("/build/");
    if (buildIndex > 0) {
        projectRoot = normalizedAppDir.left(buildIndex);
    }

    const QString outputRoot = QDir(projectRoot).filePath("segmentation_outputs");
    QDir().mkpath(outputRoot);

    const QString outputDir = QDir(outputRoot).filePath(
        QString("medicalpro_segmentation_%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")));

    QDir().mkpath(outputDir);
    return QDir(outputDir).absolutePath();
}

QStringList SegmentationServiceImpl::buildSegmentationCommand(const SegmentationTask& task)
{
    QStringList args;

    // 仅使用 pip 安装的 TotalSegmentator 模块入口
    args << "-m" << "totalsegmentator.bin.TotalSegmentator";

    // 输入输出路径
    args << "-i" << task.inputPath;
    const bool multilabel = m_parameters.value("ml", true).toBool();
    if (multilabel) {
        // TotalSegmentator 在 --ml 模式下期望 -o 为文件路径
        // 使用 .nii（非压缩）更利于后续 VTK 读取与导出 STL
        const QString mlOutputPath = QDir(task.outputDir).filePath("segmentation.nii");
        args << "-o" << mlOutputPath;
    } else {
        // 非 --ml 模式下 -o 为输出目录
        args << "-o" << task.outputDir;
    }

    // 快速模式 (3mm 分辨率)
    if (m_parameters.value("fast", false).toBool()) {
        args << "--fast";
    }

    // 多标签输出模式
    if (multilabel) {
        args << "--ml";
    }

    // 指定设备 (gpu, cpu, gpu:X)
    QString device = m_parameters.value("device", "gpu").toString();
    args << "--device" << device;

    // 指定任务类型
    // 默认使用 appendicular_bones 任务，因为它包含所有四肢骨骼（胫骨、腓骨、足骨等）
    // total 任务只包含股骨，不适合下肢分割
    // 注意：appendicular_bones 需要 TotalSegmentator 学术许可证
    QString taskType = m_parameters.value("task", "appendicular_bones").toString();
    if (!taskType.isEmpty() && taskType != "total") {
        args << "--task" << taskType;
    }

    // 注意：不添加 --quiet 参数，强制显示 TotalSegmentator 进度输出
    // 这样用户可以在命令行窗口中看到分割进度

    // 指定身体部位子集
    if (!task.bodyPart.isEmpty() && task.bodyPart != "all") {
        args << "--roi_subset" << task.bodyPart;
    }

    return args;
}

bool SegmentationServiceImpl::startSegmentationProcess(SegmentationTask& task)
{
    qDebug() << "[SegmentationService] startSegmentationProcess called";
    qDebug() << "[SegmentationService] Current thread:" << QThread::currentThread();
    qDebug() << "[SegmentationService] Object thread:" << this->thread();
    qDebug() << "[SegmentationService] App main thread:" << QCoreApplication::instance()->thread();

    // 简化处理：直接执行，不做复杂的线程检查
    // QProcess 在 Qt 5.x 中可以在任何线程创建，只要信号槽连接正确
    // 如果遇到线程问题，会在运行时报警告，但不会崩溃
    qDebug() << "[SegmentationService] Calling startSegmentationProcessInternal directly";
    return startSegmentationProcessInternal(task);
}

bool SegmentationServiceImpl::startSegmentationProcessInternal(SegmentationTask& task)
{
    if (m_pythonPath.isEmpty()) {
        m_lastError = "Python environment not configured";
        qWarning() << "[SegmentationService]" << m_lastError;
        return false;
    }

    // 创建 QProcess - 不指定父对象，避免跨线程问题
    // 注意：由于 SegmentationServiceImpl 可能在线程池线程中创建，
    // 而调用可能来自主线程，所以不能将 this 作为父对象
    // 进程结束后会在 handleProcessFinished 中手动删除
    QProcess* process = new QProcess(nullptr);
    task.process = process;

    // 设置进程环境变量
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    process->setProcessEnvironment(env);

    // 连接信号 - 使用 DirectConnection 因为 QProcess 没有父对象，
    // 信号会在调用线程中直接执行
    connect(process, &QProcess::readyReadStandardOutput,
            this, &SegmentationServiceImpl::handleProcessOutput, Qt::DirectConnection);
    connect(process, &QProcess::readyReadStandardError,
            this, &SegmentationServiceImpl::handleProcessError, Qt::DirectConnection);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &SegmentationServiceImpl::handleProcessFinished, Qt::DirectConnection);

    // 构建命令
    QStringList args = buildSegmentationCommand(task);
    // 强制 Python 不缓冲输出，便于在命令行窗口实时看到进度
    args.prepend("-u");

    qDebug() << "[SegmentationService] Starting process:" << m_pythonPath << args;

    // 初始化日志文件（命令行窗口将 tail 这个文件）
    if (!task.logFilePath.isEmpty()) {
        QFile logFile(task.logFilePath);
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            auto quoteArg = [](const QString& arg) -> QString {
                if (arg.isEmpty()) {
                    return "\"\"";
                }
                return arg.contains(' ') ? QString("\"%1\"").arg(arg) : arg;
            };

            QString cmdLine = quoteArg(m_pythonPath);
            for (const QString& arg : args) {
                cmdLine += " " + quoteArg(arg);
            }

            QTextStream ts(&logFile);
            ts.setCodec("UTF-8");
            ts << "TotalSegmentator console log\n";
            ts << "OutputDir: " << task.outputDir << "\n";
            ts << "Command: " << cmdLine << "\n";
            ts << "----------------------------------------\n";
            ts.flush();
        }
    }

#ifdef Q_OS_WIN
    // 打开一个单独的命令行窗口实时查看日志（不影响主进程结束/解析结果）
    // 使用 PowerShell 显示日志：用 FileShare.ReadWrite 自己实现 tail，避免 Get-Content -Wait 占用句柄导致写入失败
    if (task.consoleProcess == nullptr && !task.logFilePath.isEmpty()) {
        QProcess* console = new QProcess(nullptr);
        task.consoleProcess = console;
        console->setWorkingDirectory(task.outputDir);

        QString nativeLogPath = QDir::toNativeSeparators(task.logFilePath);
        // 转义路径中的单引号（PowerShell 字符串）
        QString escapedLogPath = nativeLogPath;
        escapedLogPath.replace("'", "''");

        QString escapedTitle = task.taskName;
        escapedTitle.replace("'", "''");

        // PowerShell 脚本：打开文件为 FileShare.ReadWrite，逐行读取并持续输出
        // （注意：我们在写入端会把 '\r' 规范化为 '\n'，否则 tqdm 进度条可能看不到）
        QString psScript = QString(
            "$host.UI.RawUI.WindowTitle = 'TotalSegmentator - %1';"
            "$path = '%2';"
            "Write-Host ('Tailing: ' + $path) -ForegroundColor Gray;"
            "Write-Host '----------------------------------------' -ForegroundColor Gray;"
            "while (-not (Test-Path -LiteralPath $path)) { Start-Sleep -Milliseconds 200 };"
            "$fs = [System.IO.FileStream]::new($path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite);"
            "$sr = [System.IO.StreamReader]::new($fs, [System.Text.Encoding]::UTF8);"
            "while ($true) {"
            "  while (-not $sr.EndOfStream) { $line = $sr.ReadLine(); if ($line -ne $null) { Write-Host $line } }"
            "  Start-Sleep -Milliseconds 200"
            "}"
        ).arg(escapedTitle, escapedLogPath);

        console->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* procArgs) {
            procArgs->flags |= CREATE_NEW_CONSOLE;
        });

        qDebug() << "[SegmentationService] Launching console tail window for log:" << task.logFilePath;
        console->start("powershell.exe", QStringList() << "-NoProfile" << "-NoExit" << "-Command" << psScript);
    }
#endif

    // 直接启动 Python 进程（便于 Qt 获取退出状态并解析输出文件）
    // 非 Windows 平台：可以考虑使用 xterm 或 gnome-terminal
    // 目前先用默认方式
    process->start(m_pythonPath, args);

    if (!process->waitForStarted(10000)) {
        m_lastError = QString("Failed to start segmentation process: %1")
                     .arg(process->errorString());
        qWarning() << "[SegmentationService]" << m_lastError;
        delete process;
        task.process = nullptr;
        return false;
    }

    task.status = "running";
    task.progress = 0;

    qDebug() << "[SegmentationService] Process started successfully, PID:" << process->processId();
    qDebug() << "[SegmentationService] A console window should appear showing TotalSegmentator progress";

    return true;
}

bool SegmentationServiceImpl::parseSegmentationOutput(SegmentationTask& task)
{
    // 检查输出目录
    QDir outputDir(task.outputDir);
    if (!outputDir.exists()) {
        m_lastError = "Output directory does not exist";
        return false;
    }

    // 检查是否有输出文件
    QStringList files = outputDir.entryList(QStringList() << "*.nii" << "*.nii.gz", QDir::Files);

    // 兼容：旧逻辑在 --ml 模式下可能会在输出目录同级生成 <outputDir>.nii
    if (files.isEmpty()) {
        const QString siblingNii = task.outputDir + ".nii";
        const QString siblingNiiGz = task.outputDir + ".nii.gz";
        QString sourcePath;
        QString targetPath;

        if (QFile::exists(siblingNii)) {
            sourcePath = siblingNii;
            targetPath = outputDir.filePath("segmentation.nii");
        } else if (QFile::exists(siblingNiiGz)) {
            sourcePath = siblingNiiGz;
            targetPath = outputDir.filePath("segmentation.nii.gz");
        }

        if (!sourcePath.isEmpty()) {
            if (!QFile::rename(sourcePath, targetPath)) {
                // rename 失败则尝试复制
                QFile::remove(targetPath);
                QFile::copy(sourcePath, targetPath);
                QFile::remove(sourcePath);
            }
            files = outputDir.entryList(QStringList() << "*.nii" << "*.nii.gz", QDir::Files);
        }
    }

    if (files.isEmpty()) {
        m_lastError = "No output files found";
        return false;
    }

    qDebug() << "[SegmentationService] Found" << files.count() << "segmentation files";
    return true;
}

vtkSmartPointer<vtkImageData> SegmentationServiceImpl::loadNiftiMask(const QString& filePath)
{
    if (!QFile::exists(filePath)) {
        m_lastError = QString("NIfTI file does not exist: %1").arg(filePath);
        qWarning() << "[SegmentationService]" << m_lastError;
        return nullptr;
    }

    qDebug() << "[SegmentationService] Loading NIfTI with ITK:" << filePath;

    try {
        auto imageIO = itk::ImageIOFactory::CreateImageIO(filePath.toStdString().c_str(), itk::ImageIOFactory::ReadMode);
        if (!imageIO) {
            m_lastError = QString("Failed to create ImageIO for NIfTI: %1").arg(filePath);
            qWarning() << "[SegmentationService]" << m_lastError;
            return nullptr;
        }

        imageIO->SetFileName(filePath.toStdString());
        imageIO->ReadImageInformation();

        if (imageIO->GetNumberOfDimensions() != 3) {
            m_lastError = QString("Unsupported NIfTI dimension (%1), expected 3D: %2")
                              .arg(imageIO->GetNumberOfDimensions())
                              .arg(filePath);
            qWarning() << "[SegmentationService]" << m_lastError;
            return nullptr;
        }

        if (imageIO->GetNumberOfComponents() != 1) {
            m_lastError = QString("Unsupported NIfTI components (%1), expected scalar image: %2")
                              .arg(imageIO->GetNumberOfComponents())
                              .arg(filePath);
            qWarning() << "[SegmentationService]" << m_lastError;
            return nullptr;
        }

        const auto componentType = imageIO->GetComponentType();
        qDebug() << "[SegmentationService] NIfTI component type:"
                 << QString::fromStdString(itk::ImageIOBase::GetComponentTypeAsString(componentType));

        vtkSmartPointer<vtkImageData> vtkImage;
        switch (componentType) {
        case itk::ImageIOBase::UCHAR:
            vtkImage = readNiftiToVtkImage<unsigned char>(filePath, VTK_UNSIGNED_CHAR);
            break;
        case itk::ImageIOBase::CHAR:
            vtkImage = readNiftiToVtkImage<signed char>(filePath, VTK_SIGNED_CHAR);
            break;
        case itk::ImageIOBase::USHORT:
            vtkImage = readNiftiToVtkImage<unsigned short>(filePath, VTK_UNSIGNED_SHORT);
            break;
        case itk::ImageIOBase::SHORT:
            vtkImage = readNiftiToVtkImage<short>(filePath, VTK_SHORT);
            break;
        case itk::ImageIOBase::UINT:
            vtkImage = readNiftiToVtkImage<unsigned int>(filePath, VTK_UNSIGNED_INT);
            break;
        case itk::ImageIOBase::INT:
            vtkImage = readNiftiToVtkImage<int>(filePath, VTK_INT);
            break;
        case itk::ImageIOBase::FLOAT:
            vtkImage = readNiftiToVtkImage<float>(filePath, VTK_FLOAT);
            break;
        case itk::ImageIOBase::DOUBLE:
            vtkImage = readNiftiToVtkImage<double>(filePath, VTK_DOUBLE);
            break;
        default:
            qWarning() << "[SegmentationService] Unsupported component type, falling back to float:"
                       << QString::fromStdString(itk::ImageIOBase::GetComponentTypeAsString(componentType));
            vtkImage = readNiftiToVtkImage<float>(filePath, VTK_FLOAT);
            break;
        }

        if (!vtkImage) {
            m_lastError = QString("Failed to load NIfTI into VTK ImageData: %1").arg(filePath);
            qWarning() << "[SegmentationService]" << m_lastError;
            return nullptr;
        }

        int dims[3];
        vtkImage->GetDimensions(dims);
        double spacing[3];
        vtkImage->GetSpacing(spacing);
        qDebug() << "[SegmentationService] ITK loaded - Size:"
                 << dims[0] << "x" << dims[1] << "x" << dims[2]
                 << "Spacing:" << spacing[0] << spacing[1] << spacing[2];

        double range[2];
        vtkImage->GetScalarRange(range);
        qDebug() << "[SegmentationService] Scalar range:" << range[0] << "-" << range[1];

        if (range[0] == range[1]) {
            m_lastError = QString("NIfTI file has no valid data (constant value %1): %2")
                              .arg(range[0]).arg(filePath);
            qWarning() << "[SegmentationService]" << m_lastError;
            return nullptr;
        }

        qDebug() << "[SegmentationService] Successfully loaded NIfTI with ITK";
        return vtkImage;

#if 0
        // 使用 ITK 读取 NIfTI 文件（比 VTK 兼容性更好）
        using PixelType = unsigned char;  // TotalSegmentator 输出是标签图，通常是 uint8
        constexpr unsigned int Dimension = 3;
        using ImageType = itk::Image<PixelType, Dimension>;
        using ReaderType = itk::ImageFileReader<ImageType>;

        auto reader = ReaderType::New();
        reader->SetFileName(filePath.toStdString());

        // 使用 NIfTI IO
        auto niftiIO = itk::NiftiImageIO::New();
        reader->SetImageIO(niftiIO);

        reader->Update();

        ImageType::Pointer itkImage = reader->GetOutput();
        ImageType::RegionType region = itkImage->GetLargestPossibleRegion();
        ImageType::SizeType size = region.GetSize();
        ImageType::SpacingType spacing = itkImage->GetSpacing();
        ImageType::PointType origin = itkImage->GetOrigin();

        qDebug() << "[SegmentationService] ITK loaded - Size:"
                 << size[0] << "x" << size[1] << "x" << size[2]
                 << "Spacing:" << spacing[0] << spacing[1] << spacing[2];

        // 转换为 VTK ImageData
        vtkSmartPointer<vtkImageData> vtkImage = vtkSmartPointer<vtkImageData>::New();
        vtkImage->SetDimensions(size[0], size[1], size[2]);
        vtkImage->SetSpacing(spacing[0], spacing[1], spacing[2]);
        vtkImage->SetOrigin(origin[0], origin[1], origin[2]);
        vtkImage->AllocateScalars(VTK_UNSIGNED_CHAR, 1);

        // 复制数据
        unsigned char* vtkPtr = static_cast<unsigned char*>(vtkImage->GetScalarPointer());
        const unsigned char* itkPtr = itkImage->GetBufferPointer();
        const size_t totalVoxels = size[0] * size[1] * size[2];
        std::memcpy(vtkPtr, itkPtr, totalVoxels * sizeof(unsigned char));

        // 检查数据范围
        double range[2];
        vtkImage->GetScalarRange(range);
        qDebug() << "[SegmentationService] Scalar range:" << range[0] << "-" << range[1];

        if (range[0] == range[1]) {
            m_lastError = QString("NIfTI file has no valid data (constant value %1): %2")
                              .arg(range[0]).arg(filePath);
            qWarning() << "[SegmentationService]" << m_lastError;
            return nullptr;
        }

        qDebug() << "[SegmentationService] Successfully loaded NIfTI with ITK";
        return vtkImage;

#endif
    } catch (const itk::ExceptionObject& ex) {
        m_lastError = QString("ITK exception while reading NIfTI: %1").arg(ex.what());
        qWarning() << "[SegmentationService]" << m_lastError;
        return nullptr;
    } catch (const std::exception& ex) {
        m_lastError = QString("Exception while reading NIfTI: %1").arg(ex.what());
        qWarning() << "[SegmentationService]" << m_lastError;
        return nullptr;
    } catch (...) {
        m_lastError = QString("Unknown exception while reading NIfTI file: %1").arg(filePath);
        qWarning() << "[SegmentationService]" << m_lastError;
        return nullptr;
    }
}

vtkSmartPointer<vtkPolyData> SegmentationServiceImpl::generateMeshFromMask(vtkImageData* mask, double threshold)
{
    if (!mask) {
        m_lastError = "Input mask is null";
        return nullptr;
    }

    // 获取数据范围以确定合适的阈值
    double range[2];
    mask->GetScalarRange(range);
    qDebug() << "[SegmentationService] generateMeshFromMask - input range:" << range[0] << "-" << range[1]
             << "requested threshold:" << threshold;

    // 对于标签掩码（TotalSegmentator输出），值通常是整数 0, 1, 2, ...
    // 阈值应该设置为 0.5 以提取所有非零标签
    // 但如果用户传入的阈值大于最大值，则使用 0.5
    double actualThreshold = threshold;
    if (threshold > range[1]) {
        actualThreshold = 0.5;
        qDebug() << "[SegmentationService] Threshold adjusted to" << actualThreshold
                 << "(original was above max value)";
    }

    const bool enableSmoothing = m_parameters.value("smoothing", true).toBool();

    // 1) 先把多标签/概率图阈值成 0/1（这样等值面在 0.5 附近更稳定，也方便后续做体素级预平滑）
    vtkSmartPointer<vtkImageThreshold> toBinary = vtkSmartPointer<vtkImageThreshold>::New();
    toBinary->SetInputData(mask);
    // 对多标签(0,1,2...)：把所有 >=1 的标签视为前景
    // 对概率/二值(0~1)：使用传入阈值
    const double binaryLower = (range[1] > 1.0) ? 1.0 : actualThreshold;
    toBinary->ThresholdBetween(binaryLower, range[1]);
    toBinary->SetInValue(1);
    toBinary->SetOutValue(0);
    toBinary->ReplaceInOn();
    toBinary->ReplaceOutOn();
    toBinary->SetOutputScalarTypeToUnsignedChar();

    vtkSmartPointer<vtkImageCast> castToFloat = vtkSmartPointer<vtkImageCast>::New();
    castToFloat->SetInputConnection(toBinary->GetOutputPort());
    castToFloat->SetOutputScalarTypeToFloat();

    vtkAlgorithmOutput* contourInput = castToFloat->GetOutputPort();

    // 2) 体素级轻量预平滑：减少“台阶感”（sigma 单位为 voxel）
    vtkSmartPointer<vtkImageGaussianSmooth> gaussian;
    if (enableSmoothing) {
        const double sigma = m_parameters.value("mask_gaussian_sigma", 0.8).toDouble();
        if (sigma > 0.0) {
            gaussian = vtkSmartPointer<vtkImageGaussianSmooth>::New();
            gaussian->SetInputConnection(contourInput);
            gaussian->SetStandardDeviations(sigma, sigma, sigma);
            gaussian->SetRadiusFactors(3.0, 3.0, 3.0);
            contourInput = gaussian->GetOutputPort();
        }
    }

    // 3) 等值面提取（对 0/1 场固定用 0.5）
    vtkSmartPointer<vtkMarchingCubes> marching = vtkSmartPointer<vtkMarchingCubes>::New();
    marching->SetInputConnection(contourInput);
    marching->SetValue(0, 0.5);
    marching->ComputeNormalsOff(); // 统一放到后面计算法线（避免预平滑/后平滑后法线失真）
    marching->Update();

    vtkPolyData* marchingOutput = marching->GetOutput();
    qDebug() << "[SegmentationService] Marching cubes output - points:" << marchingOutput->GetNumberOfPoints()
             << "cells:" << marchingOutput->GetNumberOfCells();

    if (marchingOutput->GetNumberOfPoints() == 0) {
        m_lastError = QString("Marching cubes produced no geometry (threshold=%1, range=%2-%3)")
                          .arg(actualThreshold).arg(range[0]).arg(range[1]);
        qWarning() << "[SegmentationService]" << m_lastError;
        return nullptr;
    }

    // 清理
    vtkSmartPointer<vtkCleanPolyData> cleaner = vtkSmartPointer<vtkCleanPolyData>::New();
    cleaner->SetInputConnection(marching->GetOutputPort());
    cleaner->Update();

    // 4) 网格级平滑：WindowedSinc 比 Laplacian 更适合做“平滑但不太缩水”的表面
    if (enableSmoothing) {
        int iterations = m_parameters.value("smoothing_iterations", 10).toInt();
        if (iterations < 20) {
            iterations = 20;
        }
        const double passBand = m_parameters.value("smoothing_passband", 0.1).toDouble();

        vtkSmartPointer<vtkTriangleFilter> triangulator = vtkSmartPointer<vtkTriangleFilter>::New();
        triangulator->SetInputConnection(cleaner->GetOutputPort());
        triangulator->Update();

        vtkSmartPointer<vtkWindowedSincPolyDataFilter> smoother = vtkSmartPointer<vtkWindowedSincPolyDataFilter>::New();
        smoother->SetInputConnection(triangulator->GetOutputPort());
        smoother->SetNumberOfIterations(iterations);
        smoother->BoundarySmoothingOff();
        smoother->FeatureEdgeSmoothingOff();
        smoother->NonManifoldSmoothingOn();
        smoother->NormalizeCoordinatesOn();
        if (passBand > 0.0 && passBand < 1.0) {
            smoother->SetPassBand(passBand);
        }
        smoother->Update();

        // 法线（用于Phong/Gouraud平滑着色）
        vtkSmartPointer<vtkPolyDataNormals> normals = vtkSmartPointer<vtkPolyDataNormals>::New();
        normals->SetInputConnection(smoother->GetOutputPort());
        normals->ComputePointNormalsOn();
        normals->ComputeCellNormalsOff();
        normals->SplittingOff();
        normals->ConsistencyOn();
        normals->AutoOrientNormalsOn();
        normals->SetFeatureAngle(60.0);
        normals->Update();

        vtkSmartPointer<vtkPolyData> mesh = vtkSmartPointer<vtkPolyData>::New();
        mesh->DeepCopy(normals->GetOutput());

        qDebug() << "[SegmentationService] Final mesh (sinc smoothing) - points:" << mesh->GetNumberOfPoints()
                 << "cells:" << mesh->GetNumberOfCells();
        return mesh;
    }

    vtkSmartPointer<vtkPolyData> mesh = vtkSmartPointer<vtkPolyData>::New();
    mesh->DeepCopy(cleaner->GetOutput());

    qDebug() << "[SegmentationService] Final mesh (no smoothing) - points:" << mesh->GetNumberOfPoints()
             << "cells:" << mesh->GetNumberOfCells();
    return mesh;
}

vtkSmartPointer<vtkPolyData> SegmentationServiceImpl::generateMeshFromIntensity(vtkImageData* volume, double isoValue)
{
    if (!volume) {
        m_lastError = "Input volume is null";
        return nullptr;
    }

    double range[2];
    volume->GetScalarRange(range);
    qDebug() << "[SegmentationService] generateMeshFromIntensity - input range:" << range[0] << "-" << range[1]
             << "requested isoValue:" << isoValue;

    if (range[0] == range[1]) {
        m_lastError = QString("Volume has constant scalar value (%1), cannot generate mesh").arg(range[0]);
        qWarning() << "[SegmentationService]" << m_lastError;
        return nullptr;
    }

    double actualIso = isoValue;
    if (actualIso <= range[0] || actualIso >= range[1]) {
        actualIso = range[0] + (range[1] - range[0]) * 0.5;
        qDebug() << "[SegmentationService] IsoValue adjusted to" << actualIso
                 << "(original was outside scalar range)";
    }

    const bool enableSmoothing = m_parameters.value("smoothing", true).toBool();

    vtkSmartPointer<vtkImageCast> castToFloat = vtkSmartPointer<vtkImageCast>::New();
    castToFloat->SetInputData(volume);
    castToFloat->SetOutputScalarTypeToFloat();

    vtkAlgorithmOutput* contourInput = castToFloat->GetOutputPort();

    vtkSmartPointer<vtkImageGaussianSmooth> gaussian;
    if (enableSmoothing) {
        const double sigma = m_parameters.value("volume_gaussian_sigma", 0.0).toDouble();
        if (sigma > 0.0) {
            gaussian = vtkSmartPointer<vtkImageGaussianSmooth>::New();
            gaussian->SetInputConnection(contourInput);
            gaussian->SetStandardDeviations(sigma, sigma, sigma);
            gaussian->SetRadiusFactors(3.0, 3.0, 3.0);
            contourInput = gaussian->GetOutputPort();
        }
    }

    vtkSmartPointer<vtkMarchingCubes> marching = vtkSmartPointer<vtkMarchingCubes>::New();
    marching->SetInputConnection(contourInput);
    marching->SetValue(0, actualIso);
    marching->ComputeNormalsOff();
    marching->Update();

    vtkPolyData* marchingOutput = marching->GetOutput();
    qDebug() << "[SegmentationService] Marching cubes (intensity) output - points:" << marchingOutput->GetNumberOfPoints()
             << "cells:" << marchingOutput->GetNumberOfCells();

    if (marchingOutput->GetNumberOfPoints() == 0) {
        m_lastError = QString("Marching cubes produced no geometry (isoValue=%1, range=%2-%3)")
                          .arg(actualIso).arg(range[0]).arg(range[1]);
        qWarning() << "[SegmentationService]" << m_lastError;
        return nullptr;
    }

    vtkSmartPointer<vtkCleanPolyData> cleaner = vtkSmartPointer<vtkCleanPolyData>::New();
    cleaner->SetInputConnection(marching->GetOutputPort());
    cleaner->Update();

    if (enableSmoothing) {
        int iterations = m_parameters.value("smoothing_iterations", 10).toInt();
        if (iterations < 20) {
            iterations = 20;
        }
        const double passBand = m_parameters.value("smoothing_passband", 0.1).toDouble();

        vtkSmartPointer<vtkTriangleFilter> triangulator = vtkSmartPointer<vtkTriangleFilter>::New();
        triangulator->SetInputConnection(cleaner->GetOutputPort());
        triangulator->Update();

        vtkSmartPointer<vtkWindowedSincPolyDataFilter> smoother = vtkSmartPointer<vtkWindowedSincPolyDataFilter>::New();
        smoother->SetInputConnection(triangulator->GetOutputPort());
        smoother->SetNumberOfIterations(iterations);
        smoother->BoundarySmoothingOff();
        smoother->FeatureEdgeSmoothingOff();
        smoother->NonManifoldSmoothingOn();
        smoother->NormalizeCoordinatesOn();
        if (passBand > 0.0 && passBand < 1.0) {
            smoother->SetPassBand(passBand);
        }
        smoother->Update();

        vtkSmartPointer<vtkPolyDataNormals> normals = vtkSmartPointer<vtkPolyDataNormals>::New();
        normals->SetInputConnection(smoother->GetOutputPort());
        normals->ComputePointNormalsOn();
        normals->ComputeCellNormalsOff();
        normals->SplittingOff();
        normals->ConsistencyOn();
        normals->AutoOrientNormalsOn();
        normals->SetFeatureAngle(60.0);
        normals->Update();

        vtkSmartPointer<vtkPolyData> mesh = vtkSmartPointer<vtkPolyData>::New();
        mesh->DeepCopy(normals->GetOutput());

        qDebug() << "[SegmentationService] Final mesh (intensity, sinc smoothing) - points:" << mesh->GetNumberOfPoints()
                 << "cells:" << mesh->GetNumberOfCells();
        return mesh;
    }

    vtkSmartPointer<vtkPolyData> mesh = vtkSmartPointer<vtkPolyData>::New();
    mesh->DeepCopy(cleaner->GetOutput());

    qDebug() << "[SegmentationService] Final mesh (intensity, no smoothing) - points:" << mesh->GetNumberOfPoints()
             << "cells:" << mesh->GetNumberOfCells();
    return mesh;
}

bool SegmentationServiceImpl::isLikelyLabelMap(vtkImageData* image, const double range[2]) const
{
    if (!image) {
        return false;
    }

    // Label maps are typically non-negative.
    if (range[0] < 0.0) {
        return false;
    }

    // Probabilities/binary masks are treated as "label-like" here.
    if (range[1] <= 1.5) {
        return true;
    }

    vtkDataArray* scalars = image->GetPointData() ? image->GetPointData()->GetScalars() : nullptr;
    if (!scalars) {
        return false;
    }

    const vtkIdType tuples = scalars->GetNumberOfTuples();
    if (tuples <= 0) {
        return false;
    }

    const vtkIdType targetSamples = 50000;
    const vtkIdType step = std::max<vtkIdType>(1, tuples / targetSamples);

    QSet<qint64> unique;
    unique.reserve(128);

    vtkIdType samples = 0;
    for (vtkIdType i = 0; i < tuples; i += step) {
        unique.insert(qRound64(scalars->GetTuple1(i)));
        ++samples;
        if (unique.size() > 256) {
            break;
        }
    }

    // TotalSegmentator label maps usually have small integer label ranges (0..N).
    const bool fewUnique = unique.size() <= 64;
    const bool smallMax = range[1] <= 512.0;
    const bool likely = fewUnique && smallMax;

    qDebug() << "[SegmentationService] NIfTI auto-detect - range:" << range[0] << "-" << range[1]
             << "unique(sampled):" << unique.size() << "samples:" << samples
             << "=> " << (likely ? "label map" : "intensity");
    return likely;
}

SegmentationTask* SegmentationServiceImpl::findTask(const QString& taskId)
{
    auto it = m_tasks.find(taskId);
    return (it != m_tasks.end()) ? &it.value() : nullptr;
}

const SegmentationTask* SegmentationServiceImpl::findTask(const QString& taskId) const
{
    auto it = m_tasks.find(taskId);
    return (it != m_tasks.end()) ? &it.value() : nullptr;
}

QString SegmentationServiceImpl::findTaskIdByProcess(QProcess* process)
{
    for (auto it = m_tasks.begin(); it != m_tasks.end(); ++it) {
        if (it->process == process) {
            return it->taskId;
        }
    }
    return QString();
}

void SegmentationServiceImpl::updateTaskProgress(const QString& taskId, int progress, const QString& message)
{
    bool shouldEmit = false;
    int boundedProgress = 0;
    {
        QMutexLocker locker(&m_mutex);
        SegmentationTask* task = findTask(taskId);
        if (task) {
            boundedProgress = qBound(0, progress, 100);
            task->progress = boundedProgress;
            shouldEmit = true;
        }
    }
    // 在锁外发射信号，避免死锁和跨线程问题
    if (shouldEmit) {
        emit segmentationProgress(taskId, boundedProgress, message);
    }
}

void SegmentationServiceImpl::completeTask(const QString& taskId, const QVariantMap& result)
{
    bool shouldEmit = false;
    {
        QMutexLocker locker(&m_mutex);
        SegmentationTask* task = findTask(taskId);
        if (task) {
            task->status = "completed";
            task->progress = 100;
            task->result = result;
            task->endTime = QDateTime::currentMSecsSinceEpoch();
            shouldEmit = true;
        }
    }
    // 在锁外发射信号，避免死锁和跨线程问题
    if (shouldEmit) {
        emit segmentationCompleted(taskId, result);
    }
}

void SegmentationServiceImpl::failTask(const QString& taskId, const QString& error)
{
    bool shouldEmit = false;
    {
        QMutexLocker locker(&m_mutex);
        SegmentationTask* task = findTask(taskId);
        if (task) {
            task->status = "failed";
            task->errorMessage = error;
            task->endTime = QDateTime::currentMSecsSinceEpoch();
            m_lastError = error;
            shouldEmit = true;
        }
    }
    // 在锁外发射信号，避免死锁和跨线程问题
    if (shouldEmit) {
        emit segmentationFailed(taskId, error);
    }
}

void SegmentationServiceImpl::loadConfiguration()
{
    // 确定配置文件路径
    QString configPath = getConfigFilePath();
    QFile configFile(configPath);

    qDebug() << "[SegmentationService] Loading configuration from:" << configPath;

    if (!configFile.exists()) {
        qDebug() << "[SegmentationService] Config file not found, using defaults";
        // 尝试自动检测 Python 环境
        autoDetectPythonEnvironment();
        return;
    }

    if (!configFile.open(QIODevice::ReadOnly)) {
        qWarning() << "[SegmentationService] Failed to open config file:" << configFile.errorString();
        return;
    }

    QByteArray data = configFile.readAll();
    configFile.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "[SegmentationService] JSON parse error:" << parseError.errorString();
        return;
    }

    QJsonObject root = doc.object();

    // 读取 Python 配置
    if (root.contains("python")) {
        QJsonObject pythonConfig = root["python"].toObject();
        m_pythonPath = pythonConfig["path"].toString();
        m_totalSegmentatorPath = pythonConfig["totalSegmentatorPath"].toString();
    }

    // 读取分割参数
    if (root.contains("parameters")) {
        QJsonObject params = root["parameters"].toObject();
        for (const QString& key : params.keys()) {
            m_parameters[key] = params[key].toVariant();
        }
    }

    qDebug() << "[SegmentationService] Configuration loaded successfully";
    qDebug() << "[SegmentationService] Python path:" << m_pythonPath;
    qDebug() << "[SegmentationService] TotalSegmentator path:" << m_totalSegmentatorPath;
}

bool SegmentationServiceImpl::saveConfiguration()
{
    QString configPath = getConfigFilePath();

    qDebug() << "[SegmentationService] Saving configuration to:" << configPath;

    // 确保目录存在
    QFileInfo fileInfo(configPath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning() << "[SegmentationService] Failed to create config directory";
            return false;
        }
    }

    QJsonObject root;

    // 写入 Python 配置
    QJsonObject pythonConfig;
    pythonConfig["path"] = m_pythonPath;
    pythonConfig["totalSegmentatorPath"] = m_totalSegmentatorPath;
    root["python"] = pythonConfig;

    // 写入分割参数
    QJsonObject params;
    for (auto it = m_parameters.constBegin(); it != m_parameters.constEnd(); ++it) {
        params[it.key()] = QJsonValue::fromVariant(it.value());
    }
    root["parameters"] = params;

    // 写入文件
    QJsonDocument doc(root);
    QFile configFile(configPath);
    if (!configFile.open(QIODevice::WriteOnly)) {
        qWarning() << "[SegmentationService] Failed to save config file:" << configFile.errorString();
        return false;
    }

    configFile.write(doc.toJson(QJsonDocument::Indented));
    configFile.close();

    qDebug() << "[SegmentationService] Configuration saved successfully";
    return true;
}

QString SegmentationServiceImpl::getConfigFilePath() const
{
    // 优先使用应用程序目录下的 config 文件夹
    QString appDir = QCoreApplication::applicationDirPath();
    QString configPath = appDir + "/config/segmentation_config.json";

    // 如果应用目录不可写，使用用户数据目录
    QFileInfo appDirInfo(appDir + "/config");
    if (!appDirInfo.isWritable() && appDirInfo.exists()) {
        configPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                   + "/config/segmentation_config.json";
    }

    return configPath;
}

void SegmentationServiceImpl::autoDetectPythonEnvironment()
{
    qDebug() << "[SegmentationService] Auto-detecting Python environment...";

    // 检测顺序：
    // 1. 项目目录下的 Python39（推荐，已安装 TotalSegmentator 和 PyTorch）
    // 2. ThirdParty 目录中的 Python 环境
    // 3. 系统 PATH 中的 python

    QString appDir = QCoreApplication::applicationDirPath();

    // 检查 Python 候选路径
    QStringList pythonCandidates;

#ifdef Q_OS_WIN
    // Windows: 优先使用项目目录下的 Python39
    // 项目目录结构: medicalpro/build/.../Release/medicalpro.exe
    // Python39 位置: medicalpro/Python39/python.exe
    pythonCandidates << "D:/Qtproject/medicalpro/Python39/python.exe"  // 项目 Python39（最高优先级）
                     << appDir + "/../../../../Python39/python.exe"     // 相对路径备选
                     << appDir + "/../../../Python39/python.exe"
                     << appDir + "/../../Python39/python.exe"
                     << appDir + "/../ThirdParty/python/python.exe"
                     << appDir + "/../../ThirdParty/python/python.exe"
                     << appDir + "/../../../ThirdParty/python/python.exe"
                     << "python.exe"
                     << "python";
#else
    // Linux/Mac
    pythonCandidates << appDir + "/../../../../Python39/bin/python3"
                     << appDir + "/../ThirdParty/python/bin/python3"
                     << appDir + "/../../ThirdParty/python/bin/python3"
                     << "python3"
                     << "python";
#endif

    for (const QString& candidate : pythonCandidates) {
        QFileInfo fileInfo(candidate);
        // 对于绝对路径，先检查文件是否存在
        if (fileInfo.isAbsolute() && !fileInfo.exists()) {
            continue;
        }

        QProcess testProcess;
        testProcess.start(candidate, QStringList() << "--version");
        if (testProcess.waitForFinished(3000) && testProcess.exitCode() == 0) {
            m_pythonPath = fileInfo.isAbsolute() ? fileInfo.absoluteFilePath() : candidate;
            qDebug() << "[SegmentationService] Found Python at:" << m_pythonPath;
            break;
        }
    }

    // 检查 TotalSegmentator 路径（通过 pip 安装，无需单独路径）
    QString totalSegDir = appDir + "/../ThirdParty/TotalSegmentator";
    if (QDir(totalSegDir).exists()) {
        m_totalSegmentatorPath = QDir(totalSegDir).absolutePath();
        qDebug() << "[SegmentationService] Found TotalSegmentator at:" << m_totalSegmentatorPath;
    }

    // 保存检测到的配置
    if (!m_pythonPath.isEmpty()) {
        saveConfiguration();
    }
}
