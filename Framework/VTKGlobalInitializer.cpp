#include "VTKGlobalInitializer.h"
#include "Logger.h"
#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <QCoreApplication>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QMutexLocker>
#include <QThread>
#include <exception>

#ifdef VTK_FOUND
#include <QVTKOpenGLNativeWidget.h>
#include <vtkImageData.h>
#include <vtkObject.h>

// ============================================================================
// 🔥 关键修复：VTK_MODULE_INIT 只在主程序中执行
// ============================================================================
// VTK_MODULE_INIT 宏会在编译单元加载时执行全局构造函数来注册模块。
// 由于 Framework 是静态库，每个链接它的模块（主程序、各个插件DLL）
// 都会包含这些全局构造函数，导致 VTK 模块被重复注册，可能导致崩溃。
//
// 解决方案：只在主程序中执行 VTK_MODULE_INIT，插件中跳过。
// 通过检查预定义宏 MEDICALPRO_MAIN_APPLICATION 来区分。
// ============================================================================
#include <vtkAutoInit.h>
#ifdef MEDICALPRO_MAIN_APPLICATION
// 只在主程序中执行 VTK 模块初始化
VTK_MODULE_INIT(vtkRenderingOpenGL2);
VTK_MODULE_INIT(vtkInteractionStyle);
#endif

#endif

VTKGlobalInitializer::VTKGlobalInitializer()
    : m_initialized(false)
    , m_vtkFactoryInitialized(false)
    , m_lastError("")
{
}

VTKGlobalInitializer::~VTKGlobalInitializer()
{
}

VTKGlobalInitializer* VTKGlobalInitializer::instance()
{
    static VTKGlobalInitializer s_instance;
    return &s_instance;
}

QMutex& VTKGlobalInitializer::getInitMutex()
{
    // 静态局部互斥锁，保证线程安全
    // Framework现在是SHARED库，这个mutex在整个进程中唯一
    static QMutex s_initMutex;
    return s_initMutex;
}

bool VTKGlobalInitializer::isVTKAvailableInProcess()
{
#ifdef VTK_FOUND
    try {
        // 通过尝试创建VTK对象来检测VTK是否在进程级别可用
        // 这比检查本地标志更可靠，因为VTK对象工厂是进程共享的
        vtkObject* testObj = vtkImageData::New();
        if (testObj) {
            // 额外验证：检查对象类型名是否正确
            const char* className = testObj->GetClassName();
            bool valid = (className != nullptr && QString(className) == "vtkImageData");
            testObj->Delete();
            return valid;
        }
        return false;
    } catch (...) {
        return false;
    }
#else
    return false;
#endif
}

bool VTKGlobalInitializer::initialize()
{
    // 线程安全：使用互斥锁保护初始化过程
    QMutexLocker locker(&getInitMutex());

    if (m_initialized) {
        LOG_DEBUG("VTKGlobalInitializer", "Already initialized (this instance), skipping");
        return true;
    }

    // 检查是否在主线程中调用（VTK/OpenGL应在主线程初始化）
    if (auto* app = QCoreApplication::instance();
        app && QThread::currentThread() != app->thread()) {
        LOG_WARNING("VTKGlobalInitializer",
            "initialize() called from non-main thread - VTK should be initialized in main thread");
    }

    // 🔥 关键修复：检查VTK是否已在进程中初始化（由主程序完成）
    // 如果已初始化，只需要标记本实例的标志位，不要重新执行初始化
#ifdef VTK_FOUND
    // 使用更可靠的进程级检测方法
    if (isVTKAvailableInProcess()) {
        // VTK已经可用（由主程序初始化），直接标记成功
        m_vtkFactoryInitialized = true;
        m_initialized = true;
        LOG_INFO("VTKGlobalInitializer", "VTK already initialized by main application, marking local instance as ready");
        return true;
    }
    // 如果检测失败，说明 VTK 尚未初始化，继续执行完整初始化流程
    LOG_DEBUG("VTKGlobalInitializer", "VTK not yet initialized, performing full initialization...");
#endif

    LOG_INFO("VTKGlobalInitializer", "Starting global VTK initialization...");

    try {
        // 1. 设置全局OpenGL表面格式
        LOG_DEBUG("VTKGlobalInitializer", "Setting global OpenGL surface format...");
        setupGlobalSurfaceFormat();
        LOG_INFO("VTKGlobalInitializer", "✓ OpenGL surface format configured");

        // 2. 初始化VTK对象工厂
#ifdef VTK_FOUND
        LOG_DEBUG("VTKGlobalInitializer", "Initializing VTK object factory...");
        if (!initializeVTKObjectFactory()) {
            m_lastError = "VTK object factory initialization failed";
            LOG_ERROR("VTKGlobalInitializer", QString("✗ %1").arg(m_lastError));
            return false;
        }
        LOG_INFO("VTKGlobalInitializer", "✓ VTK object factory initialized successfully");
#else
        LOG_INFO("VTKGlobalInitializer", "VTK not enabled, skipping object factory initialization");
#endif

        // 3. 跳过OpenGL能力验证（避免在某些系统上卡死）
        // OpenGL能力将在第一个VTK组件创建时自动验证
        LOG_DEBUG("VTKGlobalInitializer", "Skipping OpenGL capability validation (will validate on first use)");

        m_initialized = true;
        LOG_INFO("VTKGlobalInitializer", "✓ Global VTK initialization completed");
        return true;

    } catch (const std::exception& e) {
        m_lastError = QString("Initialization exception: %1").arg(e.what());
        LOG_CRITICAL("VTKGlobalInitializer", QString("✗ %1").arg(m_lastError));
        return false;
    } catch (...) {
        m_lastError = "Unknown exception during initialization";
        LOG_CRITICAL("VTKGlobalInitializer", QString("✗ %1").arg(m_lastError));
        return false;
    }
}

