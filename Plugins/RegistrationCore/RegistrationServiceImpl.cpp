#include "RegistrationServiceImpl.h"
#include <QDebug>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutexLocker>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <cmath>
#include <limits>

#include <vtkMatrix4x4.h>
#include <vtkTransform.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkLandmarkTransform.h>
#include <vtkIterativeClosestPointTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkMath.h>
#include <vtkPointData.h>
#include <vtkCell.h>

#include "Framework/Platform/Kernel/platform_service_registry.h"

// MeshGPU DLL header (CUDA-free, pure C++ interface)
#include "algorithms/meshgpu/include/mesh_gpu_runtime_api.h"

// Registration2D3D 插件头文件
#include "../Registration2D3D/Registration2D3DService.h"
#include "../Registration2D3D/Registration2D3DDataStructures.h"

RegistrationServiceImpl::RegistrationServiceImpl(QObject* parent)
    : registration_core::RegistrationService(parent)
    , m_serviceRegistry(nullptr)
    , m_defaultLandmarkMode(VTK_LANDMARK_RIGIDBODY)
    , m_enableICPCentroids(true)
    , m_defaultICPMaxIterations(100)
    , m_defaultICPMaxLandmarks(200)
{
    qDebug() << "[RegistrationService] Initialized";
}

void RegistrationServiceImpl::setServiceRegistry(PlatformServiceRegistry* serviceRegistry)
{
    m_serviceRegistry = serviceRegistry;
}

Registration2D3DService* RegistrationServiceImpl::getRegistration2D3DService()
{
    if (!m_serviceRegistry) {
        qWarning() << "[RegistrationService] Platform service registry unavailable";
        return nullptr;
    }

    auto* service = qobject_cast<Registration2D3DService*>(
        m_serviceRegistry->service(QStringLiteral("Registration2D3DService")));
    if (!service) {
        qWarning() << "[RegistrationService] Registration2D3D service not available";
        return nullptr;
    }

    return service;
}

RegistrationServiceImpl::~RegistrationServiceImpl()
{
    // 清理 MeshGPU DLL
    if (m_meshGPU && m_destroyMeshGPU) {
        m_destroyMeshGPU(m_meshGPU);
        m_meshGPU = nullptr;
    }
    if (m_meshGPULib.isLoaded()) {
        m_meshGPULib.unload();
    }

    QMutexLocker locker(&m_mutex);
    m_registrations.clear();
}

// ==================== MeshGPU DLL 集成 ====================

bool RegistrationServiceImpl::loadMeshGPUDLL(const QString& dllPath)
{
    if (m_meshGPULoaded) return true;

    QString path = dllPath;
    if (path.isEmpty()) {
        // 默认路径：ICPtry/MeshGPU/build/Release/MeshGPULib.dll
        path = QCoreApplication::applicationDirPath() + "/MeshGPULib.dll";
    }

    qDebug() << "[RegistrationService] Attempting MeshGPU DLL load from:" << path;
    m_meshGPULib.setFileName(path);
    if (!m_meshGPULib.load()) {
        qWarning() << "[RegistrationService] MeshGPU DLL load failed:" << m_meshGPULib.errorString();
        return false;
    }

    m_createMeshGPU = reinterpret_cast<CreateMeshGPUFn>(m_meshGPULib.resolve("CreateMeshGPURuntimeApi"));
    m_destroyMeshGPU = reinterpret_cast<DestroyMeshGPUFn>(m_meshGPULib.resolve("DestroyMeshGPURuntimeApi"));

    if (!m_createMeshGPU || !m_destroyMeshGPU) {
        qWarning() << "[RegistrationService] MeshGPU DLL: failed to resolve runtime factory functions";
        m_meshGPULib.unload();
        return false;
    }

    m_meshGPU = m_createMeshGPU();
    if (!m_meshGPU) {
        qWarning() << "[RegistrationService] MeshGPU DLL: CreateMeshGPURuntimeApi returned null";
        m_meshGPULib.unload();
        return false;
    }

    m_meshGPULoaded = true;
    qDebug() << "[RegistrationService] MeshGPU DLL loaded from:" << path;
    return true;
}

std::vector<float> RegistrationServiceImpl::polyDataToFloatArray(vtkPolyData* polyData)
{
    std::vector<float> result;
    if (!polyData) return result;

    vtkIdType n = polyData->GetNumberOfPoints();
    result.resize(n * 3);
    for (vtkIdType i = 0; i < n; ++i) {
        double p[3];
        polyData->GetPoint(i, p);
        result[i * 3 + 0] = static_cast<float>(p[0]);
        result[i * 3 + 1] = static_cast<float>(p[1]);
        result[i * 3 + 2] = static_cast<float>(p[2]);
    }
    return result;
}

vtkSmartPointer<vtkMatrix4x4> RegistrationServiceImpl::meshGPUTransformToVTK(const float* data16)
{
    vtkSmartPointer<vtkMatrix4x4> matrix = vtkSmartPointer<vtkMatrix4x4>::New();
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            matrix->SetElement(i, j, static_cast<double>(data16[i * 4 + j]));
    return matrix;
}

