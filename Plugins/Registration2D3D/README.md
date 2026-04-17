# Registration2D3D CTK插件

## 📋 插件概述

Registration2D3D是一个基于CTK插件框架的2D-3D医学图像配准插件，集成了先进的Python配准算法、GPU加速DRR生成和CMA-ES优化算法。

### 核心功能

- ✅ **双视角配准**：支持AP（前后位）和LAT（侧位）两个视角的同时配准
- ✅ **GPU加速**：使用GPU加速的Siddon算法快速生成DRR（数字重建X射线）
- ✅ **智能优化**：采用CMA-ES（协方差矩阵自适应演化策略）进行全局优化
- ✅ **空间划分**：K-d树空间划分实现多起点优化，避免局部最优
- ✅ **GO度量**：梯度方向（Gradient Orientation）相似性度量
- ✅ **异步执行**：后台线程执行配准，不阻塞主界面
- ✅ **结果存储**：配准结果自动保存到数据库，支持历史查询
- ✅ **验证图像**：自动生成DRR、棋盘格、边缘叠加等验证图像

---

## 🏗️ 插件架构

```
Registration2D3D/
├── Registration2D3DActivator.h/cpp         # CTK插件激活器
├── Registration2D3DService.h               # 服务接口定义
├── Registration2D3DServiceImpl.h/cpp       # 服务实现（含Python调用）
├── Registration2D3DDataStructures.h        # 数据结构定义
├── CMakeLists.txt                          # 构建配置
├── MANIFEST.MF                             # 插件清单
└── README.md                               # 本文档
```

### 服务架构

```
┌─────────────────────────────────────────────────────────────┐
│                  Registration2D3DService                     │
│                      (服务接口层)                             │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│              Registration2D3DServiceImpl                     │
│                    (服务实现层)                               │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  - 参数验证                                           │  │
│  │  - Python环境管理                                     │  │
│  │  - 工作线程调度                                       │  │
│  │  - 结果存储与查询                                     │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                Registration2D3DWorker                        │
│                   (工作线程层)                                │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  - Python C API调用                                   │  │
│  │  - 进度报告                                           │  │
│  │  │  - 异常处理                                        │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    Python配准算法层                          │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  2D3DRegistration02.py (主流程)                       │  │
│  │  ├─ changedTest()  - 双视角配准入口                   │  │
│  │  ├─ test()         - 单视角配准                       │  │
│  │  ├─ space_partition() - K-d树空间划分                │  │
│  │  └─ local_optimize()  - CMA-ES优化                   │  │
│  │                                                          │  │
│  │  ProjectorsModule_multi.py (DRR生成)                  │  │
│  │  └─ SiddonGpu类                                        │  │
│  │     ├─ update()    - GPU加速DRR生成                   │  │
│  │     └─ GO_metric() - 梯度方向度量计算                 │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

---

## 🚀 快速开始

### 1. 环境准备

#### Python环境
```bash
# 确保Python 3.7+已安装
python --version

# 安装必需的Python包
pip install numpy cma matplotlib SimpleITK torch
```

#### Python脚本部署
将以下Python脚本复制到应用程序目录的`Regi/`子目录：
- `2D3DRegistration02.py` - 主配准算法
- `ProjectorsModule_multi.py` - DRR生成模块
- `RigidMotion.py` - 刚体变换矩阵工具
- `SiddonGpuPy` - GPU加速库（.pyd或.so）

目录结构：
```
medicalpro.exe
├── Python/            # Python环境（可选）
└── Regi/              # Python脚本（必需）
    ├── 2D3DRegistration02.py
    ├── ProjectorsModule_multi.py
    ├── RigidMotion.py
    └── SiddonGpuPy.pyd
```

### 2. 构建插件

```bash
cd build
cmake .. -DENABLE_PLUGIN_REGISTRATION_2D3D=ON
cmake --build . --config Release
```

### 3. 基本使用

#### C++代码示例

```cpp
#include <ctkPluginContext.h>
#include <ctkServiceReference.h>
#include "Registration2D3DService.h"

// 1. 获取服务
ctkPluginContext* context = framework->getPluginContext();
ctkServiceReference ref = context->getServiceReference<Registration2D3DService>();
Registration2D3DService* regService = 
    context->getService<Registration2D3DService>(ref);

if (!regService) {
    qCritical() << "配准服务不可用";
    return;
}

// 2. 准备配准参数
Registration2D3DParameters params;
params.ctPath = "D:/data/patient001/ct.nrrd";
params.xrayApPath = "D:/data/patient001/xray_ap.png";
params.xrayLatPath = "D:/data/patient001/xray_lat.png";
params.jingguPath = "D:/models/tibia.stl";

// 初始参数：[rx, ry, rz, tx, ty, tz]
params.initParams = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
params.searchRange = {15, 15, 15, 50, 50, 50};  // 度和毫米
params.kdTreeNum = 50;  // K-d树划分数量

params.generateDRR = true;
params.outputDirectory = "D:/data/patient001/registration_results/";

