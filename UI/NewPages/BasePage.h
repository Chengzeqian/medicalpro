#ifndef NEWBASEPAGE_H
#define NEWBASEPAGE_H

#include <QWidget>
#include <QMessageBox>
#include <QDebug>

/**
 * @brief 简化的页面基类
 *
 * 设计理念：
 * - 每个页面对应一个 .ui 文件
 * - 使用信号槽进行页面间导航
 * - 最小化的接口设计
 */
class BasePage : public QWidget
{
    Q_OBJECT

public:
    explicit BasePage(QWidget* parent = nullptr) : QWidget(parent) {}
    virtual ~BasePage() = default;

    /**
     * @brief 页面激活时调用（进入页面时）
     * 子类可重写以刷新数据或初始化状态
     */
    virtual void onActivated() {
        qDebug() << "[" << objectName() << "] Page activated";
    }

    /**
     * @brief 页面失活时调用（离开页面时）
     * 子类可重写以保存状态或释放资源
     */
    virtual void onDeactivated() {
        qDebug() << "[" << objectName() << "] Page deactivated";
    }

    // 兼容旧 MainInterfaceWidget 的钩子命名
    virtual void onPageActivated() { onActivated(); }
    virtual void onPageDeactivated() { onDeactivated(); }

signals:
    /**
     * @brief 请求导航到指定页面
     * @param pageIndex 目标页面索引
     */
    void navigateTo(int pageIndex);

    /**
     * @brief 请求返回上一页
     */
    void goBack();

    /**
     * @brief 请求退出系统
     */
    void exitRequested();

    /**
     * @brief 请求登出
     */
    void logoutRequested();

protected:
    // 通用工具方法
    void showInfo(const QString& title, const QString& message) {
        QMessageBox::information(this, title, message);
    }

    void showWarning(const QString& title, const QString& message) {
        QMessageBox::warning(this, title, message);
    }

    void showError(const QString& title, const QString& message) {
        QMessageBox::critical(this, title, message);
    }

    bool showConfirm(const QString& title, const QString& message) {
        return QMessageBox::question(this, title, message,
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
    }
};

#endif // NEWBASEPAGE_H
