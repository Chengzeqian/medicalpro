# CTK Descriptor Policy Bridge Design

## Scope Note

- This document defines the next cleanup slice after `plugin legacy consumer governance`.
- This slice replaces the remaining runtime classification dependence on `PluginLoadPolicy` inside `CTKManager`.
- This slice does not reopen product startup mainline truth, which remains descriptor-driven already.
- This slice accepts descriptor and runtime-config facts as the final authority when they conflict with legacy policy metadata.
- This slice does not remove every compatibility artifact in one batch, but it does remove `PluginLoadPolicy` from product runtime classification semantics.

## Implementation Links

- Base governance design: `docs/superpowers/specs/2026-04-16-platform-kernel-governance-design.md`
- Plugin-chain remediation Phase 1 design: `docs/superpowers/specs/2026-04-20-plugin-chain-remediation-phase1-design.md`
- Plugin-chain remediation Phase 2 design: `docs/superpowers/specs/2026-04-20-plugin-chain-remediation-phase2-design.md`
- Plugin truth-source governance design: `docs/superpowers/specs/2026-04-21-plugin-truth-source-governance-design.md`
- Plugin legacy consumer governance design: `docs/superpowers/specs/2026-04-21-plugin-legacy-consumer-governance-design.md`
- Legacy consumer inventory: `docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md`
- Decision log: `docs/superpowers/tracking/platform-migration-decision-log.md`
- Current project overview: `docs/current_status_and_project_overview.md`

Date: 2026-04-22  
Scope: replace `CTKManager` internal `PluginLoadPolicy`-driven bucket classification with a descriptor/runtime-config bridge  
Goal: make `CTKManager` classify installed plugins from descriptor and runtime-config truth instead of `plugin_load_policy.json`, so product runtime semantics stop depending on legacy policy metadata even inside internal CTK compatibility paths.

## 1. Design Background

The current codebase has already completed two important governance steps:

- product startup truth is locked to `config/platform_runtime.json + plugins/descriptors/*.json + PlatformDescriptorLoader`
- remaining legacy policy consumers are inventoried and separated from default product runtime acceptance

That means the product mainline is already clean, but one important internal debt remains:

- `CTKManager::installPlugin()` still routes newly installed plugins through `applyPolicyForPlugin()`
- `applyPolicyForPlugin()` still calls `policyForPlugin()`
- `policyForPlugin()` still reads `PluginLoadPolicy`
- `PluginLoadPolicy` still sources its classification facts from `config/plugin_load_policy.json`

This leaves the runtime in an inconsistent state:

1. startup truth is descriptor-driven, but internal CTK bucket classification is still legacy-policy-driven
2. `plugin_load_policy.json` is documented as compatibility-only, but it still influences runtime behavior
3. descriptor/runtime facts and legacy policy facts can disagree, and the internal CTK path still follows the wrong authority

This slice removes that inconsistency by introducing a bridge that maps descriptor/runtime facts into the narrow classification model `CTKManager` still needs today.

## 2. Decision Summary

The following decisions are fixed for this slice:

- Descriptor and runtime-config facts become the final authority for `CTKManager` internal bucket classification.
- `PluginLoadPolicy` no longer drives `immediate / deferred / on_demand / critical` runtime semantics for product code paths.
- `CTKManager` continues to own CTK install/start execution details, but not product runtime semantic interpretation.
- When descriptor/runtime facts conflict with `plugin_load_policy.json`, descriptor/runtime wins.
- Missing descriptor facts must not revive legacy policy fallback; they must produce explicit bridge diagnostics and a conservative non-critical fallback.

## 3. Goals And Non-Goals

### 3.1 Goals

This slice must achieve the following goals:

1. Replace `PluginLoadPolicy` as the runtime classification source for `CTKManager`.
2. Introduce a thin bridge that translates descriptor/runtime facts into `CTKManager` bucket decisions.
3. Keep `CTKManager` external behavior surfaces stable while swapping the truth source underneath.
4. Make `safe mode` semantics align with `platform_runtime.json.core_plugin_ids`.
5. Lock the new bridge behavior with unit tests and source contracts.

### 3.2 Non-Goals

This slice does not do the following:

- It does not change `main.cpp` startup truth ownership or phase ordering.
- It does not redesign `PlatformStartupCoordinator` or on-demand activation orchestration.
- It does not remove `plugin_load_policy.json` from packaging yet.
- It does not rewrite all plugin lifecycle logic in `CTKManager`.
- It does not change UI routing or welcome-page behavior.

## 4. Replacement Target And Boundary

### 4.1 What Gets Replaced

The replacement target is not the startup mainline. It is the internal `CTKManager` bucket-classification path:

