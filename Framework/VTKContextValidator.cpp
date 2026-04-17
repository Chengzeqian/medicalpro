#include "VTKContextValidator.h"
#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <QWidget>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QVTKOpenGLNativeWidget.h>
#include <QDebug>
#include <QSurfaceFormat>
#include <QOffscreenSurface>
#include <exception>

VTKContextValidator::ValidationResult VTKContextValidator::validate(QWidget* widget)
{
    ValidationResult result;
    
    if (!widget) {
        result.errorMessage = "Widget指针为空";
        qWarning() << "[VTKContextValidator] 验证失败:" << result.errorMessage;
        return result;
    }
    
    // 检查widget是否可见
    if (!widget->isVisible()) {
        result.errorMessage = "Widget未显示，OpenGL上下文可能未创建";
        qWarning() << "[VTKContextValidator]" << result.errorMessage;
        return result;
    }
    
    // 检查widget尺寸
    if (widget->width() <= 0 || widget->height() <= 0) {
        result.errorMessage = QString("Widget尺寸无效: %1x%2")
            .arg(widget->width())
            .arg(widget->height());
        qWarning() << "[VTKContextValidator]" << result.errorMessage;
        return result;
    }
    
    // 执行实际验证
    return performValidation(widget);
}

VTKContextValidator::ValidationResult VTKContextValidator::validateVTKWidget(QVTKOpenGLNativeWidget* vtkWidget)
{
    ValidationResult result;
    
    if (!vtkWidget) {
        result.errorMessage = "VTK Widget指针为空";
        qWarning() << "[VTKContextValidator] 验证失败:" << result.errorMessage;
        return result;
    }
    
    // 检查VTK widget特定属性
    if (!vtkWidget->isValid()) {
        result.errorMessage = "QVTKOpenGLNativeWidget未正确初始化";
        qWarning() << "[VTKContextValidator]" << result.errorMessage;
        return result;
    }
    
    // 使用通用验证逻辑
    return validate(static_cast<QWidget*>(vtkWidget));
}

bool VTKContextValidator::checkOpenGLSupport()
{
    qDebug() << "[VTKContextValidator] 检查系统OpenGL支持...";
    
    try {
        // 创建临时OpenGL上下文进行检查
        QOpenGLContext context;
        QSurfaceFormat format;
        format.setVersion(MIN_OPENGL_MAJOR, MIN_OPENGL_MINOR);
        format.setProfile(QSurfaceFormat::CoreProfile);
        context.setFormat(format);
        
        if (!context.create()) {
            qWarning() << "[VTKContextValidator] 无法创建OpenGL上下文";
            return false;
        }
        
        // 创建离屏表面
        QOffscreenSurface surface;
        surface.setFormat(format);
        surface.create();
        
        if (!surface.isValid()) {
            qWarning() << "[VTKContextValidator] 无法创建离屏表面";
            return false;
        }
        
        // 激活上下文
        if (!context.makeCurrent(&surface)) {
            qWarning() << "[VTKContextValidator] 无法激活OpenGL上下文";
            return false;
        }
        
        // 获取实际版本
        QSurfaceFormat actualFormat = context.format();
        int major = actualFormat.majorVersion();
        int minor = actualFormat.minorVersion();
        
        qDebug() << "[VTKContextValidator] 系统OpenGL版本:" << major << "." << minor;
        
        context.doneCurrent();
        
        // 检查版本是否满足要求
        bool supported = checkMinimumVersion(major, minor);
        if (supported) {
            qDebug() << "[VTKContextValidator] ✓ 系统支持OpenGL" << MIN_OPENGL_MAJOR << "." << MIN_OPENGL_MINOR << "+";
        } else {
            qWarning() << "[VTKContextValidator] ✗ 系统OpenGL版本不足，需要" 
                      << MIN_OPENGL_MAJOR << "." << MIN_OPENGL_MINOR << "+";
        }
        
        return supported;
        
    } catch (const std::exception& e) {
        qWarning() << "[VTKContextValidator] OpenGL支持检查异常:" << e.what();
        return false;
    } catch (...) {
        qWarning() << "[VTKContextValidator] OpenGL支持检查未知异常";
        return false;
    }
}

bool VTKContextValidator::validateContext(QWidget* widget)
{
    if (!widget) {
        qWarning() << "[VTKContextValidator] Widget指针为空";
        return false;
    }
    
    ValidationResult result = validate(widget);
    return result.isValid;
}

