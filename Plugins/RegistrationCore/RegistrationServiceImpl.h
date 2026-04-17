#ifndef REGISTRATION_SERVICE_IMPL_H
#define REGISTRATION_SERVICE_IMPL_H

#include "RegistrationService.h"
#include <QObject>
#include <QHash>
#include <QMutex>
#include <QVariantMap>
#include <QLibrary>
#include <vtkSmartPointer.h>

// CTK Framework
#include <ctkPluginContext.h>
#include <ctkServiceTracker.h>

// MeshGPU DLL forward declarations
namespace mesh_gpu {
    class MeshGPUInterface;
    struct RegistrationParams;
    struct Transform4x4;
}

class vtkMatrix4x4;
class vtkTransform;
class vtkPoints;
class vtkPolyData;
class vtkLandmarkTransform;
class vtkIterativeClosestPointTransform;
class Registration2D3DService;

/**
 * @brief 配准结果记录
 */
struct RegistrationRecord {
    QString registrationId;
    QString type;  // "landmark", "icp", "2d3d"
    vtkSmartPointer<vtkMatrix4x4> transform;
    QVariantMap metadata;
    qint64 timestamp;
    double fre;  // Fiducial Registration Error
    int numPoints;

    // 源数据和目标数据（用于误差计算）
    vtkSmartPointer<vtkPoints> sourcePoints;
    vtkSmartPointer<vtkPoints> targetPoints;

    RegistrationRecord()
        : timestamp(0), fre(0.0), numPoints(0)
    {}
};

/**
 * @brief 配准核心服务实现
 *
 * 实现细节：
 * 1. 使用 vtkLandmarkTransform 进行 Landmark 配准
 * 2. 使用 vtkIterativeClosestPointTransform 进行 ICP 配准
 * 3. 支持 Rigid/Similarity/Affine 变换模式
 * 4. 自动计算 FRE 和 TRE
 * 5. 结果持久化（内存 + 可选文件）
 */
class RegistrationServiceImpl : public RegistrationService
{
    Q_OBJECT
    Q_INTERFACES(RegistrationService)

public:
    explicit RegistrationServiceImpl(QObject* parent = nullptr);
    virtual ~RegistrationServiceImpl();

    /**
     * @brief 设置 CTK 插件上下文（用于服务追踪）
     */
    void setPluginContext(ctkPluginContext* context);

    /**
     * @brief 加载 MeshGPU DLL（GPU-GICP 配准引擎）
     * @param dllPath DLL 路径，空则使用默认路径
     * @return 加载成功返回 true
     */
    bool loadMeshGPUDLL(const QString& dllPath = QString());

    // ==================== RegistrationService 接口实现 ====================

    // Landmark 配准
    vtkSmartPointer<vtkMatrix4x4> performLandmarkRegistration(
        vtkPoints* sourcePoints,
        vtkPoints* targetPoints,
        const QString& registrationId = QString()) override;

    QList<double> performLandmarkRegistrationList(
        const QList<QList<double>>& sourcePoints,
        const QList<QList<double>>& targetPoints) override;

    // ICP 配准
    vtkSmartPointer<vtkMatrix4x4> performICPRegistration(
        vtkPolyData* source,
        vtkPolyData* target,
        vtkMatrix4x4* initialTransform = nullptr,
        int maxIterations = 100,
        const QString& registrationId = QString()) override;

    vtkSmartPointer<vtkMatrix4x4> performICPRegistrationAdvanced(
        vtkPolyData* source,
        vtkPolyData* target,
        const QVariantMap& parameters) override;

    // 配准质量评估
    double computeRegistrationError(vtkPoints* sourcePoints,
                                   vtkPoints* targetPoints) override;

    double computeRegistrationErrorList(
        const QList<QList<double>>& sourcePoints,
        const QList<QList<double>>& targetPoints,
        const QList<double>& transform) override;

    double computeFRE(const QString& registrationId) override;

    double computeTRE(const QString& registrationId,
                     const QList<double>& targetPoint) override;

    QVariantMap evaluateRegistrationQuality(const QString& registrationId) override;

    // 变换矩阵操作
    bool saveRegistrationResult(const QString& registrationId,
                               vtkMatrix4x4* transform,
                               const QVariantMap& metadata = QVariantMap()) override;

    vtkSmartPointer<vtkMatrix4x4> loadRegistrationResult(const QString& registrationId) override;

    QStringList getRegistrationList() const override;
    QVariantMap getRegistrationInfo(const QString& registrationId) const override;
    bool deleteRegistration(const QString& registrationId) override;

    vtkSmartPointer<vtkMatrix4x4> invertMatrix(vtkMatrix4x4* matrix) override;

    vtkSmartPointer<vtkMatrix4x4> multiplyMatrix(vtkMatrix4x4* matrix1,
                                                 vtkMatrix4x4* matrix2) override;

    QList<double> transformPoint(const QList<double>& point,
                                vtkMatrix4x4* transform) override;

    vtkSmartPointer<vtkPoints> transformPoints(vtkPoints* points,
                                               vtkMatrix4x4* transform) override;

