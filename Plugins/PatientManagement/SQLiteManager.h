#ifndef SQLITE_MANAGER_H
#define SQLITE_MANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMutex>
#include <QMutexLocker>
#include <QString>
#include <QVariant>
#include <QDateTime>
#include <memory>

/**
 * @brief SQLite数据库管理器
 * 
 * 单例模式的SQLite数据库管理类，提供线程安全的数据库操作。
 * 负责数据库连接、表创建、查询执行和维护操作。
 */
class SQLiteManager : public QObject
{
    Q_OBJECT
    
public:
    /**
     * @brief 获取单例实例
     * @return SQLiteManager单例引用
     */
    static SQLiteManager& instance();
    
    /**
     * @brief 析构函数
     */
    ~SQLiteManager();
    
    /**
     * @brief 初始化数据库
     * @param dbPath 数据库文件路径，如果为空则使用默认路径
     * @return 成功返回true，失败返回false
     */
    bool initializeDatabase(const QString& dbPath = QString());
    
    /**
     * @brief 检查数据库是否已连接
     * @return 已连接返回true，否则返回false
     */
    bool isConnected() const;
    
    /**
     * @brief 关闭数据库连接
     */
    void closeDatabase();
    
    /**
     * @brief 获取默认数据库路径
     * @return 默认数据库文件路径
     */
    QString getDefaultDatabasePath() const;
    
    // ========== 事务管理 ==========
    
    /**
     * @brief 开始事务
     * @return 成功返回true，失败返回false
     */
    bool beginTransaction();
    
    /**
     * @brief 提交事务
     * @return 成功返回true，失败返回false
     */
    bool commitTransaction();
    
    /**
     * @brief 回滚事务
     * @return 成功返回true，失败返回false
     */
    bool rollbackTransaction();
    
    // ========== 查询执行 ==========
    
    /**
     * @brief 执行查询SQL（SELECT语句）
     * @param queryString SQL查询语句
     * @return QSqlQuery对象，包含查询结果
     */
    QSqlQuery executeQuery(const QString& queryString);
    
    /**
     * @brief 执行查询SQL（带参数绑定）
     * @param queryString 带参数占位符的SQL语句
     * @param bindValues 参数值列表
     * @return QSqlQuery对象，包含查询结果
     */
    QSqlQuery executeQuery(const QString& queryString, const QVariantList& bindValues);
    
    /**
     * @brief 执行非查询SQL（INSERT, UPDATE, DELETE等）
     * @param queryString SQL语句
     * @return 成功返回true，失败返回false
     */
    bool executeNonQuery(const QString& queryString);
    
    /**
     * @brief 执行非查询SQL（带参数绑定）
     * @param queryString 带参数占位符的SQL语句
     * @param bindValues 参数值列表
     * @return 成功返回true，失败返回false
     */
    bool executeNonQuery(const QString& queryString, const QVariantList& bindValues);
    
    /**
     * @brief 执行单个值查询
     * @param queryString SQL查询语句
     * @param defaultValue 默认值
     * @return 查询结果，如果无结果则返回默认值
     */
    QVariant executeScalar(const QString& queryString, const QVariant& defaultValue = QVariant());
    
    // ========== 数据库维护 ==========
    
    /**
     * @brief 创建数据库备份
     * @param backupPath 备份文件路径
     * @return 成功返回true，失败返回false
     */
    bool createBackup(const QString& backupPath);
    
    /**
     * @brief 从备份恢复数据库
     * @param backupPath 备份文件路径
     * @return 成功返回true，失败返回false
     */
    bool restoreFromBackup(const QString& backupPath);
    
    /**
     * @brief 优化数据库
     * @return 成功返回true，失败返回false
     */
    bool optimizeDatabase();
    
    /**
     * @brief 检查数据库完整性
     * @return 完整性良好返回true，否则返回false
     */
    bool checkDatabaseIntegrity();
    
    /**
     * @brief 获取数据库大小（字节）
     * @return 数据库文件大小
     */
    qint64 getDatabaseSize() const;
    
    /**
     * @brief 获取表的记录数
     * @param tableName 表名
     * @return 记录数，错误时返回-1
     */
    int getTableRowCount(const QString& tableName);
    
    /**
     * @brief 获取最后一个错误信息
     * @return 错误描述字符串
     */
    QString getLastError() const;
    
    /**
     * @brief 清除错误状态
     */
    void clearError();

signals:
    /**
     * @brief 数据库错误信号
     * @param error 错误描述
     */
    void databaseError(const QString& error);
    
    /**
     * @brief 数据库连接成功信号
     */
    void databaseConnected();
    
    /**
     * @brief 数据库断开连接信号
     */
    void databaseDisconnected();
    
    /**
     * @brief 数据库操作进度信号（用于大操作如备份恢复）
     * @param percentage 进度百分比 (0-100)
     */
    void operationProgress(int percentage);

private:
    /**
     * @brief 私有构造函数（单例模式）
     */
    SQLiteManager();
    

    
    // 禁用拷贝和赋值
    Q_DISABLE_COPY(SQLiteManager)
    
    /**
     * @brief 创建数据库表
     * @return 成功返回true，失败返回false
     */
    bool createTables();
    
    /**
     * @brief 升级数据库结构
     * @param currentVersion 当前版本
     * @param targetVersion 目标版本
     * @return 成功返回true，失败返回false
     */
    bool upgradeDatabase(int currentVersion, int targetVersion);
    
    /**
     * @brief 检查并创建必要的索引
     * @return 成功返回true，失败返回false
     */
    bool createIndexes();
    
    /**
     * @brief 设置数据库选项
     */
    void setupDatabaseOptions();
    
    /**
     * @brief 记录错误信息
     * @param error 错误描述
     */
    void logError(const QString& error);
    
    /**
     * @brief 生成唯一的连接名
     * @return 连接名字符串
     */
    QString generateConnectionName() const;

private:
    QSqlDatabase m_database;                    // 数据库连接对象
    QString m_lastError;                        // 最后一个错误信息
    mutable QMutex m_mutex;                     // 线程安全互斥锁
    bool m_connected;                           // 连接状态标志
    QString m_databasePath;                     // 数据库文件路径
    QString m_connectionName;                   // 连接名称
    
    static const int DATABASE_VERSION = 1;      // 数据库版本号
    static const QString DATABASE_NAME;         // 数据库文件名
    static std::unique_ptr<SQLiteManager> m_instance; // 单例实例
    static QMutex m_instanceMutex;              // 单例创建互斥锁
};

#endif // SQLITE_MANAGER_H
