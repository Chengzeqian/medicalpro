#pragma once

#include "Framework/Navigation/ankle_navigation_types.h"
#include "Framework/Navigation/navigation_confidence_evaluator.h"
#include "UI/NewPages/Navigation/navigation_runtime_state.h"

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

    void handleRegistrationResult(const PointRegistrationResult& registrationResult);
    void handleTrackingQuality(const QVariantMap& trackingQuality);
    void handleCalibrationCompleted(const QVariantMap& calibrationResult);
    void recomputeConfidence();
    void persistEvaluationReportSnapshot(bool exportMetricsCsv = false);

private:
    NavigationConfidenceInputs buildConfidenceInputs() const;
    PersistenceActions createDefaultPersistenceActions() const;

    NavigationRuntimeState* m_runtimeState = nullptr;
    NavigationConfidenceEvaluator m_confidenceEvaluator;
    PersistenceActions m_persistenceActions;
    QString m_casesRoot;
};
