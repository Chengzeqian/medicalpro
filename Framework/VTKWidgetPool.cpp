#include "VTKWidgetPool.h"

#include "StartupOrchestrator.h"
#include "ErrorHandler.h"
#include "VTKGlobalInitializer.h"
#include "VTKWidgetFactory.h"

#ifdef _MSC_VER
#include <intrin.h>
#endif

#include <QMutexLocker>
#include <QPointer>
#include <QQueue>
#include <QSet>

#ifdef VTK_FOUND
#include <QVTKOpenGLNativeWidget.h>
#endif

// 单例由SingletonManager管理，不再需要静态成员和手动instance()实现

VTKWidgetPool::VTKWidgetPool()
    : QObject(nullptr)
    , m_defaultParent(nullptr)
    , m_capacity(0)
    , m_validateContext(false)
    , m_initialized(false)
{
}

VTKWidgetPool::~VTKWidgetPool()
{
    destroyAllWidgets();
}

void VTKWidgetPool::initialize(int poolSize, QWidget* defaultParent, bool validateContext)
{
#ifdef VTK_FOUND
    if (poolSize <= 0) {
        StartupOrchestrator::instance()->logDiagnostic(
            ErrorHandler::ErrorLevel::Warning,
            QStringLiteral("VTKWidgetPool 初始化时提供了无效的池大小，默认为 1"));
        poolSize = 1;
    }

    QMutexLocker locker(&m_mutex);
    if (m_initialized) {
        StartupOrchestrator::instance()->logDiagnostic(
            ErrorHandler::ErrorLevel::Info,
            QStringLiteral("VTKWidgetPool 已初始化，忽略重复初始化"));
        return;
    }

    if (!VTKGlobalInitializer::instance()->isVTKFactoryInitialized()) {
        if (!VTKGlobalInitializer::instance()->initialize()) {
            StartupOrchestrator::instance()->logDiagnostic(
                ErrorHandler::ErrorLevel::Critical,
                QStringLiteral("VTKWidgetPool 无法初始化：VTK 环境初始化失败"));
            return;
        }
    }

    m_defaultParent = defaultParent;
    m_capacity = poolSize;
    m_validateContext = validateContext;

    for (int i = 0; i < m_capacity; ++i) {
        QVTKOpenGLNativeWidget* widget = createWidget(m_defaultParent);
        if (!widget) {
            StartupOrchestrator::instance()->logDiagnostic(
                ErrorHandler::ErrorLevel::Error,
                QStringLiteral("VTKWidgetPool 创建预分配的 VTK widget 失败"));
            continue;
        }
        m_available.append(widget);
        m_allWidgets.insert(widget);
    }

    m_initialized = true;

    StartupOrchestrator::instance()->logDiagnostic(
        ErrorHandler::ErrorLevel::Info,
        QStringLiteral("VTKWidgetPool 初始化完成：容量 = %1，可用 = %2")
            .arg(m_capacity)
            .arg(m_available.size()));
#else
    Q_UNUSED(poolSize);
    Q_UNUSED(defaultParent);
    Q_UNUSED(validateContext);
    StartupOrchestrator::instance()->logDiagnostic(
        ErrorHandler::ErrorLevel::Warning,
        QStringLiteral("VTK 未启用，VTKWidgetPool 初始化被跳过"));
#endif
}

QVTKOpenGLNativeWidget* VTKWidgetPool::acquireWidget(QWidget* parent)
{
#ifdef VTK_FOUND
    QMutexLocker locker(&m_mutex);
    if (!m_initialized) {
        StartupOrchestrator::instance()->logDiagnostic(
            ErrorHandler::ErrorLevel::Warning,
            QStringLiteral("VTKWidgetPool 在未初始化时尝试获取 widget"));
        return nullptr;
    }

    if (m_available.isEmpty()) {
        StartupOrchestrator::instance()->logDiagnostic(
            ErrorHandler::ErrorLevel::Warning,
            QStringLiteral("VTKWidgetPool 可用 widget 耗尽，尝试动态扩容"));
        emit poolExhausted();

        QVTKOpenGLNativeWidget* widget = createWidget(effectiveParent(parent));
        if (!widget) {
            StartupOrchestrator::instance()->logDiagnostic(
                ErrorHandler::ErrorLevel::Error,
                QStringLiteral("VTKWidgetPool 无法动态创建 VTK widget"));
            return nullptr;
        }

        m_allWidgets.insert(widget);
        m_inUse.insert(widget);

        emit widgetAcquired(widget);
        return widget;
    }

    QVTKOpenGLNativeWidget* widget = m_available.takeLast();
    m_inUse.insert(widget);

    if (widget->parentWidget() != parent && parent) {
        widget->setParent(parent);
    }

    emit widgetAcquired(widget);
    return widget;
#else
    Q_UNUSED(parent);
    StartupOrchestrator::instance()->logDiagnostic(
        ErrorHandler::ErrorLevel::Warning,
        QStringLiteral("VTK 未启用，无法从 VTKWidgetPool 获取 widget"));
    return nullptr;
#endif
}

