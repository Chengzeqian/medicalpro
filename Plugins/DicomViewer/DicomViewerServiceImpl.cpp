#include "DicomViewerServiceImpl.h"
#include "DicomViewerWidget.h"
#include <QApplication>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QVariant>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QPixmap>
#include <QPainter>
#include <QUuid>
#include <QMutexLocker>
#include <QStandardPaths>

// ITK includes for DICOM reading
#include <itkImageFileReader.h>
#include <itkImageSeriesReader.h>
#include <itkGDCMImageIO.h>
#include <itkGDCMSeriesFileNames.h>
#include <itkMetaDataObject.h>
#include <itkRescaleIntensityImageFilter.h>
#include <itkIntensityWindowingImageFilter.h>
#include <itkCastImageFilter.h>
#include <itkImageRegionIterator.h>

// VTK includes for visualization (if needed)
#include <vtkSmartPointer.h>
#include <vtkImageData.h>
#include <vtkDICOMImageReader.h>
#include <vtkImageViewer2.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkImageActor.h>
#include <QDateTime>

// Framework includes
#include "../../Framework/StartupOrchestrator.h"
#include "../../Framework/ErrorHandler.h"
#include "../../Framework/ImageDataTransfer.h"

DicomViewerServiceImpl::DicomViewerServiceImpl(QObject *parent)
    : DicomViewerService(parent)
    , m_databaseName("dicom_viewer.db")
    , m_initialized(false)
    , m_renderingPaused(false)
{
    // 初始化足踝CT预设窗宽窗位
    m_ankleCtPresets = {
        {"骨窗", {300, 1500}},
        {"软组织窗", {50, 400}},
        {"肺窗", {-600, 1600}},
        {"腹部窗", {60, 400}},
        {"自定义", {0, 0}}
    };
}

DicomViewerServiceImpl::~DicomViewerServiceImpl()
{
    shutdown();
}

bool DicomViewerServiceImpl::initialize()
{
    if (m_initialized) {
        return true;
    }
    
    qDebug() << "[DicomViewerService] 初始化DICOM查看器服务...";
    
    if (!setupDatabase()) {
        logError("initialize", "数据库设置失败");
        return false;
    }
    
    if (!createDatabaseTables()) {
        logError("initialize", "数据库表创建失败");
        return false;
    }
    
    m_initialized = true;
    qDebug() << "[DicomViewerService] DICOM查看器服务初始化成功";
    
    return true;
}

void DicomViewerServiceImpl::shutdown()
{
    if (!m_initialized) {
        return;
    }
    
    qDebug() << "[DicomViewerService] 关闭DICOM查看器服务...";
    
    // 清理缓存
    m_patientCache.clear();
    m_studyCache.clear();
    m_seriesCache.clear();
    m_imageCache.clear();
    m_displayParamsCache.clear();
    
    // 关闭数据库连接
    if (m_database.isOpen()) {
        m_database.close();
    }
    
    m_initialized = false;
}

bool DicomViewerServiceImpl::setupDatabase()
{
    // 获取应用数据目录
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(dataDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    QString dbPath = dir.absoluteFilePath(m_databaseName);
    
    // 创建数据库连接
    m_database = QSqlDatabase::addDatabase("QSQLITE", "DicomViewerConnection");
    m_database.setDatabaseName(dbPath);
    
    if (!m_database.open()) {
        logError("setupDatabase", QString("无法打开数据库: %1").arg(m_database.lastError().text()));
        return false;
    }
    
    qDebug() << "[DicomViewerService] 数据库连接成功:" << dbPath;
    return true;
}

bool DicomViewerServiceImpl::createDatabaseTables()
{
    QMutexLocker locker(&m_dbMutex);
    
    bool tablesOk = createPatientsTable() && 
                    createStudiesTable() && 
                    createSeriesTable() && 
                    createImagesTable() && 
                    createAnnotationsTable() && 
                    createDisplayParamsTable();
    
    if (tablesOk) {
        // 执行数据库迁移（添加新字段等）
        migrateDatabaseSchema();
    }
    
    return tablesOk;
}

bool DicomViewerServiceImpl::migrateDatabaseSchema()
{
    qDebug() << "[DicomViewerService] 检查数据库架构迁移...";
    
    // 检查dicom_patients表是否有management_patient_id字段
    QSqlQuery checkQuery(m_database);
    checkQuery.exec("PRAGMA table_info(dicom_patients)");
    bool hasManagementId = false;
    while (checkQuery.next()) {
        QString columnName = checkQuery.value(1).toString();
        if (columnName == "management_patient_id") {
            hasManagementId = true;
            break;
        }
    }
    
    // 如果没有该字段，添加它
    if (!hasManagementId) {
        qDebug() << "[DicomViewerService] 添加management_patient_id字段到dicom_patients表";
        QSqlQuery alterQuery(m_database);
        if (!alterQuery.exec("ALTER TABLE dicom_patients ADD COLUMN management_patient_id INTEGER")) {
            qWarning() << "[DicomViewerService] 添加management_patient_id字段失败:" 
                       << alterQuery.lastError().text();
            return false;
        }
        qDebug() << "[DicomViewerService] management_patient_id字段添加成功";
        
        // 尝试根据姓名自动建立映射关系
        QSqlQuery updateQuery(m_database);
        QString updateSql = R"(
            UPDATE dicom_patients 
            SET management_patient_id = (
                SELECT p.id 
                FROM patients p 
                WHERE p.name = dicom_patients.patient_name 
                LIMIT 1
            )
            WHERE EXISTS (
                SELECT 1 FROM patients p WHERE p.name = dicom_patients.patient_name
            )
        )";
        
        if (updateQuery.exec(updateSql)) {
            int updatedRows = updateQuery.numRowsAffected();
            qDebug() << "[DicomViewerService] 自动建立了" << updatedRows << "个患者ID映射关系";
        } else {
            qWarning() << "[DicomViewerService] 自动映射失败:" << updateQuery.lastError().text();
        }
    }
    
    return true;
}

bool DicomViewerServiceImpl::createPatientsTable()
{
    QSqlQuery query(m_database);
    QString sql = R"(
        CREATE TABLE IF NOT EXISTS dicom_patients (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            patient_id TEXT NOT NULL UNIQUE,
            patient_name TEXT NOT NULL,
            patient_birth_date TEXT,
            patient_sex TEXT,
            patient_age TEXT,
            patient_weight TEXT,
            patient_height TEXT,
            patient_comments TEXT,
            management_patient_id INTEGER,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    
    if (!query.exec(sql)) {
        logError("createPatientsTable", query.lastError().text());
        return false;
    }
    
    // 检查是否需要添加 management_patient_id 列（数据库迁移）
    QSqlQuery checkQuery(m_database);
    checkQuery.exec("PRAGMA table_info(dicom_patients)");
    bool hasManagementIdColumn = false;
    while (checkQuery.next()) {
        QString colName = checkQuery.value(1).toString();
        if (colName == "management_patient_id") {
            hasManagementIdColumn = true;
            break;
        }
    }
    
    if (!hasManagementIdColumn) {
        qDebug() << "[DicomViewerService] 添加 management_patient_id 列到 dicom_patients 表";
        QSqlQuery alterQuery(m_database);
        if (!alterQuery.exec("ALTER TABLE dicom_patients ADD COLUMN management_patient_id INTEGER")) {
            qWarning() << "[DicomViewerService] 添加 management_patient_id 列失败:" << alterQuery.lastError().text();
        } else {
            qDebug() << "[DicomViewerService] management_patient_id 列添加成功";
        }
    }
    
    return true;
}

bool DicomViewerServiceImpl::createStudiesTable()
{
    QSqlQuery query(m_database);
    QString sql = R"(
        CREATE TABLE IF NOT EXISTS dicom_studies (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            patient_db_id INTEGER NOT NULL,
            study_uid TEXT NOT NULL UNIQUE,
            study_id TEXT,
            study_description TEXT,
            study_date DATETIME,
            study_time DATETIME,
            referring_physician TEXT,
            performing_physician TEXT,
            study_comments TEXT,
            accession_number TEXT,
            number_of_series INTEGER DEFAULT 0,
            study_status TEXT DEFAULT 'Active',
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (patient_db_id) REFERENCES dicom_patients(id)
        )
    )";
    
    if (!query.exec(sql)) {
        logError("createStudiesTable", query.lastError().text());
        return false;
    }
    
    return true;
}

