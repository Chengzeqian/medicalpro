#ifndef PATIENT_DATABASE_SERVICE_H
#define PATIENT_DATABASE_SERVICE_H

#include "PatientDataStructures.h"
#include <QObject>

class QWidget;

/**
 * @brief 患者数据库服务接口
 * 
 * 提供患者数据管理的完整服务接口，包括患者信息、影像数据和手术记录的管理。
 * 这是一个纯虚接口，遵循CTK服务架构设计原则。
 */
class PatientDatabaseService : public QObject
{
    Q_OBJECT
    
public:
    explicit PatientDatabaseService(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~PatientDatabaseService() = default;
    
    // ========== 患者信息管理 ==========
    
    /**
     * @brief 添加新患者
     * @param patient 患者信息
     * @return 成功返回true，失败返回false
     */
    virtual bool addPatient(const PatientInfo& patient) = 0;
    
    /**
     * @brief 更新患者信息
     * @param patient 患者信息（包含ID）
     * @return 成功返回true，失败返回false
     */
    virtual bool updatePatient(const PatientInfo& patient) = 0;
    
    /**
     * @brief 删除患者
     * @param patientId 患者ID
     * @return 成功返回true，失败返回false
     */
    virtual bool deletePatient(int patientId) = 0;
    
    /**
     * @brief 根据ID获取患者信息
     * @param patientId 患者ID
     * @return 患者信息，如果不存在则返回无效的PatientInfo
     */
    virtual PatientInfo getPatient(int patientId) = 0;
    
    /**
     * @brief 根据条件搜索患者
     * @param criteria 搜索条件
     * @return 符合条件的患者列表
     */
    virtual QList<PatientInfo> searchPatients(const PatientSearchCriteria& criteria) = 0;
    
    /**
     * @brief 获取所有患者列表
     * @return 所有患者信息
     */
    virtual QList<PatientInfo> getAllPatients() = 0;
    
    /**
     * @brief 根据手机号查找患者
     * @param phone 手机号
     * @return 患者信息，如果不存在则返回无效的PatientInfo
     */
    virtual PatientInfo getPatientByPhone(const QString& phone) = 0;
    
    // ========== 患者影像管理 ==========
    
    /**
     * @brief 添加患者影像信息
     * @param imageInfo 影像信息
     * @return 成功返回true，失败返回false
     */
    virtual bool addPatientImage(const PatientImageInfo& imageInfo) = 0;
    
    /**
     * @brief 更新患者影像信息
     * @param imageInfo 影像信息（包含ID）
     * @return 成功返回true，失败返回false
     */
    virtual bool updatePatientImage(const PatientImageInfo& imageInfo) = 0;
    
    /**
     * @brief 删除患者影像
     * @param imageId 影像ID
     * @return 成功返回true，失败返回false
     */
    virtual bool deletePatientImage(int imageId) = 0;
    
    /**
     * @brief 获取患者的所有影像
     * @param patientId 患者ID
     * @return 该患者的影像列表
     */
    virtual QList<PatientImageInfo> getPatientImages(int patientId) = 0;
    
    /**
     * @brief 根据ID获取影像信息
     * @param imageId 影像ID
     * @return 影像信息，如果不存在则返回无效的PatientImageInfo
     */
    virtual PatientImageInfo getPatientImage(int imageId) = 0;
    
    // ========== 手术记录管理 ==========
    
    /**
     * @brief 添加手术记录
     * @param record 手术记录
     * @return 成功返回true，失败返回false
     */
    virtual bool addSurgeryRecord(const SurgeryRecord& record) = 0;
    
    /**
     * @brief 更新手术记录
     * @param record 手术记录（包含ID）
     * @return 成功返回true，失败返回false
     */
    virtual bool updateSurgeryRecord(const SurgeryRecord& record) = 0;
    
    /**
     * @brief 删除手术记录
     * @param surgeryId 手术记录ID
     * @return 成功返回true，失败返回false
     */
    virtual bool deleteSurgeryRecord(int surgeryId) = 0;
    
    /**
     * @brief 获取患者的所有手术记录
     * @param patientId 患者ID
     * @return 该患者的手术记录列表
     */
    virtual QList<SurgeryRecord> getPatientSurgeries(int patientId) = 0;
    
    // ========== 数据库管理 ==========
    
    /**
     * @brief 初始化数据库
     * @return 成功返回true，失败返回false
     */
    virtual bool initializeDatabase() = 0;
    
    /**
     * @brief 备份数据库
     * @param backupPath 备份文件路径
     * @return 成功返回true，失败返回false
     */
    virtual bool backupDatabase(const QString& backupPath) = 0;
    
    /**
     * @brief 从备份恢复数据库
     * @param backupPath 备份文件路径
     * @return 成功返回true，失败返回false
     */
    virtual bool restoreDatabase(const QString& backupPath) = 0;
    
    /**
     * @brief 获取数据库状态信息
     * @return 数据库状态描述
     */
    virtual QString getDatabaseStatus() = 0;
    
    /**
     * @brief 获取患者总数
     * @return 患者总数
     */
    virtual int getPatientCount() = 0;
    
    /**
     * @brief 获取影像总数
     * @return 影像总数
     */
    virtual int getImageCount() = 0;
    
    // ========== UI显示管理 ==========
    
    /**
     * @brief 显示患者信息录入界面
     * @param parent 父窗口（可选）
     * @return 成功返回true，失败返回false
     */
    virtual bool showPatientInfoDialog(QWidget* parent = nullptr) = 0;
    
    /**
     * @brief 显示患者列表管理界面
     * @param parent 父窗口（可选）
     * @return 成功返回true，失败返回false
     */
    virtual bool showPatientListDialog(QWidget* parent = nullptr) = 0;

signals:
    /**
     * @brief 患者添加成功信号
     * @param patient 新添加的患者信息
     */
    void patientAdded(const PatientInfo& patient);
    
    /**
     * @brief 患者更新成功信号
     * @param patient 更新后的患者信息
     */
    void patientUpdated(const PatientInfo& patient);
    
    /**
     * @brief 患者删除成功信号
     * @param patientId 被删除的患者ID
     */
    void patientDeleted(int patientId);
    
    /**
     * @brief 影像添加成功信号
     * @param imageInfo 新添加的影像信息
     */
    void imageAdded(const PatientImageInfo& imageInfo);
    
    /**
     * @brief 影像更新成功信号
     * @param imageInfo 更新后的影像信息
     */
    void imageUpdated(const PatientImageInfo& imageInfo);
    
    /**
     * @brief 影像删除成功信号
     * @param imageId 被删除的影像ID
     */
    void imageDeleted(int imageId);
    
    /**
     * @brief 数据库错误信号
     * @param error 错误描述
     */
    void databaseError(const QString& error);
    
    /**
     * @brief 数据库状态变化信号
     * @param status 新状态
     */
    void databaseStatusChanged(const QString& status);
};

// 声明为CTK服务接口
Q_DECLARE_INTERFACE(PatientDatabaseService, "medical.PatientDatabaseService")

#endif // PATIENT_DATABASE_SERVICE_H