void VTKWidgetPool::releaseWidget(QVTKOpenGLNativeWidget* widget)
{
#ifdef VTK_FOUND
    if (!widget) {
        return;
    }

    QMutexLocker locker(&m_mutex);
    if (!m_inUse.contains(widget)) {
        return;
    }

    m_inUse.remove(widget);
    if (!m_available.contains(widget)) {
        m_available.append(widget);
    }

    emit widgetReleased(widget);
#else
    Q_UNUSED(widget);
#endif
}

QWidget* VTKWidgetPool::acquireView(QWidget* parent)
{
#ifdef VTK_FOUND
    return acquireWidget(parent);
#else
    Q_UNUSED(parent);
    return nullptr;
#endif
}

void VTKWidgetPool::releaseView(QWidget* widget)
{
#ifdef VTK_FOUND
    releaseWidget(qobject_cast<QVTKOpenGLNativeWidget*>(widget));
#else
    Q_UNUSED(widget);
#endif
}

QString VTKWidgetPool::getPoolStatus() const
{
#ifdef VTK_FOUND
    QMutexLocker locker(&m_mutex);
    return QStringLiteral("VTKWidgetPool[capacity=%1, available=%2, inUse=%3]")
        .arg(m_capacity)
        .arg(m_available.size())
        .arg(m_inUse.size());
#else
    return QStringLiteral("VTK 未启用");
#endif
}

int VTKWidgetPool::availableCount() const
{
#ifdef VTK_FOUND
    QMutexLocker locker(&m_mutex);
    return m_available.size();
#else
    return 0;
#endif
}

int VTKWidgetPool::totalCapacity() const
{
#ifdef VTK_FOUND
    QMutexLocker locker(&m_mutex);
    return m_capacity;
#else
    return 0;
#endif
}

void VTKWidgetPool::handleWidgetDestroyed(QObject* object)
{
#ifdef VTK_FOUND
    QMutexLocker locker(&m_mutex);
    auto widget = static_cast<QVTKOpenGLNativeWidget*>(object);
    m_available.removeAll(widget);
    m_inUse.remove(widget);
    m_allWidgets.remove(widget);
    if (m_initialized) {
        StartupOrchestrator::instance()->logDiagnostic(
            ErrorHandler::ErrorLevel::Warning,
            QStringLiteral("VTKWidgetPool 中的 VTK widget 被外部销毁，容量将减少"));
    }
#endif
}

QVTKOpenGLNativeWidget* VTKWidgetPool::createWidget(QWidget* parent)
{
#ifdef VTK_FOUND
    QVTKOpenGLNativeWidget* widget = VTKWidgetFactory::createVTKWidget(parent, m_validateContext);
    if (!widget) {
        return nullptr;
    }

    connect(widget, &QObject::destroyed, this, &VTKWidgetPool::handleWidgetDestroyed, Qt::UniqueConnection);
    return widget;
#else
    Q_UNUSED(parent);
    return nullptr;
#endif
}

void VTKWidgetPool::destroyAllWidgets()
{
#ifdef VTK_FOUND
    QSet<QVTKOpenGLNativeWidget*> widgets;
    {
        QMutexLocker locker(&m_mutex);
        widgets = m_allWidgets;
        m_available.clear();
        m_inUse.clear();
        m_allWidgets.clear();
        m_initialized = false;
    }

    for (QVTKOpenGLNativeWidget* widget : widgets) {
        if (widget) {
            widget->deleteLater();
        }
    }
#endif
}

QWidget* VTKWidgetPool::effectiveParent(QWidget* parent) const
{
    if (parent) {
        return parent;
    }
    return m_defaultParent;
}
