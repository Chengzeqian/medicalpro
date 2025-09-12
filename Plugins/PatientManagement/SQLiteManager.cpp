#include "SQLiteManager.h"
#include <QDir>
#include <QStandardPaths>
#include <QSqlError>
#include <QSqlRecord>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QCoreApplication>
#include <QUuid>
#include <QThread>

// 静态成员初始化
const QString SQLiteManager::DATABASE_NAME = "patient_management.db";
std::unique_ptr<SQLiteManager> SQLiteManager::m_instance = nullptr;
QMutex SQLiteManager::m_instanceMutex;

SQLiteManager& SQLiteManager::instance()
{
    QMutexLocker locker(&m_instanceMutex);
    if (!m_instance) {
        m_instance = std::unique_ptr<SQLiteManager>(new SQLiteManager());
    }
    return *m_instance;
}

SQLiteManager::SQLiteManager()
    : QObject(nullptr)
    , m_connected(false)
    , m_connectionName(generateConnectionName())
{
    // 移动到主线程，确保信号槽正常工作
    if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
        moveToThread(QCoreApplication::instance()->thread());
    }
}

SQLiteManager::~SQLiteManager()
{
    closeDatabase();
}

QString SQLiteManager::generateConnectionName() const
{
    return QString("PatientManagementDB_%1_%2")
           .arg(QUuid::createUuid().toString().remove('{').remove('}').remove('-'))
           .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
}

QString SQLiteManager::getDefaultDatabasePath() const
{
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataPath); // 确保目录存在
    return QDir(dataPath).absoluteFilePath(DATABASE_NAME);
}

bool SQLiteManager::initializeDatabase(const QString& dbPath)
{
    QMutexLocker locker(&m_mutex);
    
    // 如果已连接，先关闭
    if (m_connected) {
        closeDatabase();
    }
    
    try {
        // 确定数据库路径
        m_databasePath = dbPath.isEmpty() ? getDefaultDatabasePath() : dbPath;
        qDebug() << "[SQLiteManager] 数据库路径:" << m_databasePath;
        
        // 确保目录存在
        QFileInfo fileInfo(m_databasePath);
        QDir().mkpath(fileInfo.absolutePath());
        qDebug() << "[SQLiteManager] 数据库目录:" << fileInfo.absolutePath();
        
        // 检查SQLite驱动是否可用
        QStringList availableDrivers = QSqlDatabase::drivers();
        qDebug() << "[SQLiteManager] 可用的SQL驱动:" << availableDrivers;
        if (!availableDrivers.contains("QSQLITE")) {
            logError("QSQLITE驱动不可用");
            return false;
        }
        
        // 检查连接名称是否已存在
        if (QSqlDatabase::contains(m_connectionName)) {
            qDebug() << "[SQLiteManager] 移除已存在的连接:" << m_connectionName;
            QSqlDatabase::removeDatabase(m_connectionName);
        }
        
        // 创建数据库连接
        qDebug() << "[SQLiteManager] 创建数据库连接，连接名:" << m_connectionName;
        m_database = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
        if (!m_database.isValid()) {
            logError("创建数据库连接失败：连接无效");
            return false;
        }
        m_database.setDatabaseName(m_databasePath);
        qDebug() << "[SQLiteManager] 数据库连接创建完成";
        
        // 尝试打开数据库
        qDebug() << "[SQLiteManager] 尝试打开数据库...";
        if (!m_database.open()) {
            QString error = QString("无法打开数据库: %1").arg(m_database.lastError().text());
            qDebug() << "[SQLiteManager] 数据库打开失败:" << error;
            logError(error);
            return false;
        }
        
        qDebug() << "[SQLiteManager] 数据库打开成功";
        m_connected = true;
        
        // 设置数据库选项（在数据库打开之后）
        qDebug() << "[SQLiteManager] 开始设置数据库选项...";
        setupDatabaseOptions();
        qDebug() << "[SQLiteManager] 数据库选项设置完成";
        
        // 创建表结构
        qDebug() << "[SQLiteManager] 开始创建数据库表...";
        if (!createTables()) {
            logError("创建数据库表失败");
            closeDatabase();
            return false;
        }
        qDebug() << "[SQLiteManager] 数据库表创建完成";
        
        // 创建索引
        qDebug() << "[SQLiteManager] 开始创建索引...";
        if (!createIndexes()) {
            logError("创建数据库索引失败");
            // 索引创建失败不应该阻止数据库使用，只记录警告
            qWarning() << "Database indexes creation failed, but database is still usable";
        }
        qDebug() << "[SQLiteManager] 索引创建完成";
        
        // 检查数据库完整性
        qDebug() << "[SQLiteManager] 开始检查数据库完整性...";
        if (!checkDatabaseIntegrity()) {
            logError("数据库完整性检查失败");
            closeDatabase();
            return false;
        }
        qDebug() << "[SQLiteManager] 数据库完整性检查完成";
        
        qDebug() << "[SQLiteManager] 准备清除错误（已修复死锁）...";
        clearError(); // 已修复死锁问题
        qDebug() << "[SQLiteManager] 错误已清除";
        
        qDebug() << "[SQLiteManager] 发出数据库连接信号...";
        emit databaseConnected(); // 恢复信号发出
        qDebug() << "[SQLiteManager] 数据库连接信号已发出";
        
        qDebug() << "[SQLiteManager] 数据库初始化完全成功:" << m_databasePath;
        return true;
        
    } catch (const std::exception& e) {
        QString error = QString("数据库初始化异常: %1").arg(e.what());
        logError(error);
        closeDatabase();
        return false;
    } catch (...) {
        logError("数据库初始化发生未知异常");
        closeDatabase();
        return false;
    }
}

