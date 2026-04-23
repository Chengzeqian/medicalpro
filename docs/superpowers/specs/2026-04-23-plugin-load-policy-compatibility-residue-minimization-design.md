# Plugin Load Policy Compatibility Residue Minimization Design

## Scope Note

- This document defines the cleanup slice after `CTK descriptor policy bridge`.
- This slice does not reopen product startup truth or CTK runtime classification truth.
- The purpose is to minimize the remaining `PluginLoadPolicy / plugin_load_policy.json` compatibility residue to an explicit, governed, and smallest-possible shell.
- This slice preserves the final compatibility entry only where a live compatibility surface still exists.
- This slice does not delete every compatibility artifact in one batch, but it does remove the remaining false signals that legacy policy metadata is still an authoritative strategy source.

## Implementation Links

- Plugin truth-source governance design: `docs/superpowers/specs/2026-04-21-plugin-truth-source-governance-design.md`
- Plugin legacy consumer governance design: `docs/superpowers/specs/2026-04-21-plugin-legacy-consumer-governance-design.md`
- CTK descriptor policy bridge design: `docs/superpowers/specs/2026-04-22-ctk-descriptor-policy-bridge-design.md`
- Legacy consumer inventory: `docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md`
- Decision log: `docs/superpowers/tracking/platform-migration-decision-log.md`
- Governance matrix: `docs/superpowers/tracking/platform-plugin-governance-matrix.md`
- Current project overview: `docs/current_status_and_project_overview.md`

Date: 2026-04-23  
Scope: minimize the remaining `PluginLoadPolicy` and `plugin_load_policy.json` compatibility residue into an explicit, smallest-possible compatibility shell  
Goal: keep only the compatibility entry and metadata that still have a live, governed reason to exist, while deleting dead APIs, shrinking legacy policy content to a minimal projection, and preventing compatibility metadata from regrowing into a second truth source.

## 1. Design Background

The repository has already completed three important governance steps:

- product startup truth is locked to `config/platform_runtime.json + plugins/descriptors/*.json + PlatformDescriptorLoader`
- default product runtime artifact acceptance no longer requires `plugin_load_policy.json`
- `CTKManager` runtime bucket classification and safe-mode criticality now resolve through descriptor/runtime truth via `PlatformCtkPolicyBridge`

That means the old legacy policy chain no longer decides product startup truth or CTK runtime semantics.

However, one compatibility residue still remains:

- `CTKManager::loadPluginPolicy()` still exists as a compatibility entry
- `PluginLoadPolicy` still exposes a broad query-style API surface
- `plugin_load_policy.json` still looks like a full strategy table instead of a constrained compatibility artifact
- compatibility artifact shipping still relies on copying the whole `config/` directory instead of explicit compatibility governance

This creates a new class of governance ambiguity:

1. the product truth source is already settled, but the compatibility shell still looks larger and more authoritative than it really is
2. dead query APIs suggest that legacy policy remains readable runtime truth even though current product code no longer uses them
3. `plugin_load_policy.json` still contains historical plugin entries outside the currently descriptor-governed plugin set, which makes the file look like a general plugin catalog rather than a narrow compatibility projection
4. whole-directory config deployment hides whether compatibility artifacts are retained intentionally or only by build inertia

This slice removes that ambiguity by shrinking the compatibility shell until it reflects only live, governed compatibility needs.

## 2. Decision Summary

The following decisions are fixed for this slice:

- Compatibility residue may preserve an entry and a carrier, but not strategy authority.
- `CTKManager::loadPluginPolicy()` may remain temporarily as the final compatibility entry, but it must remain metadata-only.
- `PluginLoadPolicy` must be reduced from a legacy strategy query surface to a minimal compatibility metadata carrier.
- `plugin_load_policy.json` must no longer behave like a historical full plugin strategy snapshot.
- `plugin_load_policy.json` may retain only the current descriptor-governed CTK plugin set as a minimal compatibility projection.
- Compatibility artifacts must move toward explicit deployment governance rather than implicit survival through whole-directory config copying.
- New compatibility residue may not be introduced without inventory, documentation, and contract coverage first.

## 3. Goals And Non-Goals

### 3.1 Goals

