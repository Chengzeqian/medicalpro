# Probe-Guided Robust Initializer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a probe-guided robust initial transform module so surgical-site point collection produces fast, stable, high-confidence registration seeds.

**Architecture:** Keep the first delivery hardware-independent inside `RegistrationCore`: stable probe sampling, point-set quality evaluation, and robust rigid initial transform estimation. Later stages can feed this confidence into GPU candidate scoring and top-k refine without forcing every step onto GPU.

**Tech Stack:** C++17, Qt `QVector3D` / `QMatrix4x4`, existing `AnkleRegistrationUtils::solveWeightedRigid`, QtTest, CMake.

---

### Task 1: Add Robust Initializer Contract Tests

**Files:**
- Create: `tests/unit/RobustInitialTransformTest.cpp`
- Modify: `tests/unit/CMakeLists.txt`

- [ ] **Step 1: Write failing tests**

```cpp
#include <QtTest/QtTest>

#include "Plugins/RegistrationCore/robust_initial_transform.h"

class RobustInitialTransformTest : public QObject
{
    Q_OBJECT

private slots:
    void stable_probe_point_accepts_low_jitter_samples();
    void stable_probe_point_rejects_high_tracking_error_and_jitter();
    void initial_point_set_quality_rejects_nearly_collinear_points();
    void robust_initial_transform_rejects_one_outlier_correspondence();
};

QTEST_APPLESS_MAIN(RobustInitialTransformTest)
#include "RobustInitialTransformTest.moc"
```

- [ ] **Step 2: Run RED**

Run: `cmake --build build_x64 --config Release --target robust_initial_transform_test`

Expected: FAIL because `robust_initial_transform.h` does not exist.

### Task 2: Implement Stable Probe Point Collection

**Files:**
- Create: `Plugins/RegistrationCore/robust_initial_transform.h`
- Create: `Plugins/RegistrationCore/robust_initial_transform.cpp`
- Modify: `Plugins/RegistrationCore/CMakeLists.txt`

- [ ] **Step 1: Define public data structures**

```cpp
struct ProbeTipFrameSample
{
    QVector3D tipPositionMm;
    double trackingErrorMm = 0.0;
    double timestampMs = 0.0;
    bool valid = false;
};
```

- [ ] **Step 2: Implement collector**

Use median center plus RMS jitter. Reject invalid samples, tracking error above threshold, too few accepted samples, and RMS jitter above threshold.

- [ ] **Step 3: Run GREEN**

Run: `cmake --build build_x64 --config Release --target robust_initial_transform_test`

Expected: stable sampling tests pass.

### Task 3: Implement Point Geometry Quality Gate

**Files:**
- Modify: `Plugins/RegistrationCore/robust_initial_transform.h`
- Modify: `Plugins/RegistrationCore/robust_initial_transform.cpp`
- Modify: `tests/unit/RobustInitialTransformTest.cpp`

- [ ] **Step 1: Write test for nearly collinear points**

Reject point sets that do not span enough volume for a stable rigid transform.

- [ ] **Step 2: Implement quality metrics**

Calculate bounding diagonal, smallest triangle area, and a normalized non-collinearity score.

- [ ] **Step 3: Run GREEN**

Run: `build_x64/tests/unit/Release/robust_initial_transform_test.exe -txt -o robust_initial_transform_green.txt`

Expected: quality gate test passes.

### Task 4: Implement RANSAC + Weighted SVD Initial Transform

**Files:**
- Modify: `Plugins/RegistrationCore/robust_initial_transform.h`
- Modify: `Plugins/RegistrationCore/robust_initial_transform.cpp`
- Modify: `tests/unit/RobustInitialTransformTest.cpp`

- [ ] **Step 1: Write test with one bad correspondence**

Use four source/target correspondences, corrupt one target point, and require the estimator to keep the three correct inliers.

- [ ] **Step 2: Implement deterministic triplet RANSAC**

Enumerate 3-point combinations, solve each using `AnkleRegistrationUtils::solveWeightedRigid`, count inliers by residual threshold, then refit with inlier weights.

- [ ] **Step 3: Run GREEN**

Run: `build_x64/tests/unit/Release/robust_initial_transform_test.exe -txt -o robust_initial_transform_green.txt`

Expected: robust initial transform test passes with low inlier RMS and one rejected outlier.

### Task 5: Verify And Prepare Runtime Integration

**Files:**
- Read: `Plugins/PointRegistration/PointRegistrationServiceImpl.cpp`
- Read: `Plugins/RegistrationCore/RegistrationServiceImpl.cpp`

- [ ] **Step 1: Build target**

Run: `cmake --build build_x64 --config Release --target robust_initial_transform_test`

- [ ] **Step 2: Run focused test**

Run: `build_x64/tests/unit/Release/robust_initial_transform_test.exe -txt -o robust_initial_transform_green.txt`

- [ ] **Step 3: Record integration notes**

Next integration point is `PointRegistrationServiceImpl::captureProbePoint()`: replace single-frame capture with stable sample collection, then attach quality/confidence metrics to registration parameters.