- `CTKManager::installPlugin()`
- `CTKManager::applyPolicyForPlugin()`
- `CTKManager::policyForPlugin()`
- the resulting writes into:
  - `m_deferredPlugins`
  - `m_onDemandPlugins`
  - safe-mode critical/non-critical branching

### 4.2 What Stays Unchanged

The following boundaries remain unchanged in this slice:

- `main.cpp` still owns startup assembly from runtime config and descriptors
- `main.cpp` must not reintroduce legacy policy truth; if `CTKManager` needs bridge context, it receives the already-loaded runtime config and descriptors through an explicit context handoff instead of loading its own truth source
- descriptor-driven startup planning remains owned by the platform governance layer
- `CTKManager` still performs install/start/stop operations against the CTK framework

### 4.3 Architecture Direction

After this slice, `CTKManager` should no longer act as both:

- CTK runtime executor
- product semantic interpreter

Instead:

- descriptor/runtime truth stays in the platform layer
- `CTKManager` consumes a thin, resolved bridge result
- `PluginLoadPolicy` remains only as compatibility residue until a later deletion slice

## 5. Bridge Model

### 5.1 New Bridge Component

Introduce a small bridge component:

- `Framework/Platform/Kernel/PlatformCtkPolicyBridge.h`
- `Framework/Platform/Kernel/PlatformCtkPolicyBridge.cpp`

This bridge has one responsibility:

- map `PlatformRuntimeConfig + descriptor catalog + CTK symbolic name` into the narrow classification result `CTKManager` still needs

### 5.2 Bridge Output Model

The bridge should produce an internal result shaped around the following facts:

- `resolved_plugin_id`
- `ctk_symbolic_name`
- `load_bucket`
  - `immediate`
  - `deferred`
  - `on_demand`
- `is_critical`
- `resolution_status`
- `diagnostic_code`

The bridge is not a new general-purpose startup planner. It is a thin compatibility adapter for the still-existing CTK bucket path.

### 5.3 Required Bridge Inputs

The bridge must resolve from:

- runtime config
  - especially `core_plugin_ids`
- descriptor catalog
  - especially `runtime.ctk_symbolic_name`
  - `runtime.startup_policy`
  - `runtime.bootstrap_level` for diagnostics only in this slice
- current CTK symbolic name being classified

In this slice, `runtime.bootstrap_level` does not override `startup_policy` when resolving the CTK load bucket.

## 6. Mapping Rules

### 6.1 Resolution Order

The bridge must resolve in this order:

1. normalize the incoming CTK symbolic name using:
   - exact match
   - case-insensitive match
   - leading `lib` prefix stripping before retry
2. find descriptor by `runtime.ctk_symbolic_name`
3. determine whether the descriptor id is part of `platform_runtime.json.core_plugin_ids`
4. classify `load_bucket`
5. classify `is_critical`
6. emit resolution status and diagnostic code

### 6.2 Criticality Rule

Recommended rule:

- if `descriptor.id` is in `core_plugin_ids`
  - `is_critical = true`
- otherwise
  - `is_critical = false`

This makes safe-mode behavior align with runtime-config truth instead of legacy policy.

### 6.3 Load Bucket Rule

Recommended rule:

- if `descriptor.runtime.startup_policy == on_demand`
  - `load_bucket = on_demand`
- else if `descriptor.id` is in `core_plugin_ids`
  - `load_bucket = immediate`
- else
  - `load_bucket = deferred`

This gives the following intended outcomes:

- `UserManagement`, `DicomViewer`, `FourViewDisplay`
  - `immediate + critical`
- `OpticalTracking`
  - `on_demand + non-critical`
- `RegistrationCore`
  - `on_demand + non-critical`

### 6.4 Conflict Rule

If descriptor/runtime facts disagree with legacy policy metadata:

- descriptor/runtime wins
- no runtime decision may be taken from `plugin_load_policy.json`

This is an explicit architecture convergence, not an accidental side effect.

### 6.5 Missing Descriptor Rule

If no descriptor can be resolved for a CTK symbolic name:

- do not fall back to `PluginLoadPolicy`
- emit `descriptor_missing_for_ctk_policy_bridge`
- use conservative fallback:
  - `load_bucket = on_demand`
  - `is_critical = false`

This preserves safety while preventing legacy policy from regaining authority.

## 7. Safe Mode Semantics

After this slice, safe mode must mean:

- current runtime core set must not be skipped
- non-core `deferred` and `on_demand` plugins may be skipped

This is a deliberate change in semantic ownership:

- old behavior depended on `PluginLoadPolicy.isCriticalPlugin()`
- new behavior depends on `platform_runtime.json.core_plugin_ids`

That means current runtime truth now decides what is essential, not legacy policy metadata.

## 8. Code Touchpoints

### 8.1 New Files

Create:

