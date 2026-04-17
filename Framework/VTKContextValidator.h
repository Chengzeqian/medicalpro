#ifndef VTKCONTEXTVALIDATOR_H
#define VTKCONTEXTVALIDATOR_H

#include "FrameworkExport.h"
#include <QString>
#include <QStringList>

class QWidget;
class QVTKOpenGLNativeWidget;

/**
 * @brief VTK OpenGL上下文验证器
 * 
 * 提供OpenGL上下文验证功能，确保VTK组件在正确的OpenGL环境中初始化
 */
class FRAMEWORK_EXPORT VTKContextValidator
{
public:
    /**
     * @brief OpenGL上下文验证结果
     */
    struct ValidationResult {
        bool isValid;                    // 上下文是否有效
        QString errorMessage;            // 错误信息（如果验证失败）
        int openGLMajorVersion;          // OpenGL主版本号
        int openGLMinorVersion;          // OpenGL次版本号
        QString vendor;                  // OpenGL供应商
        QString renderer;                // OpenGL渲染器
        QString version;                 // OpenGL版本字符串
        QStringList supportedExtensions; // 支持的扩展列表
        
        ValidationResult() 
            : isValid(false)
            , openGLMajorVersion(0)
            , openGLMinorVersion(0)
        {}
    };
    
    /**
     * @brief 验证QWidget的OpenGL上下文
     * @param widget 要验证的widget
     * @return 验证结果
     */
    static ValidationResult validate(QWidget* widget);
    
    /**
     * @brief 验证QVTKWidget的OpenGL上下文
     * @param vtkWidget 要验证的VTK widget
     * @return 验证结果
     */
    static ValidationResult validateVTKWidget(QVTKOpenGLNativeWidget* vtkWidget);
    
    /**
     * @brief 检查系统OpenGL支持
     * @return 如果系统支持OpenGL 3.3+返回true
     */
    static bool checkOpenGLSupport();
    
    /**
     * @brief 验证Widget的OpenGL上下文
     * @param widget 要验证的widget
     * @return 如果上下文有效返回true
     */
    static bool validateContext(QWidget* widget);
    
    /**
     * @brief 获取上下文诊断信息
     * @param widget 要查询的widget
     * @return 诊断信息字符串
     */
    static QString getContextInfo(QWidget* widget);
    
    /**
     * @brief 检查OpenGL版本是否满足最小要求
     * @param major 主版本号
     * @param minor 次版本号
     * @return 是否满足要求
     */
    static bool checkMinimumVersion(int major, int minor);
    
    /**
     * @brief 获取详细的OpenGL信息
     * @param widget 要查询的widget
     * @return OpenGL信息字符串
     */
    static QString getOpenGLInfo(QWidget* widget);
    
private:
    static const int MIN_OPENGL_MAJOR = 3;
    static const int MIN_OPENGL_MINOR = 3;
    
    /**
     * @brief 执行实际的上下文验证逻辑
     * @param widget 要验证的widget
     * @return 验证结果
     */
    static ValidationResult performValidation(QWidget* widget);
};

#endif // VTKCONTEXTVALIDATOR_H