bool VTKGlobalInitializer::initializeVTKObjectFactory()
{
#ifdef VTK_FOUND
    // 检查本实例是否已初始化
    if (m_vtkFactoryInitialized) {
        LOG_DEBUG("VTKGlobalInitializer", "VTK object factory already initialized (this instance)");
        return true;
    }

    // 🔥 关键：尝试创建一个VTK对象来检测VTK是否已在进程中初始化
    // 如果VTK已经被其他模块（如主程序）初始化过，这个操作会成功
    // 如果尚未初始化，VTK_MODULE_INIT宏会触发初始化
    try {
        // 尝试创建VTK对象
        // VTK的对象工厂是进程级别的，只要有一个模块初始化过，其他模块都可以使用
        vtkObject* warmup = vtkImageData::New();
        if (warmup) {
            warmup->Delete();
            m_vtkFactoryInitialized = true;
            LOG_DEBUG("VTKGlobalInitializer", "VTK object factory warmed up successfully");
            return true;
        } else {
            m_lastError = "VTK object creation failed";
            LOG_ERROR("VTKGlobalInitializer", m_lastError);
            return false;
        }
    } catch (const std::exception& e) {
        m_lastError = QString("VTK object factory initialization exception: %1").arg(e.what());
        LOG_ERROR("VTKGlobalInitializer", m_lastError);
        return false;
    } catch (...) {
        m_lastError = "Unknown exception during VTK object factory initialization";
        LOG_ERROR("VTKGlobalInitializer", m_lastError);
        return false;
    }
#else
    LOG_INFO("VTKGlobalInitializer", "VTK not enabled");
    return true;
#endif
}

void VTKGlobalInitializer::setupGlobalSurfaceFormat()
{
    // 使用推荐的表面格式并设置为全局默认
    QSurfaceFormat format = VTKGlobalInitializer::getRecommendedSurfaceFormat();
    QSurfaceFormat::setDefaultFormat(format);

    LOG_INFO("VTKGlobalInitializer", "OpenGL surface format configured from VTK default:");
    LOG_INFO_F("VTKGlobalInitializer", "  - Alpha buffer: %1", format.alphaBufferSize());
    LOG_INFO_F("VTKGlobalInitializer", "  - Depth buffer: %1", format.depthBufferSize());
    LOG_INFO_F("VTKGlobalInitializer", "  - Stencil buffer: %1", format.stencilBufferSize());
    LOG_INFO_F("VTKGlobalInitializer", "  - OpenGL version: %1.%2",
               format.majorVersion(), format.minorVersion());
    LOG_INFO_F("VTKGlobalInitializer", "  - Multisampling: %1x", format.samples());
    LOG_INFO_F("VTKGlobalInitializer", "  - Buffer mode: %1",
               format.swapBehavior() == QSurfaceFormat::DoubleBuffer ? "Double" : "Single");
}

QSurfaceFormat VTKGlobalInitializer::getRecommendedSurfaceFormat()
{
#ifdef VTK_FOUND
    // 返回 VTK 推荐的默认表面格式，不做额外修改
    QSurfaceFormat format = QVTKOpenGLNativeWidget::defaultFormat();
#else
    QSurfaceFormat format;
#endif
    return format;
}