// 3. 连接信号
connect(regService, &Registration2D3DService::progressUpdated,
        this, [](const QString& id, const Registration2D3DProgress& progress) {
    qDebug() << "进度:" << progress.percentage << "%" 
             << progress.currentPhase << progress.message;
});

connect(regService, &Registration2D3DService::registrationCompleted,
        this, [](const QString& id, const Registration2D3DResult& result) {
    qDebug() << "配准完成!";
    qDebug() << "AP结果: rx=" << result.apResult.rx 
             << "ry=" << result.apResult.ry 
             << "rz=" << result.apResult.rz;
    qDebug() << "LAT结果: rx=" << result.latResult.rx 
             << "ry=" << result.latResult.ry 
             << "rz=" << result.latResult.rz;
    qDebug() << "耗时:" << result.durationSeconds << "秒";
});

// 4. 启动配准（异步）
QString registrationId = regService->startRegistration(params);
if (registrationId.isEmpty()) {
    qCritical() << "启动配准失败:" << regService->getLastError();
    return;
}

qDebug() << "配准任务已启动，ID:" << registrationId;
```

#### 同步执行示例

```cpp
// 同步执行（阻塞，适用于测试或批处理）
Registration2D3DResult result;
bool success = regService->executeRegistrationSync(params, result);

if (success) {
    qDebug() << "配准成功!";
    qDebug() << "AP: [" << result.apResult.rx << "," 
             << result.apResult.ry << "," 
             << result.apResult.rz << "]";
} else {
    qCritical() << "配准失败:" << regService->getLastError();
}
```

---

## 📊 配准参数详解

### Registration2D3DParameters

| 参数 | 类型 | 说明 | 默认值 |
|------|------|------|--------|
| `ctPath` | QString | CT图像路径（.nrrd/.mhd/.nii） | - |
| `xrayApPath` | QString | AP视角X射线图像路径 | - |
| `xrayLatPath` | QString | LAT视角X射线图像路径 | - |
| `jingguPath` | QString | 胫骨模型路径（用于边缘验证） | - |
| `initParams` | QVector\<double\> | 初始配准参数 [rx,ry,rz,tx,ty,tz] | [0,0,0,0,0,0] |
| `searchRange` | QVector\<int\> | 搜索范围 [rx,ry,rz,tx,ty,tz] | [15,15,15,50,50,50] |
| `kdTreeNum` | int | K-d树划分数量 | 50 |
| `apUpDown` | bool | AP视角上下翻转 | false |
| `apHorizontal` | bool | AP视角水平翻转 | false |
| `latUpDown` | bool | LAT视角上下翻转 | false |
| `latHorizontal` | bool | LAT视角水平翻转 | false |
| `generateDRR` | bool | 是否生成验证图像 | true |
| `outputDirectory` | QString | 输出目录 | "" |

### 参数说明

#### 初始参数（initParams）
- **rx, ry, rz**：X、Y、Z轴旋转角度（单位：度）
- **tx, ty, tz**：X、Y、Z轴平移距离（单位：毫米）

#### 搜索范围（searchRange）
- 定义每个参数的搜索空间大小
- 例如：`searchRange[0]=15` 表示rx的搜索范围是 `[initParams[0]-15, initParams[0]+15]` 度

#### K-d树数量（kdTreeNum）
- 空间划分数量，越大搜索越细致，但计算时间越长
- 推荐值：50-100

---

## 📈 配准结果

### Registration2D3DResult

```cpp
struct Registration2D3DResult {
    QString registrationId;              // 唯一标识
    QDateTime startTime;                 // 开始时间
    QDateTime endTime;                   // 结束时间
    int durationSeconds;                 // 耗时（秒）
    Status status;                       // 状态
    
    SingleViewRegistrationResult apResult;   // AP视角结果
    SingleViewRegistrationResult latResult;  // LAT视角结果
    
    double finalMetric;                  // 最终度量值
    int totalIterations;                 // 总迭代次数
};

struct SingleViewRegistrationResult {
    QString viewName;                    // "AP" 或 "LAT"
    double rx, ry, rz;                   // 旋转（度）
    double tx, ty, tz;                   // 平移（毫米）
    double goMetric;                     // GO度量值
    
    QString drrImagePath;                // DRR图像路径
    QString checkerboardPath;            // 棋盘格图像路径
    QString edgeOverlayPath;             // 边缘叠加图像路径
};
```

### 验证图像

配准完成后会生成以下验证图像（如果`generateDRR=true`）：

1. **DRR图像**（`222222Ap.png`, `222222Lat.png`）
   - 根据配准参数生成的数字重建X射线图像
   
2. **棋盘格图像**（`333333Ap.png`, `333333Lat.png`）
   - X射线和DRR交替显示，用于视觉比对
   
3. **边缘叠加图像**（`444444Ap.png`, `444444Lat.png`）
   - 胫骨模型边缘叠加在X射线图像上

---

## 🔧 高级功能

### 配准历史查询

```cpp
// 获取所有配准历史
QList<Registration2D3DResult> history = regService->getRegistrationHistory();
for (const auto& result : history) {
    qDebug() << result.registrationId 
             << result.startTime 
             << result.getStatusString();
}

