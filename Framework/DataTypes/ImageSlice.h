#ifndef IMAGE_SLICE_H
#define IMAGE_SLICE_H

#include <cstdint>
#include <cstring>
#include <algorithm>

/**
 * @brief 小尺寸影像切片结构
 * 
 * 用于实时影像处理、切片缓存等场景。
 * 固定大小256x256像素，适合内存池分配。
 * 
 * 使用示例：
 * @code
 * // 从内存池分配
 * ImageSlice* slice = memoryManager->allocate<ImageSlice>();
 * slice->setDimensions(256, 256);
 * slice->setPixel(x, y, value);
 * 
 * // 使用完毕后归还
 * memoryManager->deallocate(slice);
 * @endcode
 * 
 * @note 此结构体大小固定为65544字节（256*256 + 8字节元数据）
 */
struct ImageSlice
{
    static constexpr int MAX_WIDTH = 256;
    static constexpr int MAX_HEIGHT = 256;
    static constexpr int MAX_SIZE = MAX_WIDTH * MAX_HEIGHT;

    uint8_t data[MAX_SIZE];  ///< 像素数据（灰度值0-255）
    int16_t width;           ///< 实际宽度
    int16_t height;          ///< 实际高度
    uint32_t sliceIndex;     ///< 切片索引

    /**
     * @brief 默认构造函数
     */
    ImageSlice()
        : width(0), height(0), sliceIndex(0)
    {
        std::memset(data, 0, MAX_SIZE);
    }

    /**
     * @brief 设置切片尺寸
     */
    void setDimensions(int w, int h)
    {
        width = static_cast<int16_t>(std::min(w, MAX_WIDTH));
        height = static_cast<int16_t>(std::min(h, MAX_HEIGHT));
    }

    /**
     * @brief 获取像素值
     */
    uint8_t getPixel(int x, int y) const
    {
        if (x < 0 || x >= width || y < 0 || y >= height) {
            return 0;
        }
        return data[y * MAX_WIDTH + x];
    }

    /**
     * @brief 设置像素值
     */
    void setPixel(int x, int y, uint8_t value)
    {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            data[y * MAX_WIDTH + x] = value;
        }
    }

    /**
     * @brief 填充整个切片
     */
    void fill(uint8_t value)
    {
        std::memset(data, value, MAX_SIZE);
    }

    /**
     * @brief 清空切片
     */
    void clear()
    {
        std::memset(data, 0, MAX_SIZE);
        width = 0;
        height = 0;
        sliceIndex = 0;
    }

    /**
     * @brief 从外部数据复制
     * @param src 源数据指针
     * @param srcWidth 源宽度
     * @param srcHeight 源高度
     * @param srcStride 源行跨度（字节）
     */
    void copyFrom(const uint8_t* src, int srcWidth, int srcHeight, int srcStride = 0)
    {
        if (!src) return;
        
        setDimensions(srcWidth, srcHeight);
        if (srcStride == 0) {
            srcStride = srcWidth;
        }

        for (int y = 0; y < height; ++y) {
            std::memcpy(&data[y * MAX_WIDTH], &src[y * srcStride], width);
        }
    }

    /**
     * @brief 复制到外部缓冲区
     */
    void copyTo(uint8_t* dst, int dstStride = 0) const
    {
        if (!dst) return;
        
        if (dstStride == 0) {
            dstStride = width;
        }

        for (int y = 0; y < height; ++y) {
            std::memcpy(&dst[y * dstStride], &data[y * MAX_WIDTH], width);
        }
    }

    /**
     * @brief 获取有效像素数量
     */
    int pixelCount() const
    {
        return width * height;
    }

    /**
     * @brief 检查是否有效
     */
    bool isValid() const
    {
        return width > 0 && height > 0;
    }
};

// 静态断言确保结构体大小合理
static_assert(sizeof(ImageSlice) == 65544, "ImageSlice size should be 65544 bytes");

#endif // IMAGE_SLICE_H

