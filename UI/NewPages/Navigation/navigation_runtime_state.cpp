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
    m_trackedInstrumentVisibility.clear();
    m_activeInstrumentPoseSummaries.clear();
    m_hasTrackingQuality = false;
    m_hasRegistrationResult = false;
    m_hasConfidenceResult = false;
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