bool DicomViewerServiceImpl::createSeriesTable()
{
    QSqlQuery query(m_database);
    QString sql = R"(
        CREATE TABLE IF NOT EXISTS dicom_series (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            study_db_id INTEGER NOT NULL,
            series_uid TEXT NOT NULL UNIQUE,
            series_number TEXT,
            series_description TEXT,
            modality TEXT,
            body_part_examined TEXT,
            scanning_sequence TEXT,
            number_of_images INTEGER DEFAULT 0,
            slice_thickness REAL DEFAULT 1.0,
            pixel_spacing_x REAL DEFAULT 1.0,
            pixel_spacing_y REAL DEFAULT 1.0,
            series_time DATETIME,
            notes TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (study_db_id) REFERENCES dicom_studies(id)
        )
    )";
    
    if (!query.exec(sql)) {
        logError("createSeriesTable", query.lastError().text());
        return false;
    }
    
    return true;
}

bool DicomViewerServiceImpl::createImagesTable()
{
    QSqlQuery query(m_database);
    QString sql = R"(
        CREATE TABLE IF NOT EXISTS dicom_images (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            series_db_id INTEGER NOT NULL,
            instance_uid TEXT NOT NULL UNIQUE,
            sop_class_uid TEXT,
            instance_number INTEGER DEFAULT 0,
            image_path TEXT NOT NULL,
            rows INTEGER DEFAULT 0,
            columns INTEGER DEFAULT 0,
            pixel_spacing_x REAL DEFAULT 1.0,
            pixel_spacing_y REAL DEFAULT 1.0,
            slice_thickness REAL DEFAULT 1.0,
            image_position_x TEXT,
            image_position_y TEXT,
            image_position_z TEXT,
            image_orientation_xx TEXT,
            image_orientation_xy TEXT,
            image_orientation_xz TEXT,
            image_orientation_yx TEXT,
            image_orientation_yy TEXT,
            image_orientation_yz TEXT,
            window_center INTEGER DEFAULT 0,
            window_width INTEGER DEFAULT 0,
            acquisition_time DATETIME,
            notes TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (series_db_id) REFERENCES dicom_series(id)
        )
    )";
    
    if (!query.exec(sql)) {
        logError("createImagesTable", query.lastError().text());
        return false;
    }
    
    return true;
}

bool DicomViewerServiceImpl::createAnnotationsTable()
{
    QSqlQuery query(m_database);
    QString sql = R"(
        CREATE TABLE IF NOT EXISTS dicom_annotations (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            image_db_id INTEGER NOT NULL,
            annotation_type TEXT NOT NULL,
            annotation_text TEXT,
            points_data TEXT,
            measure_value REAL DEFAULT 0.0,
            measure_unit TEXT,
            color TEXT DEFAULT '#ff0000',
            line_width INTEGER DEFAULT 2,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            created_by TEXT,
            FOREIGN KEY (image_db_id) REFERENCES dicom_images(id)
        )
    )";
    
    if (!query.exec(sql)) {
        logError("createAnnotationsTable", query.lastError().text());
        return false;
    }
    
    return true;
}

bool DicomViewerServiceImpl::createDisplayParamsTable()
{
    QSqlQuery query(m_database);
    QString sql = R"(
        CREATE TABLE IF NOT EXISTS dicom_display_params (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            image_db_id INTEGER NOT NULL UNIQUE,
            window_center INTEGER DEFAULT 0,
            window_width INTEGER DEFAULT 0,
            zoom REAL DEFAULT 1.0,
            pan_offset_x REAL DEFAULT 0.0,
            pan_offset_y REAL DEFAULT 0.0,
            invert_image INTEGER DEFAULT 0,
            flip_horizontal INTEGER DEFAULT 0,
            flip_vertical INTEGER DEFAULT 0,
            rotation REAL DEFAULT 0.0,
            color_map TEXT DEFAULT 'grayscale',
            show_annotations INTEGER DEFAULT 1,
            show_measurements INTEGER DEFAULT 1,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (image_db_id) REFERENCES dicom_images(id)
        )
    )";
    
    if (!query.exec(sql)) {
        logError("createDisplayParamsTable", query.lastError().text());
        return false;
    }
    
    return true;
}

// === 病人管理实现 ===
bool DicomViewerServiceImpl::createDicomPatient(const DicomPatientInfo& patient)
{
    if (!patient.isValid()) {
        logError("createDicomPatient", "无效的病人信息");
        return false;
    }
    
    QMutexLocker locker(&m_dbMutex);
    QSqlQuery query(m_database);
    
    QString sql = R"(
        INSERT INTO dicom_patients 
        (patient_id, patient_name, patient_birth_date, patient_sex, 
         patient_age, patient_weight, patient_height, patient_comments)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    query.prepare(sql);
    query.addBindValue(patient.patientID);
    query.addBindValue(patient.patientName);
    query.addBindValue(patient.patientBirthDate);
    query.addBindValue(patient.patientSex);
    query.addBindValue(patient.patientAge);
    query.addBindValue(patient.patientWeight);
    query.addBindValue(patient.patientHeight);
    query.addBindValue(patient.patientComments);
    
    if (!query.exec()) {
        logError("createDicomPatient", query.lastError().text());
        return false;
    }
    
    qDebug() << "[DicomViewerService] 创建DICOM病人记录成功:" << patient.patientName;
    return true;
}

DicomPatientInfo DicomViewerServiceImpl::getDicomPatient(int patientId)
{
    DicomPatientInfo patient;
    
    if (patientId <= 0) {
        return patient;
    }
    
    // 检查缓存
    if (m_patientCache.contains(patientId)) {
        return m_patientCache[patientId];
    }
    
    QMutexLocker locker(&m_dbMutex);
    QSqlQuery query(m_database);
    
    QString sql = R"(
        SELECT id, patient_id, patient_name, patient_birth_date, patient_sex,
               patient_age, patient_weight, patient_height, patient_comments,
               created_at, updated_at
        FROM dicom_patients WHERE id = ?
    )";
    
    query.prepare(sql);
    query.addBindValue(patientId);
    
    if (query.exec() && query.next()) {
        patient.id = query.value("id").toInt();
        patient.patientID = query.value("patient_id").toString();
        patient.patientName = query.value("patient_name").toString();
        patient.patientBirthDate = query.value("patient_birth_date").toString();
        patient.patientSex = query.value("patient_sex").toString();
        patient.patientAge = query.value("patient_age").toString();
        patient.patientWeight = query.value("patient_weight").toString();
        patient.patientHeight = query.value("patient_height").toString();
        patient.patientComments = query.value("patient_comments").toString();
        patient.createdAt = query.value("created_at").toDateTime();
        patient.updatedAt = query.value("updated_at").toDateTime();
        
        // 缓存结果
        m_patientCache[patientId] = patient;
    }
    
    return patient;
}

QList<DicomPatientInfo> DicomViewerServiceImpl::listDicomPatients()
{
    QList<DicomPatientInfo> patients;
    
    QMutexLocker locker(&m_dbMutex);
    QSqlQuery query(m_database);
    
    QString sql = R"(
        SELECT id, patient_id, patient_name, patient_birth_date, patient_sex,
               patient_age, patient_weight, patient_height, patient_comments,
               created_at, updated_at
        FROM dicom_patients ORDER BY created_at DESC
    )";
    
    if (query.exec()) {
        while (query.next()) {
            DicomPatientInfo patient;
            patient.id = query.value("id").toInt();
            patient.patientID = query.value("patient_id").toString();
            patient.patientName = query.value("patient_name").toString();
            patient.patientBirthDate = query.value("patient_birth_date").toString();
            patient.patientSex = query.value("patient_sex").toString();
            patient.patientAge = query.value("patient_age").toString();
            patient.patientWeight = query.value("patient_weight").toString();
            patient.patientHeight = query.value("patient_height").toString();
            patient.patientComments = query.value("patient_comments").toString();
            patient.createdAt = query.value("created_at").toDateTime();
            patient.updatedAt = query.value("updated_at").toDateTime();
            
            patients.append(patient);
        }
    } else {
        logError("listDicomPatients", query.lastError().text());
    }
    
    return patients;
}