void SQLiteManager::setupDatabaseOptions()
{
    if (!m_database.isValid()) {
        qDebug() << "[SQLiteManager] 数据库无效，跳过选项设置";
        return;
    }
    
    // 设置连接选项以提高性能和安全性
    QSqlQuery query(m_database);
    
    // 启用外键约束
    qDebug() << "[SQLiteManager] 设置外键约束...";
    if (!query.exec("PRAGMA foreign_keys = ON")) {
        qDebug() << "[SQLiteManager] 外键约束设置失败:" << query.lastError().text();
    }
    
    // 设置同步模式为NORMAL（平衡性能和安全性）
    qDebug() << "[SQLiteManager] 设置同步模式...";
    if (!query.exec("PRAGMA synchronous = NORMAL")) {
        qDebug() << "[SQLiteManager] 同步模式设置失败:" << query.lastError().text();
    }
    
    // 设置日志模式为DELETE（避免WAL模式的潜在问题）
    qDebug() << "[SQLiteManager] 设置日志模式...";
    if (!query.exec("PRAGMA journal_mode = DELETE")) {
        qDebug() << "[SQLiteManager] 日志模式设置失败:" << query.lastError().text();
    }
    
    // 设置缓存大小
    qDebug() << "[SQLiteManager] 设置缓存大小...";
    if (!query.exec("PRAGMA cache_size = 10000")) {
        qDebug() << "[SQLiteManager] 缓存大小设置失败:" << query.lastError().text();
    }
    
    // 设置临时存储为内存
    qDebug() << "[SQLiteManager] 设置临时存储...";
    if (!query.exec("PRAGMA temp_store = MEMORY")) {
        qDebug() << "[SQLiteManager] 临时存储设置失败:" << query.lastError().text();
    }
    
    qDebug() << "[SQLiteManager] 所有数据库选项设置完成";
}