vtkSmartPointer<vtkMatrix4x4> RegistrationServiceImpl::performGICPRegistration(
    vtkPolyData* source,
    vtkPolyData* target,
    vtkMatrix4x4* initialTransform,
    const QVariantMap& parameters,
    const QString& registrationId)
{
    if (!m_meshGPULoaded) {
        qWarning() << "[RegistrationService] GICP: MeshGPU DLL not loaded, falling back to VTK ICP";
        return nullptr; // caller will fallback
    }

    emit registrationStarted(registrationId, "gicp");
    QElapsedTimer timer;
    timer.start();

    try {
        qDebug() << "[RegistrationService] GPU-GICP stage: begin"
                 << "registrationId=" << registrationId;
        // 将 target mesh 加载到 MeshGPU
        // 先尝试从文件加载（如果参数中提供了路径）
        QString targetMeshPath = parameters.value("targetMeshPath").toString();
        if (!targetMeshPath.isEmpty()) {
            qDebug() << "[RegistrationService] GPU-GICP stage: loadTargetMesh(file)"
                     << "path=" << targetMeshPath;
            if (!m_meshGPU->loadTargetMesh(targetMeshPath.toStdString())) {
                qWarning() << "[RegistrationService] GICP: failed to load target mesh from file";
                return nullptr;
            }
            qDebug() << "[RegistrationService] GPU-GICP stage: loadTargetMesh(file) done";
        } else if (!m_meshGPU->hasTargetMesh()) {
            // 从 vtkPolyData 转换点云设置为 target
            // MeshGPU 需要 PLY 文件或 vertices+normals+triangles
            // 这里用 setTargetMesh 接口
            vtkIdType nVerts = target->GetNumberOfPoints();
            vtkIdType nTris = target->GetNumberOfCells();
            qDebug() << "[RegistrationService] GPU-GICP stage: setTargetMesh(begin)"
                     << "vertices=" << nVerts
                     << "cells=" << nTris;

            std::vector<mesh_gpu::Point3D> vertices(nVerts);
            std::vector<mesh_gpu::Normal3D> normals(nVerts);
            std::vector<std::array<int, 3>> triangles;

            for (vtkIdType i = 0; i < nVerts; ++i) {
                double p[3];
                target->GetPoint(i, p);
                vertices[i] = {static_cast<float>(p[0]), static_cast<float>(p[1]), static_cast<float>(p[2])};
            }

            // 法向量
            vtkDataArray* normalArray = target->GetPointData() ? target->GetPointData()->GetNormals() : nullptr;
            if (normalArray) {
                for (vtkIdType i = 0; i < nVerts; ++i) {
                    double n[3];
                    normalArray->GetTuple(i, n);
                    normals[i] = {static_cast<float>(n[0]), static_cast<float>(n[1]), static_cast<float>(n[2])};
                }
            }

            // 三角面
            triangles.reserve(nTris);
            for (vtkIdType i = 0; i < nTris; ++i) {
                vtkCell* cell = target->GetCell(i);
                if (cell && cell->GetNumberOfPoints() == 3) {
                    triangles.push_back({
                        static_cast<int>(cell->GetPointId(0)),
                        static_cast<int>(cell->GetPointId(1)),
                        static_cast<int>(cell->GetPointId(2))
                    });
                }
            }

            float cellSize = parameters.value("cellSize", 1.0).toFloat();
            qDebug() << "[RegistrationService] GPU-GICP stage: setTargetMesh(call)"
                     << "triangleCount=" << triangles.size()
                     << "cellSize=" << cellSize;
            if (!m_meshGPU->setTargetMesh(vertices, normals, triangles, cellSize)) {
                qWarning() << "[RegistrationService] GICP: failed to set target mesh";
                return nullptr;
            }
            qDebug() << "[RegistrationService] GPU-GICP stage: setTargetMesh(done)";
        } else {
            qDebug() << "[RegistrationService] GPU-GICP stage: target mesh already cached";
        }

        // 设置源点云
        vtkIdType nSource = source->GetNumberOfPoints();
        std::vector<mesh_gpu::Point3D> sourcePoints(nSource);

        // 如果有初始变换，先应用
        if (initialTransform) {
            for (vtkIdType i = 0; i < nSource; ++i) {
                double p[3], out[3];
                source->GetPoint(i, p);
                double pt[4] = {p[0], p[1], p[2], 1.0};
                double res[4];
                initialTransform->MultiplyPoint(pt, res);
                sourcePoints[i] = {static_cast<float>(res[0]), static_cast<float>(res[1]), static_cast<float>(res[2])};
            }
        } else {
            for (vtkIdType i = 0; i < nSource; ++i) {
                double p[3];
                source->GetPoint(i, p);
                sourcePoints[i] = {static_cast<float>(p[0]), static_cast<float>(p[1]), static_cast<float>(p[2])};
            }
        }

        qDebug() << "[RegistrationService] GPU-GICP stage: setSourcePointCloud(call)"
                 << "sourcePoints=" << nSource;
        if (!m_meshGPU->setSourcePointCloud(sourcePoints)) {
            qWarning() << "[RegistrationService] GICP: failed to set source point cloud";
            return nullptr;
        }
        qDebug() << "[RegistrationService] GPU-GICP stage: setSourcePointCloud(done)";

        // 配置配准参数
        mesh_gpu::RegistrationParams regParams;
        regParams.max_iterations = parameters.value("maxIterations", 50).toInt();
        regParams.convergence_threshold = parameters.value("convergenceThreshold", 1e-6f).toFloat();
        regParams.distance_threshold = parameters.value("distanceThreshold", 10.0f).toFloat();
        regParams.use_point_to_plane = parameters.value("usePointToPlane", true).toBool();
        regParams.verbose = parameters.value("verbose", false).toBool();

        int cwMode = parameters.value("curvatureWeightMode", 0).toInt();
        regParams.curvature_weight_mode = static_cast<mesh_gpu::CurvatureWeightMode>(cwMode);

        // 执行配准
        bool useRotationSearch = parameters.value("useRotationSearch", false).toBool();
        mesh_gpu::RuntimeRegistrationResult result;

        if (useRotationSearch) {
            qDebug() << "[RegistrationService] GPU-GICP stage: runRegistrationWithRotationSearch(call)";
            result = m_meshGPU->runRegistrationWithRotationSearch(
                mesh_gpu::RotationSearchParams(), regParams);
        } else {
            qDebug() << "[RegistrationService] GPU-GICP stage: runRegistration(call)";
            result = m_meshGPU->runRegistration(regParams);
        }
        qDebug() << "[RegistrationService] GPU-GICP stage: registration(done)"
                 << "rmse=" << result.rmse
                 << "iterations=" << result.iterations
                 << "converged=" << result.converged;

        qint64 elapsedMs = timer.elapsed();

        // 转换结果
        vtkSmartPointer<vtkMatrix4x4> gicpMatrix = meshGPUTransformToVTK(result.transform.data);

        // 如果有初始变换，合成最终变换
        vtkSmartPointer<vtkMatrix4x4> finalMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
        if (initialTransform) {
            vtkMatrix4x4::Multiply4x4(gicpMatrix, initialTransform, finalMatrix);
        } else {
            finalMatrix->DeepCopy(gicpMatrix);
        }

        // 保存配准记录
        RegistrationRecord record;
        record.registrationId = registrationId;
        record.type = "gicp";
        record.transform = finalMatrix;
        record.timestamp = QDateTime::currentMSecsSinceEpoch();
        record.fre = static_cast<double>(result.rmse);
        record.numPoints = static_cast<int>(nSource);

        QVariantMap metadata;
        metadata["algorithm"] = "GPU-GICP";
        metadata["iterations"] = result.iterations;
        metadata["converged"] = result.converged;
        metadata["rmse"] = static_cast<double>(result.rmse);
        metadata["elapsedMs"] = elapsedMs;
        metadata["sourcePoints"] = static_cast<int>(nSource);
        metadata["targetPoints"] = static_cast<int>(target->GetNumberOfPoints());
        metadata["useRotationSearch"] = useRotationSearch;
        record.metadata = metadata;

        saveRecord(registrationId, record);

        qDebug() << "[RegistrationService] GPU-GICP completed:"
                 << "ID=" << registrationId
                 << "RMSE=" << result.rmse << "mm"
                 << "Iterations=" << result.iterations
                 << "Time=" << elapsedMs << "ms"
                 << "Converged=" << result.converged;

        QVariantMap resultInfo;
        resultInfo["registrationId"] = registrationId;
        resultInfo["type"] = "gicp";
        resultInfo["rmse"] = static_cast<double>(result.rmse);
        resultInfo["iterations"] = result.iterations;
        resultInfo["converged"] = result.converged;
        resultInfo["elapsedMs"] = elapsedMs;
        emit registrationCompleted(registrationId, resultInfo);

        if (result.rmse > 3.0f) {
            QVariantMap quality;
            quality["rmse"] = static_cast<double>(result.rmse);
            emit registrationQualityWarning(registrationId, quality,
                QString("High GICP RMSE: %1 mm").arg(result.rmse, 0, 'f', 2));
        }

        return finalMatrix;

    } catch (const std::exception& ex) {
        m_lastError = QString("GPU-GICP registration failed: %1").arg(ex.what());
        qCritical() << "[RegistrationService]" << m_lastError;
        emit registrationFailed(registrationId, m_lastError);
        return nullptr;
    }
}

// ==================== Landmark 配准 ====================

vtkSmartPointer<vtkMatrix4x4> RegistrationServiceImpl::performLandmarkRegistration(
    vtkPoints* sourcePoints,
    vtkPoints* targetPoints,
    const QString& registrationId)
{
    if (!validatePointSets(sourcePoints, targetPoints)) {
        return nullptr;
    }

    QString regId = registrationId.isEmpty() ? generateRegistrationId("landmark") : registrationId;

    emit registrationStarted(regId, "landmark");

    try {
        // 创建 Landmark Transform
        vtkSmartPointer<vtkLandmarkTransform> landmarkTransform =
            vtkSmartPointer<vtkLandmarkTransform>::New();

        landmarkTransform->SetSourceLandmarks(sourcePoints);
        landmarkTransform->SetTargetLandmarks(targetPoints);
        landmarkTransform->SetModeToRigidBody();  // Rigid 变换（刚体）
        landmarkTransform->Update();

        // 获取变换矩阵
        vtkSmartPointer<vtkMatrix4x4> matrix = vtkSmartPointer<vtkMatrix4x4>::New();
        matrix->DeepCopy(landmarkTransform->GetMatrix());

        // 计算 FRE
        double fre = computeRMSError(sourcePoints, targetPoints, matrix);

        // 保存配准记录
        RegistrationRecord record;
        record.registrationId = regId;
        record.type = "landmark";
        record.transform = matrix;
        record.timestamp = QDateTime::currentMSecsSinceEpoch();
        record.fre = fre;
        record.numPoints = sourcePoints->GetNumberOfPoints();

        // 保存源点和目标点的副本（用于后续 TRE 计算）
        record.sourcePoints = vtkSmartPointer<vtkPoints>::New();
        record.sourcePoints->DeepCopy(sourcePoints);
        record.targetPoints = vtkSmartPointer<vtkPoints>::New();
        record.targetPoints->DeepCopy(targetPoints);

        saveRecord(regId, record);

        qDebug() << "[RegistrationService] Landmark registration completed:"
                 << "ID=" << regId
                 << "Points=" << record.numPoints
                 << "FRE=" << fre << "mm";

        // 发射完成信号
        QVariantMap result;
        result["registrationId"] = regId;
        result["type"] = "landmark";
        result["fre"] = fre;
        result["numPoints"] = record.numPoints;
        emit registrationCompleted(regId, result);

        // 质量检查
        if (fre > 5.0) {  // 如果 FRE > 5mm，发出警告
            QVariantMap quality;
            quality["fre"] = fre;
            emit registrationQualityWarning(regId, quality,
                QString("High FRE detected: %1 mm. Consider re-selecting landmarks.").arg(fre, 0, 'f', 2));
        }

        return matrix;

    } catch (const std::exception& ex) {
        m_lastError = QString("Landmark registration failed: %1").arg(ex.what());
        qCritical() << "[RegistrationService]" << m_lastError;
        emit registrationFailed(regId, m_lastError);
        return nullptr;
    }
}