// 继续实现其他方法...
// === 图像加载功能 ===
QPixmap DicomViewerServiceImpl::loadDicomFromFile(const QString& filePath, int windowCenter, int windowWidth)
{
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        return QPixmap();
    }
    
    try {
        // 定义ITK图像类型
        typedef signed short PixelType;
        typedef itk::Image<PixelType, 2> ImageType;
        typedef itk::ImageFileReader<ImageType> ReaderType;
        typedef itk::GDCMImageIO ImageIOType;
        
        // 创建DICOM图像读取器
        ReaderType::Pointer reader = ReaderType::New();
        ImageIOType::Pointer dicomIO = ImageIOType::New();
        
        reader->SetFileName(filePath.toStdString());
        reader->SetImageIO(dicomIO);
        
        // 尝试读取文件
        try {
            reader->Update();
        } catch (const itk::ExceptionObject& e) {
            qWarning() << "[DicomViewerService] ITK读取DICOM文件失败:" << e.what();
            // 如果ITK失败，尝试使用VTK
            return loadDicomWithVTK(filePath, windowCenter, windowWidth);
        }
        
        // 获取图像
        ImageType::Pointer image = reader->GetOutput();
        
        // 应用窗宽窗位
        typedef itk::RescaleIntensityImageFilter<ImageType, ImageType> RescaleFilterType;
        RescaleFilterType::Pointer rescaler = RescaleFilterType::New();
        rescaler->SetInput(image);
        
        // 设置输出范围
        rescaler->SetOutputMinimum(0);
        rescaler->SetOutputMaximum(255);
        
        // 计算窗位窗宽的映射范围
        int minIntensity = windowCenter - windowWidth / 2;
        int maxIntensity = windowCenter + windowWidth / 2;
        
        // 使用窗宽窗位过滤器来限制输入范围
        typedef itk::IntensityWindowingImageFilter<ImageType, ImageType> WindowingFilterType;
        WindowingFilterType::Pointer windower = WindowingFilterType::New();
        windower->SetInput(image);
        windower->SetWindowMinimum(minIntensity);
        windower->SetWindowMaximum(maxIntensity);
        windower->SetOutputMinimum(0);
        windower->SetOutputMaximum(255);
        windower->Update();
        
        ImageType::Pointer outputImage = windower->GetOutput();
        
        // 转换为QImage
        ImageType::RegionType region = outputImage->GetLargestPossibleRegion();
        ImageType::SizeType size = region.GetSize();
        
        QImage qImage(size[0], size[1], QImage::Format_Grayscale8);
        
        // 复制像素数据
        itk::ImageRegionIterator<ImageType> it(outputImage, region);
        int y = 0;
        int x = 0;
        
        for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
            unsigned char pixelValue = static_cast<unsigned char>(it.Get());
            qImage.setPixel(x, y, qRgb(pixelValue, pixelValue, pixelValue));
            
            x++;
            if (x >= size[0]) {
                x = 0;
                y++;
            }
        }
        
        qDebug() << "[DicomViewerService] 成功加载DICOM图像:" << filePath;
        qDebug() << "  - 尺寸:" << size[0] << "x" << size[1];
        qDebug() << "  - 窗位/窗宽:" << windowCenter << "/" << windowWidth;
        
        return QPixmap::fromImage(qImage);
        
    } catch (const std::exception& e) {
        qWarning() << "[DicomViewerService] 加载DICOM文件异常:" << e.what();
    } catch (...) {
        qWarning() << "[DicomViewerService] 加载DICOM文件未知异常";
    }
    
    return QPixmap();
}

// 使用VTK作为备选方案加载DICOM
QPixmap DicomViewerServiceImpl::loadDicomWithVTK(const QString& filePath, int windowCenter, int windowWidth)
{
    try {
        vtkSmartPointer<vtkDICOMImageReader> reader = vtkSmartPointer<vtkDICOMImageReader>::New();
        reader->SetFileName(filePath.toStdString().c_str());
        reader->Update();
        
        vtkImageData* imageData = reader->GetOutput();
        if (!imageData) {
            return QPixmap();
        }
        
        // 获取图像维度
        int* dimensions = imageData->GetDimensions();
        int width = dimensions[0];
        int height = dimensions[1];
        
        // 创建QImage
        QImage qImage(width, height, QImage::Format_Grayscale8);
        
        // 获取标量范围
        double* range = imageData->GetScalarRange();
        double slope = 255.0 / windowWidth;
        double intercept = -slope * (windowCenter - windowWidth / 2.0);
        
        // 复制并应用窗宽窗位
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                double value = imageData->GetScalarComponentAsDouble(x, y, 0, 0);
                
                // 应用窗宽窗位
                int pixelValue = static_cast<int>(slope * value + intercept);
                pixelValue = qBound(0, pixelValue, 255);
                
                qImage.setPixel(x, y, qRgb(pixelValue, pixelValue, pixelValue));
            }
        }
        
        qDebug() << "[DicomViewerService] VTK成功加载DICOM图像:" << filePath;
        return QPixmap::fromImage(qImage);
        
    } catch (...) {
        qWarning() << "[DicomViewerService] VTK加载DICOM失败";
    }
    
    return QPixmap();
}

// === 预设窗宽窗位 ===
QList<QPair<QString, QPair<int, int>>> DicomViewerServiceImpl::getAnkleCtPresets()
{
    return m_ankleCtPresets;
}

// === 工具函数 ===
void DicomViewerServiceImpl::logError(const QString& operation, const QString& error)
{
    qWarning() << "[DicomViewerService Error]" << operation << ":" << error;
}

QString DicomViewerServiceImpl::generateUID()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void DicomViewerServiceImpl::onDatabaseError(const QSqlError& error)
{
    logError("Database", error.text());
}

// === 通过PatientManagement ID获取或创建DICOM患者 ===
int DicomViewerServiceImpl::getOrCreatePatientByManagementId(int managementPatientId, const QString& patientName)
{
    if (!m_initialized || managementPatientId <= 0) {
        qWarning() << "[DicomViewerService] getOrCreatePatientByManagementId: 无效参数";
        return -1;
    }
    
    QMutexLocker locker(&m_dbMutex);
    
    // 首先查找是否已存在该management_patient_id的映射
    QSqlQuery query(m_database);
    query.prepare("SELECT id FROM dicom_patients WHERE management_patient_id = ?");
    query.addBindValue(managementPatientId);
    
    if (query.exec() && query.next()) {
        int dicomPatientId = query.value(0).toInt();
        qDebug() << "[DicomViewerService] 找到已存在的DICOM患者映射: managementId=" 
                 << managementPatientId << ", dicomId=" << dicomPatientId;
        return dicomPatientId;
    }
    
    // 如果不存在，创建新的DICOM患者记录
    qDebug() << "[DicomViewerService] 创建新的DICOM患者映射: managementId=" 
             << managementPatientId << ", patientName=" << patientName;
    
    QSqlQuery insertQuery(m_database);
    QString sql = R"(
        INSERT INTO dicom_patients 
        (patient_id, patient_name, management_patient_id)
        VALUES (?, ?, ?)
    )";
    
    insertQuery.prepare(sql);
    insertQuery.addBindValue(QString("PM_%1").arg(managementPatientId));  // patient_id使用前缀+management_id
    insertQuery.addBindValue(patientName);
    insertQuery.addBindValue(managementPatientId);
    
    if (!insertQuery.exec()) {
        logError("getOrCreatePatientByManagementId", insertQuery.lastError().text());
        return -1;
    }
    
    // 获取刚创建的记录ID
    QSqlQuery lastIdQuery(m_database);
    lastIdQuery.exec("SELECT last_insert_rowid()");
    if (lastIdQuery.next()) {
        int newDicomPatientId = lastIdQuery.value(0).toInt();
        qDebug() << "[DicomViewerService] DICOM患者记录创建成功: dicomId=" << newDicomPatientId;
        return newDicomPatientId;
    }
    
    return -1;
}


