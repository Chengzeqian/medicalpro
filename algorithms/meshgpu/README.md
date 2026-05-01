# MeshGPU

MeshGPU 是踝关节导航配准算法子系统，当前以 `MeshGPULib` 共享库形式对外导出核心 GPU 网格配准能力。

## 构建输入

- `MEDICALPRO_EIGEN_ROOT`：必须指向仓内可读的 Eigen 根目录
- CUDA：当 `MESHGPU_ENABLE_CUDA=ON` 时必须可用，核心库 `MeshGPULib` 依赖 CUDA 编译

## 构建边界

- 本目录只纳入 `include/`、`src/`、子项目 `CMakeLists.txt` 与本说明文件
- CLI、demo、Open3D 可视化和 Ascend 示例源码已保留在仓库中，但默认不构建
- 不保留外部私有路径、实验脚本目录或临时抓取产物作为构建入口

## 运行时输出

- Windows 运行时核心产物：`MeshGPULib.dll`
