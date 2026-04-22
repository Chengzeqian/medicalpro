# Platform Plugin Governance Matrix

Updated: 2026-04-22

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
- Product startup truth is explicitly `platform_runtime.json + plugins/descriptors/*.json + PlatformDescriptorLoader`.
- `main.cpp` performs explicit descriptor policy handoff through `ctkManager->setDescriptorPolicyContext(runtimeConfig, descriptors)` before plugin install/start orchestration.
- `CTKManager::setDescriptorPolicyContext()` is the governed descriptor policy handoff boundary between startup assembly and CTK runtime classification.
- `CTKManager` runtime bucket classification now resolves through `PlatformCtkPolicyBridge` instead of legacy load-policy lookup.
- Safe mode criticality now follows `platform_runtime.json.core_plugin_ids`.
- `plugin_load_policy.json` and `PluginLoadPolicy` remain compatibility-only metadata for `CTKManager::loadPluginPolicy()` and compatibility runtime surfaces.
- `runtime_artifact_layout_test` covers product-mainline runtime artifacts only.
- `plugin_legacy_compatibility_runtime_contract_test` owns `plugin_load_policy.json` and `plugin_load_policy_compatibility.md` shipping verification.
- The authoritative human-readable inventory for remaining legacy consumers is `docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md`.
- `CriticalPluginStart` starts only the core startup set declared in runtime config.
- `DeferredPluginStart` goes through `CTKManager::startDeferredPlugins(false)` instead of a hard-coded plugin list in `main.cpp`.
- Descriptor governance includes a dedicated `diagnostics` block with `required_services`, `service_ready_timeout_ms`, `warmup_tasks`, `warmup_timeout_ms`, `warmup_impacts_ready`, and `degrade_on`.
- Lifecycle diagnostics are modeled through `PlatformLifecycleEvent` rather than ad-hoc logs, so `install`, `start`, `service_ready`, `warmup`, `failed`, `degraded`, and `skipped_by_mode` are first-class governed facts.
- Runtime mode behavior is explicit in diagnostics output:
  - `observe_only` emits `skipped_by_mode` for governed stages.
  - `facade_mode` times framework/core startup while keeping deferred start and warmup outside the governed ready-path.
  - `orchestrate_core` records the full ready-path and warmup-tail contract.
- Recovery-hint governance is standardized for `descriptor_missing`, `plugin_install_failed`, `plugin_start_failed`, `service_missing`, `service_ready_timeout`, `warmup_failed`, `skipped_by_mode`, and `ctk_platform_state_mismatch`.
