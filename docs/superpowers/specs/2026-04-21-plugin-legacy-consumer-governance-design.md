# Plugin Legacy Consumer Governance Design

## Scope Note

- This document defines the governance slice after `plugin truth source governance`.
- This slice does not change the product startup mainline truth source.
- The purpose is to classify, constrain, and document the remaining consumers of the legacy CTK load-policy chain.
- This slice separates `product runtime artifacts` from `compatibility runtime artifacts` so runtime acceptance can stop mixing the two concerns.
- This slice does not remove `PluginLoadPolicy` yet and does not rewrite `CTKManager` internal policy behavior in one batch.

## Implementation Links

- Base governance design: `docs/superpowers/specs/2026-04-16-platform-kernel-governance-design.md`
- Plugin-chain remediation Phase 1 design: `docs/superpowers/specs/2026-04-20-plugin-chain-remediation-phase1-design.md`
- Plugin-chain remediation Phase 2 design: `docs/superpowers/specs/2026-04-20-plugin-chain-remediation-phase2-design.md`
- Plugin truth-source governance design: `docs/superpowers/specs/2026-04-21-plugin-truth-source-governance-design.md`
- Decision log: `docs/superpowers/tracking/platform-migration-decision-log.md`
- Current project overview: `docs/current_status_and_project_overview.md`

Date: 2026-04-21  
Scope: govern the remaining consumers of `plugin_load_policy.json`, `PluginLoadPolicy`, `CTKManager::loadPluginPolicy()`, and `CTKManager::installPluginsFromDirectory()` without changing the product startup mainline  
Goal: make the remaining legacy-policy consumer set explicit, testable, and documented, so product runtime acceptance no longer treats compatibility artifacts as universal runtime truth.

## 1. Design Background

The previous truth-source governance slice already established that product startup truth is:

- `config/platform_runtime.json`
- `plugins/descriptors/*.json`
- `PlatformDescriptorLoader`
- descriptor-driven startup and governed activation

That work successfully removed the legacy policy chain from `main.cpp` product assembly and added acceptance to lock the boundary.

However, the repository still contains a second layer of ambiguity:

- some runtime acceptance still treats `plugin_load_policy.json` as if it were a general runtime artifact
- `CTKManager` still contains internal compatibility-oriented policy behavior
- there is no single inventory that says which remaining legacy consumers are acceptable, temporary, or forbidden

This creates a more subtle governance problem than the original truth-source ambiguity:

1. product-mainline truth is already clear, but compatibility scope is not
2. tests still partially blur `product runtime requirements` with `compatibility runtime requirements`
3. future contributors still cannot easily tell whether a new legacy-policy call site is an allowed compatibility use or an architecture regression

This slice resolves that second-order ambiguity by explicitly governing the remaining legacy consumers instead of silently tolerating them.

## 2. Decision Summary

The following decisions are fixed for this slice:

- Product startup truth remains unchanged and is not reopened in this batch.
- Remaining legacy-policy consumers must be classified into explicit governance buckets.
- Product runtime artifact acceptance must not require compatibility-only artifacts by default.
- Compatibility-only runtime artifacts must have their own dedicated acceptance path.
- Internal legacy consumers that are not yet removed may remain temporarily, but they must be documented as internal compatibility debt rather than implied product truth.
- New product-mainline consumers of `PluginLoadPolicy` or legacy CTK load-policy helpers are forbidden.

## 3. Goals And Non-Goals

### 3.1 Goals

This slice must achieve the following goals:

1. Produce an explicit inventory of remaining legacy-policy consumers.
2. Separate product runtime artifact acceptance from compatibility runtime artifact acceptance.
3. Define which current legacy consumers are allowed, temporary, or forbidden.
4. Prevent new drift where tests or docs accidentally treat compatibility artifacts as product runtime truth.
5. Keep current runtime behavior stable while improving governance clarity.

### 3.2 Non-Goals

This slice does not do the following:

- It does not delete `plugin_load_policy.json`.
- It does not remove `PluginLoadPolicy` from `CTKManager` internals yet.
- It does not redesign deferred/on-demand plugin behavior.
- It does not widen the managed startup scope.
- It does not modify UI routing, welcome-page behavior, or startup-shell behavior.
- It does not convert every internal legacy consumer into descriptor-driven logic in one batch.

## 4. Legacy Consumer Classification Model

### 4.1 Consumer Buckets

All remaining legacy-policy consumers must be classified into one of the following buckets:

- `forbidden_product_mainline`
  - any call site that would influence product startup truth, governed plugin recognition, or product-visible startup reasoning
- `allowed_compatibility_surface`
  - explicit compatibility helpers or compatibility runtime artifacts that remain intentionally available
- `temporary_internal_compatibility_debt`
  - internal code that still depends on legacy-policy data but is not part of the product mainline and is not yet removed

No uncategorized legacy consumer should remain after this slice.

### 4.2 Current Expected Mapping

This slice assumes the following initial mapping:

- `main.cpp`
  - bucket: `forbidden_product_mainline`
  - expected state: no legacy-policy helper calls
- `config/plugin_load_policy.json`
  - bucket: `allowed_compatibility_surface`
  - expected state: shipped only as compatibility metadata, not product runtime truth
- `config/plugin_load_policy_compatibility.md`
  - bucket: `allowed_compatibility_surface`
  - expected state: sidecar note explaining the artifact boundary
- `CTKManager::loadPluginPolicy()`
  - bucket: `allowed_compatibility_surface`
  - expected state: documented compatibility entry, not product bootstrap
