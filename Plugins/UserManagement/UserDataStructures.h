#ifndef USER_DATA_STRUCTURES_H
#define USER_DATA_STRUCTURES_H

#include <QString>
#include <QDateTime>
#include <QList>
#include <QMetaType>

// 引入核心框架的共享数据结构（包括InstrumentItem等）
#include "../../Framework/Core/MedicalDataStructures.h"

/**
 * @brief 用户角色枚举
 */
enum class UserRole {
    Guest = 0,          // 访客（最低权限）
    Doctor = 1,         // 医生（常规操作权限）
    Administrator = 2,  // 管理员（最高权限）
    Operator = 3        // 操作员（部分权限）
};

/**
 * @brief 用户信息结构体
 */
struct UserInfo {
    int id;                         // 用户ID（数据库主键）
    QString username;               // 用户名（唯一）
    QString password;               // 密码（加密存储）
    QString realName;               // 真实姓名
    QString email;                  // 邮箱地址
    QString phone;                  // 手机号码
    QString department;             // 科室/部门
    QString jobTitle;               // 职位/职称
    UserRole role;                  // 用户角色
    QDateTime createTime;           // 创建时间
    QDateTime lastLoginTime;        // 最后登录时间
    QDateTime lastUpdateTime;       // 最后更新时间
    bool isActive;                  // 账户是否激活
    bool isLocked;                  // 账户是否被锁定
    QString notes;                  // 备注信息
    
    // 默认构造函数
    UserInfo() : id(-1), role(UserRole::Guest), isActive(true), isLocked(false) {}
    
    // 检查用户信息是否有效
    bool isValid() const {
        return id >= 0 && !username.isEmpty() && !realName.isEmpty();
    }
    
    // 获取角色字符串
    QString getRoleString() const {
        switch (role) {
            case UserRole::Guest: return "访客";
            case UserRole::Doctor: return "医生";
            case UserRole::Administrator: return "管理员";
            case UserRole::Operator: return "操作员";
            default: return "未知";
        }
    }
    
    // 检查是否有管理员权限
    bool isAdmin() const {
        return role == UserRole::Administrator;
    }
    
    // 检查是否有医生权限
    bool isDoctor() const {
        return role == UserRole::Doctor || role == UserRole::Administrator;
    }
};

/**
 * @brief 用户搜索条件结构体
 */
struct UserSearchCriteria {
    QString username;               // 用户名（模糊搜索）
    QString realName;               // 真实姓名（模糊搜索）
    QString email;                  // 邮箱（模糊搜索）
    QString phone;                  // 手机号（模糊搜索）
    QString department;             // 科室/部门（模糊搜索）
    QString jobTitle;               // 职位/职称（模糊搜索）
    UserRole role;                  // 用户角色（精确匹配）
    bool searchByRole;              // 是否按角色搜索
    bool activeOnly;                // 仅搜索激活用户
    bool unlockedOnly;              // 仅搜索未锁定用户
    QDateTime createTimeFrom;       // 创建时间起始
    QDateTime createTimeTo;         // 创建时间结束
    QDateTime lastLoginFrom;        // 最后登录时间起始
    QDateTime lastLoginTo;          // 最后登录时间结束
    
    // 默认构造函数
    UserSearchCriteria() : role(UserRole::Guest), searchByRole(false), 
                          activeOnly(false), unlockedOnly(false) {}
    
    // 检查是否有搜索条件
    bool hasSearchCriteria() const {
        return !username.isEmpty() || !realName.isEmpty() || !email.isEmpty() ||
               !phone.isEmpty() || !department.isEmpty() || !jobTitle.isEmpty() ||
               searchByRole || activeOnly || unlockedOnly ||
               createTimeFrom.isValid() || createTimeTo.isValid() ||
               lastLoginFrom.isValid() || lastLoginTo.isValid();
    }
};

/**
 * @brief 用户登录会话信息
 */
struct UserSession {
    int userId;                     // 用户ID
    QString sessionId;              // 会话ID
    QDateTime loginTime;            // 登录时间
    QDateTime lastActivityTime;     // 最后活动时间
    QString ipAddress;              // 登录IP地址
    QString userAgent;              // 用户代理信息
    bool isActive;                  // 会话是否活跃
    
    // 默认构造函数
    UserSession() : userId(-1), isActive(false) {}
    
    // 检查会话是否有效
    bool isValid() const {
        return userId >= 0 && !sessionId.isEmpty() && isActive;
    }
    
    // 检查会话是否过期（30分钟无活动）
    bool isExpired() const {
        return lastActivityTime.isValid() && 
               lastActivityTime.secsTo(QDateTime::currentDateTime()) > 1800;
    }
};

/**
 * @brief 用户操作日志结构体
 */
