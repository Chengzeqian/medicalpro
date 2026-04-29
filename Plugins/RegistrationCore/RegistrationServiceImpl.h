#ifndef REGISTRATION_SERVICE_IMPL_H
#define REGISTRATION_SERVICE_IMPL_H

#include "RegistrationService.h"

#include <QObject>
#include <QHash>
#include <QMutex>
#include <QVariantMap>
#include <QLibrary>
#include <vtkSmartPointer.h>

class PlatformServiceRegistry;

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

// Stores one persisted registration result.
struct RegistrationRecord {
    QString registrationId;
    QString type;  // "landmark", "icp", "2d3d"
    vtkSmartPointer<vtkMatrix4x4> transform;
    QVariantMap metadata;
    qint64 timestamp;
    double fre;  // Fiducial Registration Error
    int numPoints;

    // Raw point sets used for error evaluation.
    vtkSmartPointer<vtkPoints> sourcePoints;
    vtkSmartPointer<vtkPoints> targetPoints;

    RegistrationRecord()
        : timestamp(0), fre(0.0), numPoints(0)
    {}
};

// Concrete implementation of the registration service contract.
class RegistrationServiceImpl : public registration_core::RegistrationService
{
    Q_OBJECT
    Q_INTERFACES(registration_core::RegistrationService)

public:
    explicit RegistrationServiceImpl(QObject* parent = nullptr);
    virtual ~RegistrationServiceImpl();

    // Inject the platform service registry used for service lookup.
    void setServiceRegistry(PlatformServiceRegistry* serviceRegistry);

    // Load the MeshGPU DLL used by the GPU-GICP registration path.
    bool loadMeshGPUDLL(const QString& dllPath = QString());

    // RegistrationService interface
    vtkSmartPointer<vtkMatrix4x4> performLandmarkRegistration(
        vtkPoints* sourcePoints,
        vtkPoints* targetPoints,
        const QString& registrationId = QString()) override;

    QList<double> performLandmarkRegistrationList(
        const QList<QList<double>>& sourcePoints,
        const QList<QList<double>>& targetPoints) override;

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

    vtkSmartPointer<vtkMatrix4x4> perform2D3DRegistration(
        const QString& image2D,
        vtkPolyData* model3D,
        vtkMatrix4x4* initialTransform = nullptr,
        const QVariantMap& parameters = QVariantMap()) override;

    QList<double> matrixToList(vtkMatrix4x4* matrix) override;
    vtkSmartPointer<vtkMatrix4x4> listToMatrix(const QList<double>& list) override;

    bool exportMatrix(vtkMatrix4x4* matrix,
                      const QString& filePath,
                      const QString& format = "txt") override;

    vtkSmartPointer<vtkMatrix4x4> importMatrix(const QString& filePath) override;

    QString getLastError() const override;

private:
    // MeshGPU DLL (GPU-GICP)
    vtkSmartPointer<vtkMatrix4x4> performGICPRegistration(
        vtkPolyData* source,
        vtkPolyData* target,
        vtkMatrix4x4* initialTransform,
        const QVariantMap& parameters,
        const QString& registrationId);

    // Convert vtkPolyData into the float buffer expected by MeshGPU.
    static std::vector<float> polyDataToFloatArray(vtkPolyData* polyData);

    // Convert a MeshGPU transform array into vtkMatrix4x4.
    static vtkSmartPointer<vtkMatrix4x4> meshGPUTransformToVTK(const float* data16);

    // MeshGPU DLL function pointer types
    using CreateMeshGPUFn = mesh_gpu::MeshGPUInterface* (*)();
    using DestroyMeshGPUFn = void (*)(mesh_gpu::MeshGPUInterface*);

    QLibrary m_meshGPULib;
    CreateMeshGPUFn m_createMeshGPU = nullptr;
    DestroyMeshGPUFn m_destroyMeshGPU = nullptr;
    mesh_gpu::MeshGPUInterface* m_meshGPU = nullptr;
    bool m_meshGPULoaded = false;

    // Internal helpers
    QString generateRegistrationId(const QString& prefix = "reg");

    bool validatePointSets(vtkPoints* sourcePoints, vtkPoints* targetPoints, int minPoints = 3);

    vtkSmartPointer<vtkPoints> listToVtkPoints(const QList<QList<double>>& pointList);

    // Compute RMS error between point sets, optionally after applying a transform.
    double computeRMSError(vtkPoints* sourcePoints,
                           vtkPoints* targetPoints,
                           vtkMatrix4x4* transform = nullptr);

    void transformPoint(double in[3], double out[3], vtkMatrix4x4* matrix);

    RegistrationRecord* findRecord(const QString& registrationId);
    const RegistrationRecord* findRecord(const QString& registrationId) const;

    void saveRecord(const QString& registrationId, const RegistrationRecord& record);

    QString evaluateQualityLevel(double fre, double treMax);

    QString generateRecommendation(double fre, double treMax, int numPoints);

    Registration2D3DService* getRegistration2D3DService();

    // Estimate TRE from FRE and fiducial distribution statistics.
    double computeStatisticalTRE(const RegistrationRecord* record,
                                 const QList<double>& targetPoint);

    double computeDistanceToFiducialCentroid(const RegistrationRecord* record,
                                             const double point[3]);

    void computeFiducialCovariance(const RegistrationRecord* record,
                                   double centroid[3],
                                   double covariance[3][3]);

private:
    mutable QMutex m_mutex;
    QHash<QString, RegistrationRecord> m_registrations;
    QString m_lastError;

    // Service registry bridge used to discover dependent services.
    PlatformServiceRegistry* m_serviceRegistry;

    // Registration parameter defaults.
    int m_defaultLandmarkMode;  // VTK_LANDMARK_RIGIDBODY, VTK_LANDMARK_SIMILARITY, VTK_LANDMARK_AFFINE
    bool m_enableICPCentroids;
    int m_defaultICPMaxIterations;
    int m_defaultICPMaxLandmarks;
};

#endif // REGISTRATION_SERVICE_IMPL_H
