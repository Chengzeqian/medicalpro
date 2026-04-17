#ifndef USER_MANAGEMENT_SERVICE_H
#define USER_MANAGEMENT_SERVICE_H

#include "UserDataStructures.h"
#include <QObject>

class QWidget;

/**
 * @brief 用户管理服务接口
 * 
 * 提供用户账户管理的完整服务接口，包括用户注册、登录、权限管理等功能。
 * 这是一个纯虚接口，遵循CTK服务架构设计原则。
 */
class UserManagementService : public QObject
{
    Q_OBJECT
    
public:
    explicit UserManagementService(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~UserManagementService() = default;
    
    // ========== 用户认证管理 ==========
    
    /**
     * @brief 用户登录
     * @param username 用户名
     * @param password 密码
     * @return 登录成功返回用户信息，失败返回无效的UserInfo
     */
    virtual UserInfo loginUser(const QString& username, const QString& password) = 0;
    
    /**
     * @brief 用户登出
     * @param userId 用户ID
     * @return 成功返回true，失败返回false
     */
    virtual bool logoutUser(int userId) = 0;
    
    /**
     * @brief 检查用户是否已登录
     * @param userId 用户ID
     * @return 已登录返回true，未登录返回false
     */
    virtual bool isUserLoggedIn(int userId) = 0;
    
    /**
     * @brief 获取当前登录用户
     * @return 当前登录用户信息，未登录返回无效的UserInfo
     */
    virtual UserInfo getCurrentUser() = 0;
    
    // ========== 用户账户管理 ==========
    
    /**
     * @brief 注册新用户
     * @param user 用户信息
     * @return 成功返回true，失败返回false
     */
    virtual bool registerUser(const UserInfo& user) = 0;
    
    /**
     * @brief 更新用户信息
     * @param user 用户信息（包含ID）
     * @return 成功返回true，失败返回false
     */
    virtual bool updateUser(const UserInfo& user) = 0;
    
    /**
     * @brief 删除用户
     * @param userId 用户ID
     * @return 成功返回true，失败返回false
     */
    virtual bool deleteUser(int userId) = 0;
    
    /**
     * @brief 根据ID获取用户信息
     * @param userId 用户ID
     * @return 用户信息，如果不存在则返回无效的UserInfo
     */
    virtual UserInfo getUser(int userId) = 0;
    
    /**
     * @brief 根据用户ID获取用户信息（别名方法）
     * @param userId 用户ID
     * @return 用户信息，如果不存在则返回无效的UserInfo
     */
    virtual UserInfo getUserById(int userId) = 0;
    
    /**
     * @brief 根据用户名获取用户信息
     * @param username 用户名
     * @return 用户信息，如果不存在则返回无效的UserInfo
     */
    virtual UserInfo getUserByUsername(const QString& username) = 0;
    
    /**
     * @brief 获取所有用户列表
     * @return 所有用户信息
     */
    virtual QList<UserInfo> getAllUsers() = 0;
    
    /**
     * @brief 根据条件搜索用户
     * @param criteria 搜索条件
     * @return 符合条件的用户列表
     */
    virtual QList<UserInfo> searchUsers(const UserSearchCriteria& criteria) = 0;

    // ========== 医生规范化资料 ==========
    virtual bool upsertDoctorProfile(const DoctorProfile& profile) = 0;
    virtual DoctorProfile getDoctorProfile(int userId) = 0;

    // ========== 手术项管理 ==========
    virtual bool createSurgeryItem(const SurgeryItem& item) = 0;
    virtual bool updateSurgeryItem(const SurgeryItem& item) = 0;
    virtual bool deleteSurgeryItem(int id) = 0;
    virtual SurgeryItem getSurgeryItem(int id) = 0;
    virtual QList<SurgeryItem> listSurgeryItems(const QString& keyword = QString()) = 0;
    virtual bool isSurgeryNameExists(const QString& name, int excludeId = -1) = 0;

    // ========== 病人管理 ==========
    virtual bool createPatient(const PatientItem& p) = 0;
    virtual bool updatePatient(const PatientItem& p) = 0;
    virtual bool deletePatient(int id) = 0;
    virtual PatientItem getPatient(int id) = 0;
    virtual QList<PatientItem> listPatients(const QString& keyword = QString()) = 0;
    virtual bool isPatientDuplicate(const QString& name, const QString& phone, int excludeId = -1) = 0;

    // ========== 关联：手术病例 ==========
    virtual bool createSurgeryCase(const SurgeryCase& c) = 0;
    virtual bool updateSurgeryCase(const SurgeryCase& c) = 0;
    virtual bool deleteSurgeryCase(int id) = 0;
    virtual QList<SurgeryCase> listSurgeryCasesByPatient(int patientId) = 0;
    virtual QList<SurgeryCase> listSurgeryCasesByDoctor(int doctorUserId) = 0;
    virtual QList<SurgeryCase> listSurgeryCasesBySurgeryItem(int surgeryItemId) = 0;

    // ========== 关联：医生 ↔ 病人 ==========
    /** 建立医生-病人关联（若已存在则忽略） */
    virtual bool linkDoctorPatient(int doctorUserId, int patientId) = 0;
    /** 解除医生-病人关联 */
    virtual bool unlinkDoctorPatient(int doctorUserId, int patientId) = 0;
    /** 查询某医生关联的所有病人 */
    virtual QList<PatientItem> listPatientsByDoctor(int doctorUserId) = 0;
    /** 查询某病人关联的所有医生 */
    virtual QList<UserInfo> listDoctorsByPatient(int patientId) = 0;
    /** 判断是否已关联 */
    virtual bool isDoctorPatientLinked(int doctorUserId, int patientId) = 0;

    // ========== 器械管理 ==========
    virtual bool createInstrument(const InstrumentItem& ins) = 0;
    virtual bool updateInstrument(const InstrumentItem& ins) = 0;
    virtual bool deleteInstrument(int id) = 0;
    virtual QList<InstrumentItem> listInstruments(const QString& keyword = QString()) = 0;
    virtual InstrumentItem getInstrument(int id) = 0;

    // ========== 关联：手术项 ↔ 器械 ==========
    virtual bool linkSurgeryInstrument(int surgeryItemId, int instrumentId) = 0;
    virtual bool unlinkSurgeryInstrument(int surgeryItemId, int instrumentId) = 0;
    virtual QList<InstrumentItem> listInstrumentsBySurgeryItem(int surgeryItemId) = 0;
    virtual QList<SurgeryItem> listSurgeryItemsByInstrument(int instrumentId) = 0;
    
    // ========== 密码管理 ==========
    
    /**
     * @brief 修改密码
     * @param userId 用户ID
     * @param oldPassword 旧密码
     * @param newPassword 新密码
     * @return 成功返回true，失败返回false
     */
    virtual bool changePassword(int userId, const QString& oldPassword, const QString& newPassword) = 0;
    
    /**
     * @brief 重置密码（管理员功能）
     * @param userId 用户ID
     * @param newPassword 新密码
     * @return 成功返回true，失败返回false
     */
    virtual bool resetPassword(int userId, const QString& newPassword) = 0;
    
    /**
     * @brief 验证密码强度
     * @param password 密码
     * @return 密码强度评级（0-4，4为最强）
     */
    virtual int validatePasswordStrength(const QString& password) = 0;
    
    // ========== 权限管理 ==========
    
    /**
     * @brief 检查用户权限
     * @param userId 用户ID
     * @param permission 权限名称
     * @return 有权限返回true，无权限返回false
     */
    virtual bool checkUserPermission(int userId, const QString& permission) = 0;
    
    /**
     * @brief 获取用户角色
     * @param userId 用户ID
     * @return 用户角色
     */
    virtual UserRole getUserRole(int userId) = 0;
    
    /**
     * @brief 设置用户角色
     * @param userId 用户ID
     * @param role 新角色
     * @return 成功返回true，失败返回false
     */
    virtual bool setUserRole(int userId, UserRole role) = 0;
    
    // ========== 数据库管理 ==========
    
    /**
     * @brief 初始化用户数据库
     * @return 成功返回true，失败返回false
     */
    virtual bool initializeDatabase() = 0;
    
    /**
     * @brief 获取数据库状态信息
     * @return 数据库状态描述
     */
    virtual QString getDatabaseStatus() = 0;
    
    /**
     * @brief 获取用户总数
     * @return 用户总数
     */
    virtual int getUserCount() = 0;
    
    /**
     * @brief 备份用户数据库
     * @param backupPath 备份文件路径
     * @return 成功返回true，失败返回false
     */
    virtual bool backupDatabase(const QString& backupPath) = 0;
    
    // ========== UI显示管理 ==========
    
    /**
     * @brief 显示登录界面
     * @param parent 父窗口（可选）
     * @return 成功返回true，失败返回false
     */
    virtual bool showLoginDialog(QWidget* parent = nullptr) = 0;
    
    /**
     * @brief 显示用户注册界面
     * @param parent 父窗口（可选）
     * @return 成功返回true，失败返回false
     */
    virtual bool showRegisterDialog(QWidget* parent = nullptr) = 0;
    
    /**
     * @brief 显示用户管理界面
     * @param parent 父窗口（可选）
     * @return 成功返回true，失败返回false
     */
    virtual bool showUserManagementDialog(QWidget* parent = nullptr) = 0;

signals:
    /**
     * @brief 用户登录成功信号
     * @param user 登录用户信息
     */
    void userLoggedIn(const UserInfo& user);
    
    /**
     * @brief 用户登出信号
     * @param userId 登出用户ID
     */
    void userLoggedOut(int userId);
    
    /**
     * @brief 用户注册成功信号
     * @param user 新注册的用户信息
     */
    void userRegistered(const UserInfo& user);
    
    /**
     * @brief 用户更新成功信号
     * @param user 更新后的用户信息
     */
    void userUpdated(const UserInfo& user);
    
    /**
     * @brief 用户删除成功信号
     * @param userId 被删除的用户ID
     */
    void userDeleted(int userId);
    
    /**
     * @brief 密码修改成功信号
     * @param userId 修改密码的用户ID
     */
    void passwordChanged(int userId);
    
    /**
     * @brief 用户权限变更信号
     * @param userId 用户ID
     * @param newRole 新角色
     */
    void userRoleChanged(int userId, UserRole newRole);
    
    /**
     * @brief 数据库错误信号
     * @param error 错误描述
     */
    void databaseError(const QString& error);
    
    /**
     * @brief 认证失败信号
     * @param username 用户名
     * @param reason 失败原因
     */
    void authenticationFailed(const QString& username, const QString& reason);
};

// 声明为CTK服务接口
Q_DECLARE_INTERFACE(UserManagementService, "medical.UserManagementService")

#endif // USER_MANAGEMENT_SERVICE_H
