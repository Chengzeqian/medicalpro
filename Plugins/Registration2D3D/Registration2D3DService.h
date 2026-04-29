#ifndef REGISTRATION2D3D_SERVICE_H
#define REGISTRATION2D3D_SERVICE_H

/**
 * @file Registration2D3DService.h
 * @brief 2D3D配准服务接口
 * 
 * 提供2D-3D医学图像配准的完整服务接口
 * 支持双视角（AP+LAT）配准、GPU加速DRR生成、CMA-ES优化
 */

#include "Registration2D3DDataStructures.h"
#include <QObject>

/**
 * @brief 2D3D配准服务接口
 * 
 * 这是一个纯虚接口，遵循平台服务契约。
 * 实现类负责：
 * - Python环境初始化和管理
 * - 调用Python配准算法
 * - 异步配准任务管理
 * - 配准结果存储和查询
 */
class Registration2D3DService : public QObject
{
    Q_OBJECT
    
public:
    explicit Registration2D3DService(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~Registration2D3DService() = default;
    
    // ========== 配准执行 ==========
    
    /**
     * @brief 启动2D3D配准任务（异步）
     * @param params 配准参数
     * @return 成功返回配准ID，失败返回空字符串
     * 
     * 该方法会在后台线程执行配准，不阻塞主线程。
     * 通过信号progressUpdated和registrationCompleted获取进度和结果。
     */
    virtual QString startRegistration(const Registration2D3DParameters& params) = 0;
    
    /**
     * @brief 取消正在运行的配准任务
     * @param registrationId 配准ID
     * @return 成功返回true，失败返回false
     */
    virtual bool cancelRegistration(const QString& registrationId) = 0;
    
    /**
     * @brief 同步执行配准（阻塞方法，用于测试）
     * @param params 配准参数
     * @param result 输出配准结果
     * @return 成功返回true，失败返回false
     */
    virtual bool executeRegistrationSync(const Registration2D3DParameters& params, 
                                         Registration2D3DResult& result) = 0;
    
    // ========== 结果查询 ==========
    
    /**
     * @brief 获取配准结果
     * @param registrationId 配准ID
     * @return 配准结果
     */
    virtual Registration2D3DResult getRegistrationResult(const QString& registrationId) = 0;
    
    /**
     * @brief 获取所有配准历史
     * @return 配准结果列表（按时间倒序）
     */
    virtual QList<Registration2D3DResult> getRegistrationHistory() = 0;
    
    /**
     * @brief 获取指定患者的配准历史
     * @param patientId 患者ID
     * @return 配准结果列表
     */
    virtual QList<Registration2D3DResult> getRegistrationHistoryByPatient(const QString& patientId) = 0;
    
    /**
     * @brief 删除配准记录
     * @param registrationId 配准ID
     * @return 成功返回true，失败返回false
     */
    virtual bool deleteRegistration(const QString& registrationId) = 0;
    
    // ========== 统计信息 ==========
    
    /**
     * @brief 获取配准统计信息
     * @return 统计信息
     */
    virtual Registration2D3DStatistics getStatistics() = 0;
    
    // ========== Python环境管理 ==========
    
    /**
     * @brief 初始化Python环境
     * @param pythonHome Python主目录路径
     * @param scriptsPath Python脚本路径
     * @return 成功返回true，失败返回false
     */
    virtual bool initializePythonEnvironment(const QString& pythonHome, 
                                            const QString& scriptsPath) = 0;
    
    /**
     * @brief 检查Python环境是否已初始化
     * @return 已初始化返回true，未初始化返回false
     */
    virtual bool isPythonInitialized() = 0;
    
    /**
     * @brief 清理Python环境
     */
    virtual void finalizePythonEnvironment() = 0;
    
    // ========== 配置管理 ==========
    
    /**
     * @brief 设置配准配置
     * @param key 配置键
     * @param value 配置值
     */
    virtual void setConfiguration(const QString& key, const QVariant& value) = 0;
    
    /**
     * @brief 获取配准配置
     * @param key 配置键
     * @param defaultValue 默认值
     * @return 配置值
     */
    virtual QVariant getConfiguration(const QString& key, const QVariant& defaultValue = QVariant()) = 0;
    
    // ========== 工具方法 ==========
    
    /**
     * @brief 验证配准参数
     * @param params 配准参数
     * @param errorMessage 输出错误消息
     * @return 有效返回true，无效返回false
     */
    virtual bool validateParameters(const Registration2D3DParameters& params, 
                                    QString& errorMessage) = 0;
    
    /**
     * @brief 生成DRR预览图像（用于参数调试）
     * @param ctPath CT图像路径
     * @param params 配准参数
     * @param view 视角（"ap" 或 "lat"）
     * @param outputPath 输出图像路径
     * @return 成功返回true，失败返回false
     */
    virtual bool generateDRRPreview(const QString& ctPath, 
                                   const QVector<double>& params,
                                   const QString& view,
                                   const QString& outputPath) = 0;
    
    /**
     * @brief 获取最后一次错误信息
     * @return 错误信息字符串
     */
    virtual QString getLastError() const = 0;
    
    // ========== UI 组件工厂 ==========

    /**
     * @brief 创建一个独立的2D3D配准 Widget 组件
     * @param parent 父 Widget
     * @return QWidget 指针（实际类型为 Registration2D3DWidget），由调用者负责管理生命周期
     * @note 每次调用都会创建一个全新的、独立的配准组件实例
     * @note 多个实例互不干扰，可同时在不同界面使用
     * @note 返回 QWidget* 以避免主程序需要包含 Registration2D3DWidget 头文件
     */
    virtual QWidget* createRegistrationWidget(QWidget* parent = nullptr) = 0;

    // ========== VTK渲染控制（防闪烁） ==========

    /**
     * @brief 暂停VTK渲染
     * @note 在页面切换前调用，防止隐藏的VTK Widget继续渲染导致闪烁
     */
    virtual void pauseRendering() = 0;

    /**
     * @brief 恢复VTK渲染
     * @note 在页面切换后调用
     */
    virtual void resumeRendering() = 0;

signals:
    /**
     * @brief 配准开始信号
     * @param registrationId 配准ID
     */
    void registrationStarted(const QString& registrationId);
    
    /**
     * @brief 配准进度更新信号
     * @param registrationId 配准ID
     * @param progress 进度信息
     */
    void progressUpdated(const QString& registrationId, 
                        const Registration2D3DProgress& progress);
    
    /**
     * @brief 配准完成信号
     * @param registrationId 配准ID
     * @param result 配准结果
     */
    void registrationCompleted(const QString& registrationId, 
                              const Registration2D3DResult& result);
    
    /**
     * @brief 配准失败信号
     * @param registrationId 配准ID
     * @param errorMessage 错误消息
     */
    void registrationFailed(const QString& registrationId, 
                           const QString& errorMessage);
    
    /**
     * @brief 配准取消信号
     * @param registrationId 配准ID
     */
    void registrationCancelled(const QString& registrationId);
};

// 服务接口声明（必需）
Q_DECLARE_INTERFACE(Registration2D3DService, "com.medicalpro.Registration2D3DService")

#endif // REGISTRATION2D3D_SERVICE_H