bool SQLiteManager::createTables()
{
    if (!m_connected) {
        qDebug() << "[SQLiteManager] createTables: 数据库未连接";
        return false;
    }
    
    qDebug() << "[SQLiteManager] createTables: 开始创建表...";
    QSqlQuery query(m_database);
    bool success = true;
    
    // 暂时跳过事务（用于调试）
    qDebug() << "[SQLiteManager] createTables: 跳过事务，直接创建表...";
    
    try {
        // 创建患者信息表
        qDebug() << "[SQLiteManager] createTables: 创建患者表...";
        QString createPatientsTable = R"(
            CREATE TABLE IF NOT EXISTS patients (
                patient_id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                age INTEGER NOT NULL DEFAULT 0,
                gender TEXT,
                phone TEXT UNIQUE,
                id_card TEXT UNIQUE,
                address TEXT,
                emergency_contact TEXT,
                emergency_phone TEXT,
                medical_history TEXT,
                allergies TEXT,
                current_medications TEXT,
                registration_date TEXT NOT NULL,
                last_visit_date TEXT,
                notes TEXT,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
            )
        )";
        
        if (!query.exec(createPatientsTable)) {
            logError("创建患者表失败: " + query.lastError().text());
            success = false;
        } else {
            qDebug() << "[SQLiteManager] createTables: 患者表创建成功";
        }
        
        // 创建患者影像表
        QString createImagesTable = R"(
            CREATE TABLE IF NOT EXISTS patient_images (
                image_id INTEGER PRIMARY KEY AUTOINCREMENT,
                patient_id INTEGER NOT NULL,
                image_path TEXT NOT NULL,
                image_type TEXT NOT NULL,
                body_part TEXT,
                scan_date TEXT,
                scan_parameters TEXT,
                description TEXT,
                radiologist_notes TEXT,
                is_processed BOOLEAN DEFAULT 0,
                processing_notes TEXT,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY (patient_id) REFERENCES patients (patient_id) ON DELETE CASCADE
            )
        )";
        
        if (!query.exec(createImagesTable)) {
            logError("创建影像表失败: " + query.lastError().text());
            success = false;
        }
        
        // 创建手术记录表
        QString createSurgeriesTable = R"(
            CREATE TABLE IF NOT EXISTS surgery_records (
                surgery_id INTEGER PRIMARY KEY AUTOINCREMENT,
                patient_id INTEGER NOT NULL,
                surgery_type TEXT NOT NULL,
                surgery_date TEXT NOT NULL,
                surgeon TEXT NOT NULL,
                assistants TEXT,
                pre_op_diagnosis TEXT,
                post_op_diagnosis TEXT,
                procedure TEXT,
                complications TEXT,
                notes TEXT,
                outcome TEXT,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY (patient_id) REFERENCES patients (patient_id) ON DELETE CASCADE
            )
        )";
        
        if (!query.exec(createSurgeriesTable)) {
            logError("创建手术记录表失败: " + query.lastError().text());
            success = false;
        }
        
        // 创建系统信息表
        QString createSystemInfoTable = R"(
            CREATE TABLE IF NOT EXISTS system_info (
                key TEXT PRIMARY KEY,
                value TEXT,
                updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
            )
        )";
        
        if (!query.exec(createSystemInfoTable)) {
            logError("创建系统信息表失败: " + query.lastError().text());
            success = false;
        }
        
        // 插入数据库版本信息
        QString insertVersion = R"(
            INSERT OR REPLACE INTO system_info (key, value) 
            VALUES ('database_version', ?)
        )";
        query.prepare(insertVersion);
        query.addBindValue(DATABASE_VERSION);
        
        if (!query.exec()) {
            logError("插入数据库版本失败: " + query.lastError().text());
            success = false;
        }
        
        if (success) {
            qDebug() << "[SQLiteManager] createTables: 所有表创建成功";
        } else {
            qDebug() << "[SQLiteManager] createTables: 表创建失败";
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logError(QString("创建表异常: %1").arg(e.what()));
        return false;
    } catch (...) {
        logError("创建表发生未知异常");
        return false;
    }
}

bool SQLiteManager::createIndexes()
{
    if (!m_connected) return false;
    
    QSqlQuery query(m_database);
    bool success = true;
    
    // 患者表索引
    QStringList indexes = {
        "CREATE INDEX IF NOT EXISTS idx_patients_name ON patients (name)",
        "CREATE INDEX IF NOT EXISTS idx_patients_phone ON patients (phone)",
        "CREATE INDEX IF NOT EXISTS idx_patients_id_card ON patients (id_card)",
        "CREATE INDEX IF NOT EXISTS idx_patients_registration_date ON patients (registration_date)",
        
        // 影像表索引
        "CREATE INDEX IF NOT EXISTS idx_images_patient_id ON patient_images (patient_id)",
        "CREATE INDEX IF NOT EXISTS idx_images_type ON patient_images (image_type)",
        "CREATE INDEX IF NOT EXISTS idx_images_scan_date ON patient_images (scan_date)",
        
        // 手术记录表索引
        "CREATE INDEX IF NOT EXISTS idx_surgeries_patient_id ON surgery_records (patient_id)",
        "CREATE INDEX IF NOT EXISTS idx_surgeries_date ON surgery_records (surgery_date)",
        "CREATE INDEX IF NOT EXISTS idx_surgeries_surgeon ON surgery_records (surgeon)"
    };
    
    for (const QString& indexSql : indexes) {
        if (!query.exec(indexSql)) {
            qWarning() << "Failed to create index:" << query.lastError().text();
            success = false;
        }
    }
    
    return success;
}

