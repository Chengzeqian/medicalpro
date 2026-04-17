#include "VTKWidgetFactory.h"
#include "VTKGlobalInitializer.h"
#include "VTKContextValidator.h"
#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <QDebug>

#ifdef VTK_FOUND
#include <QVTKOpenGLNativeWidget.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#endif

QVTKOpenGLNativeWidget* VTKWidgetFactory::createVTKWidget(QWidget* parent, bool validateContext)
{
#ifdef VTK_FOUND
    qDebug() << "[VTKWidgetFactory] Starting VTK widget creation...";
    qDebug() << "[VTKWidgetFactory]   - Parent widget:" << (parent ? "provided" : "none");
    qDebug() << "[VTKWidgetFactory]   - Context validation:" << (validateContext ? "enabled" : "disabled");

    try {
        qDebug() << "[VTKWidgetFactory] Step 1: checking VTK object factory...";
        if (!VTKGlobalInitializer::instance()->isVTKFactoryInitialized()) {
            qDebug() << "[VTKWidgetFactory] Local VTK factory flag is not set; validating/initializing...";
            if (!VTKGlobalInitializer::instance()->initialize()) {
                qCritical() << "[VTKWidgetFactory] VTK object factory initialization failed:"
                            << VTKGlobalInitializer::instance()->getLastError();
                return nullptr;
            }
            qDebug() << "[VTKWidgetFactory] VTK object factory validation/initialization succeeded";
        } else {
            qDebug() << "[VTKWidgetFactory] VTK object factory already initialized";
        }

        qDebug() << "[VTKWidgetFactory] Step 2: creating QVTKOpenGLNativeWidget instance...";
        QVTKOpenGLNativeWidget* widget = new QVTKOpenGLNativeWidget(parent);
        if (!widget) {
            qCritical() << "[VTKWidgetFactory] Failed to create QVTKOpenGLNativeWidget";
            return nullptr;
        }
        qDebug() << "[VTKWidgetFactory] QVTKOpenGLNativeWidget instance created";

        qDebug() << "[VTKWidgetFactory] Step 3: applying standard attributes...";
        applyStandardAttributes(widget);
        qDebug() << "[VTKWidgetFactory] Standard attributes applied";

        qDebug() << "[VTKWidgetFactory] Step 4: setting minimum size...";
        widget->setMinimumSize(400, 300);
        qDebug() << "[VTKWidgetFactory] Minimum size set to 400x300";

        qDebug() << "[VTKWidgetFactory] Step 5: performing basic widget validation...";
        if (!widget->isValid()) {
            qWarning() << "[VTKWidgetFactory] Widget is not fully initialized yet: QVTKOpenGLNativeWidget::isValid() returned false";
            qWarning() << "[VTKWidgetFactory] This is expected before the widget is shown";
        } else {
            qDebug() << "[VTKWidgetFactory] Basic widget validation passed";
        }

        if (validateContext) {
            qDebug() << "[VTKWidgetFactory] Step 6: OpenGL context validation...";
            if (VTKContextValidator::validateContext(widget)) {
                qDebug() << "[VTKWidgetFactory] OpenGL context validation passed";
                const QString contextInfo = VTKContextValidator::getContextInfo(widget);
                qDebug() << "[VTKWidgetFactory] Context information:\n" << contextInfo;
            } else {
                qWarning() << "[VTKWidgetFactory] OpenGL context validation failed";
                qWarning() << "[VTKWidgetFactory] The widget may not be visible yet; creation will continue";
            }
        } else {
            qDebug() << "[VTKWidgetFactory] Step 6: OpenGL context validation skipped (not requested)";
        }

        qDebug() << "[VTKWidgetFactory] VTK widget creation completed";
        return widget;

    } catch (const std::exception& e) {
        qCritical() << "[VTKWidgetFactory] Exception while creating VTK widget:" << e.what();
        return nullptr;
    } catch (...) {
        qCritical() << "[VTKWidgetFactory] Unknown exception while creating VTK widget";
        return nullptr;
    }

#else
    qWarning() << "[VTKWidgetFactory] VTK support is disabled, widget creation is unavailable";
    Q_UNUSED(parent);
    Q_UNUSED(validateContext);
    return nullptr;
#endif
}

