#include "RegistrationWorkflow.h"
#include "PointRegistrationService.h"

#include <QUuid>
#include <QDebug>
#include <QtMath>

RegistrationWorkflow::RegistrationWorkflow(PointRegistrationService* service, QObject* parent)
    : QObject(parent)
    , m_service(service)
    , m_state(RegistrationSessionState::Idle)
    , m_probeSource(ProbePointSource::Manual)
{
    if (m_service) {
        m_service->setExecutionOptions(m_executionOptions);
        connectServiceSignals();
    }
}

void RegistrationWorkflow::invalidateLastResult()
{
    m_lastResult = PointRegistrationResult();
}

void RegistrationWorkflow::connectServiceSignals()
{
    // 连接服务信号到工作流信号
    connect(m_service, &PointRegistrationService::modelLoaded,
            this, &RegistrationWorkflow::modelLoaded);

    connect(m_service, &PointRegistrationService::probePointCaptured,
            this, &RegistrationWorkflow::probePointCaptured);

    connect(m_service, &PointRegistrationService::registrationCompleted,
            this, [this](const PointRegistrationResult& rawResult) {
                PointRegistrationResult result = rawResult;
                decorateRegistrationResult(result);
                m_lastResult = result;
                if (result.success) {
                    setState(RegistrationSessionState::Completed);
                } else {
                    setState(RegistrationSessionState::Failed);
                }
                emit registrationCompleted(result);
            });

    connect(m_service, &PointRegistrationService::registrationFailed,
            this, [this](const QString& error) {
                invalidateLastResult();
                setState(RegistrationSessionState::Failed);
                emit registrationFailed(error);
            });

    connect(m_service, &PointRegistrationService::pointRemoved,
            this, [this](int) {
                invalidateLastResult();
            });

    connect(m_service, &PointRegistrationService::pointsCleared,
            this, [this]() {
                invalidateLastResult();
            });

    connect(m_service, &PointRegistrationService::pointUpdated,
            this, [this](int) {
                invalidateLastResult();
            });

    connect(m_service, &PointRegistrationService::progressUpdated,
            this, &RegistrationWorkflow::progressUpdated);

    connect(m_service, &PointRegistrationService::registrationApplied,
            this, &RegistrationWorkflow::registrationApplied);
}

// ========== 会话管理 ==========

QString RegistrationWorkflow::startNewSession(const QString& patientId)
{
    if (!m_service) {
        emit errorOccurred(QString::fromUtf8("服务未初始化"));
        return QString();
    }

    // 清空旧数据
    m_service->clearPoints();

    // 生成新会话ID
    m_sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    // 重置状态
    invalidateLastResult();
    setState(RegistrationSessionState::Idle);

    qDebug() << "[RegistrationWorkflow] New session started:" << m_sessionId
             << "Patient:" << patientId;

    return m_sessionId;
}

void RegistrationWorkflow::resetSession()
{
    if (m_service) {
        m_service->clearPoints();
    }

    invalidateLastResult();
    setState(RegistrationSessionState::Idle);

    emit progressUpdated(0, QString::fromUtf8("会话已重置"));
}

QString RegistrationWorkflow::currentSessionId() const
{
    return m_sessionId;
}

RegistrationSessionState RegistrationWorkflow::currentState() const
{
    return m_state;
}

void RegistrationWorkflow::setState(RegistrationSessionState state)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged(state);
        qDebug() << "[RegistrationWorkflow] State changed to:" << sessionStateToString(state);
    }
}

// ========== 模型加载 ==========

bool RegistrationWorkflow::loadModel(const QString& filePath)
{
    if (!m_service) {
        emit errorOccurred(QString::fromUtf8("服务未初始化"));
        return false;
    }

    setState(RegistrationSessionState::ModelLoading);
    emit progressUpdated(10, QString::fromUtf8("正在加载模型..."));

    bool success = m_service->loadModelFromFile(filePath);

    if (success) {
        setState(RegistrationSessionState::PointCollection);
        emit progressUpdated(100, QString::fromUtf8("模型加载完成"));
    } else {
        setState(RegistrationSessionState::Failed);
        emit errorOccurred(m_service->getLastError());
    }

    return success;
}

