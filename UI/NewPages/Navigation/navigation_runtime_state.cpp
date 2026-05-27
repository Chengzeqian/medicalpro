#include "UI/NewPages/Navigation/navigation_runtime_state.h"

void NavigationRuntimeState::setCaseContext(
    const QString& caseId,
    const QString& trackingSessionId,
    const QString& navigationToolId)
{
    const bool caseContextChanged =
        m_caseId != caseId
        || m_trackingSessionId != trackingSessionId
        || m_navigationToolId != navigationToolId;

    m_caseId = caseId;
    m_trackingSessionId = trackingSessionId;
    m_navigationToolId = navigationToolId;

    if (!caseContextChanged) {
        return;
    }

    m_trackingQuality.clear();
    m_registrationResult = PointRegistrationResult();
    m_confidenceResult = NavigationConfidenceResult();
    m_targetRegionDefinition = DigitalTwinTargetRegionDefinition();
    m_targetRegionNavigationStatus = TargetRegionNavigationStatus();
    m_digitalTwinRiskReport = DigitalTwinRiskReport();
    m_digitalTwinState = DigitalTwinState();
    m_trackedInstrumentVisibility.clear();
    m_activeInstrumentPoseSummaries.clear();
    m_hasTrackingQuality = false;
    m_hasRegistrationResult = false;
    m_hasConfidenceResult = false;
    m_hasTargetRegionDefinition = false;
    m_hasTargetRegionNavigationStatus = false;
    m_hasDigitalTwinRiskReport = false;
    m_hasDigitalTwinState = false;
}

void NavigationRuntimeState::setTrackingQuality(const QVariantMap& trackingQuality)
{
    m_trackingQuality = trackingQuality;
    m_hasTrackingQuality = true;
}

void NavigationRuntimeState::setRegistrationResult(const PointRegistrationResult& registrationResult)
{
    m_registrationResult = registrationResult;
    m_hasRegistrationResult = true;
}

void NavigationRuntimeState::setConfidenceResult(const NavigationConfidenceResult& confidenceResult)
{
    m_confidenceResult = confidenceResult;
    m_hasConfidenceResult = true;
}

void NavigationRuntimeState::setTargetRegionDefinition(const DigitalTwinTargetRegionDefinition& targetRegionDefinition)
{
    m_targetRegionDefinition = targetRegionDefinition;
    m_hasTargetRegionDefinition = true;
}

void NavigationRuntimeState::setTargetRegionNavigationStatus(const TargetRegionNavigationStatus& targetRegionNavigationStatus)
{
    m_targetRegionNavigationStatus = targetRegionNavigationStatus;
    m_hasTargetRegionNavigationStatus = true;
}

void NavigationRuntimeState::setDigitalTwinRiskReport(const DigitalTwinRiskReport& digitalTwinRiskReport)
{
    m_digitalTwinRiskReport = digitalTwinRiskReport;
    m_hasDigitalTwinRiskReport = true;
}

void NavigationRuntimeState::setDigitalTwinState(const DigitalTwinState& digitalTwinState)
{
    m_digitalTwinState = digitalTwinState;
    m_hasDigitalTwinState = true;
}

void NavigationRuntimeState::setTrackedInstrumentVisible(const QString& instrumentId, bool visible)
{
    m_trackedInstrumentVisibility.insert(instrumentId, visible);
}

bool NavigationRuntimeState::isTrackedInstrumentVisible(const QString& instrumentId) const
{
    return m_trackedInstrumentVisibility.value(instrumentId, false);
}

void NavigationRuntimeState::setActiveInstrumentPoseSummary(const QString& instrumentId, const QString& summary)
{
    m_activeInstrumentPoseSummaries.insert(instrumentId, summary);
}

QString NavigationRuntimeState::activeInstrumentPoseSummary(const QString& instrumentId) const
{
    return m_activeInstrumentPoseSummaries.value(instrumentId);
}

void NavigationRuntimeState::clearTrackingQuality()
{
    m_trackingQuality.clear();
    m_hasTrackingQuality = false;
}

void NavigationRuntimeState::clearRegistrationResult()
{
    m_registrationResult = PointRegistrationResult();
    m_hasRegistrationResult = false;
}

void NavigationRuntimeState::clearConfidenceResult()
{
    m_confidenceResult = NavigationConfidenceResult();
    m_hasConfidenceResult = false;
}

void NavigationRuntimeState::clearTargetRegionDefinition()
{
    m_targetRegionDefinition = DigitalTwinTargetRegionDefinition();
    m_hasTargetRegionDefinition = false;
}

void NavigationRuntimeState::clearTargetRegionNavigationStatus()
{
    m_targetRegionNavigationStatus = TargetRegionNavigationStatus();
    m_hasTargetRegionNavigationStatus = false;
}

void NavigationRuntimeState::clearDigitalTwinRiskReport()
{
    m_digitalTwinRiskReport = DigitalTwinRiskReport();
    m_hasDigitalTwinRiskReport = false;
}

void NavigationRuntimeState::clearDigitalTwinState()
{
    m_digitalTwinState = DigitalTwinState();
    m_hasDigitalTwinState = false;
}

const QString& NavigationRuntimeState::caseId() const
{
    return m_caseId;
}

const QString& NavigationRuntimeState::trackingSessionId() const
{
    return m_trackingSessionId;
}

const QString& NavigationRuntimeState::navigationToolId() const
{
    return m_navigationToolId;
}

const QVariantMap& NavigationRuntimeState::trackingQuality() const
{
    return m_trackingQuality;
}

const PointRegistrationResult& NavigationRuntimeState::registrationResult() const
{
    return m_registrationResult;
}

const NavigationConfidenceResult& NavigationRuntimeState::confidenceResult() const
{
    return m_confidenceResult;
}

const DigitalTwinTargetRegionDefinition& NavigationRuntimeState::targetRegionDefinition() const
{
    return m_targetRegionDefinition;
}

const TargetRegionNavigationStatus& NavigationRuntimeState::targetRegionNavigationStatus() const
{
    return m_targetRegionNavigationStatus;
}

const DigitalTwinRiskReport& NavigationRuntimeState::digitalTwinRiskReport() const
{
    return m_digitalTwinRiskReport;
}

const DigitalTwinState& NavigationRuntimeState::digitalTwinState() const
{
    return m_digitalTwinState;
}

bool NavigationRuntimeState::hasTrackingQuality() const
{
    return m_hasTrackingQuality;
}

bool NavigationRuntimeState::hasRegistrationResult() const
{
    return m_hasRegistrationResult;
}

bool NavigationRuntimeState::hasConfidenceResult() const
{
    return m_hasConfidenceResult;
}

bool NavigationRuntimeState::hasTargetRegionDefinition() const
{
    return m_hasTargetRegionDefinition;
}

bool NavigationRuntimeState::hasTargetRegionNavigationStatus() const
{
    return m_hasTargetRegionNavigationStatus;
}

bool NavigationRuntimeState::hasDigitalTwinRiskReport() const
{
    return m_hasDigitalTwinRiskReport;
}

bool NavigationRuntimeState::hasDigitalTwinState() const
{
    return m_hasDigitalTwinState;
}
