#include "Framework/Navigation/navigation_transform_graph.h"

void NavigationTransformGraph::setLatestPoseFrame(const NavigationPoseFrame& frame)
{
    m_latestPoseFrame = frame;
    m_hasLatestPoseFrame = true;
}

void NavigationTransformGraph::clearLatestPoseFrame()
{
    m_latestPoseFrame = NavigationPoseFrame();
    m_hasLatestPoseFrame = false;
}

bool NavigationTransformGraph::hasLatestPoseFrame() const
{
    return m_hasLatestPoseFrame;
}

const NavigationPoseFrame& NavigationTransformGraph::latestPoseFrame() const
{
    return m_latestPoseFrame;
}

void NavigationTransformGraph::setMarkerToToolTransform(const QMatrix4x4& markerToToolTransform)
{
    m_markerToToolTransform = markerToToolTransform;
    m_hasMarkerToToolTransform = true;
}

void NavigationTransformGraph::clearMarkerToToolTransform()
{
    m_markerToToolTransform = QMatrix4x4();
    m_hasMarkerToToolTransform = false;
}

bool NavigationTransformGraph::hasMarkerToToolTransform() const
{
    return m_hasMarkerToToolTransform;
}

void NavigationTransformGraph::setPatientToVtkWorldTransform(const QMatrix4x4& patientToVtkWorldTransform)
{
    m_patientToVtkWorldTransform = patientToVtkWorldTransform;
    m_hasPatientToVtkWorldTransform = true;
}

void NavigationTransformGraph::clearPatientToVtkWorldTransform()
{
    m_patientToVtkWorldTransform = QMatrix4x4();
    m_hasPatientToVtkWorldTransform = false;
}

bool NavigationTransformGraph::hasPatientToVtkWorldTransform() const
{
    return m_hasPatientToVtkWorldTransform;
}

NavigationTransformResult NavigationTransformGraph::compute() const
{
    NavigationTransformResult result;
    result.trackingAvailable = m_hasLatestPoseFrame && m_latestPoseFrame.trackingVisible;
    result.calibrationAvailable = m_hasMarkerToToolTransform;
    result.registrationAvailable = m_hasPatientToVtkWorldTransform;

    if (!result.trackingAvailable) {
        result.failureCode = QStringLiteral("tracking_unavailable");
        result.failureText = QStringLiteral("实时跟踪不可用");
        return result;
    }

    if (!result.calibrationAvailable) {
        result.failureCode = QStringLiteral("calibration_missing");
        result.failureText = QStringLiteral("器械标定结果缺失");
        return result;
    }

    if (!result.registrationAvailable) {
        result.failureCode = QStringLiteral("registration_missing");
        result.failureText = QStringLiteral("患者配准结果缺失");
        return result;
    }

    result.vtkToolTransform = m_patientToVtkWorldTransform
        * m_latestPoseFrame.trackingToMarker
        * m_markerToToolTransform;
    result.valid = true;
    result.failureCode = QStringLiteral("ok");
    result.failureText = QStringLiteral("导航位姿链路有效");
    return result;
}