// === DICOM文件导入实现 ===
bool DicomViewerServiceImpl::importDicomFile(const QString& filePath, int patientId)
{
    if (!m_initialized || filePath.isEmpty() || patientId <= 0) {
        logError("importDicomFile", "无效的参数");
        return false;
    }
    
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        logError("importDicomFile", "文件不存在: " + filePath);
        return false;
    }
    
    try {
        // 使用ITK读取DICOM文件元数据
        typedef itk::GDCMImageIO ImageIOType;
        ImageIOType::Pointer dicomIO = ImageIOType::New();
        
        // 读取文件信息而不加载像素数据
        dicomIO->SetFileName(filePath.toStdString());
        dicomIO->ReadImageInformation();
        
        // 获取元数据字典
        const itk::MetaDataDictionary& dict = dicomIO->GetMetaDataDictionary();
        
        // 辅助函数：从字典获取字符串值
        auto getTagValue = [&dict](const std::string& tag) -> QString {
            std::string value;
            if (itk::ExposeMetaData<std::string>(dict, tag, value)) {
                return QString::fromStdString(value).trimmed();
            }
            return "";
        };
        
        // 解析检查信息
        DicomStudyInfo studyInfo;
        studyInfo.studyUID = getTagValue("0020|000d");  // Study Instance UID
        studyInfo.studyID = getTagValue("0020|0010");    // Study ID
        studyInfo.studyDescription = getTagValue("0008|1030");  // Study Description
        
        // 解析日期时间
        QString studyDate = getTagValue("0008|0020");  // Study Date
        QString studyTime = getTagValue("0008|0030");  // Study Time
        if (!studyDate.isEmpty()) {
            studyInfo.studyDate = QDateTime::fromString(studyDate, "yyyyMMdd");
        } else {
            studyInfo.studyDate = QDateTime::currentDateTime();
        }
        
        studyInfo.referringPhysician = getTagValue("0008|0090");  // Referring Physician
        studyInfo.performingPhysician = getTagValue("0008|1050"); // Performing Physician
        studyInfo.accessionNumber = getTagValue("0008|0050");      // Accession Number
        studyInfo.studyStatus = "COMPLETED";
        studyInfo.patientId = patientId;
        
        qDebug() << "[DicomViewerService] ========== 导入时调试信息 ==========";
        qDebug() << "[DicomViewerService] 导入时传入的patientId参数:" << patientId;
        qDebug() << "[DicomViewerService] 分配给studyInfo.patientId的值:" << studyInfo.patientId;
        qDebug() << "[DicomViewerService] ===============================";
        
        // 输出解析的Study信息用于诊断
        qDebug() << "[DicomViewerService] 解析DICOM Study信息:";
        qDebug() << "  Study UID:" << studyInfo.studyUID;
        qDebug() << "  Study ID:" << studyInfo.studyID;
        qDebug() << "  Study Description:" << studyInfo.studyDescription;
        qDebug() << "  Patient ID:" << studyInfo.patientId;
        qDebug() << "  Study Date:" << studyInfo.studyDate;
        qDebug() << "  Referring Physician:" << studyInfo.referringPhysician;
        qDebug() << "  isValid()检查: id=" << studyInfo.id << ", patientId=" << studyInfo.patientId 
                 << ", studyUID=" << (studyInfo.studyUID.isEmpty() ? "空" : "有值");
        
        // 解析序列信息
        DicomSeriesInfo seriesInfo;
        seriesInfo.seriesUID = getTagValue("0020|000e");        // Series Instance UID
        seriesInfo.seriesNumber = getTagValue("0020|0011");     // Series Number
        seriesInfo.seriesDescription = getTagValue("0008|103e"); // Series Description
        seriesInfo.modality = getTagValue("0008|0060");         // Modality
        seriesInfo.bodyPartExamined = getTagValue("0018|0015"); // Body Part Examined
        
        // 如果是足踝检查，确保正确标记
        if (seriesInfo.bodyPartExamined.isEmpty() || 
            (!seriesInfo.bodyPartExamined.contains("ANKLE", Qt::CaseInsensitive) &&
             !seriesInfo.bodyPartExamined.contains("FOOT", Qt::CaseInsensitive))) {
            seriesInfo.bodyPartExamined = "ANKLE";
        }
        
        // 获取像素间距和层厚
        QString pixelSpacing = getTagValue("0028|0030");  // Pixel Spacing
        if (!pixelSpacing.isEmpty()) {
            QStringList spacing = pixelSpacing.split("\\");
            if (spacing.size() >= 2) {
                seriesInfo.pixelSpacing[0] = spacing[0].toDouble();
                seriesInfo.pixelSpacing[1] = spacing[1].toDouble();
            }
        }
        
        QString sliceThickness = getTagValue("0018|0050");  // Slice Thickness
        if (!sliceThickness.isEmpty()) {
            seriesInfo.sliceThickness = sliceThickness.toDouble();
        }
        
        // 解析图像信息
        DicomImageInfo imageInfo;
        imageInfo.instanceUID = getTagValue("0008|0018");     // SOP Instance UID
        imageInfo.sopClassUID = getTagValue("0008|0016");     // SOP Class UID
        imageInfo.instanceNumber = getTagValue("0020|0013").toInt(); // Instance Number
        imageInfo.imagePath = filePath;
        
        // 获取图像尺寸
        imageInfo.rows = dicomIO->GetDimensions(1);
        imageInfo.columns = dicomIO->GetDimensions(0);
        
        // 获取像素间距
        QString imgPixelSpacing = getTagValue("0028|0030");
        if (!imgPixelSpacing.isEmpty()) {
            QStringList spacing = imgPixelSpacing.split("\\");
            if (spacing.size() >= 2) {
                imageInfo.pixelSpacing[0] = spacing[0].toDouble();
                imageInfo.pixelSpacing[1] = spacing[1].toDouble();
            }
        }
        
        // 获取窗宽窗位
        QString windowCenter = getTagValue("0028|1050");  // Window Center
        QString windowWidth = getTagValue("0028|1051");   // Window Width
        
        if (!windowCenter.isEmpty()) {
            imageInfo.windowCenter = windowCenter.split("\\")[0].toInt();
        } else {
            // 足踝CT的默认窗位
            imageInfo.windowCenter = 300;
        }
        
        if (!windowWidth.isEmpty()) {
            imageInfo.windowWidth = windowWidth.split("\\")[0].toInt();
        } else {
            // 足踝CT的默认窗宽
            imageInfo.windowWidth = 1500;
        }
        
        imageInfo.sliceThickness = seriesInfo.sliceThickness;
        imageInfo.acquisitionTime = QDateTime::currentDateTime();
        
        // 检查是否已存在相同的检查/序列
        QSqlQuery checkQuery(m_database);
        checkQuery.prepare("SELECT id, patient_db_id FROM dicom_studies WHERE study_uid = ?");
        checkQuery.addBindValue(studyInfo.studyUID);
        
        int studyId = -1;
        if (checkQuery.exec() && checkQuery.next()) {
            studyId = checkQuery.value(0).toInt();
            int existingPatientId = checkQuery.value(1).toInt();
            studyInfo.id = studyId;
            
            qDebug() << "[DicomViewerService] 发现已存在的Study，ID:" << studyId;
            qDebug() << "  - 现有patient_db_id:" << existingPatientId;
            qDebug() << "  - 导入请求的patient_db_id:" << patientId;
            
            // 检查patient_db_id是否匹配
            if (existingPatientId != patientId) {
                qWarning() << "[DicomViewerService] 警告：Study UID已存在但关联了不同的患者！";
                qWarning() << "  - Study UID:" << studyInfo.studyUID;
                qWarning() << "  - 原患者ID:" << existingPatientId << "-> 新患者ID:" << patientId;
                qWarning() << "  - 将更新Study的patient_db_id为新患者ID";
                
                // 更新Study的patient_db_id
                QSqlQuery updateQuery(m_database);
                updateQuery.prepare("UPDATE dicom_studies SET patient_db_id = ? WHERE id = ?");
                updateQuery.addBindValue(patientId);
                updateQuery.addBindValue(studyId);
                
                if (!updateQuery.exec()) {
                    qCritical() << "[DicomViewerService] 更新Study的patient_db_id失败:" 
                               << updateQuery.lastError().text();
                    return false;
                }
                
                qDebug() << "[DicomViewerService] Study的patient_db_id已更新为:" << patientId;
            } else {
                qDebug() << "[DicomViewerService] patient_db_id匹配，使用已存在的Study";
            }
        } else {
            // 创建新的检查
            qDebug() << "[DicomViewerService] Study不存在，创建新记录...";
            if (!createDicomStudy(studyInfo)) {
                logError("importDicomFile", "创建检查记录失败");
                return false;
            }
            // 查询刚刚插入的记录ID
            checkQuery.prepare("SELECT id FROM dicom_studies WHERE study_uid = ?");
            checkQuery.addBindValue(studyInfo.studyUID);
            if (checkQuery.exec() && checkQuery.next()) {
                studyId = checkQuery.value(0).toInt();
                qDebug() << "[DicomViewerService] 新创建的Study ID:" << studyId;
            } else {
                logError("importDicomFile", "无法获取新创建的Study ID");
                return false;
            }
        }
        
        // 设置序列的检查ID
        seriesInfo.studyId = studyId;
        
        // 检查序列是否存在
        checkQuery.prepare("SELECT id, study_db_id FROM dicom_series WHERE series_uid = ?");
        checkQuery.addBindValue(seriesInfo.seriesUID);
        
        int seriesId = -1;
        if (checkQuery.exec() && checkQuery.next()) {
            seriesId = checkQuery.value(0).toInt();
            int existingStudyId = checkQuery.value(1).toInt();
            seriesInfo.id = seriesId;
            
            qDebug() << "[DicomViewerService] 发现已存在的Series，ID:" << seriesId;
            qDebug() << "  - 现有study_db_id:" << existingStudyId;
            qDebug() << "  - 导入请求的study_db_id:" << studyId;
            
            // 检查study_db_id是否匹配
            if (existingStudyId != studyId) {
                qWarning() << "[DicomViewerService] 警告：Series UID已存在但关联了不同的Study！";
                qWarning() << "  - Series UID:" << seriesInfo.seriesUID;
                qWarning() << "  - 原Study ID:" << existingStudyId << "-> 新Study ID:" << studyId;
                qWarning() << "  - 将更新Series的study_db_id为新Study ID";
                
                // 更新Series的study_db_id
                QSqlQuery updateQuery(m_database);
                updateQuery.prepare("UPDATE dicom_series SET study_db_id = ? WHERE id = ?");
                updateQuery.addBindValue(studyId);
                updateQuery.addBindValue(seriesId);
                
                if (!updateQuery.exec()) {
                    qCritical() << "[DicomViewerService] 更新Series的study_db_id失败:" 
                               << updateQuery.lastError().text();
                    return false;
                }
                
                qDebug() << "[DicomViewerService] Series的study_db_id已更新为:" << studyId;
            } else {
                qDebug() << "[DicomViewerService] study_db_id匹配，使用已存在的Series";
            }
        } else {
            // 创建新的序列，初始化图像计数为0
            qDebug() << "[DicomViewerService] Series不存在，创建新记录...";
            seriesInfo.numberOfImages = 0;
            if (!createDicomSeries(seriesInfo)) {
                logError("importDicomFile", "创建序列记录失败");
                return false;
            }
            // 查询刚刚插入的记录ID
            checkQuery.prepare("SELECT id FROM dicom_series WHERE series_uid = ?");
            checkQuery.addBindValue(seriesInfo.seriesUID);
            if (checkQuery.exec() && checkQuery.next()) {
                seriesId = checkQuery.value(0).toInt();
                qDebug() << "[DicomViewerService] 新创建的Series ID:" << seriesId;
            } else {
                logError("importDicomFile", "无法获取新创建的Series ID");
                return false;
            }
        }
        
        // 设置图像的序列ID
        imageInfo.seriesId = seriesId;
        
        // 检查该图像是否已经存在（基于 instance_uid）
        QSqlQuery checkImageQuery(m_database);
        checkImageQuery.prepare("SELECT id FROM dicom_images WHERE instance_uid = ?");
        checkImageQuery.addBindValue(imageInfo.instanceUID);
        
        bool imageExists = false;
        int existingImageId = -1;
        
        if (checkImageQuery.exec() && checkImageQuery.next()) {
            imageExists = true;
            existingImageId = checkImageQuery.value(0).toInt();
            qDebug() << "[DicomViewerService] 图像已存在，跳过导入:";
            qDebug() << "  - Instance UID:" << imageInfo.instanceUID;
            qDebug() << "  - 现有ID:" << existingImageId;
            qDebug() << "  - 文件路径:" << filePath;
            // 图像已存在，视为成功导入（但不增加计数）
            return true;
        }
        
        // 图像不存在，创建新记录
        if (!createDicomImage(imageInfo)) {
            logError("importDicomFile", "创建图像记录失败");
            return false;
        }
        
        // 更新Series的图像计数
        QSqlQuery updateQuery(m_database);
        updateQuery.prepare("UPDATE dicom_series SET number_of_images = number_of_images + 1 WHERE id = ?");
        updateQuery.addBindValue(seriesId);
        if (!updateQuery.exec()) {
            qWarning() << "[DicomViewerService] 更新Series图像计数失败:" << updateQuery.lastError().text();
        }
        
        qDebug() << "[DicomViewerService] DICOM文件导入成功:" << filePath;
        qDebug() << "  - 检查:" << studyInfo.studyDescription;
        qDebug() << "  - 序列:" << seriesInfo.seriesDescription;
        qDebug() << "  - 模态:" << seriesInfo.modality;
        qDebug() << "  - 尺寸:" << imageInfo.columns << "x" << imageInfo.rows;
        
        emit dicomDataLoaded(studyId);
        return true;
        
    } catch (const itk::ExceptionObject& e) {
        QString errorMsg = QString("ITK异常: ") + e.what();
        logError("importDicomFile", errorMsg);
        qCritical() << "[DicomViewerService] DICOM导入失败 - ITK异常:";
        qCritical() << "  文件:" << filePath;
        qCritical() << "  错误:" << e.what();
        qCritical() << "  提示: 请确认ITK库已正确配置并支持GDCM";
    } catch (const std::exception& e) {
        QString errorMsg = QString("标准异常: ") + e.what();
        logError("importDicomFile", errorMsg);
        qCritical() << "[DicomViewerService] DICOM导入失败 - 标准异常:";
        qCritical() << "  文件:" << filePath;
        qCritical() << "  错误:" << e.what();
    } catch (...) {
        logError("importDicomFile", "未知异常");
        qCritical() << "[DicomViewerService] DICOM导入失败 - 未知异常:";
        qCritical() << "  文件:" << filePath;
        qCritical() << "  提示: 可能是数据库连接失败或文件格式不正确";
    }
    
    return false;
}

