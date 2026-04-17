#ifndef POINT3D_H
#define POINT3D_H

#include <cstdint>
#include <cmath>

/**
 * @brief 高频传感器数据点结构
 * 
 * 用于光学追踪、传感器采集等高频数据场景。
 * 设计为固定大小（32字节），适合内存池分配。
 * 
 * 使用示例：
 * @code
 * // 从内存池分配
 * Point3D* point = realtimeThread->allocate<Point3D>();
 * point->x = 1.0;
 * point->y = 2.0;
 * point->z = 3.0;
 * point->timestamp = getCurrentTimestamp();
 * 
 * // 使用完毕后归还
 * realtimeThread->deallocate(point);
 * @endcode
 * 
 * @note 此结构体大小固定为32字节，适合FixedSizeMemoryPool
 */
struct Point3D
{
    double x;           ///< X坐标（mm）
    double y;           ///< Y坐标（mm）
    double z;           ///< Z坐标（mm）
    uint64_t timestamp; ///< 时间戳（微秒）

    /**
     * @brief 默认构造函数
     */
    Point3D()
        : x(0.0), y(0.0), z(0.0), timestamp(0)
    {
    }

    /**
     * @brief 带参数构造函数
     */
    Point3D(double px, double py, double pz, uint64_t ts = 0)
        : x(px), y(py), z(pz), timestamp(ts)
    {
    }

    /**
     * @brief 计算到另一点的距离
     */
    double distanceTo(const Point3D& other) const
    {
        double dx = x - other.x;
        double dy = y - other.y;
        double dz = z - other.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    /**
     * @brief 计算向量长度
     */
    double length() const
    {
        return std::sqrt(x * x + y * y + z * z);
    }

    /**
     * @brief 归一化（原地修改）
     */
    void normalize()
    {
        double len = length();
        if (len > 1e-10) {
            x /= len;
            y /= len;
            z /= len;
        }
    }

    /**
     * @brief 重置为零
     */
    void reset()
    {
        x = 0.0;
        y = 0.0;
        z = 0.0;
        timestamp = 0;
    }

    /**
     * @brief 检查是否有效（非NaN）
     */
    bool isValid() const
    {
        return !std::isnan(x) && !std::isnan(y) && !std::isnan(z);
    }

    // 运算符重载
    Point3D operator+(const Point3D& other) const
    {
        return Point3D(x + other.x, y + other.y, z + other.z, timestamp);
    }

    Point3D operator-(const Point3D& other) const
    {
        return Point3D(x - other.x, y - other.y, z - other.z, timestamp);
    }

    Point3D operator*(double scalar) const
    {
        return Point3D(x * scalar, y * scalar, z * scalar, timestamp);
    }

    bool operator==(const Point3D& other) const
    {
        return x == other.x && y == other.y && z == other.z;
    }
};

// 静态断言确保结构体大小为32字节
static_assert(sizeof(Point3D) == 32, "Point3D must be 32 bytes for memory pool optimization");

#endif // POINT3D_H

