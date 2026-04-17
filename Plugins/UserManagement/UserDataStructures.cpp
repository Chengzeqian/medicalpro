#include "UserDataStructures.h"
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QDebug>

// UserInfo 附加实现方法

/**
 * @brief 生成密码哈希值
 * @param password 原始密码
 * @param salt 盐值
 * @return 哈希后的密码
 */
QString generatePasswordHash(const QString& password, const QString& salt)
{
    QString saltedPassword = password + salt;
    QByteArray hash = QCryptographicHash::hash(saltedPassword.toUtf8(), QCryptographicHash::Sha256);
    return QString(hash.toHex());
}

/**
 * @brief 生成随机盐值
 * @return 盐值字符串
 */
QString generateSalt()
{
    QString salt;
    const QString chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    
    for (int i = 0; i < 16; ++i) {
        int index = qrand() % chars.length();
        salt.append(chars.at(index));
    }
    
    return salt;
}

/**
 * @brief 验证密码强度
 * @param password 要验证的密码
 * @param policy 密码策略
 * @return 密码强度等级 (0-4)
 */
int validatePasswordStrength(const QString& password, const PasswordPolicy& policy)
{
    int score = 0;
    
    // 检查长度
    if (password.length() >= policy.minLength) {
        score++;
    }
    
    // 检查是否包含大写字母
    if (policy.requireUppercase && password.contains(QRegularExpression("[A-Z]"))) {
        score++;
    }
    
    // 检查是否包含小写字母
    if (policy.requireLowercase && password.contains(QRegularExpression("[a-z]"))) {
        score++;
    }
    
    // 检查是否包含数字
    if (policy.requireDigits && password.contains(QRegularExpression("[0-9]"))) {
        score++;
    }
    
    // 检查是否包含特殊字符
    if (policy.requireSpecialChars) {
        QString specialPattern = QString("[%1]").arg(QRegularExpression::escape(policy.specialChars));
        if (password.contains(QRegularExpression(specialPattern))) {
            score++;
        }
    }
    
    // 检查连续字符
    bool hasConsecutive = false;
    for (int i = 0; i < password.length() - policy.maxConsecutiveChars + 1; ++i) {
        QString substr = password.mid(i, policy.maxConsecutiveChars);
        if (substr.at(0) == substr.at(1) && substr.at(1) == substr.at(2)) {
            hasConsecutive = true;
            break;
        }
    }
    
    if (hasConsecutive) {
        score = qMax(0, score - 1);
    }
    
    return qMin(4, score);
}

/**
 * @brief 验证邮箱格式
 * @param email 邮箱地址
 * @return 格式正确返回true
 */
bool validateEmail(const QString& email)
{
    QRegularExpression emailRegex("^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$");
    return emailRegex.match(email).hasMatch();
}

/**
 * @brief 验证手机号格式
 * @param phone 手机号
 * @return 格式正确返回true
 */
bool validatePhone(const QString& phone)
{
    // 支持中国大陆手机号格式：1开头的11位数字
    QRegularExpression phoneRegex("^1[3-9]\\d{9}$");
    return phoneRegex.match(phone).hasMatch();
}

/**
 * @brief 验证用户名格式
 * @param username 用户名
 * @return 格式正确返回true
 */
bool validateUsername(const QString& username)
{
    // 用户名：3-20位，包含字母、数字、下划线，以字母开头
    if (username.length() < 3 || username.length() > 20) {
        return false;
    }
    
    QRegularExpression usernameRegex("^[a-zA-Z][a-zA-Z0-9_]*$");
    return usernameRegex.match(username).hasMatch();
}

/**
 * @brief 格式化用户角色为显示字符串
 * @param role 用户角色
 * @return 角色显示字符串
 */
QString formatUserRole(UserRole role)
{
    switch (role) {
        case UserRole::Guest:
            return "访客";
        case UserRole::Doctor:
            return "医生";
        case UserRole::Administrator:
            return "管理员";
        case UserRole::Operator:
            return "操作员";
        default:
            return "未知角色";
    }
}

/**
 * @brief 从字符串解析用户角色
 * @param roleString 角色字符串
 * @return 用户角色枚举
 */
UserRole parseUserRole(const QString& roleString)
{
    if (roleString == "访客" || roleString.toLower() == "guest") {
        return UserRole::Guest;
    } else if (roleString == "医生" || roleString.toLower() == "doctor") {
        return UserRole::Doctor;
    } else if (roleString == "管理员" || roleString.toLower() == "administrator" || roleString.toLower() == "admin") {
        return UserRole::Administrator;
    } else if (roleString == "操作员" || roleString.toLower() == "operator") {
        return UserRole::Operator;
    } else {
        return UserRole::Guest; // 默认为访客
    }
}

/**
 * @brief 生成会话ID
 * @return 唯一的会话ID
 */
QString generateSessionId()
{
    QString sessionId;
    const QString chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    
    for (int i = 0; i < 32; ++i) {
        int index = qrand() % chars.length();
        sessionId.append(chars.at(index));
    }
    
    return sessionId;
}

/**
 * @brief 记录用户操作日志
 * @param userId 用户ID
 * @param username 用户名
 * @param operation 操作类型
 * @param description 操作描述
 * @param target 操作目标
 * @param success 操作是否成功
 * @param errorMessage 错误信息
 * @return 操作日志对象
 */
UserOperationLog createOperationLog(int userId, const QString& username, 
                                   const QString& operation, const QString& description,
                                   const QString& target, bool success,
                                   const QString& errorMessage)
{
    UserOperationLog log;
    log.userId = userId;
    log.username = username;
    log.operation = operation;
    log.description = description;
    log.target = target;
    log.operationTime = QDateTime::currentDateTime();
    log.success = success;
    log.errorMessage = errorMessage;
    // IP地址在实际使用时需要从网络获取
    log.ipAddress = "127.0.0.1"; 
    
    return log;
}
