#ifndef OPTICAL_REGISTRATION_SERVICE_H
#define OPTICAL_REGISTRATION_SERVICE_H

#include <QObject>
#include <QVector>
#include <QVector3D>
#include <QMatrix4x4>

class QWidget;

/**
 * @brief 单个光学配准点(一对: 影像坐标 + 光学跟踪坐标)
 */
struct OpticalRegistrationPoint
{
    QString   name;               ///< 点名称
    QVector3D imagePosition;      ///< 影像/模型坐标系中的位置
    QVector3D trackerPosition;    ///< 光学跟踪坐标系中的位置
    bool      hasImagePosition{false};
    bool      hasTrackerPosition{false};
};

/**
 * @brief 光学配准结果
 */
struct OpticalRegistrationResult
{
    bool       success{false};
    QMatrix4x4 transform;         ///< tracker -> image 的4x4变换矩阵
    double     rmsError{0.0};     ///< 均方根误差
    double     maxError{0.0};     ///< 最大误差
    int        pointCount{0};     ///< 参与配准的点数
    QString    message;           ///< 状态/错误信息
};

/**
 * @brief 光学配准服务接口
 *
 * 负责使用光学跟踪系统(如Atracsys)采集配准点, 并计算
 * tracker空間 到 影像空間 的刚体变换矩阵。
 */
class OpticalRegistrationService : public QObject
{
    Q_OBJECT

public:
    explicit OpticalRegistrationService(QObject* parent = nullptr)
        : QObject(parent) {}
    ~OpticalRegistrationService() override = default;

    // ========== 跟踪上下文配置 ==========

    /// 设置将用于光学配准的跟踪会话和工具
    virtual void setTrackingContext(const QString& sessionId,
                                    const QString& referenceToolId,
                                    const QString& pointerToolId) = 0;

    // ========== 配准点管理 ==========

    /// 新增一个配准点, 返回其索引
    virtual int addPoint(const QString& name = QString()) = 0;

    /// 设置影像坐标系中的位置
    virtual bool setImagePosition(int index, const QVector3D& position) = 0;

    /// 通过当前光学跟踪状态采集指针位置, 写入trackerPosition
    virtual bool captureTrackerPosition(int index) = 0;

    /// 设置当前活动配准点索引（用于导航四视图选点时确定作用到哪个点）
    virtual void setActivePointIndex(int index) = 0;

    /// 获取当前活动配准点索引（<0 表示未选择）
    virtual int getActivePointIndex() const = 0;

    /// 为当前活动配准点设置影像坐标（如果未选择或索引无效则返回false）
    virtual bool setImagePositionForActivePoint(const QVector3D& position) = 0;

    /// 清空所有配准点
    virtual void clearPoints() = 0;

    /// 配准点数量
    virtual int pointCount() const = 0;

    /// 获取单个配准点
    virtual OpticalRegistrationPoint getPoint(int index) const = 0;

    /// 获取所有配准点
    virtual QVector<OpticalRegistrationPoint> getAllPoints() const = 0;

    // ========== 配准执行 ==========

    /// 检查当前是否可以执行配准(至少3个完整点对)
    virtual bool canExecuteRegistration() const = 0;

    /// 执行一次配准并返回结果
    virtual OpticalRegistrationResult executeRegistration() = 0;

    /// 获取最近一次配准结果
    virtual OpticalRegistrationResult getLastResult() const = 0;

    /// 获取当前登记矩阵(tracker -> image)
    virtual QMatrix4x4 getRegistrationMatrix() const = 0;

    // ========== Widget 工厂 ==========

    /**
     * @brief 创建光学配准界面Widget（包含完整UI，旧接口）
     * @deprecated 建议使用 createVTKWidget() 并在主程序设计控制UI
     */
    virtual QWidget* createRegistrationWidget(QWidget* parent = nullptr) = 0;

    /**
     * @brief 创建纯VTK 3D视图Widget（推荐）
     * @param parent 父Widget
     * @return QWidget* 指针（实际类型为 OpticalRegistrationVTKWidget）
     * @note 只包含3D视图和工具位姿显示，不包含控制UI
     * @note 控制UI应在主程序的.ui文件中设计
     */
    virtual QWidget* createVTKWidget(QWidget* parent = nullptr) = 0;

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

    // ========== 错误信息 ==========

    /// 返回最近一次错误信息
    virtual QString getLastError() const = 0;

signals:
    void pointUpdated(int index);
    void pointsCleared();
    void registrationStarted();
    void registrationCompleted(const OpticalRegistrationResult& result);
    void registrationFailed(const QString& error);
    void progressUpdated(int progress, const QString& message);
};

// CTK服务接口声明
Q_DECLARE_INTERFACE(OpticalRegistrationService, "org.medicalpro.OpticalRegistrationService")

#endif // OPTICAL_REGISTRATION_SERVICE_H