bool VTKGlobalInitializer::validateOpenGLCapabilities()
{
    try {
        // 创建一个临时的OpenGL上下文来验证能力
        QOpenGLContext context;
        QSurfaceFormat format = getRecommendedSurfaceFormat();
        context.setFormat(format);
        
        if (!context.create()) {
            m_lastError = "Failed to create OpenGL context";
            LOG_WARNING("VTKGlobalInitializer", m_lastError);
            return false;
        }
        
        // 创建一个离屏表面用于上下文
        QOffscreenSurface surface;
        surface.setFormat(format);
        surface.create();
        
        if (!surface.isValid()) {
            m_lastError = "Failed to create offscreen surface";
            LOG_WARNING("VTKGlobalInitializer", m_lastError);
            return false;
        }
        
        // 使上下文当前化
        if (!context.makeCurrent(&surface)) {
            m_lastError = "Failed to make OpenGL context current";
            LOG_WARNING("VTKGlobalInitializer", m_lastError);
            return false;
        }
        
        // 获取OpenGL版本信息
        QSurfaceFormat actualFormat = context.format();
        int majorVersion = actualFormat.majorVersion();
        int minorVersion = actualFormat.minorVersion();
        
        LOG_INFO("VTKGlobalInitializer", "OpenGL information:");
        LOG_INFO_F("VTKGlobalInitializer", "  - Version: %1.%2", majorVersion, minorVersion);
        LOG_INFO_F("VTKGlobalInitializer", "  - Profile: %1",
                   actualFormat.profile() == QSurfaceFormat::CoreProfile ? "Core" : "Compatibility");
        
        // 检查最低版本要求（OpenGL 3.3）
        if (majorVersion < 3 || (majorVersion == 3 && minorVersion < 3)) {
            m_lastError = QString("Insufficient OpenGL version: %1.%2 (requires at least 3.3)")
                .arg(majorVersion).arg(minorVersion);
            LOG_WARNING("VTKGlobalInitializer", m_lastError);
            context.doneCurrent();
            return false;
        }
        
        LOG_INFO("VTKGlobalInitializer", "✓ OpenGL version meets requirements");

        // 释放上下文
        context.doneCurrent();

        return true;

    } catch (const std::exception& e) {
        m_lastError = QString("OpenGL capability validation exception: %1").arg(e.what());
        LOG_WARNING("VTKGlobalInitializer", m_lastError);
        return false;
    } catch (...) {
        m_lastError = "Unknown exception during OpenGL capability validation";
        LOG_WARNING("VTKGlobalInitializer", m_lastError);
        return false;
    }
}

QString VTKGlobalInitializer::getDiagnosticInfo() const
{
    QStringList info;
    info << "=== VTK Global Initializer Diagnostic ===";
    info << QString("Instance initialized: %1").arg(m_initialized ? "Yes" : "No");
    info << QString("VTK factory initialized (this instance): %1").arg(m_vtkFactoryInitialized ? "Yes" : "No");
    info << QString("VTK available in process: %1").arg(isVTKAvailableInProcess() ? "Yes" : "No");
    if (auto* app = QCoreApplication::instance()) {
        info << QString("Current thread is main: %1").arg(
            QThread::currentThread() == app->thread() ? "Yes" : "No");
    } else {
        info << "Current thread is main: Unknown (no QCoreApplication instance)";
    }

#ifdef VTK_FOUND
    info << "VTK_FOUND: Yes";
#else
    info << "VTK_FOUND: No";
#endif

#ifdef MEDICALPRO_MAIN_APPLICATION
    info << "MEDICALPRO_MAIN_APPLICATION: Defined (VTK_MODULE_INIT executed here)";
#else
    info << "MEDICALPRO_MAIN_APPLICATION: Not defined (plugin mode)";
#endif

    // OpenGL信息
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    info << QString("Default OpenGL version: %1.%2").arg(format.majorVersion()).arg(format.minorVersion());
    info << QString("Default OpenGL profile: %1").arg(
        format.profile() == QSurfaceFormat::CoreProfile ? "Core" : "Compatibility");
    info << QString("Depth buffer size: %1").arg(format.depthBufferSize());
    info << QString("Stencil buffer size: %1").arg(format.stencilBufferSize());
    info << QString("Samples (MSAA): %1").arg(format.samples());

    if (!m_lastError.isEmpty()) {
        info << QString("Last error: %1").arg(m_lastError);
    }

    info << "==========================================";
    return info.join("\n");
}
