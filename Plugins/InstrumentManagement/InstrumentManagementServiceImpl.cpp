#include "InstrumentManagementServiceImpl.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QSettings>
#include <QFile>
#include <QTextStream>

// VTK头文件（用于生成缩略图和高质量预览）
#ifdef VTK_FOUND
#include <vtkSmartPointer.h>
#include <vtkSTLReader.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkWindowToImageFilter.h>
#include <vtkPNGWriter.h>
#include <vtkCamera.h>
#include <vtkProperty.h>
#include <vtkCleanPolyData.h>
#include <vtkTriangleFilter.h>
#include <vtkPolyDataNormals.h>
#include <vtkLight.h>  // 用于三点照明系统
#endif

InstrumentManagementServiceImpl::InstrumentManagementServiceImpl(QObject* parent)
    : InstrumentManagementService(parent)
{
    qDebug() << "[InstrumentManagementService] 初始化服务";
    
    // 获取项目路径
    m_projectPath = getProjectPath();
    m_modelsPath = m_projectPath + "/plustoolkitModels";
    m_geometryPath = m_projectPath + "/geometry";
    m_thumbnailsPath = m_projectPath + "/data/instrumentThumbnails";
    
    // 确保目录存在
    ensureDirectoriesExist();
    
    // 初始化数据库
    initializeDatabase();
}

InstrumentManagementServiceImpl::~InstrumentManagementServiceImpl()
{
    qDebug() << "[InstrumentManagementService] 服务销毁";
    if (m_database.isOpen()) {
        m_database.close();
    }
}

// ========== 辅助方法 ==========

QString InstrumentManagementServiceImpl::getProjectPath() const
{
    // 获取可执行文件所在目录的上级目录（项目根目录）
    QString appPath = QCoreApplication::applicationDirPath();
    QDir dir(appPath);
    
    qDebug() << "[InstrumentManagementService] 应用程序目录:" << appPath;
    qDebug() << "[InstrumentManagementService] 当前目录名:" << dir.dirName();
    
    // 如果在build目录下，向上查找项目根目录
    // 例如：D:/Qtproject/medicalpro/build/Desktop_Qt_5_15_2_MSVC2019_64bit/Release
    //       需要回到 D:/Qtproject/medicalpro
    while (dir.dirName() != "medicalpro" && !dir.isRoot()) {
        QString currentName = dir.dirName();
        if (currentName == "Release" || currentName == "Debug" || 
            currentName.contains("build", Qt::CaseInsensitive) ||
            currentName.contains("Desktop_", Qt::CaseInsensitive) ||
            currentName.contains("MSVC", Qt::CaseInsensitive)) {
            dir.cdUp();
        } else {
            break;
        }
    }
    
    QString projectPath = dir.absolutePath();
    qDebug() << "[InstrumentManagementService] 项目根目录:" << projectPath;
    
    return projectPath;
}

void InstrumentManagementServiceImpl::ensureDirectoriesExist()
{
    QDir dir;
    if (!dir.exists(m_thumbnailsPath)) {
        dir.mkpath(m_thumbnailsPath);
        qDebug() << "[InstrumentManagementService] 创建缩略图目录:" << m_thumbnailsPath;
    }
}

bool InstrumentManagementServiceImpl::checkDatabaseConnection() const
{
    if (!m_database.isOpen()) {
        m_lastError = "数据库未连接";
        return false;
    }
    return true;
}

bool InstrumentManagementServiceImpl::executeQuery(QSqlQuery& query)
{
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        qWarning() << "[InstrumentManagementService] SQL错误:" << m_lastError;
        qWarning() << "[InstrumentManagementService] SQL语句:" << query.lastQuery();
        emit databaseError(m_lastError);
        return false;
    }
    return true;
}

void InstrumentManagementServiceImpl::logMessage(const QString& level, const QString& message) const
{
    qDebug() << QString("[InstrumentManagementService][%1] %2").arg(level).arg(message);
}

// ========== 数据库管理 ==========

bool InstrumentManagementServiceImpl::initializeDatabase()
{
    QMutexLocker locker(&m_mutex);
    
    // 获取共享的数据库连接（与UserManagement共用）
    if (QSqlDatabase::contains("medical_db")) {
        m_database = QSqlDatabase::database("medical_db");
        qDebug() << "[InstrumentManagementService] 使用现有数据库连接";
    } else {
        // 创建新的数据库连接
        m_database = QSqlDatabase::addDatabase("QSQLITE", "medical_db");
        QString dbPath = m_projectPath + "/data/medical.db";
        m_database.setDatabaseName(dbPath);
        
        if (!m_database.open()) {
            m_lastError = "无法打开数据库: " + m_database.lastError().text();
            logMessage("ERROR", m_lastError);
            return false;
        }
        
        qDebug() << "[InstrumentManagementService] 创建新数据库连接:" << dbPath;
    }
    
    // 创建表
    QSqlQuery query(m_database);
    
    // 器械基础信息表
    QString createInstrumentsTable = R"(
        CREATE TABLE IF NOT EXISTS instruments (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            model TEXT,
            serial TEXT,
            category TEXT,
            status TEXT DEFAULT '在库',
            description TEXT,
            is_active INTEGER DEFAULT 1,
            created_at TEXT,
            updated_at TEXT,
            
            model_file_path TEXT,
            thumbnail_path TEXT,
            display_color TEXT DEFAULT '255,215,0',
            model_scale REAL DEFAULT 1.0,
            
            tracking_marker_id TEXT,
            geometry_file_path TEXT,
            is_calibrated INTEGER DEFAULT 0,
            calibration_time TEXT,
            tip_offset_x REAL DEFAULT 0.0,
            tip_offset_y REAL DEFAULT 0.0,
            tip_offset_z REAL DEFAULT 0.0
        )
    )";
    
    if (!query.exec(createInstrumentsTable)) {
        m_lastError = "创建instruments表失败: " + query.lastError().text();
        logMessage("ERROR", m_lastError);
        return false;
    }
    
    // 器械校准数据表
    QString createCalibrationsTable = R"(
        CREATE TABLE IF NOT EXISTS instrument_calibrations (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            instrument_id INTEGER NOT NULL,
            calibration_type TEXT,
            calibration_time TEXT,
            tip_offset_x REAL,
            tip_offset_y REAL,
            tip_offset_z REAL,
            rmse REAL,
            point_count INTEGER,
            calibration_note TEXT,
            is_valid INTEGER DEFAULT 1,
            FOREIGN KEY (instrument_id) REFERENCES instruments(id)
        )
    )";
    
    if (!query.exec(createCalibrationsTable)) {
        m_lastError = "创建instrument_calibrations表失败: " + query.lastError().text();
        logMessage("ERROR", m_lastError);
        return false;
    }
    
    // 器械使用记录表
    QString createUsageTable = R"(
        CREATE TABLE IF NOT EXISTS instrument_usage (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            instrument_id INTEGER NOT NULL,
            surgery_id INTEGER,
            patient_id INTEGER,
            start_time TEXT,
            end_time TEXT,
            duration_minutes INTEGER,
            usage_note TEXT,
            FOREIGN KEY (instrument_id) REFERENCES instruments(id)
        )
    )";
    
    if (!query.exec(createUsageTable)) {
        m_lastError = "创建instrument_usage表失败: " + query.lastError().text();
        logMessage("ERROR", m_lastError);
        return false;
    }
    
    // 器械维护记录表
    QString createMaintenanceTable = R"(
        CREATE TABLE IF NOT EXISTS instrument_maintenance (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            instrument_id INTEGER NOT NULL,
            maintenance_type TEXT,
            maintenance_time TEXT,
            performed_by TEXT,
            description TEXT,
            result TEXT,
            cost REAL DEFAULT 0.0,
            FOREIGN KEY (instrument_id) REFERENCES instruments(id)
        )
    )";
    
    if (!query.exec(createMaintenanceTable)) {
        m_lastError = "创建instrument_maintenance表失败: " + query.lastError().text();
        logMessage("ERROR", m_lastError);
        return false;
    }
    
    // 器械-手术关联表
    QString createInstrumentSurgeryTable = R"(
        CREATE TABLE IF NOT EXISTS instrument_surgery_link (
            instrument_id INTEGER NOT NULL,
            surgery_id INTEGER NOT NULL,
            created_at TEXT,
            PRIMARY KEY (instrument_id, surgery_id),
            FOREIGN KEY (instrument_id) REFERENCES instruments(id),
            FOREIGN KEY (surgery_id) REFERENCES surgery_items(id)
        )
    )";
    
    if (!query.exec(createInstrumentSurgeryTable)) {
        m_lastError = "创建instrument_surgery_link表失败: " + query.lastError().text();
        logMessage("ERROR", m_lastError);
        return false;
    }
    
    logMessage("INFO", "数据库表初始化完成");
    return true;
}