QList<double> RegistrationServiceImpl::performLandmarkRegistrationList(
    const QList<QList<double>>& sourcePoints,
    const QList<QList<double>>& targetPoints)
{
    vtkSmartPointer<vtkPoints> srcVtk = listToVtkPoints(sourcePoints);
    vtkSmartPointer<vtkPoints> tgtVtk = listToVtkPoints(targetPoints);

    if (!srcVtk || !tgtVtk) {
        return QList<double>();
    }

    vtkSmartPointer<vtkMatrix4x4> matrix = performLandmarkRegistration(srcVtk, tgtVtk);
    if (!matrix) {
        return QList<double>();
    }

    return matrixToList(matrix);
}

// ==================== ICP 配准 ====================

vtkSmartPointer<vtkMatrix4x4> RegistrationServiceImpl::performICPRegistration(
    vtkPolyData* source,
    vtkPolyData* target,
    vtkMatrix4x4* initialTransform,
    int maxIterations,
    const QString& registrationId)
{
    if (!source || !target) {
        m_lastError = "Source or target mesh is null";
        qWarning() << "[RegistrationService]" << m_lastError;
        return nullptr;
    }

    if (source->GetNumberOfPoints() == 0 || target->GetNumberOfPoints() == 0) {
        m_lastError = "Source or target mesh has no points";
        qWarning() << "[RegistrationService]" << m_lastError;
        return nullptr;
    }

    QString regId = registrationId.isEmpty() ? generateRegistrationId("icp") : registrationId;

    emit registrationStarted(regId, "icp");

    try {
        // 应用初始变换（如果提供）
        vtkSmartPointer<vtkPolyData> transformedSource = source;
        if (initialTransform) {
            vtkSmartPointer<vtkTransformPolyDataFilter> transformFilter =
                vtkSmartPointer<vtkTransformPolyDataFilter>::New();
            vtkSmartPointer<vtkTransform> transform = vtkSmartPointer<vtkTransform>::New();
            transform->SetMatrix(initialTransform);
            transformFilter->SetTransform(transform);
            transformFilter->SetInputData(source);
            transformFilter->Update();

            transformedSource = vtkSmartPointer<vtkPolyData>::New();
            transformedSource->DeepCopy(transformFilter->GetOutput());
        }

        // 创建 ICP Transform
        vtkSmartPointer<vtkIterativeClosestPointTransform> icp =
            vtkSmartPointer<vtkIterativeClosestPointTransform>::New();

        icp->SetSource(transformedSource);
        icp->SetTarget(target);
        icp->GetLandmarkTransform()->SetModeToRigidBody();  // Rigid 变换
        icp->SetMaximumNumberOfIterations(maxIterations);
        icp->SetMaximumNumberOfLandmarks(m_defaultICPMaxLandmarks);

        if (m_enableICPCentroids) {
            icp->StartByMatchingCentroidsOn();
        }

        icp->Modified();
        icp->Update();

        // 获取变换矩阵
        vtkSmartPointer<vtkMatrix4x4> icpMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
        icpMatrix->DeepCopy(icp->GetMatrix());

        // 如果有初始变换，需要合成最终变换
        vtkSmartPointer<vtkMatrix4x4> finalMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
        if (initialTransform) {
            vtkMatrix4x4::Multiply4x4(icpMatrix, initialTransform, finalMatrix);
        } else {
            finalMatrix->DeepCopy(icpMatrix);
        }

        // 获取 ICP 误差
        double meanDistance = icp->GetMeanDistance();

        // 保存配准记录
        RegistrationRecord record;
        record.registrationId = regId;
        record.type = "icp";
        record.transform = finalMatrix;
        record.timestamp = QDateTime::currentMSecsSinceEpoch();
        record.fre = meanDistance;
        record.numPoints = source->GetNumberOfPoints();

        QVariantMap metadata;
        metadata["maxIterations"] = maxIterations;
        metadata["meanDistance"] = meanDistance;
        metadata["sourcePoints"] = static_cast<int>(source->GetNumberOfPoints());
        metadata["targetPoints"] = static_cast<int>(target->GetNumberOfPoints());
        record.metadata = metadata;

        saveRecord(regId, record);

        qDebug() << "[RegistrationService] ICP registration completed:"
                 << "ID=" << regId
                 << "Iterations=" << maxIterations
                 << "MeanDistance=" << meanDistance << "mm";

        // 发射完成信号
        QVariantMap result;
        result["registrationId"] = regId;
        result["type"] = "icp";
        result["meanDistance"] = meanDistance;
        result["maxIterations"] = maxIterations;
        emit registrationCompleted(regId, result);

        // 质量检查
        if (meanDistance > 3.0) {
            QVariantMap quality;
            quality["meanDistance"] = meanDistance;
            emit registrationQualityWarning(regId, quality,
                QString("High ICP mean distance: %1 mm").arg(meanDistance, 0, 'f', 2));
        }

        return finalMatrix;

    } catch (const std::exception& ex) {
        m_lastError = QString("ICP registration failed: %1").arg(ex.what());
        qCritical() << "[RegistrationService]" << m_lastError;
        emit registrationFailed(regId, m_lastError);
        return nullptr;
    }
}