bool DicomViewerServiceImpl::importDicomDirectory(const QString& dirPath, int patientId)
{
    qDebug() << "[DicomViewerService] ========== 开始导入DICOM目录 ==========";
    qDebug() << "  目录:" << dirPath;
    qDebug() << "  病人ID:" << patientId;
    
    if (!m_initialized || dirPath.isEmpty() || patientId <= 0) {
        QString error = QString("无效的参数 - initialized:%1, patientId:%2")
            .arg(m_initialized).arg(patientId);
        logError("importDicomDirectory", error);
        qCritical() << "[DicomViewerService]" << error;
        return false;
    }
    
    QDir dir(dirPath);
    if (!dir.exists()) {
        logError("importDicomDirectory", "目录不存在: " + dirPath);
        qCritical() << "[DicomViewerService] 目录不存在:" << dirPath;
        return false;
    }
    
    // 查找DICOM文件（通常没有扩展名或.dcm扩展名）
    QStringList filters;
    filters << "*.dcm" << "*.DCM";
    
    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files);
    qDebug() << "[DicomViewerService] 找到" << fileList.size() << "个 .dcm 文件";
    
    if (fileList.isEmpty()) {
        // 尝试查找所有文件
        fileList = dir.entryInfoList(QDir::Files);
        qDebug() << "[DicomViewerService] 目录中共有" << fileList.size() << "个文件";
    }
    
    int importedCount = 0;
    int failedCount = 0;
    
    for (const QFileInfo& fileInfo : fileList) {
        // 简单的文件大小过滤（DICOM文件通常较大）
        if (fileInfo.size() > 1024) { // 大于1KB
            qDebug() << "[DicomViewerService] 尝试导入:" << fileInfo.fileName() 
                     << "(" << fileInfo.size() << "bytes)";
            
            if (importDicomFile(fileInfo.absoluteFilePath(), patientId)) {
                importedCount++;
                qDebug() << "[DicomViewerService] ✓ 成功:" << fileInfo.fileName();
            } else {
                failedCount++;
                qDebug() << "[DicomViewerService] ✗ 失败:" << fileInfo.fileName();
            }
        } else {
            qDebug() << "[DicomViewerService] 跳过小文件:" << fileInfo.fileName() 
                     << "(" << fileInfo.size() << "bytes)";
        }
    }
    
    qDebug() << "[DicomViewerService] ========== 导入统计 ==========";
    qDebug() << "  成功:" << importedCount << "个";
    qDebug() << "  失败:" << failedCount << "个";
    qDebug() << "  总计:" << fileList.size() << "个";
    qDebug() << "==========================================";
    
    return importedCount > 0;
}

