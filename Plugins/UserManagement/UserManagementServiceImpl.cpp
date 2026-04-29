#include "UserManagementServiceImpl.h"
// 注意：InstrumentItem等数据结构已通过UserDataStructures.h包含核心框架定义
#include <QApplication>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QCryptographicHash>
#include <QUuid>
#include <QMessageBox>
#include <QDebug>
#include <QRegularExpression>
#include <QMutexLocker>
#include <QTimer>
#include <QThread>
#include <QDateTime>
#include <QMetaObject>
#include "Framework/DatabaseManager.h"
#include "Framework/Platform/Contracts/platform_runtime_host_ports.h"

UserManagementServiceImpl::UserManagementServiceImpl(QObject* parent)
    : UserManagementService(parent)
    , m_databaseInitialized(false)
    , m_serviceInitialized(false)
    , m_eventBus(nullptr)
    , m_totalLoginAttempts(0)
    , m_successfulLogins(0)
    , m_failedLogins(0)
    , m_offlineMode(false)
    , m_databasePromptActive(false)
{
    qDebug() << "[UserManagementServiceImpl] UserManagement service instance created";
    
    // 初始化数据库路径
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(dataDir);
    if (!dir.exists()) {
        dir.mkpath(dataDir);
    }
    m_databasePath = dataDir + "/users.db";
    
    // 初始化会话定时器
    m_sessionTimer = new QTimer(this);
    m_sessionTimer->setInterval(300000); // 5分钟检查一次
    connect(m_sessionTimer, &QTimer::timeout, this, &UserManagementServiceImpl::checkSessionTimeout);
    
    // 初始化角色权限映射
    initializeRolePermissions();
    
    qDebug() << "[UserManagementServiceImpl] UserManagement service instance initialization complete";
}

void UserManagementServiceImpl::setEventBus(IPlatformEventBusPort* eventBus)
{
    m_eventBus = eventBus;
}

UserManagementServiceImpl::~UserManagementServiceImpl()
{
    qDebug() << "[UserManagementServiceImpl] Destroying UserManagement service instance";
    
    // 停止会话定时器
    if (m_sessionTimer) {
        m_sessionTimer->stop();
    }
    
    // 清理所有会话
    QMutexLocker locker(&m_sessionMutex);
    m_activeSessions.clear();
    
    // 关闭并释放数据库连接
    if (m_database.isValid()) {
        if (m_database.isOpen()) {
            m_database.close();
        }

        DatabaseManager* manager = DatabaseManager::instance();
        if (manager && manager->isInitialized()) {
            manager->releaseConnection(m_database);
        }

        m_database = QSqlDatabase();
    }
    
    qDebug() << "[UserManagementServiceImpl] UserManagement service instance destroyed";
}

bool UserManagementServiceImpl::ensureDatabaseAvailable(const char* operationContext)
{
    if (m_offlineMode) {
        m_lastError = QStringLiteral("Database is in offline mode");
        logDatabaseStatus(QStringLiteral("Offline mode - %1").arg(QString::fromUtf8(operationContext)));
        return false;
    }

    if (!ensureDatabaseInitialized()) {
        logDatabaseStatus(QStringLiteral("Initialization failed - %1").arg(QString::fromUtf8(operationContext)));
        return false;
    }

    if (!m_database.isValid() || !m_database.isOpen()) {
        m_lastError = QStringLiteral("Database connection unavailable");
        logDatabaseStatus(QStringLiteral("Connection invalid - %1").arg(QString::fromUtf8(operationContext)));
        return false;
    }

    return true;
}

bool UserManagementServiceImpl::ensureDatabaseInitialized()
{
    if (m_offlineMode) {
        m_lastError = QStringLiteral("Database is in offline mode");
        return false;
    }

    if (m_databaseInitialized && m_database.isValid() && m_database.isOpen()) {
        return true;
    }

    QMutexLocker locker(&m_databaseMutex);

    if (m_offlineMode) {
        m_lastError = QStringLiteral("Database is in offline mode");
        return false;
    }

    if (m_databaseInitialized && m_database.isValid() && m_database.isOpen()) {
        return true;
    }

    DatabaseManager* manager = DatabaseManager::instance();
    if (!manager) {
        locker.unlock();
        return handleDatabaseInitializationFailure(tr("数据库管理器不可用"));
    }

    if (!manager->initialize(m_databasePath)) {
        const QString error = manager->lastError();
        locker.unlock();
        return handleDatabaseInitializationFailure(error.isEmpty() ? tr("数据库初始化失败") : error);
    }

    QSqlDatabase connection = manager->getConnection();
    if (!connection.isValid() || !connection.isOpen()) {
        const QString error = manager->lastError();
        if (connection.isValid()) {
            manager->releaseConnection(connection);
        }
        locker.unlock();
        return handleDatabaseInitializationFailure(error.isEmpty() ? tr("获取数据库连接失败") : error);
    }

    m_database = connection;

    QSqlQuery pragmaQuery(m_database);
    if (!pragmaQuery.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        const QString error = pragmaQuery.lastError().text();
        manager->releaseConnection(m_database);
        m_database = QSqlDatabase();
        locker.unlock();
        return handleDatabaseInitializationFailure(tr("启用外键支持失败: %1").arg(error));
    }

    m_offlineMode = false;
    m_databaseInitialized = true;
    m_serviceInitialized = true;

    if (!createDatabaseTables()) {
        manager->releaseConnection(m_database);
        m_database = QSqlDatabase();
        m_databaseInitialized = false;
        m_serviceInitialized = false;
        locker.unlock();
        return handleDatabaseInitializationFailure(tr("创建数据库表失败"));
    }

    initializeRolePermissions();

    locker.unlock();

    initializeTestData();
    createDefaultAdminUser();

    logDatabaseStatus(QStringLiteral("Database initialized"));

    return true;
}

bool UserManagementServiceImpl::handleDatabaseInitializationFailure(const QString& errorMessage)
{
    QString message = errorMessage;
    if (message.isEmpty()) {
        message = tr("未知错误");
    }

    m_lastError = message;
    logDatabaseStatus(QStringLiteral("Database initialization failed: %1").arg(message));

    if (m_offlineMode) {
        return false;
    }

    if (m_databasePromptActive) {
        return false;
    }

    m_databasePromptActive = true;

    QMessageBox::StandardButton choice = QMessageBox::Retry;

    auto dialogInvoker = [&]() {
        choice = QMessageBox::critical(
            nullptr,
            tr("数据库初始化失败"),
            tr("无法初始化用户数据库：\n%1\n\n请选择操作：\n- 重试：再次尝试初始化数据库\n- 忽略：进入离线模式（部分功能不可用）\n- 退出：关闭应用程序").arg(message),
            QMessageBox::Retry | QMessageBox::Ignore | QMessageBox::Abort,
            QMessageBox::Retry);
    };

    if (QApplication::instance() && QThread::currentThread() != QApplication::instance()->thread()) {
        QMetaObject::invokeMethod(QApplication::instance(), dialogInvoker, Qt::BlockingQueuedConnection);
    } else {
        dialogInvoker();
    }

    m_databasePromptActive = false;

    if (choice == QMessageBox::Retry) {
        logDatabaseStatus(QStringLiteral("User selected retry for database initialization"));
        return ensureDatabaseInitialized();
    }

    if (choice == QMessageBox::Ignore) {
        m_offlineMode = true;
        logDatabaseStatus(QStringLiteral("User selected offline mode"));
        emit databaseError(tr("数据库初始化失败，已切换至离线模式"));
        return false;
    }

    logDatabaseStatus(QStringLiteral("User selected application exit"));
    if (QApplication::instance()) {
        QMetaObject::invokeMethod(QApplication::instance(), []() {
            QApplication::quit();
        }, Qt::QueuedConnection);
    }

    return false;
}

void UserManagementServiceImpl::logDatabaseStatus(const QString& status)
{
    qInfo() << "[UserManagementServiceImpl][Database]" << status;
}

// ========== 用户认证管理 ==========