bool RegistrationWorkflow::loadModelFromSegmentation(const QString& taskId, const QString& bodyPart)
{
    if (!m_service) {
        emit errorOccurred(QString::fromUtf8("服务未初始化"));
        return false;
    }

    setState(RegistrationSessionState::ModelLoading);
    emit progressUpdated(10, QString::fromUtf8("正在从分割结果加载模型..."));

    bool success = m_service->loadModelFromSegmentation(taskId, bodyPart);

    if (success) {
        setState(RegistrationSessionState::PointCollection);
        emit progressUpdated(100, QString::fromUtf8("模型加载完成"));
    } else {
        setState(RegistrationSessionState::Failed);
        emit errorOccurred(m_service->getLastError());
    }

    return success;
}

bool RegistrationWorkflow::hasModel() const
{
    return m_service && m_service->hasModel();
}

// ========== CT点管理 ==========

int RegistrationWorkflow::addCtPoint(const QVector3D& position, const QString& name)
{
    if (!m_service) {
        emit errorOccurred(QString::fromUtf8("服务未初始化"));
        return -1;
    }

    // 确保在点采集状态
    if (m_state != RegistrationSessionState::PointCollection &&
        m_state != RegistrationSessionState::Idle) {
        setState(RegistrationSessionState::PointCollection);
    }

    int index = m_service->addPoint(name);
    if (index >= 0) {
        m_service->setSourcePosition(index, position);
        emit ctPointAdded(index, position);
        qDebug() << "[RegistrationWorkflow] CT point added at index" << index
                 << "position:" << position;
    }

    return index;
}

bool RegistrationWorkflow::removeCtPoint(int index)
{
    if (!m_service) return false;
    return m_service->removePoint(index);
}

void RegistrationWorkflow::clearCtPoints()
{
    if (m_service) {
        m_service->clearPoints();
    }
}

int RegistrationWorkflow::ctPointCount() const
{
    return m_service ? m_service->pointCount() : 0;
}

int RegistrationWorkflow::validPairCount() const
{
    if (!m_service) return 0;

    int count = 0;
    auto points = m_service->getAllPoints();
    for (const auto& pt : points) {
        if (pt.isComplete()) {
            count++;
        }
    }
    return count;
}

// ========== 探针点采集 ==========

void RegistrationWorkflow::setProbeSource(ProbePointSource source)
{
    m_probeSource = source;
    if (m_service) {
        m_service->setProbePointSource(source);
    }
    qDebug() << "[RegistrationWorkflow] Probe source set to:" << probeSourceToString(source);
}

bool RegistrationWorkflow::captureProbePoint(int pointIndex)
{
    if (!m_service) {
        emit errorOccurred(QString::fromUtf8("服务未初始化"));
        return false;
    }

    return m_service->captureProbePoint(pointIndex);
}

bool RegistrationWorkflow::setProbePosition(int pointIndex, const QVector3D& position)
{
    if (!m_service) return false;
    return m_service->setTargetPosition(pointIndex, position);
}

int RegistrationWorkflow::generateSimulatedProbePoints(double noiseLevel)
{
    if (!m_service) {
        emit errorOccurred(QString::fromUtf8("服务未初始化"));
        return 0;
    }

    emit progressUpdated(50, QString::fromUtf8("生成模拟探针点 (噪声: %1mm)...").arg(noiseLevel));

    int count = m_service->generateAllSimulatedProbePoints(noiseLevel);

    emit progressUpdated(100, QString::fromUtf8("生成了 %1 个模拟探针点").arg(count));

    return count;
}

void RegistrationWorkflow::setExecutionOptions(const PointRegistrationExecutionOptions& options)
{
    m_executionOptions = options;
    if (m_service) {
        m_service->setExecutionOptions(options);
    }
}

