# Ankle Navigation Algorithm Subsystems Reintegration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `MeshGPU` 和 `ProbeCalibration` 从 `E:\ICPtry\...` 迁入 `medicalpro` 仓库内，统一为可构建、可部署、可验证的算法子系统，并消除宿主层的私有路径依赖。

**Architecture:** 保留 `medicalpro -> Plugins -> algorithms` 的三层边界，不把算法源码揉入插件内部。顶层 CMake 负责引入 `algorithms/meshgpu` 与 `algorithms/probe_calibration`，并统一复制 `MeshGPULib.dll` 与 `ProbeCalibration.dll` 到主程序输出目录；`RegistrationCore` 与 `OpticalTracking` 只保留动态加载和业务编排职责。

**Tech Stack:** CMake, Qt 5/6, Qt Test, PowerShell `robocopy`, CUDA (MeshGPU), Atracsys fusionTrack SDK, QLibrary runtime loading

---

## File Structure

### New Files

- `algorithms/meshgpu/CMakeLists.txt`
  - `MeshGPU` 迁入后的子项目构建入口，负责 `MeshGPULib` 与可选工具目标
- `algorithms/meshgpu/README.md`
  - 说明 `MeshGPU` 在 `medicalpro` 内的职责、依赖和构建方式
- `algorithms/meshgpu/include/*`
- `algorithms/meshgpu/src/*`
  - 从 `E:\ICPtry\MeshGPU` 迁入的源码资产
- `algorithms/probe_calibration/CMakeLists.txt`
  - `ProbeCalibration` 迁入后的子项目构建入口，负责 `ProbeCalibrationDLL`
- `algorithms/probe_calibration/README.md`
  - 说明 `ProbeCalibration` 在 `medicalpro` 内的职责、依赖和构建方式
- `algorithms/probe_calibration/include/*`
- `algorithms/probe_calibration/src/*`
  - 从 `E:\ICPtry\ProbeCalibration` 迁入的源码资产
- `tests/unit/AlgorithmSubsystemsRepositoryContractTest.cpp`
  - 校验仓库内 `algorithms/*`、`ThirdParty/eigen` 和顶层依赖变量约定
- `tests/unit/MeshGpuSubprojectContractTest.cpp`
  - 校验 `algorithms/meshgpu/CMakeLists.txt` 已脱离外部路径假设
- `tests/unit/ProbeCalibrationSubprojectContractTest.cpp`
  - 校验 `algorithms/probe_calibration/CMakeLists.txt` 已脱离外部路径假设
- `tests/unit/AlgorithmBuildIntegrationContractTest.cpp`
  - 校验顶层 CMake、运行时工件验证脚本和测试入口已接入两个算法子系统
- `tests/unit/AlgorithmRuntimeLoadPathContractTest.cpp`
  - 校验宿主层只从应用输出目录加载 DLL，且不再存在私有绝对路径回退

### Modified Files

- `CMakeLists.txt`
  - 增加 `MEDICALPRO_EIGEN_ROOT`、`MEDICALPRO_ATRACSYS_SDK_DIR`，引入 `algorithms/*` 子目录，统一复制算法 DLL 到主程序输出目录
- `CMakePresets.json`
  - 如需，为计划中的构建说明补充对新 cache 变量的可见说明，不写入私有机器路径
- `Plugins/RegistrationCore/CMakeLists.txt`
  - 改用仓库内 `algorithms/meshgpu/include`，删除旧 `ICPtry/MeshGPU` 源路径假设
- `Plugins/OpticalTracking/CMakeLists.txt`
  - 改用仓库内 `algorithms/probe_calibration/include`，删除旧 `ICPtry/ProbeCalibration` 源路径假设
- `Plugins/RegistrationCore/RegistrationServiceImpl.cpp`
  - 删除 `D:/Qtproject/medicalpro/ICPtry/MeshGPU/...` 回退，只从输出目录或显式传入路径加载 `MeshGPULib.dll`
