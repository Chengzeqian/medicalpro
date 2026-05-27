# Ankle Navigation Error-Aware Digital Twin Experiment Guide

## Core Outputs

- `twin_confidence_score`
- `local_risk_score`
- `target_region_distance_mm`
- `target_region_angle_error_deg`
- `dominant_risk_source`
- `re_register_recommended`
- `tracking_degradation_detected`

## Comparison Groups

1. Display-only digital twin
2. Evidence-aware digital twin
3. Error-aware decision digital twin

## Recommended Thresholds

- `target_tre_mm > 2.0` => registration risk high
- `tracking_jitter_mm > 0.8` => tracking degradation
- `visible_frame_ratio < 0.85` => tracking degradation
- `twin_confidence_score < 0.45` => recommend re-register

## Suggested Evidence Chain

- Registration evidence:
  - `target_tre_mm`
  - `coverage_score`
  - `best_candidate_rank`
  - `parallel_search_enabled`
- Tracking evidence:
  - `tracking_jitter_mm`
  - `visible_frame_ratio`
  - `tracking_latency_ms`
- Target-region evidence:
  - `target_region_distance_mm`
  - `target_region_angle_error_deg`
- Decision outputs:
  - `dominant_risk_source`
  - `re_register_recommended`
  - `tracking_degradation_detected`

## Recommended Chapter Framing

1. Display-only digital twin:
   only render anatomy and tool pose, without risk aggregation.
2. Evidence-aware digital twin:
   aggregate registration, tracking, and target-region evidence into twin metrics.
3. Error-aware decision digital twin:
   generate local risk, dominant risk source, and re-register recommendation.

## Suggested Report Alignment

- Workspace summary should expose:
  - `twin_confidence_score`
  - `local_risk_score`
  - `target_region_distance_mm`
  - `dominant_risk_source`
  - `re_register_recommended`
- Case summary / batch csv / innovation summary should use the same metric names.
- Thesis charts should keep the same naming to avoid export-to-paper drift.