vtkSmartPointer<vtkMatrix4x4> RegistrationServiceImpl::performICPRegistrationAdvanced(
    vtkPolyData* source,
    vtkPolyData* target,
    const QVariantMap& parameters)
{
    if (!source || !target) {
        m_lastError = "Source or target mesh is null";
        qWarning() << "[RegistrationService]" << m_lastError;
        return nullptr;
    }

    if (source->GetNumberOfPoints() == 0 || target->GetNumberOfPoints() == 0) {
        m_lastError = "Source or target mesh has no points";
        qWarning() << "[RegistrationService]" << m_lastError;
        return nullptr;
    }

    // GPU-GICP 路径：如果请求 useGPU 且 DLL 可用
    bool useGPU = parameters.value("useGPU", false).toBool();
    if (useGPU) {
        qDebug() << "[RegistrationService] Advanced ICP requested GPU refinement:"
                 << "sourcePoints=" << source->GetNumberOfPoints()
                 << "targetPoints=" << target->GetNumberOfPoints()
                 << "registrationId=" << parameters.value("registrationId").toString();
        if (!m_meshGPULoaded) {
            loadMeshGPUDLL();
        }
        if (m_meshGPULoaded) {
            QString registrationId = parameters.value("registrationId", QString()).toString();
            if (registrationId.isEmpty()) {
                registrationId = generateRegistrationId("gicp");
            }

            // 解析初始变换
            vtkSmartPointer<vtkMatrix4x4> initialMatrix = nullptr;
            if (parameters.contains("initialTransform")) {
                QList<QVariant> matrixList = parameters.value("initialTransform").toList();
                if (matrixList.size() == 16) {
                    QList<double> matrixValues;
                    for (const QVariant& v : matrixList) {
                        matrixValues.append(v.toDouble());
                    }
                    initialMatrix = listToMatrix(matrixValues);
                }
            }

            qDebug() << "[RegistrationService] Dispatching GPU-GICP:"
                     << "registrationId=" << registrationId
                     << "hasInitialTransform=" << (initialMatrix != nullptr)
                     << "useRotationSearch=" << parameters.value("useRotationSearch", false).toBool()
                     << "distanceThreshold=" << parameters.value("distanceThreshold", 10.0f).toFloat()
                     << "maxIterations=" << parameters.value("maxIterations", 50).toInt();

            auto result = performGICPRegistration(source, target, initialMatrix, parameters, registrationId);
            if (result) return result;

            qWarning() << "[RegistrationService] GPU-GICP returned no result, falling back to VTK ICP:"
                       << "registrationId=" << registrationId
                       << "lastError=" << m_lastError;
        } else {
            qWarning() << "[RegistrationService] MeshGPU DLL unavailable, falling back to VTK ICP:"
                       << "registrationId=" << parameters.value("registrationId").toString()
                       << "applicationDir=" << QCoreApplication::applicationDirPath();
        }
    }

    // 原有 VTK CPU ICP 路径

    // 解析高级参数
    int maxIterations = parameters.value("maxIterations", m_defaultICPMaxIterations).toInt();
    int maxLandmarks = parameters.value("maxLandmarks", m_defaultICPMaxLandmarks).toInt();
    bool startByMatchingCentroids = parameters.value("startByMatchingCentroids", m_enableICPCentroids).toBool();
    bool checkMeanDistance = parameters.value("checkMeanDistance", false).toBool();
    double maxMeanDistance = parameters.value("maxMeanDistance", 0.01).toDouble();
    QString transformMode = parameters.value("transformMode", "rigid").toString().toLower();
    QString registrationId = parameters.value("registrationId", QString()).toString();

    if (registrationId.isEmpty()) {
        registrationId = generateRegistrationId("icp_adv");
    }

    emit registrationStarted(registrationId, "icp_advanced");

    try {
        // 解析初始变换矩阵（如果提供）
        vtkSmartPointer<vtkMatrix4x4> initialMatrix = nullptr;
        if (parameters.contains("initialTransform")) {
            QList<QVariant> matrixList = parameters.value("initialTransform").toList();
            if (matrixList.size() == 16) {
                QList<double> matrixValues;
                for (const QVariant& v : matrixList) {
                    matrixValues.append(v.toDouble());
                }
                initialMatrix = listToMatrix(matrixValues);
            }
        }

        // 应用初始变换（如果提供）
        vtkSmartPointer<vtkPolyData> transformedSource = source;
        if (initialMatrix) {
            vtkSmartPointer<vtkTransformPolyDataFilter> transformFilter =
                vtkSmartPointer<vtkTransformPolyDataFilter>::New();
            vtkSmartPointer<vtkTransform> transform = vtkSmartPointer<vtkTransform>::New();
            transform->SetMatrix(initialMatrix);
            transformFilter->SetTransform(transform);
            transformFilter->SetInputData(source);
            transformFilter->Update();

            transformedSource = vtkSmartPointer<vtkPolyData>::New();
            transformedSource->DeepCopy(transformFilter->GetOutput());
        }

        // 创建 ICP Transform
        vtkSmartPointer<vtkIterativeClosestPointTransform> icp =
            vtkSmartPointer<vtkIterativeClosestPointTransform>::New();

        icp->SetSource(transformedSource);
        icp->SetTarget(target);

        // 设置变换模式
        if (transformMode == "similarity") {
            icp->GetLandmarkTransform()->SetModeToSimilarity();
        } else if (transformMode == "affine") {
            icp->GetLandmarkTransform()->SetModeToAffine();
        } else {
            icp->GetLandmarkTransform()->SetModeToRigidBody();
        }

        // 设置迭代参数
        icp->SetMaximumNumberOfIterations(maxIterations);
        icp->SetMaximumNumberOfLandmarks(maxLandmarks);

        // 设置质心匹配
        if (startByMatchingCentroids) {
            icp->StartByMatchingCentroidsOn();
        } else {
            icp->StartByMatchingCentroidsOff();
        }

        // 设置均值距离检查
        if (checkMeanDistance) {
            icp->CheckMeanDistanceOn();
            icp->SetMaximumMeanDistance(maxMeanDistance);
        } else {
            icp->CheckMeanDistanceOff();
        }

        icp->Modified();
        icp->Update();

        // 获取变换矩阵
        vtkSmartPointer<vtkMatrix4x4> icpMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
        icpMatrix->DeepCopy(icp->GetMatrix());

        // 如果有初始变换，需要合成最终变换
        vtkSmartPointer<vtkMatrix4x4> finalMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
        if (initialMatrix) {
            vtkMatrix4x4::Multiply4x4(icpMatrix, initialMatrix, finalMatrix);
        } else {
            finalMatrix->DeepCopy(icpMatrix);
        }

        // 获取 ICP 统计信息
        double meanDistance = icp->GetMeanDistance();
        int actualIterations = icp->GetNumberOfIterations();

        // 保存配准记录
        RegistrationRecord record;
        record.registrationId = registrationId;
        record.type = "icp_advanced";
        record.transform = finalMatrix;
        record.timestamp = QDateTime::currentMSecsSinceEpoch();
        record.fre = meanDistance;
        record.numPoints = source->GetNumberOfPoints();

        QVariantMap metadata;
        metadata["maxIterations"] = maxIterations;
        metadata["actualIterations"] = actualIterations;
        metadata["maxLandmarks"] = maxLandmarks;
        metadata["transformMode"] = transformMode;
        metadata["startByMatchingCentroids"] = startByMatchingCentroids;
        metadata["checkMeanDistance"] = checkMeanDistance;
        metadata["maxMeanDistance"] = maxMeanDistance;
        metadata["meanDistance"] = meanDistance;
        metadata["sourcePoints"] = static_cast<int>(source->GetNumberOfPoints());
        metadata["targetPoints"] = static_cast<int>(target->GetNumberOfPoints());
        record.metadata = metadata;

        saveRecord(registrationId, record);

        qDebug() << "[RegistrationService] Advanced ICP registration completed:"
                 << "ID=" << registrationId
                 << "Iterations=" << actualIterations << "/" << maxIterations
                 << "MeanDistance=" << meanDistance << "mm"
                 << "Mode=" << transformMode;

        // 发射完成信号
        QVariantMap result;
        result["registrationId"] = registrationId;
        result["type"] = "icp_advanced";
        result["meanDistance"] = meanDistance;
        result["actualIterations"] = actualIterations;
        result["maxIterations"] = maxIterations;
        result["transformMode"] = transformMode;
        emit registrationCompleted(registrationId, result);

        // 质量检查
        if (meanDistance > 3.0) {
            QVariantMap quality;
            quality["meanDistance"] = meanDistance;
            emit registrationQualityWarning(registrationId, quality,
                QString("High ICP mean distance: %1 mm").arg(meanDistance, 0, 'f', 2));
        }

        return finalMatrix;

    } catch (const std::exception& ex) {
        m_lastError = QString("Advanced ICP registration failed: %1").arg(ex.what());
        qCritical() << "[RegistrationService]" << m_lastError;
        emit registrationFailed(registrationId, m_lastError);
        return nullptr;
    }
}

// ==================== 配准质量评估 ====================

double RegistrationServiceImpl::computeRegistrationError(vtkPoints* sourcePoints,
                                                         vtkPoints* targetPoints)
{
    if (!validatePointSets(sourcePoints, targetPoints, 1)) {
        return -1.0;
    }

    return computeRMSError(sourcePoints, targetPoints, nullptr);
}

double RegistrationServiceImpl::computeRegistrationErrorList(
    const QList<QList<double>>& sourcePoints,
    const QList<QList<double>>& targetPoints,
    const QList<double>& transform)
{
    vtkSmartPointer<vtkPoints> srcVtk = listToVtkPoints(sourcePoints);
    vtkSmartPointer<vtkPoints> tgtVtk = listToVtkPoints(targetPoints);
    vtkSmartPointer<vtkMatrix4x4> matrix = listToMatrix(transform);

    if (!srcVtk || !tgtVtk || !matrix) {
        return -1.0;
    }

    return computeRMSError(srcVtk, tgtVtk, matrix);
}

