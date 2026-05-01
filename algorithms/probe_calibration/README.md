# ProbeCalibration

`ProbeCalibration` 是踝关节导航探针尖端标定算法子系统，负责采集 Atracsys 探针位姿、执行 tip calibration，并导出 `ProbeCalibration.dll` 供上层集成调用。

## 构建输入

- `MEDICALPRO_EIGEN_ROOT`
- `MEDICALPRO_ATRACSYS_SDK_DIR`

## 运行时输出

- `ProbeCalibration.dll`

## 目录边界

- 仅收口核心 `include/` 与 `src/`
- 默认不构建 standalone tools、geometry 工具、collection test、atnet discover
- 不包含外部开发专用的 `build/`、`geometry/`、`.bat` 脚本和临时产物
