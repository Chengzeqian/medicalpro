#pragma once

#include "Framework/Navigation/ankle_navigation_types.h"
#include "Framework/Navigation/navigation_confidence_evaluator.h"
#include "Framework/Navigation/navigation_pose_frame.h"
#include "Framework/Navigation/navigation_transform_graph.h"
#include "UI/NewPages/Navigation/navigation_runtime_state.h"

#include <QMatrix4x4>
#include <functional>

class NavigationRuntimeCoordinator
{
public:
    struct PersistenceActions
    {
        std::function<AnkleEvaluationSnapshot(const QString& caseId)> loadEvaluationSnapshot;
        std::function<bool(const AnkleEvaluationReport& report)> saveEvaluationReport;
        std::function<bool(const QString& caseId)> exportMetricsCsv;
        std::function<bool(const QString& caseId)> exportCaseSummary;
    };

    explicit NavigationRuntimeCoordinator(
        NavigationRuntimeState* runtimeState,
        PersistenceActions persistenceActions = {});

    NavigationRuntimeState* runtimeState() const;
    void setCasesRoot(const QString& casesRoot);

    void setTargetRegionDefinition(const DigitalTwinTargetRegionDefinition& targetRegionDefinition);
    void clearTargetRegionDefinition();
    void clearPoseTrackingState();
    void clearRegistrationTransform();
    void handleRegistrationResult(const PointRegistrationResult& registrationResult);
    void handleTrackingQuality(const QVariantMap& trackingQuality);
    void handleCalibrationCompleted(const QVariantMap& calibrationResult);
    void handleCalibrationTransform(const QMatrix4x4& markerToToolTransform);
    void handleRegistrationTransform(const QMatrix4x4& patientToVtkWorldTransform);
    void handlePoseFrame(const NavigationPoseFrame& frame);
    NavigationDisplayState buildDisplayState(
        const QStringList& boneModelPaths,
        const QString& activeToolModelPath) const;
    void recomputeConfidence();
    void persistEvaluationReportSnapshot(bool exportMetricsCsv = false);

private:
    NavigationConfidenceInputs buildConfidenceInputs() const;
    PersistenceActions createDefaultPersistenceActions() const;
    void refreshDigitalTwinState();

    NavigationRuntimeState* m_runtimeState = nullptr;
    NavigationConfidenceEvaluator m_confidenceEvaluator;
    PersistenceActions m_persistenceActions;
    NavigationTransformGraph m_transformGraph;
    QString m_casesRoot;
};
