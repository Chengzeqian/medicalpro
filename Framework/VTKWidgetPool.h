#ifndef VTKWIDGETPOOL_H
#define VTKWIDGETPOOL_H

#include "FrameworkExport.h"
#include "ResourceManagement/SingletonManager.h"

#include <QObject>
#include <QMutex>
#include <QPointer>
#include <QSet>
#include <QVector>

class QWidget;
class QVTKOpenGLNativeWidget;

/**
 * @brief VTK Widget对象池
 *
 * 使用SingletonManager模式管理单例生命周期（需求6.1-6.5）
 * 与VTKObjectManager集成管理VTK对象生命周期
 */
class FRAMEWORK_EXPORT VTKWidgetPool : public QObject, public SingletonManager<VTKWidgetPool>
{
    Q_OBJECT
    friend class SingletonManager<VTKWidgetPool>;

public:
    /**
     * @brief 获取单例实例指针（兼容性接口）
     * @return 单例实例指针
     */
    static VTKWidgetPool* instance() { return &SingletonManager<VTKWidgetPool>::instance(); }

    void initialize(int poolSize, QWidget* defaultParent = nullptr, bool validateContext = false);

    QVTKOpenGLNativeWidget* acquireWidget(QWidget* parent = nullptr);
    void releaseWidget(QVTKOpenGLNativeWidget* widget);

    QString getPoolStatus() const;
    int availableCount() const;
    int totalCapacity() const;

signals:
    void widgetAcquired(QVTKOpenGLNativeWidget* widget);
    void widgetReleased(QVTKOpenGLNativeWidget* widget);
    void poolExhausted();

private slots:
    void handleWidgetDestroyed(QObject* object);

private:
    VTKWidgetPool();
    ~VTKWidgetPool() override;

    QVTKOpenGLNativeWidget* createWidget(QWidget* parent);
    void destroyAllWidgets();

    QWidget* effectiveParent(QWidget* parent) const;

    mutable QMutex m_mutex;
    QVector<QVTKOpenGLNativeWidget*> m_available;
    QSet<QVTKOpenGLNativeWidget*> m_inUse;
    QSet<QVTKOpenGLNativeWidget*> m_allWidgets;

    QWidget* m_defaultParent;
    int m_capacity;
    bool m_validateContext;
    bool m_initialized;
};

#endif // VTKWIDGETPOOL_H
