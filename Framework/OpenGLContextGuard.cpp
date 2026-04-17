#include "OpenGLContextGuard.h"
#include "Logger.h"
#include <QVTKOpenGLNativeWidget.h>

OpenGLContextGuard::OpenGLContextGuard(QVTKOpenGLNativeWidget* widget)
    : m_widget(widget)
    , m_isValid(false)
{
    if (!m_widget) {
        LOG_WARNING("OpenGLContextGuard", "Widget is null, cannot make context current");
        return;
    }
    
    // 尝试使OpenGL上下文当前化
    try {
        m_widget->makeCurrent();
        m_isValid = true;
        LOG_DEBUG("OpenGLContextGuard", "OpenGL context made current");
    } catch (const std::exception& e) {
        LOG_ERROR_F("OpenGLContextGuard", "Failed to make context current: %1", e.what());
        m_isValid = false;
    } catch (...) {
        LOG_ERROR("OpenGLContextGuard", "Unknown exception while making context current");
        m_isValid = false;
    }
}

OpenGLContextGuard::~OpenGLContextGuard()
{
    if (m_widget && m_isValid) {
        try {
            m_widget->doneCurrent();
            LOG_DEBUG("OpenGLContextGuard", "OpenGL context released");
        } catch (const std::exception& e) {
            LOG_ERROR_F("OpenGLContextGuard", "Exception during doneCurrent: %1", e.what());
        } catch (...) {
            LOG_ERROR("OpenGLContextGuard", "Unknown exception during doneCurrent");
        }
    }
}