struct UserOperationLog {
    int id;                         // 日志ID
    int userId;                     // 操作用户ID
    QString username;               // 操作用户名
    QString operation;              // 操作类型
    QString description;            // 操作描述
    QString target;                 // 操作目标
    QDateTime operationTime;        // 操作时间
    QString ipAddress;              // 操作IP地址
    bool success;                   // 操作是否成功
    QString errorMessage;           // 错误信息（如果失败）
    
    // 默认构造函数
    UserOperationLog() : id(-1), userId(-1), success(true) {}
    
    // 检查日志是否有效
    bool isValid() const {
        return id >= 0 && userId >= 0 && !operation.isEmpty();
    }
};

/**
 * @brief 医生扩展资料（规范化）
 */
struct DoctorProfile {
    int userId;                 // 关联的用户ID（主键=外键）
    QString gender;             // 性别：男/女
    int age;                    // 年龄
    QString hospital;           // 医院
    QDateTime createdAt;        // 创建时间
    QDateTime updatedAt;        // 更新时间
    
    DoctorProfile() : userId(-1), age(0) {}
    bool isValid() const { return userId >= 0; }
};

/**
 * @brief 手术项（规范化）
 */
struct SurgeryItem {
    int id;                // 主键
    QString name;          // 手术名称（唯一）
    QString tags;          // 标签（逗号分隔，可选）
    QString description;   // 描述
    bool isActive;         // 是否启用
    QDateTime createdAt;
    QDateTime updatedAt;

    SurgeryItem() : id(-1), isActive(true) {}
    bool isValid() const { return id >= 0 && !name.isEmpty(); }
};

/**
 * @brief 病人（规范化）
 */
struct PatientItem {
    int id;               // 主键
    QString name;         // 姓名
    QString gender;       // 性别
    int age;              // 年龄
    QString phone;        // 手机
    QString email;        // 邮箱
    QString description;  // 描述
    bool isActive;        // 是否有效
    QDateTime createdAt;
    QDateTime updatedAt;

    PatientItem() : id(-1), age(0), isActive(true) {}
    bool isValid() const { return id >= 0 && !name.isEmpty(); }
};

/**
 * @brief 手术病例（病人与手术以及主刀医生的关联）
 */
struct SurgeryCase {
    int id;                 // 主键
    int patientId;          // 关联病人ID
    int surgeryItemId;      // 关联手术项ID
    int doctorUserId;       // 主刀医生用户ID
    QDateTime scheduledTime;// 计划时间
    QString status;         // 状态：计划/进行中/完成/取消
    QString note;           // 备注
    QDateTime createdAt;
    QDateTime updatedAt;

    SurgeryCase()
        : id(-1), patientId(-1), surgeryItemId(-1), doctorUserId(-1) {}
    bool isValid() const { return patientId >= 0 && surgeryItemId >= 0; }
};

// 注意：InstrumentItem 已在 Framework/Core/MedicalDataStructures.h 中定义
// 通过上面的 #include 已经引入，这里不需要前向声明或重复定义

/**
 * @brief 密码策略配置
 */
struct PasswordPolicy {
    int minLength;                  // 最小长度
    int maxLength;                  // 最大长度
    bool requireUppercase;          // 要求大写字母
    bool requireLowercase;          // 要求小写字母
    bool requireDigits;             // 要求数字
    bool requireSpecialChars;       // 要求特殊字符
    QString specialChars;           // 允许的特殊字符
    int maxConsecutiveChars;        // 最大连续相同字符数
    int passwordHistoryCount;       // 密码历史记录数量
    int maxLoginAttempts;           // 最大登录尝试次数
    int lockoutDuration;            // 锁定持续时间（分钟）
    
    // 默认构造函数 - 设置默认策略
    PasswordPolicy() : minLength(8), maxLength(32), requireUppercase(true),
                      requireLowercase(true), requireDigits(true), requireSpecialChars(true),
                      specialChars("!@#$%^&*()_+-=[]{}|;:,.<>?"), maxConsecutiveChars(3),
                      passwordHistoryCount(5), maxLoginAttempts(5), lockoutDuration(30) {}
};

// 声明为Qt元类型，以支持信号槽传递
Q_DECLARE_METATYPE(UserInfo)
Q_DECLARE_METATYPE(UserRole)
Q_DECLARE_METATYPE(UserSearchCriteria)
Q_DECLARE_METATYPE(UserSession)
Q_DECLARE_METATYPE(UserOperationLog)
Q_DECLARE_METATYPE(DoctorProfile)
Q_DECLARE_METATYPE(SurgeryItem)
Q_DECLARE_METATYPE(PatientItem)
Q_DECLARE_METATYPE(SurgeryCase)
// Q_DECLARE_METATYPE(InstrumentItem) - 已在Framework/Core/MedicalDataStructures.h中声明
Q_DECLARE_METATYPE(PasswordPolicy)

#endif // USER_DATA_STRUCTURES_H