UserInfo UserManagementServiceImpl::loginUser(const QString& username, const QString& password)
{
    qDebug() << "[UserManagementServiceImpl] User login requested:" << username;
    
    UserInfo invalidUser;
    m_totalLoginAttempts++;

    if (!ensureDatabaseAvailable("loginUser")) {
        qWarning() << "[UserManagementServiceImpl] User login failed, database unavailable:" << m_lastError;
        emit authenticationFailed(username, QStringLiteral("数据库不可用"));
        return invalidUser;
    }
    
    // 检查账户锁定状态
    if (m_lockoutEndTime.contains(username) && 
        QDateTime::currentDateTime() < m_lockoutEndTime[username]) {
        logSecurityEvent("WARNING", QString("Login attempt on locked account: %1").arg(username));
        emit authenticationFailed(username, "账户已被锁定");
        return invalidUser;
    }
    
    // 验证用户凭据
    UserInfo user = authenticateUser(username, password);
    if (!user.isValid()) {
        // 记录失败尝试
        m_failedLogins++;
        m_loginAttempts[username] = m_loginAttempts.value(username, 0) + 1;
        
        // 检查是否需要锁定账户
        if (m_loginAttempts[username] >= m_passwordPolicy.maxLoginAttempts) {
            m_lockoutEndTime[username] = QDateTime::currentDateTime().addSecs(m_passwordPolicy.lockoutDuration * 60);
            logSecurityEvent("ERROR", QString("Account locked after repeated failed logins: %1").arg(username));
        }
        
        logSecurityEvent("WARNING", QString("Login failed: %1").arg(username));
        emit authenticationFailed(username, "用户名或密码错误");
        return invalidUser;
    }
    
    // 检查账户状态
    if (!user.isActive) {
        logSecurityEvent("WARNING", QString("Login attempt on inactive account: %1").arg(username));
        emit authenticationFailed(username, "账户未激活");
        return invalidUser;
    }
    
    if (user.isLocked) {
        logSecurityEvent("WARNING", QString("Login attempt on locked account: %1").arg(username));
        emit authenticationFailed(username, "账户已被锁定");
        return invalidUser;
    }
    
    // 登录成功处理
    m_successfulLogins++;
    m_loginAttempts.remove(username); // 清除失败尝试记录
    m_lockoutEndTime.remove(username);
    
    // 更新最后登录时间
    user.lastLoginTime = QDateTime::currentDateTime();
    updateUser(user);
    
    // 创建会话
    QString sessionId = createUserSession(user);
    if (sessionId.isEmpty()) {
        logSecurityEvent("ERROR", QString("Failed to create session: %1").arg(username));
        return invalidUser;
    }
    
    // 设置当前用户
    m_currentUser = user;

    // 启动会话定时器（确保在正确的线程中启动）
    // 🔥 修复：使用 invokeMethod 确保定时器在创建它的线程中启动
    if (!m_sessionTimer->isActive()) {
        if (QThread::currentThread() == m_sessionTimer->thread()) {
            m_sessionTimer->start();
        } else {
            QMetaObject::invokeMethod(m_sessionTimer, "start", Qt::QueuedConnection);
        }
    }

    // 记录登录日志
    logOperation(user.id, "LOGIN", QString("User login: %1").arg(username), QString(), true);
    logSecurityEvent("INFO", QString("User login succeeded: %1").arg(username), user.id);
    
    // 发送登录成功事件
    publishPlatformEvent("user/login", {{"userId", user.id}, {"username", username}});
    emit userLoggedIn(user);

    qDebug() << "[UserManagementServiceImpl] User login succeeded:" << username;
    return user;
}

bool UserManagementServiceImpl::logoutUser(int userId)
{
    qDebug() << "[UserManagementServiceImpl] User logout requested:" << userId;

    if (!ensureDatabaseAvailable("logoutUser")) {
        qWarning() << "[UserManagementServiceImpl] User logout failed, database unavailable:" << m_lastError;
        return false;
    }

    UserInfo user = getUser(userId);
    if (!user.isValid()) {
        qWarning() << "[UserManagementServiceImpl] Logout failed, user not found:" << userId;
        return false;
    }

    if (!destroyUserSession(userId)) {
        qWarning() << "[UserManagementServiceImpl] Failed to destroy session:" << userId;
        return false;
    }

    if (m_currentUser.id == userId) {
        m_currentUser = UserInfo();
    }

    logOperation(userId, "LOGOUT", QString("User logout: %1").arg(user.username), QString(), true);
    logSecurityEvent("INFO", QString("User logout: %1").arg(user.username), userId);

    publishPlatformEvent("user/logout", {{"userId", userId}, {"username", user.username}});
    emit userLoggedOut(userId);

    qDebug() << "[UserManagementServiceImpl] User logout succeeded:" << user.username;
    return true;
}

bool UserManagementServiceImpl::updateUser(const UserInfo& user)
{
    qDebug() << "[UserManagementServiceImpl] Updating user profile:" << user.id;

    if (!ensureDatabaseAvailable("updateUser")) {
        qWarning() << "[UserManagementServiceImpl] User update failed, database unavailable:" << m_lastError;
        return false;
    }

    // 验证用户信息
    auto validation = validateUserInfo(user, true);
    if (!validation.first) {
        m_lastError = validation.second;
        qWarning() << "[UserManagementServiceImpl] User validation failed:" << m_lastError;
        return false;
    }

    // 检查用户名是否已存在
    if (isUsernameExists(user.username, user.id)) {
        m_lastError = "Username already exists";
        qWarning() << "[UserManagementServiceImpl] Username already exists:" << user.username;
        return false;
    }

    // 检查邮箱是否已存在
    if (!user.email.isEmpty() && isEmailExists(user.email, user.id)) {
        m_lastError = "Email already exists";
        qWarning() << "[UserManagementServiceImpl] Email already exists:" << user.email;
        return false;
    }

    // 准备SQL更新语句
    QSqlQuery query(m_database);
    query.prepare("UPDATE users SET username = ?, real_name = ?, email = ?, phone = ?, "
                  "department = ?, job_title = ?, role = ?, last_update_time = ?, "
                  "is_active = ?, is_locked = ?, notes = ? WHERE id = ?");
    query.addBindValue(user.username);
    query.addBindValue(user.realName);
    query.addBindValue(user.email);
    query.addBindValue(user.phone);
    query.addBindValue(user.department);
    query.addBindValue(user.jobTitle);
    query.addBindValue(static_cast<int>(user.role));
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    query.addBindValue(user.isActive);
    query.addBindValue(user.isLocked);
    query.addBindValue(user.notes);
    query.addBindValue(user.id);

    if (!executeQuery(query)) {
        m_lastError = "Database update failed: " + query.lastError().text();
        qWarning() << "[UserManagementServiceImpl] Failed to update user:" << m_lastError;
        return false;
    }

    // 记录更新日志
    logOperation(user.id, "UPDATE", QString("User profile updated: %1").arg(user.username), QString(), true);
    logSecurityEvent("INFO", QString("User profile updated: %1").arg(user.username), user.id);

    // 发送更新事件
    publishPlatformEvent("user/update", {{"userId", user.id}, {"username", user.username}});
    emit userUpdated(user);

    qDebug() << "[UserManagementServiceImpl] User updated successfully:" << user.username;
    return true;
}

UserInfo UserManagementServiceImpl::getUser(int userId)
{
    UserInfo user;

    if (!ensureDatabaseAvailable("getUser")) {
        qWarning() << "[UserManagementServiceImpl] Failed to load user, database unavailable:" << m_lastError;
        return user;
    }

    QSqlQuery query(m_database);
    query.prepare("SELECT id, username, password_hash, salt, real_name, email, phone, "
                  "department, job_title, role, create_time, last_login_time, last_update_time, "
                  "is_active, is_locked, notes FROM users WHERE id = ?");
    query.addBindValue(userId);

    if (!executeQuery(query)) {
        return user;
    }

    if (query.next()) {
        user.id = query.value(0).toInt();
        user.username = query.value(1).toString();
        user.realName = query.value(4).toString();
        user.email = query.value(5).toString();
        user.phone = query.value(6).toString();
        user.department = query.value(7).toString();
        user.jobTitle = query.value(8).toString();
        user.role = static_cast<UserRole>(query.value(9).toInt());
        user.createTime = QDateTime::fromString(query.value(10).toString(), Qt::ISODate);
        user.lastLoginTime = QDateTime::fromString(query.value(11).toString(), Qt::ISODate);
        user.lastUpdateTime = QDateTime::fromString(query.value(12).toString(), Qt::ISODate);
        user.isActive = query.value(13).toBool();
        user.isLocked = query.value(14).toBool();
        user.notes = query.value(15).toString();
    }

    return user;
}

UserInfo UserManagementServiceImpl::getUserByUsername(const QString& username)
{
    UserInfo user;

    if (!ensureDatabaseAvailable("getUserByUsername")) {
        qWarning() << "[UserManagementServiceImpl] Failed to load user by username, database unavailable:" << m_lastError;
        return user;
    }

    QSqlQuery query(m_database);
    query.prepare("SELECT id, username, password_hash, salt, real_name, email, phone, "
                  "department, job_title, role, create_time, last_login_time, last_update_time, "
                  "is_active, is_locked, notes FROM users WHERE username = ?");
    query.addBindValue(username);

    if (!executeQuery(query)) {
        return user;
    }

    if (query.next()) {
        user.id = query.value(0).toInt();
        user.username = query.value(1).toString();
        user.realName = query.value(4).toString();
        user.email = query.value(5).toString();
        user.phone = query.value(6).toString();
        user.department = query.value(7).toString();
        user.jobTitle = query.value(8).toString();
        user.role = static_cast<UserRole>(query.value(9).toInt());
        user.createTime = QDateTime::fromString(query.value(10).toString(), Qt::ISODate);
        user.lastLoginTime = QDateTime::fromString(query.value(11).toString(), Qt::ISODate);
        user.lastUpdateTime = QDateTime::fromString(query.value(12).toString(), Qt::ISODate);
        user.isActive = query.value(13).toBool();
        user.isLocked = query.value(14).toBool();
        user.notes = query.value(15).toString();
    }

    return user;
}

QList<UserInfo> UserManagementServiceImpl::getAllUsers()
{
    QList<UserInfo> users;

    if (!ensureDatabaseAvailable("getAllUsers")) {
        qWarning() << "[UserManagementServiceImpl] Failed to list users, database unavailable:" << m_lastError;
        return users;
    }

    QSqlQuery query(m_database);
    query.prepare("SELECT id, username, password_hash, salt, real_name, email, phone, "
                  "department, job_title, role, create_time, last_login_time, last_update_time, "
                  "is_active, is_locked, notes FROM users");

    if (!executeQuery(query)) {
        return users;
    }

    while (query.next()) {
        UserInfo user;
        user.id = query.value(0).toInt();
        user.username = query.value(1).toString();
        user.realName = query.value(4).toString();
        user.email = query.value(5).toString();
        user.phone = query.value(6).toString();
        user.department = query.value(7).toString();
        user.jobTitle = query.value(8).toString();
        user.role = static_cast<UserRole>(query.value(9).toInt());
        user.createTime = QDateTime::fromString(query.value(10).toString(), Qt::ISODate);
        user.lastLoginTime = QDateTime::fromString(query.value(11).toString(), Qt::ISODate);
        user.lastUpdateTime = QDateTime::fromString(query.value(12).toString(), Qt::ISODate);
        user.isActive = query.value(13).toBool();
        user.isLocked = query.value(14).toBool();
        user.notes = query.value(15).toString();
        users.append(user);
    }

    return users;
}