- `Framework/Platform/Kernel/PlatformCtkPolicyBridge.h`
- `Framework/Platform/Kernel/PlatformCtkPolicyBridge.cpp`
- `tests/unit/PlatformCtkPolicyBridgeTest.cpp`

### 8.2 CTKManager Changes

`CTKManager` should be updated so that:

- `policyForPlugin()` no longer reads `PluginLoadPolicy`
- `applyPolicyForPlugin()` no longer asks `PluginLoadPolicy` for criticality
- bucket classification comes from `PlatformCtkPolicyBridge`

`CTKManager` should continue to own:

- install/start execution
- maintaining `m_deferredPlugins` and `m_onDemandPlugins`
- applying safe-mode skipping

### 8.3 Platform Data Provisioning

The platform layer must provide `CTKManager` with enough descriptor/runtime context to use the bridge consistently.

This slice fixes the injection mechanism explicitly:

- add a narrow `CTKManager` context setter that accepts the already-loaded `PlatformRuntimeConfig` and descriptor catalog
- call that setter from the existing startup assembly after `platform_runtime.json` and descriptors are loaded, before managed plugin installation begins
- `CTKManager` and `PlatformCtkPolicyBridge` must consume only that handed-off context for bucket classification

This keeps startup truth ownership unchanged while avoiding hidden re-loading of runtime metadata inside `CTKManager`.

## 9. Testing Strategy And Acceptance

### 9.1 Bridge Unit Tests

Add direct bridge tests for at least these cases:

1. core plugin descriptor resolves to `immediate + critical`
2. non-core eager descriptor resolves to `deferred + non-critical`
3. `on_demand` descriptor resolves to `on_demand + non-critical`
4. missing descriptor resolves to `on_demand + non-critical + descriptor_missing_for_ctk_policy_bridge`

### 9.2 CTKManager Source Contracts

Add source-contract coverage that proves:

- `CTKManager.cpp` no longer uses `PluginLoadPolicy::getLoadPolicy()`
- `CTKManager.cpp` no longer uses `PluginLoadPolicy::isCriticalPlugin()`
- `PlatformCtkPolicyBridge` is the only runtime classification source for the remaining CTK bucket path

### 9.3 Runtime Governance Regression Protection

Retain the already-landed tests for:

- product runtime artifact acceptance
- compatibility runtime artifact acceptance
- truth-source governance contracts
- legacy consumer governance contracts

This slice must extend those protections, not replace them.

### 9.4 Acceptance Criteria

This slice is accepted only when all of the following are true:

- `CTKManager` internal runtime classification no longer depends on `PluginLoadPolicy`
- descriptor/runtime truth drives `deferred / on_demand / critical` bucket resolution
- safe mode uses runtime core-set truth instead of legacy critical flags
- existing product-runtime and compatibility-runtime contract tests remain green
- new bridge tests and source contracts are green

## 10. Implementation Batches

### Batch 1: bridge model landing

- add `PlatformCtkPolicyBridge`
- add pure unit tests for mapping rules
- add CTK symbolic-name normalization coverage
- do not wire it into `CTKManager` yet

### Batch 2: CTKManager truth-source switch

- add explicit runtime-context handoff into `CTKManager`
- replace `policyForPlugin()` / `applyPolicyForPlugin()` classification source
- remove direct `PluginLoadPolicy` dependence from runtime bucket decisions
- keep `CTKManager` public behavior surfaces stable

### Batch 3: governance write-back

- add source contracts that lock bridge ownership
- update current status, decision log, governance matrix, and inventory
- explicitly record that `PluginLoadPolicy` no longer drives runtime classification semantics

## 11. Risks And Mitigations

### 11.1 Risk: descriptor/runtime semantics change behavior relative to legacy policy

Current descriptors and legacy policy already disagree for some plugins.

Mitigation:

- accept descriptor/runtime truth explicitly as the final authority
- document the semantic convergence instead of treating it as accidental regression
- lock the intended mapping with bridge tests

### 11.2 Risk: CTKManager becomes tightly coupled to platform-layer types

If bridge integration is done poorly, `CTKManager` could gain too much knowledge of descriptor internals.

Mitigation:

- keep `PlatformCtkPolicyBridge` thin
- have the bridge return a small resolved result instead of exposing full descriptor internals everywhere

### 11.3 Risk: missing descriptor handling becomes unstable

Unknown or legacy-only plugins may still exist in some compatibility scenarios.

Mitigation:

- use conservative fallback
- emit explicit diagnostic codes
- do not revive legacy policy fallback

## 12. Expected Outcome

After this slice lands, the runtime should communicate one consistent fact:

- product startup truth is descriptor/runtime-driven
- internal CTK bucket classification is also descriptor/runtime-driven
- `PluginLoadPolicy` no longer influences product runtime semantics, even indirectly
- the next cleanup slice can focus on deleting the remaining compatibility residue instead of still debating which truth source CTK internals obey