QVTKOpenGLNativeWidget* VTKWidgetFactory::createStandardVTKWidget(QWidget* parent)
{
    return createVTKWidget(parent, false);
}

void VTKWidgetFactory::applyStandardAttributes(QVTKOpenGLNativeWidget* widget)
{
#ifdef VTK_FOUND
    if (!widget) {
        qWarning() << "[VTKWidgetFactory] Widget pointer is null; standard attributes skipped";
        return;
    }

    qDebug() << "[VTKWidgetFactory] Applying standard attributes...";

    setAntiFlickerAttributes(widget);
    setRenderingAttributes(widget);

    const QSurfaceFormat format = getStandardSurfaceFormat();
    widget->setFormat(format);
    qDebug() << "[VTKWidgetFactory] OpenGL surface format applied";
    qDebug() << "[VTKWidgetFactory] Standard attribute application completed";
#else
    qWarning() << "[VTKWidgetFactory] VTK support is disabled";
    Q_UNUSED(widget);
#endif
}

QSurfaceFormat VTKWidgetFactory::getStandardSurfaceFormat()
{
#ifdef VTK_FOUND
    QSurfaceFormat format = VTKGlobalInitializer::getRecommendedSurfaceFormat();
#else
    QSurfaceFormat format;
#endif
    return format;
}

void VTKWidgetFactory::setAntiFlickerAttributes(QWidget* widget)
{
    if (!widget) {
        return;
    }

    qDebug() << "[VTKWidgetFactory]   - Using a minimal attribute set to avoid transparency and crash issues";
}

void VTKWidgetFactory::setRenderingAttributes(QWidget* widget)
{
    if (!widget) {
        return;
    }

    qDebug() << "[VTKWidgetFactory]   - Skipping WA_PaintOnScreen to avoid OpenGL conflicts";
}

void VTKWidgetFactory::pauseVTKRendering(QWidget* widget)
{
#ifdef VTK_FOUND
    if (!widget) {
        return;
    }

    QVTKOpenGLNativeWidget* vtkWidget = qobject_cast<QVTKOpenGLNativeWidget*>(widget);
    if (vtkWidget) {
        vtkRenderWindow* renderWindow = vtkWidget->renderWindow();
        if (renderWindow) {
            vtkRenderWindowInteractor* interactor = renderWindow->GetInteractor();
            if (interactor) {
                interactor->Disable();
            }
            renderWindow->SetSwapBuffers(false);
            qDebug() << "[VTKWidgetFactory] Paused VTK rendering for:" << widget->objectName();
        }
    }

    for (QObject* child : widget->children()) {
        QWidget* childWidget = qobject_cast<QWidget*>(child);
        if (childWidget) {
            pauseVTKRendering(childWidget);
        }
    }
#else
    Q_UNUSED(widget);
#endif
}

void VTKWidgetFactory::resumeVTKRendering(QWidget* widget)
{
#ifdef VTK_FOUND
    if (!widget) {
        return;
    }

    QVTKOpenGLNativeWidget* vtkWidget = qobject_cast<QVTKOpenGLNativeWidget*>(widget);
    if (vtkWidget && vtkWidget->isVisible()) {
        vtkRenderWindow* renderWindow = vtkWidget->renderWindow();
        if (renderWindow) {
            renderWindow->SetSwapBuffers(true);
            vtkRenderWindowInteractor* interactor = renderWindow->GetInteractor();
            if (interactor) {
                interactor->Enable();
            }
            renderWindow->Render();
            qDebug() << "[VTKWidgetFactory] Resumed VTK rendering for:" << widget->objectName();
        }
    }

    for (QObject* child : widget->children()) {
        QWidget* childWidget = qobject_cast<QWidget*>(child);
        if (childWidget) {
            resumeVTKRendering(childWidget);
        }
    }
#else
    Q_UNUSED(widget);
#endif
}