bool InstrumentManagementServiceImpl::isDatabaseConnected()
{
    return checkDatabaseConnection();
}

QString InstrumentManagementServiceImpl::getLastError() const
{
    return m_lastError;
}

// ========== 器械基础管理 ==========

int InstrumentManagementServiceImpl::addInstrument(const InstrumentItem& instrument)
{
    QMutexLocker locker(&m_mutex);
    
    if (!checkDatabaseConnection()) {
        return -1;
    }
    
    QSqlQuery query(m_database);
    query.prepare(R"(
        INSERT INTO instruments (
            name, model, serial, category, status, description, is_active,
            created_at, updated_at,
            model_file_path, thumbnail_path, display_color, model_scale,
            tracking_marker_id, geometry_file_path, is_calibrated, calibration_time,
            tip_offset_x, tip_offset_y, tip_offset_z
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");
    
    QDateTime now = QDateTime::currentDateTime();
    query.addBindValue(instrument.name);
    query.addBindValue(instrument.model);
    query.addBindValue(instrument.serial);
    query.addBindValue(instrument.category);
    query.addBindValue(instrument.status.isEmpty() ? "在库" : instrument.status);
    query.addBindValue(instrument.description);
    query.addBindValue(instrument.isActive ? 1 : 0);
    query.addBindValue(now.toString(Qt::ISODate));
    query.addBindValue(now.toString(Qt::ISODate));
    query.addBindValue(instrument.modelFilePath);
    query.addBindValue(instrument.thumbnailPath);
    query.addBindValue(instrument.displayColor);
    query.addBindValue(instrument.modelScale);
    query.addBindValue(instrument.trackingMarkerId);
    query.addBindValue(instrument.geometryFilePath);
    query.addBindValue(instrument.isCalibrated ? 1 : 0);
    query.addBindValue(instrument.calibrationTime.toString(Qt::ISODate));
    query.addBindValue(instrument.tipOffsetX);
    query.addBindValue(instrument.tipOffsetY);
    query.addBindValue(instrument.tipOffsetZ);
    
    if (!executeQuery(query)) {
        return -1;
    }
    
    int newId = query.lastInsertId().toInt();
    logMessage("INFO", QString("添加器械成功: %1 (ID: %2)").arg(instrument.name).arg(newId));
    emit instrumentAdded(newId);
    
    return newId;
}

bool InstrumentManagementServiceImpl::updateInstrument(const InstrumentItem& instrument)
{
    QMutexLocker locker(&m_mutex);
    
    if (!checkDatabaseConnection() || instrument.id < 0) {
        return false;
    }
    
    QSqlQuery query(m_database);
    query.prepare(R"(
        UPDATE instruments SET
            name=?, model=?, serial=?, category=?, status=?, description=?, is_active=?,
            updated_at=?,
            model_file_path=?, thumbnail_path=?, display_color=?, model_scale=?,
            tracking_marker_id=?, geometry_file_path=?, is_calibrated=?, calibration_time=?,
            tip_offset_x=?, tip_offset_y=?, tip_offset_z=?
        WHERE id=?
    )");
    
    query.addBindValue(instrument.name);
    query.addBindValue(instrument.model);
    query.addBindValue(instrument.serial);
    query.addBindValue(instrument.category);
    query.addBindValue(instrument.status);
    query.addBindValue(instrument.description);
    query.addBindValue(instrument.isActive ? 1 : 0);
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    query.addBindValue(instrument.modelFilePath);
    query.addBindValue(instrument.thumbnailPath);
    query.addBindValue(instrument.displayColor);
    query.addBindValue(instrument.modelScale);
    query.addBindValue(instrument.trackingMarkerId);
    query.addBindValue(instrument.geometryFilePath);
    query.addBindValue(instrument.isCalibrated ? 1 : 0);
    query.addBindValue(instrument.calibrationTime.toString(Qt::ISODate));
    query.addBindValue(instrument.tipOffsetX);
    query.addBindValue(instrument.tipOffsetY);
    query.addBindValue(instrument.tipOffsetZ);
    query.addBindValue(instrument.id);
    
    if (!executeQuery(query)) {
        return false;
    }
    
    logMessage("INFO", QString("更新器械成功: ID=%1").arg(instrument.id));
    emit instrumentUpdated(instrument.id);
    
    return true;
}

bool InstrumentManagementServiceImpl::deleteInstrument(int instrumentId)
{
    QMutexLocker locker(&m_mutex);

    if (!checkDatabaseConnection() || instrumentId < 0) {
        return false;
    }

    qDebug() << "[InstrumentManagementService] ========== 删除器械（软删除）==========";
    qDebug() << "[InstrumentManagementService] 器械ID:" << instrumentId;

    // 先获取器械信息，以便删除缩略图和预览图
    QString thumbnailPath;
    QSqlQuery selectQuery(m_database);
    selectQuery.prepare("SELECT thumbnail_path FROM instruments WHERE id=?");
    selectQuery.addBindValue(instrumentId);

    if (selectQuery.exec() && selectQuery.next()) {
        thumbnailPath = selectQuery.value(0).toString();
    }

    // ✅ 删除缩略图文件（转换为绝对路径，使用项目根目录）
    if (!thumbnailPath.isEmpty()) {
        // 将相对路径转换为绝对路径（使用项目根目录）
        QString absoluteThumbnailPath;
        if (QDir::isRelativePath(thumbnailPath)) {
            absoluteThumbnailPath = QDir(m_projectPath).absoluteFilePath(thumbnailPath);
        } else {
            absoluteThumbnailPath = thumbnailPath;
        }

        QFile thumbnailFile(absoluteThumbnailPath);
        if (thumbnailFile.exists()) {
            if (thumbnailFile.remove()) {
                qDebug() << "[InstrumentManagementService] ✅ 删除缩略图成功:" << absoluteThumbnailPath;
                logMessage("INFO", QString("删除缩略图成功: %1").arg(absoluteThumbnailPath));
            } else {
                qWarning() << "[InstrumentManagementService] ❌ 删除缩略图失败:" << absoluteThumbnailPath;
                logMessage("WARNING", QString("删除缩略图失败: %1").arg(absoluteThumbnailPath));
            }
        } else {
            qDebug() << "[InstrumentManagementService] ⏭️ 缩略图文件不存在（跳过删除）:" << absoluteThumbnailPath;
        }
    } else {
        qDebug() << "[InstrumentManagementService] ⏭️ 数据库中没有缩略图路径记录";
    }

    // ✅ 删除预览图文件（根据ID构造文件名）
    QString previewFileName = QString("instrument_%1_preview.png").arg(instrumentId);
    QString previewPath = m_thumbnailsPath + "/" + previewFileName;

    QFile previewFile(previewPath);
    if (previewFile.exists()) {
        if (previewFile.remove()) {
            qDebug() << "[InstrumentManagementService] ✅ 删除预览图成功:" << previewPath;
            logMessage("INFO", QString("删除预览图成功: %1").arg(previewPath));
        } else {
            qWarning() << "[InstrumentManagementService] ❌ 删除预览图失败:" << previewPath;
            logMessage("WARNING", QString("删除预览图失败: %1").arg(previewPath));
        }
    } else {
        qDebug() << "[InstrumentManagementService] ⏭️ 预览图文件不存在（跳过删除）:" << previewPath;
    }

    // 软删除（设置is_active=0）
    QSqlQuery query(m_database);
    query.prepare("UPDATE instruments SET is_active=0, updated_at=? WHERE id=?");
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    query.addBindValue(instrumentId);

    if (!executeQuery(query)) {
        qCritical() << "[InstrumentManagementService] ❌ 软删除失败 ID:" << instrumentId;
        return false;
    }

    qDebug() << "[InstrumentManagementService] ✅ 软删除成功 ID:" << instrumentId;
    logMessage("INFO", QString("删除器械成功: ID=%1").arg(instrumentId));
    emit instrumentDeleted(instrumentId);

    return true;
}

bool InstrumentManagementServiceImpl::removeInstrumentPermanently(int instrumentId)
{
    QMutexLocker locker(&m_mutex);

    if (!checkDatabaseConnection() || instrumentId < 0) {
        return false;
    }

    qDebug() << "[InstrumentManagementService] ========== 永久删除器械 ==========";
    qDebug() << "[InstrumentManagementService] 器械ID:" << instrumentId;

    // 先获取器械信息，以便删除缩略图和预览图
    QString thumbnailPath;
    QSqlQuery selectQuery(m_database);
    selectQuery.prepare("SELECT thumbnail_path FROM instruments WHERE id=?");
    selectQuery.addBindValue(instrumentId);

    if (selectQuery.exec() && selectQuery.next()) {
        thumbnailPath = selectQuery.value(0).toString();
    }

    // ✅ 删除缩略图文件（转换为绝对路径，使用项目根目录）
    if (!thumbnailPath.isEmpty()) {
        // 将相对路径转换为绝对路径（使用项目根目录）
        QString absoluteThumbnailPath;
        if (QDir::isRelativePath(thumbnailPath)) {
            absoluteThumbnailPath = QDir(m_projectPath).absoluteFilePath(thumbnailPath);
        } else {
            absoluteThumbnailPath = thumbnailPath;
        }

        QFile thumbnailFile(absoluteThumbnailPath);
        if (thumbnailFile.exists()) {
            if (thumbnailFile.remove()) {
                qDebug() << "[InstrumentManagementService] ✅ 删除缩略图成功:" << absoluteThumbnailPath;
                logMessage("INFO", QString("删除缩略图成功: %1").arg(absoluteThumbnailPath));
            } else {
                qWarning() << "[InstrumentManagementService] ❌ 删除缩略图失败:" << absoluteThumbnailPath;
                logMessage("WARNING", QString("删除缩略图失败: %1").arg(absoluteThumbnailPath));
            }
        } else {
            qDebug() << "[InstrumentManagementService] ⏭️ 缩略图文件不存在（跳过删除）:" << absoluteThumbnailPath;
        }
    } else {
        qDebug() << "[InstrumentManagementService] ⏭️ 数据库中没有缩略图路径记录";
    }

    // ✅ 删除预览图文件（根据ID构造文件名）
    QString previewFileName = QString("instrument_%1_preview.png").arg(instrumentId);
    QString previewPath = m_thumbnailsPath + "/" + previewFileName;

    QFile previewFile(previewPath);
    if (previewFile.exists()) {
        if (previewFile.remove()) {
            qDebug() << "[InstrumentManagementService] ✅ 删除预览图成功:" << previewPath;
            logMessage("INFO", QString("删除预览图成功: %1").arg(previewPath));
        } else {
            qWarning() << "[InstrumentManagementService] ❌ 删除预览图失败:" << previewPath;
            logMessage("WARNING", QString("删除预览图失败: %1").arg(previewPath));
        }
    } else {
        qDebug() << "[InstrumentManagementService] ⏭️ 预览图文件不存在（跳过删除）:" << previewPath;
    }

    // 物理删除
    QSqlQuery query(m_database);
    query.prepare("DELETE FROM instruments WHERE id=?");
    query.addBindValue(instrumentId);

    if (!executeQuery(query)) {
        qCritical() << "[InstrumentManagementService] ❌ 物理删除失败 ID:" << instrumentId;
        return false;
    }

    qDebug() << "[InstrumentManagementService] ✅ 永久删除成功 ID:" << instrumentId;
    logMessage("INFO", QString("永久删除器械: ID=%1").arg(instrumentId));
    emit instrumentDeleted(instrumentId);

    return true;
}

InstrumentItem InstrumentManagementServiceImpl::getInstrument(int instrumentId)
{
    QMutexLocker locker(&m_mutex);
    
    InstrumentItem item;
    item.id = -1;
    
    if (!checkDatabaseConnection() || instrumentId < 0) {
        return item;
    }
    
    QSqlQuery query(m_database);
    query.prepare(R"(
        SELECT id, name, model, serial, category, status, description, is_active,
               created_at, updated_at,
               model_file_path, thumbnail_path, display_color, model_scale,
               tracking_marker_id, geometry_file_path, is_calibrated, calibration_time,
               tip_offset_x, tip_offset_y, tip_offset_z
        FROM instruments WHERE id=?
    )");
    query.addBindValue(instrumentId);
    
    if (!executeQuery(query)) {
        return item;
    }
    
    if (query.next()) {
        item.id = query.value(0).toInt();
        item.name = query.value(1).toString();
        item.model = query.value(2).toString();
        item.serial = query.value(3).toString();
        item.category = query.value(4).toString();
        item.status = query.value(5).toString();
        item.description = query.value(6).toString();
        item.isActive = query.value(7).toBool();
        item.createdAt = QDateTime::fromString(query.value(8).toString(), Qt::ISODate);
        item.updatedAt = QDateTime::fromString(query.value(9).toString(), Qt::ISODate);
        item.modelFilePath = query.value(10).toString();
        item.thumbnailPath = query.value(11).toString();
        item.displayColor = query.value(12).toString();
        item.modelScale = query.value(13).toDouble();
        item.trackingMarkerId = query.value(14).toString();
        item.geometryFilePath = query.value(15).toString();
        item.isCalibrated = query.value(16).toBool();
        item.calibrationTime = QDateTime::fromString(query.value(17).toString(), Qt::ISODate);
        item.tipOffsetX = query.value(18).toDouble();
        item.tipOffsetY = query.value(19).toDouble();
        item.tipOffsetZ = query.value(20).toDouble();
    }
    
    return item;
}

QList<InstrumentItem> InstrumentManagementServiceImpl::getAllInstruments(bool includeInactive)
{
    QMutexLocker locker(&m_mutex);
    
    QList<InstrumentItem> list;
    
    if (!checkDatabaseConnection()) {
        return list;
    }
    
    QSqlQuery query(m_database);
    QString sql = R"(
        SELECT id, name, model, serial, category, status, description, is_active,
               created_at, updated_at,
               model_file_path, thumbnail_path, display_color, model_scale,
               tracking_marker_id, geometry_file_path, is_calibrated, calibration_time,
               tip_offset_x, tip_offset_y, tip_offset_z
        FROM instruments
    )";
    
    if (!includeInactive) {
        sql += " WHERE is_active=1";
    }
    
    sql += " ORDER BY created_at DESC";
    
    if (!query.exec(sql)) {
        m_lastError = query.lastError().text();
        return list;
    }
    
    while (query.next()) {
        InstrumentItem item;
        item.id = query.value(0).toInt();
        item.name = query.value(1).toString();
        item.model = query.value(2).toString();
        item.serial = query.value(3).toString();
        item.category = query.value(4).toString();
        item.status = query.value(5).toString();
        item.description = query.value(6).toString();
        item.isActive = query.value(7).toBool();
        item.createdAt = QDateTime::fromString(query.value(8).toString(), Qt::ISODate);
        item.updatedAt = QDateTime::fromString(query.value(9).toString(), Qt::ISODate);
        item.modelFilePath = query.value(10).toString();
        item.thumbnailPath = query.value(11).toString();
        item.displayColor = query.value(12).toString();
        item.modelScale = query.value(13).toDouble();
        item.trackingMarkerId = query.value(14).toString();
        item.geometryFilePath = query.value(15).toString();
        item.isCalibrated = query.value(16).toBool();
        item.calibrationTime = QDateTime::fromString(query.value(17).toString(), Qt::ISODate);
        item.tipOffsetX = query.value(18).toDouble();
        item.tipOffsetY = query.value(19).toDouble();
        item.tipOffsetZ = query.value(20).toDouble();
        
        list.append(item);
    }
    
    return list;
}

QList<InstrumentItem> InstrumentManagementServiceImpl::getInstrumentsByCategory(const QString& category)
{
    QMutexLocker locker(&m_mutex);
    
    QList<InstrumentItem> list;
    
    if (!checkDatabaseConnection()) {
        return list;
    }
    
    QSqlQuery query(m_database);
    query.prepare("SELECT * FROM instruments WHERE category=? AND is_active=1 ORDER BY created_at DESC");
    query.addBindValue(category);
    
    if (!executeQuery(query)) {
        return list;
    }
    
    while (query.next()) {
        InstrumentItem item;
        // ... 填充数据（同上）
        list.append(item);
    }
    
    return list;
}

QList<InstrumentItem> InstrumentManagementServiceImpl::getInstrumentsByStatus(const QString& status)
{
    QMutexLocker locker(&m_mutex);
    
    QList<InstrumentItem> list;
    
    if (!checkDatabaseConnection()) {
        return list;
    }
    
    QSqlQuery query(m_database);
    query.prepare("SELECT * FROM instruments WHERE status=? AND is_active=1 ORDER BY created_at DESC");
    query.addBindValue(status);
    
    if (!executeQuery(query)) {
        return list;
    }
    
    // 填充列表（略）
    
    return list;
}

QList<InstrumentItem> InstrumentManagementServiceImpl::searchInstruments(const QString& keyword)
{
    QMutexLocker locker(&m_mutex);

    QList<InstrumentItem> list;

    if (!checkDatabaseConnection()) {
        return list;
    }

    QSqlQuery query(m_database);
    query.prepare(R"(
        SELECT id, name, model, serial, category, status, description, is_active,
               created_at, updated_at,
               model_file_path, thumbnail_path, display_color, model_scale,
               tracking_marker_id, geometry_file_path, is_calibrated, calibration_time,
               tip_offset_x, tip_offset_y, tip_offset_z
        FROM instruments
        WHERE (name LIKE ? OR model LIKE ? OR serial LIKE ? OR description LIKE ?)
        AND is_active=1
        ORDER BY created_at DESC
    )");
    QString pattern = "%" + keyword + "%";
    query.addBindValue(pattern);
    query.addBindValue(pattern);
    query.addBindValue(pattern);
    query.addBindValue(pattern);

    if (!executeQuery(query)) {
        return list;
    }

    // ✅ 填充结果列表
    while (query.next()) {
        InstrumentItem item;
        item.id = query.value("id").toInt();
        item.name = query.value("name").toString();
        item.model = query.value("model").toString();
        item.serial = query.value("serial").toString();
        item.category = query.value("category").toString();
        item.status = query.value("status").toString();
        item.description = query.value("description").toString();
        item.isActive = query.value("is_active").toBool();
        item.createdAt = QDateTime::fromString(query.value("created_at").toString(), Qt::ISODate);
        item.updatedAt = QDateTime::fromString(query.value("updated_at").toString(), Qt::ISODate);
        item.modelFilePath = query.value("model_file_path").toString();
        item.thumbnailPath = query.value("thumbnail_path").toString();
        item.displayColor = query.value("display_color").toString();
        item.modelScale = query.value("model_scale").toDouble();
        item.trackingMarkerId = query.value("tracking_marker_id").toString();
        item.geometryFilePath = query.value("geometry_file_path").toString();
        item.isCalibrated = query.value("is_calibrated").toBool();
        item.calibrationTime = QDateTime::fromString(query.value("calibration_time").toString(), Qt::ISODate);
        item.tipOffsetX = query.value("tip_offset_x").toDouble();
        item.tipOffsetY = query.value("tip_offset_y").toDouble();
        item.tipOffsetZ = query.value("tip_offset_z").toDouble();

        list.append(item);
    }

    qDebug() << "[InstrumentManagementService] 搜索关键词:" << keyword << "找到" << list.size() << "个结果";

    return list;
}

// ========== 3D模型管理 ==========

bool InstrumentManagementServiceImpl::setInstrumentModel(int instrumentId, const QString& modelFilePath)
{
    InstrumentItem item = getInstrument(instrumentId);
    if (!item.isValid()) {
        return false;
    }
    
    item.modelFilePath = modelFilePath;
    return updateInstrument(item);
}

QString InstrumentManagementServiceImpl::generateInstrumentThumbnail(int instrumentId, int thumbnailSize)
{
    InstrumentItem item = getInstrument(instrumentId);
    if (!item.isValid() || item.modelFilePath.isEmpty()) {
        m_lastError = "器械模型文件路径为空";
        return QString();
    }

    // modelFilePath已经是绝对路径，直接使用
    QString fullModelPath = item.modelFilePath;
    QString thumbnailFileName = QString("instrument_%1.png").arg(instrumentId);
    QString thumbnailPath = m_thumbnailsPath + "/" + thumbnailFileName;

    qDebug() << "[InstrumentManagementService] 生成缩略图 - 模型路径:" << fullModelPath;
    qDebug() << "[InstrumentManagementService] 生成缩略图 - 输出路径:" << thumbnailPath;

    // 使用VTK生成缩略图
    if (generateThumbnailVTK(fullModelPath, thumbnailPath, thumbnailSize)) {
        // 更新数据库中的缩略图路径（相对路径）
        item.thumbnailPath = "data/instrumentThumbnails/" + thumbnailFileName;
        updateInstrument(item);
        return item.thumbnailPath;
    }

    return QString();
}

QString InstrumentManagementServiceImpl::generateInstrumentPreview(int instrumentId, int previewSize)
{
    InstrumentItem item = getInstrument(instrumentId);
    if (!item.isValid() || item.modelFilePath.isEmpty()) {
        m_lastError = "器械模型文件路径为空";
        return QString();
    }

    // modelFilePath已经是绝对路径，直接使用
    QString fullModelPath = item.modelFilePath;
    QString previewFileName = QString("instrument_%1_preview.png").arg(instrumentId);
    QString previewPath = m_thumbnailsPath + "/" + previewFileName;

    qDebug() << "[InstrumentManagementService] 生成高质量预览图 - 模型路径:" << fullModelPath;
    qDebug() << "[InstrumentManagementService] 生成高质量预览图 - 输出路径:" << previewPath;
    qDebug() << "[InstrumentManagementService] 预览图尺寸:" << previewSize << "x" << previewSize;

    // 使用VTK生成高质量预览图（带金属材质）
    if (generatePreviewVTK(fullModelPath, previewPath, previewSize)) {
        logMessage("INFO", QString("生成高质量预览图成功: %1").arg(previewPath));
        return previewPath;  // 返回绝对路径，用于直接显示
    }

    return QString();
}

bool InstrumentManagementServiceImpl::generatePreviewFromModel(const QString& modelFilePath, const QString& outputPath, int previewSize)
{
    logMessage("INFO", QString("从模型生成预览图 - 模型: %1, 输出: %2, 尺寸: %3x%3")
        .arg(modelFilePath).arg(outputPath).arg(previewSize));

    // 检查模型文件是否存在
    if (!QFile::exists(modelFilePath)) {
        m_lastError = QString("模型文件不存在: %1").arg(modelFilePath);
        logMessage("ERROR", m_lastError);
        return false;
    }

    // 确保输出目录存在
    QFileInfo outputInfo(outputPath);
    QDir outputDir = outputInfo.absoluteDir();
    if (!outputDir.exists()) {
        outputDir.mkpath(".");
    }

    // 使用VTK生成高质量预览图
    bool success = generatePreviewVTK(modelFilePath, outputPath, previewSize);

    if (success) {
        logMessage("INFO", QString("预览图生成成功: %1").arg(outputPath));
    } else {
        logMessage("ERROR", "预览图生成失败");
    }

    return success;
}

bool InstrumentManagementServiceImpl::generateThumbnailAsync(const QString& modelFilePath, const QString& outputPath, int size)
{
    qDebug() << "[InstrumentManagementService] 异步生成缩略图 - 模型:" << modelFilePath << "输出:" << outputPath << "尺寸:" << size;

    // 检查模型文件是否存在
    if (!QFile::exists(modelFilePath)) {
        qWarning() << "[InstrumentManagementService] 模型文件不存在:" << modelFilePath;
        return false;
    }

    // 确保输出目录存在
    QFileInfo outputInfo(outputPath);
    QDir outputDir = outputInfo.absoluteDir();
    if (!outputDir.exists()) {
        outputDir.mkpath(".");
    }

    // 调用VTK渲染（此方法将在后台线程执行）
    bool success = generateThumbnailVTK(modelFilePath, outputPath, size);

    if (success) {
        qDebug() << "[InstrumentManagementService] 异步缩略图生成成功:" << outputPath;
    } else {
        qWarning() << "[InstrumentManagementService] 异步缩略图生成失败";
    }

    return success;
}

bool InstrumentManagementServiceImpl::generatePreviewAsync(const QString& modelFilePath, const QString& outputPath, int size)
{
    qDebug() << "[InstrumentManagementService] 异步生成预览图 - 模型:" << modelFilePath << "输出:" << outputPath << "尺寸:" << size;

    // 检查模型文件是否存在
    if (!QFile::exists(modelFilePath)) {
        qWarning() << "[InstrumentManagementService] 模型文件不存在:" << modelFilePath;
        return false;
    }

    // 确保输出目录存在
    QFileInfo outputInfo(outputPath);
    QDir outputDir = outputInfo.absoluteDir();
    if (!outputDir.exists()) {
        outputDir.mkpath(".");
    }

    // 调用VTK渲染（此方法将在后台线程执行）
    bool success = generatePreviewVTK(modelFilePath, outputPath, size);

    if (success) {
        qDebug() << "[InstrumentManagementService] 异步预览图生成成功:" << outputPath;
    } else {
        qWarning() << "[InstrumentManagementService] 异步预览图生成失败";
    }

    return success;
}

bool InstrumentManagementServiceImpl::generateThumbnailVTK(const QString& modelPath, const QString& outputPath, int size)
{
#ifdef VTK_FOUND
    try {
        // 1. 加载STL模型
        vtkSmartPointer<vtkSTLReader> reader = vtkSmartPointer<vtkSTLReader>::New();
        reader->SetFileName(modelPath.toLocal8Bit().data());
        reader->Update();

        // 2. 简化的几何数据处理（缩略图不需要太高质量）
        vtkSmartPointer<vtkPolyDataNormals> normalFilter = vtkSmartPointer<vtkPolyDataNormals>::New();
        normalFilter->SetInputConnection(reader->GetOutputPort());
        normalFilter->ComputePointNormalsOn();
        normalFilter->ComputeCellNormalsOff();  // 缩略图不需要单元法线
        normalFilter->SplittingOff();           // 不分裂，加快速度
        normalFilter->Update();

        // 3. 创建Mapper和Actor
        vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputConnection(normalFilter->GetOutputPort());

        vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
        actor->SetMapper(mapper);
        actor->GetProperty()->SetColor(0.75, 0.75, 0.78); // 银灰色
        actor->GetProperty()->SetSpecular(0.5);
        actor->GetProperty()->SetSpecularPower(30);
        actor->GetProperty()->SetOpacity(1.0);

        // 4. 创建离屏渲染窗口
        vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
        renderer->AddActor(actor);
        renderer->SetBackground(0.118, 0.161, 0.231); // #1e293b 深蓝色背景
        renderer->SetBackgroundAlpha(1.0);
        renderer->ResetCamera();

        vtkSmartPointer<vtkRenderWindow> renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
        renderWindow->AddRenderer(renderer);
        renderWindow->SetOffScreenRendering(1);
        renderWindow->SetSize(size, size);
        renderWindow->SetAlphaBitPlanes(0);  // 禁用Alpha通道
        renderWindow->SetMultiSamples(0);    // 缩略图不需要抗锯齿，加快速度

        // 5. 渲染
        renderWindow->Render();

        // 6. 保存为PNG
        vtkSmartPointer<vtkWindowToImageFilter> windowToImageFilter = vtkSmartPointer<vtkWindowToImageFilter>::New();
        windowToImageFilter->SetInput(renderWindow);
        windowToImageFilter->SetInputBufferTypeToRGB();  // 只需要RGB，不需要Alpha
        windowToImageFilter->Update();

        vtkSmartPointer<vtkPNGWriter> writer = vtkSmartPointer<vtkPNGWriter>::New();
        writer->SetFileName(outputPath.toLocal8Bit().data());
        writer->SetInputConnection(windowToImageFilter->GetOutputPort());
        writer->Write();

        logMessage("INFO", QString("生成缩略图成功: %1").arg(outputPath));
        return true;

    } catch (...) {
        m_lastError = "VTK渲染异常";
        logMessage("ERROR", m_lastError);
        return false;
    }
#else
    m_lastError = "VTK未启用";
    return false;
#endif
}

bool InstrumentManagementServiceImpl::generatePreviewVTK(const QString& modelPath, const QString& outputPath, int size)
{
#ifdef VTK_FOUND
    try {
        // 1. 加载STL模型
        vtkSmartPointer<vtkSTLReader> reader = vtkSmartPointer<vtkSTLReader>::New();
        reader->SetFileName(modelPath.toLocal8Bit().data());
        reader->Update();

        // 2. 几何数据清理和法线计算
        vtkSmartPointer<vtkCleanPolyData> cleaner = vtkSmartPointer<vtkCleanPolyData>::New();
        cleaner->SetInputConnection(reader->GetOutputPort());

        vtkSmartPointer<vtkTriangleFilter> triangleFilter = vtkSmartPointer<vtkTriangleFilter>::New();
        triangleFilter->SetInputConnection(cleaner->GetOutputPort());

        vtkSmartPointer<vtkPolyDataNormals> normalFilter = vtkSmartPointer<vtkPolyDataNormals>::New();
        normalFilter->SetInputConnection(triangleFilter->GetOutputPort());
        normalFilter->ComputePointNormalsOn();
        normalFilter->ComputeCellNormalsOn();
        normalFilter->SplittingOff();  // 不分裂边缘，保持平滑
        normalFilter->Update();

        // 3. 创建Mapper和Actor
        vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputConnection(normalFilter->GetOutputPort());

        vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
        actor->SetMapper(mapper);

        // 4. 设置金属材质（钢铁效果）
        vtkProperty* property = actor->GetProperty();
        property->SetColor(0.75, 0.75, 0.78);      // 银灰色
        property->SetMetallic(0.9);                 // 高金属度
        property->SetRoughness(0.2);                // 低粗糙度（光滑）
        property->SetSpecular(0.8);                 // 高镜面反射
        property->SetSpecularPower(100);            // 高镜面强度
        property->SetAmbient(0.15);                 // 环境光
        property->SetDiffuse(0.7);                  // 漫反射

        // 5. 创建渲染器并设置高级光照
        vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
        renderer->AddActor(actor);
        renderer->SetBackground(0.1, 0.1, 0.15);    // 深色背景

        // 添加多个光源以增强金属效果
        // 主光源（从右上方）
        vtkSmartPointer<vtkLight> mainLight = vtkSmartPointer<vtkLight>::New();
        mainLight->SetPosition(1.0, 1.0, 1.0);
        mainLight->SetFocalPoint(0.0, 0.0, 0.0);
        mainLight->SetColor(1.0, 1.0, 1.0);
        mainLight->SetIntensity(1.2);
        renderer->AddLight(mainLight);

        // 补光（从左侧）
        vtkSmartPointer<vtkLight> fillLight = vtkSmartPointer<vtkLight>::New();
        fillLight->SetPosition(-1.0, 0.5, 0.5);
        fillLight->SetFocalPoint(0.0, 0.0, 0.0);
        fillLight->SetColor(0.8, 0.8, 1.0);
        fillLight->SetIntensity(0.5);
        renderer->AddLight(fillLight);

        // 背光（从后方）
        vtkSmartPointer<vtkLight> backLight = vtkSmartPointer<vtkLight>::New();
        backLight->SetPosition(0.0, -1.0, -0.5);
        backLight->SetFocalPoint(0.0, 0.0, 0.0);
        backLight->SetColor(0.6, 0.6, 0.8);
        backLight->SetIntensity(0.3);
        renderer->AddLight(backLight);

        renderer->ResetCamera();

        // 6. 创建高分辨率离屏渲染窗口
        vtkSmartPointer<vtkRenderWindow> renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
        renderWindow->AddRenderer(renderer);
        renderWindow->SetOffScreenRendering(1);
        renderWindow->SetSize(size, size);
        renderWindow->SetMultiSamples(8);  // 8x抗锯齿

        // 7. 渲染
        renderWindow->Render();

        // 8. 保存为高质量PNG
        vtkSmartPointer<vtkWindowToImageFilter> windowToImageFilter = vtkSmartPointer<vtkWindowToImageFilter>::New();
        windowToImageFilter->SetInput(renderWindow);
        windowToImageFilter->SetScale(1);  // 1:1比例
        windowToImageFilter->SetInputBufferTypeToRGBA();  // 包含Alpha通道
        windowToImageFilter->ReadFrontBufferOff();
        windowToImageFilter->Update();

        vtkSmartPointer<vtkPNGWriter> writer = vtkSmartPointer<vtkPNGWriter>::New();
        writer->SetFileName(outputPath.toLocal8Bit().data());
        writer->SetInputConnection(windowToImageFilter->GetOutputPort());
        writer->Write();

        logMessage("INFO", QString("生成高质量预览图成功: %1 (%2x%2)").arg(outputPath).arg(size));
        return true;

    } catch (...) {
        m_lastError = "VTK渲染异常";
        logMessage("ERROR", m_lastError);
        return false;
    }
#else
    m_lastError = "VTK未启用";
    return false;
#endif
}

QStringList InstrumentManagementServiceImpl::getAvailableModelFiles()
{
    qDebug() << "[InstrumentManagementService] 查找模型文件，路径:" << m_modelsPath;
    
    QDir modelsDir(m_modelsPath);
    if (!modelsDir.exists()) {
        qWarning() << "[InstrumentManagementService] 模型目录不存在:" << m_modelsPath;
        return QStringList();
    }
    
    QStringList filters;
    filters << "*.stl" << "*.STL" << "*.obj" << "*.OBJ";
    
    QStringList fileList = modelsDir.entryList(filters, QDir::Files);
    qDebug() << "[InstrumentManagementService] 找到" << fileList.size() << "个模型文件";
    
    // 返回绝对路径
    QStringList absolutePaths;
    for (const QString& file : fileList) {
        absolutePaths.append(modelsDir.absoluteFilePath(file));
    }
    
    if (!absolutePaths.isEmpty()) {
        qDebug() << "[InstrumentManagementService] 示例模型文件:" << absolutePaths.first();
    }
    
    return absolutePaths;
}

int InstrumentManagementServiceImpl::importInstrumentsFromModels()
{
    qDebug() << "[InstrumentManagementService] ========== 开始从模型文件导入器械 ==========";

    QStringList modelFiles = getAvailableModelFiles();
    qDebug() << "[InstrumentManagementService] 获取到" << modelFiles.size() << "个模型文件";

    // 获取所有现有器械（用于去重检查）
    QList<InstrumentItem> existingInstruments = getAllInstruments(false);
    QSet<QString> existingModelPaths;
    QMap<QString, int> existingModelIdMap;  // 模型路径 -> ID 映射

    for (const InstrumentItem& item : existingInstruments) {
        existingModelPaths.insert(item.modelFilePath);
        existingModelIdMap[item.modelFilePath] = item.id;
    }

    qDebug() << "[InstrumentManagementService] 数据库中已有" << existingInstruments.size() << "个器械";

    // 输出现有器械的ID范围
    if (!existingInstruments.isEmpty()) {
        int minId = INT_MAX;
        int maxId = INT_MIN;
        for (const InstrumentItem& item : existingInstruments) {
            if (item.id < minId) minId = item.id;
            if (item.id > maxId) maxId = item.id;
        }
        qDebug() << "[InstrumentManagementService] 现有器械ID范围:" << minId << "~" << maxId;
    }

    int importedCount = 0;
    int skippedCount = 0;
    QStringList importedList;
    QStringList skippedList;

    for (const QString& modelFile : modelFiles) {
        // 从文件名提取器械名称
        QFileInfo fileInfo(modelFile);
        QString baseName = fileInfo.baseName();

        // ✅ 使用模型文件路径去重（更准确）
        if (existingModelPaths.contains(modelFile)) {
            int existingId = existingModelIdMap[modelFile];
            qDebug() << "[InstrumentManagementService] ⏭️ 器械已存在，跳过:" << baseName
                     << "(ID:" << existingId << ")";
            skippedCount++;
            skippedList.append(QString("%1(ID:%2)").arg(baseName).arg(existingId));
            continue;
        }

        // 创建新器械
        InstrumentItem newItem;
        newItem.name = baseName;
        newItem.model = baseName;
        newItem.category = "未分类";
        newItem.status = "在库";
        newItem.modelFilePath = modelFile;
        newItem.description = QString("从模型文件导入: %1").arg(fileInfo.fileName());

        int newId = addInstrument(newItem);
        if (newId > 0) {
            importedCount++;
            importedList.append(QString("%1(ID:%2)").arg(baseName).arg(newId));
            qDebug() << "[InstrumentManagementService] ✅ 成功导入器械:" << baseName
                     << "-> 分配ID:" << newId;
            logMessage("INFO", QString("导入器械: %1 (ID: %2)").arg(baseName).arg(newId));
        } else {
            qWarning() << "[InstrumentManagementService] ❌ 添加器械失败:" << baseName;
        }
    }

    qDebug() << "[InstrumentManagementService] ========== 导入完成 ==========";
    qDebug() << "[InstrumentManagementService] 新增:" << importedCount << "个，跳过:" << skippedCount << "个";

    if (!importedList.isEmpty()) {
        qDebug() << "[InstrumentManagementService] 新增列表:" << importedList.join(", ");
    }
    if (!skippedList.isEmpty()) {
        qDebug() << "[InstrumentManagementService] 跳过列表:" << skippedList.join(", ");
    }

    logMessage("INFO", QString("从模型文件导入完成，共导入 %1 个器械，跳过 %2 个").arg(importedCount).arg(skippedCount));
    return importedCount;
}

int InstrumentManagementServiceImpl::cleanupDuplicateInstruments()
{
    QMutexLocker locker(&m_mutex);

    qDebug() << "[InstrumentManagementService] ========== 开始清理重复器械 ==========";

    if (!checkDatabaseConnection()) {
        return 0;
    }

    // 1. 查找所有器械，按模型路径分组，并按ID排序
    QSqlQuery query(m_database);
    query.prepare(R"(
        SELECT model_file_path, COUNT(*) as count, GROUP_CONCAT(id ORDER BY id ASC) as ids
        FROM instruments
        WHERE is_active = 1 AND model_file_path IS NOT NULL AND model_file_path != ''
        GROUP BY model_file_path
        HAVING count > 1
    )");

    if (!executeQuery(query)) {
        return 0;
    }

    int deletedCount = 0;
    QStringList deletedIdsList;
    QStringList keptIdsList;

    // 2. 对于每个重复的模型路径，保留ID最小的记录，删除其他的
    while (query.next()) {
        QString modelPath = query.value("model_file_path").toString();
        QString idsStr = query.value("ids").toString();
        QStringList ids = idsStr.split(',');

        // 提取模型文件名（用于日志）
        QFileInfo fileInfo(modelPath);
        QString modelName = fileInfo.baseName();

        qDebug() << "[InstrumentManagementService] 发现重复模型:" << modelName
                 << "路径:" << modelPath
                 << "共" << ids.size() << "条记录";
        qDebug() << "[InstrumentManagementService] 所有ID（已排序）:" << ids.join(", ");

        // ✅ 保留第一个ID（最小的ID），删除其他的
        int keptId = ids[0].toInt();
        keptIdsList.append(QString::number(keptId));
        qDebug() << "[InstrumentManagementService] ✅ 保留ID:" << keptId << "（最小ID）";

        for (int i = 1; i < ids.size(); ++i) {
            int duplicateId = ids[i].toInt();
            deletedIdsList.append(QString::number(duplicateId));

            QSqlQuery deleteQuery(m_database);
            deleteQuery.prepare("DELETE FROM instruments WHERE id = ?");
            deleteQuery.addBindValue(duplicateId);

            if (executeQuery(deleteQuery)) {
                deletedCount++;
                qDebug() << "[InstrumentManagementService] ❌ 删除重复记录 ID:" << duplicateId;
            }
        }
    }

    qDebug() << "[InstrumentManagementService] ========== 清理完成 ==========";
    qDebug() << "[InstrumentManagementService] 共删除" << deletedCount << "条重复记录";
    qDebug() << "[InstrumentManagementService] 保留的ID列表:" << keptIdsList.join(", ");
    qDebug() << "[InstrumentManagementService] 删除的ID列表:" << deletedIdsList.join(", ");

    logMessage("INFO", QString("清理重复器械完成，删除 %1 条记录").arg(deletedCount));

    return deletedCount;
}

bool InstrumentManagementServiceImpl::resetAutoIncrement()
{
    QMutexLocker locker(&m_mutex);

    qDebug() << "[InstrumentManagementService] ========== 重置自增ID ==========";

    if (!checkDatabaseConnection()) {
        qCritical() << "[InstrumentManagementService] ❌ 数据库未连接";
        return false;
    }

    // ✅ 步骤1：检查活跃记录数（is_active=1）
    QSqlQuery checkActiveQuery(m_database);
    checkActiveQuery.prepare("SELECT COUNT(*) FROM instruments WHERE is_active=1");

    if (!executeQuery(checkActiveQuery)) {
        qCritical() << "[InstrumentManagementService] ❌ 检查活跃记录失败";
        return false;
    }

    int activeCount = 0;
    if (checkActiveQuery.next()) {
        activeCount = checkActiveQuery.value(0).toInt();
    }

    qDebug() << "[InstrumentManagementService] 当前活跃记录数 (is_active=1):" << activeCount;

    // ✅ 步骤2：检查所有记录数（包括软删除的）
    QSqlQuery checkAllQuery(m_database);
    checkAllQuery.prepare("SELECT COUNT(*) FROM instruments");

    if (!executeQuery(checkAllQuery)) {
        qCritical() << "[InstrumentManagementService] ❌ 检查所有记录失败";
        return false;
    }

    int totalCount = 0;
    if (checkAllQuery.next()) {
        totalCount = checkAllQuery.value(0).toInt();
    }

    int inactiveCount = totalCount - activeCount;
    qDebug() << "[InstrumentManagementService] 总记录数:" << totalCount;
    qDebug() << "[InstrumentManagementService] 软删除记录数 (is_active=0):" << inactiveCount;

    // ✅ 步骤3：如果有软删除的历史记录，先清理它们
    if (inactiveCount > 0) {
        qDebug() << "[InstrumentManagementService] 发现" << inactiveCount << "条软删除的历史记录，正在清理...";

        QSqlQuery cleanupQuery(m_database);
        cleanupQuery.prepare("DELETE FROM instruments WHERE is_active=0");

        if (!executeQuery(cleanupQuery)) {
            qCritical() << "[InstrumentManagementService] ❌ 清理软删除记录失败";
            return false;
        }

        int deletedRows = cleanupQuery.numRowsAffected();
        qDebug() << "[InstrumentManagementService] ✅ 成功清理" << deletedRows << "条软删除记录";
        logMessage("INFO", QString("清理软删除记录: %1 条").arg(deletedRows));
    }

    // ✅ 步骤4：再次检查是否还有活跃记录
    if (activeCount > 0) {
        qWarning() << "[InstrumentManagementService] ⚠️ 表中仍有" << activeCount << "条活跃数据，建议先删除所有数据再重置自增ID";
        // 不阻止重置，但给出警告
    }

    // ✅ 步骤5：检查 sqlite_sequence 表是否存在
    QSqlQuery checkSeqQuery(m_database);
    checkSeqQuery.prepare("SELECT name FROM sqlite_master WHERE type='table' AND name='sqlite_sequence'");

    if (!executeQuery(checkSeqQuery)) {
        qCritical() << "[InstrumentManagementService] ❌ 检查sqlite_sequence表失败";
        return false;
    }

    bool seqTableExists = checkSeqQuery.next();
    qDebug() << "[InstrumentManagementService] sqlite_sequence表存在:" << (seqTableExists ? "是" : "否");

    if (!seqTableExists) {
        qWarning() << "[InstrumentManagementService] ⚠️ sqlite_sequence表不存在，可能是因为从未插入过数据";
        qDebug() << "[InstrumentManagementService] ✅ 无需重置，下次插入将自动从1开始";
        return true;
    }

    // ✅ 步骤6：查看当前的自增ID值
    QSqlQuery viewSeqQuery(m_database);
    viewSeqQuery.prepare("SELECT seq FROM sqlite_sequence WHERE name='instruments'");

    if (executeQuery(viewSeqQuery) && viewSeqQuery.next()) {
        int currentSeq = viewSeqQuery.value(0).toInt();
        qDebug() << "[InstrumentManagementService] 当前自增ID值:" << currentSeq;
    } else {
        qDebug() << "[InstrumentManagementService] sqlite_sequence中没有instruments记录";
    }

    // ✅ 步骤7：删除sqlite_sequence表中的记录
    QSqlQuery resetQuery(m_database);
    resetQuery.prepare("DELETE FROM sqlite_sequence WHERE name='instruments'");

    if (!executeQuery(resetQuery)) {
        qCritical() << "[InstrumentManagementService] ❌ 删除sqlite_sequence记录失败";
        qCritical() << "[InstrumentManagementService] SQL错误:" << resetQuery.lastError().text();
        return false;
    }

    int affectedRows = resetQuery.numRowsAffected();
    qDebug() << "[InstrumentManagementService] 删除sqlite_sequence记录数:" << affectedRows;

    // ✅ 步骤8：验证删除是否成功
    QSqlQuery verifyQuery(m_database);
    verifyQuery.prepare("SELECT seq FROM sqlite_sequence WHERE name='instruments'");

    if (executeQuery(verifyQuery) && verifyQuery.next()) {
        qWarning() << "[InstrumentManagementService] ⚠️ 删除后仍然存在记录，seq值:" << verifyQuery.value(0).toInt();
        return false;
    } else {
        qDebug() << "[InstrumentManagementService] ✅ 验证成功：sqlite_sequence中已无instruments记录";
    }

    qDebug() << "[InstrumentManagementService] ✅ 自增ID已重置，下次插入将从1开始";
    logMessage("INFO", "数据库自增ID已重置");

    return true;
}

// ========== 光学追踪配置 ==========

bool InstrumentManagementServiceImpl::setInstrumentTracking(int instrumentId, const QString& markerId, const QString& geometryFilePath)
{
    InstrumentItem item = getInstrument(instrumentId);
    if (!item.isValid()) {
        return false;
    }
    
    item.trackingMarkerId = markerId;
    item.geometryFilePath = geometryFilePath;
    
    return updateInstrument(item);
}

MarkerGeometry InstrumentManagementServiceImpl::loadMarkerGeometry(const QString& geometryFilePath)
{
    QString fullPath = m_projectPath + "/" + geometryFilePath;
    return parseGeometryIni(fullPath);
}

MarkerGeometry InstrumentManagementServiceImpl::parseGeometryIni(const QString& filePath) const
{
    MarkerGeometry geometry;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        logMessage("ERROR", QString("无法打开几何文件: %1").arg(filePath));
        return geometry;
    }
    
    QTextStream in(&file);
    QString section;
    QMap<QString, QString> currentSection;
    
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        
        // 跳过空行和注释
        if (line.isEmpty() || line.startsWith("#") || line.startsWith(";")) {
            continue;
        }
        
        // 解析节
        if (line.startsWith("[") && line.endsWith("]")) {
            // 处理上一个节
            if (section == "geometry") {
                geometry.geometryId = currentSection.value("id").toInt();
                geometry.fiducialCount = currentSection.value("count").toInt();
            } else if (section.startsWith("fiducial")) {
                double x = currentSection.value("x").toDouble();
                double y = currentSection.value("y").toDouble();
                double z = currentSection.value("z").toDouble();
                geometry.fiducialPositions.append(QVector3D(x, y, z));
            }
            
            section = line.mid(1, line.length() - 2);
            currentSection.clear();
        } else {
            // 解析键值对
            QStringList parts = line.split('=');
            if (parts.size() == 2) {
                currentSection[parts[0].trimmed()] = parts[1].trimmed();
            }
        }
    }
    
    // 处理最后一个节
    if (section.startsWith("fiducial")) {
        double x = currentSection.value("x").toDouble();
        double y = currentSection.value("y").toDouble();
        double z = currentSection.value("z").toDouble();
        geometry.fiducialPositions.append(QVector3D(x, y, z));
    }
    
    file.close();
    
    geometry.filePath = filePath;
    logMessage("INFO", QString("加载几何文件成功: ID=%1, 标记点数=%2")
        .arg(geometry.geometryId).arg(geometry.fiducialCount));
    
    return geometry;
}

