#ifndef PATIENT_DATABASE_SERVICE_IMPL_H
#define PATIENT_DATABASE_SERVICE_IMPL_H

#include "PatientDatabaseService.h"
#include "SQLiteManager.h"
#include <QObject>
#include <QMutex>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QVariant>
#include <QDateTime>

class ctkPluginContext;

/**
 * @brief 患者数据库服务实现类
 * 
 * PatientDatabaseService接口的SQLite具体实现。
 * 提供完整的患者数据管理功能，包括CRUD操作、搜索和数据库管理。
 */
class PatientDatabaseServiceImpl : public PatientDatabaseService
{
    Q_OBJECT
    Q_INTERFACES(PatientDatabaseService)
    
public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit PatientDatabaseServiceImpl(QObject *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~PatientDatabaseServiceImpl() override;
    
    // ========== PatientDatabaseService接口实现 ==========
    
    // 患者信息管理
    bool addPatient(const PatientInfo& patient) override;
    bool updatePatient(const PatientInfo& patient) override;
    bool deletePatient(int patientId) override;
    PatientInfo getPatient(int patientId) override;
    QList<PatientInfo> searchPatients(const PatientSearchCriteria& criteria) override;
    QList<PatientInfo> getAllPatients() override;
    PatientInfo getPatientByPhone(const QString& phone) override;
    
    // 患者影像管理
    bool addPatientImage(const PatientImageInfo& imageInfo) override;
    bool updatePatientImage(const PatientImageInfo& imageInfo) override;
    bool deletePatientImage(int imageId) override;
    QList<PatientImageInfo> getPatientImages(int patientId) override;
    PatientImageInfo getPatientImage(int imageId) override;
    
    // 手术记录管理
    bool addSurgeryRecord(const SurgeryRecord& record) override;
    bool updateSurgeryRecord(const SurgeryRecord& record) override;
    bool deleteSurgeryRecord(int surgeryId) override;
    QList<SurgeryRecord> getPatientSurgeries(int patientId) override;
    
    // 数据库管理
    bool initializeDatabase() override;
    bool backupDatabase(const QString& backupPath) override;
    bool restoreDatabase(const QString& backupPath) override;
    QString getDatabaseStatus() override;
    int getPatientCount() override;
    int getImageCount() override;
    
    // UI显示管理
    bool showPatientInfoDialog(QWidget* parent = nullptr) override;
    bool showPatientListDialog(QWidget* parent = nullptr) override;
    
    /**
     * @brief 设置CTK插件上下文
     * @param context CTK插件上下文
     */
    void setPluginContext(ctkPluginContext* context);

private slots:
    /**
     * @brief 处理数据库错误
     * @param error 错误信息
     */
    void onDatabaseError(const QString& error);

private:
    /**
     * @brief 从查询结果创建PatientInfo对象
     * @param query 查询结果
     * @return PatientInfo对象
     */
    PatientInfo createPatientFromQuery(const QSqlQuery& query) const;
    
    /**
     * @brief 从查询结果创建PatientImageInfo对象
     * @param query 查询结果
     * @return PatientImageInfo对象
     */
    PatientImageInfo createImageFromQuery(const QSqlQuery& query) const;
    
    /**
     * @brief 从查询结果创建SurgeryRecord对象
     * @param query 查询结果
     * @return SurgeryRecord对象
     */
    SurgeryRecord createSurgeryFromQuery(const QSqlQuery& query) const;
    
    /**
     * @brief 检查患者是否存在
     * @param patientId 患者ID
     * @return 存在返回true，否则返回false
     */
    bool patientExists(int patientId) const;
    
    /**
     * @brief 检查影像是否存在
     * @param imageId 影像ID
     * @return 存在返回true，否则返回false
     */
    bool imageExists(int imageId) const;
    
    /**
     * @brief 检查手术记录是否存在
     * @param surgeryId 手术记录ID
     * @return 存在返回true，否则返回false
     */
    bool surgeryExists(int surgeryId) const;
    
    /**
     * @brief 构建搜索查询的WHERE子句
     * @param criteria 搜索条件
     * @param bindValues 绑定参数值的输出列表
     * @return WHERE子句字符串
     */
    QString buildSearchWhereClause(const PatientSearchCriteria& criteria, 
                                  QVariantList& bindValues) const;
    
    /**
     * @brief 验证患者数据
     * @param patient 患者信息
     * @param isUpdate 是否为更新操作
     * @return 验证通过返回空字符串，否则返回错误信息
     */
    QString validatePatientData(const PatientInfo& patient, bool isUpdate = false) const;
    
    /**
     * @brief 验证影像数据
     * @param imageInfo 影像信息
     * @param isUpdate 是否为更新操作
     * @return 验证通过返回空字符串，否则返回错误信息
     */
    QString validateImageData(const PatientImageInfo& imageInfo, bool isUpdate = false) const;
    
    /**
     * @brief 验证手术记录数据
     * @param record 手术记录
     * @param isUpdate 是否为更新操作
     * @return 验证通过返回空字符串，否则返回错误信息
     */
    QString validateSurgeryData(const SurgeryRecord& record, bool isUpdate = false) const;
    
    /**
     * @brief 记录错误日志并发送信号
     * @param operation 操作名称
     * @param error 错误信息
     */
    void logAndEmitError(const QString& operation, const QString& error);
    
    /**
     * @brief 获取下一个可用的患者ID
     * @return 下一个患者ID
     */
    int getNextPatientId() const;
    
    /**
     * @brief 更新患者的最后访问时间
     * @param patientId 患者ID
     */
    void updatePatientLastVisit(int patientId);

private:
    SQLiteManager& m_dbManager;     // 数据库管理器引用
    mutable QMutex m_mutex;         // 线程安全互斥锁
    QString m_lastError;            // 最后一个错误信息
    ctkPluginContext* m_pluginContext; // CTK插件上下文
};

#endif // PATIENT_DATABASE_SERVICE_IMPL_H