// === 其他方法的基本实现 ===
bool DicomViewerServiceImpl::createDicomStudy(const DicomStudyInfo& study)
{
    if (!study.isValid()) {
        logError("createDicomStudy", "Study数据无效");
        return false;
    }
    
    qDebug() << "[DicomViewerService] createDicomStudy: 创建Study记录";
    qDebug() << "  - Patient ID:" << study.patientId;
    qDebug() << "  - Study UID:" << study.studyUID;
    qDebug() << "  - Study ID:" << study.studyID;
    qDebug() << "  - Study Description:" << study.studyDescription;
    
    QMutexLocker locker(&m_dbMutex);
    QSqlQuery query(m_database);
    
    QString sql = R"(
        INSERT INTO dicom_studies 
        (patient_db_id, study_uid, study_id, study_description, 
         study_date, study_time, referring_physician, performing_physician,
         study_comments, accession_number, number_of_series, study_status)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    query.prepare(sql);
    query.addBindValue(study.patientId);
    query.addBindValue(study.studyUID);
    query.addBindValue(study.studyID);
    query.addBindValue(study.studyDescription);
    query.addBindValue(study.studyDate);
    query.addBindValue(study.studyTime);
    query.addBindValue(study.referringPhysician);
    query.addBindValue(study.performingPhysician);
    query.addBindValue(study.studyComments);
    query.addBindValue(study.accessionNumber);
    query.addBindValue(study.numberOfSeries);
    query.addBindValue(study.studyStatus);
    
    if (!query.exec()) {
        logError("createDicomStudy", QString("SQL执行失败: %1").arg(query.lastError().text()));
        qCritical() << "[DicomViewerService] 创建Study记录失败:";
        qCritical() << "  SQL错误:" << query.lastError().text();
        qCritical() << "  患者ID:" << study.patientId;
        qCritical() << "  Study UID:" << study.studyUID;
        return false;
    }
    
    int newStudyId = query.lastInsertId().toInt();
    qDebug() << "[DicomViewerService] Study记录创建成功，ID:" << newStudyId;
    qDebug() << "[DicomViewerService] 确认插入的数据: patient_db_id=" << study.patientId << ", study_uid=" << study.studyUID;
    return true;
}

bool DicomViewerServiceImpl::createDicomSeries(const DicomSeriesInfo& series)
{
    if (!series.isValid()) {
        logError("createDicomSeries", "Series数据无效");
        return false;
    }
    
    QMutexLocker locker(&m_dbMutex);
    QSqlQuery query(m_database);
    
    QString sql = R"(
        INSERT INTO dicom_series 
        (study_db_id, series_uid, series_number, series_description,
         modality, body_part_examined, scanning_sequence, number_of_images,
         slice_thickness, pixel_spacing_x, pixel_spacing_y, series_time, notes)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    query.prepare(sql);
    query.addBindValue(series.studyId);
    query.addBindValue(series.seriesUID);
    query.addBindValue(series.seriesNumber);
    query.addBindValue(series.seriesDescription);
    query.addBindValue(series.modality);
    query.addBindValue(series.bodyPartExamined);
    query.addBindValue(series.scanningSequence);
    query.addBindValue(series.numberOfImages);
    query.addBindValue(series.sliceThickness);
    query.addBindValue(series.pixelSpacing[0]);
    query.addBindValue(series.pixelSpacing[1]);
    query.addBindValue(series.seriesTime);
    query.addBindValue(series.notes);
    
    if (!query.exec()) {
        logError("createDicomSeries", QString("SQL执行失败: %1").arg(query.lastError().text()));
        qCritical() << "[DicomViewerService] 创建Series记录失败:";
        qCritical() << "  SQL错误:" << query.lastError().text();
        qCritical() << "  Study ID:" << series.studyId;
        qCritical() << "  Series UID:" << series.seriesUID;
        return false;
    }
    
    qDebug() << "[DicomViewerService] Series记录创建成功，ID:" << query.lastInsertId().toInt();
    return true;
}

bool DicomViewerServiceImpl::createDicomImage(const DicomImageInfo& image)
{
    if (!image.isValid()) {
        logError("createDicomImage", "Image数据无效");
        return false;
    }
    
    QMutexLocker locker(&m_dbMutex);
    QSqlQuery query(m_database);
    
    QString sql = R"(
        INSERT INTO dicom_images 
        (series_db_id, instance_uid, sop_class_uid, instance_number,
         image_path, rows, columns, pixel_spacing_x, pixel_spacing_y,
         slice_thickness, window_center, window_width, acquisition_time, notes)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    query.prepare(sql);
    query.addBindValue(image.seriesId);
    query.addBindValue(image.instanceUID);
    query.addBindValue(image.sopClassUID);
    query.addBindValue(image.instanceNumber);
    query.addBindValue(image.imagePath);
    query.addBindValue(image.rows);
    query.addBindValue(image.columns);
    query.addBindValue(image.pixelSpacing[0]);
    query.addBindValue(image.pixelSpacing[1]);
    query.addBindValue(image.sliceThickness);
    query.addBindValue(image.windowCenter);
    query.addBindValue(image.windowWidth);
    query.addBindValue(image.acquisitionTime);
    query.addBindValue(image.notes);
    
    if (!query.exec()) {
        logError("createDicomImage", QString("SQL执行失败: %1").arg(query.lastError().text()));
        qCritical() << "[DicomViewerService] 创建Image记录失败:";
        qCritical() << "  SQL错误:" << query.lastError().text();
        qCritical() << "  Series ID:" << image.seriesId;
        qCritical() << "  Instance UID:" << image.instanceUID;
        qCritical() << "  Image Path:" << image.imagePath;
        return false;
    }
    
    qDebug() << "[DicomViewerService] Image记录创建成功，ID:" << query.lastInsertId().toInt();
    return true;
}

// === 查询方法实现 ===
QList<DicomStudyInfo> DicomViewerServiceImpl::listStudiesByPatient(int patientId) 
{
    QList<DicomStudyInfo> studies;
    
    if (!m_initialized || patientId <= 0) {
        qDebug() << "[DicomViewerService] listStudiesByPatient: 参数无效，patientId=" << patientId;
        return studies;
    }
    
    QMutexLocker locker(&m_dbMutex);
    
    // 【关键修复】首先通过management_patient_id找到对应的DICOM patient_db_id
    QSqlQuery mappingQuery(m_database);
    mappingQuery.prepare("SELECT id FROM dicom_patients WHERE management_patient_id = ?");
    mappingQuery.addBindValue(patientId);
    
    int dicomPatientId = -1;
    if (mappingQuery.exec() && mappingQuery.next()) {
        dicomPatientId = mappingQuery.value(0).toInt();
        qDebug() << "[DicomViewerService] 找到患者ID映射: managementId=" << patientId 
                 << "-> dicomId=" << dicomPatientId;
    } else {
        qDebug() << "[DicomViewerService] 未找到患者ID映射，managementId=" << patientId;
        qDebug() << "[DicomViewerService] 该患者可能还没有导入任何DICOM数据";
        return studies;
    }
    
    // 使用DICOM patient_db_id查询studies
    QSqlQuery query(m_database);
    QString sql = R"(
        SELECT id, patient_db_id, study_uid, study_id, study_description,
               study_date, study_time, referring_physician, performing_physician,
               study_comments, accession_number, number_of_series, study_status
        FROM dicom_studies
        WHERE patient_db_id = ?
        ORDER BY study_date DESC
    )";
    
    query.prepare(sql);
    query.addBindValue(dicomPatientId);
    
    qDebug() << "[DicomViewerService] 查询DICOM studies，dicomPatientId=" << dicomPatientId;
    
    if (!query.exec()) {
        logError("listStudiesByPatient", QString("查询失败: %1").arg(query.lastError().text()));
        return studies;
    }
    
    while (query.next()) {
        DicomStudyInfo study;
        study.id = query.value(0).toInt();
        study.patientId = query.value(1).toInt();
        study.studyUID = query.value(2).toString();
        study.studyID = query.value(3).toString();
        study.studyDescription = query.value(4).toString();
        study.studyDate = query.value(5).toDateTime();
        study.studyTime = query.value(6).toDateTime();
        study.referringPhysician = query.value(7).toString();
        study.performingPhysician = query.value(8).toString();
        study.studyComments = query.value(9).toString();
        study.accessionNumber = query.value(10).toString();
        study.numberOfSeries = query.value(11).toInt();
        study.studyStatus = query.value(12).toString();
        
        studies.append(study);
    }
    
    qDebug() << "[DicomViewerService] 查询到患者(managementId=" << patientId 
             << ", dicomId=" << dicomPatientId << ")的" << studies.size() << "个检查";
    return studies;
}