PointRegistrationExecutionOptions RegistrationWorkflow::executionOptions() const
{
    return m_executionOptions;
}

// ========== 配准执行 ==========

bool RegistrationWorkflow::canExecute() const
{
    return m_service && m_service->canExecuteRegistration();
}

bool RegistrationWorkflow::executeRegistration()
{
    if (!m_service) {
        emit errorOccurred(QString::fromUtf8("服务未初始化"));
        return false;
    }

    if (!canExecute()) {
        QString error = QString::fromUtf8("有效点对不足，至少需要3对点（当前: %1）")
                            .arg(validPairCount());
        invalidateLastResult();
        emit errorOccurred(error);
        return false;
    }

    setState(RegistrationSessionState::Computing);
    emit progressUpdated(10, QString::fromUtf8("开始配准计算..."));

    PointRegistrationResult result = m_service->executeRegistration();
    decorateRegistrationResult(result);
    m_lastResult = result;

    return result.success;
}

void RegistrationWorkflow::setTargetRegistrationRegion(const TargetRegistrationRegion& region)
{
    m_targetRegion = region;
    if (m_service) {
        m_service->setTargetRegistrationRegion(region);
    }
}

void RegistrationWorkflow::setPlanningConstraintContext(const QVariantMap& context)
{
    m_planningConstraintContext = context;
    if (m_service) {
        m_service->setPlanningConstraintContext(context);
    }
}

void RegistrationWorkflow::setPlanningConstraintRegions(const QMap<QString, QList<QVector3D>>& regions)
{
    m_planningConstraintRegions = regions;
    if (m_service) {
        m_service->setPlanningConstraintRegions(regions);
    }
}

QList<RecommendedRegistrationPoint> RegistrationWorkflow::recommendRegistrationPoints(
    const QList<CandidateRegistrationPoint>& candidates) const
{
    return recommendRegistrationPoints(candidates, m_executionOptions.pointSelectionStrategyId);
}

QList<RecommendedRegistrationPoint> RegistrationWorkflow::recommendRegistrationPoints(
    const QList<CandidateRegistrationPoint>& candidates,
    const QString& strategyId) const
{
    QList<QVector3D> selected;
    if (m_service) {
        const auto points = m_service->getAllPoints();
        for (const auto& point : points) {
            if (point.hasSource) {
                selected.append(point.sourcePosition);
            }
        }
    }

    const RegistrationPointSelectionStrategy* strategy = m_strategyRegistry.strategy(strategyId);
    if (!strategy) {
        return {};
    }

    return strategy->select(m_targetRegion, candidates, selected);
}

void RegistrationWorkflow::decorateRegistrationResult(PointRegistrationResult& result) const
{
    if (!result.success) {
        return;
    }

    if (m_targetRegion.radiusMm > 0.0) {
        result.metrics.insert(QStringLiteral("target_region_radius_mm"), m_targetRegion.radiusMm);
        result.metrics.insert(QStringLiteral("target_region_origin_x"), m_targetRegion.origin.x());
        result.metrics.insert(QStringLiteral("target_region_origin_y"), m_targetRegion.origin.y());
        result.metrics.insert(QStringLiteral("target_region_origin_z"), m_targetRegion.origin.z());
    }

    if (result.targetRegionTre <= 0.0) {
        result.targetRegionTre = result.rmsError;
    }

    if (result.coverageScore <= 0.0 && result.pointCount > 0) {
        result.coverageScore = qMin(1.0, static_cast<double>(result.pointCount) / 5.0);
    }

    result.metrics.insert(QStringLiteral("registration_mode"), m_executionOptions.registrationMethodId);
    result.metrics.insert(QStringLiteral("point_selection_strategy_id"), m_executionOptions.pointSelectionStrategyId);
    result.metrics.insert(QStringLiteral("export_detailed_metrics"), m_executionOptions.exportDetailedMetrics);

    for (auto it = m_planningConstraintContext.cbegin(); it != m_planningConstraintContext.cend(); ++it) {
        result.metrics.insert(it.key(), it.value());
    }
}