This slice must achieve the following goals:

1. classify the remaining `PluginLoadPolicy / plugin_load_policy.json` residue into keep, shrink, or delete outcomes
2. reduce `PluginLoadPolicy` to the smallest API surface still justified by a live compatibility entry
3. reduce `plugin_load_policy.json` to a minimal compatibility projection of the currently descriptor-governed CTK plugin set
4. prevent compatibility metadata from expressing a second copy of runtime policy truth
5. make compatibility artifact deployment intentional and test-owned
6. align docs, inventory, and tests on the same residue-minimization model

### 3.2 Non-Goals

This slice does not do the following:

- it does not reopen product startup truth ownership
- it does not reintroduce `PluginLoadPolicy` into runtime bucket or safe-mode semantics
- it does not delete `CTKManager::loadPluginPolicy()` in this batch
- it does not require automatic generation of `plugin_load_policy.json` yet
- it does not convert every non-governed legacy plugin in the repo into descriptor-governed plugins
- it does not change UI routing or startup shell behavior

## 4. Residue Evaluation Model

### 4.1 Four Evaluation Gates

Each remaining residue item must pass the following gates before it can remain:

- `live_consumer_gate`
  - a residue item may remain only if it still has a live consumer in source, tests, or governed documentation
  - vague future compatibility guesses are not enough
- `truth_overlap_gate`
  - if a field or API duplicates stable descriptor/runtime truth, it must not remain as an independent policy fact
- `drift_cost_gate`
  - if an item has already drifted from descriptor/runtime truth and no governed product path consumes that drift, it should not remain in a form that looks authoritative
- `explicit_governance_gate`
  - anything retained must have explicit inventory, documentation, deployment ownership, and contract coverage

### 4.2 Outcome Buckets

Every residue item must be placed into one of three buckets:

- `retained`
  - a live compatibility entry or note that still has an explicit reason to exist
- `shrunk`
  - an item that still needs to exist, but only as a smallest-possible compatibility shell
- `deleted`
  - an item with no live justified consumer, or one that creates false authority or unmanaged drift

### 4.3 Governing Rules

This slice fixes the following governing rules:

- compatibility residue may retain an entry and a carrier, but not runtime strategy authority
- compatibility residue may retain only the smallest proven-needed information, not a historical full snapshot

## 5. Current Residue Classification

### 5.1 Retained Compatibility Surfaces

The following items remain in this slice:

- `CTKManager::loadPluginPolicy()`
  - retained as the final compatibility entry for loading legacy metadata
  - must not imply startup truth or CTK runtime semantics
- `config/plugin_load_policy_compatibility.md`
  - retained as the sidecar note that explains the compatibility-only boundary
- `plugin_legacy_compatibility_runtime_contract_test`
  - retained as the dedicated runtime acceptance owner for explicit compatibility artifacts

### 5.2 Shrunk Compatibility Surfaces

The following items remain only in reduced form:

- `PluginLoadPolicy`
  - shrunk from a policy query surface into a minimal compatibility metadata carrier
  - the intended surviving responsibilities are:
    - `loadConfig(...)`
    - `configPath()`
    - `hasValidConfig()`
  - exact naming may remain stable if needed for compatibility, but the type must no longer present itself as a strategy authority

- `config/plugin_load_policy.json`
  - shrunk from a historical full plugin policy table into a minimal compatibility projection
  - it may cover only the current descriptor-governed CTK plugin set:
    - `UserManagement`
    - `DicomViewer`
    - `FourViewDisplay`
    - `RegistrationCore`
    - `OpticalTracking`
  - even if the old schema is temporarily retained, the file must no longer be treated as an independently curated strategy table

- compatibility artifact deployment
  - shrunk from whole-directory `config/` inheritance toward explicit compatibility artifact ownership
  - the design target is intentional compatibility deployment, not accidental survival through `copy_directory(config)`

### 5.3 Deleted Surfaces

The following items are delete targets in this slice:

- dead `PluginLoadPolicy` query APIs
  - `getLoadPolicy(...)`
  - `getDependencies(...)`
  - `isCriticalPlugin(...)`
  - `getPluginsByPolicy(...)`
  - `getCriticalPlugins()`
  - `getAllConfiguredPlugins()`