QList<UserInfo> UserManagementServiceImpl::searchUsers(const UserSearchCriteria& criteria)
{
    QList<UserInfo> users;

    if (!ensureDatabaseAvailable("searchUsers")) {
        qWarning() << "[UserManagementServiceImpl] Failed to search users, database unavailable:" << m_lastError;
        return users;
    }

    if (!criteria.hasSearchCriteria()) {
        return getAllUsers();
    }

    QSqlQuery query(m_database);
    QString sql = "SELECT id, username, password_hash, salt, real_name, email, phone, "
                  "department, job_title, role, create_time, last_login_time, last_update_time, "
                  "is_active, is_locked, notes FROM users WHERE ";
    QList<QString> conditions;

    if (!criteria.username.isEmpty()) {
        conditions.append("username LIKE '%" + criteria.username + "%'");
    }

    if (!criteria.realName.isEmpty()) {
        conditions.append("real_name LIKE '%" + criteria.realName + "%'");
    }

    if (!criteria.email.isEmpty()) {
        conditions.append("email LIKE '%" + criteria.email + "%'");
    }

    if (!criteria.phone.isEmpty()) {
        conditions.append("phone LIKE '%" + criteria.phone + "%'");
    }

    if (!criteria.department.isEmpty()) {
        conditions.append("department LIKE '%" + criteria.department + "%'");
    }

    if (!criteria.jobTitle.isEmpty()) {
        conditions.append("job_title LIKE '%" + criteria.jobTitle + "%'");
    }

    if (criteria.searchByRole) {
        conditions.append("role = " + QString::number(static_cast<int>(criteria.role)));
    }

    if (criteria.activeOnly) {
        conditions.append("is_active = 1");
    }

    if (criteria.unlockedOnly) {
        conditions.append("is_locked = 0");
    }

    sql += conditions.join(" AND ");

    query.prepare(sql);

    if (!executeQuery(query)) {
        return users;
    }

    while (query.next()) {
        UserInfo user;
        user.id = query.value(0).toInt();
        user.username = query.value(1).toString();
        user.realName = query.value(4).toString();
        user.email = query.value(5).toString();
        user.phone = query.value(6).toString();
        user.department = query.value(7).toString();
        user.jobTitle = query.value(8).toString();
        user.role = static_cast<UserRole>(query.value(9).toInt());
        user.createTime = QDateTime::fromString(query.value(10).toString(), Qt::ISODate);
        user.lastLoginTime = QDateTime::fromString(query.value(11).toString(), Qt::ISODate);
        user.lastUpdateTime = QDateTime::fromString(query.value(12).toString(), Qt::ISODate);
        user.isActive = query.value(13).toBool();
        user.isLocked = query.value(14).toBool();
        user.notes = query.value(15).toString();
        users.append(user);
    }

    return users;
}

bool UserManagementServiceImpl::changePassword(int userId, const QString& oldPassword, const QString& newPassword)
{
    qDebug() << "[UserManagementServiceImpl] Change password requested:" << userId;

    if (!ensureDatabaseAvailable("changePassword")) {
        qWarning() << "[UserManagementServiceImpl] Change password failed, database unavailable:" << m_lastError;
        return false;
    }

    // 检查用户是否存在
    UserInfo user = getUser(userId);
    if (!user.isValid()) {
        qWarning() << "[UserManagementServiceImpl] Change password failed, user not found:" << userId;
        return false;
    }

    // 从数据库获取密码哈希和盐值
    QSqlQuery query(m_database);
    query.prepare("SELECT password_hash, salt FROM users WHERE id = ?");
    query.addBindValue(userId);

    if (!executeQuery(query) || !query.next()) {
        qWarning() << "[UserManagementServiceImpl] Failed to load password metadata";
        return false;
    }

    QString storedHash = query.value(0).toString();
    QString storedSalt = query.value(1).toString();

    // 验证旧密码
    if (!verifyPassword(oldPassword, storedHash, storedSalt)) {
        qWarning() << "[UserManagementServiceImpl] Old password verification failed";
        return false;
    }

    // 生成新密码哈希
    QString newSalt = generateSalt();
    QString passwordHash = hashPassword(newPassword, newSalt);

    // 准备SQL更新语句
    QSqlQuery updateQuery(m_database);
    updateQuery.prepare("UPDATE users SET password_hash = ?, salt = ? WHERE id = ?");
    updateQuery.addBindValue(passwordHash);
    updateQuery.addBindValue(newSalt);
    updateQuery.addBindValue(userId);

    if (!executeQuery(updateQuery)) {
        m_lastError = "Database update failed: " + updateQuery.lastError().text();
        qWarning() << "[UserManagementServiceImpl] Failed to change password:" << m_lastError;
        return false;
    }

    // 记录修改日志
    logOperation(userId, "UPDATE", QString("Password changed: %1").arg(user.username), QString(), true);
    logSecurityEvent("INFO", QString("Password changed: %1").arg(user.username), userId);

    // 发送修改事件
    publishPlatformEvent("user/update", {{"userId", userId}, {"username", user.username}});
    emit userUpdated(user);

    qDebug() << "[UserManagementServiceImpl] Password changed successfully:" << user.username;
    return true;
}

bool UserManagementServiceImpl::resetPassword(int userId, const QString& newPassword)
{
    qDebug() << "[UserManagementServiceImpl] Reset password requested:" << userId;

    if (!ensureDatabaseAvailable("resetPassword")) {
        qWarning() << "[UserManagementServiceImpl] Reset password failed, database unavailable:" << m_lastError;
        return false;
    }

    // 检查用户是否存在
    UserInfo user = getUser(userId);
    if (!user.isValid()) {
        qWarning() << "[UserManagementServiceImpl] Reset password failed, user not found:" << userId;
        return false;
    }

    // 生成新密码哈希
    QString salt = generateSalt();
    QString passwordHash = hashPassword(newPassword, salt);

    // 准备SQL更新语句
    QSqlQuery query(m_database);
    query.prepare("UPDATE users SET password_hash = ?, salt = ? WHERE id = ?");
    query.addBindValue(passwordHash);
    query.addBindValue(salt);
    query.addBindValue(userId);

    if (!executeQuery(query)) {
        m_lastError = "Database update failed: " + query.lastError().text();
        qWarning() << "[UserManagementServiceImpl] Failed to reset password:" << m_lastError;
        return false;
    }

    // 记录重置日志
    logOperation(userId, "UPDATE", QString("Password reset: %1").arg(user.username), QString(), true);
    logSecurityEvent("INFO", QString("Password reset: %1").arg(user.username), userId);

    // 发送重置事件
    publishPlatformEvent("user/update", {{"userId", userId}, {"username", user.username}});
    emit userUpdated(user);

    qDebug() << "[UserManagementServiceImpl] Password reset successfully:" << user.username;
    return true;
}

bool UserManagementServiceImpl::checkUserPermission(int userId, const QString& permission)
{
    if (!ensureDatabaseAvailable("checkUserPermission")) {
        qWarning() << "[UserManagementServiceImpl] Permission check failed, database unavailable:" << m_lastError;
        return false;
    }

    UserRole role = getUserRole(userId);
    if (role == UserRole::Guest) {
        return false;
    }

    QStringList permissions = getRolePermissions(role);
    return permissions.contains(permission);
}

UserRole UserManagementServiceImpl::getUserRole(int userId)
{
    if (!ensureDatabaseAvailable("getUserRole")) {
        qWarning() << "[UserManagementServiceImpl] Failed to load user role, database unavailable:" << m_lastError;
        return UserRole::Guest;
    }

    QSqlQuery query(m_database);
    query.prepare("SELECT role FROM users WHERE id = ?");
    query.addBindValue(userId);

    if (!executeQuery(query)) {
        return UserRole::Guest;
    }

    if (query.next()) {
        return static_cast<UserRole>(query.value(0).toInt());
    }

    return UserRole::Guest;
}

bool UserManagementServiceImpl::setUserRole(int userId, UserRole role)
{
    qDebug() << "[UserManagementServiceImpl] Setting user role:" << userId << "role:" << static_cast<int>(role);

    if (!ensureDatabaseAvailable("setUserRole")) {
        qWarning() << "[UserManagementServiceImpl] Set user role failed, database unavailable:" << m_lastError;
        return false;
    }

    // 检查用户是否存在
    UserInfo user = getUser(userId);
    if (!user.isValid()) {
        qWarning() << "[UserManagementServiceImpl] Set user role failed, user not found:" << userId;
        return false;
    }

    // 准备SQL更新语句
    QSqlQuery query(m_database);
    query.prepare("UPDATE users SET role = ? WHERE id = ?");
    query.addBindValue(static_cast<int>(role));
    query.addBindValue(userId);

    if (!executeQuery(query)) {
        m_lastError = "Database update failed: " + query.lastError().text();
        qWarning() << "[UserManagementServiceImpl] Failed to set user role:" << m_lastError;
        return false;
    }

    // 记录设置日志
    logOperation(userId, "UPDATE", QString("User role updated: %1").arg(user.username), QString(), true);
    logSecurityEvent("INFO", QString("User role updated: %1").arg(user.username), userId);

    // 发送设置事件
    publishPlatformEvent("user/update", {{"userId", userId}, {"username", user.username}});
    emit userUpdated(user);

    qDebug() << "[UserManagementServiceImpl] User role updated successfully:" << user.username;
    return true;
}