    // 2D-3D 配准支持
    vtkSmartPointer<vtkMatrix4x4> perform2D3DRegistration(
        const QString& image2D,
        vtkPolyData* model3D,
        vtkMatrix4x4* initialTransform = nullptr,
        const QVariantMap& parameters = QVariantMap()) override;

    // 工具方法
    QList<double> matrixToList(vtkMatrix4x4* matrix) override;
    vtkSmartPointer<vtkMatrix4x4> listToMatrix(const QList<double>& list) override;

    bool exportMatrix(vtkMatrix4x4* matrix,
                     const QString& filePath,
                     const QString& format = "txt") override;

    vtkSmartPointer<vtkMatrix4x4> importMatrix(const QString& filePath) override;

    QString getLastError() const override;

private:
    // ==================== MeshGPU DLL (GPU-GICP) ====================

    /**
     * @brief 使用 MeshGPU DLL 执行 GPU-GICP 配准
     */
    vtkSmartPointer<vtkMatrix4x4> performGICPRegistration(
        vtkPolyData* source,
        vtkPolyData* target,
        vtkMatrix4x4* initialTransform,
        const QVariantMap& parameters,
        const QString& registrationId);

    /**
     * @brief vtkPolyData 转 mesh_gpu::Point3D 向量
     */
    static std::vector<float> polyDataToFloatArray(vtkPolyData* polyData);

    /**
     * @brief mesh_gpu::Transform4x4 转 vtkMatrix4x4
     */
    static vtkSmartPointer<vtkMatrix4x4> meshGPUTransformToVTK(const float* data16);

    // MeshGPU DLL 函数指针类型
    using CreateMeshGPUFn = mesh_gpu::MeshGPUInterface* (*)();
    using DestroyMeshGPUFn = void (*)(mesh_gpu::MeshGPUInterface*);

    QLibrary m_meshGPULib;
    CreateMeshGPUFn m_createMeshGPU = nullptr;
    DestroyMeshGPUFn m_destroyMeshGPU = nullptr;
    mesh_gpu::MeshGPUInterface* m_meshGPU = nullptr;
    bool m_meshGPULoaded = false;

    // ==================== 原有私有成员 ====================

    /**
     * @brief 生成唯一配准ID
     */
    QString generateRegistrationId(const QString& prefix = "reg");

    /**
     * @brief 验证点集
     */
    bool validatePointSets(vtkPoints* sourcePoints, vtkPoints* targetPoints, int minPoints = 3);

    /**
     * @brief 转换 QList 点集到 vtkPoints
     */
    vtkSmartPointer<vtkPoints> listToVtkPoints(const QList<QList<double>>& pointList);

    /**
     * @brief 计算两个点集之间的 RMS 误差
     * @param sourcePoints 源点集
     * @param targetPoints 目标点集（已变换）
     * @param transform 变换矩阵（可选，如果提供则先变换源点）
     */
    double computeRMSError(vtkPoints* sourcePoints,
                          vtkPoints* targetPoints,
                          vtkMatrix4x4* transform = nullptr);

    /**
     * @brief 应用变换到点
     */
    void transformPoint(double in[3], double out[3], vtkMatrix4x4* matrix);

    /**
     * @brief 查找配准记录
     */
    RegistrationRecord* findRecord(const QString& registrationId);
    const RegistrationRecord* findRecord(const QString& registrationId) const;

    /**
     * @brief 保存配准记录
     */
    void saveRecord(const QString& registrationId, const RegistrationRecord& record);

    /**
     * @brief 评估配准质量等级
     */
    QString evaluateQualityLevel(double fre, double treMax);

    /**
     * @brief 生成配准建议
     */
    QString generateRecommendation(double fre, double treMax, int numPoints);

    /**
     * @brief 获取 Registration2D3D 服务
     */
    Registration2D3DService* getRegistration2D3DService();

    /**
     * @brief 计算配准点的统计 TRE（基于 FRE 和点分布）
     * @param record 配准记录
     * @param targetPoint 目标点
     * @return TRE 估计值
     */
    double computeStatisticalTRE(const RegistrationRecord* record,
                                  const QList<double>& targetPoint);

    /**
     * @brief 计算点到配准质心的距离
     */
    double computeDistanceToFiducialCentroid(const RegistrationRecord* record,
                                              const double point[3]);

    /**
     * @brief 计算配准点的协方差矩阵
     */
    void computeFiducialCovariance(const RegistrationRecord* record,
                                    double centroid[3],
                                    double covariance[3][3]);

private:
    mutable QMutex m_mutex;
    QHash<QString, RegistrationRecord> m_registrations;
    QString m_lastError;

    // CTK 插件上下文
    ctkPluginContext* m_context;

    // 配准参数配置
    int m_defaultLandmarkMode;  // VTK_LANDMARK_RIGIDBODY, VTK_LANDMARK_SIMILARITY, VTK_LANDMARK_AFFINE
    bool m_enableICPCentroids;
    int m_defaultICPMaxIterations;
    int m_defaultICPMaxLandmarks;
};

#endif // REGISTRATION_SERVICE_IMPL_H