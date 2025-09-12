#ifndef SAMPLESERVICE_H
#define SAMPLESERVICE_H

#include <QObject>
#include <QString>

/**
 * @brief 示例服务接口
 * 定义插件对外提供的服务接口
 */
class SampleService
{
public:
    virtual ~SampleService() = default;
    
    /**
     * @brief 获取服务名称
     * @return 服务名称
     */
    virtual QString getServiceName() const = 0;
    
    /**
     * @brief 获取服务版本
     * @return 服务版本
     */
    virtual QString getServiceVersion() const = 0;
    
    /**
     * @brief 执行示例操作
     * @param input 输入参数
     * @return 操作结果
     */
    virtual QString performOperation(const QString& input) = 0;
    
    /**
     * @brief 获取服务状态
     * @return 是否活动
     */
    virtual bool isActive() const = 0;
};

// Qt接口声明
Q_DECLARE_INTERFACE(SampleService, "org.medicalpro.SampleService/1.0")

#endif // SAMPLESERVICE_H