double RegistrationServiceImpl::computeFRE(const QString& registrationId)
{
    QMutexLocker locker(&m_mutex);
    const RegistrationRecord* record = findRecord(registrationId);

    if (!record) {
        m_lastError = QString("Registration not found: %1").arg(registrationId);
        return -1.0;
    }

    return record->fre;
}

double RegistrationServiceImpl::computeTRE(const QString& registrationId,
                                          const QList<double>& targetPoint)
{
    QMutexLocker locker(&m_mutex);
    const RegistrationRecord* record = findRecord(registrationId);

    if (!record) {
        m_lastError = QString("Registration not found: %1").arg(registrationId);
        return -1.0;
    }

    if (targetPoint.size() < 3) {
        m_lastError = "Target point must have at least 3 coordinates";
        return -1.0;
    }

    // 使用统计学方法计算 TRE
    // 基于 Fitzpatrick 的 TRE 理论公式：
    // TRE^2 ≈ FRE^2 * (1/N + d^2 / (sum of squared distances from fiducials to centroid))
    return computeStatisticalTRE(record, targetPoint);
}

double RegistrationServiceImpl::computeStatisticalTRE(const RegistrationRecord* record,
                                                       const QList<double>& targetPoint)
{
    if (!record || !record->sourcePoints || record->numPoints < 3) {
        // 回退到简化计算
        if (record) {
            return record->fre * 1.5;  // 粗略估计
        }
        return -1.0;
    }

    double point[3] = { targetPoint[0], targetPoint[1], targetPoint[2] };

    // 计算配准点的质心
    double centroid[3] = {0, 0, 0};
    vtkIdType numPoints = record->sourcePoints->GetNumberOfPoints();

    for (vtkIdType i = 0; i < numPoints; ++i) {
        double p[3];
        record->sourcePoints->GetPoint(i, p);
        centroid[0] += p[0];
        centroid[1] += p[1];
        centroid[2] += p[2];
    }
    centroid[0] /= numPoints;
    centroid[1] /= numPoints;
    centroid[2] /= numPoints;

    // 计算目标点到质心的距离
    double d2 = std::pow(point[0] - centroid[0], 2) +
                std::pow(point[1] - centroid[1], 2) +
                std::pow(point[2] - centroid[2], 2);

    // 计算配准点到质心的平方距离之和（各轴分开）
    double sumSqDistX = 0.0, sumSqDistY = 0.0, sumSqDistZ = 0.0;
    for (vtkIdType i = 0; i < numPoints; ++i) {
        double p[3];
        record->sourcePoints->GetPoint(i, p);
        sumSqDistX += std::pow(p[0] - centroid[0], 2);
        sumSqDistY += std::pow(p[1] - centroid[1], 2);
        sumSqDistZ += std::pow(p[2] - centroid[2], 2);
    }

    // 计算总方差
    double totalVariance = sumSqDistX + sumSqDistY + sumSqDistZ;

    if (totalVariance < 1e-10) {
        // 所有点重合，无法计算有意义的 TRE
        return record->fre;
    }

    // Fitzpatrick TRE 公式（简化版）：
    // TRE^2 ≈ FRE^2 * (1/N + d^2/f^2)
    // 其中 N 是配准点数量，d 是目标点到质心的距离，f^2 是配准点的总方差

    double fre2 = record->fre * record->fre;
    double tre2 = fre2 * (1.0 / numPoints + d2 / totalVariance);

    // 对于三维情况，还需要考虑各轴的分布
    // 更精确的公式需要计算协方差矩阵的逆，这里用简化公式

    // 返回 TRE 的 RMS 值
    return std::sqrt(tre2);
}

double RegistrationServiceImpl::computeDistanceToFiducialCentroid(const RegistrationRecord* record,
                                                                    const double point[3])
{
    if (!record || !record->sourcePoints || record->numPoints == 0) {
        return 0.0;
    }

    // 计算质心
    double centroid[3] = {0, 0, 0};
    vtkIdType numPoints = record->sourcePoints->GetNumberOfPoints();

    for (vtkIdType i = 0; i < numPoints; ++i) {
        double p[3];
        record->sourcePoints->GetPoint(i, p);
        centroid[0] += p[0];
        centroid[1] += p[1];
        centroid[2] += p[2];
    }
    centroid[0] /= numPoints;
    centroid[1] /= numPoints;
    centroid[2] /= numPoints;

    // 计算距离
    return std::sqrt(
        std::pow(point[0] - centroid[0], 2) +
        std::pow(point[1] - centroid[1], 2) +
        std::pow(point[2] - centroid[2], 2)
    );
}

void RegistrationServiceImpl::computeFiducialCovariance(const RegistrationRecord* record,
                                                         double centroid[3],
                                                         double covariance[3][3])
{
    // 初始化
    for (int i = 0; i < 3; ++i) {
        centroid[i] = 0.0;
        for (int j = 0; j < 3; ++j) {
            covariance[i][j] = 0.0;
        }
    }

    if (!record || !record->sourcePoints || record->numPoints == 0) {
        return;
    }

    vtkIdType numPoints = record->sourcePoints->GetNumberOfPoints();

    // 计算质心
    for (vtkIdType i = 0; i < numPoints; ++i) {
        double p[3];
        record->sourcePoints->GetPoint(i, p);
        centroid[0] += p[0];
        centroid[1] += p[1];
        centroid[2] += p[2];
    }
    centroid[0] /= numPoints;
    centroid[1] /= numPoints;
    centroid[2] /= numPoints;

    // 计算协方差矩阵
    for (vtkIdType i = 0; i < numPoints; ++i) {
        double p[3];
        record->sourcePoints->GetPoint(i, p);

        double diff[3] = {
            p[0] - centroid[0],
            p[1] - centroid[1],
            p[2] - centroid[2]
        };

        for (int j = 0; j < 3; ++j) {
            for (int k = 0; k < 3; ++k) {
                covariance[j][k] += diff[j] * diff[k];
            }
        }
    }

    // 归一化
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            covariance[i][j] /= numPoints;
        }
    }
}

