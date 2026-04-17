#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include "FrameworkExport.h"
#include "ResourceManagement/SingletonManager.h"

#include <QObject>
#include <QMutex>
#include <QQueue>
#include <QSet>
#include <QSqlDatabase>
#include <QString>

/**
 * @brief 轻量级数据库连接池
 *
 * 仅提供同步获取/释放连接的能力，以满足插件的 CRUD 需求。
 */
class FRAMEWORK_EXPORT DatabaseManager : public QObject, public SingletonManager<DatabaseManager>
{
    Q_OBJECT
    friend class SingletonManager<DatabaseManager>;

public:
    static DatabaseManager* instance() { return &SingletonManager<DatabaseManager>::instance(); }

    bool initialize(const QString& databasePath = QString());
    bool isInitialized() const;

    QSqlDatabase getConnection();
    void releaseConnection(const QSqlDatabase& connection);

    QString getPoolStatus() const;
    QString lastError() const;

    int minConnections() const;
    int maxConnections() const;
    void setConnectionLimits(int minimum, int maximum);

private:
    DatabaseManager();
    ~DatabaseManager() override;

    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    bool createConnection();
    QString buildConnectionName(int index) const;
    void closeAllConnections();

    mutable QMutex m_mutex;
    QString m_databasePath;
    QString m_lastError;

    bool m_initialized;
    int m_minConnections;
    int m_maxConnections;
    int m_connectionCounter;

    QQueue<QString> m_availableConnections;
    QSet<QString> m_inUseConnections;
};

#endif // DATABASEMANAGER_H
