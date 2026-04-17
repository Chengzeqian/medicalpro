#include "DatabaseManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include "Logger.h"

DatabaseManager::DatabaseManager()
    : QObject(nullptr)
    , m_initialized(false)
    , m_minConnections(1)
    , m_maxConnections(5)
    , m_connectionCounter(0)
{
}

DatabaseManager::~DatabaseManager()
{
    closeAllConnections();
}

bool DatabaseManager::initialize(const QString& databasePath)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_initialized) {
            return true;
        }
    }

    QString resolvedPath = databasePath;
    if (resolvedPath.isEmpty()) {
        const QString defaultDir = QCoreApplication::applicationDirPath() + "/data";
        QDir().mkpath(defaultDir);
        resolvedPath = defaultDir + "/medical.db";
    }

    QFileInfo info(resolvedPath);
    const QString absolutePath = info.absoluteFilePath();

    LOG_INFO("DatabaseManager", QString("Initializing database at %1").arg(absolutePath));

    QMutexLocker locker(&m_mutex);
    m_databasePath = absolutePath;

    m_availableConnections.clear();
    m_inUseConnections.clear();
    m_connectionCounter = 0;

    for (int i = 0; i < m_minConnections; ++i) {
        if (!createConnection()) {
            m_lastError = QStringLiteral("Failed to create initial database connections");
            return false;
        }
    }

    m_initialized = true;
    LOG_INFO("DatabaseManager", "Database manager initialized successfully");
    return true;
}

bool DatabaseManager::isInitialized() const
{
    QMutexLocker locker(&m_mutex);
    return m_initialized;
}

QSqlDatabase DatabaseManager::getConnection()
{
    QMutexLocker locker(&m_mutex);
    if (!m_initialized) {
        m_lastError = QStringLiteral("Database manager is not initialized");
        return QSqlDatabase();
    }

    if (!m_availableConnections.isEmpty()) {
        const QString connectionName = m_availableConnections.dequeue();
        m_inUseConnections.insert(connectionName);
        return QSqlDatabase::database(connectionName);
    }

    if (m_connectionCounter < m_maxConnections && createConnection()) {
        const QString connectionName = m_availableConnections.dequeue();
        m_inUseConnections.insert(connectionName);
        return QSqlDatabase::database(connectionName);
    }

    m_lastError = QStringLiteral("All database connections are in use");
    return QSqlDatabase();
}

void DatabaseManager::releaseConnection(const QSqlDatabase& connection)
{
    if (!connection.isValid()) {
        return;
    }

    const QString name = connection.connectionName();
    QMutexLocker locker(&m_mutex);
    if (m_inUseConnections.remove(name)) {
        m_availableConnections.enqueue(name);
    }
}

QString DatabaseManager::getPoolStatus() const
{
    QMutexLocker locker(&m_mutex);
    return QString("Connections - Available: %1, InUse: %2, Total: %3")
        .arg(m_availableConnections.size())
        .arg(m_inUseConnections.size())
        .arg(m_connectionCounter);
}

QString DatabaseManager::lastError() const
{
    QMutexLocker locker(&m_mutex);
    return m_lastError;
}

int DatabaseManager::minConnections() const
{
    QMutexLocker locker(&m_mutex);
    return m_minConnections;
}

int DatabaseManager::maxConnections() const
{
    QMutexLocker locker(&m_mutex);
    return m_maxConnections;
}

void DatabaseManager::setConnectionLimits(int minimum, int maximum)
{
    if (minimum <= 0 || maximum < minimum) {
        LOG_WARNING("DatabaseManager", "Invalid connection limits specified");
        return;
    }

    QMutexLocker locker(&m_mutex);
    m_minConnections = minimum;
    m_maxConnections = maximum;
}

bool DatabaseManager::createConnection()
{
    const QString connectionName = buildConnectionName(++m_connectionCounter);

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    db.setDatabaseName(m_databasePath);

    if (!db.open()) {
        const QString error = db.lastError().text();
        LOG_ERROR("DatabaseManager", QString("Failed to open database connection: %1").arg(error));
        QSqlDatabase::removeDatabase(connectionName);
        --m_connectionCounter;
        return false;
    }

    LOG_INFO("DatabaseManager", QString("Database connection created: %1").arg(connectionName));
    m_availableConnections.enqueue(connectionName);
    return true;
}

QString DatabaseManager::buildConnectionName(int index) const
{
    return QStringLiteral("DatabasePoolConnection_%1").arg(index);
}

void DatabaseManager::closeAllConnections()
{
    QMutexLocker locker(&m_mutex);

    for (const QString& name : std::as_const(m_inUseConnections)) {
        QSqlDatabase::database(name).close();
        QSqlDatabase::removeDatabase(name);
    }

    for (const QString& name : std::as_const(m_availableConnections)) {
        QSqlDatabase::database(name).close();
        QSqlDatabase::removeDatabase(name);
    }

    m_inUseConnections.clear();
    m_availableConnections.clear();
    m_connectionCounter = 0;
    m_initialized = false;
}