QVariantMap RegistrationServiceImpl::evaluateRegistrationQuality(const QString& registrationId)
{
    QMutexLocker locker(&m_mutex);
    const RegistrationRecord* record = findRecord(registrationId);

    if (!record) {
        m_lastError = QString("Registration not found: %1").arg(registrationId);
        return QVariantMap();
    }

    QVariantMap quality;
    quality["registrationId"] = registrationId;
    quality["type"] = record->type;
    quality["fre"] = record->fre;
    quality["numPoints"] = record->numPoints;

    // 使用增强的 TRE 统计计算
    double treMax = 0.0;
    double treMean = 0.0;
    double treMin = std::numeric_limits<double>::max();
    QList<double> treValues;

    if (record->sourcePoints && record->numPoints >= 3) {
        // 计算质心和协方差
        double centroid[3];
        double covariance[3][3];
        computeFiducialCovariance(record, centroid, covariance);

        // 计算各个测试点的 TRE（使用配准点周围的采样点）
        vtkIdType numPoints = record->sourcePoints->GetNumberOfPoints();

        // 计算配准点边界框
        double bounds[6] = {
            std::numeric_limits<double>::max(), std::numeric_limits<double>::lowest(),
            std::numeric_limits<double>::max(), std::numeric_limits<double>::lowest(),
            std::numeric_limits<double>::max(), std::numeric_limits<double>::lowest()
        };

        for (vtkIdType i = 0; i < numPoints; ++i) {
            double p[3];
            record->sourcePoints->GetPoint(i, p);
            bounds[0] = std::min(bounds[0], p[0]); bounds[1] = std::max(bounds[1], p[0]);
            bounds[2] = std::min(bounds[2], p[1]); bounds[3] = std::max(bounds[3], p[1]);
            bounds[4] = std::min(bounds[4], p[2]); bounds[5] = std::max(bounds[5], p[2]);
        }

        // 在边界框扩展区域采样测试点
        double margin = 50.0;  // 50mm 边距
        double spacing = 20.0; // 20mm 间隔

        int samplesCollected = 0;
        for (double x = bounds[0] - margin; x <= bounds[1] + margin && samplesCollected < 100; x += spacing) {
            for (double y = bounds[2] - margin; y <= bounds[3] + margin && samplesCollected < 100; y += spacing) {
                for (double z = bounds[4] - margin; z <= bounds[5] + margin && samplesCollected < 100; z += spacing) {
                    QList<double> testPoint = {x, y, z};
                    double tre = computeStatisticalTRE(record, testPoint);

                    if (tre >= 0) {
                        treValues.append(tre);
                        treMax = std::max(treMax, tre);
                        treMin = std::min(treMin, tre);
                        treMean += tre;
                        samplesCollected++;
                    }
                }
            }
        }

        if (!treValues.isEmpty()) {
            treMean /= treValues.size();
        } else {
            // 回退到简化估计
            treMax = record->fre * 2.0;
            treMean = record->fre * 1.2;
            treMin = record->fre;
        }

        // 计算 TRE 标准差
        double treStdDev = 0.0;
        if (treValues.size() > 1) {
            for (double tre : treValues) {
                treStdDev += std::pow(tre - treMean, 2);
            }
            treStdDev = std::sqrt(treStdDev / (treValues.size() - 1));
        }

        quality["tre_max"] = treMax;
        quality["tre_min"] = treMin;
        quality["tre_mean"] = treMean;
        quality["tre_std"] = treStdDev;
        quality["tre_samples"] = treValues.size();

        // 计算配准点分布特征
        double totalVariance = covariance[0][0] + covariance[1][1] + covariance[2][2];
        quality["fiducial_spread"] = std::sqrt(totalVariance);
        quality["centroid_x"] = centroid[0];
        quality["centroid_y"] = centroid[1];
        quality["centroid_z"] = centroid[2];

    } else {
        // 回退到简化估计
        treMax = record->fre * 1.5;
        treMean = record->fre;
        quality["tre_max"] = treMax;
        quality["tre_mean"] = treMean;
    }

    // 评估质量等级
    QString qualityLevel = evaluateQualityLevel(record->fre, treMax);
    quality["quality"] = qualityLevel;

    // 生成建议
    QString recommendation = generateRecommendation(record->fre, treMax, record->numPoints);
    quality["recommendation"] = recommendation;

    // 添加质量分数（0-100）
    double qualityScore = 100.0;
    if (record->fre > 1.0) qualityScore -= (record->fre - 1.0) * 10.0;
    if (treMax > 2.0) qualityScore -= (treMax - 2.0) * 5.0;
    if (record->numPoints < 5) qualityScore -= (5 - record->numPoints) * 5.0;
    qualityScore = std::max(0.0, std::min(100.0, qualityScore));
    quality["score"] = qualityScore;

    return quality;
}

// ==================== 变换矩阵操作 ====================

bool RegistrationServiceImpl::saveRegistrationResult(const QString& registrationId,
                                                     vtkMatrix4x4* transform,
                                                     const QVariantMap& metadata)
{
    if (!transform) {
        m_lastError = "Transform matrix is null";
        return false;
    }

    RegistrationRecord record;
    record.registrationId = registrationId;
    record.type = metadata.value("type", "manual").toString();
    record.transform = vtkSmartPointer<vtkMatrix4x4>::New();
    record.transform->DeepCopy(transform);
    record.timestamp = QDateTime::currentMSecsSinceEpoch();
    record.metadata = metadata;

    saveRecord(registrationId, record);
    return true;
}

vtkSmartPointer<vtkMatrix4x4> RegistrationServiceImpl::loadRegistrationResult(const QString& registrationId)
{
    QMutexLocker locker(&m_mutex);
    const RegistrationRecord* record = findRecord(registrationId);

    if (!record) {
        m_lastError = QString("Registration not found: %1").arg(registrationId);
        return nullptr;
    }

    return record->transform;
}

QStringList RegistrationServiceImpl::getRegistrationList() const
{
    QMutexLocker locker(&m_mutex);
    return m_registrations.keys();
}

QVariantMap RegistrationServiceImpl::getRegistrationInfo(const QString& registrationId) const
{
    QMutexLocker locker(&m_mutex);
    const RegistrationRecord* record = findRecord(registrationId);

    if (!record) {
        return QVariantMap();
    }

    QVariantMap info;
    info["registrationId"] = record->registrationId;
    info["type"] = record->type;
    info["timestamp"] = record->timestamp;
    info["fre"] = record->fre;
    info["numPoints"] = record->numPoints;
    info["metadata"] = record->metadata;

    // 手动转换矩阵为列表（因为 matrixToList 不是 const）
    QList<double> transformList;
    if (record->transform) {
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                transformList.append(record->transform->GetElement(i, j));
            }
        }
    }
    info["transform"] = QVariant::fromValue(transformList);

    return info;
}

bool RegistrationServiceImpl::deleteRegistration(const QString& registrationId)
{
    QMutexLocker locker(&m_mutex);

    if (!m_registrations.contains(registrationId)) {
        m_lastError = QString("Registration not found: %1").arg(registrationId);
        return false;
    }

    m_registrations.remove(registrationId);
    qDebug() << "[RegistrationService] Deleted registration:" << registrationId;
    return true;
}

vtkSmartPointer<vtkMatrix4x4> RegistrationServiceImpl::invertMatrix(vtkMatrix4x4* matrix)
{
    if (!matrix) {
        m_lastError = "Input matrix is null";
        return nullptr;
    }

    vtkSmartPointer<vtkMatrix4x4> inverse = vtkSmartPointer<vtkMatrix4x4>::New();
    vtkMatrix4x4::Invert(matrix, inverse);
    return inverse;
}

vtkSmartPointer<vtkMatrix4x4> RegistrationServiceImpl::multiplyMatrix(vtkMatrix4x4* matrix1,
                                                                       vtkMatrix4x4* matrix2)
{
    if (!matrix1 || !matrix2) {
        m_lastError = "Input matrix is null";
        return nullptr;
    }

    vtkSmartPointer<vtkMatrix4x4> result = vtkSmartPointer<vtkMatrix4x4>::New();
    vtkMatrix4x4::Multiply4x4(matrix1, matrix2, result);
    return result;
}

QList<double> RegistrationServiceImpl::transformPoint(const QList<double>& point,
                                                      vtkMatrix4x4* transform)
{
    if (point.size() < 3 || !transform) {
        return QList<double>();
    }

    double in[3] = { point[0], point[1], point[2] };
    double out[3];

    transformPoint(in, out, transform);

    QList<double> result;
    result << out[0] << out[1] << out[2];

    return result;
}

vtkSmartPointer<vtkPoints> RegistrationServiceImpl::transformPoints(vtkPoints* points,
                                                                     vtkMatrix4x4* transform)
{
    if (!points || !transform) {
        return nullptr;
    }

    vtkSmartPointer<vtkPoints> transformedPoints = vtkSmartPointer<vtkPoints>::New();
    transformedPoints->SetNumberOfPoints(points->GetNumberOfPoints());

    for (vtkIdType i = 0; i < points->GetNumberOfPoints(); ++i) {
        double in[3], out[3];
        points->GetPoint(i, in);
        transformPoint(in, out, transform);
        transformedPoints->SetPoint(i, out);
    }

    return transformedPoints;
}

// ==================== 2D-3D 配准支持 ====================