QString VTKContextValidator::getContextInfo(QWidget* widget)
{
    if (!widget) {
        return "Widget指针为空";
    }
    
    ValidationResult result = validate(widget);
    
    QString info;
    info += "OpenGL上下文诊断信息:\n";
    info += QString("  状态: %1\n").arg(result.isValid ? "✓ 有效" : "✗ 无效");
    
    if (!result.isValid) {
        info += QString("  错误: %1\n").arg(result.errorMessage);
    }
    
    if (result.openGLMajorVersion > 0) {
        info += QString("  OpenGL版本: %1.%2\n")
            .arg(result.openGLMajorVersion)
            .arg(result.openGLMinorVersion);
    }
    
    if (!result.version.isEmpty()) {
        info += QString("  版本字符串: %1\n").arg(result.version);
    }
    
    if (!result.vendor.isEmpty()) {
        info += QString("  供应商: %1\n").arg(result.vendor);
    }
    
    if (!result.renderer.isEmpty()) {
        info += QString("  渲染器: %1\n").arg(result.renderer);
    }
    
    if (!result.supportedExtensions.isEmpty()) {
        info += QString("  扩展数量: %1\n").arg(result.supportedExtensions.size());
    }
    
    return info;
}

bool VTKContextValidator::checkMinimumVersion(int major, int minor)
{
    if (major > MIN_OPENGL_MAJOR) {
        return true;
    }
    
    if (major == MIN_OPENGL_MAJOR && minor >= MIN_OPENGL_MINOR) {
        return true;
    }
    
    return false;
}

QString VTKContextValidator::getOpenGLInfo(QWidget* widget)
{
    if (!widget) {
        return "无效的Widget";
    }
    
    ValidationResult result = validate(widget);
    
    QString info;
    info += "OpenGL上下文信息:\n";
    info += QString("  状态: %1\n").arg(result.isValid ? "有效" : "无效");
    info += QString("  版本: %1.%2\n").arg(result.openGLMajorVersion).arg(result.openGLMinorVersion);
    info += QString("  版本字符串: %1\n").arg(result.version);
    info += QString("  供应商: %1\n").arg(result.vendor);
    info += QString("  渲染器: %1\n").arg(result.renderer);
    
    if (!result.isValid) {
        info += QString("  错误: %1\n").arg(result.errorMessage);
    }
    
    if (!result.supportedExtensions.isEmpty()) {
        info += QString("  扩展数量: %1\n").arg(result.supportedExtensions.size());
    }
    
    return info;
}

VTKContextValidator::ValidationResult VTKContextValidator::performValidation(QWidget* widget)
{
    ValidationResult result;
    
    // 获取当前OpenGL上下文
    QOpenGLContext* context = QOpenGLContext::currentContext();
    
    if (!context) {
        result.errorMessage = "无法获取OpenGL上下文，上下文可能未创建";
        qWarning() << "[VTKContextValidator]" << result.errorMessage;
        return result;
    } else {
        // 从上下文获取详细信息
        QSurfaceFormat format = context->format();
        result.openGLMajorVersion = format.majorVersion();
        result.openGLMinorVersion = format.minorVersion();
        
        qDebug() << "[VTKContextValidator] OpenGL版本:"
                 << result.openGLMajorVersion << "." << result.openGLMinorVersion;
        
        // 获取OpenGL函数以查询更多信息
        QOpenGLFunctions* functions = context->functions();
        if (functions) {
            // 获取供应商、渲染器和版本字符串
            const GLubyte* vendorStr = functions->glGetString(GL_VENDOR);
            const GLubyte* rendererStr = functions->glGetString(GL_RENDERER);
            const GLubyte* versionStr = functions->glGetString(GL_VERSION);
            
            if (vendorStr) {
                result.vendor = QString::fromLatin1(reinterpret_cast<const char*>(vendorStr));
            }
            if (rendererStr) {
                result.renderer = QString::fromLatin1(reinterpret_cast<const char*>(rendererStr));
            }
            if (versionStr) {
                result.version = QString::fromLatin1(reinterpret_cast<const char*>(versionStr));
            }
            
            qDebug() << "[VTKContextValidator] 供应商:" << result.vendor;
            qDebug() << "[VTKContextValidator] 渲染器:" << result.renderer;
            qDebug() << "[VTKContextValidator] 版本字符串:" << result.version;
            
            // 获取扩展列表（OpenGL 3.0+）
            // Note: glGetStringi is not available in base QOpenGLFunctions
            // We'll skip extension enumeration for now as it's not critical
            qDebug() << "[VTKContextValidator] 扩展列表查询已跳过（需要OpenGL 3.0+ functions）";
        }
    }
    
    // 检查版本是否满足最小要求
    if (!checkMinimumVersion(result.openGLMajorVersion, result.openGLMinorVersion)) {
        result.errorMessage = QString("OpenGL版本不足: 需要%1.%2+，当前为%3.%4")
            .arg(MIN_OPENGL_MAJOR)
            .arg(MIN_OPENGL_MINOR)
            .arg(result.openGLMajorVersion)
            .arg(result.openGLMinorVersion);
        qWarning() << "[VTKContextValidator]" << result.errorMessage;
        return result;
    }
    
    // 验证成功
    result.isValid = true;
    qDebug() << "[VTKContextValidator] OpenGL上下文验证成功";
    
    return result;
}