QStringList InstrumentManagementServiceImpl::getAvailableGeometryFiles()
{
    QDir geometryDir(m_geometryPath);
    QStringList filters;
    filters << "*.ini" << "*.INI";
    
    QStringList fileList = geometryDir.entryList(filters, QDir::Files);
    
    // 返回相对路径
    QStringList relativePaths;
    for (const QString& file : fileList) {
        relativePaths.append("geometry/" + file);
    }
    
    return relativePaths;
}

// ========== 其他方法实现（简化版） ==========

// 校准相关
bool InstrumentManagementServiceImpl::saveCalibrationData(const InstrumentCalibrationData& calibrationData)
{
    // TODO: 实现校准数据保存
    return true;
}

QList<InstrumentCalibrationData> InstrumentManagementServiceImpl::getCalibrationHistory(int instrumentId)
{
    return QList<InstrumentCalibrationData>();
}

InstrumentCalibrationData InstrumentManagementServiceImpl::getLatestCalibration(int instrumentId)
{
    return InstrumentCalibrationData();
}

bool InstrumentManagementServiceImpl::applyCalibrationToInstrument(int instrumentId, const InstrumentCalibrationData& calibrationData)
{
    InstrumentItem item = getInstrument(instrumentId);
    if (!item.isValid()) {
        return false;
    }
    
    item.isCalibrated = true;
    item.calibrationTime = QDateTime::currentDateTime();
    item.tipOffsetX = calibrationData.tipOffset.x();
    item.tipOffsetY = calibrationData.tipOffset.y();
    item.tipOffsetZ = calibrationData.tipOffset.z();
    
    bool success = updateInstrument(item);
    if (success) {
        emit instrumentCalibrated(instrumentId, calibrationData.rmse);
    }
    
    return success;
}

