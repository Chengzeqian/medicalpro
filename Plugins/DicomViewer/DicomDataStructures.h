#ifndef DICOMDATASTRUCTURES_H
#define DICOMDATASTRUCTURES_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QPixmap>
#include <QList>
#include <QVariant>

// DICOM图像信息结构
struct DicomImageInfo
{
    int id = -1;
    int seriesId = -1;
    QString instanceUID;        // DICOM实例UID
    QString sopClassUID;        // SOP类UID
    int instanceNumber = 0;     // 实例编号
    QString imagePath;          // 图像文件路径
    int rows = 0;              // 图像行数
    int columns = 0;           // 图像列数
    double pixelSpacing[2] = {1.0, 1.0};  // 像素间距
    double sliceThickness = 1.0;          // 层厚
    QString imagePosition[3];   // 图像位置
    QString imageOrientation[6]; // 图像方向
    int windowCenter = 0;       // 窗位
    int windowWidth = 0;        // 窗宽
    QDateTime acquisitionTime;  // 采集时间
    QString notes;              // 备注
    
    bool isValid() const {
        // 注意：对于新记录，id可以是-1（尚未插入数据库）
        // seriesId必须有效（关联的Series必须已存在）
        return seriesId >= 0 && !instanceUID.isEmpty();
    }
};

// DICOM序列信息结构
struct DicomSeriesInfo
{
    int id = -1;
    int studyId = -1;
    QString seriesUID;          // 序列UID
    QString seriesNumber;       // 序列号
    QString seriesDescription;  // 序列描述
    QString modality;           // 模态(CT, MR, XA等)
    QString bodyPartExamined;   // 检查部位
    QString scanningSequence;   // 扫描序列
    int numberOfImages = 0;     // 图像数量
    double sliceThickness = 1.0; // 层厚
    double pixelSpacing[2] = {1.0, 1.0}; // 像素间距
    QDateTime seriesTime;       // 序列时间
    QString notes;              // 备注
    
    QList<DicomImageInfo> images; // 该序列的所有图像
    
    bool isValid() const {
        // 注意：对于新记录，id可以是-1（尚未插入数据库）
        // studyId必须有效（关联的Study必须已存在）
        return studyId >= 0 && !seriesUID.isEmpty();
    }
};

// DICOM检查信息结构
struct DicomStudyInfo
{
    int id = -1;
    int patientId = -1;
    QString studyUID;           // 检查UID
    QString studyID;            // 检查ID
    QString studyDescription;   // 检查描述
    QDateTime studyDate;        // 检查日期
    QDateTime studyTime;        // 检查时间
    QString referringPhysician; // 申请医生
    QString performingPhysician; // 执行医生
    QString studyComments;      // 检查备注
    QString accessionNumber;    // 检查号
    int numberOfSeries = 0;     // 序列数量
    QString studyStatus;        // 检查状态
    
    QList<DicomSeriesInfo> series; // 该检查的所有序列
    
    bool isValid() const {
        // 注意：对于新记录，id可以是-1（尚未插入数据库）
        // 只验证必需的业务字段
        return patientId >= 0 && !studyUID.isEmpty();
    }
};

// DICOM病人信息结构
struct DicomPatientInfo
{
    int id = -1;
    QString patientID;          // 病人ID
    QString patientName;        // 病人姓名
    QString patientBirthDate;   // 出生日期
    QString patientSex;         // 性别
    QString patientAge;         // 年龄
    QString patientWeight;      // 体重
    QString patientHeight;      // 身高
    QString patientComments;    // 病人备注
    QDateTime createdAt;        // 创建时间
    QDateTime updatedAt;        // 更新时间
    
    QList<DicomStudyInfo> studies; // 该病人的所有检查
    
    bool isValid() const {
        return id >= 0 && !patientID.isEmpty() && !patientName.isEmpty();
    }
};

// DICOM测量标注结构
struct DicomAnnotation
{
    int id = -1;
    int imageId = -1;
    QString annotationType;     // 标注类型: distance, angle, area, text
    QString annotationText;     // 标注文本
    QList<QPointF> points;      // 标注点坐标
    double measureValue = 0.0;  // 测量值
    QString measureUnit;        // 测量单位
    QString color;              // 标注颜色
    int lineWidth = 2;          // 线宽
    QDateTime createdAt;        // 创建时间
    QString createdBy;          // 创建者
    
    bool isValid() const {
        return id >= 0 && imageId >= 0 && !annotationType.isEmpty();
    }
};

// DICOM显示参数结构
struct DicomDisplayParams
{
    int windowCenter = 0;       // 窗位
    int windowWidth = 0;        // 窗宽
    double zoom = 1.0;          // 缩放比例
    QPointF panOffset;          // 平移偏移
    bool invertImage = false;   // 图像反转
    bool flipHorizontal = false; // 水平翻转
    bool flipVertical = false;  // 垂直翻转
    double rotation = 0.0;      // 旋转角度
    QString colorMap;           // 颜色映射
    bool showAnnotations = true; // 显示标注
    bool showMeasurements = true; // 显示测量
};

// 注册Qt元类型系统
Q_DECLARE_METATYPE(DicomImageInfo)
Q_DECLARE_METATYPE(DicomSeriesInfo)
Q_DECLARE_METATYPE(DicomStudyInfo)
Q_DECLARE_METATYPE(DicomPatientInfo)
Q_DECLARE_METATYPE(DicomAnnotation)
Q_DECLARE_METATYPE(DicomDisplayParams)

#endif // DICOMDATASTRUCTURES_H
