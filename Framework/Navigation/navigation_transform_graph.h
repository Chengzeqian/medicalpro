#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Navigation/navigation_pose_frame.h"

#include <QMatrix4x4>
#include <QString>

struct FRAMEWORK_EXPORT NavigationTransformResult
{
    bool trackingAvailable = false;
    bool calibrationAvailable = false;
    bool registrationAvailable = false;
    bool valid = false;
    QString failureCode;
    QString failureText;
    QMatrix4x4 vtkToolTransform;
};

class FRAMEWORK_EXPORT NavigationTransformGraph
{
public:
    void setLatestPoseFrame(const NavigationPoseFrame& frame);
    void clearLatestPoseFrame();
    bool hasLatestPoseFrame() const;
    const NavigationPoseFrame& latestPoseFrame() const;

    void setMarkerToToolTransform(const QMatrix4x4& markerToToolTransform);
    void clearMarkerToToolTransform();
    bool hasMarkerToToolTransform() const;

    void setPatientToVtkWorldTransform(const QMatrix4x4& patientToVtkWorldTransform);
    void clearPatientToVtkWorldTransform();
    bool hasPatientToVtkWorldTransform() const;

    NavigationTransformResult compute() const;

private:
    NavigationPoseFrame m_latestPoseFrame;
    QMatrix4x4 m_markerToToolTransform;
    QMatrix4x4 m_patientToVtkWorldTransform;
    bool m_hasLatestPoseFrame = false;
    bool m_hasMarkerToToolTransform = false;
    bool m_hasPatientToVtkWorldTransform = false;
};
