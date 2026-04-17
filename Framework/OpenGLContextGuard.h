#ifndef OPENGLCONTEXTGUARD_H
#define OPENGLCONTEXTGUARD_H

#include "FrameworkExport.h"

class QVTKOpenGLNativeWidget;

/**
 * @brief OpenGL上下文RAII管理类
 * 
 * 自动管理OpenGL上下文的当前化和释放，确保异常安全。
 * 
 * 使用场景：
 * - 离屏渲染
 * - 多OpenGL Widget环境
 * - 需要显式上下文管理的操作
 * 
 * 设计原则：
 * - RAII (Resource Acquisition Is Initialization)
 * - 构造时makeCurrent，析构时doneCurrent
 * - 异常安全
 * 
 * 使用示例：
 * @code
 * void MyWidget::offscreenRender() {
 *     OpenGLContextGuard guard(m_vtkWidget);
 *     
 *     // 在guard作用域内，上下文自动当前化
 *     performOpenGLOperations();
 *     m_vtkWidget->renderWindow()->Render();
 *     
 *     // guard析构时自动doneCurrent
 * }
 * @endcode
 * 
 * 注意事项：
 * - 确保widget生命周期长于guard
 * - 避免嵌套使用（可能导致上下文切换开销）
 * - 大多数情况下QVTKOpenGLNativeWidget自动管理，无需使用此类
 */
class FRAMEWORK_EXPORT OpenGLContextGuard
{
public:
    /**
     * @brief 构造函数，自动makeCurrent
     * @param widget QVTKOpenGLNativeWidget指针
     * 
     * 如果widget非空且有效，调用makeCurrent()使其OpenGL上下文当前化
     */
    explicit OpenGLContextGuard(QVTKOpenGLNativeWidget* widget);
    
    /**
     * @brief 析构函数，自动doneCurrent
     * 
     * 如果构造时成功makeCurrent，则调用doneCurrent()释放上下文
     */
    ~OpenGLContextGuard();
    
    // 禁用拷贝和赋值
    OpenGLContextGuard(const OpenGLContextGuard&) = delete;
    OpenGLContextGuard& operator=(const OpenGLContextGuard&) = delete;
    
    /**
     * @brief 检查上下文是否成功当前化
     * @return true=成功，false=失败
     */
    bool isValid() const { return m_isValid; }
    
    /**
     * @brief 获取关联的widget
     * @return QVTKOpenGLNativeWidget指针
     */
    QVTKOpenGLNativeWidget* widget() const { return m_widget; }

private:
    QVTKOpenGLNativeWidget* m_widget;  ///< 关联的VTK Widget
    bool m_isValid;                     ///< 上下文是否成功当前化
};

/**
 * @brief OpenGL上下文作用域助手宏
 * 
 * 简化OpenGLContextGuard的使用
 * 
 * 使用示例：
 * @code
 * void MyWidget::doSomething() {
 *     OPENGL_CONTEXT_SCOPE(m_vtkWidget);
 *     
 *     // OpenGL操作
 *     performOpenGLOperations();
 * }
 * @endcode
 */
#define OPENGL_CONTEXT_SCOPE(widget) \
    OpenGLContextGuard __contextGuard(widget); \
    if (!__contextGuard.isValid()) { \
        LOG_ERROR("OpenGLContextGuard", "Failed to make context current"); \
        return; \
    }

#endif // OPENGLCONTEXTGUARD_H
