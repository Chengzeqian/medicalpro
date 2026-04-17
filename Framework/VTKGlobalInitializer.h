#ifndef VTKGLOBALINITIALIZER_H
#define VTKGLOBALINITIALIZER_H

#include <QString>
#include <QSurfaceFormat>
#include <QMutex>
#include "FrameworkExport.h"

/**
 * @brief VTK全局初始化管理器
 *
 * 负责在应用启动时初始化全局VTK环境，包括：
 * - VTK对象工厂初始化
 * - OpenGL表面格式配置
 * - OpenGL能力验证
 *
 * 使用单例模式确保全局只有一个实例
 *
 * @note Framework现在是SHARED库（DLL），单例在整个进程中真正全局唯一。
 *       主程序和所有插件共享同一个VTKGlobalInitializer实例。
 *
 * @note VTK_MODULE_INIT宏通过MEDICALPRO_MAIN_APPLICATION条件编译，
 *       确保只在主程序中执行，避免DLL重复注册导致的问题。
 *
 * @note 线程安全：initialize()方法使用互斥锁保护
 */
class FRAMEWORK_EXPORT VTKGlobalInitializer
{
public:
    /**
     * @brief 获取单例实例
     * @return VTKGlobalInitializer实例指针
     */
    static VTKGlobalInitializer* instance();
    
    /**
     * @brief 初始化全局VTK环境
     * 
     * 应在main()函数开始处调用，早于QApplication创建
     * 
     * @return 初始化成功返回true，失败返回false
     */
    bool initialize();
    
    /**
     * @brief 验证OpenGL能力
     * 
     * 检查系统OpenGL版本和扩展支持
     * 
     * @return 验证通过返回true，失败返回false
     */
    bool validateOpenGLCapabilities();
    
    /**
     * @brief 获取推荐的OpenGL表面格式
     * 
     * 返回适用于VTK的标准OpenGL表面格式配置
     * 
     * @return QSurfaceFormat对象
     */
    static QSurfaceFormat getRecommendedSurfaceFormat();
    
    /**
     * @brief 检查VTK对象工厂是否已初始化
     * @return 已初始化返回true，否则返回false
     */
    bool isVTKFactoryInitialized() const { return m_vtkFactoryInitialized; }

    /**
     * @brief 获取最后一次错误信息
     * @return 错误信息字符串
     */
    QString getLastError() const { return m_lastError; }

    /**
     * @brief 获取VTK诊断信息
     * @return 包含VTK状态、OpenGL信息等的诊断字符串
     */
    QString getDiagnosticInfo() const;

    /**
     * @brief 检查VTK是否在进程级别已初始化（跨DLL检测）
     * @return VTK对象工厂可用返回true
     * @note 此方法通过尝试创建VTK对象来检测，比检查本地标志更可靠
     */
    static bool isVTKAvailableInProcess();
    
private:
    VTKGlobalInitializer();
    ~VTKGlobalInitializer();
    
    // 禁用拷贝构造和赋值
    VTKGlobalInitializer(const VTKGlobalInitializer&) = delete;
    VTKGlobalInitializer& operator=(const VTKGlobalInitializer&) = delete;
    
    /**
     * @brief 初始化VTK对象工厂
     * 
     * 通过创建并销毁一个VTK对象来触发对象工厂初始化
     * 确保跨DLL的VTK对象创建使用同一个工厂实例
     * 
     * @return 初始化成功返回true，失败返回false
     */
    bool initializeVTKObjectFactory();
    
    /**
     * @brief 设置全局OpenGL表面格式
     *
     * 配置深度缓冲、模板缓冲、OpenGL版本、多重采样等
     */
    void setupGlobalSurfaceFormat();

    /**
     * @brief 获取初始化互斥锁（线程安全）
     * @return 静态互斥锁引用
     */
    static QMutex& getInitMutex();

private:
    bool m_initialized;              ///< 是否已初始化（本实例）
    bool m_vtkFactoryInitialized;    ///< VTK对象工厂是否已初始化
    QString m_lastError;             ///< 最后一次错误信息
};

#endif // VTKGLOBALINITIALIZER_H
