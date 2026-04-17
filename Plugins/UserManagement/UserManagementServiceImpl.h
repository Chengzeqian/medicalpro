#ifndef USER_MANAGEMENT_SERVICE_IMPL_H
#define USER_MANAGEMENT_SERVICE_IMPL_H

#include "UserManagementService.h"
#include "UserDataStructures.h"
#include <QObject>
#include <QTimer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMap>
#include <QMutex>

class ctkEventAdmin;

/**
 * @brief 用户管理服务实现类
 * 
 * 实现UserManagementService接口，提供完整的用户管理功能，
 * 包括用户认证、账户管理、权限控制等。
 */
class UserManagementServiceImpl : public UserManagementService
{
    Q_OBJECT
    Q_INTERFACES(UserManagementService)

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit UserManagementServiceImpl(QObject* parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~UserManagementServiceImpl() override;

    // ========== 用户认证管理 ==========
    
    UserInfo loginUser(const QString& username, const QString& password) override;
    bool logoutUser(int userId) override;
    bool isUserLoggedIn(int userId) override;
    UserInfo getCurrentUser() override;

    // ========== 用户账户管理 ==========
    
    bool registerUser(const UserInfo& user) override;
    bool updateUser(const UserInfo& user) override;
    bool deleteUser(int userId) override;
    UserInfo getUser(int userId) override;
    UserInfo getUserById(int userId) override;
    UserInfo getUserByUsername(const QString& username) override;
    QList<UserInfo> getAllUsers() override;
    QList<UserInfo> searchUsers(const UserSearchCriteria& criteria) override;

    // ========== 医生规范化资料 ==========
    bool upsertDoctorProfile(const DoctorProfile& profile) override;
    DoctorProfile getDoctorProfile(int userId) override;

    // ========== 手术项管理 ==========
    bool createSurgeryItem(const SurgeryItem& item) override;
    bool updateSurgeryItem(const SurgeryItem& item) override;
    bool deleteSurgeryItem(int id) override;
    SurgeryItem getSurgeryItem(int id) override;
    QList<SurgeryItem> listSurgeryItems(const QString& keyword = QString()) override;
    bool isSurgeryNameExists(const QString& name, int excludeId = -1) override;

    // ========== 病人管理 ==========
    bool createPatient(const PatientItem& p) override;
    bool updatePatient(const PatientItem& p) override;
    bool deletePatient(int id) override;
    PatientItem getPatient(int id) override;
    virtual QList<PatientItem> listPatients(const QString& keyword = QString()) override;
    virtual bool isPatientDuplicate(const QString& name, const QString& phone, int excludeId = -1) override;

    /**
     * @brief 返回最近一次操作的错误信息
     */
    QString getLastError() const;

    // ========== 手术病例 ==========
    bool createSurgeryCase(const SurgeryCase& c) override;
    bool updateSurgeryCase(const SurgeryCase& c) override;
    bool deleteSurgeryCase(int id) override;
    QList<SurgeryCase> listSurgeryCasesByPatient(int patientId) override;
    QList<SurgeryCase> listSurgeryCasesByDoctor(int doctorUserId) override;
    QList<SurgeryCase> listSurgeryCasesBySurgeryItem(int surgeryItemId) override;

    // 医生 ↔ 病人
    bool linkDoctorPatient(int doctorUserId, int patientId) override;
    bool unlinkDoctorPatient(int doctorUserId, int patientId) override;
    QList<PatientItem> listPatientsByDoctor(int doctorUserId) override;
    QList<UserInfo> listDoctorsByPatient(int patientId) override;
    bool isDoctorPatientLinked(int doctorUserId, int patientId) override;

    // 器械管理
    bool createInstrument(const InstrumentItem& ins) override;
    bool updateInstrument(const InstrumentItem& ins) override;
    bool deleteInstrument(int id) override;
    QList<InstrumentItem> listInstruments(const QString& keyword = QString()) override;
    InstrumentItem getInstrument(int id) override;

    // 手术项 ↔ 器械
    bool linkSurgeryInstrument(int surgeryItemId, int instrumentId) override;
    bool unlinkSurgeryInstrument(int surgeryItemId, int instrumentId) override;
    QList<InstrumentItem> listInstrumentsBySurgeryItem(int surgeryItemId) override;
    QList<SurgeryItem> listSurgeryItemsByInstrument(int instrumentId) override;

    // ========== 密码管理 ==========
    
    bool changePassword(int userId, const QString& oldPassword, const QString& newPassword) override;
    bool resetPassword(int userId, const QString& newPassword) override;
    int validatePasswordStrength(const QString& password) override;

    // ========== 权限管理 ==========
    
    bool checkUserPermission(int userId, const QString& permission) override;
    UserRole getUserRole(int userId) override;
    bool setUserRole(int userId, UserRole role) override;

    // ========== 数据库管理 ==========
    
    bool initializeDatabase() override;
    QString getDatabaseStatus() override;
    int getUserCount() override;
    bool backupDatabase(const QString& backupPath) override;

    bool isOfflineModeEnabled() const { return m_offlineMode; }

    // ========== UI显示管理 ==========
    
    bool showLoginDialog(QWidget* parent = nullptr) override;
    bool showRegisterDialog(QWidget* parent = nullptr) override;
    bool showUserManagementDialog(QWidget* parent = nullptr) override;

public slots:
    /**
     * @brief 会话超时检查
     */
    void checkSessionTimeout();
    
    /**
     * @brief 清理过期会话
     */
    void cleanupExpiredSessions();

private slots:
    /**
     * @brief 处理数据库错误
     * @param query 出错的查询
     */
    void handleDatabaseError(const QSqlQuery& query);

private:
    bool ensureDatabaseInitialized();
    bool handleDatabaseInitializationFailure(const QString& errorMessage);
    void logDatabaseStatus(const QString& status);
    bool ensureDatabaseAvailable(const char* operationContext);

    // ========== 数据库操作 ==========
    
    /**
     * @brief 初始化数据库连接
     * @return 成功返回true
     */
    bool initializeDatabaseConnection();
    
    /**
     * @brief 创建数据库表
     * @return 成功返回true
     */
    bool createDatabaseTables();
    bool createDoctorTables();
    bool createSurgeryTables();
    bool createPatientsTables();
    bool createSurgeryCaseTables();
    bool createDoctorPatientTables();
    bool createInstrumentTables();
    bool createSurgeryInstrumentTables();
    
    /**
     * @brief 初始化测试数据
     */
    void initializeTestData();
    
    /**
     * @brief 执行SQL查询
     * @param query SQL查询对象
     * @return 成功返回true
     */
    bool executeQuery(QSqlQuery& query);
    
    /**
     * @brief 检查数据库连接状态
     * @return 连接正常返回true
     */
    bool checkDatabaseConnection();

    // ========== 用户认证 ==========
    
    /**
     * @brief 验证用户凭据
     * @param username 用户名
     * @param password 密码
     * @return 验证成功返回用户信息
     */
    UserInfo authenticateUser(const QString& username, const QString& password);
    
    /**
     * @brief 创建用户会话
     * @param user 用户信息
     * @return 会话ID
     */
    QString createUserSession(const UserInfo& user);
    
    /**
     * @brief 销毁用户会话
     * @param userId 用户ID
     * @return 成功返回true
     */
    bool destroyUserSession(int userId);
    
    /**
     * @brief 更新会话活动时间
     * @param userId 用户ID
     */
    void updateSessionActivity(int userId);

    // ========== 密码处理 ==========
    
    /**
     * @brief 生成密码哈希
     * @param password 原始密码
     * @param salt 盐值
     * @return 哈希后的密码
     */
    QString hashPassword(const QString& password, const QString& salt);
    
    /**
     * @brief 生成盐值
     * @return 随机盐值
     */
    QString generateSalt();
    
    /**
     * @brief 验证密码
     * @param password 原始密码
     * @param hash 存储的哈希值
     * @param salt 盐值
     * @return 密码正确返回true
     */
    bool verifyPassword(const QString& password, const QString& hash, const QString& salt);

    // ========== 权限管理 ==========
    
    /**
     * @brief 获取角色权限列表
     * @param role 用户角色
     * @return 权限列表
     */
    QStringList getRolePermissions(UserRole role);
    
    /**
     * @brief 检查操作权限
     * @param currentUserRole 当前用户角色
     * @param operation 操作类型
     * @param targetUserRole 目标用户角色（如果适用）
     * @return 有权限返回true
     */
    bool checkOperationPermission(UserRole currentUserRole, const QString& operation, 
                                 UserRole targetUserRole = UserRole::Guest);

    // ========== 日志记录 ==========
    
    /**
     * @brief 记录操作日志
     * @param userId 用户ID
     * @param operation 操作类型
     * @param description 操作描述
     * @param target 操作目标
     * @param success 操作是否成功
     * @param errorMessage 错误信息
     */
    void logOperation(int userId, const QString& operation, const QString& description,
                     const QString& target = QString(), bool success = true,
                     const QString& errorMessage = QString());
    
    /**
     * @brief 记录安全事件
     * @param level 事件级别
     * @param message 事件消息
     * @param userId 相关用户ID
     */
    void logSecurityEvent(const QString& level, const QString& message, int userId = -1);

    // ========== 数据验证 ==========
    
    /**
     * @brief 验证用户信息
     * @param user 用户信息
     * @param isUpdate 是否为更新操作
     * @return 验证结果和错误信息
     */
    QPair<bool, QString> validateUserInfo(const UserInfo& user, bool isUpdate = false);
    
    /**
     * @brief 检查用户名是否已存在
     * @param username 用户名
     * @param excludeUserId 排除的用户ID（更新时使用）
     * @return 已存在返回true
     */
    bool isUsernameExists(const QString& username, int excludeUserId = -1);
    
    /**
     * @brief 检查邮箱是否已存在
     * @param email 邮箱
     * @param excludeUserId 排除的用户ID（更新时使用）
     * @return 已存在返回true
     */
    bool isEmailExists(const QString& email, int excludeUserId = -1);

    // ========== CTK事件发送 ==========
    
    /**
     * @brief 发送CTK事件
     * @param topic 事件主题
     * @param properties 事件属性
     */
    void sendCTKEvent(const QString& topic, const QVariantMap& properties);
    
    /**
     * @brief 初始化角色权限映射
     */
    void initializeRolePermissions();
    
    /**
     * @brief 创建默认管理员用户
     */
    void createDefaultAdminUser();
    
    /**
     * @brief 格式化用户角色为显示字符串
     * @param role 用户角色
     * @return 角色显示字符串
     */
    QString formatUserRole(UserRole role);

private:
    // 数据库相关
    QSqlDatabase m_database;                           // 数据库连接
    QString m_databasePath;                           // 数据库文件路径
    bool m_databaseInitialized;                       // 数据库初始化状态
    bool m_offlineMode;                               // 离线模式标志
    mutable QMutex m_databaseMutex;                   // 数据库初始化互斥锁
    bool m_databasePromptActive;                      // 是否正在显示数据库错误提示
    
    // 用户会话管理
    QMap<int, UserSession> m_activeSessions;          // 活跃会话列表
    UserInfo m_currentUser;                           // 当前登录用户
    QTimer* m_sessionTimer;                           // 会话检查定时器
    QMutex m_sessionMutex;                            // 会话操作互斥锁
    
    // 密码策略
    PasswordPolicy m_passwordPolicy;                  // 密码策略配置
    
    // 登录尝试控制
    QMap<QString, int> m_loginAttempts;               // 登录尝试次数
    QMap<QString, QDateTime> m_lockoutEndTime;        // 锁定结束时间
    
    // 权限映射
    QMap<UserRole, QStringList> m_rolePermissions;    // 角色权限映射
    
    // CTK事件服务
    ctkEventAdmin* m_eventAdmin;                      // CTK事件管理器
    
    // 服务状态
    bool m_serviceInitialized;                        // 服务初始化状态
    QString m_lastError;                              // 最后的错误信息
    
    // 统计信息
    int m_totalLoginAttempts;                         // 总登录尝试次数
    int m_successfulLogins;                           // 成功登录次数
    int m_failedLogins;                               // 失败登录次数
};

#endif // USER_MANAGEMENT_SERVICE_IMPL_H