vtkSmartPointer<vtkMatrix4x4> RegistrationServiceImpl::perform2D3DRegistration(
    const QString& image2D,
    vtkPolyData* model3D,
    vtkMatrix4x4* initialTransform,
    const QVariantMap& parameters)
{
    Q_UNUSED(model3D);  // 2D-3D 配准使用 CT 体积数据而非 mesh

    // 获取 Registration2D3D 服务
    Registration2D3DService* reg2D3DService = getRegistration2D3DService();

    if (!reg2D3DService) {
        m_lastError = "Registration2D3D service not available. Please ensure the Registration2D3D plugin is loaded.";
        qWarning() << "[RegistrationService]" << m_lastError;
        return nullptr;
    }

    // 检查 Python 环境
    if (!reg2D3DService->isPythonInitialized()) {
        // 尝试初始化 Python 环境
        QString pythonHome = parameters.value("pythonHome", QString()).toString();
        QString scriptsPath = parameters.value("scriptsPath", QString()).toString();

        if (pythonHome.isEmpty() || scriptsPath.isEmpty()) {
            m_lastError = "Python environment not initialized. Please provide pythonHome and scriptsPath parameters.";
            qWarning() << "[RegistrationService]" << m_lastError;
            return nullptr;
        }

        if (!reg2D3DService->initializePythonEnvironment(pythonHome, scriptsPath)) {
            m_lastError = QString("Failed to initialize Python environment: %1").arg(reg2D3DService->getLastError());
            qWarning() << "[RegistrationService]" << m_lastError;
            return nullptr;
        }
    }

    // 构建 2D-3D 配准参数
    Registration2D3DParameters regParams;

    // 必需参数
    regParams.ctPath = parameters.value("ctPath", QString()).toString();
    regParams.xrayApPath = image2D;  // 使用传入的 image2D 作为 AP 视角
    regParams.xrayLatPath = parameters.value("xrayLatPath", QString()).toString();

    if (regParams.ctPath.isEmpty()) {
        m_lastError = "CT path is required for 2D-3D registration";
        qWarning() << "[RegistrationService]" << m_lastError;
        return nullptr;
    }

    if (regParams.xrayApPath.isEmpty() && regParams.xrayLatPath.isEmpty()) {
        m_lastError = "At least one X-ray image (AP or LAT) is required";
        qWarning() << "[RegistrationService]" << m_lastError;
        return nullptr;
    }

    // 可选参数
    regParams.jingguPath = parameters.value("jingguPath", QString()).toString();
    regParams.outputDirectory = parameters.value("outputDirectory", QString()).toString();
    regParams.generateDRR = parameters.value("generateDRR", true).toBool();

    // 初始参数（从初始变换矩阵或参数中获取）
    if (initialTransform) {
        // 从变换矩阵提取旋转和平移参数
        // 简化处理：直接使用变换矩阵的元素
        regParams.initParams = {
            0.0, 0.0, 0.0,  // 旋转角度
            initialTransform->GetElement(0, 3),
            initialTransform->GetElement(1, 3),
            initialTransform->GetElement(2, 3)
        };
    } else if (parameters.contains("initParams")) {
        QList<QVariant> initList = parameters.value("initParams").toList();
        regParams.initParams.clear();
        for (const QVariant& v : initList) {
            regParams.initParams.append(v.toDouble());
        }
    }

    // 搜索范围
    if (parameters.contains("searchRange")) {
        QList<QVariant> rangeList = parameters.value("searchRange").toList();
        regParams.searchRange.clear();
        for (const QVariant& v : rangeList) {
            regParams.searchRange.append(v.toInt());
        }
    }

    // 优化参数
    regParams.kdTreeNum = parameters.value("kdTreeNum", 50).toInt();

    // 图像翻转设置
    regParams.apUpDown = parameters.value("apUpDown", false).toBool();
    regParams.apHorizontal = parameters.value("apHorizontal", false).toBool();
    regParams.latUpDown = parameters.value("latUpDown", false).toBool();
    regParams.latHorizontal = parameters.value("latHorizontal", false).toBool();

    // 验证参数
    QString validationError;
    if (!reg2D3DService->validateParameters(regParams, validationError)) {
        m_lastError = QString("Invalid 2D-3D registration parameters: %1").arg(validationError);
        qWarning() << "[RegistrationService]" << m_lastError;
        return nullptr;
    }

    // 生成配准 ID
    QString registrationId = parameters.value("registrationId", QString()).toString();
    if (registrationId.isEmpty()) {
        registrationId = generateRegistrationId("2d3d");
    }

    emit registrationStarted(registrationId, "2d3d");

    qDebug() << "[RegistrationService] Starting 2D-3D registration:"
             << "ID=" << registrationId
             << "CT=" << regParams.ctPath
             << "AP=" << regParams.xrayApPath
             << "LAT=" << regParams.xrayLatPath;

    // 执行同步配准
    Registration2D3DResult result;
    bool success = reg2D3DService->executeRegistrationSync(regParams, result);

    if (!success || result.status != Registration2D3DResult::Completed) {
        m_lastError = QString("2D-3D registration failed: %1").arg(
            result.errorMessage.isEmpty() ? reg2D3DService->getLastError() : result.errorMessage);
        qCritical() << "[RegistrationService]" << m_lastError;
        emit registrationFailed(registrationId, m_lastError);
        return nullptr;
    }

    // 从配准结果构建变换矩阵
    // 使用 AP 和 LAT 的平均结果
    double rx = (result.apResult.rx + result.latResult.rx) / 2.0;
    double ry = (result.apResult.ry + result.latResult.ry) / 2.0;
    double rz = (result.apResult.rz + result.latResult.rz) / 2.0;
    double tx = (result.apResult.tx + result.latResult.tx) / 2.0;
    double ty = (result.apResult.ty + result.latResult.ty) / 2.0;
    double tz = (result.apResult.tz + result.latResult.tz) / 2.0;

    // 创建变换矩阵
    vtkSmartPointer<vtkTransform> transform = vtkSmartPointer<vtkTransform>::New();
    transform->Identity();
    transform->Translate(tx, ty, tz);
    transform->RotateX(rx);
    transform->RotateY(ry);
    transform->RotateZ(rz);

    vtkSmartPointer<vtkMatrix4x4> resultMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
    resultMatrix->DeepCopy(transform->GetMatrix());

    // 保存配准记录
    RegistrationRecord record;
    record.registrationId = registrationId;
    record.type = "2d3d";
    record.transform = resultMatrix;
    record.timestamp = QDateTime::currentMSecsSinceEpoch();
    record.fre = result.finalMetric;  // 使用配准度量值作为 FRE 的替代
    record.numPoints = 0;  // 2D-3D 配准不使用点

    QVariantMap metadata;
    metadata["ctPath"] = regParams.ctPath;
    metadata["xrayApPath"] = regParams.xrayApPath;
    metadata["xrayLatPath"] = regParams.xrayLatPath;
    metadata["duration"] = result.durationSeconds;
    metadata["totalIterations"] = result.totalIterations;
    metadata["finalMetric"] = result.finalMetric;
    metadata["apMetric"] = result.apResult.goMetric;
    metadata["latMetric"] = result.latResult.goMetric;
    metadata["apParams"] = QVariantList{result.apResult.rx, result.apResult.ry, result.apResult.rz,
                                        result.apResult.tx, result.apResult.ty, result.apResult.tz};
    metadata["latParams"] = QVariantList{result.latResult.rx, result.latResult.ry, result.latResult.rz,
                                         result.latResult.tx, result.latResult.ty, result.latResult.tz};
    if (!result.apResult.drrImagePath.isEmpty()) {
        metadata["apDRRPath"] = result.apResult.drrImagePath;
    }
    if (!result.latResult.drrImagePath.isEmpty()) {
        metadata["latDRRPath"] = result.latResult.drrImagePath;
    }
    record.metadata = metadata;

    saveRecord(registrationId, record);

    qDebug() << "[RegistrationService] 2D-3D registration completed:"
             << "ID=" << registrationId
             << "Duration=" << result.durationSeconds << "s"
             << "FinalMetric=" << result.finalMetric;

    // 发射完成信号
    QVariantMap resultInfo;
    resultInfo["registrationId"] = registrationId;
    resultInfo["type"] = "2d3d";
    resultInfo["duration"] = result.durationSeconds;
    resultInfo["finalMetric"] = result.finalMetric;
    resultInfo["totalIterations"] = result.totalIterations;
    emit registrationCompleted(registrationId, resultInfo);

    return resultMatrix;
}

// ==================== 工具方法 ====================

QList<double> RegistrationServiceImpl::matrixToList(vtkMatrix4x4* matrix)
{
    QList<double> list;
    if (!matrix) {
        return list;
    }

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            list.append(matrix->GetElement(i, j));
        }
    }

    return list;
}

vtkSmartPointer<vtkMatrix4x4> RegistrationServiceImpl::listToMatrix(const QList<double>& list)
{
    if (list.size() != 16) {
        m_lastError = "Matrix list must have exactly 16 elements";
        return nullptr;
    }

    vtkSmartPointer<vtkMatrix4x4> matrix = vtkSmartPointer<vtkMatrix4x4>::New();
    int index = 0;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            matrix->SetElement(i, j, list[index++]);
        }
    }

    return matrix;
}