// 使用记录
int InstrumentManagementServiceImpl::recordInstrumentUsage(const InstrumentUsageRecord& record)
{
    // TODO: 实现
    return -1;
}

QList<InstrumentUsageRecord> InstrumentManagementServiceImpl::getUsageHistory(int instrumentId)
{
    return QList<InstrumentUsageRecord>();
}

// 维护记录
int InstrumentManagementServiceImpl::addMaintenanceRecord(const InstrumentMaintenanceRecord& record)
{
    // TODO: 实现
    return -1;
}

QList<InstrumentMaintenanceRecord> InstrumentManagementServiceImpl::getMaintenanceHistory(int instrumentId)
{
    return QList<InstrumentMaintenanceRecord>();
}

// 手术关联
bool InstrumentManagementServiceImpl::linkInstrumentToSurgery(int instrumentId, int surgeryId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!checkDatabaseConnection()) {
        return false;
    }
    
    QSqlQuery query(m_database);
    query.prepare("INSERT OR IGNORE INTO instrument_surgery_link (instrument_id, surgery_id, created_at) VALUES (?, ?, ?)");
    query.addBindValue(instrumentId);
    query.addBindValue(surgeryId);
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    
    return executeQuery(query);
}

bool InstrumentManagementServiceImpl::unlinkInstrumentFromSurgery(int instrumentId, int surgeryId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!checkDatabaseConnection()) {
        return false;
    }
    
    QSqlQuery query(m_database);
    query.prepare("DELETE FROM instrument_surgery_link WHERE instrument_id=? AND surgery_id=?");
    query.addBindValue(instrumentId);
    query.addBindValue(surgeryId);
    
    return executeQuery(query);
}