PointRegistrationResult RegistrationWorkflow::getLastResult() const
{
    return m_lastResult;
}

QMatrix4x4 RegistrationWorkflow::getTransformMatrix() const
{
    return m_service ? m_service->getTransformMatrix() : QMatrix4x4();
}

// ========== 精度评估 ==========

int RegistrationWorkflow::evaluateQuality() const
{
    if (!m_lastResult.success) return 0;

    double rms = m_lastResult.rmsError;

    if (rms < 1.0) return 3;      // 优秀
    if (rms < 2.0) return 2;      // 良好
    if (rms < 3.0) return 1;      // 可接受
    return 0;                      // 差
}

QString RegistrationWorkflow::getQualityDescription() const
{
    if (!m_lastResult.success) {
        return QString::fromUtf8("配准失败");
    }

    int quality = evaluateQuality();
    QString desc;

    switch (quality) {
        case 3:
            desc = QString::fromUtf8("优秀 (RMS < 1.0mm)");
            break;
        case 2:
            desc = QString::fromUtf8("良好 (RMS < 2.0mm)");
            break;
        case 1:
            desc = QString::fromUtf8("可接受 (RMS < 3.0mm)");
            break;
        default:
            desc = QString::fromUtf8("较差 (RMS >= 3.0mm)");
            break;
    }

    return QString::fromUtf8("%1\nRMS误差: %2 mm\n最大误差: %3 mm\n平均误差: %4 mm")
               .arg(desc)
               .arg(m_lastResult.rmsError, 0, 'f', 3)
               .arg(m_lastResult.maxError, 0, 'f', 3)
               .arg(m_lastResult.meanError, 0, 'f', 3);
}

QStringList RegistrationWorkflow::getImprovementSuggestions() const
{
    QStringList suggestions;

    if (!m_lastResult.success) {
        suggestions << QString::fromUtf8("请确保至少有3对有效的配准点");
        return suggestions;
    }

    // 基于误差分析给出建议
    if (m_lastResult.rmsError >= 2.0) {
        suggestions << QString::fromUtf8("RMS误差较大，建议增加配准点数量");
    }

    if (m_lastResult.maxError > 3.0) {
        // 找出最大误差点
        double maxErr = 0;
        int maxIdx = -1;
        for (int i = 0; i < m_lastResult.pointErrors.size(); ++i) {
            if (m_lastResult.pointErrors[i] > maxErr) {
                maxErr = m_lastResult.pointErrors[i];
                maxIdx = i;
            }
        }
        if (maxIdx >= 0) {
            suggestions << QString::fromUtf8("点 %1 误差过大 (%2mm)，建议重新采集")
                               .arg(maxIdx + 1)
                               .arg(maxErr, 0, 'f', 2);
        }
    }

    if (m_lastResult.pointCount < 5) {
        suggestions << QString::fromUtf8("当前使用 %1 个点，建议使用 5-7 个点以提高稳定性")
                           .arg(m_lastResult.pointCount);
    }

    if (suggestions.isEmpty()) {
        suggestions << QString::fromUtf8("配准质量良好，可以继续进行导航");
    }

    return suggestions;
}

// ========== 配准应用 ==========

bool RegistrationWorkflow::applyToNavigation()
{
    if (!m_service) {
        emit errorOccurred(QString::fromUtf8("服务未初始化"));
        return false;
    }

    if (!m_lastResult.success) {
        emit errorOccurred(QString::fromUtf8("没有有效的配准结果"));
        return false;
    }

    QString registrationId = QString("%1_%2")
                                 .arg(m_sessionId)
                                 .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));

    bool success = m_service->applyRegistrationToNavigation(registrationId);

    if (success) {
        qDebug() << "[RegistrationWorkflow] Registration applied:" << registrationId;
    } else {
        emit errorOccurred(m_service->getLastError());
    }

    return success;
}
