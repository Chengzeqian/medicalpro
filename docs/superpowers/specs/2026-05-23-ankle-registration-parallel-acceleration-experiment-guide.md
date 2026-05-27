# Ankle Registration Parallel Acceleration Experiment Guide

## Scope

- This guide is for the current `PointRegistration -> RegistrationCore -> MeshGPU` route.
- The recommended research method is `ankle_two_stage_constrained`.
- The goal is to compare robustness, runtime, and target-region accuracy before and after parallel search is enabled.

## Default Profile

- `candidate_count = 64`
- `top_k_count = 4`
- `multi_resolution_profile = ankle_roi_three_level`
- `target_region_radius_mm = 18.0`
- `parallel_search_enabled = true`

## Recommended Input Conditions

- Use the same case set for all comparison groups.
- Keep landmark source points, target mesh, target region, and constraint regions identical across groups.
- Run every group with the same planning context so that `target_tre_mm` remains comparable.
- If candidate search is disabled for a baseline method, keep `candidate_count = 0` and `top_k_count = 0` in exported summaries.

## Comparison Groups

1. `single_stage_landmark`
2. `landmark_plus_global_icp`
3. `landmark_plus_global_gicp`
4. `ankle_two_stage_constrained`

## Mandatory Metrics

- `fre_mm`
- `overall_tre_mm`
- `target_tre_mm`
- `runtime_ms`
- `candidate_count`
- `top_k_count`
- `coarse_search_ms`
- `best_candidate_rank`
- `parallel_search_enabled`
- `multi_resolution_profile`

## Optional Support Metrics

- `coarse_score`
- `coverage_score`
- `constraint_region_count`
- `refine_ms`
- `roi_filter_ms`

## Interpretation Rules

- `target_tre_mm` is the primary accuracy metric for the thesis chapter because it reflects target-region navigation value better than global error alone.
- `runtime_ms` is used to measure total method cost, while `coarse_search_ms` isolates the cost of the parallel initial search stage.
- `candidate_count` and `top_k_count` describe search scale and refinement budget.
- `best_candidate_rank` reflects whether the best final solution comes from a stable coarse-search ordering.
- `parallel_search_enabled` and `multi_resolution_profile` must be exported with every record so later analysis can separate algorithm differences from configuration drift.

## Recommended Experiment Matrix

- Baseline comparison: compare all four methods on the same case set.
- Robustness comparison: add pose perturbation to the coarse initial transform and compare success rate.
- Acceleration comparison: test `candidate_count = 32`, `64`, and `128` while keeping `top_k_count = 4`.
- Ablation comparison: disable `parallel_search_enabled` or switch off constraint-side filtering to isolate each component's contribution.

## Suggested Tables And Figures

- Table 1: mean and standard deviation of `fre_mm`, `overall_tre_mm`, and `target_tre_mm`.
- Table 2: mean `runtime_ms`, `coarse_search_ms`, `candidate_count`, and `top_k_count`.
- Table 3: success rate under initial pose perturbation.
- Figure 1: bar chart of `target_tre_mm` across the four comparison groups.
- Figure 2: line chart of runtime versus `candidate_count`.
- Figure 3: scatter plot of `best_candidate_rank` versus `target_tre_mm`.

## Reporting Guidance

- Use `ankle_two_stage_constrained` as the final method name in paper text, figures, and exported summaries.
- Describe the method as a target-region-constrained two-stage registration method with parallel initial search and local constrained GICP refine.
- When discussing acceleration, separate "parallel search stage cost" from "total registration cost" to avoid overstating GPU gains.
- When discussing innovation, emphasize convergence-domain expansion, success-rate improvement, and better target-region accuracy instead of only reporting GPU runtime reduction.

## Data Export Check

- Confirm summary exports include `candidate_count`, `top_k_count`, `coarse_search_ms`, `best_candidate_rank`, `parallel_search_enabled`, and `multi_resolution_profile`.
- Confirm navigation evaluation snapshots preserve the same metric keys for downstream digital twin use.
- Confirm all records for `ankle_two_stage_constrained` carry non-empty `multi_resolution_profile`.
