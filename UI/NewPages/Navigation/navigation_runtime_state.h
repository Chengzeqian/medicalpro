#pragma once

#include "Framework/Navigation/ankle_navigation_types.h"
#include "Framework/Navigation/navigation_confidence_evaluator.h"
#include "Plugins/PointRegistration/PointRegistrationDataStructures.h"

#include <QHash>
#include <QString>
#include <QVariantMap>

class NavigationRuntimeState
{
public:
    void setCaseContext(const QString& caseId, const QString& trackingSessionId, const QString& navigationToolId);
    void setTrackingQuality(const QVariantMap& trackingQuality);
    void setRegistrationResult(const PointRegistrationResult& registrationResult);
    void setConfidenceResult(const NavigationConfidenceResult& confidenceResult);
    void setTargetRegionDefinition(const DigitalTwinTargetRegionDefinition& targetRegionDefinition);
    void setTargetRegionNavigationStatus(const TargetRegionNavigationStatus& targetRegionNavigationStatus);
    void setDigitalTwinRiskReport(const DigitalTwinRiskReport& digitalTwinRiskReport);
    void setDigitalTwinState(const DigitalTwinState& digitalTwinState);
    void setTrackedInstrumentVisible(const QString& instrumentId, bool visible);
    bool isTrackedInstrumentVisible(const QString& instrumentId) const;
    void setActiveInstrumentPoseSummary(const QString& instrumentId, const QString& summary);
    QString activeInstrumentPoseSummary(const QString& instrumentId) const;
    void clearTrackingQuality();
    void clearRegistrationResult();
    void clearConfidenceResult();
    void clearTargetRegionDefinition();
    void clearTargetRegionNavigationStatus();
    void clearDigitalTwinRiskReport();
    void clearDigitalTwinState();

    const QString& caseId() const;
    const QString& trackingSessionId() const;
    const QString& navigationToolId() const;
    const QVariantMap& trackingQuality() const;
    const PointRegistrationResult& registrationResult() const;
    const NavigationConfidenceResult& confidenceResult() const;
    const DigitalTwinTargetRegionDefinition& targetRegionDefinition() const;
    const TargetRegionNavigationStatus& targetRegionNavigationStatus() const;
    const DigitalTwinRiskReport& digitalTwinRiskReport() const;
    const DigitalTwinState& digitalTwinState() const;

    bool hasTrackingQuality() const;
    bool hasRegistrationResult() const;
    bool hasConfidenceResult() const;
    bool hasTargetRegionDefinition() const;
    bool hasTargetRegionNavigationStatus() const;
    bool hasDigitalTwinRiskReport() const;
    bool hasDigitalTwinState() const;

private:
    QString m_caseId;
    QString m_trackingSessionId;
    QString m_navigationToolId;
    QVariantMap m_trackingQuality;
    PointRegistrationResult m_registrationResult;
    NavigationConfidenceResult m_confidenceResult;
    DigitalTwinTargetRegionDefinition m_targetRegionDefinition;
    TargetRegionNavigationStatus m_targetRegionNavigationStatus;
    DigitalTwinRiskReport m_digitalTwinRiskReport;
    DigitalTwinState m_digitalTwinState;
    QHash<QString, bool> m_trackedInstrumentVisibility;
    QHash<QString, QString> m_activeInstrumentPoseSummaries;
    bool m_hasTrackingQuality = false;
    bool m_hasRegistrationResult = false;
    bool m_hasConfidenceResult = false;
    bool m_hasTargetRegionDefinition = false;
    bool m_hasTargetRegionNavigationStatus = false;
    bool m_hasDigitalTwinRiskReport = false;
    bool m_hasDigitalTwinState = false;
};