DicomStudyInfo DicomViewerServiceImpl::getDicomStudy(int studyId) 
{
    DicomStudyInfo study;
    
    if (!m_initialized || studyId <= 0) {
        return study;
    }
    
    QMutexLocker locker(&m_dbMutex);
    QSqlQuery query(m_database);
    
    QString sql = R"(
        SELECT id, patient_db_id, study_uid, study_id, study_description,
               study_date, study_time, referring_physician, performing_physician,
               study_comments, accession_number, number_of_series, study_status
        FROM dicom_studies
        WHERE id = ?
    )";
    
    query.prepare(sql);
    query.addBindValue(studyId);
    
    if (query.exec() && query.next()) {
        study.id = query.value(0).toInt();
        study.patientId = query.value(1).toInt();
        study.studyUID = query.value(2).toString();
        study.studyID = query.value(3).toString();
        study.studyDescription = query.value(4).toString();
        study.studyDate = query.value(5).toDateTime();
        study.studyTime = query.value(6).toDateTime();
        study.referringPhysician = query.value(7).toString();
        study.performingPhysician = query.value(8).toString();
        study.studyComments = query.value(9).toString();
        study.accessionNumber = query.value(10).toString();
        study.numberOfSeries = query.value(11).toInt();
        study.studyStatus = query.value(12).toString();
    }
    
    return study;
}

QList<DicomSeriesInfo> DicomViewerServiceImpl::listSeriesByStudy(int studyId) 
{
    QList<DicomSeriesInfo> seriesList;
    
    if (!m_initialized || studyId <= 0) {
        qDebug() << "[DicomViewerService] listSeriesByStudy: 参数无效，studyId=" << studyId;
        return seriesList;
    }
    
    QMutexLocker locker(&m_dbMutex);
    QSqlQuery query(m_database);
    
    // 动态查询实际的图像数量，不依赖于number_of_images字段
    QString sql = R"(
        SELECT s.id, s.study_db_id, s.series_uid, s.series_number, s.series_description,
               s.modality, s.body_part_examined, s.scanning_sequence,
               COUNT(i.id) as actual_image_count,
               s.slice_thickness, s.pixel_spacing_x, s.pixel_spacing_y, s.series_time, s.notes
        FROM dicom_series s
        LEFT JOIN dicom_images i ON s.id = i.series_db_id
        WHERE s.study_db_id = ?
        GROUP BY s.id, s.study_db_id, s.series_uid, s.series_number, s.series_description,
                 s.modality, s.body_part_examined, s.scanning_sequence,
                 s.slice_thickness, s.pixel_spacing_x, s.pixel_spacing_y, s.series_time, s.notes
        ORDER BY s.series_number
    )";
    
    query.prepare(sql);
    query.addBindValue(studyId);
    
    if (!query.exec()) {
        logError("listSeriesByStudy", QString("查询失败: %1").arg(query.lastError().text()));
        return seriesList;
    }
    
    while (query.next()) {
        DicomSeriesInfo series;
        series.id = query.value(0).toInt();
        series.studyId = query.value(1).toInt();
        series.seriesUID = query.value(2).toString();
        series.seriesNumber = query.value(3).toString();
        series.seriesDescription = query.value(4).toString();
        series.modality = query.value(5).toString();
        series.bodyPartExamined = query.value(6).toString();
        series.scanningSequence = query.value(7).toString();
        series.numberOfImages = query.value(8).toInt();  // 使用实际的图像计数
        series.sliceThickness = query.value(9).toDouble();
        series.pixelSpacing[0] = query.value(10).toDouble();
        series.pixelSpacing[1] = query.value(11).toDouble();
        series.seriesTime = query.value(12).toDateTime();
        series.notes = query.value(13).toString();
        
        seriesList.append(series);
    }
    
    qDebug() << "[DicomViewerService] 查询到Study" << studyId << "的" << seriesList.size() << "个序列";
    return seriesList;
}

DicomSeriesInfo DicomViewerServiceImpl::getDicomSeries(int seriesId) 
{
    DicomSeriesInfo series;
    
    if (!m_initialized || seriesId <= 0) {
        return series;
    }
    
    QMutexLocker locker(&m_dbMutex);
    QSqlQuery query(m_database);
    
    QString sql = R"(
        SELECT id, study_db_id, series_uid, series_number, series_description,
               modality, body_part_examined, scanning_sequence, number_of_images,
               slice_thickness, pixel_spacing_x, pixel_spacing_y, series_time, notes
        FROM dicom_series
        WHERE id = ?
    )";
    
    query.prepare(sql);
    query.addBindValue(seriesId);
    
    if (query.exec() && query.next()) {
        series.id = query.value(0).toInt();
        series.studyId = query.value(1).toInt();
        series.seriesUID = query.value(2).toString();
        series.seriesNumber = query.value(3).toString();
        series.seriesDescription = query.value(4).toString();
        series.modality = query.value(5).toString();
        series.bodyPartExamined = query.value(6).toString();
        series.scanningSequence = query.value(7).toString();
        series.numberOfImages = query.value(8).toInt();
        series.sliceThickness = query.value(9).toDouble();
        series.pixelSpacing[0] = query.value(10).toDouble();
        series.pixelSpacing[1] = query.value(11).toDouble();
        series.seriesTime = query.value(12).toDateTime();
        series.notes = query.value(13).toString();
    }
    
    return series;
}

bool DicomViewerServiceImpl::deleteSeries(int seriesId)
{
    if (!m_initialized || seriesId <= 0) {
        qDebug() << "[DicomViewerService] deleteSeries: 参数无效，seriesId=" << seriesId;
        return false;
    }
    
    QMutexLocker locker(&m_dbMutex);
    
    // 开始事务
    m_database.transaction();
    
    try {
        QSqlQuery query(m_database);
        
        // 1. 获取该序列的所有图像信息（用于删除物理文件）
        QList<DicomImageInfo> images = listImagesBySeries(seriesId);
        
        // 2. 删除该序列的所有图像记录
        QString deleteImagesSql = "DELETE FROM dicom_images WHERE series_db_id = ?";
        query.prepare(deleteImagesSql);
        query.addBindValue(seriesId);
        if (!query.exec()) {
            qDebug() << "[DicomViewerService] deleteSeries: 删除图像记录失败:" << query.lastError().text();
            m_database.rollback();
            return false;
        }
        qDebug() << "[DicomViewerService] 已删除" << query.numRowsAffected() << "条图像记录";
        
        // 3. 删除该序列所有图像的标注
        for (const DicomImageInfo& img : images) {
            QString deleteAnnotationsSql = "DELETE FROM dicom_annotations WHERE image_db_id = ?";
            query.prepare(deleteAnnotationsSql);
            query.addBindValue(img.id);
            if (!query.exec()) {
                qDebug() << "[DicomViewerService] deleteSeries: 删除标注失败:" << query.lastError().text();
                // 继续删除，标注失败不影响整体删除
            }
        }
        
        // 4. 删除该序列所有图像的显示参数
        for (const DicomImageInfo& img : images) {
            QString deleteDisplayParamsSql = "DELETE FROM dicom_display_params WHERE image_db_id = ?";
            query.prepare(deleteDisplayParamsSql);
            query.addBindValue(img.id);
            if (!query.exec()) {
                qDebug() << "[DicomViewerService] deleteSeries: 删除显示参数失败:" << query.lastError().text();
                // 继续删除，显示参数失败不影响整体删除
            }
        }
        
        // 5. 删除序列记录
        QString deleteSeriesSql = "DELETE FROM dicom_series WHERE id = ?";
        query.prepare(deleteSeriesSql);
        query.addBindValue(seriesId);
        if (!query.exec()) {
            qDebug() << "[DicomViewerService] deleteSeries: 删除序列记录失败:" << query.lastError().text();
            m_database.rollback();
            return false;
        }
        
        // 6. 提交事务
        if (!m_database.commit()) {
            qDebug() << "[DicomViewerService] deleteSeries: 事务提交失败:" << m_database.lastError().text();
            m_database.rollback();
            return false;
        }
        
        // 7. 清除缓存
        m_seriesCache.remove(seriesId);
        for (const DicomImageInfo& img : images) {
            m_imageCache.remove(img.id);
            m_displayParamsCache.remove(img.id);
        }
        
        qDebug() << "[DicomViewerService] 成功删除序列，seriesId=" << seriesId 
                 << "，共删除" << images.size() << "张图像";
        
        return true;
        
    } catch (...) {
        qDebug() << "[DicomViewerService] deleteSeries: 发生异常";
        m_database.rollback();
        return false;
    }
}