bool RegistrationServiceImpl::exportMatrix(vtkMatrix4x4* matrix,
                                          const QString& filePath,
                                          const QString& format)
{
    if (!matrix) {
        m_lastError = "Matrix is null";
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_lastError = QString("Cannot open file: %1").arg(filePath);
        return false;
    }

    QTextStream out(&file);

    if (format == "txt") {
        // 文本格式
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                out << matrix->GetElement(i, j);
                if (j < 3) out << " ";
            }
            out << "\n";
        }
    } else if (format == "json") {
        // JSON 格式
        QJsonObject json;
        QJsonArray matrixArray;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                matrixArray.append(matrix->GetElement(i, j));
            }
        }
        json["matrix"] = matrixArray;
        json["timestamp"] = QDateTime::currentMSecsSinceEpoch();

        QJsonDocument doc(json);
        out << doc.toJson();
    } else {
        m_lastError = QString("Unsupported format: %1").arg(format);
        file.close();
        return false;
    }

    file.close();
    return true;
}

vtkSmartPointer<vtkMatrix4x4> RegistrationServiceImpl::importMatrix(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = QString("Cannot open file: %1").arg(filePath);
        return nullptr;
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    vtkSmartPointer<vtkMatrix4x4> matrix = vtkSmartPointer<vtkMatrix4x4>::New();

    // 尝试 JSON 格式
    if (content.trimmed().startsWith("{")) {
        QJsonDocument doc = QJsonDocument::fromJson(content.toUtf8());
        if (!doc.isNull() && doc.isObject()) {
            QJsonObject json = doc.object();
            QJsonArray matrixArray = json["matrix"].toArray();

            if (matrixArray.size() == 16) {
                int index = 0;
                for (int i = 0; i < 4; ++i) {
                    for (int j = 0; j < 4; ++j) {
                        matrix->SetElement(i, j, matrixArray[index++].toDouble());
                    }
                }
                return matrix;
            }
        }
    }

    // 尝试文本格式
    QStringList lines = content.split('\n', Qt::SkipEmptyParts);
    if (lines.size() == 4) {
        for (int i = 0; i < 4; ++i) {
            QStringList values = lines[i].split(' ', Qt::SkipEmptyParts);
            if (values.size() == 4) {
                for (int j = 0; j < 4; ++j) {
                    matrix->SetElement(i, j, values[j].toDouble());
                }
            }
        }
        return matrix;
    }

    m_lastError = "Invalid matrix file format";
    return nullptr;
}

QString RegistrationServiceImpl::getLastError() const
{
    return m_lastError;
}

// ==================== Private Methods ====================

QString RegistrationServiceImpl::generateRegistrationId(const QString& prefix)
{
    return QString("%1_%2_%3")
        .arg(prefix)
        .arg(QDateTime::currentDateTime().toString("yyyyMMddHHmmss"))
        .arg(qrand() % 10000, 4, 10, QChar('0'));
}

bool RegistrationServiceImpl::validatePointSets(vtkPoints* sourcePoints, vtkPoints* targetPoints, int minPoints)
{
    if (!sourcePoints || !targetPoints) {
        m_lastError = "Source or target points are null";
        qWarning() << "[RegistrationService]" << m_lastError;
        return false;
    }

    vtkIdType numSource = sourcePoints->GetNumberOfPoints();
    vtkIdType numTarget = targetPoints->GetNumberOfPoints();

    if (numSource != numTarget) {
        m_lastError = QString("Point count mismatch: source=%1, target=%2")
            .arg(numSource).arg(numTarget);
        qWarning() << "[RegistrationService]" << m_lastError;
        return false;
    }

    if (numSource < minPoints) {
        m_lastError = QString("Insufficient points: %1 (minimum %2 required)")
            .arg(numSource).arg(minPoints);
        qWarning() << "[RegistrationService]" << m_lastError;
        return false;
    }

    return true;
}

vtkSmartPointer<vtkPoints> RegistrationServiceImpl::listToVtkPoints(const QList<QList<double>>& pointList)
{
    if (pointList.isEmpty()) {
        return nullptr;
    }

    vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
    points->SetNumberOfPoints(pointList.size());

    for (int i = 0; i < pointList.size(); ++i) {
        const QList<double>& point = pointList[i];
        if (point.size() >= 3) {
            points->SetPoint(i, point[0], point[1], point[2]);
        }
    }

    return points;
}

double RegistrationServiceImpl::computeRMSError(vtkPoints* sourcePoints,
                                                vtkPoints* targetPoints,
                                                vtkMatrix4x4* transform)
{
    if (!sourcePoints || !targetPoints) {
        return -1.0;
    }

    vtkIdType numPoints = sourcePoints->GetNumberOfPoints();
    if (numPoints == 0 || numPoints != targetPoints->GetNumberOfPoints()) {
        return -1.0;
    }

    double sumSquaredError = 0.0;

    for (vtkIdType i = 0; i < numPoints; ++i) {
        double sourcePoint[3], targetPoint[3];
        sourcePoints->GetPoint(i, sourcePoint);
        targetPoints->GetPoint(i, targetPoint);

        // 如果提供了变换，先变换源点
        if (transform) {
            double transformedPoint[3];
            transformPoint(sourcePoint, transformedPoint, transform);
            sourcePoint[0] = transformedPoint[0];
            sourcePoint[1] = transformedPoint[1];
            sourcePoint[2] = transformedPoint[2];
        }

        // 计算欧氏距离
        double dx = sourcePoint[0] - targetPoint[0];
        double dy = sourcePoint[1] - targetPoint[1];
        double dz = sourcePoint[2] - targetPoint[2];

        sumSquaredError += (dx * dx + dy * dy + dz * dz);
    }

    double rms = std::sqrt(sumSquaredError / numPoints);
    return rms;
}

void RegistrationServiceImpl::transformPoint(double in[3], double out[3], vtkMatrix4x4* matrix)
{
    double point[4] = { in[0], in[1], in[2], 1.0 };
    double transformed[4];

    for (int i = 0; i < 4; ++i) {
        transformed[i] = 0.0;
        for (int j = 0; j < 4; ++j) {
            transformed[i] += matrix->GetElement(i, j) * point[j];
        }
    }

    out[0] = transformed[0] / transformed[3];
    out[1] = transformed[1] / transformed[3];
    out[2] = transformed[2] / transformed[3];
}

RegistrationRecord* RegistrationServiceImpl::findRecord(const QString& registrationId)
{
    auto it = m_registrations.find(registrationId);
    return (it != m_registrations.end()) ? &it.value() : nullptr;
}

const RegistrationRecord* RegistrationServiceImpl::findRecord(const QString& registrationId) const
{
    auto it = m_registrations.find(registrationId);
    return (it != m_registrations.end()) ? &it.value() : nullptr;
}

void RegistrationServiceImpl::saveRecord(const QString& registrationId, const RegistrationRecord& record)
{
    QMutexLocker locker(&m_mutex);
    m_registrations[registrationId] = record;
    qDebug() << "[RegistrationService] Saved registration record:" << registrationId;
}

QString RegistrationServiceImpl::evaluateQualityLevel(double fre, double treMax)
{
    if (fre < 2.0 && treMax < 3.0) {
        return "excellent";
    } else if (fre < 3.0 && treMax < 5.0) {
        return "good";
    } else if (fre < 5.0 && treMax < 8.0) {
        return "acceptable";
    } else {
        return "poor";
    }
}

QString RegistrationServiceImpl::generateRecommendation(double fre, double treMax, int numPoints)
{
    QStringList recommendations;

    if (fre > 5.0) {
        recommendations << "High FRE detected. Consider re-selecting landmarks for better accuracy.";
    }

    if (treMax > 8.0) {
        recommendations << "High TRE detected. Registration may not be suitable for navigation.";
    }

    if (numPoints < 5) {
        recommendations << QString("Only %1 points used. Consider using more points (≥5) for better stability.").arg(numPoints);
    }

    if (fre < 2.0 && numPoints >= 5) {
        recommendations << "Registration quality is excellent. Safe to proceed with navigation.";
    }

    if (recommendations.isEmpty()) {
        return "Registration quality is acceptable.";
    }

    return recommendations.join(" ");
}