- `Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp`
  - 删除 `D:/Qtproject/medicalpro/ICPtry/ProbeCalibration/...` 回退，只从输出目录或显式传入路径加载 `ProbeCalibration.dll`
- `tests/CMakeLists.txt`
  - 让 `runtime_artifact_layout_test` 同时校验 `MeshGPULib.dll` 和 `ProbeCalibration.dll`
- `tests/runtime/verify_runtime_artifacts.cmake`
  - 增加 `probe_calibration_runtime_dll` 校验
- `tests/unit/CMakeLists.txt`
  - 注册 5 个新的 contract test 目标

## Task 1: Establish Repository Dependency Roots And Layout Contracts

**Files:**
- Create: `tests/unit/AlgorithmSubsystemsRepositoryContractTest.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/unit/CMakeLists.txt`
- Create: `ThirdParty/eigen/*`

- [ ] **Step 1: Write the failing repository contract test**

```cpp
#include <QtTest/QtTest>

#include <QFile>
#include <QFileInfo>

class AlgorithmSubsystemsRepositoryContractTest : public QObject
{
    Q_OBJECT

private slots:
    void repository_exposes_in_tree_dependency_roots_for_algorithm_subsystems();

private:
    QString readSource(const QString& relativePath) const
    {
        QFile file(relativePath);
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(relativePath));
        return QString::fromUtf8(file.readAll());
    }
};

void AlgorithmSubsystemsRepositoryContractTest::repository_exposes_in_tree_dependency_roots_for_algorithm_subsystems()
{
    QVERIFY2(QFileInfo::exists(QStringLiteral("ThirdParty/eigen/Eigen/Core")),
        "ThirdParty/eigen must exist before algorithm projects are migrated in-tree");

    const QString rootCMake = readSource(QStringLiteral("CMakeLists.txt"));
    QVERIFY(rootCMake.contains(QStringLiteral("MEDICALPRO_EIGEN_ROOT")));
    QVERIFY(rootCMake.contains(QStringLiteral("MEDICALPRO_ATRACSYS_SDK_DIR")));
    QVERIFY(rootCMake.contains(QStringLiteral("ThirdParty/eigen")));
}

QTEST_APPLESS_MAIN(AlgorithmSubsystemsRepositoryContractTest)
#include "AlgorithmSubsystemsRepositoryContractTest.moc"
```

- [ ] **Step 2: Run the contract test to verify it fails**

Run: `cmake --build build_x64 --config Release --target algorithm_subsystems_repository_contract_test`

Expected: build fails because `algorithm_subsystems_repository_contract_test` is not registered yet, or test fails because `ThirdParty/eigen` and the new cache variables do not exist.

- [ ] **Step 3: Copy Eigen into the repository and declare shared dependency roots**

```powershell
New-Item -ItemType Directory -Force -Path 'ThirdParty\eigen' | Out-Null
robocopy 'E:\ICPtry\eigen' 'ThirdParty\eigen' /E /XD '.git' 'build'
if ($LASTEXITCODE -gt 7) { exit $LASTEXITCODE }
```

```cmake
# CMakeLists.txt
set(MEDICALPRO_EIGEN_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/eigen" CACHE PATH
    "Path to the Eigen source tree used by in-repo algorithm subsystems")
set(MEDICALPRO_ATRACSYS_SDK_DIR "" CACHE PATH
    "Path to the Atracsys fusionTrack SDK x64 directory")

if(NOT EXISTS "${MEDICALPRO_EIGEN_ROOT}/Eigen/Core")
    message(FATAL_ERROR "Eigen not found at ${MEDICALPRO_EIGEN_ROOT}")
endif()
```

```cmake
# tests/unit/CMakeLists.txt
add_executable(algorithm_subsystems_repository_contract_test
    AlgorithmSubsystemsRepositoryContractTest.cpp
)

target_include_directories(algorithm_subsystems_repository_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(algorithm_subsystems_repository_contract_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME algorithm_subsystems_repository_contract_test
    COMMAND algorithm_subsystems_repository_contract_test
)
```

- [ ] **Step 4: Run the repository contract test**

