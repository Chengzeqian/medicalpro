#ifndef VTKWIDGETFACTORY_H
#define VTKWIDGETFACTORY_H

#include "FrameworkExport.h"
#include <QWidget>
#include <QSurfaceFormat>

// 前向声明
class QVTKOpenGLNativeWidget;

/**
 * @brief VTKWidget标准化工厂
 * 
 * 提供标准配置的QVTKOpenGLNativeWidget创建，确保：
 * - 统一的OpenGL表面格式配置
 * - 统一的widget属性设置
 * - 防闪烁和渲染优化
 * - VTK对象工厂初始化检查
 * - OpenGL上下文验证
 * 
 * 所有VTK组件应使用此工厂创建QVTKWidget，而不是直接new
 */
class FRAMEWORK_EXPORT VTKWidgetFactory
{
public:
    /**
     * @brief 创建VTK Widget（推荐方法）
     * 
     * 创建一个完全配置好的QVTKOpenGLNativeWidget实例，包括：
     * - 检查VTK对象工厂是否初始化
     * - 应用标准OpenGL表面格式
     * - 设置防闪烁属性
     * - 设置最小尺寸
     * - Widget验证逻辑
     * - 集成VTKContextValidator
     * - 详细的创建日志
     * 
     * @param parent 父widget
     * @param validateContext 是否验证OpenGL上下文（默认false，因为上下文在显示后才创建）
     * @return 配置好的QVTKOpenGLNativeWidget指针，失败返回nullptr
     */
    static QVTKOpenGLNativeWidget* createVTKWidget(QWidget* parent = nullptr, bool validateContext = false);
    
    /**
     * @brief 创建标准配置的QVTKWidget（兼容旧代码）
     * 
     * 创建一个完全配置好的QVTKOpenGLNativeWidget实例
     * 
     * @param parent 父widget
     * @return 配置好的QVTKOpenGLNativeWidget指针
     */
    static QVTKOpenGLNativeWidget* createStandardVTKWidget(QWidget* parent = nullptr);
    
    /**
     * @brief 应用标准属性到现有widget
     * 
     * 对已存在的QVTKWidget应用标准配置，包括：
     * - 防闪烁属性（WA_OpaquePaintEvent等）
     * - OpenGL表面格式
     * - 最小尺寸
     * 
     * @param widget 要配置的widget
     */
    static void applyStandardAttributes(QVTKOpenGLNativeWidget* widget);
    
    /**
     * @brief 获取标准OpenGL表面格式
     *
     * 返回适用于VTK的标准OpenGL表面格式配置：
     * - 深度缓冲：24位
     * - 模板缓冲：8位
     * - OpenGL版本：3.3 Core Profile
     * - 多重采样：4x MSAA
     * - 双缓冲模式
     *
     * @return QSurfaceFormat对象
     */
    static QSurfaceFormat getStandardSurfaceFormat();

    /**
     * @brief 暂停VTK渲染（用于页面切换防闪烁）
     *
     * 递归查找widget及其子widget中的所有QVTKOpenGLNativeWidget，
     * 并暂停其渲染，防止页面切换时的闪烁
     *
     * @param widget 要暂停渲染的widget树根节点
     */
    static void pauseVTKRendering(QWidget* widget);

    /**
     * @brief 恢复VTK渲染
     *
     * 恢复之前被暂停的VTK渲染
     *
     * @param widget 要恢复渲染的widget树根节点
     */
    static void resumeVTKRendering(QWidget* widget);

private:
    /**
     * @brief 设置防闪烁属性
     * 
     * 设置Qt widget属性以减少闪烁和提高渲染性能
     * 
     * @param widget 要配置的widget
     */
    static void setAntiFlickerAttributes(QWidget* widget);
    
    /**
     * @brief 设置渲染属性
     * 
     * 设置OpenGL渲染相关的widget属性
     * 
     * @param widget 要配置的widget
     */
    static void setRenderingAttributes(QWidget* widget);
};

#endif // VTKWIDGETFACTORY_H
