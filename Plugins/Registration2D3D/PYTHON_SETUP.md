# Python环境配置指南

## 📋 系统要求

### Python版本
- **Python 3.7** 或更高版本
- 推荐：**Python 3.8 或 3.9**

### GPU要求（强烈推荐）
- **NVIDIA GPU**（支持CUDA）
- **CUDA Toolkit 10.2** 或更高版本（推荐11.x）
- **最新的NVIDIA驱动**

---

## 🚀 快速安装（推荐）

### 步骤1：检查Python版本

```bash
python --version
# 应输出：Python 3.7.x 或更高
```

如果没有Python，请从 [python.org](https://www.python.org/downloads/) 下载安装。

### 步骤2：检查CUDA（GPU加速必需）

```bash
nvidia-smi
```

应该看到GPU信息和CUDA版本。如果没有，请先安装NVIDIA驱动和CUDA。

### 步骤3：安装依赖包

```bash
# 进入插件目录
cd D:\Qtproject\medicalpro\Plugins\Registration2D3D

# 安装所有依赖（GPU版本，CUDA 11.x）
pip install numpy>=1.19.0
pip install cma>=3.0.0
pip install matplotlib>=3.3.0
pip install SimpleITK>=2.0.0
pip install scipy>=1.5.0
pip install Pillow>=8.0.0

# PyTorch GPU版本（最重要！）
pip install torch==1.10.0+cu113 torchvision==0.11.0+cu113 --extra-index-url https://download.pytorch.org/whl/cu113
```

或者使用requirements.txt一键安装：

```bash
# 先编辑requirements.txt，根据您的CUDA版本选择对应的torch版本
pip install -r requirements.txt
```

---

## 📦 分步安装说明

### 1. 安装NumPy（数值计算）

```bash
pip install numpy>=1.19.0
```

**验证**：
```python
python -c "import numpy; print('NumPy版本:', numpy.__version__)"
```

### 2. 安装CMA（优化算法）

```bash
pip install cma>=3.0.0
```

**验证**：
```python
python -c "import cma; print('CMA版本:', cma.__version__)"
```

### 3. 安装Matplotlib（可视化）

```bash
pip install matplotlib>=3.3.0
```

**验证**：
```python
python -c "import matplotlib; print('Matplotlib版本:', matplotlib.__version__)"
```

### 4. 安装SimpleITK（医学图像处理）

```bash
pip install SimpleITK>=2.0.0
```

**验证**：
```python
python -c "import SimpleITK as sitk; print('SimpleITK版本:', sitk.Version_VersionString())"
```

### 5. 安装PyTorch（GPU加速，最关键！）

#### 选项A：CUDA 11.3（推荐）
```bash
pip install torch==1.10.0+cu113 torchvision==0.11.0+cu113 --extra-index-url https://download.pytorch.org/whl/cu113
```

#### 选项B：CUDA 11.1
```bash
pip install torch==1.10.0+cu111 torchvision==0.11.0+cu111 --extra-index-url https://download.pytorch.org/whl/cu111
```

#### 选项C：CUDA 10.2
```bash
pip install torch==1.10.0+cu102 torchvision==0.11.0+cu102 --extra-index-url https://download.pytorch.org/whl/cu102
```

**验证GPU支持**：
```python
python -c "import torch; print('PyTorch版本:', torch.__version__); print('CUDA可用:', torch.cuda.is_available()); print('GPU数量:', torch.cuda.device_count())"
```

**输出示例**：
```
PyTorch版本: 1.10.0+cu113
CUDA可用: True
GPU数量: 1
```

⚠️ **如果CUDA可用显示False，配准速度会非常慢！**

### 6. 安装可选依赖

```bash
pip install scipy>=1.5.0
pip install Pillow>=8.0.0
```

---

## 🔧 完整安装脚本

### Windows PowerShell脚本

创建 `install_python_deps.ps1`：

```powershell
# Python依赖安装脚本 - Windows
Write-Host "开始安装Registration2D3D Python依赖..." -ForegroundColor Green

# 检查Python
Write-Host "`n检查Python版本..." -ForegroundColor Yellow
python --version

# 升级pip
Write-Host "`n升级pip..." -ForegroundColor Yellow
python -m pip install --upgrade pip

# 安装基础依赖
Write-Host "`n安装基础依赖..." -ForegroundColor Yellow
pip install numpy>=1.19.0
pip install cma>=3.0.0
pip install matplotlib>=3.3.0
pip install SimpleITK>=2.0.0
pip install scipy>=1.5.0
pip install Pillow>=8.0.0

# 检查CUDA版本
Write-Host "`n检查CUDA版本..." -ForegroundColor Yellow
nvidia-smi

# 安装PyTorch（CUDA 11.3）
Write-Host "`n安装PyTorch GPU版本（CUDA 11.3）..." -ForegroundColor Yellow
pip install torch==1.10.0+cu113 torchvision==0.11.0+cu113 --extra-index-url https://download.pytorch.org/whl/cu113

# 验证安装
Write-Host "`n验证安装..." -ForegroundColor Yellow
python -c "import numpy; print('NumPy:', numpy.__version__)"
python -c "import cma; print('CMA:', cma.__version__)"
python -c "import matplotlib; print('Matplotlib:', matplotlib.__version__)"
python -c "import SimpleITK; print('SimpleITK:', SimpleITK.Version_VersionString())"
python -c "import torch; print('PyTorch:', torch.__version__); print('CUDA可用:', torch.cuda.is_available())"

Write-Host "`n安装完成！" -ForegroundColor Green
```

**运行**：
```powershell
powershell -ExecutionPolicy Bypass -File install_python_deps.ps1
```

### Linux/macOS Bash脚本

创建 `install_python_deps.sh`：

```bash
#!/bin/bash
# Python依赖安装脚本 - Linux/macOS

echo "开始安装Registration2D3D Python依赖..."

# 检查Python
echo -e "\n检查Python版本..."
python3 --version

# 升级pip
echo -e "\n升级pip..."
python3 -m pip install --upgrade pip

# 安装基础依赖
echo -e "\n安装基础依赖..."
pip3 install numpy>=1.19.0
pip3 install cma>=3.0.0
pip3 install matplotlib>=3.3.0
pip3 install SimpleITK>=2.0.0
pip3 install scipy>=1.5.0
pip3 install Pillow>=8.0.0

# 安装PyTorch（CUDA 11.3）
echo -e "\n安装PyTorch GPU版本（CUDA 11.3）..."
pip3 install torch==1.10.0+cu113 torchvision==0.11.0+cu113 --extra-index-url https://download.pytorch.org/whl/cu113

# 验证安装
echo -e "\n验证安装..."
python3 -c "import numpy; print('NumPy:', numpy.__version__)"
python3 -c "import cma; print('CMA:', cma.__version__)"
python3 -c "import matplotlib; print('Matplotlib:', matplotlib.__version__)"
python3 -c "import SimpleITK; print('SimpleITK:', SimpleITK.Version_VersionString())"
python3 -c "import torch; print('PyTorch:', torch.__version__); print('CUDA可用:', torch.cuda.is_available())"

echo -e "\n安装完成！"
```

**运行**：
```bash
chmod +x install_python_deps.sh
./install_python_deps.sh
```

---

## ✅ 验证安装

### 完整验证脚本

创建 `test_python_env.py`：

```python
#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
Registration2D3D Python环境验证脚本
"""

import sys

def check_package(name, import_name=None):
    """检查包是否安装"""
    import_name = import_name or name
    try:
        module = __import__(import_name)
        version = getattr(module, '__version__', 'unknown')
        print(f"✅ {name:15s} {version}")
        return True
    except ImportError:
        print(f"❌ {name:15s} 未安装")
        return False

def check_torch_gpu():
    """检查PyTorch GPU支持"""
    try:
        import torch
        print(f"\nPyTorch信息:")
        print(f"  版本: {torch.__version__}")
        print(f"  CUDA可用: {torch.cuda.is_available()}")
        if torch.cuda.is_available():
            print(f"  CUDA版本: {torch.version.cuda}")
            print(f"  GPU数量: {torch.cuda.device_count()}")
            for i in range(torch.cuda.device_count()):
                print(f"  GPU {i}: {torch.cuda.get_device_name(i)}")
        else:
            print("  ⚠️  警告: GPU不可用，配准速度会很慢！")
        return torch.cuda.is_available()
    except ImportError:
        print("❌ PyTorch未安装")
        return False

def main():
    print("=" * 60)
    print("Registration2D3D Python环境检查")
    print("=" * 60)
    
    print(f"\nPython版本: {sys.version}")
    
    print("\n必需包:")
    all_ok = True
    all_ok &= check_package("numpy")
    all_ok &= check_package("cma")
    all_ok &= check_package("matplotlib")
    all_ok &= check_package("SimpleITK")
    all_ok &= check_package("torch")
    
    print("\n可选包:")
    check_package("scipy")
    check_package("Pillow", "PIL")
    
    # 检查GPU
    gpu_ok = check_torch_gpu()
    
    print("\n" + "=" * 60)
    if all_ok and gpu_ok:
        print("✅ 环境检查通过！可以使用Registration2D3D插件。")
    elif all_ok and not gpu_ok:
        print("⚠️  所有包已安装，但GPU不可用。配准速度会很慢。")
    else:
        print("❌ 环境检查失败！请安装缺失的包。")
    print("=" * 60)

if __name__ == '__main__':
    main()
```

**运行验证**：
```bash
python test_python_env.py
```

**期望输出**：
```
============================================================
Registration2D3D Python环境检查
============================================================

Python版本: 3.9.x ...

必需包:
✅ numpy           1.21.0
✅ cma             3.2.0
✅ matplotlib      3.5.0
✅ SimpleITK       2.1.0
✅ torch           1.10.0+cu113

可选包:
✅ scipy           1.7.0
✅ Pillow          9.0.0

PyTorch信息:
  版本: 1.10.0+cu113
  CUDA可用: True
  CUDA版本: 11.3
  GPU数量: 1
  GPU 0: NVIDIA GeForce RTX 3080

============================================================
✅ 环境检查通过！可以使用Registration2D3D插件。
============================================================
```

---

## 🐛 常见问题

### 问题1：pip安装速度慢

**解决方案：使用国内镜像源**

临时使用：
```bash
pip install numpy -i https://pypi.tuna.tsinghua.edu.cn/simple
```

永久配置：
```bash
# Windows
pip config set global.index-url https://pypi.tuna.tsinghua.edu.cn/simple

# Linux/macOS
pip3 config set global.index-url https://pypi.tuna.tsinghua.edu.cn/simple
```

国内镜像源推荐：
- 清华：`https://pypi.tuna.tsinghua.edu.cn/simple`
- 阿里：`https://mirrors.aliyun.com/pypi/simple/`
- 中科大：`https://pypi.mirrors.ustc.edu.cn/simple/`

### 问题2：torch.cuda.is_available() 返回 False

**可能原因和解决方案**：

1. **未安装GPU版本的PyTorch**
   ```bash
   # 卸载CPU版本
   pip uninstall torch torchvision
   
   # 安装GPU版本
   pip install torch==1.10.0+cu113 torchvision==0.11.0+cu113 --extra-index-url https://download.pytorch.org/whl/cu113
   ```

2. **CUDA未安装或版本不匹配**
   ```bash
   # 检查CUDA
   nvidia-smi
   
   # 下载并安装对应版本的CUDA Toolkit
   # https://developer.nvidia.com/cuda-downloads
   ```

3. **NVIDIA驱动过旧**
   ```bash
   # 更新NVIDIA驱动到最新版本
   # https://www.nvidia.com/drivers
   ```

### 问题3：ImportError: DLL load failed

**Windows上常见问题**

解决方案：
```bash
# 1. 安装 Visual C++ Redistributable
# 下载：https://aka.ms/vs/16/release/vc_redist.x64.exe

# 2. 确保PyTorch版本与CUDA匹配
pip uninstall torch torchvision
pip install torch==1.10.0+cu113 torchvision==0.11.0+cu113 --extra-index-url https://download.pytorch.org/whl/cu113
```

### 问题4：SimpleITK安装失败

**解决方案**：
```bash
# 尝试使用conda安装
conda install -c simpleitk simpleitk

# 或者安装预编译的wheel
pip install --upgrade pip
pip install SimpleITK
```

### 问题5：内存不足

**大型CT图像处理时可能遇到**

解决方案：
```python
# 在Python脚本中设置
import os
os.environ['PYTORCH_CUDA_ALLOC_CONF'] = 'max_split_size_mb:512'
```

---

## 📝 部署checklist

安装完成后，检查以下项目：

- [ ] Python 3.7+ 已安装
- [ ] numpy ≥ 1.19.0
- [ ] cma ≥ 3.0.0
- [ ] matplotlib ≥ 3.3.0
- [ ] SimpleITK ≥ 2.0.0
- [ ] torch ≥ 1.7.0（GPU版本）
- [ ] torch.cuda.is_available() 返回 True
- [ ] CUDA Toolkit 已安装
- [ ] NVIDIA 驱动已更新
- [ ] test_python_env.py 验证通过

---

## 🔗 相关资源

- **PyTorch官网**: https://pytorch.org/
- **CUDA下载**: https://developer.nvidia.com/cuda-downloads
- **NVIDIA驱动**: https://www.nvidia.com/drivers
- **SimpleITK**: https://simpleitk.org/
- **CMA-ES**: https://github.com/CMA-ES/pycma

---

## 💡 最佳实践

### 使用虚拟环境（推荐）

```bash
# 创建虚拟环境
python -m venv registration_env

# 激活虚拟环境
# Windows:
registration_env\Scripts\activate
# Linux/macOS:
source registration_env/bin/activate

# 安装依赖
pip install -r requirements.txt

# 验证
python test_python_env.py
```

### 使用Conda（可选）

```bash
# 创建conda环境
conda create -n registration python=3.9

# 激活环境
conda activate registration

# 安装依赖
conda install numpy cma matplotlib scipy pillow
conda install -c simpleitk simpleitk
conda install pytorch torchvision cudatoolkit=11.3 -c pytorch

# 验证
python test_python_env.py
```

---

**安装完成后，返回 [快速开始.md](快速开始.md) 继续配置插件！**

