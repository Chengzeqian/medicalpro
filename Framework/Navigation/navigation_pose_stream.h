#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Navigation/navigation_pose_frame.h"

class FRAMEWORK_EXPORT NavigationPoseStream
{
public:
    explicit NavigationPoseStream(int maxFrameCount = 10);

    void pushFrame(const NavigationPoseFrame& frame);
    bool hasLatestFrame() const;
    NavigationPoseFrame latestFrame() const;
    NavigationPoseSampleWindow sampleWindow() const;
    int maxFrameCount() const;
    void clear();

private:
    NavigationPoseSampleWindow m_sampleWindow;
};
