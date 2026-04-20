#ifndef PAGEINDEX_H
#define PAGEINDEX_H

/**
 * @brief 页面索引枚举
 *
 * 统一管理所有页面的索引值，用于页面导航
 */
enum class PageIndex {
    Welcome = 0,
    Login = 1,
    ModuleSelection = 2,
    SystemSettings = 3,
    Management = 4,
    Dashboard = 5,
    Navigation = 6,
    Diagnostics = 7
};

inline int toInt(PageIndex page)
{
    return static_cast<int>(page);
}

#endif // PAGEINDEX_H
