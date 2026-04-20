# Platform Plugin Governance Matrix

Updated: 2026-04-17

## Core Plugin Governance Matrix

| Plugin | Descriptor Id | CTK Symbolic Name | Bootstrap | Startup Policy | Facade Owner | Legacy Adapter | UI Entry |
| --- | --- | --- | --- | --- | --- | --- | --- |
| UserManagement | `org.medicalpro.user_management` | `UserManagement` | core | eager | `IdentityAppService` | `LegacyUserManagementAdapter` | Welcome / Management |
| DicomViewer | `org.medicalpro.dicom_viewer` | `DicomViewer` | core | eager | `ImagingAppService` | `LegacyImagingAdapter` | Welcome / Dashboard |
| FourViewDisplay | `org.medicalpro.four_view_display` | `FourViewDisplay` | core | eager | `ImagingAppService` | `LegacyImagingAdapter` | Dashboard |
| RegistrationCore | `org.medicalpro.registration_core` | `RegistrationCore` | deferred | on_demand | `NavigationAppService` | `LegacyNavigationAdapter` | Navigation |
| OpticalTracking | `org.medicalpro.optical_tracking` | `OpticalTracking` | deferred | on_demand | `NavigationAppService` | `LegacyNavigationAdapter` | Navigation |

## Startup Diagnostics Governance Addendum

| Plugin | Required Service | Service Ready Timeout | Warmup Task | Warmup Failure Impact |
| --- | --- | --- | --- | --- |
| UserManagement | `UserManagementService` | 3000 ms | `session_cache` | warning |
| DicomViewer | `DicomViewerService` | 5000 ms | `data_path_precheck` | degraded |
| FourViewDisplay | `FourViewDisplayService` | 5000 ms | `render_backend_warmup` | degraded |
| RegistrationCore | `RegistrationService` | 5000 ms | `core_binary_probe` | degraded |
| OpticalTracking | `InstrumentManagementService` | 5000 ms | `adapter_probe` | degraded |

## Runtime Mode Constraints

| Runtime Mode | Framework Init | Plugin Install | Core Start | Deferred Start | Service Warmup |
| --- | --- | --- | --- | --- | --- |
| `observe_only` | no | no | no | no | no |
| `facade_mode` | yes | yes | yes | no | no |
| `orchestrate_core` | yes | yes | yes | yes | yes |

## Current Implementation Notes

- `config/platform_runtime.json` stores platform descriptor ids, not CTK symbolic names.
- The main startup chain in `main.cpp` reads runtime config first, then resolves CTK symbolic names through `PlatformRuntimeConfig::resolveCoreCtkPluginNames()`.
- `CriticalPluginStart` now starts only the core startup set declared in runtime config.
- `DeferredPluginStart` now goes through `CTKManager::startDeferredPlugins(false)` instead of maintaining a hard-coded plugin list in `main.cpp`.
