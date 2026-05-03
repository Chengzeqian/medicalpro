#include "UI/NewPages/Navigation/navigation_runtime_state.h"

void NavigationRuntimeState::setCaseContext(
    const QString& caseId,
    const QString& trackingSessionId,
    const QString& navigationToolId)
{
    m_caseId = caseId;
    m_trackingSessionId = trackingSessionId;
    m_navigationToolId = navigationToolId;
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