int UserManagementServiceImpl::getUserCount()
{
    if (!ensureDatabaseAvailable("getUserCount")) {
        qWarning() << "[UserManagementServiceImpl] Failed to count users, database unavailable:" << m_lastError;
        return 0;
    }

    QSqlQuery query(m_database);
    query.prepare("SELECT COUNT(*) FROM users");

    if (!executeQuery(query)) {
        return 0;
    }

    if (query.next()) {
        return query.value(0).toInt();
    }

    return 0;
}

bool UserManagementServiceImpl::backupDatabase(const QString& backupPath)
{
    qDebug() << "[UserManagementServiceImpl] Backing up user database to:" << backupPath;

    if (!ensureDatabaseAvailable("backupDatabase")) {
        m_lastError = "Database unavailable";
        return false;
    }

    // 检查备份路径是否有效
    if (backupPath.isEmpty()) {
        m_lastError = "Backup path is empty";
        qWarning() << "[UserManagementServiceImpl] Backup failed:" << m_lastError;
        return false;
    }

    // 获取数据库文件路径
    QString dbPath = m_database.databaseName();
    if (dbPath.isEmpty()) {
        m_lastError = "Failed to resolve database file path";
        qWarning() << "[UserManagementServiceImpl] Backup failed:" << m_lastError;
        return false;
    }

    // 复制数据库文件到备份位置
    QFile sourceFile(dbPath);
    if (!sourceFile.exists()) {
        m_lastError = "Database file not found: " + dbPath;
        qWarning() << "[UserManagementServiceImpl] Backup failed:" << m_lastError;
        return false;
    }

    // 如果备份文件已存在，先删除
    QFile backupFile(backupPath);
    if (backupFile.exists()) {
        if (!backupFile.remove()) {
            m_lastError = "Failed to remove existing backup file: " + backupPath;
            qWarning() << "[UserManagementServiceImpl] Backup failed:" << m_lastError;
            return false;
        }
    }

    // 执行文件复制
    if (!sourceFile.copy(backupPath)) {
        m_lastError = "Failed to copy database file: " + sourceFile.errorString();
        qWarning() << "[UserManagementServiceImpl] Backup failed:" << m_lastError;
        return false;
    }

    qDebug() << "[UserManagementServiceImpl] Database backup completed:" << backupPath;
    return true;
}

bool UserManagementServiceImpl::checkDatabaseConnection()
{
    return ensureDatabaseAvailable("checkDatabaseConnection");
}

QString UserManagementServiceImpl::createUserSession(const UserInfo& user)
{
    QString sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    QMutexLocker locker(&m_sessionMutex);
    
    UserSession session;
    session.userId = user.id;
    session.sessionId = sessionId;
    session.loginTime = QDateTime::currentDateTime();
    session.lastActivityTime = session.loginTime;
    session.ipAddress = "127.0.0.1";
    session.isActive = true;
    
    m_activeSessions[user.id] = session;
    
    return sessionId;
}

bool UserManagementServiceImpl::destroyUserSession(int userId)
{
    QMutexLocker locker(&m_sessionMutex);
    m_activeSessions.remove(userId);
    return true;
}

void UserManagementServiceImpl::updateSessionActivity(int userId)
{
    QMutexLocker locker(&m_sessionMutex);
    
    if (m_activeSessions.contains(userId)) {
        m_activeSessions[userId].lastActivityTime = QDateTime::currentDateTime();
    }
}

QString UserManagementServiceImpl::hashPassword(const QString& password, const QString& salt)
{
    QString saltedPassword = password + salt;
    QByteArray hash = QCryptographicHash::hash(saltedPassword.toUtf8(), QCryptographicHash::Sha256);
    return QString(hash.toHex());
}

QString UserManagementServiceImpl::generateSalt()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).left(16);
}

bool UserManagementServiceImpl::verifyPassword(const QString& password, const QString& hash, const QString& salt)
{
    return hashPassword(password, salt) == hash;
}

QStringList UserManagementServiceImpl::getRolePermissions(UserRole role)
{
    return m_rolePermissions.value(role, QStringList());
}

bool UserManagementServiceImpl::checkOperationPermission(UserRole currentUserRole, const QString& operation, UserRole targetUserRole)
{
    if (currentUserRole == UserRole::Administrator) {
        return true;
    }
    
    QStringList permissions = getRolePermissions(currentUserRole);
    return permissions.contains(operation);
}

void UserManagementServiceImpl::logOperation(int userId, const QString& operation, const QString& description, 
                                           const QString& target, bool success, const QString& errorMessage)
{
    qDebug() << "[UserManagementServiceImpl] Recording operation:" << operation << description;
}

void UserManagementServiceImpl::logSecurityEvent(const QString& level, const QString& message, int userId)
{
    qDebug() << QString("[UserManagementServiceImpl][%1] %2").arg(level).arg(message);
}

QPair<bool, QString> UserManagementServiceImpl::validateUserInfo(const UserInfo& user, bool isUpdate)
{
    if (user.username.isEmpty()) {
        return qMakePair(false, QString("Username cannot be empty"));
    }
    
    if (user.realName.isEmpty()) {
        return qMakePair(false, QString("Real name cannot be empty"));
    }
    
    if (!isUpdate && user.password.isEmpty()) {
        return qMakePair(false, QString("Password cannot be empty"));
    }
    
    return qMakePair(true, QString());
}

bool UserManagementServiceImpl::isUsernameExists(const QString& username, int excludeUserId)
{
    QSqlQuery query(m_database);
    
    if (excludeUserId >= 0) {
        query.prepare("SELECT COUNT(*) FROM users WHERE username = ? AND id != ?");
        query.addBindValue(username);
        query.addBindValue(excludeUserId);
    } else {
        query.prepare("SELECT COUNT(*) FROM users WHERE username = ?");
        query.addBindValue(username);
    }
    
    if (executeQuery(query) && query.next()) {
        return query.value(0).toInt() > 0;
    }
    
    return false;
}

bool UserManagementServiceImpl::isEmailExists(const QString& email, int excludeUserId)
{
    if (email.isEmpty()) return false;
    
    QSqlQuery query(m_database);
    
    if (excludeUserId >= 0) {
        query.prepare("SELECT COUNT(*) FROM users WHERE email = ? AND id != ?");
        query.addBindValue(email);
        query.addBindValue(excludeUserId);
    } else {
        query.prepare("SELECT COUNT(*) FROM users WHERE email = ?");
        query.addBindValue(email);
    }
    
    if (executeQuery(query) && query.next()) {
        return query.value(0).toInt() > 0;
    }
    
    return false;
}

void UserManagementServiceImpl::publishPlatformEvent(const QString& topic, const QVariantMap& properties)
{
    if (m_eventBus) {
        m_eventBus->publish(topic, properties);
        qDebug() << "[UserManagementServiceImpl] Publishing platform event:" << topic;
    }
}

void UserManagementServiceImpl::initializeRolePermissions()
{
    // 访客权限
    m_rolePermissions[UserRole::Guest] = QStringList{
        "VIEW_PROFILE"
    };
    
    // 操作员权限
    m_rolePermissions[UserRole::Operator] = QStringList{
        "VIEW_PROFILE",
        "CHANGE_PASSWORD",
        "VIEW_PATIENTS",
        "ADD_PATIENT",
        "UPDATE_PATIENT"
    };
    
    // 医生权限
    m_rolePermissions[UserRole::Doctor] = QStringList{
        "VIEW_PROFILE",
        "CHANGE_PASSWORD",
        "VIEW_PATIENTS",
        "ADD_PATIENT",
        "UPDATE_PATIENT",
        "DELETE_PATIENT",
        "VIEW_IMAGES",
        "ADD_IMAGE",
        "UPDATE_IMAGE",
        "DELETE_IMAGE"
    };
    
    // 管理员权限
    m_rolePermissions[UserRole::Administrator] = QStringList{
        "ALL"
    };
}

void UserManagementServiceImpl::createDefaultAdminUser()
{
    // 检查是否已存在管理员用户
    QSqlQuery query(m_database);
    query.prepare("SELECT COUNT(*) FROM users WHERE role = ?");
    query.addBindValue(static_cast<int>(UserRole::Administrator));
    
    if (executeQuery(query) && query.next() && query.value(0).toInt() > 0) {
        qDebug() << "[UserManagementServiceImpl] Administrator account already exists";
        return;
    }
    
    // 创建默认管理员用户
    UserInfo admin;
    admin.username = "admin";
    admin.password = "admin123";
    admin.realName = "系统管理员";
    admin.email = "admin@medicalpro.com";
    admin.department = "信息科";
    admin.jobTitle = "系统管理员";
    admin.role = UserRole::Administrator;
    admin.isActive = true;
    admin.isLocked = false;
    admin.notes = "系统默认管理员账户";
    
    if (registerUser(admin)) {
        qDebug() << "[UserManagementServiceImpl] Default administrator account created";
        qDebug() << "[UserManagementServiceImpl] Default credentials: username=admin, password=admin123";
    } else {
        qWarning() << "[UserManagementServiceImpl] Failed to create default administrator account:" << m_lastError;
    }
}

// ========== 缺失方法的实现 ==========