- historical `plugin_load_policy.json` entries outside the current descriptor-governed plugin set
  - `BoneSegmentation`
  - `InstrumentManagement`
  - `Registration2D3D`
  - `PointRegistration`
  - `OpticalRegistration`

- the assumption that copying the whole `config/` directory is an acceptable long-term compatibility governance model

## 6. Minimal Compatibility Projection Model

### 6.1 Projection Boundary

`plugin_load_policy.json` must now be understood as a compatibility projection, not a truth source.

That means:

- descriptor/runtime truth remains authoritative
- `plugin_load_policy.json` may preserve a compatibility-shaped view of a small governed subset
- `plugin_load_policy.json` must not define startup truth, runtime bucket truth, criticality truth, or plugin catalog truth

### 6.2 Allowed Coverage

The projection may contain only plugins that are both:

- part of the currently descriptor-governed CTK plugin set
- represented by the platform-governed descriptor surface in the current architecture

In the current repository, that means plugins whose authored descriptors live under `Plugins/*/platform/plugin.json` and whose governed runtime descriptors are deployed under `plugins/descriptors/*.json`.

This slice therefore limits coverage to:

- `UserManagement`
- `DicomViewer`
- `FourViewDisplay`
- `RegistrationCore`
- `OpticalTracking`

### 6.3 Projection Drift Rules

The projection must obey the following rules:

- it must not contain historical plugin entries outside the current descriptor-governed set
- it must not express more startup semantics than descriptor/runtime truth already provides
- if descriptor/runtime truth changes for one of the retained plugins, the compatibility projection must be updated in the same slice
- new plugins may not be added to the projection unless they first join the descriptor-governed set

### 6.4 Projection Semantics

This slice does not require automatic projection generation yet.

However, even if the file remains hand-maintained temporarily:

- it must be documented as a projection
- it must be kept consistent with descriptor/runtime truth
- it must not carry unmanaged historical drift

Future cleanup may replace the hand-maintained projection with generated output.

## 7. API Surface Minimization

### 7.1 `PluginLoadPolicy` Direction

After this slice, `PluginLoadPolicy` should no longer look like a service used to answer runtime strategy questions.

Instead, it should clearly behave as:

- a compatibility metadata loader
- a compatibility metadata presence tracker
- a narrow carrier for the final compatibility entry path

### 7.2 Forbidden API Semantics

After this slice, the repository must not reintroduce `PluginLoadPolicy` APIs that imply:

- runtime bucket authority
- criticality authority
- dependency-authoritative plugin ordering
- general plugin inventory truth

### 7.3 Naming Constraint

This slice does not require renaming the `PluginLoadPolicy` type itself.

However, all comments, contracts, and documentation must make the shrunk semantics explicit so the stable type name cannot be misread as preserved strategy authority.

## 8. Compatibility Deployment Governance

### 8.1 Current Problem

Today, compatibility artifacts survive because the build copies the whole `config/` directory into runtime output.

That is not precise enough for long-term governance because it does not tell maintainers:

- which config files are product-mainline truth
- which config files are explicit compatibility artifacts
- which artifacts are retained intentionally rather than accidentally

### 8.2 Target Direction

This slice establishes the design direction that compatibility artifacts must be explicitly governed in deployment.

That means future runtime output ownership should answer:

- which compatibility artifacts are intentionally shipped
- which test owns each shipped compatibility artifact
- which docs describe their boundary and purpose

This slice does not require a perfect final deployment abstraction, but it does require movement away from unqualified whole-directory inheritance as the governing model.

## 9. Testing Strategy

### 9.1 Required Contract Layers

This slice should add or tighten the following contract protection:

- `compatibility residue inventory contract`
  - protects the explicit allowed residue list
- `PluginLoadPolicy api surface contract`
  - protects the reduced API surface and prevents dead query APIs from returning
- `plugin_load_policy projection contract`
  - protects the minimal plugin set and prevents historical entries from reappearing
- `compatibility artifact deployment contract`
  - protects explicit compatibility artifact deployment ownership

### 9.2 Minimum Test Intent

Minimum acceptance coverage should prove:

1. `PluginLoadPolicy` no longer exposes strategy query semantics
2. `plugin_load_policy.json` contains only the descriptor-governed minimal plugin set
3. compatibility artifact acceptance still exists, but it is explicitly scoped
4. docs and inventory describe the same residue-minimization model
5. product mainline still does not read legacy policy metadata to decide startup behavior

### 9.3 Drift Prevention Rules

The tests must make the following drift hard to reintroduce:

- re-adding removed historical plugin entries to `plugin_load_policy.json`
- re-adding dead policy query APIs to `PluginLoadPolicy`
- treating compatibility projection fields as independent strategy truth
- retaining compatibility artifacts without explicit inventory and contract ownership

## 10. Documentation And Governance Write-Back

The following documents must align after this slice:

- `docs/current_status_and_project_overview.md`
- `docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md`
- `docs/superpowers/tracking/platform-plugin-governance-matrix.md`
- `docs/superpowers/tracking/platform-migration-decision-log.md`

They must all describe the same final picture:

- product truth remains descriptor/runtime-driven
- `PluginLoadPolicy` is only a minimal compatibility carrier
- `plugin_load_policy.json` is only a minimal compatibility projection
- historical non-governed entries are no longer treated as an accepted compatibility baseline
- compatibility artifact deployment must be explicit and test-owned

## 11. Risks And Mitigations

### 11.1 Risk: unknown compatibility dependency is removed too aggressively

If some undocumented compatibility path still needs the final entry, a full deletion slice could create surprise regressions.

Mitigation:

- keep `CTKManager::loadPluginPolicy()` in this batch
- remove dead query APIs and historical projection entries first
- defer full entry deletion to a later slice once the final compatibility entry is proven unused

### 11.2 Risk: the projection drifts again by hand

If the file remains hand-maintained without governance, it may again diverge from descriptor/runtime truth.

Mitigation:

- define it explicitly as a projection
- shrink the projection scope
- add contract tests for retained plugin set and retained semantics
- consider generation in a later cleanup slice

### 11.3 Risk: deployment cleanup creates noisy test churn

If deployment changes and semantic cleanup are mixed too aggressively, failures may become harder to interpret.

Mitigation:

- first lock the explicit compatibility artifact list in contract tests
- then tighten deployment ownership against that list
- keep product artifact and compatibility artifact failures clearly separated

## 12. Acceptance Criteria

This slice is accepted only when all of the following are true:

- `PluginLoadPolicy` no longer exposes policy query APIs that imply runtime truth authority
- `plugin_load_policy.json` no longer contains historical entries outside the descriptor-governed minimal plugin set
- `plugin_load_policy.json` no longer behaves like a full plugin strategy snapshot
- compatibility artifact acceptance still verifies the retained compatibility shell, but not a vague historical config bundle
- product mainline still does not use legacy policy metadata to decide startup behavior
- inventory, docs, decision log, and governance matrix all describe the same minimized residue boundary

## 13. Implementation Batches

This slice should land in four batches:

### Batch 1: residue classification and contract hardening

- update inventory and source contracts to describe the minimized residue model
- add API surface protection for `PluginLoadPolicy`

### Batch 2: `PluginLoadPolicy` surface reduction

- remove dead query APIs
- keep only the minimal compatibility carrier behavior

### Batch 3: compatibility projection reduction

- reduce `plugin_load_policy.json` to the descriptor-governed minimal plugin set
- tighten compatibility projection tests

### Batch 4: deployment and governance write-back

- tighten compatibility artifact deployment ownership
- update current status, governance matrix, decision log, and acceptance notes

## 14. Final Design Summary

This slice intentionally does not jump straight to full deletion.

Instead, it establishes a stricter intermediate architecture:

- keep the last compatibility entry
- keep the sidecar note
- keep the explicit compatibility runtime contract
- shrink `PluginLoadPolicy` into a minimal carrier
- shrink `plugin_load_policy.json` into a minimal compatibility projection
- delete dead APIs, delete historical non-governed entries, and delete the assumption that accidental config-directory copying counts as compatibility governance

The result should be a compatibility shell that is:

- explicit
- smallest-possible
- test-owned
- unable to masquerade as a second truth source
