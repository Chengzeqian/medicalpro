#pragma once
#include <QString>
#include <QDateTime>
#include <QMetaType>
#include <QList>

// 基础患者信息
struct PatientInfo {
    int patientId = 0;
    QString name;
    int age = 0;
    QString gender;              // 男/女/其他
    QString phone;               // 手机号
    QString idCard;              // 身份证号
    QString address;             // 地址
    QString emergencyContact;    // 紧急联系人
    QString emergencyPhone;      // 紧急联系人电话
    QString medicalHistory;      // 既往史
    QString allergies;           // 过敏史
    QString currentMedications;  // 现用药物
    QDateTime registrationDate;  // 初诊/建档日期
    QDateTime lastVisitDate;     // 最近就诊日期
    QString notes;               // 备注
};

// 影像信息
struct PatientImageInfo {
    int imageId = 0;
    int patientId = 0;
    QString imagePath;           // 影像文件路径（DICOM/NIfTI/PNG等）
    QString imageType;           // CT/MR/XRay/US 等
    QString bodyPart;            // 部位
    QDateTime scanDate;          // 扫描日期
    QString scanParameters;      // 扫描参数说明
    QString description;         // 描述
    QString radiologistNotes;    // 影像科医师备注
    bool isProcessed = false;    // 是否已处理
    QString processingNotes;     // 处理备注
};

// 手术记录
struct SurgeryRecord {
    int surgeryId = 0;
    int patientId = 0;
    QString surgeryType;         // 手术类型
    QDateTime surgeryDate;       // 手术日期
    QString surgeon;             // 主刀
    QString assistants;          // 助手
    QString preOpDiagnosis;      // 术前诊断
    QString postOpDiagnosis;     // 术后诊断
    QString procedure;           // 手术过程/操作
    QString complications;       // 并发症
    QString notes;               // 备注
    QString outcome;             // 结局/效果
};

// 搜索条件
struct PatientSearchCriteria {
    QString nameFilter;
    QString phoneFilter;
    QString idCardFilter;
    int ageMin = -1;
    int ageMax = -1;
    QString genderFilter;
    QDateTime registrationDateStart;
    QDateTime registrationDateEnd;
    QString medicalHistoryFilter;

    bool isEmpty() const {
        return nameFilter.isEmpty() && phoneFilter.isEmpty() && idCardFilter.isEmpty() &&
               ageMin < 0 && ageMax < 0 && genderFilter.isEmpty() &&
               !registrationDateStart.isValid() && !registrationDateEnd.isValid() &&
               medicalHistoryFilter.isEmpty();
    }
};

Q_DECLARE_METATYPE(PatientInfo)
Q_DECLARE_METATYPE(PatientImageInfo)
Q_DECLARE_METATYPE(SurgeryRecord)
Q_DECLARE_METATYPE(PatientSearchCriteria)