Run: `cmake --build build_x64 --config Release --target algorithm_subsystems_repository_contract_test && ctest --test-dir build_x64 -C Release -R "^algorithm_subsystems_repository_contract_test$" --output-on-failure`

Expected: test passes and confirms `ThirdParty/eigen` plus the two root cache variables exist.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt tests/unit/CMakeLists.txt tests/unit/AlgorithmSubsystemsRepositoryContractTest.cpp ThirdParty/eigen
git commit -m "build: add repository dependency roots for algorithm subsystems"
```

## Task 2: Migrate `MeshGPU` Into `algorithms/meshgpu` And Normalize Its CMake

**Files:**
- Create: `algorithms/meshgpu/CMakeLists.txt`
- Create: `algorithms/meshgpu/README.md`
- Create: `algorithms/meshgpu/include/*`
- Create: `algorithms/meshgpu/src/*`
- Create: `tests/unit/MeshGpuSubprojectContractTest.cpp`
- Modify: `tests/unit/CMakeLists.txt`

- [ ] **Step 1: Write the failing MeshGPU subproject contract test**

```cpp
#include <QtTest/QtTest>

#include <QFile>

class MeshGpuSubprojectContractTest : public QObject
{
    Q_OBJECT

private slots:
    void meshgpu_subproject_uses_in_tree_paths_and_exports_runtime_library();

private:
    QString readSource(const QString& relativePath) const
    {
        QFile file(relativePath);
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(relativePath));
        return QString::fromUtf8(file.readAll());
    }
};

void MeshGpuSubprojectContractTest::meshgpu_subproject_uses_in_tree_paths_and_exports_runtime_library()
{
    const QString cmakeText = readSource(QStringLiteral("algorithms/meshgpu/CMakeLists.txt"));

    QVERIFY(cmakeText.contains(QStringLiteral("project(MeshGPU")));
    QVERIFY(cmakeText.contains(QStringLiteral("add_library(MeshGPULib SHARED")));
    QVERIFY(cmakeText.contains(QStringLiteral("MEDICALPRO_EIGEN_ROOT")));
    QVERIFY(!cmakeText.contains(QStringLiteral("../eigen")));
    QVERIFY(!cmakeText.contains(QStringLiteral("E:/ICPtry")));
}

QTEST_APPLESS_MAIN(MeshGpuSubprojectContractTest)
#include "MeshGpuSubprojectContractTest.moc"
```

- [ ] **Step 2: Run the subproject contract test to verify it fails**

Run: `cmake --build build_x64 --config Release --target meshgpu_subproject_contract_test`

Expected: build fails because the test target is not registered yet, or test fails because `algorithms/meshgpu/CMakeLists.txt` does not exist.

- [ ] **Step 3: Copy the MeshGPU source tree and rewrite its build entry for in-tree integration**

```powershell
New-Item -ItemType Directory -Force -Path 'algorithms\meshgpu' | Out-Null
robocopy 'E:\ICPtry\MeshGPU' 'algorithms\meshgpu' /E `
    /XD 'build' 'build_ascend_only' 'logs' 'benchmark_results' 'visualization_output' `
    /XF 'tmp_*' 'nul'
if ($LASTEXITCODE -gt 7) { exit $LASTEXITCODE }
```

```cmake
# algorithms/meshgpu/CMakeLists.txt
cmake_minimum_required(VERSION 3.18)

option(MESHGPU_ENABLE_CUDA "Build CUDA core targets and demos" ON)
option(MESHGPU_BUILD_TOOLS "Build MeshGPU CLI tools and demos" OFF)
option(BUILD_ASCEND_STUB_PLUGIN "Build sample Ascend backend plugin template" OFF)
option(BUILD_ASCEND_ALGO_SAMPLE "Build sample Ascend algorithm hook plugin" OFF)
option(BUILD_ASCEND_CANN_KERNEL_SAMPLE "Build sample external CANN kernel hook plugin" OFF)

if(MESHGPU_ENABLE_CUDA)
    project(MeshGPU LANGUAGES CXX CUDA)
else()
    project(MeshGPU LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/utf-8>)
endif()

set(MESHGPU_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(MESHGPU_EIGEN_DIR "${MEDICALPRO_EIGEN_ROOT}")

if(NOT EXISTS "${MESHGPU_EIGEN_DIR}/Eigen/Core")
    message(FATAL_ERROR "MeshGPU requires Eigen at ${MESHGPU_EIGEN_DIR}")
endif()

include_directories("${MESHGPU_INCLUDE_DIR}" "${MESHGPU_EIGEN_DIR}")
```

```markdown
# algorithms/meshgpu/README.md

## Responsibility

`MeshGPU` is the ankle-navigation registration algorithm subsystem that exports `MeshGPULib`.

## Build Inputs

- `MEDICALPRO_EIGEN_ROOT`
- CUDA toolchain when `MESHGPU_ENABLE_CUDA=ON`

## Runtime Output

- `MeshGPULib.dll`
```

- [ ] **Step 4: Register and run the MeshGPU contract test**

```cmake
# tests/unit/CMakeLists.txt
add_executable(meshgpu_subproject_contract_test
    MeshGpuSubprojectContractTest.cpp
)

target_include_directories(meshgpu_subproject_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(meshgpu_subproject_contract_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME meshgpu_subproject_contract_test
    COMMAND meshgpu_subproject_contract_test
)
```

Run: `cmake --build build_x64 --config Release --target meshgpu_subproject_contract_test && ctest --test-dir build_x64 -C Release -R "^meshgpu_subproject_contract_test$" --output-on-failure`

Expected: test passes and confirms the migrated subproject is now rooted in `algorithms/meshgpu`.

- [ ] **Step 5: Commit**

```bash
git add algorithms/meshgpu tests/unit/CMakeLists.txt tests/unit/MeshGpuSubprojectContractTest.cpp
git commit -m "build: migrate meshgpu into repository"
```

## Task 3: Migrate `ProbeCalibration` Into `algorithms/probe_calibration` And Normalize Its CMake

**Files:**
- Create: `algorithms/probe_calibration/CMakeLists.txt`
- Create: `algorithms/probe_calibration/README.md`
- Create: `algorithms/probe_calibration/include/*`
- Create: `algorithms/probe_calibration/src/*`
- Create: `tests/unit/ProbeCalibrationSubprojectContractTest.cpp`
- Modify: `tests/unit/CMakeLists.txt`

- [ ] **Step 1: Write the failing ProbeCalibration subproject contract test**

```cpp
#include <QtTest/QtTest>

#include <QFile>

class ProbeCalibrationSubprojectContractTest : public QObject
{
    Q_OBJECT

private slots:
    void probe_calibration_subproject_uses_repository_eigen_and_explicit_sdk_root();

private:
    QString readSource(const QString& relativePath) const
    {
        QFile file(relativePath);
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(relativePath));
        return QString::fromUtf8(file.readAll());
    }
};

void ProbeCalibrationSubprojectContractTest::probe_calibration_subproject_uses_repository_eigen_and_explicit_sdk_root()
{
    const QString cmakeText = readSource(QStringLiteral("algorithms/probe_calibration/CMakeLists.txt"));

    QVERIFY(cmakeText.contains(QStringLiteral("project(ProbeCalibration")));
    QVERIFY(cmakeText.contains(QStringLiteral("MEDICALPRO_ATRACSYS_SDK_DIR")));
    QVERIFY(cmakeText.contains(QStringLiteral("MEDICALPRO_EIGEN_ROOT")));
    QVERIFY(!cmakeText.contains(QStringLiteral("../eigen")));
    QVERIFY(!cmakeText.contains(QStringLiteral("../Atracsys")));
    QVERIFY(!cmakeText.contains(QStringLiteral("E:/ICPtry")));
}

QTEST_APPLESS_MAIN(ProbeCalibrationSubprojectContractTest)
#include "ProbeCalibrationSubprojectContractTest.moc"
```

- [ ] **Step 2: Run the subproject contract test to verify it fails**

Run: `cmake --build build_x64 --config Release --target probe_calibration_subproject_contract_test`

Expected: build fails because the test target is not registered yet, or test fails because `algorithms/probe_calibration/CMakeLists.txt` does not exist.

- [ ] **Step 3: Copy the ProbeCalibration source tree and rewrite its build entry**

```powershell
New-Item -ItemType Directory -Force -Path 'algorithms\probe_calibration' | Out-Null
robocopy 'E:\ICPtry\ProbeCalibration' 'algorithms\probe_calibration' /E `
    /XD 'build' `
    /XF 'run_calibration.bat'
if ($LASTEXITCODE -gt 7) { exit $LASTEXITCODE }
```

```cmake
# algorithms/probe_calibration/CMakeLists.txt
cmake_minimum_required(VERSION 3.18)
project(ProbeCalibration LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(PROBECALIB_BUILD_SHARED "Build ProbeCalibration DLL with C API" ON)
option(PROBECALIB_BUILD_TOOLS "Build ProbeCalibration standalone tools" OFF)

if(MSVC)
    add_compile_options(/utf-8)
endif()

set(PROBE_CALIBRATION_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(PROBE_CALIBRATION_EIGEN_DIR "${MEDICALPRO_EIGEN_ROOT}")
set(PROBE_CALIBRATION_SDK_DIR "${MEDICALPRO_ATRACSYS_SDK_DIR}")

if(NOT EXISTS "${PROBE_CALIBRATION_EIGEN_DIR}/Eigen/Core")
    message(FATAL_ERROR "ProbeCalibration requires Eigen at ${PROBE_CALIBRATION_EIGEN_DIR}")
endif()

if(NOT EXISTS "${PROBE_CALIBRATION_SDK_DIR}/include/ftkInterface.h")
    message(FATAL_ERROR "Atracsys SDK not found at ${PROBE_CALIBRATION_SDK_DIR}")
endif()

include_directories(
    "${PROBE_CALIBRATION_INCLUDE_DIR}"
    "${PROBE_CALIBRATION_EIGEN_DIR}"
    "${PROBE_CALIBRATION_SDK_DIR}/include"
)

link_directories("${PROBE_CALIBRATION_SDK_DIR}/lib")
```

```markdown
# algorithms/probe_calibration/README.md

## Responsibility

`ProbeCalibration` is the ankle-navigation probe-tip calibration subsystem that exports `ProbeCalibration.dll`.

## Build Inputs

- `MEDICALPRO_EIGEN_ROOT`
- `MEDICALPRO_ATRACSYS_SDK_DIR`

## Runtime Output

- `ProbeCalibration.dll`
```

- [ ] **Step 4: Register and run the ProbeCalibration contract test**

```cmake
# tests/unit/CMakeLists.txt
add_executable(probe_calibration_subproject_contract_test
    ProbeCalibrationSubprojectContractTest.cpp
)

target_include_directories(probe_calibration_subproject_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(probe_calibration_subproject_contract_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME probe_calibration_subproject_contract_test
    COMMAND probe_calibration_subproject_contract_test
)
```

Run: `cmake --build build_x64 --config Release --target probe_calibration_subproject_contract_test && ctest --test-dir build_x64 -C Release -R "^probe_calibration_subproject_contract_test$" --output-on-failure`

Expected: test passes and confirms the migrated subproject only depends on in-repo Eigen plus an explicit SDK path.

- [ ] **Step 5: Commit**

```bash
git add algorithms/probe_calibration tests/unit/CMakeLists.txt tests/unit/ProbeCalibrationSubprojectContractTest.cpp
git commit -m "build: migrate probe calibration into repository"
```

## Task 4: Integrate Algorithm Subprojects Into The Top-Level Build And Runtime Artifact Checks

**Files:**
- Create: `tests/unit/AlgorithmBuildIntegrationContractTest.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/runtime/verify_runtime_artifacts.cmake`
- Modify: `tests/unit/CMakeLists.txt`

- [ ] **Step 1: Write the failing build-integration contract test**

```cpp
#include <QtTest/QtTest>

#include <QFile>

class AlgorithmBuildIntegrationContractTest : public QObject
{
    Q_OBJECT

private slots:
    void top_level_build_wires_algorithm_subdirectories_and_runtime_artifact_checks();

private:
    QString readSource(const QString& relativePath) const
    {
        QFile file(relativePath);
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(relativePath));
        return QString::fromUtf8(file.readAll());
    }
};

void AlgorithmBuildIntegrationContractTest::top_level_build_wires_algorithm_subdirectories_and_runtime_artifact_checks()
{
    const QString rootCMake = readSource(QStringLiteral("CMakeLists.txt"));
    const QString testsCMake = readSource(QStringLiteral("tests/CMakeLists.txt"));
    const QString runtimeCheck = readSource(QStringLiteral("tests/runtime/verify_runtime_artifacts.cmake"));

    QVERIFY(rootCMake.contains(QStringLiteral("add_subdirectory(algorithms/meshgpu)")));
    QVERIFY(rootCMake.contains(QStringLiteral("add_subdirectory(algorithms/probe_calibration)")));
    QVERIFY(rootCMake.contains(QStringLiteral("$<TARGET_FILE:MeshGPULib>")));
    QVERIFY(rootCMake.contains(QStringLiteral("$<TARGET_FILE:ProbeCalibrationDLL>")));
    QVERIFY(testsCMake.contains(QStringLiteral("probe_calibration_runtime_dll")));
    QVERIFY(runtimeCheck.contains(QStringLiteral("probe_calibration_runtime_dll")));
}

QTEST_APPLESS_MAIN(AlgorithmBuildIntegrationContractTest)
#include "AlgorithmBuildIntegrationContractTest.moc"
```

- [ ] **Step 2: Run the build-integration contract test to verify it fails**

Run: `cmake --build build_x64 --config Release --target algorithm_build_integration_contract_test`

Expected: build fails because the test target is not registered yet, or test fails because the top-level build does not yet include the algorithm subdirectories and `ProbeCalibration.dll` artifact checks.

- [ ] **Step 3: Wire the two subprojects into the main build and runtime artifact verification**

```cmake
# CMakeLists.txt
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/algorithms/meshgpu/CMakeLists.txt")
    add_subdirectory(algorithms/meshgpu)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/algorithms/probe_calibration/CMakeLists.txt")
    add_subdirectory(algorithms/probe_calibration)
endif()
```

```cmake
# CMakeLists.txt
if(TARGET medicalpro AND TARGET MeshGPULib)
    add_dependencies(medicalpro MeshGPULib)
    add_custom_command(TARGET medicalpro POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:MeshGPULib>
            $<TARGET_FILE_DIR:medicalpro>/MeshGPULib.dll
        COMMENT "Copying MeshGPULib runtime DLL to application output directory"
    )
endif()

if(TARGET medicalpro AND TARGET ProbeCalibrationDLL)
    add_dependencies(medicalpro ProbeCalibrationDLL)
    add_custom_command(TARGET medicalpro POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:ProbeCalibrationDLL>
            $<TARGET_FILE_DIR:medicalpro>/ProbeCalibration.dll
        COMMENT "Copying ProbeCalibration runtime DLL to application output directory"
    )
endif()
```

```cmake
# tests/CMakeLists.txt
add_test(
    NAME runtime_artifact_layout_test
    COMMAND ${CMAKE_COMMAND}
        -Druntime_dir=$<TARGET_FILE_DIR:medicalpro>
        -Dmeshgpu_runtime_dll=$<TARGET_FILE_DIR:medicalpro>/MeshGPULib.dll
        -Dprobe_calibration_runtime_dll=$<TARGET_FILE_DIR:medicalpro>/ProbeCalibration.dll
        -Ddata_dir=$<TARGET_FILE_DIR:medicalpro>/data
        -Drequire_platform_descriptors=ON
        -P ${CMAKE_CURRENT_SOURCE_DIR}/runtime/verify_runtime_artifacts.cmake
)
```

```cmake
# tests/runtime/verify_runtime_artifacts.cmake
if(meshgpu_runtime_dll)
    list(APPEND required_files "${meshgpu_runtime_dll}")
endif()

if(probe_calibration_runtime_dll)
    list(APPEND required_files "${probe_calibration_runtime_dll}")
endif()
```

- [ ] **Step 4: Register and run the integration contract plus runtime artifact checks**

```cmake
# tests/unit/CMakeLists.txt
add_executable(algorithm_build_integration_contract_test
    AlgorithmBuildIntegrationContractTest.cpp
)

target_include_directories(algorithm_build_integration_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(algorithm_build_integration_contract_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME algorithm_build_integration_contract_test
    COMMAND algorithm_build_integration_contract_test
)
```

Run: `cmake --build build_x64 --config Release --target medicalpro algorithm_build_integration_contract_test && ctest --test-dir build_x64 -C Release -R "algorithm_build_integration_contract_test|runtime_artifact_layout_test" --output-on-failure`

Expected: test passes and the runtime artifact layout now requires both algorithm DLLs.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt tests/CMakeLists.txt tests/runtime/verify_runtime_artifacts.cmake tests/unit/CMakeLists.txt tests/unit/AlgorithmBuildIntegrationContractTest.cpp
git commit -m "build: integrate algorithm subprojects into medicalpro"
```

## Task 5: Remove Private Path Fallbacks From The Host Adapters And Verify Batch-A Regression

**Files:**
- Create: `tests/unit/AlgorithmRuntimeLoadPathContractTest.cpp`
- Modify: `Plugins/RegistrationCore/CMakeLists.txt`
- Modify: `Plugins/OpticalTracking/CMakeLists.txt`
- Modify: `Plugins/RegistrationCore/RegistrationServiceImpl.cpp`
- Modify: `Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp`
- Modify: `tests/unit/CMakeLists.txt`

- [ ] **Step 1: Write the failing runtime load-path contract test**

```cpp
#include <QtTest/QtTest>

#include <QFile>

class AlgorithmRuntimeLoadPathContractTest : public QObject
{
    Q_OBJECT

private slots:
    void host_adapters_load_algorithm_dlls_from_runtime_output_without_private_source_fallbacks();

private:
    QString readSource(const QString& relativePath) const
    {
        QFile file(relativePath);
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(relativePath));
        return QString::fromUtf8(file.readAll());
    }
};

void AlgorithmRuntimeLoadPathContractTest::host_adapters_load_algorithm_dlls_from_runtime_output_without_private_source_fallbacks()
{
    const QString registrationSource = readSource(QStringLiteral("Plugins/RegistrationCore/RegistrationServiceImpl.cpp"));
    const QString trackingSource = readSource(QStringLiteral("Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp"));

    QVERIFY(registrationSource.contains(QStringLiteral("QCoreApplication::applicationDirPath() + \"/MeshGPULib.dll\"")));
    QVERIFY(trackingSource.contains(QStringLiteral("QCoreApplication::applicationDirPath() + \"/ProbeCalibration.dll\"")));

    QVERIFY(!registrationSource.contains(QStringLiteral("D:/Qtproject/medicalpro/ICPtry")));
    QVERIFY(!registrationSource.contains(QStringLiteral("E:/ICPtry")));
    QVERIFY(!trackingSource.contains(QStringLiteral("D:/Qtproject/medicalpro/ICPtry")));
    QVERIFY(!trackingSource.contains(QStringLiteral("E:/ICPtry")));
}

QTEST_APPLESS_MAIN(AlgorithmRuntimeLoadPathContractTest)
#include "AlgorithmRuntimeLoadPathContractTest.moc"
```

- [ ] **Step 2: Run the load-path contract test to verify it fails**

Run: `cmake --build build_x64 --config Release --target algorithm_runtime_load_path_contract_test`

Expected: build fails because the test target is not registered yet, or test fails because the host adapters still contain hardcoded source-tree fallback paths.

- [ ] **Step 3: Switch plugin include paths and host loading logic to the in-repo algorithm outputs**

```cmake
# Plugins/RegistrationCore/CMakeLists.txt
set(MESHGPU_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/algorithms/meshgpu/include")
if(EXISTS "${MESHGPU_INCLUDE_DIR}")
    target_include_directories(RegistrationCorePlatformModuleLib PUBLIC
        ${MESHGPU_INCLUDE_DIR}
    )
endif()
```

```cmake
# Plugins/OpticalTracking/CMakeLists.txt
set(PROBE_CALIBRATION_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/algorithms/probe_calibration/include")
if(EXISTS "${PROBE_CALIBRATION_INCLUDE_DIR}")
    target_include_directories(OpticalTrackingPlatformModuleLib PRIVATE
        ${PROBE_CALIBRATION_INCLUDE_DIR}
    )
endif()
```

```cpp
// Plugins/RegistrationCore/RegistrationServiceImpl.cpp
QString path = dllPath;
if (path.isEmpty()) {
    path = QCoreApplication::applicationDirPath() + "/MeshGPULib.dll";
}
```

```cpp
// Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp
QString path = dllPath;
if (path.isEmpty()) {
    path = QCoreApplication::applicationDirPath() + "/ProbeCalibration.dll";
}
```

- [ ] **Step 4: Register the load-path contract test and run the batch-A regression**

```cmake
# tests/unit/CMakeLists.txt
add_executable(algorithm_runtime_load_path_contract_test
    AlgorithmRuntimeLoadPathContractTest.cpp
)

target_include_directories(algorithm_runtime_load_path_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(algorithm_runtime_load_path_contract_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME algorithm_runtime_load_path_contract_test
    COMMAND algorithm_runtime_load_path_contract_test
)
```

Run: `cmake --build build_x64 --config Release --target medicalpro RegistrationCorePlatformModuleLib OpticalTrackingPlatformModuleLib algorithm_runtime_load_path_contract_test ankle_registration_baseline_test optical_tracking_quality_snapshot_test && ctest --test-dir build_x64 -C Release -R "algorithm_runtime_load_path_contract_test|runtime_artifact_layout_test|ankle_registration_baseline_test|optical_tracking_quality_snapshot_test|navigation_confidence_evaluator_test|innovation_2_registration_experiment_test" --output-on-failure`

Expected: all selected tests pass, `MeshGPULib.dll` and `ProbeCalibration.dll` are present in the runtime output, and neither host adapter depends on source-tree fallback paths.

- [ ] **Step 5: Commit**

```bash
git add Plugins/RegistrationCore/CMakeLists.txt Plugins/OpticalTracking/CMakeLists.txt Plugins/RegistrationCore/RegistrationServiceImpl.cpp Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp tests/unit/CMakeLists.txt tests/unit/AlgorithmRuntimeLoadPathContractTest.cpp
git commit -m "refactor: remove external algorithm path fallbacks"
```

## Self-Review

### Spec Coverage

- 统一源码边界由 Task 1、Task 2、Task 3 实现。
- `MeshGPU` 与 `ProbeCalibration` 的物理迁入由 Task 2、Task 3 实现。
- 顶层统一构建与运行时 DLL 部署由 Task 4 实现。
- 宿主适配层删除私有路径回退由 Task 5 实现。
- 批次 A 的最小回归与运行时验证由 Task 4、Task 5 实现。

### Placeholder Scan

- 本计划未使用 `TODO`、`TBD`、`稍后实现`、`类似 Task N` 这类占位写法。
- 每个任务都给出了明确文件路径、测试文件、命令和预期结果。

### Type Consistency

- 顶层依赖变量统一使用 `MEDICALPRO_EIGEN_ROOT` 与 `MEDICALPRO_ATRACSYS_SDK_DIR`。
- 运行时 DLL 名称统一使用 `MeshGPULib.dll` 与 `ProbeCalibration.dll`。
- 运行时验证变量统一使用 `meshgpu_runtime_dll` 与 `probe_calibration_runtime_dll`。