- `CTKManager::installPluginsFromDirectory()`
  - bucket: `allowed_compatibility_surface`
  - expected state: documented compatibility helper, not product bootstrap
- `CTKManager::policyForPlugin()` and `CTKManager::applyPolicyForPlugin()`
  - bucket: `temporary_internal_compatibility_debt`
  - expected state: explicitly documented as internal remaining debt, not product truth

If scan results reveal additional live consumers, they must be added to the same classification table before implementation is considered complete.

## 5. Runtime Artifact Governance Boundary

### 5.1 Product Runtime Artifacts

The default product runtime acceptance must cover only artifacts required by the product mainline, including:

- governed plugin bundles required by the current acceptance scope
- `config/platform_runtime.json`
- descriptor runtime layout under `plugins/descriptors`
- other product-mainline runtime assets already required by the current runtime tests

These tests must not require `plugin_load_policy.json` by default.

### 5.2 Compatibility Runtime Artifacts

Compatibility runtime acceptance must cover the legacy-policy artifact set explicitly, including:

- `config/plugin_load_policy.json`
- `config/plugin_load_policy_compatibility.md`

This acceptance exists to prove that compatibility artifacts still ship intentionally, not to imply that they define product truth.

### 5.3 Required Testing Separation

This slice must restructure tests so that:

- `runtime_artifact_layout_test` verifies product runtime artifacts only
- a dedicated compatibility runtime contract verifies compatibility artifacts
- truth-source runtime contract remains focused on boundary semantics rather than generic runtime layout

The result should make it obvious which failures indicate:

- product runtime breakage
- compatibility artifact drift
- truth-source governance regression

## 6. Source And Documentation Governance

### 6.1 Source-Level Governance

The codebase must make the following facts easy to discover:

- `main.cpp` is forbidden from using legacy-policy helpers
- `CTKManager` compatibility helpers still exist, but are not product-mainline APIs
- remaining internal dependence on `PluginLoadPolicy` is recognized technical debt, not implied architecture approval

This slice does not require renaming every internal function, but it does require that the governance status be explicit in comments, tests, or documentation.

### 6.2 Documentation-Level Governance

The following documents must align after this slice:

- current status
- decision log
- governance matrix
- new legacy-consumer inventory or tracking doc

These docs must answer:

1. what is product truth
2. what is compatibility-only
3. which remaining internal legacy consumers are still tolerated temporarily
4. which new usages are forbidden

## 7. Testing Strategy And Acceptance

### 7.1 Test Goals

Minimum acceptance for this slice must prove:

1. product runtime artifact tests no longer require `plugin_load_policy.json`
2. compatibility runtime artifacts are still intentionally shipped and verified separately
3. `main.cpp` still contains no legacy-policy helper usage
4. remaining legacy consumers are inventoried and documented
5. documentation and tests describe the same consumer-boundary model

### 7.2 Recommended Acceptance Coverage

Recommended coverage includes:

- update `runtime_artifact_layout_test` so it validates product runtime artifacts only
- add a dedicated compatibility runtime contract test
- add a source-contract or inventory test that protects the allowed/temporary/forbidden consumer boundary
- retain the existing truth-source governance source contract and runtime contract

### 7.3 Acceptance Criteria

This slice is accepted only when all of the following are true:

- product runtime acceptance no longer treats compatibility artifacts as universal runtime requirements
- compatibility artifacts still have dedicated acceptance and documentation
- a reader can identify the remaining allowed legacy consumers without scanning the entire repo manually
- no new product-mainline legacy consumer is introduced
- docs, runtime tests, and source-contract tests agree on the same classification

## 8. Implementation Batches

This slice should land in four batches:

### Batch 1: legacy consumer inventory

- scan the repository for remaining legacy-policy consumers
- write the consumer inventory and classification
- document allowed, temporary, and forbidden buckets

### Batch 2: runtime acceptance separation

- remove `plugin_load_policy.json` from default product runtime layout acceptance
- add a dedicated compatibility runtime contract
- keep truth-source governance tests aligned with the new split

### Batch 3: governance hardening

- add source-level or scan-level protection so new forbidden consumers are caught early
- make internal remaining debt explicit where needed

### Batch 4: status write-back

- update current status, decision log, governance matrix, and implementation tracking
- record the accepted consumer-boundary model for future cleanup slices

## 9. Risks And Mitigations

### 9.1 Risk: test meaning becomes less obvious during the split

If runtime acceptance is split poorly, maintainers may not know which test to run or how to interpret failures.

Mitigation:

- name tests according to governance meaning
- keep product runtime, compatibility runtime, and truth-source tests distinct
- document each test's ownership clearly in the implementation plan

### 9.2 Risk: internal legacy debt gets mistaken for approved long-term architecture

If internal `CTKManager` legacy dependencies remain undocumented, future code may keep building on top of them.

Mitigation:

- classify them explicitly as `temporary_internal_compatibility_debt`
- record them in docs and source-level notes
- forbid new product-mainline usage even while the internal debt remains temporarily

### 9.3 Risk: compatibility artifacts get removed accidentally

Once product runtime tests stop requiring `plugin_load_policy.json`, a future packaging change could accidentally stop shipping it.

Mitigation:

- keep a dedicated compatibility runtime contract
- keep the sidecar note as a runtime acceptance artifact

## 10. Expected Outcome

After this slice lands, the codebase should communicate the following clearly:

- product runtime truth and product runtime artifacts are one thing
- compatibility runtime artifacts are a different thing
- remaining legacy-policy consumers are explicitly governed rather than silently tolerated
- the next cleanup slice can remove internal legacy debt from a stable, documented baseline instead of from a blurry mixed-state system