QList<InstrumentItem> InstrumentManagementServiceImpl::getInstrumentsBySurgery(int surgeryId)
{
    // TODO: 实现
    return QList<InstrumentItem>();
}

QList<int> InstrumentManagementServiceImpl::getSurgeriesByInstrument(int instrumentId)
{
    // TODO: 实现
    return QList<int>();
}

// 统计信息
InstrumentStatistics InstrumentManagementServiceImpl::getStatistics()
{
    InstrumentStatistics stats;
    
    QList<InstrumentItem> all = getAllInstruments(true);
    stats.totalInstruments = all.size();
    
    for (const InstrumentItem& item : all) {
        if (item.isActive) {
            stats.activeInstruments++;
            if (item.status == "在库") {
                stats.availableInstruments++;
            } else if (item.status == "维护") {
                stats.maintenanceInstruments++;
            }
        } else {
            stats.retiredInstruments++;
        }
        
        if (item.isCalibrated) {
            stats.calibratedInstruments++;
        }
    }
    
    return stats;
}

QList<InstrumentItem> InstrumentManagementServiceImpl::getUncalibratedInstruments()
{
    QMutexLocker locker(&m_mutex);
    
    QList<InstrumentItem> list;
    
    if (!checkDatabaseConnection()) {
        return list;
    }
    
    QSqlQuery query(m_database);
    query.prepare("SELECT * FROM instruments WHERE is_calibrated=0 AND is_active=1");
    
    if (!executeQuery(query)) {
        return list;
    }
    
    // 填充列表（略）
    
    return list;
}

QList<InstrumentItem> InstrumentManagementServiceImpl::getInstrumentsNeedingMaintenance()
{
    // 返回状态为"维护"的器械
    return getInstrumentsByStatus("维护");
}

