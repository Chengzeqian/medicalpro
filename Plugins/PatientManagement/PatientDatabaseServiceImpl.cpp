#include "PatientDatabaseServiceImpl.h"
#include "PatientInfoWidget.h"
#include "PatientListWidget.h"
#include <QDebug>
#include <QSqlError>
#include <QRegularExpression>
#include <QFileInfo>
#include <QMutexLocker>
#include <QDialog>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QWidget>

PatientDatabaseServiceImpl::PatientDatabaseServiceImpl(QObject *parent)
    : PatientDatabaseService(parent)
    , m_dbManager(SQLiteManager::instance())
    , m_pluginContext(nullptr)
{
    // 连接数据库错误信号
    connect(&m_dbManager, &SQLiteManager::databaseError,
            this, &PatientDatabaseServiceImpl::onDatabaseError);
    
    // 连接数据库状态信号
    connect(&m_dbManager, &SQLiteManager::databaseConnected,
            this, [this]() {
                emit databaseStatusChanged("已连接");
                qDebug() << "Patient database service connected";
            });
    
    connect(&m_dbManager, &SQLiteManager::databaseDisconnected,
            this, [this]() {
                emit databaseStatusChanged("已断开");
                qDebug() << "Patient database service disconnected";
            });
}

PatientDatabaseServiceImpl::~PatientDatabaseServiceImpl()
{
    // 析构函数中不需要特殊处理，SQLiteManager是单例会自动管理
}

void PatientDatabaseServiceImpl::onDatabaseError(const QString& error)
{
    m_lastError = error;
    emit databaseError(error);
}

// ========== 患者信息管理实现 ==========

