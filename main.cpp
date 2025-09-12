#include "mainwindow.h"
#include "Framework/CTKManager.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QDir>
#include <QDebug>
#include <QSurfaceFormat>
#include <QOpenGLWidget>
#include <QMessageBox>
#include <QThread>
#include <QThreadPool>
#include <QTimer>
#include <exception>

#ifdef VTK_FOUND
#include <QVTKOpenGLNativeWidget.h>
#endif

int main(int argc, char *argv[])
{
    // 设置OpenGL表面格式以确保VTK兼容性
#ifdef VTK_FOUND
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
#else
    // 设置基本OpenGL格式
    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);
#endif
    
    QApplication a(argc, argv);

    // Setup application information
    a.setApplicationName("Medical Pro");
    a.setApplicationVersion("1.0");
    a.setOrganizationName("Medical Solutions");

    // Setup internationalization
    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "medicalpro_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }
    
    // Initialize CTK Plugin Framework
    CTKManager* ctkManager = CTKManager::instance();
    
    if (ctkManager->isCTKAvailable() || ctkManager->initializeFramework(&a)) {
        qDebug() << "CTK Plugin Framework initialization attempt...";
        
        if (ctkManager->startFramework()) {
            qDebug() << "CTK Plugin Framework started successfully";
            
            // Load plugins from plugins directory
            QString pluginsPath = QCoreApplication::applicationDirPath() + "/plugins";
            QDir pluginsDir(pluginsPath);
            
            if (pluginsDir.exists()) {
                qDebug() << "Loading plugins from:" << pluginsPath;
                int loadedCount = ctkManager->loadPluginsFromDirectory(pluginsPath);
                qDebug() << "Loaded" << loadedCount << "plugins";
                
                // Show loaded plugins
                QStringList loadedPlugins = ctkManager->getLoadedPlugins();
                if (!loadedPlugins.isEmpty()) {
                    qDebug() << "Loaded plugins:" << loadedPlugins;
                } else {
                    qDebug() << "No plugins found in plugins directory";
                }
            } else {
                qDebug() << "Plugins directory does not exist:" << pluginsPath;
                qDebug() << "Creating plugins directory...";
                pluginsDir.mkpath(".");
            }
        } else {
            qDebug() << "Failed to start CTK Plugin Framework";
        }
    } else {
        qDebug() << "CTK Plugin Framework not available or initialization failed";
    }
    
    // Create and show main window with error handling
    qDebug() << "Creating main window...";
    
    try {
        MainWindow w;
        qDebug() << "Main window created successfully";
        
        // Add a small delay to ensure all plugin services are fully ready
        QApplication::processEvents();
        
        w.show();
        qDebug() << "Main window shown successfully";
        
        // Setup shutdown handling for CTK
        QObject::connect(&a, &QApplication::aboutToQuit, [&]() {
            qDebug() << "Application shutting down...";
            qDebug() << "开始强制清理所有资源...";
            
            // 强制处理所有待处理事件
            QApplication::processEvents();
            
            if (ctkManager) {
                qDebug() << "停止CTK框架...";
                try {
                    ctkManager->stopFramework();
                    
                    // 等待CTK框架完全停止
                    QThread::msleep(1000);  // 增加等待时间
                    QApplication::processEvents();
                    
                    qDebug() << "CTK框架已停止";
                } catch (const std::exception& e) {
                    qDebug() << "停止CTK框架时发生异常:" << e.what();
                } catch (...) {
                    qDebug() << "停止CTK框架时发生未知异常";
                }
            }
            
            // 强制清理Qt全局线程池
            qDebug() << "强制清理Qt全局线程池...";
            QThreadPool::globalInstance()->clear();
            
            // 限时等待线程池清理完成
            bool done = false;
            for (int i = 0; i < 30 && !done; ++i) {  // 最多等待3秒
                QThread::msleep(100);
                QApplication::processEvents();
                done = QThreadPool::globalInstance()->waitForDone(1);  // 很短的超时检查
            }
            
            if (!done) {
                qWarning() << "Qt线程池清理超时";
            } else {
                qDebug() << "Qt线程池清理完成";
            }
            
            // 最后一次强制处理事件
            QApplication::processEvents();
            qDebug() << "应用程序清理完成";
        });
        
        qDebug() << "Starting main event loop...";
        
        // 设置强制退出定时器（安全网）
        QTimer forceExitTimer;
        forceExitTimer.setSingleShot(true);
        QObject::connect(&forceExitTimer, &QTimer::timeout, [&]() {
            qWarning() << "应用程序退出超时，强制终止进程...";
            QCoreApplication::exit(0);  // 强制退出
            
            // 如果QCoreApplication::exit()不起作用，使用更强制的方法
            QTimer::singleShot(1000, []() {
                qFatal("应用程序无法正常退出，强制终止");
            });
        });
        
        // 当aboutToQuit信号发出时，启动强制退出定时器
        QObject::connect(&a, &QApplication::aboutToQuit, [&forceExitTimer]() {
            forceExitTimer.start(10000);  // 10秒后强制退出
        });
        
        int result = a.exec();
        
        // 如果正常退出，取消强制退出定时器
        forceExitTimer.stop();
        
        return result;
        
    } catch (const std::exception& e) {
        qDebug() << "Exception during main window creation:" << e.what();
        QMessageBox::critical(nullptr, "Fatal Error", 
                            QString("Application failed to start: %1").arg(e.what()));
        return -1;
    } catch (...) {
        qDebug() << "Unknown exception during main window creation";
        QMessageBox::critical(nullptr, "Fatal Error", 
                            "Application failed to start due to unknown error");
        return -1;
    }
}
