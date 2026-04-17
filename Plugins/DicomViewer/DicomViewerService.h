#ifndef DICOMVIEWERSERVICE_H
#define DICOMVIEWERSERVICE_H

#include <QObject>
#include <QString>
#include <QList>
#include <QPixmap>
#include "Framework/ImageDataTransfer.h"
#include "DicomDataStructures.h"

class QWidget;

/**
 * @brief DICOM影像查看服务接口
 * 
 * 提供DICOM影像的加载、显示、测量、标注等功能
 * 专门针对足踝外科CT影像进行优化
 */
class DicomViewerService : public QObject
{
    Q_OBJECT

public:
    explicit DicomViewerService(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~DicomViewerService() = default;

    // === 病人管理 ===
    /**
     * @brief 创建DICOM病人记录
     */
    virtual bool createDicomPatient(const DicomPatientInfo& patient) = 0;
    
    /**
     * @brief 获取DICOM病人信息
     */
    virtual DicomPatientInfo getDicomPatient(int patientId) = 0;
    
    /**
     * @brief 获取所有DICOM病人列表
     */
    virtual QList<DicomPatientInfo> listDicomPatients() = 0;

    // === 检查管理 ===
    /**
     * @brief 创建DICOM检查记录
     */
    virtual bool createDicomStudy(const DicomStudyInfo& study) = 0;
    
    /**
     * @brief 获取指定病人的所有检查
     */
    virtual QList<DicomStudyInfo> listStudiesByPatient(int patientId) = 0;
    
    /**
     * @brief 获取检查详细信息
     */
    virtual DicomStudyInfo getDicomStudy(int studyId) = 0;

    // === 序列管理 ===
    /**
     * @brief 创建DICOM序列记录
     */
    virtual bool createDicomSeries(const DicomSeriesInfo& series) = 0;
    
    /**
     * @brief 获取指定检查的所有序列
     */
    virtual QList<DicomSeriesInfo> listSeriesByStudy(int studyId) = 0;
    
    /**
     * @brief 获取序列详细信息
     */
    virtual DicomSeriesInfo getDicomSeries(int seriesId) = 0;
    
    /**
     * @brief 删除DICOM序列及其所有图像
     * @param seriesId 序列ID
     * @return 删除成功返回true，否则返回false
     */
    virtual bool deleteSeries(int seriesId) = 0;

    // === 图像管理 ===
    /**
     * @brief 创建DICOM图像记录
     */
    virtual bool createDicomImage(const DicomImageInfo& image) = 0;
    
    /**
     * @brief 获取指定序列的所有图像
     */
    virtual QList<DicomImageInfo> listImagesBySeries(int seriesId) = 0;
    
    /**
     * @brief 获取图像详细信息
     */
    virtual DicomImageInfo getDicomImage(int imageId) = 0;
    
    /**
     * @brief 加载DICOM图像像素数据
     */
    virtual QPixmap loadDicomPixmap(const DicomImageInfo& image, const DicomDisplayParams& params) = 0;

    // === DICOM文件导入 ===
    /**
     * @brief 从文件导入DICOM数据
     */
    virtual bool importDicomFile(const QString& filePath, int patientId) = 0;
    
    /**
     * @brief 从目录批量导入DICOM文件
     */
    virtual bool importDicomDirectory(const QString& dirPath, int patientId) = 0;

    // === 测量和标注 ===
    /**
     * @brief 创建测量标注
     */
    virtual bool createAnnotation(const DicomAnnotation& annotation) = 0;
    
    /**
     * @brief 获取指定图像的所有标注
     */
    virtual QList<DicomAnnotation> listAnnotationsByImage(int imageId) = 0;
    
    /**
     * @brief 删除标注
     */
    virtual bool deleteAnnotation(int annotationId) = 0;
    
    /**
     * @brief 更新标注
     */
    virtual bool updateAnnotation(const DicomAnnotation& annotation) = 0;

    // === 显示参数管理 ===
    /**
     * @brief 保存显示参数
     */
    virtual bool saveDisplayParams(int imageId, const DicomDisplayParams& params) = 0;
    
    /**
     * @brief 获取显示参数
     */
    virtual DicomDisplayParams getDisplayParams(int imageId) = 0;

    // === 预设窗宽窗位 ===
    /**
     * @brief 获取足踝CT预设窗宽窗位
     */
    virtual QList<QPair<QString, QPair<int, int>>> getAnkleCtPresets() = 0;

    // === DICOM查看器组件 ===
    /**
     * @brief 从文件路径加载DICOM图像为QPixmap
     * @param filePath DICOM文件路径
     * @param windowCenter 窗位
     * @param windowWidth 窗宽
     * @return 处理后的QPixmap，如果加载失败返回空QPixmap
     */
    virtual QPixmap loadDicomFromFile(const QString& filePath, int windowCenter = 300, int windowWidth = 1500) = 0;
    virtual void requestImageData(int seriesId, int imageIndex, const DicomDisplayParams& params) = 0;

    // === Widget工厂方法 ===
    /**
     * @brief 创建DICOM查看器Widget
     * @param parent 父Widget（由调用者管理生命周期）
     * @return Widget指针，主程序只持有QWidget*
     * @note Widget在插件内部实现，避免主程序链接插件符号的问题
     */
    virtual QWidget* createDicomViewerWidget(QWidget* parent = nullptr) = 0;

    // === VTK渲染控制（防闪烁） ===
    /**
     * @brief 暂停VTK渲染
     * @note 在页面切换前调用，防止隐藏的VTK Widget继续渲染导致闪烁
     */
    virtual void pauseRendering() = 0;

    /**
     * @brief 恢复VTK渲染
     * @note 在页面切换后调用
     */
    virtual void resumeRendering() = 0;

signals:
    /**
     * @brief DICOM数据加载完成信号
     */
    void dicomDataLoaded(int studyId);
    
    /**
     * @brief 图像显示参数改变信号
     */
    void displayParamsChanged(int imageId, const DicomDisplayParams& params);
    
    /**
     * @brief 标注创建信号
     */
    void annotationCreated(const DicomAnnotation& annotation);
    
    /**
     * @brief 标注删除信号
     */
    void annotationDeleted(int annotationId);
    
    /**
     * @brief 图像数据准备就绪信号
     * @param seriesId 序列ID
     * @param imageIndex 图像索引
     * @param data 图像数据
     */
    void imageDataReady(int seriesId, int imageIndex, const ImageData& data);
};

// 定义服务接口ID，用于CTK服务注册
#define DicomViewerService_iid "org.medicalpro.DicomViewerService"
Q_DECLARE_INTERFACE(DicomViewerService, DicomViewerService_iid)

#endif // DICOMVIEWERSERVICE_H