bool PatientDatabaseServiceImpl::addPatient(const PatientInfo& patient)
{
    QMutexLocker locker(&m_mutex);
    
    try {
        // 验证数据
        QString validationError = validatePatientData(patient, false);
        if (!validationError.isEmpty()) {
            logAndEmitError("添加患者", "数据验证失败: " + validationError);
            return false;
        }
        
        // 准备SQL语句
        QString sql = R"(
            INSERT INTO patients (
                name, age, gender, phone, id_card, address, 
                emergency_contact, emergency_phone, medical_history, 
                allergies, current_medications, registration_date, 
                last_visit_date, notes
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )";
        
        QVariantList values;
        values << patient.name
               << patient.age
               << patient.gender
               << patient.phone
               << patient.idCard
               << patient.address
               << patient.emergencyContact
               << patient.emergencyPhone
               << patient.medicalHistory
               << patient.allergies
               << patient.currentMedications
               << patient.registrationDate.toString(Qt::ISODate)
               << patient.lastVisitDate.toString(Qt::ISODate)
               << patient.notes;
        
        bool success = m_dbManager.executeNonQuery(sql, values);
        
        if (success) {
            // 获取新插入的患者信息（包含生成的ID）
            QString getLastSql = "SELECT * FROM patients WHERE phone = ? ORDER BY patient_id DESC LIMIT 1";
            QSqlQuery query = m_dbManager.executeQuery(getLastSql, QVariantList() << patient.phone);
            
            if (query.next()) {
                PatientInfo newPatient = createPatientFromQuery(query);
                emit patientAdded(newPatient);
                qDebug() << "Patient added successfully with ID:" << newPatient.patientId;
            }
        } else {
            logAndEmitError("添加患者", "数据库操作失败");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logAndEmitError("添加患者", QString("异常: %1").arg(e.what()));
        return false;
    }
}

bool PatientDatabaseServiceImpl::updatePatient(const PatientInfo& patient)
{
    QMutexLocker locker(&m_mutex);
    
    try {
        // 验证数据
        QString validationError = validatePatientData(patient, true);
        if (!validationError.isEmpty()) {
            logAndEmitError("更新患者", "数据验证失败: " + validationError);
            return false;
        }
        
        // 检查患者是否存在
        if (!patientExists(patient.patientId)) {
            logAndEmitError("更新患者", "患者不存在 ID: " + QString::number(patient.patientId));
            return false;
        }
        
        // 准备SQL语句
        QString sql = R"(
            UPDATE patients SET 
                name = ?, age = ?, gender = ?, phone = ?, id_card = ?, 
                address = ?, emergency_contact = ?, emergency_phone = ?, 
                medical_history = ?, allergies = ?, current_medications = ?, 
                last_visit_date = ?, notes = ?, updated_at = CURRENT_TIMESTAMP
            WHERE patient_id = ?
        )";
        
        QVariantList values;
        values << patient.name
               << patient.age
               << patient.gender
               << patient.phone
               << patient.idCard
               << patient.address
               << patient.emergencyContact
               << patient.emergencyPhone
               << patient.medicalHistory
               << patient.allergies
               << patient.currentMedications
               << patient.lastVisitDate.toString(Qt::ISODate)
               << patient.notes
               << patient.patientId;
        
        bool success = m_dbManager.executeNonQuery(sql, values);
        
        if (success) {
            emit patientUpdated(patient);
            qDebug() << "Patient updated successfully, ID:" << patient.patientId;
        } else {
            logAndEmitError("更新患者", "数据库操作失败");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logAndEmitError("更新患者", QString("异常: %1").arg(e.what()));
        return false;
    }
}

bool PatientDatabaseServiceImpl::deletePatient(int patientId)
{
    QMutexLocker locker(&m_mutex);
    
    try {
        // 检查患者是否存在
        if (!patientExists(patientId)) {
            logAndEmitError("删除患者", "患者不存在 ID: " + QString::number(patientId));
            return false;
        }
        
        // 开始事务（因为需要删除关联数据）
        if (!m_dbManager.beginTransaction()) {
            logAndEmitError("删除患者", "无法开始事务");
            return false;
        }
        
        bool success = true;
        
        // 删除患者的所有影像记录
        QString deleteImagesSql = "DELETE FROM patient_images WHERE patient_id = ?";
        if (!m_dbManager.executeNonQuery(deleteImagesSql, QVariantList() << patientId)) {
            logAndEmitError("删除患者", "删除影像记录失败");
            success = false;
        }
        
        // 删除患者的所有手术记录
        QString deleteSurgeriesSql = "DELETE FROM surgery_records WHERE patient_id = ?";
        if (success && !m_dbManager.executeNonQuery(deleteSurgeriesSql, QVariantList() << patientId)) {
            logAndEmitError("删除患者", "删除手术记录失败");
            success = false;
        }
        
        // 删除患者主记录
        QString deletePatientSql = "DELETE FROM patients WHERE patient_id = ?";
        if (success && !m_dbManager.executeNonQuery(deletePatientSql, QVariantList() << patientId)) {
            logAndEmitError("删除患者", "删除患者主记录失败");
            success = false;
        }
        
        if (success) {
            m_dbManager.commitTransaction();
            emit patientDeleted(patientId);
            qDebug() << "Patient deleted successfully, ID:" << patientId;
        } else {
            m_dbManager.rollbackTransaction();
        }
        
        return success;
        
    } catch (const std::exception& e) {
        m_dbManager.rollbackTransaction();
        logAndEmitError("删除患者", QString("异常: %1").arg(e.what()));
        return false;
    }
}

PatientInfo PatientDatabaseServiceImpl::getPatient(int patientId)
{
    QMutexLocker locker(&m_mutex);
    
    try {
        QString sql = "SELECT * FROM patients WHERE patient_id = ?";
        QSqlQuery query = m_dbManager.executeQuery(sql, QVariantList() << patientId);
        
        if (query.next()) {
            return createPatientFromQuery(query);
        }
        
        return PatientInfo(); // 返回无效的患者信息
        
    } catch (const std::exception& e) {
        logAndEmitError("获取患者", QString("异常: %1").arg(e.what()));
        return PatientInfo();
    }
}

PatientInfo PatientDatabaseServiceImpl::getPatientByPhone(const QString& phone)
{
    QMutexLocker locker(&m_mutex);
    
    try {
        QString sql = "SELECT * FROM patients WHERE phone = ?";
        QSqlQuery query = m_dbManager.executeQuery(sql, QVariantList() << phone);
        
        if (query.next()) {
            return createPatientFromQuery(query);
        }
        
        return PatientInfo(); // 返回无效的患者信息
        
    } catch (const std::exception& e) {
        logAndEmitError("根据手机号获取患者", QString("异常: %1").arg(e.what()));
        return PatientInfo();
    }
}

QList<PatientInfo> PatientDatabaseServiceImpl::searchPatients(const PatientSearchCriteria& criteria)
{
    QMutexLocker locker(&m_mutex);
    QList<PatientInfo> patients;
    
    try {
        QString baseSql = "SELECT * FROM patients";
        QVariantList bindValues;
        
        if (!criteria.isEmpty()) {
            baseSql += " WHERE " + buildSearchWhereClause(criteria, bindValues);
        }
        
        baseSql += " ORDER BY registration_date DESC";
        
        QSqlQuery query = m_dbManager.executeQuery(baseSql, bindValues);
        
        while (query.next()) {
            patients.append(createPatientFromQuery(query));
        }
        
        qDebug() << "Found" << patients.size() << "patients matching criteria";
        
    } catch (const std::exception& e) {
        logAndEmitError("搜索患者", QString("异常: %1").arg(e.what()));
    }
    
    return patients;
}

QList<PatientInfo> PatientDatabaseServiceImpl::getAllPatients()
{
    return searchPatients(PatientSearchCriteria());
}

// ========== 患者影像管理实现 ==========

bool PatientDatabaseServiceImpl::addPatientImage(const PatientImageInfo& imageInfo)
{
    QMutexLocker locker(&m_mutex);
    
    try {
        // 验证数据
        QString validationError = validateImageData(imageInfo, false);
        if (!validationError.isEmpty()) {
            logAndEmitError("添加影像", "数据验证失败: " + validationError);
            return false;
        }
        
        // 检查患者是否存在
        if (!patientExists(imageInfo.patientId)) {
            logAndEmitError("添加影像", "患者不存在 ID: " + QString::number(imageInfo.patientId));
            return false;
        }
        
        QString sql = R"(
            INSERT INTO patient_images (
                patient_id, image_path, image_type, body_part, 
                scan_date, scan_parameters, description, 
                radiologist_notes, is_processed, processing_notes
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )";
        
        QVariantList values;
        values << imageInfo.patientId
               << imageInfo.imagePath
               << imageInfo.imageType
               << imageInfo.bodyPart
               << imageInfo.scanDate.toString(Qt::ISODate)
               << imageInfo.scanParameters
               << imageInfo.description
               << imageInfo.radiologistNotes
               << imageInfo.isProcessed
               << imageInfo.processingNotes;
        
        bool success = m_dbManager.executeNonQuery(sql, values);
        
        if (success) {
            // 获取新插入的影像信息
            QString getLastSql = "SELECT * FROM patient_images WHERE patient_id = ? AND image_path = ? ORDER BY image_id DESC LIMIT 1";
            QSqlQuery query = m_dbManager.executeQuery(getLastSql, 
                QVariantList() << imageInfo.patientId << imageInfo.imagePath);
            
            if (query.next()) {
                PatientImageInfo newImage = createImageFromQuery(query);
                emit imageAdded(newImage);
                qDebug() << "Patient image added successfully with ID:" << newImage.imageId;
            }
        } else {
            logAndEmitError("添加影像", "数据库操作失败");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logAndEmitError("添加影像", QString("异常: %1").arg(e.what()));
        return false;
    }
}

bool PatientDatabaseServiceImpl::updatePatientImage(const PatientImageInfo& imageInfo)
{
    QMutexLocker locker(&m_mutex);
    
    try {
        // 验证数据
        QString validationError = validateImageData(imageInfo, true);
        if (!validationError.isEmpty()) {
            logAndEmitError("更新影像", "数据验证失败: " + validationError);
            return false;
        }
        
        // 检查影像是否存在
        if (!imageExists(imageInfo.imageId)) {
            logAndEmitError("更新影像", "影像不存在 ID: " + QString::number(imageInfo.imageId));
            return false;
        }
        
        QString sql = R"(
            UPDATE patient_images SET 
                image_path = ?, image_type = ?, body_part = ?, 
                scan_date = ?, scan_parameters = ?, description = ?, 
                radiologist_notes = ?, is_processed = ?, processing_notes = ?,
                updated_at = CURRENT_TIMESTAMP
            WHERE image_id = ?
        )";
        
        QVariantList values;
        values << imageInfo.imagePath
               << imageInfo.imageType
               << imageInfo.bodyPart
               << imageInfo.scanDate.toString(Qt::ISODate)
               << imageInfo.scanParameters
               << imageInfo.description
               << imageInfo.radiologistNotes
               << imageInfo.isProcessed
               << imageInfo.processingNotes
               << imageInfo.imageId;
        
        bool success = m_dbManager.executeNonQuery(sql, values);
        
        if (success) {
            emit imageUpdated(imageInfo);
            qDebug() << "Patient image updated successfully, ID:" << imageInfo.imageId;
        } else {
            logAndEmitError("更新影像", "数据库操作失败");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logAndEmitError("更新影像", QString("异常: %1").arg(e.what()));
        return false;
    }
}

bool PatientDatabaseServiceImpl::deletePatientImage(int imageId)
{
    QMutexLocker locker(&m_mutex);
    
    try {
        // 检查影像是否存在
        if (!imageExists(imageId)) {
            logAndEmitError("删除影像", "影像不存在 ID: " + QString::number(imageId));
            return false;
        }
        
        QString sql = "DELETE FROM patient_images WHERE image_id = ?";
        bool success = m_dbManager.executeNonQuery(sql, QVariantList() << imageId);
        
        if (success) {
            emit imageDeleted(imageId);
            qDebug() << "Patient image deleted successfully, ID:" << imageId;
        } else {
            logAndEmitError("删除影像", "数据库操作失败");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logAndEmitError("删除影像", QString("异常: %1").arg(e.what()));
        return false;
    }
}

QList<PatientImageInfo> PatientDatabaseServiceImpl::getPatientImages(int patientId)
{
    QMutexLocker locker(&m_mutex);
    QList<PatientImageInfo> images;
    
    try {
        QString sql = "SELECT * FROM patient_images WHERE patient_id = ? ORDER BY scan_date DESC";
        QSqlQuery query = m_dbManager.executeQuery(sql, QVariantList() << patientId);
        
        while (query.next()) {
            images.append(createImageFromQuery(query));
        }
        
        qDebug() << "Found" << images.size() << "images for patient ID:" << patientId;
        
    } catch (const std::exception& e) {
        logAndEmitError("获取患者影像", QString("异常: %1").arg(e.what()));
    }
    
    return images;
}

PatientImageInfo PatientDatabaseServiceImpl::getPatientImage(int imageId)
{
    QMutexLocker locker(&m_mutex);
    
    try {
        QString sql = "SELECT * FROM patient_images WHERE image_id = ?";
        QSqlQuery query = m_dbManager.executeQuery(sql, QVariantList() << imageId);
        
        if (query.next()) {
            return createImageFromQuery(query);
        }
        
        return PatientImageInfo(); // 返回无效的影像信息
        
    } catch (const std::exception& e) {
        logAndEmitError("获取影像", QString("异常: %1").arg(e.what()));
        return PatientImageInfo();
    }
}

// ========== 手术记录管理实现 ==========

bool PatientDatabaseServiceImpl::addSurgeryRecord(const SurgeryRecord& record)
{
    QMutexLocker locker(&m_mutex);
    
    try {
        // 验证数据
        QString validationError = validateSurgeryData(record, false);
        if (!validationError.isEmpty()) {
            logAndEmitError("添加手术记录", "数据验证失败: " + validationError);
            return false;
        }
        
        // 检查患者是否存在
        if (!patientExists(record.patientId)) {
            logAndEmitError("添加手术记录", "患者不存在 ID: " + QString::number(record.patientId));
            return false;
        }
        
        QString sql = R"(
            INSERT INTO surgery_records (
                patient_id, surgery_type, surgery_date, surgeon, assistants,
                pre_op_diagnosis, post_op_diagnosis, procedure, 
                complications, notes, outcome
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )";
        
        QVariantList values;
        values << record.patientId
               << record.surgeryType
               << record.surgeryDate.toString(Qt::ISODate)
               << record.surgeon
               << record.assistants
               << record.preOpDiagnosis
               << record.postOpDiagnosis
               << record.procedure
               << record.complications
               << record.notes
               << record.outcome;
        
        bool success = m_dbManager.executeNonQuery(sql, values);
        
        if (success) {
            qDebug() << "Surgery record added successfully for patient ID:" << record.patientId;
        } else {
            logAndEmitError("添加手术记录", "数据库操作失败");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logAndEmitError("添加手术记录", QString("异常: %1").arg(e.what()));
        return false;
    }
}

bool PatientDatabaseServiceImpl::updateSurgeryRecord(const SurgeryRecord& record)
{
    QMutexLocker locker(&m_mutex);
    
    try {
        // 验证数据
        QString validationError = validateSurgeryData(record, true);
        if (!validationError.isEmpty()) {
            logAndEmitError("更新手术记录", "数据验证失败: " + validationError);
            return false;
        }
        
        // 检查手术记录是否存在
        if (!surgeryExists(record.surgeryId)) {
            logAndEmitError("更新手术记录", "手术记录不存在 ID: " + QString::number(record.surgeryId));
            return false;
        }
        
        QString sql = R"(
            UPDATE surgery_records SET 
                surgery_type = ?, surgery_date = ?, surgeon = ?, assistants = ?,
                pre_op_diagnosis = ?, post_op_diagnosis = ?, procedure = ?, 
                complications = ?, notes = ?, outcome = ?,
                updated_at = CURRENT_TIMESTAMP
            WHERE surgery_id = ?
        )";
        
        QVariantList values;
        values << record.surgeryType
               << record.surgeryDate.toString(Qt::ISODate)
               << record.surgeon
               << record.assistants
               << record.preOpDiagnosis
               << record.postOpDiagnosis
               << record.procedure
               << record.complications
               << record.notes
               << record.outcome
               << record.surgeryId;
        
        bool success = m_dbManager.executeNonQuery(sql, values);
        
        if (success) {
            qDebug() << "Surgery record updated successfully, ID:" << record.surgeryId;
        } else {
            logAndEmitError("更新手术记录", "数据库操作失败");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logAndEmitError("更新手术记录", QString("异常: %1").arg(e.what()));
        return false;
    }
}

bool PatientDatabaseServiceImpl::deleteSurgeryRecord(int surgeryId)
{
    QMutexLocker locker(&m_mutex);
    
    try {
        // 检查手术记录是否存在
        if (!surgeryExists(surgeryId)) {
            logAndEmitError("删除手术记录", "手术记录不存在 ID: " + QString::number(surgeryId));
            return false;
        }
        
        QString sql = "DELETE FROM surgery_records WHERE surgery_id = ?";
        bool success = m_dbManager.executeNonQuery(sql, QVariantList() << surgeryId);
        
        if (success) {
            qDebug() << "Surgery record deleted successfully, ID:" << surgeryId;
        } else {
            logAndEmitError("删除手术记录", "数据库操作失败");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logAndEmitError("删除手术记录", QString("异常: %1").arg(e.what()));
        return false;
    }
}

QList<SurgeryRecord> PatientDatabaseServiceImpl::getPatientSurgeries(int patientId)
{
    QMutexLocker locker(&m_mutex);
    QList<SurgeryRecord> surgeries;
    
    try {
        QString sql = "SELECT * FROM surgery_records WHERE patient_id = ? ORDER BY surgery_date DESC";
        QSqlQuery query = m_dbManager.executeQuery(sql, QVariantList() << patientId);
        
        while (query.next()) {
            surgeries.append(createSurgeryFromQuery(query));
        }
        
        qDebug() << "Found" << surgeries.size() << "surgeries for patient ID:" << patientId;
        
    } catch (const std::exception& e) {
        logAndEmitError("获取患者手术记录", QString("异常: %1").arg(e.what()));
    }
    
    return surgeries;
}

// ========== 数据库管理实现 ==========

bool PatientDatabaseServiceImpl::initializeDatabase()
{
    return m_dbManager.initializeDatabase();
}

bool PatientDatabaseServiceImpl::backupDatabase(const QString& backupPath)
{
    return m_dbManager.createBackup(backupPath);
}

bool PatientDatabaseServiceImpl::restoreDatabase(const QString& backupPath)
{
    return m_dbManager.restoreFromBackup(backupPath);
}

QString PatientDatabaseServiceImpl::getDatabaseStatus()
{
    if (!m_dbManager.isConnected()) {
        return "未连接";
    }
    
    QString status = "已连接\n";
    status += QString("患者数量: %1\n").arg(getPatientCount());
    status += QString("影像数量: %1\n").arg(getImageCount());
    status += QString("数据库大小: %1 KB").arg(m_dbManager.getDatabaseSize() / 1024);
    
    return status;
}

int PatientDatabaseServiceImpl::getPatientCount()
{
    return m_dbManager.getTableRowCount("patients");
}

int PatientDatabaseServiceImpl::getImageCount()
{
    return m_dbManager.getTableRowCount("patient_images");
}

// ========== 辅助方法实现 ==========

PatientInfo PatientDatabaseServiceImpl::createPatientFromQuery(const QSqlQuery& query) const
{
    PatientInfo patient;
    patient.patientId = query.value("patient_id").toInt();
    patient.name = query.value("name").toString();
    patient.age = query.value("age").toInt();
    patient.gender = query.value("gender").toString();
    patient.phone = query.value("phone").toString();
    patient.idCard = query.value("id_card").toString();
    patient.address = query.value("address").toString();
    patient.emergencyContact = query.value("emergency_contact").toString();
    patient.emergencyPhone = query.value("emergency_phone").toString();
    patient.medicalHistory = query.value("medical_history").toString();
    patient.allergies = query.value("allergies").toString();
    patient.currentMedications = query.value("current_medications").toString();
    patient.registrationDate = QDateTime::fromString(query.value("registration_date").toString(), Qt::ISODate);
    patient.lastVisitDate = QDateTime::fromString(query.value("last_visit_date").toString(), Qt::ISODate);
    patient.notes = query.value("notes").toString();
    
    return patient;
}

PatientImageInfo PatientDatabaseServiceImpl::createImageFromQuery(const QSqlQuery& query) const
{
    PatientImageInfo image;
    image.imageId = query.value("image_id").toInt();
    image.patientId = query.value("patient_id").toInt();
    image.imagePath = query.value("image_path").toString();
    image.imageType = query.value("image_type").toString();
    image.bodyPart = query.value("body_part").toString();
    image.scanDate = QDateTime::fromString(query.value("scan_date").toString(), Qt::ISODate);
    image.scanParameters = query.value("scan_parameters").toString();
    image.description = query.value("description").toString();
    image.radiologistNotes = query.value("radiologist_notes").toString();
    image.isProcessed = query.value("is_processed").toBool();
    image.processingNotes = query.value("processing_notes").toString();
    
    return image;
}

SurgeryRecord PatientDatabaseServiceImpl::createSurgeryFromQuery(const QSqlQuery& query) const
{
    SurgeryRecord surgery;
    surgery.surgeryId = query.value("surgery_id").toInt();
    surgery.patientId = query.value("patient_id").toInt();
    surgery.surgeryType = query.value("surgery_type").toString();
    surgery.surgeryDate = QDateTime::fromString(query.value("surgery_date").toString(), Qt::ISODate);
    surgery.surgeon = query.value("surgeon").toString();
    surgery.assistants = query.value("assistants").toString();
    surgery.preOpDiagnosis = query.value("pre_op_diagnosis").toString();
    surgery.postOpDiagnosis = query.value("post_op_diagnosis").toString();
    surgery.procedure = query.value("procedure").toString();
    surgery.complications = query.value("complications").toString();
    surgery.notes = query.value("notes").toString();
    surgery.outcome = query.value("outcome").toString();
    
    return surgery;
}

bool PatientDatabaseServiceImpl::patientExists(int patientId) const
{
    QString sql = "SELECT COUNT(*) FROM patients WHERE patient_id = ?";
    QVariant result = m_dbManager.executeScalar(sql + QString(" AND patient_id = %1").arg(patientId), 0);
    return result.toInt() > 0;
}

bool PatientDatabaseServiceImpl::imageExists(int imageId) const
{
    QString sql = "SELECT COUNT(*) FROM patient_images WHERE image_id = ?";
    QVariant result = m_dbManager.executeScalar(sql + QString(" AND image_id = %1").arg(imageId), 0);
    return result.toInt() > 0;
}

bool PatientDatabaseServiceImpl::surgeryExists(int surgeryId) const
{
    QString sql = "SELECT COUNT(*) FROM surgery_records WHERE surgery_id = ?";
    QVariant result = m_dbManager.executeScalar(sql + QString(" AND surgery_id = %1").arg(surgeryId), 0);
    return result.toInt() > 0;
}

QString PatientDatabaseServiceImpl::buildSearchWhereClause(const PatientSearchCriteria& criteria, 
                                                          QVariantList& bindValues) const
{
    QStringList conditions;
    
    if (!criteria.nameFilter.isEmpty()) {
        conditions << "name LIKE ?";
        bindValues << ("%" + criteria.nameFilter + "%");
    }
    
    if (!criteria.phoneFilter.isEmpty()) {
        conditions << "phone LIKE ?";
        bindValues << ("%" + criteria.phoneFilter + "%");
    }
    
    if (!criteria.idCardFilter.isEmpty()) {
        conditions << "id_card LIKE ?";
        bindValues << ("%" + criteria.idCardFilter + "%");
    }
    
    if (criteria.ageMin >= 0) {
        conditions << "age >= ?";
        bindValues << criteria.ageMin;
    }
    
    if (criteria.ageMax >= 0) {
        conditions << "age <= ?";
        bindValues << criteria.ageMax;
    }
    
    if (!criteria.genderFilter.isEmpty()) {
        conditions << "gender = ?";
        bindValues << criteria.genderFilter;
    }
    
    if (criteria.registrationDateStart.isValid()) {
        conditions << "registration_date >= ?";
        bindValues << criteria.registrationDateStart.toString(Qt::ISODate);
    }
    
    if (criteria.registrationDateEnd.isValid()) {
        conditions << "registration_date <= ?";
        bindValues << criteria.registrationDateEnd.toString(Qt::ISODate);
    }
    
    if (!criteria.medicalHistoryFilter.isEmpty()) {
        conditions << "medical_history LIKE ?";
        bindValues << ("%" + criteria.medicalHistoryFilter + "%");
    }
    
    return conditions.join(" AND ");
}

QString PatientDatabaseServiceImpl::validatePatientData(const PatientInfo& patient, bool isUpdate) const
{
    if (patient.name.trimmed().isEmpty()) {
        return "患者姓名不能为空";
    }
    
    if (patient.age < 0 || patient.age > 150) {
        return "患者年龄必须在0-150之间";
    }
    
    if (!patient.phone.isEmpty()) {
        QRegularExpression phoneRegex("^1[3-9]\\d{9}$");
        if (!phoneRegex.match(patient.phone).hasMatch()) {
            return "手机号格式不正确";
        }
    }
    
    if (!patient.idCard.isEmpty()) {
        QRegularExpression idCardRegex("^[1-9]\\d{5}(18|19|20)\\d{2}((0[1-9])|(1[0-2]))(([0-2][1-9])|10|20|30|31)\\d{3}[0-9Xx]$");
        if (!idCardRegex.match(patient.idCard).hasMatch()) {
            return "身份证号格式不正确";
        }
    }
    
    return QString(); // 验证通过
}

QString PatientDatabaseServiceImpl::validateImageData(const PatientImageInfo& imageInfo, bool isUpdate) const
{
    if (imageInfo.patientId <= 0) {
        return "患者ID必须大于0";
    }
    
    if (imageInfo.imagePath.trimmed().isEmpty()) {
        return "影像路径不能为空";
    }
    
    if (imageInfo.imageType.trimmed().isEmpty()) {
        return "影像类型不能为空";
    }
    
    return QString(); // 验证通过
}

QString PatientDatabaseServiceImpl::validateSurgeryData(const SurgeryRecord& record, bool isUpdate) const
{
    if (record.patientId <= 0) {
        return "患者ID必须大于0";
    }
    
    if (record.surgeryType.trimmed().isEmpty()) {
        return "手术类型不能为空";
    }
    
    if (record.surgeon.trimmed().isEmpty()) {
        return "主刀医生不能为空";
    }
    
    if (!record.surgeryDate.isValid()) {
        return "手术日期无效";
    }
    
    return QString(); // 验证通过
}

void PatientDatabaseServiceImpl::logAndEmitError(const QString& operation, const QString& error)
{
    QString fullError = QString("[%1] %2").arg(operation, error);
    m_lastError = fullError;
    qWarning() << "PatientDatabaseService Error:" << fullError;
    emit databaseError(fullError);
}

bool PatientDatabaseServiceImpl::showPatientInfoDialog(QWidget* parent)
{
    try {
        // 创建患者信息录入对话框
        QDialog* dialog = new QDialog(parent);
        dialog->setWindowTitle("患者信息录入");
        dialog->setWindowIcon(QIcon(":/icons/patient_add.png"));
        dialog->setModal(true);
        dialog->resize(800, 600);
        
        // 创建布局
        QVBoxLayout* layout = new QVBoxLayout(dialog);
        
        // 创建患者信息录入界面
        PatientInfoWidget* patientInfoWidget = new PatientInfoWidget(dialog);
        if (m_pluginContext) {
            patientInfoWidget->setPluginContext(m_pluginContext);
        }
        layout->addWidget(patientInfoWidget);
        
        // 创建按钮框
        QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
        layout->addWidget(buttonBox);
        
        // 连接信号
        QObject::connect(buttonBox, &QDialogButtonBox::clicked, dialog, &QDialog::accept);
        QObject::connect(patientInfoWidget, &PatientInfoWidget::patientSaved, 
                        [this](const PatientInfo& patient, bool isNewPatient) {
            emit patientAdded(patient);
            qDebug() << "Patient" << (isNewPatient ? "added" : "updated") << ":" << patient.name;
        });
        
        // 显示对话框
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
        
        qDebug() << "[PatientManagement] Patient info dialog displayed";
        return true;
        
    } catch (const std::exception& e) {
        logAndEmitError("显示患者信息录入界面", e.what());
        return false;
    }
}

bool PatientDatabaseServiceImpl::showPatientListDialog(QWidget* parent)
{
    try {
        // 创建患者列表管理对话框
        QDialog* dialog = new QDialog(parent);
        dialog->setWindowTitle("患者列表管理");
        dialog->setWindowIcon(QIcon(":/icons/patient_list.png"));
        dialog->setModal(false);  // 非模态，允许与主界面交互
        dialog->resize(1000, 700);
        
        // 创建布局
        QVBoxLayout* layout = new QVBoxLayout(dialog);
        layout->setContentsMargins(5, 5, 5, 5);
        
        // 创建患者列表管理界面
        PatientListWidget* patientListWidget = new PatientListWidget(dialog);
        if (m_pluginContext) {
            patientListWidget->setPluginContext(m_pluginContext);
        }
        layout->addWidget(patientListWidget);
        
        // 创建按钮框
        QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
        layout->addWidget(buttonBox);
        
        // 连接信号
        QObject::connect(buttonBox, &QDialogButtonBox::clicked, dialog, &QDialog::accept);
        QObject::connect(patientListWidget, &PatientListWidget::patientListUpdated, 
                        [this](int totalCount) {
            qDebug() << "Patient list updated, total count:" << totalCount;
        });
        QObject::connect(patientListWidget, &PatientListWidget::editPatientRequested, 
                        [this](const PatientInfo& patient) {
            qDebug() << "Edit request for patient:" << patient.name;
            // 可以调用 showPatientInfoDialog 来编辑患者
        });
        
        // 显示对话框
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
        
        // 刷新患者列表
        patientListWidget->refreshPatientList();
        
        qDebug() << "[PatientManagement] Patient list dialog displayed";
        return true;
        
    } catch (const std::exception& e) {
        logAndEmitError("显示患者列表管理界面", e.what());
        return false;
    }
}

void PatientDatabaseServiceImpl::setPluginContext(ctkPluginContext* context)
{
    m_pluginContext = context;
    qDebug() << "[PatientDatabaseService] Plugin context set:" << (context ? "valid" : "null");
}