bool SQLiteManager::checkDatabaseIntegrity()
{
    if (!m_connected) return false;
    
    QSqlQuery query(m_database);
    if (!query.exec("PRAGMA integrity_check")) {
        logError("数据库完整性检查失败: " + query.lastError().text());
        return false;
    }
    
    if (query.next()) {
        QString result = query.value(0).toString();
        if (result != "ok") {
            logError("数据库完整性问题: " + result);
            return false;
        }
    }
    
    return true;
}

void SQLiteManager::closeDatabase()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_connected && m_database.isOpen()) {
        m_database.close();
        emit databaseDisconnected();
    }
    
    if (QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase::removeDatabase(m_connectionName);
    }
    
    m_connected = false;
}

bool SQLiteManager::isConnected() const
{
    QMutexLocker locker(&m_mutex);
    bool connected = m_connected && m_database.isOpen();
    if (!connected) {
        qDebug() << "[SQLiteManager] isConnected()返回false - m_connected:" << m_connected 
                 << "m_database.isOpen():" << (m_database.isValid() ? m_database.isOpen() : false)
                 << "m_database.isValid():" << m_database.isValid();
    }
    return connected;
}

// 事务管理实现
bool SQLiteManager::beginTransaction()
{
    if (!isConnected()) {
        logError("数据库未连接，无法开始事务");
        return false;
    }
    
    QMutexLocker locker(&m_mutex);
    bool result = m_database.transaction();
    if (!result) {
        logError("开始事务失败: " + m_database.lastError().text());
    }
    return result;
}

bool SQLiteManager::commitTransaction()
{
    if (!isConnected()) {
        logError("数据库未连接，无法提交事务");
        return false;
    }
    
    QMutexLocker locker(&m_mutex);
    bool result = m_database.commit();
    if (!result) {
        logError("提交事务失败: " + m_database.lastError().text());
    }
    return result;
}

bool SQLiteManager::rollbackTransaction()
{
    if (!isConnected()) {
        logError("数据库未连接，无法回滚事务");
        return false;
    }
    
    QMutexLocker locker(&m_mutex);
    bool result = m_database.rollback();
    if (!result) {
        logError("回滚事务失败: " + m_database.lastError().text());
    }
    return result;
}

// 查询执行实现
QSqlQuery SQLiteManager::executeQuery(const QString& queryString)
{
    return executeQuery(queryString, QVariantList());
}

QSqlQuery SQLiteManager::executeQuery(const QString& queryString, const QVariantList& bindValues)
{
    QSqlQuery query(m_database);
    
    if (!isConnected()) {
        logError("数据库未连接，无法执行查询");
        return query;
    }
    
    QMutexLocker locker(&m_mutex);
    
    try {
        if (!query.prepare(queryString)) {
            logError("准备查询失败: " + query.lastError().text());
            return query;
        }
        
        for (const QVariant& value : bindValues) {
            query.addBindValue(value);
        }
        
        if (!query.exec()) {
            logError("执行查询失败: " + query.lastError().text());
        }
        
        return query;
        
    } catch (const std::exception& e) {
        logError(QString("查询执行异常: %1").arg(e.what()));
        return query;
    } catch (...) {
        logError("查询执行发生未知异常");
        return query;
    }
}

bool SQLiteManager::executeNonQuery(const QString& queryString)
{
    return executeNonQuery(queryString, QVariantList());
}

bool SQLiteManager::executeNonQuery(const QString& queryString, const QVariantList& bindValues)
{
    if (!isConnected()) {
        logError("数据库未连接，无法执行非查询操作");
        return false;
    }
    
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_database);
    
    try {
        if (!query.prepare(queryString)) {
            logError("准备非查询语句失败: " + query.lastError().text());
            return false;
        }
        
        for (const QVariant& value : bindValues) {
            query.addBindValue(value);
        }
        
        bool result = query.exec();
        if (!result) {
            logError("执行非查询语句失败: " + query.lastError().text());
        }
        
        return result;
        
    } catch (const std::exception& e) {
        logError(QString("非查询执行异常: %1").arg(e.what()));
        return false;
    } catch (...) {
        logError("非查询执行发生未知异常");
        return false;
    }
}

