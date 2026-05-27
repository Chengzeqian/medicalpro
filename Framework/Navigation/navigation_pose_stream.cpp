#include "Framework/Navigation/navigation_pose_stream.h"

NavigationPoseStream::NavigationPoseStream(int maxFrameCount)
{
    m_sampleWindow.maxFrameCount = maxFrameCount > 0 ? maxFrameCount : 1;
}

void NavigationPoseStream::pushFrame(const NavigationPoseFrame& frame)
{
    m_sampleWindow.recentFrames.append(frame);
    while (m_sampleWindow.recentFrames.size() > m_sampleWindow.maxFrameCount) {
        m_sampleWindow.recentFrames.removeFirst();
    }
}

bool NavigationPoseStream::hasLatestFrame() const
{
    return !m_sampleWindow.recentFrames.isEmpty();
}

NavigationPoseFrame NavigationPoseStream::latestFrame() const
{
    return hasLatestFrame() ? m_sampleWindow.recentFrames.last() : NavigationPoseFrame();
}

NavigationPoseSampleWindow NavigationPoseStream::sampleWindow() const
{
    return m_sampleWindow;
}

int NavigationPoseStream::maxFrameCount() const
{
    return m_sampleWindow.maxFrameCount;
}

void NavigationPoseStream::clear()
{
    m_sampleWindow.recentFrames.clear();
}