// 按患者查询
QList<Registration2D3DResult> patientHistory = 
    regService->getRegistrationHistoryByPatient("patient001");
```

### 配准统计

```cpp
Registration2D3DStatistics stats = regService->getStatistics();
qDebug() << "总配准次数:" << stats.totalRegistrations;
qDebug() << "成功次数:" << stats.successfulRegistrations;
qDebug() << "平均耗时:" << stats.averageDuration << "秒";
qDebug() << "平均度量值:" << stats.averageMetric;
```

### 取消配准

```cpp
QString regId = regService->startRegistration(params);
// ... 用户点击取消按钮
bool cancelled = regService->cancelRegistration(regId);
```

### DRR预览生成

```cpp
// 生成DRR预览（调试参数用）
QVector<double> testParams = {5.0, -3.0, 2.0, 10.0, -5.0, 20.0};
bool success = regService->generateDRRPreview(
    "D:/data/ct.nrrd",
    testParams,
    "ap",  // 或 "lat"
    "D:/output/preview_drr.png"
);
```

---

## 🐛 故障排除

### 常见问题

#### 1. "Python环境未初始化"

**原因**：
- Python脚本路径配置错误
- Python环境未正确安装
- Python DLL未找到

**解决方案**：
```cpp
// 手动初始化Python环境
QString pythonHome = "D:/Python39";  // Python安装目录
QString scriptsPath = "D:/medicalpro/Regi";  // 脚本目录
bool success = regService->initializePythonEnvironment(pythonHome, scriptsPath);
if (!success) {
    qDebug() << "错误:" << regService->getLastError();
}
```

#### 2. "无法导入Python模块"

**原因**：
- Python脚本文件缺失
- Python路径未正确添加
- 依赖包未安装

**检查清单**：
```bash
# 检查脚本文件
ls Regi/2D3DRegistration02.py
ls Regi/ProjectorsModule_multi.py

# 检查Python包
python -c "import numpy; print(numpy.__version__)"
python -c "import cma; print(cma.__version__)"
python -c "import torch; print(torch.__version__)"
```

#### 3. "GPU加速不可用"

**原因**：
- CUDA未安装
- GPU驱动过旧
- SiddonGpuPy编译版本不匹配

**解决方案**：
- 安装CUDA 10.2+
- 更新NVIDIA驱动到最新版本
- 重新编译SiddonGpuPy库

#### 4. 配准精度不佳

**优化建议**：
1. 增加K-d树数量：`params.kdTreeNum = 100;`
2. 扩大搜索范围：`params.searchRange = {30, 30, 30, 100, 100, 100};`
3. 提供更好的初始参数
4. 检查图像质量和对比度
5. 确保X射线和CT的解剖结构匹配

---

## 📚 技术细节

### 算法原理

1. **Siddon算法**：GPU加速的射线追踪算法，快速生成DRR
2. **GO度量**：比较DRR和X射线图像的梯度方向一致性
3. **K-d树划分**：将参数空间自适应划分为多个子空间
4. **CMA-ES优化**：协方差矩阵自适应演化策略，全局+局部优化
5. **双视角融合**：同时优化AP和LAT两个视角，提高鲁棒性

### 性能指标

- **DRR生成速度**：~50ms/帧（GPU，512×512）
- **单视角配准时间**：2-5分钟（50个起点）
- **配准精度**：旋转 < 1°，平移 < 2mm（理想条件）

### 数据库表结构

```sql
CREATE TABLE registration_2d3d (
    registration_id TEXT PRIMARY KEY,
    patient_id TEXT,
    start_time TEXT,
    end_time TEXT,
    duration_seconds INTEGER,
    status TEXT,
    error_message TEXT,
    ct_path TEXT,
    xray_ap_path TEXT,
    xray_lat_path TEXT,
    init_params TEXT,
    search_range TEXT,
    kd_tree_num INTEGER,
    ap_rx REAL, ap_ry REAL, ap_rz REAL,
    ap_tx REAL, ap_ty REAL, ap_tz REAL,
    ap_metric REAL,
    lat_rx REAL, lat_ry REAL, lat_rz REAL,
    lat_tx REAL, lat_ty REAL, lat_tz REAL,
    lat_metric REAL,
    final_metric REAL,
    total_iterations INTEGER
);
```

---

## 🔗 相关文档

- [CTK插件框架文档](../docs/CTK完全架构插件开发范式.md)
- [InstrumentManagement插件参考](../InstrumentManagement/README.md)
- [Python配准算法说明](../../docs/2D3DRegistration技术文档.md)

---

## 📝 许可证

Copyright (c) 2024 MedicalPro. All rights reserved.

---

## 👥 贡献者

- 插件架构设计：MedicalPro团队
- Python算法集成：基于现有2D3DRegistration实现
- CTK插件封装：2024年11月

---

## 📞 支持

如有问题或建议，请联系：support@medicalpro.com

