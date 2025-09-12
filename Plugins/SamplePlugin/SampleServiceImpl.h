#ifndef SAMPLESERVICEIMPL_H
#define SAMPLESERVICEIMPL_H

#include "SampleService.h"
#include <QObject>
#include <QString>
#include <QDateTime>

#ifdef CTK_PLUGIN_FRAMEWORK
#include <ctkPluginContext.h>
#endif

/**
 * @brief 示例服务实现类
 * 实现SampleService接口，提供具体的服务功能
 */
class SampleServiceImpl : public QObject, public SampleService
{
    Q_OBJECT
    Q_INTERFACES(SampleService)

public:
    explicit SampleServiceImpl(QObject *parent = nullptr);
    
#ifdef CTK_PLUGIN_FRAMEWORK
    explicit SampleServiceImpl(ctkPluginContext* context, QObject *parent = nullptr);
#endif
    
    ~SampleServiceImpl() override;

    // SampleService interface implementation
    QString getServiceName() const override;
    QString getServiceVersion() const override;
    QString performOperation(const QString& input) override;
    bool isActive() const override;

public slots:
    /**
     * @brief 启动服务
     */
    void startService();
    
    /**
     * @brief 停止服务
     */
    void stopService();

signals:
    /**
     * @brief 服务状态改变信号
     * @param active 是否活动
     */
    void serviceStatusChanged(bool active);
    
    /**
     * @brief 操作执行信号
     * @param operation 操作描述
     * @param result 操作结果
     */
    void operationPerformed(const QString& operation, const QString& result);

private:
#ifdef CTK_PLUGIN_FRAMEWORK
    ctkPluginContext* m_context;
#endif
    bool m_active;
    QDateTime m_startTime;
    int m_operationCount;
    
    void logMessage(const QString& message);
};

#endif // SAMPLESERVICEIMPL_H
