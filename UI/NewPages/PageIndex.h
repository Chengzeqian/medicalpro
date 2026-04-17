#ifndef PAGEINDEX_H
#define PAGEINDEX_H

/**
 * @brief 页面索引枚举
 *
 * 统一管理所有页面的索引值，用于页面导航
 */
enum class PageIndex {
    Welcome = 0,           // 欢迎页
    Login = 1,             // 登录页
    ModuleSelection = 2,   // 模块选择页
    SystemSettings = 3,    // 系统设置页
    Management = 4,        // 数据管理页
    Dashboard = 5,         // 患者总览页
    Navigation = 6         // 手术导航页
};

// 方便使用的转换函数
inline int toInt(PageIndex page) {
    return static_cast<int>(page);
}

#endif // PAGEINDEX_H