QList<DicomImageInfo> DicomViewerServiceImpl::listImagesBySeries(int seriesId) 
{
    QList<DicomImageInfo> images;
    
    if (!m_initialized || seriesId <= 0) {
        qDebug() << "[DicomViewerService] listImagesBySeries: 参数无效，seriesId=" << seriesId;
        return images;
    }
    
    QMutexLocker locker(&m_dbMutex);
    QSqlQuery query(m_database);
    
    QString sql = R"(
        SELECT id, series_db_id, instance_uid, sop_class_uid, instance_number,
               image_path, rows, columns, pixel_spacing_x, pixel_spacing_y,
               slice_thickness, window_center, window_width, acquisition_time, notes
        FROM dicom_images
        WHERE series_db_id = ?
        ORDER BY instance_number
    )";
    
    query.prepare(sql);
    query.addBindValue(seriesId);
    
    if (!query.exec()) {
        logError("listImagesBySeries", QString("查询失败: %1").arg(query.lastError().text()));
        return images;
    }
    
    while (query.next()) {
        DicomImageInfo image;
        image.id = query.value(0).toInt();
        image.seriesId = query.value(1).toInt();
        image.instanceUID = query.value(2).toString();
        image.sopClassUID = query.value(3).toString();
        image.instanceNumber = query.value(4).toInt();
        image.imagePath = query.value(5).toString();
        image.rows = query.value(6).toInt();
        image.columns = query.value(7).toInt();
        image.pixelSpacing[0] = query.value(8).toDouble();
        image.pixelSpacing[1] = query.value(9).toDouble();
        image.sliceThickness = query.value(10).toDouble();
        image.windowCenter = query.value(11).toInt();
        image.windowWidth = query.value(12).toInt();
        image.acquisitionTime = query.value(13).toDateTime();
        image.notes = query.value(14).toString();
        
        images.append(image);
    }
    
    qDebug() << "[DicomViewerService] 查询到Series" << seriesId << "的" << images.size() << "个图像";
    return images;
}

DicomImageInfo DicomViewerServiceImpl::getDicomImage(int imageId) 
{
    DicomImageInfo image;
    
    if (!m_initialized || imageId <= 0) {
        return image;
    }
    
    QMutexLocker locker(&m_dbMutex);
    QSqlQuery query(m_database);
    
    QString sql = R"(
        SELECT id, series_db_id, instance_uid, sop_class_uid, instance_number,
               image_path, rows, columns, pixel_spacing_x, pixel_spacing_y,
               slice_thickness, window_center, window_width, acquisition_time, notes
        FROM dicom_images
        WHERE id = ?
    )";
    
    query.prepare(sql);
    query.addBindValue(imageId);
    
    if (query.exec() && query.next()) {
        image.id = query.value(0).toInt();
        image.seriesId = query.value(1).toInt();
        image.instanceUID = query.value(2).toString();
        image.sopClassUID = query.value(3).toString();
        image.instanceNumber = query.value(4).toInt();
        image.imagePath = query.value(5).toString();
        image.rows = query.value(6).toInt();
        image.columns = query.value(7).toInt();
        image.pixelSpacing[0] = query.value(8).toDouble();
        image.pixelSpacing[1] = query.value(9).toDouble();
        image.sliceThickness = query.value(10).toDouble();
        image.windowCenter = query.value(11).toInt();
        image.windowWidth = query.value(12).toInt();
        image.acquisitionTime = query.value(13).toDateTime();
        image.notes = query.value(14).toString();
    }
    
    return image;
}
void DicomViewerServiceImpl::requestImageData(int seriesId, int imageIndex, const DicomDisplayParams& params)
{
    if (!m_initialized || seriesId <= 0 || imageIndex < 0) {
        return;
    }

    QList<DicomImageInfo> images = listImagesBySeries(seriesId);
    if (imageIndex >= images.size()) {
        return;
    }

    const DicomImageInfo imageInfo = images.at(imageIndex);
    const QPixmap pixmap = loadDicomFromFile(imageInfo.imagePath, params.windowCenter, params.windowWidth);
    if (pixmap.isNull()) {
        StartupOrchestrator::instance()->logDiagnostic(
            ErrorHandler::ErrorLevel::Warning,
            QStringLiteral("无法加载DICOM图像: %1").arg(imageInfo.imagePath));
        return;
    }

    ImageData data;
    data.setQImage(pixmap.toImage());
    QVariantMap metadata;
    metadata.insert(QStringLiteral("seriesId"), seriesId);
    metadata.insert(QStringLiteral("imageId"), imageInfo.id);
    metadata.insert(QStringLiteral("windowWidth"), params.windowWidth);
    metadata.insert(QStringLiteral("windowCenter"), params.windowCenter);
    data.setMetadata(metadata);

    emit imageDataReady(seriesId, imageIndex, data);
}
bool DicomViewerServiceImpl::createAnnotation(const DicomAnnotation& annotation) { return false; }
QList<DicomAnnotation> DicomViewerServiceImpl::listAnnotationsByImage(int imageId) { return {}; }
bool DicomViewerServiceImpl::deleteAnnotation(int annotationId) { return false; }
bool DicomViewerServiceImpl::updateAnnotation(const DicomAnnotation& annotation) { return false; }
bool DicomViewerServiceImpl::saveDisplayParams(int imageId, const DicomDisplayParams& params) { return false; }
DicomDisplayParams DicomViewerServiceImpl::getDisplayParams(int imageId) { return {}; }

// ========== Widget工厂方法实现 ==========

QPixmap DicomViewerServiceImpl::loadDicomPixmap(const DicomImageInfo& image, const DicomDisplayParams& params)
{
    Q_UNUSED(image);
    Q_UNUSED(params);
    return QPixmap();
}

// ========== Widget工厂方法实现 ==========

QWidget* DicomViewerServiceImpl::createDicomViewerWidget(QWidget* parent)
{
    qDebug() << "[DicomViewerService] 创建DicomViewerWidget";

    // 创建插件内部的DicomViewerWidget
    DicomViewerWidget* widget = new DicomViewerWidget(this, parent);

    // 跟踪创建的Widget用于渲染控制
    m_createdWidgets.append(widget);

    qDebug() << "[DicomViewerService] DicomViewerWidget创建成功";
    return widget;
}

// ========== VTK渲染控制实现 ==========

void DicomViewerServiceImpl::pauseRendering()
{
    m_renderingPaused = true;

    // 暂停所有创建的Widget的渲染
    for (QWidget* widget : m_createdWidgets) {
        if (widget && widget->isVisible()) {
            widget->setUpdatesEnabled(false);
        }
    }

    qDebug() << "[DicomViewerService] Rendering paused";
}

void DicomViewerServiceImpl::resumeRendering()
{
    m_renderingPaused = false;

    // 恢复所有创建的Widget的渲染
    for (QWidget* widget : m_createdWidgets) {
        if (widget) {
            widget->setUpdatesEnabled(true);
            widget->update();
        }
    }

    qDebug() << "[DicomViewerService] Rendering resumed";
}