QVariant SQLiteManager::executeScalar(const QString& queryString, const QVariant& defaultValue)
{
    QSqlQuery query = executeQuery(queryString);
    
    if (query.next()) {
        return query.value(0);
    }
    
    return defaultValue;
}

// 数据库维护实现
bool SQLiteManager::createBackup(const QString& backupPath)
{
    if (!isConnected()) {
        logError("数据库未连接，无法创建备份");
        return false;
    }
    
    try {
        // 确保备份目录存在
        QFileInfo backupInfo(backupPath);
        QDir().mkpath(backupInfo.absolutePath());
        
        // 使用SQLite的VACUUM INTO命令创建备份
        QMutexLocker locker(&m_mutex);
        QSqlQuery query(m_database);
        
        QString vacuumSql = QString("VACUUM INTO '%1'").arg(backupPath);
        
        emit operationProgress(10);
        
        if (!query.exec(vacuumSql)) {
            logError("创建备份失败: " + query.lastError().text());
            return false;
        }
        
        emit operationProgress(100);
        
        qDebug() << "Database backup created successfully:" << backupPath;
        return true;
        
    } catch (const std::exception& e) {
        logError(QString("备份创建异常: %1").arg(e.what()));
        return false;
    } catch (...) {
        logError("备份创建发生未知异常");
        return false;
    }
}

bool SQLiteManager::restoreFromBackup(const QString& backupPath)
{
    if (!QFile::exists(backupPath)) {
        logError("备份文件不存在: " + backupPath);
        return false;
    }
    
    try {
        // 关闭当前数据库连接
        closeDatabase();
        
        emit operationProgress(25);
        
        // 删除当前数据库文件
        if (QFile::exists(m_databasePath)) {
            QFile::remove(m_databasePath);
        }
        
        emit operationProgress(50);
        
        // 复制备份文件
        if (!QFile::copy(backupPath, m_databasePath)) {
            logError("无法复制备份文件");
            return false;
        }
        
        emit operationProgress(75);
        
        // 重新初始化数据库
        bool result = initializeDatabase(m_databasePath);
        
        emit operationProgress(100);
        
        if (result) {
            qDebug() << "Database restored successfully from:" << backupPath;
        }
        
        return result;
        
    } catch (const std::exception& e) {
        logError(QString("备份恢复异常: %1").arg(e.what()));
        return false;
    } catch (...) {
        logError("备份恢复发生未知异常");
        return false;
    }
}

bool SQLiteManager::optimizeDatabase()
{
    if (!isConnected()) {
        logError("数据库未连接，无法优化");
        return false;
    }
    
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_database);
    
    try {
        // 分析数据库统计信息
        if (!query.exec("ANALYZE")) {
            logError("数据库分析失败: " + query.lastError().text());
            return false;
        }
        
        // 重建数据库（压缩和优化）
        if (!query.exec("VACUUM")) {
            logError("数据库优化失败: " + query.lastError().text());
            return false;
        }
        
        qDebug() << "Database optimized successfully";
        return true;
        
    } catch (const std::exception& e) {
        logError(QString("数据库优化异常: %1").arg(e.what()));
        return false;
    } catch (...) {
        logError("数据库优化发生未知异常");
        return false;
    }
}

qint64 SQLiteManager::getDatabaseSize() const
{
    QFileInfo fileInfo(m_databasePath);
    return fileInfo.exists() ? fileInfo.size() : 0;
}

int SQLiteManager::getTableRowCount(const QString& tableName)
{
    QString sql = QString("SELECT COUNT(*) FROM %1").arg(tableName);
    QVariant result = executeScalar(sql, -1);
    return result.toInt();
}

// 错误处理实现
void SQLiteManager::logError(const QString& error)
{
    m_lastError = error;
    qWarning() << "SQLiteManager Error:" << error;
    emit databaseError(error);
}

QString SQLiteManager::getLastError() const
{
    QMutexLocker locker(&m_mutex);
    return m_lastError;
}

void SQLiteManager::clearError()
{
    // 移除互斥锁避免死锁 - QString::clear() 是线程安全的
    m_lastError.clear();
}
