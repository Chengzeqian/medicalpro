#ifndef DICOMVIEWERSERVICEIMPL_H
#define DICOMVIEWERSERVICEIMPL_H

#include "DicomViewerService.h"
#include "DicomDataStructures.h"
#include <QObject>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QHash>
#include <QMutex>
#include <QRecursiveMutex>

// CTK Plugin Framework
#ifdef CTK_PLUGIN_FRAMEWORK
#include <ctkPluginContext.h>
#endif

/**
 * @brief DICOM影像查看服务实现类
 * 
 * 实现DICOM影像的数据库存储、文件管理、图像处理等功能
 * 专门针对足踝外科CT影像进行优化
 */
class DicomViewerServiceImpl : public DicomViewerService
{
    Q_OBJECT
    Q_INTERFACES(DicomViewerService)

public:
    explicit DicomViewerServiceImpl(QObject *parent = nullptr);
    virtual ~DicomViewerServiceImpl();

    // 初始化服务
    bool initialize();
    void shutdown();

    // === 病人管理 ===
    bool createDicomPatient(const DicomPatientInfo& patient) override;
    DicomPatientInfo getDicomPatient(int patientId) override;
    QList<DicomPatientInfo> listDicomPatients() override;
    
    // 通过PatientManagement数据库的患者ID获取或创建DICOM患者记录
    int getOrCreatePatientByManagementId(int managementPatientId, const QString& patientName);

    // === 检查管理 ===
    bool createDicomStudy(const DicomStudyInfo& study) override;
    QList<DicomStudyInfo> listStudiesByPatient(int patientId) override;
    DicomStudyInfo getDicomStudy(int studyId) override;

    // === 序列管理 ===
    bool createDicomSeries(const DicomSeriesInfo& series) override;
    QList<DicomSeriesInfo> listSeriesByStudy(int studyId) override;
    DicomSeriesInfo getDicomSeries(int seriesId) override;
    bool deleteSeries(int seriesId) override;

    // === 图像管理 ===
    bool createDicomImage(const DicomImageInfo& image) override;
    QList<DicomImageInfo> listImagesBySeries(int seriesId) override;
    DicomImageInfo getDicomImage(int imageId) override;
    QPixmap loadDicomPixmap(const DicomImageInfo& image, const DicomDisplayParams& params) override;

    // === DICOM文件导入 ===
    bool importDicomFile(const QString& filePath, int patientId) override;
    bool importDicomDirectory(const QString& dirPath, int patientId) override;

    // === 测量和标注 ===
    bool createAnnotation(const DicomAnnotation& annotation) override;
    QList<DicomAnnotation> listAnnotationsByImage(int imageId) override;
    bool deleteAnnotation(int annotationId) override;
    bool updateAnnotation(const DicomAnnotation& annotation) override;

    // === 显示参数管理 ===
    bool saveDisplayParams(int imageId, const DicomDisplayParams& params) override;
    DicomDisplayParams getDisplayParams(int imageId) override;

    // === 预设窗宽窗位 ===
    QList<QPair<QString, QPair<int, int>>> getAnkleCtPresets() override;

    // === DICOM查看器组件 ===
    QPixmap loadDicomFromFile(const QString& filePath, int windowCenter = 300, int windowWidth = 1500) override;
    void requestImageData(int seriesId, int imageIndex, const DicomDisplayParams& params) override;

    // === Widget工厂方法 ===
    /**
     * @brief 创建DICOM查看器Widget
     * @param parent 父Widget
     * @return Widget指针
     */
    QWidget* createDicomViewerWidget(QWidget* parent = nullptr) override;

    // === VTK渲染控制 ===
    void pauseRendering() override;
    void resumeRendering() override;

    // CTK Context管理（用于Widget初始化）
#ifdef CTK_PLUGIN_FRAMEWORK
    void setPluginContext(ctkPluginContext* context) { m_pluginContext = context; }
#endif

private slots:
    void onDatabaseError(const QSqlError& error);

private:
    // 数据库管理
    bool setupDatabase();
    bool createDatabaseTables();
    QSqlDatabase getDatabase();
    
    // 数据库表创建
    bool createPatientsTable();
    bool createStudiesTable();
    bool createSeriesTable();
    bool createImagesTable();
    bool createAnnotationsTable();
    bool createDisplayParamsTable();
    
    // 数据库架构迁移
    bool migrateDatabaseSchema();
    
    // DICOM文件解析
    bool parseDicomFile(const QString& filePath, DicomImageInfo& imageInfo);
    
    // 图像处理
    QPixmap processRawPixelData(const QByteArray& pixelData, int rows, int columns, 
                               const DicomDisplayParams& params);
    QPixmap applyWindowLevel(const QPixmap& pixmap, int windowCenter, int windowWidth);
    QPixmap applyTransformations(const QPixmap& pixmap, const DicomDisplayParams& params);
    
    // 工具函数
    QString generateUID();
    bool validateDicomFile(const QString& filePath);
    void logError(const QString& operation, const QString& error);
    
    // 图像加载辅助方法
    QPixmap loadDicomWithVTK(const QString& filePath, int windowCenter, int windowWidth);

private:
    QSqlDatabase m_database;
    QString m_databaseName;
    QRecursiveMutex m_dbMutex;  // 使用递归互斥锁避免死锁
    
    // 缓存
    QHash<int, DicomPatientInfo> m_patientCache;
    QHash<int, DicomStudyInfo> m_studyCache;
    QHash<int, DicomSeriesInfo> m_seriesCache;
    QHash<int, DicomImageInfo> m_imageCache;
    QHash<int, DicomDisplayParams> m_displayParamsCache;
    
    // 足踝CT预设窗宽窗位
    QList<QPair<QString, QPair<int, int>>> m_ankleCtPresets;
    
    bool m_initialized;

    // VTK渲染控制状态
    bool m_renderingPaused;
    QList<QWidget*> m_createdWidgets;  // 跟踪创建的Widget

    // CTK上下文（用于Widget创建）
#ifdef CTK_PLUGIN_FRAMEWORK
    ctkPluginContext* m_pluginContext;
#endif
};

#endif // DICOMVIEWERSERVICEIMPL_H