bool UserManagementServiceImpl::createDatabaseTables()
{
    QSqlQuery query(m_database);

    // 创建用户表
    QString createUsersTable = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            salt TEXT NOT NULL,
            real_name TEXT NOT NULL,
            email TEXT,
            phone TEXT,
            department TEXT,
            job_title TEXT,
            role INTEGER NOT NULL DEFAULT 0,
            create_time TEXT NOT NULL,
            last_login_time TEXT,
            last_update_time TEXT,
            is_active INTEGER NOT NULL DEFAULT 1,
            is_locked INTEGER NOT NULL DEFAULT 0,
            notes TEXT
        )
    )";

    if (!query.exec(createUsersTable)) {
        m_lastError = "Failed to create users table: " + query.lastError().text();
        qWarning() << "[UserManagementServiceImpl]" << m_lastError;
        return false;
    }

    qDebug() << "[UserManagementServiceImpl] User table created successfully";
    return createDoctorTables() && createPatientsTables() &&
           createSurgeryTables() && createSurgeryCaseTables() &&
           createDoctorPatientTables() && createInstrumentTables() &&
           createSurgeryInstrumentTables();
}

bool UserManagementServiceImpl::createDoctorTables()
{
    QSqlQuery query(m_database);
    QString sql = R"(
        CREATE TABLE IF NOT EXISTS doctor_profiles (
            user_id INTEGER PRIMARY KEY,
            gender TEXT,
            age INTEGER,
            hospital TEXT,
            created_at TEXT NOT NULL,
            updated_at TEXT,
            FOREIGN KEY (user_id) REFERENCES users(id)
        )
    )";

    if (!query.exec(sql)) {
        m_lastError = "Failed to create doctors table: " + query.lastError().text();
        return false;
    }
    return true;
}

bool UserManagementServiceImpl::createPatientsTables()
{
    QSqlQuery query(m_database);
    QString sql = R"(
        CREATE TABLE IF NOT EXISTS patients (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            gender TEXT,
            age INTEGER,
            phone TEXT,
            email TEXT,
            description TEXT,
            is_active INTEGER NOT NULL DEFAULT 1,
            created_at TEXT NOT NULL,
            updated_at TEXT
        )
    )";

    if (!query.exec(sql)) {
        m_lastError = "Failed to create patients table: " + query.lastError().text();
        return false;
    }
    return true;
}

bool UserManagementServiceImpl::createSurgeryTables()
{
    QSqlQuery query(m_database);

    // 创建手术项表（手术类型字典）
    QString sqlItems = R"(
        CREATE TABLE IF NOT EXISTS surgery_items (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE NOT NULL,
            tags TEXT,
            description TEXT,
            is_active INTEGER NOT NULL DEFAULT 1,
            created_at TEXT NOT NULL,
            updated_at TEXT
        )
    )";

    if (!query.exec(sqlItems)) {
        m_lastError = "Failed to create surgery_items table: " + query.lastError().text();
        return false;
    }

    return true;
}

bool UserManagementServiceImpl::createSurgeryCaseTables()
{
    QSqlQuery query(m_database);
    QString sql = R"(
        CREATE TABLE IF NOT EXISTS surgery_cases (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            patient_id INTEGER NOT NULL,
            surgery_item_id INTEGER NOT NULL,
            doctor_user_id INTEGER NOT NULL,
            scheduled_time TEXT,
            status TEXT,
            note TEXT,
            created_at TEXT NOT NULL,
            updated_at TEXT,
            FOREIGN KEY (patient_id) REFERENCES patients(id),
            FOREIGN KEY (surgery_item_id) REFERENCES surgery_items(id),
            FOREIGN KEY (doctor_user_id) REFERENCES users(id)
        )
    )";

    if (!query.exec(sql)) {
        m_lastError = "Failed to create surgery_cases table: " + query.lastError().text();
        return false;
    }
    return true;
}

bool UserManagementServiceImpl::createDoctorPatientTables()
{
    QSqlQuery query(m_database);

    // 【修复】先检查表是否存在以及结构是否正确
    QSqlQuery checkQuery(m_database);
    checkQuery.exec("PRAGMA table_info(doctor_patient)");

    bool tableExists = false;
    bool hasDoctorId = false;
    bool hasPatientId = false;

    while (checkQuery.next()) {
        tableExists = true;
        QString colName = checkQuery.value(1).toString();
        if (colName == "doctor_id") {
            hasDoctorId = true;
        } else if (colName == "patient_id") {
            hasPatientId = true;
        }
    }

    // 如果表存在但结构不对，删除重建
    if (tableExists && (!hasDoctorId || !hasPatientId)) {
        qWarning() << "[UserManagementServiceImpl] doctor_patient table schema mismatch detected, recreating table";
        if (!query.exec("DROP TABLE IF EXISTS doctor_patient")) {
            m_lastError = "Failed to drop legacy doctor_patient table: " + query.lastError().text();
            return false;
        }
    }

    QString sql = R"(
        CREATE TABLE IF NOT EXISTS doctor_patient (
            doctor_id INTEGER NOT NULL,
            patient_id INTEGER NOT NULL,
            create_time TEXT NOT NULL,
            PRIMARY KEY (doctor_id, patient_id),
            FOREIGN KEY (doctor_id) REFERENCES doctors(id),
            FOREIGN KEY (patient_id) REFERENCES patients(id)
        )
    )";

    if (!query.exec(sql)) {
        m_lastError = "Failed to create doctor_patient table: " + query.lastError().text();
        return false;
    }

    qDebug() << "[UserManagementServiceImpl] doctor_patient table created or validated";
    return true;
}

bool UserManagementServiceImpl::createInstrumentTables()
{
    // 器械表由InstrumentManagement插件管理，这里返回true
    return true;
}

bool UserManagementServiceImpl::createSurgeryInstrumentTables()
{
    QSqlQuery query(m_database);
    QString sql = R"(
        CREATE TABLE IF NOT EXISTS surgery_instrument (
            surgery_id INTEGER NOT NULL,
            instrument_id INTEGER NOT NULL,
            create_time TEXT NOT NULL,
            PRIMARY KEY (surgery_id, instrument_id),
            FOREIGN KEY (surgery_id) REFERENCES surgeries(id)
        )
    )";

    if (!query.exec(sql)) {
        m_lastError = "Failed to create surgery_instrument table: " + query.lastError().text();
        return false;
    }
    return true;
}

void UserManagementServiceImpl::initializeTestData()
{
    qDebug() << "[UserManagementServiceImpl] Initializing seed data";

    // 创建默认管理员用户
    createDefaultAdminUser();

    // 可以在这里添加更多测试数据
}

bool UserManagementServiceImpl::executeQuery(QSqlQuery& query)
{
    if (!query.exec()) {
        m_lastError = "SQL execution failed: " + query.lastError().text();
        qWarning() << "[UserManagementServiceImpl]" << m_lastError;
        qWarning() << "[UserManagementServiceImpl] SQL:" << query.lastQuery();
        return false;
    }
    return true;
}

UserInfo UserManagementServiceImpl::authenticateUser(const QString& username, const QString& password)
{
    UserInfo user;

    if (!ensureDatabaseAvailable("authenticateUser")) {
        return user;
    }

    QSqlQuery query(m_database);
    query.prepare("SELECT id, username, password_hash, salt, real_name, email, phone, "
                  "department, job_title, role, create_time, last_login_time, last_update_time, "
                  "is_active, is_locked, notes FROM users WHERE username = ?");
    query.addBindValue(username);

    if (!executeQuery(query)) {
        return user;
    }

    if (query.next()) {
        QString storedHash = query.value(2).toString();
        QString salt = query.value(3).toString();

        if (verifyPassword(password, storedHash, salt)) {
            user.id = query.value(0).toInt();
            user.username = query.value(1).toString();
            user.realName = query.value(4).toString();
            user.email = query.value(5).toString();
            user.phone = query.value(6).toString();
            user.department = query.value(7).toString();
            user.jobTitle = query.value(8).toString();
            user.role = static_cast<UserRole>(query.value(9).toInt());
            user.createTime = QDateTime::fromString(query.value(10).toString(), Qt::ISODate);
            user.lastLoginTime = QDateTime::fromString(query.value(11).toString(), Qt::ISODate);
            user.lastUpdateTime = QDateTime::fromString(query.value(12).toString(), Qt::ISODate);
            user.isActive = query.value(13).toBool();
            user.isLocked = query.value(14).toBool();
            user.notes = query.value(15).toString();
        }
    }

    return user;
}

void UserManagementServiceImpl::checkSessionTimeout()
{
    qDebug() << "[UserManagementServiceImpl] Checking session timeout";
    cleanupExpiredSessions();
}

void UserManagementServiceImpl::cleanupExpiredSessions()
{
    QMutexLocker locker(&m_sessionMutex);

    QList<int> expiredSessions;

    // 查找过期的会话
    for (auto it = m_activeSessions.begin(); it != m_activeSessions.end(); ++it) {
        if (it.value().isExpired()) {
            expiredSessions.append(it.key());
        }
    }

    // 删除过期的会话
    for (int userId : expiredSessions) {
        m_activeSessions.remove(userId);
        qDebug() << "[UserManagementServiceImpl] Cleaning expired session for user:" << userId;
    }
}

void UserManagementServiceImpl::handleDatabaseError(const QSqlQuery& query)
{
    QString errorMsg = query.lastError().text();
    m_lastError = errorMsg;

    qWarning() << "[UserManagementServiceImpl] Database error:" << errorMsg;
    qWarning() << "[UserManagementServiceImpl] SQL:" << query.lastQuery();

    // 如果是数据库连接错误，尝试重新连接
    if (query.lastError().type() == QSqlError::ConnectionError) {
        qWarning() << "[UserManagementServiceImpl] Database connection error detected, attempting reconnect";
        initializeDatabase();
    }
}

// ========== 用户管理相关方法 ==========

