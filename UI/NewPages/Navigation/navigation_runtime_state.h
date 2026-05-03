#pragma once

#include "Framework/Navigation/navigation_confidence_evaluator.h"
#include "Plugins/PointRegistration/PointRegistrationDataStructures.h"

#include <QString>
#include <QVariantMap>

class NavigationRuntimeState
{
public:
    void setCaseContext(const QString& caseId, const QString& trackingSessionId, const QString& navigationToolId);
    void setTrackingQuality(const QVariantMap& trackingQuality);
    void setRegistrationResult(const PointRegistrationResult& registrationResult);
    void setConfidenceResult(const NavigationConfidenceResult& confidenceResult);

    const QString& caseId() const;
    const QString& trackingSessionId() const;
    const QString& navigationToolId() const;
    const QVariantMap& trackingQuality() const;
    const PointRegistrationResult& registrationResult() const;
    const NavigationConfidenceResult& confidenceResult() const;

    bool hasTrackingQuality() const;
    bool hasRegistrationResult() const;
    bool hasConfidenceResult() const;

private:
    QString m_caseId;
    QString m_trackingSessionId;
    QString m_navigationToolId;
    QVariantMap m_trackingQuality;
    PointRegistrationResult m_registrationResult;
    NavigationConfidenceResult m_confidenceResult;
    bool m_hasTrackingQuality = false;
    bool m_hasRegistrationResult = false;
    bool m_hasConfidenceResult = false;
};
