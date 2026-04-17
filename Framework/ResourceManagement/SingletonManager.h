#ifndef SINGLETON_MANAGER_H
#define SINGLETON_MANAGER_H

/**
 * @brief 通用单例管理器基类模板。
 *
 * 设计要点：
 * - 使用 C++11 Meyer's Singleton（函数内静态局部变量）保证线程安全初始化；
 * - 静态局部对象在程序退出时自动析构；
 * - 禁用拷贝构造和赋值运算符，确保单例唯一性；
 * - 不依赖 Qt/CTK，可用于纯 C++ 组件，也可配合 QObject 使用。
 *
 * 使用方式示例：
 *
 * class CTKManager : public SingletonManager<CTKManager> {
 *     friend class SingletonManager<CTKManager>;
 * private:
 *     CTKManager() = default;
 * };
 *
 * // 获取实例
 * CTKManager& manager = CTKManager::instance();
 */
template<typename T>
class SingletonManager {
public:
    /**
     * @brief 获取单例实例引用（线程安全，懒加载）。
     */
    static T& instance()
    {
        // C++11 之后，函数内静态局部变量初始化是线程安全的。
        // 使用静态局部对象而非 unique_ptr，避免析构函数访问权限问题。
        static T s_instance;
        return s_instance;
    }

    // 禁用拷贝与赋值，防止出现多个实例
    SingletonManager(const SingletonManager&) = delete;
    SingletonManager& operator=(const SingletonManager&) = delete;

protected:
    SingletonManager() = default;
    ~SingletonManager() = default;
};

#endif // SINGLETON_MANAGER_H