bool UserManagementServiceImpl::registerUser(const UserInfo& user)
{
    if (!ensureDatabaseAvailable("registerUser")) {
        return false;
    }

    // 生成密码哈希
    QString salt = generateSalt();
    QString passwordHash = hashPassword(user.password, salt);

    QSqlQuery query(m_database);
    query.prepare("INSERT INTO users (username, password_hash, salt, real_name, email, phone, "
                  "department, job_title, role, create_time, is_active, is_locked, notes) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(user.username);
    query.addBindValue(passwordHash);
    query.addBindValue(salt);
    query.addBindValue(user.realName);
    query.addBindValue(user.email);
    query.addBindValue(user.phone);
    query.addBindValue(user.department);
    query.addBindValue(user.jobTitle);
    query.addBindValue(static_cast<int>(user.role));
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    query.addBindValue(user.isActive);
    query.addBindValue(user.isLocked);
    query.addBindValue(user.notes);

    return executeQuery(query);
}

bool UserManagementServiceImpl::deleteUser(int userId)
{
    if (!ensureDatabaseAvailable("deleteUser")) {
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare("DELETE FROM users WHERE id = ?");
    query.addBindValue(userId);

    return executeQuery(query);
}

UserInfo UserManagementServiceImpl::getUserById(int userId)
{
    return getUser(userId);
}

UserInfo UserManagementServiceImpl::getCurrentUser()
{
    return m_currentUser;
}

bool UserManagementServiceImpl::isUserLoggedIn(int userId)
{
    // 检查用户是否有活跃会话
    return m_activeSessions.contains(userId) && m_activeSessions[userId].isActive;
}

QString UserManagementServiceImpl::getDatabaseStatus()
{
    if (!m_database.isOpen()) {
        return "数据库未连接";
    }

    QSqlQuery query(m_database);
    query.exec("SELECT COUNT(*) FROM users");
    if (query.next()) {
        int userCount = query.value(0).toInt();
        return QString("数据库已连接，共有 %1 个用户").arg(userCount);
    }

    return "数据库已连接";
}

int UserManagementServiceImpl::validatePasswordStrength(const QString& password)
{
    int strength = 0;

    if (password.length() >= 8) strength++;
    if (password.length() >= 12) strength++;

    // 检查是否包含大写字母
    if (password.contains(QRegularExpression("[A-Z]"))) strength++;

    // 检查是否包含小写字母
    if (password.contains(QRegularExpression("[a-z]"))) strength++;

    // 检查是否包含数字
    if (password.contains(QRegularExpression("[0-9]"))) strength++;

    // 检查是否包含特殊字符
    if (password.contains(QRegularExpression("[^A-Za-z0-9]"))) strength++;

    // 返回0-4的评级
    return qMin(strength / 2, 4);
}

// ========== UI对话框方法（暂时返回false，需要UI实现） ==========

bool UserManagementServiceImpl::showLoginDialog(QWidget* parent)
{
    qWarning() << "[UserManagementServiceImpl] showLoginDialog not implemented";
    return false;
}

bool UserManagementServiceImpl::showRegisterDialog(QWidget* parent)
{
    qWarning() << "[UserManagementServiceImpl] showRegisterDialog not implemented";
    return false;
}

bool UserManagementServiceImpl::showUserManagementDialog(QWidget* parent)
{
    qWarning() << "[UserManagementServiceImpl] showUserManagementDialog not implemented";
    return false;
}

// ========== 患者管理方法 ==========

bool UserManagementServiceImpl::createPatient(const PatientItem& patient)
{
    if (!ensureDatabaseAvailable("createPatient")) {
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare("INSERT INTO patients (name, gender, age, phone, email, description, "
                  "is_active, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(patient.name);
    query.addBindValue(patient.gender);
    query.addBindValue(patient.age);
    query.addBindValue(patient.phone);
    query.addBindValue(patient.email);
    query.addBindValue(patient.description);
    query.addBindValue(patient.isActive);
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));

    return executeQuery(query);
}

bool UserManagementServiceImpl::updatePatient(const PatientItem& patient)
{
    if (!ensureDatabaseAvailable("updatePatient")) {
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare("UPDATE patients SET name=?, gender=?, age=?, phone=?, email=?, "
                  "description=?, is_active=?, updated_at=? WHERE id=?");
    query.addBindValue(patient.name);
    query.addBindValue(patient.gender);
    query.addBindValue(patient.age);
    query.addBindValue(patient.phone);
    query.addBindValue(patient.email);
    query.addBindValue(patient.description);
    query.addBindValue(patient.isActive);
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    query.addBindValue(patient.id);

    return executeQuery(query);
}

bool UserManagementServiceImpl::deletePatient(int patientId)
{
    if (!ensureDatabaseAvailable("deletePatient")) {
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare("DELETE FROM patients WHERE id = ?");
    query.addBindValue(patientId);

    return executeQuery(query);
}

PatientItem UserManagementServiceImpl::getPatient(int patientId)
{
    PatientItem patient;

    if (!ensureDatabaseAvailable("getPatient")) {
        return patient;
    }

    QSqlQuery query(m_database);
    query.prepare("SELECT id, name, gender, age, phone, email, description, is_active, "
                  "created_at, updated_at FROM patients WHERE id = ?");
    query.addBindValue(patientId);

    if (executeQuery(query) && query.next()) {
        patient.id = query.value(0).toInt();
        patient.name = query.value(1).toString();
        patient.gender = query.value(2).toString();
        patient.age = query.value(3).toInt();
        patient.phone = query.value(4).toString();
        patient.email = query.value(5).toString();
        patient.description = query.value(6).toString();
        patient.isActive = query.value(7).toBool();
        patient.createdAt = QDateTime::fromString(query.value(8).toString(), Qt::ISODate);
        patient.updatedAt = QDateTime::fromString(query.value(9).toString(), Qt::ISODate);
    }

    return patient;
}

QList<PatientItem> UserManagementServiceImpl::listPatients(const QString& keyword)
{
    QList<PatientItem> patients;

    if (!ensureDatabaseAvailable("listPatients")) {
        return patients;
    }

    QSqlQuery query(m_database);
    if (keyword.isEmpty()) {
        query.exec("SELECT id, name, gender, age, phone, email, description, is_active, "
                   "created_at, updated_at FROM patients");
    } else {
        query.prepare("SELECT id, name, gender, age, phone, email, description, is_active, "
                      "created_at, updated_at FROM patients "
                      "WHERE name LIKE ? OR phone LIKE ? OR email LIKE ?");
        QString searchPattern = "%" + keyword + "%";
        query.addBindValue(searchPattern);
        query.addBindValue(searchPattern);
        query.addBindValue(searchPattern);
        query.exec();
    }

    while (query.next()) {
        PatientItem patient;
        patient.id = query.value(0).toInt();
        patient.name = query.value(1).toString();
        patient.gender = query.value(2).toString();
        patient.age = query.value(3).toInt();
        patient.phone = query.value(4).toString();
        patient.email = query.value(5).toString();
        patient.description = query.value(6).toString();
        patient.isActive = query.value(7).toBool();
        patient.createdAt = QDateTime::fromString(query.value(8).toString(), Qt::ISODate);
        patient.updatedAt = QDateTime::fromString(query.value(9).toString(), Qt::ISODate);
        patients.append(patient);
    }

    return patients;
}

bool UserManagementServiceImpl::isPatientDuplicate(const QString& name, const QString& phone, int excludeId)
{
    if (!ensureDatabaseAvailable("isPatientDuplicate")) {
        return false;
    }

    QSqlQuery query(m_database);
    if (excludeId >= 0) {
        query.prepare("SELECT COUNT(*) FROM patients WHERE name = ? AND phone = ? AND id != ?");
        query.addBindValue(name);
        query.addBindValue(phone);
        query.addBindValue(excludeId);
    } else {
        query.prepare("SELECT COUNT(*) FROM patients WHERE name = ? AND phone = ?");
        query.addBindValue(name);
        query.addBindValue(phone);
    }

    if (executeQuery(query) && query.next()) {
        return query.value(0).toInt() > 0;
    }

    return false;
}

// ========== 医生管理方法 ==========

bool UserManagementServiceImpl::upsertDoctorProfile(const DoctorProfile& profile)
{
    if (!ensureDatabaseAvailable("upsertDoctorProfile")) {
        return false;
    }

    // 检查医生资料是否存在
    QSqlQuery checkQuery(m_database);
    checkQuery.prepare("SELECT COUNT(*) FROM doctor_profiles WHERE user_id = ?");
    checkQuery.addBindValue(profile.userId);

    if (!executeQuery(checkQuery) || !checkQuery.next()) {
        return false;
    }

    bool exists = checkQuery.value(0).toInt() > 0;

    QSqlQuery query(m_database);
    if (exists) {
        query.prepare("UPDATE doctor_profiles SET gender=?, age=?, hospital=?, updated_at=? "
                      "WHERE user_id=?");
        query.addBindValue(profile.gender);
        query.addBindValue(profile.age);
        query.addBindValue(profile.hospital);
        query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
        query.addBindValue(profile.userId);
    } else {
        query.prepare("INSERT INTO doctor_profiles (user_id, gender, age, hospital, "
                      "created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?)");
        query.addBindValue(profile.userId);
        query.addBindValue(profile.gender);
        query.addBindValue(profile.age);
        query.addBindValue(profile.hospital);
        query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
        query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    }

    return executeQuery(query);
}

DoctorProfile UserManagementServiceImpl::getDoctorProfile(int userId)
{
    DoctorProfile profile;

    if (!ensureDatabaseAvailable("getDoctorProfile")) {
        return profile;
    }

    QSqlQuery query(m_database);
    query.prepare("SELECT user_id, gender, age, hospital, created_at, updated_at "
                  "FROM doctor_profiles WHERE user_id = ?");
    query.addBindValue(userId);

    if (executeQuery(query) && query.next()) {
        profile.userId = query.value(0).toInt();
        profile.gender = query.value(1).toString();
        profile.age = query.value(2).toInt();
        profile.hospital = query.value(3).toString();
        profile.createdAt = QDateTime::fromString(query.value(4).toString(), Qt::ISODate);
        profile.updatedAt = QDateTime::fromString(query.value(5).toString(), Qt::ISODate);
    }

    return profile;
}

// ========== 医生-患者关联方法 ==========

bool UserManagementServiceImpl::linkDoctorPatient(int doctorId, int patientId)
{
    if (!ensureDatabaseAvailable("linkDoctorPatient")) {
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare("INSERT OR IGNORE INTO doctor_patient (doctor_id, patient_id, create_time) "
                  "VALUES (?, ?, ?)");
    query.addBindValue(doctorId);
    query.addBindValue(patientId);
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));

    return executeQuery(query);
}

bool UserManagementServiceImpl::unlinkDoctorPatient(int doctorId, int patientId)
{
    if (!ensureDatabaseAvailable("unlinkDoctorPatient")) {
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare("DELETE FROM doctor_patient WHERE doctor_id = ? AND patient_id = ?");
    query.addBindValue(doctorId);
    query.addBindValue(patientId);

    return executeQuery(query);
}

bool UserManagementServiceImpl::isDoctorPatientLinked(int doctorId, int patientId)
{
    if (!ensureDatabaseAvailable("isDoctorPatientLinked")) {
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare("SELECT COUNT(*) FROM doctor_patient WHERE doctor_id = ? AND patient_id = ?");
    query.addBindValue(doctorId);
    query.addBindValue(patientId);

    if (executeQuery(query) && query.next()) {
        return query.value(0).toInt() > 0;
    }

    return false;
}

QList<PatientItem> UserManagementServiceImpl::listPatientsByDoctor(int doctorId)
{
    QList<PatientItem> patients;

    if (!ensureDatabaseAvailable("listPatientsByDoctor")) {
        return patients;
    }

    QSqlQuery query(m_database);
    query.prepare("SELECT p.id, p.name, p.gender, p.age, p.phone, p.email, p.description, "
                  "p.is_active, p.create_time FROM patients p "
                  "INNER JOIN doctor_patient dp ON p.id = dp.patient_id "
                  "WHERE dp.doctor_id = ?");
    query.addBindValue(doctorId);

    if (executeQuery(query)) {
        while (query.next()) {
            PatientItem patient;
            patient.id = query.value(0).toInt();
            patient.name = query.value(1).toString();
            patient.gender = query.value(2).toString();
            patient.age = query.value(3).toInt();
            patient.phone = query.value(4).toString();
            patient.email = query.value(5).toString();
            patient.description = query.value(6).toString();
            patient.isActive = query.value(7).toBool();
            patient.createdAt = QDateTime::fromString(query.value(8).toString(), Qt::ISODate);
            patients.append(patient);
        }
    }

    return patients;
}

QList<UserInfo> UserManagementServiceImpl::listDoctorsByPatient(int patientId)
{
    QList<UserInfo> doctors;

    if (!ensureDatabaseAvailable("listDoctorsByPatient")) {
        return doctors;
    }

    // 从doctor_patient表获取医生用户ID，然后从users表获取用户信息
    QSqlQuery query(m_database);

    // 【修复】使用单行字符串避免字符串拼接问题
    QString sql = "SELECT u.id, u.username, u.real_name, u.email, u.phone, u.department, "
                  "u.job_title, u.role, u.create_time, u.last_login_time, u.last_update_time, "
                  "u.is_active, u.is_locked, u.notes "
                  "FROM users u "
                  "INNER JOIN doctor_patient dp ON u.id = dp.doctor_id "
                  "WHERE dp.patient_id = ?";

    if (!query.prepare(sql)) {
        m_lastError = "SQL prepare failed: " + query.lastError().text();
        qWarning() << "[UserManagementServiceImpl]" << m_lastError;
        qWarning() << "[UserManagementServiceImpl] SQL:" << sql;
        return doctors;
    }

    query.addBindValue(patientId);

    if (executeQuery(query)) {
        while (query.next()) {
            UserInfo doctor;
            doctor.id = query.value(0).toInt();
            doctor.username = query.value(1).toString();
            doctor.realName = query.value(2).toString();
            doctor.email = query.value(3).toString();
            doctor.phone = query.value(4).toString();
            doctor.department = query.value(5).toString();
            doctor.jobTitle = query.value(6).toString();
            doctor.role = static_cast<UserRole>(query.value(7).toInt());
            doctor.createTime = QDateTime::fromString(query.value(8).toString(), Qt::ISODate);
            doctor.lastLoginTime = QDateTime::fromString(query.value(9).toString(), Qt::ISODate);
            doctor.lastUpdateTime = QDateTime::fromString(query.value(10).toString(), Qt::ISODate);
            doctor.isActive = query.value(11).toBool();
            doctor.isLocked = query.value(12).toBool();
            doctor.notes = query.value(13).toString();
            doctors.append(doctor);
        }
    }

    return doctors;
}

// ========== 手术管理方法（Surgery Item - 手术类型字典） ==========

bool UserManagementServiceImpl::createSurgeryItem(const SurgeryItem& item)
{
    if (!ensureDatabaseAvailable("createSurgeryItem")) {
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare("INSERT INTO surgery_items (name, tags, description, is_active, "
                  "created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?)");
    query.addBindValue(item.name);
    query.addBindValue(item.tags);
    query.addBindValue(item.description);
    query.addBindValue(item.isActive);
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));

    return executeQuery(query);
}

bool UserManagementServiceImpl::updateSurgeryItem(const SurgeryItem& item)
{
    if (!ensureDatabaseAvailable("updateSurgeryItem")) {
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare("UPDATE surgery_items SET name=?, tags=?, description=?, is_active=?, "
                  "updated_at=? WHERE id=?");
    query.addBindValue(item.name);
    query.addBindValue(item.tags);
    query.addBindValue(item.description);
    query.addBindValue(item.isActive);
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    query.addBindValue(item.id);

    return executeQuery(query);
}

bool UserManagementServiceImpl::deleteSurgeryItem(int surgeryItemId)
{
    if (!ensureDatabaseAvailable("deleteSurgeryItem")) {
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare("DELETE FROM surgery_items WHERE id = ?");
    query.addBindValue(surgeryItemId);

    return executeQuery(query);
}

SurgeryItem UserManagementServiceImpl::getSurgeryItem(int surgeryItemId)
{
    SurgeryItem item;

    if (!ensureDatabaseAvailable("getSurgeryItem")) {
        return item;
    }

    QSqlQuery query(m_database);
    query.prepare("SELECT id, name, tags, description, is_active, created_at, updated_at "
                  "FROM surgery_items WHERE id = ?");
    query.addBindValue(surgeryItemId);

    if (executeQuery(query) && query.next()) {
        item.id = query.value(0).toInt();
        item.name = query.value(1).toString();
        item.tags = query.value(2).toString();
        item.description = query.value(3).toString();
        item.isActive = query.value(4).toBool();
        item.createdAt = QDateTime::fromString(query.value(5).toString(), Qt::ISODate);
        item.updatedAt = QDateTime::fromString(query.value(6).toString(), Qt::ISODate);
    }

    return item;
}

QList<SurgeryItem> UserManagementServiceImpl::listSurgeryItems(const QString& keyword)
{
    QList<SurgeryItem> items;

    if (!ensureDatabaseAvailable("listSurgeryItems")) {
        return items;
    }

    QSqlQuery query(m_database);
    if (keyword.isEmpty()) {
        query.exec("SELECT id, name, tags, description, is_active, created_at, updated_at "
                   "FROM surgery_items");
    } else {
        query.prepare("SELECT id, name, tags, description, is_active, created_at, updated_at "
                      "FROM surgery_items WHERE name LIKE ? OR tags LIKE ? OR description LIKE ?");
        QString searchPattern = "%" + keyword + "%";
        query.addBindValue(searchPattern);
        query.addBindValue(searchPattern);
        query.addBindValue(searchPattern);
        query.exec();
    }

    while (query.next()) {
        SurgeryItem item;
        item.id = query.value(0).toInt();
        item.name = query.value(1).toString();
        item.tags = query.value(2).toString();
        item.description = query.value(3).toString();
        item.isActive = query.value(4).toBool();
        item.createdAt = QDateTime::fromString(query.value(5).toString(), Qt::ISODate);
        item.updatedAt = QDateTime::fromString(query.value(6).toString(), Qt::ISODate);
        items.append(item);
    }

    return items;
}

QList<SurgeryItem> UserManagementServiceImpl::listSurgeryItemsByInstrument(int instrumentId)
{
    // 这个方法的语义不太清楚，暂时返回空列表
    // 可能需要一个surgery_item_instrument关联表
    qWarning() << "[UserManagementServiceImpl] listSurgeryItemsByInstrument requires further design";
    return QList<SurgeryItem>();
}

bool UserManagementServiceImpl::isSurgeryNameExists(const QString& surgeryName, int excludeId)
{
    if (!ensureDatabaseAvailable("isSurgeryNameExists")) {
        return false;
    }

    QSqlQuery query(m_database);
    if (excludeId >= 0) {
        query.prepare("SELECT COUNT(*) FROM surgery_items WHERE name = ? AND id != ?");
        query.addBindValue(surgeryName);
        query.addBindValue(excludeId);
    } else {
        query.prepare("SELECT COUNT(*) FROM surgery_items WHERE name = ?");
        query.addBindValue(surgeryName);
    }

    if (executeQuery(query) && query.next()) {
        return query.value(0).toInt() > 0;
    }

    return false;
}

// ========== 手术病例管理方法（Surgery Case - 患者手术记录） ==========

bool UserManagementServiceImpl::createSurgeryCase(const SurgeryCase& surgeryCase)
{
    if (!ensureDatabaseAvailable("createSurgeryCase")) {
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare("INSERT INTO surgery_cases (patient_id, surgery_item_id, doctor_user_id, "
                  "scheduled_time, status, note, created_at, updated_at) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(surgeryCase.patientId);
    query.addBindValue(surgeryCase.surgeryItemId);
    query.addBindValue(surgeryCase.doctorUserId);
    query.addBindValue(surgeryCase.scheduledTime.toString(Qt::ISODate));
    query.addBindValue(surgeryCase.status);
    query.addBindValue(surgeryCase.note);
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));

    return executeQuery(query);
}

bool UserManagementServiceImpl::updateSurgeryCase(const SurgeryCase& surgeryCase)
{
    if (!ensureDatabaseAvailable("updateSurgeryCase")) {
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare("UPDATE surgery_cases SET patient_id=?, surgery_item_id=?, doctor_user_id=?, "
                  "scheduled_time=?, status=?, note=?, updated_at=? WHERE id=?");
    query.addBindValue(surgeryCase.patientId);
    query.addBindValue(surgeryCase.surgeryItemId);
    query.addBindValue(surgeryCase.doctorUserId);
    query.addBindValue(surgeryCase.scheduledTime.toString(Qt::ISODate));
    query.addBindValue(surgeryCase.status);
    query.addBindValue(surgeryCase.note);
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    query.addBindValue(surgeryCase.id);

    return executeQuery(query);
}

bool UserManagementServiceImpl::deleteSurgeryCase(int caseId)
{
    if (!ensureDatabaseAvailable("deleteSurgeryCase")) {
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare("DELETE FROM surgery_cases WHERE id = ?");
    query.addBindValue(caseId);

    return executeQuery(query);
}

QList<SurgeryCase> UserManagementServiceImpl::listSurgeryCasesBySurgeryItem(int surgeryItemId)
{
    QList<SurgeryCase> cases;

    if (!ensureDatabaseAvailable("listSurgeryCasesBySurgeryItem")) {
        return cases;
    }

    QSqlQuery query(m_database);
    query.prepare("SELECT id, patient_id, surgery_item_id, doctor_user_id, scheduled_time, "
                  "status, note, created_at, updated_at FROM surgery_cases "
                  "WHERE surgery_item_id = ?");
    query.addBindValue(surgeryItemId);

    if (executeQuery(query)) {
        while (query.next()) {
            SurgeryCase surgeryCase;
            surgeryCase.id = query.value(0).toInt();
            surgeryCase.patientId = query.value(1).toInt();
            surgeryCase.surgeryItemId = query.value(2).toInt();
            surgeryCase.doctorUserId = query.value(3).toInt();
            surgeryCase.scheduledTime = QDateTime::fromString(query.value(4).toString(), Qt::ISODate);
            surgeryCase.status = query.value(5).toString();
            surgeryCase.note = query.value(6).toString();
            surgeryCase.createdAt = QDateTime::fromString(query.value(7).toString(), Qt::ISODate);
            surgeryCase.updatedAt = QDateTime::fromString(query.value(8).toString(), Qt::ISODate);
            cases.append(surgeryCase);
        }
    }

    return cases;
}

QList<SurgeryCase> UserManagementServiceImpl::listSurgeryCasesByDoctor(int doctorUserId)
{
    QList<SurgeryCase> cases;

    if (!ensureDatabaseAvailable("listSurgeryCasesByDoctor")) {
        return cases;
    }

    QSqlQuery query(m_database);
    query.prepare("SELECT id, patient_id, surgery_item_id, doctor_user_id, scheduled_time, "
                  "status, note, created_at, updated_at FROM surgery_cases "
                  "WHERE doctor_user_id = ?");
    query.addBindValue(doctorUserId);

    if (executeQuery(query)) {
        while (query.next()) {
            SurgeryCase surgeryCase;
            surgeryCase.id = query.value(0).toInt();
            surgeryCase.patientId = query.value(1).toInt();
            surgeryCase.surgeryItemId = query.value(2).toInt();
            surgeryCase.doctorUserId = query.value(3).toInt();
            surgeryCase.scheduledTime = QDateTime::fromString(query.value(4).toString(), Qt::ISODate);
            surgeryCase.status = query.value(5).toString();
            surgeryCase.note = query.value(6).toString();
            surgeryCase.createdAt = QDateTime::fromString(query.value(7).toString(), Qt::ISODate);
            surgeryCase.updatedAt = QDateTime::fromString(query.value(8).toString(), Qt::ISODate);
            cases.append(surgeryCase);
        }
    }

    return cases;
}

QList<SurgeryCase> UserManagementServiceImpl::listSurgeryCasesByPatient(int patientId)
{
    QList<SurgeryCase> cases;

    if (!ensureDatabaseAvailable("listSurgeryCasesByPatient")) {
        return cases;
    }

    QSqlQuery query(m_database);
    query.prepare("SELECT id, patient_id, surgery_item_id, doctor_user_id, scheduled_time, "
                  "status, note, created_at, updated_at FROM surgery_cases "
                  "WHERE patient_id = ?");
    query.addBindValue(patientId);

    if (executeQuery(query)) {
        while (query.next()) {
            SurgeryCase surgeryCase;
            surgeryCase.id = query.value(0).toInt();
            surgeryCase.patientId = query.value(1).toInt();
            surgeryCase.surgeryItemId = query.value(2).toInt();
            surgeryCase.doctorUserId = query.value(3).toInt();
            surgeryCase.scheduledTime = QDateTime::fromString(query.value(4).toString(), Qt::ISODate);
            surgeryCase.status = query.value(5).toString();
            surgeryCase.note = query.value(6).toString();
            surgeryCase.createdAt = QDateTime::fromString(query.value(7).toString(), Qt::ISODate);
            surgeryCase.updatedAt = QDateTime::fromString(query.value(8).toString(), Qt::ISODate);
            cases.append(surgeryCase);
        }
    }

    return cases;
}

// ========== 器械管理方法（Instrument） ==========
// 注意：器械管理由InstrumentManagement插件负责，这里提供基本的占位实现

bool UserManagementServiceImpl::createInstrument(const InstrumentItem& instrument)
{
    qWarning() << "[UserManagementServiceImpl] createInstrument should be implemented by InstrumentManagement plugin";
    return false;
}

bool UserManagementServiceImpl::updateInstrument(const InstrumentItem& instrument)
{
    qWarning() << "[UserManagementServiceImpl] updateInstrument should be implemented by InstrumentManagement plugin";
    return false;
}

bool UserManagementServiceImpl::deleteInstrument(int instrumentId)
{
    qWarning() << "[UserManagementServiceImpl] deleteInstrument should be implemented by InstrumentManagement plugin";
    return false;
}

InstrumentItem UserManagementServiceImpl::getInstrument(int instrumentId)
{
    qWarning() << "[UserManagementServiceImpl] getInstrument should be implemented by InstrumentManagement plugin";
    return InstrumentItem();
}

QList<InstrumentItem> UserManagementServiceImpl::listInstruments(const QString& keyword)
{
    qWarning() << "[UserManagementServiceImpl] listInstruments should be implemented by InstrumentManagement plugin";
    Q_UNUSED(keyword);
    return QList<InstrumentItem>();
}

QList<InstrumentItem> UserManagementServiceImpl::listInstrumentsBySurgeryItem(int surgeryId)
{
    QList<InstrumentItem> instruments;

    if (!ensureDatabaseAvailable("listInstrumentsBySurgeryItem")) {
        return instruments;
    }

    // 这里只返回关联关系，具体器械信息需要从InstrumentManagement插件获取
    QSqlQuery query(m_database);
    query.prepare("SELECT instrument_id FROM surgery_instrument WHERE surgery_id = ?");
    query.addBindValue(surgeryId);

    if (executeQuery(query)) {
        while (query.next()) {
            InstrumentItem info;
            info.id = query.value(0).toInt();
            instruments.append(info);
        }
    }

    return instruments;
}

// ========== 手术-器械关联方法 ==========

bool UserManagementServiceImpl::linkSurgeryInstrument(int surgeryId, int instrumentId)
{
    if (!ensureDatabaseAvailable("linkSurgeryInstrument")) {
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare("INSERT OR IGNORE INTO surgery_instrument (surgery_id, instrument_id, create_time) "
                  "VALUES (?, ?, ?)");
    query.addBindValue(surgeryId);
    query.addBindValue(instrumentId);
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));

    return executeQuery(query);
}

bool UserManagementServiceImpl::unlinkSurgeryInstrument(int surgeryId, int instrumentId)
{
    if (!ensureDatabaseAvailable("unlinkSurgeryInstrument")) {
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare("DELETE FROM surgery_instrument WHERE surgery_id = ? AND instrument_id = ?");
    query.addBindValue(surgeryId);
    query.addBindValue(instrumentId);

    return executeQuery(query);
}

// ========== 数据库初始化方法 ==========

bool UserManagementServiceImpl::initializeDatabase()
{
    qDebug() << "[UserManagementServiceImpl] Initializing database";

    // 确保数据库已初始化
    return ensureDatabaseInitialized();
}


