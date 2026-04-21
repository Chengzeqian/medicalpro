# Plugin Truth Source Governance Design

## Scope Note

- This document defines the next governance slice after plugin-chain remediation Phase 2 and the cold-start welcome-entry correction.
- This slice is about source-of-truth consolidation, not feature expansion.
- The goal is to remove ambiguity around which runtime artifacts define product startup, plugin discovery, and governed service activation.
- This slice does not delete the remaining legacy CTK policy chain yet, but it formally demotes that chain to compatibility-only status.
- This slice does not widen cold-start managed scope, does not redesign the welcome UI, and does not rewrite all plugin implementations in one batch.

## Implementation Links

- Base governance design: `docs/superpowers/specs/2026-04-16-platform-kernel-governance-design.md`
- Plugin-chain remediation Phase 1 design: `docs/superpowers/specs/2026-04-20-plugin-chain-remediation-phase1-design.md`
- Plugin-chain remediation Phase 2 design: `docs/superpowers/specs/2026-04-20-plugin-chain-remediation-phase2-design.md`
- Cold-start welcome entry design: `docs/superpowers/specs/2026-04-21-cold-start-welcome-shell-design.md`
- Decision log: `docs/superpowers/tracking/platform-migration-decision-log.md`
- Current project overview: `docs/current_status_and_project_overview.md`

Date: 2026-04-21  
Scope: consolidate `medicalpro` startup and plugin-governance truth around descriptor-driven runtime artifacts while demoting legacy CTK load-policy paths to compatibility-only status  
Goal: make `config/platform_runtime.json + plugins/descriptors/*.json + PlatformDescriptorLoader` the only product truth source for governed startup and plugin recognition, so the runtime can explain what is loaded, why it is loaded, and which chain is authoritative.

## 1. Design Background

The recent governance work has already stabilized the main product path around descriptor-driven startup:

- `main.cpp` reads `config/platform_runtime.json`
- runtime descriptors are loaded from `plugins/descriptors`
- `PlatformDescriptorLoader` interprets runtime plugin descriptors
- startup and on-demand activation are routed through governed services instead of direct UI-side CTK calls

At the same time, the old CTK policy chain still exists in the codebase:

- `config/plugin_load_policy.json`
- `CTKManager::loadPluginPolicy()`
- `CTKManager::installPluginsFromDirectory()`

That legacy path is no longer the current product startup path, but it still exists as a believable alternative. This creates three ongoing problems:

1. developers can no longer tell which files define real startup truth
2. diagnostics and runtime reasoning become harder because two different chains appear to describe plugin loading
3. future fixes risk reintroducing accidental startup regressions by wiring new code to the wrong chain

The current product is therefore functionally closer to a single governed path than the codebase language suggests, but the architecture still communicates mixed ownership.

This slice resolves that ambiguity by making the product truth source explicit and by formally reclassifying the legacy policy chain as compatibility-only.

## 2. Decision Summary

The following decisions are fixed for this slice:

- Product startup truth is defined only by:
  - `config/platform_runtime.json`
  - `plugins/descriptors/*.json`
  - `PlatformDescriptorLoader`
  - descriptor-driven startup orchestration
  - governed on-demand activation
- `config/plugin_load_policy.json` remains in the repository for now, but it is no longer part of the product startup truth.
- `CTKManager::loadPluginPolicy()` and `CTKManager::installPluginsFromDirectory()` remain available only for compatibility or tooling scenarios, not for the product mainline.
- New product-facing code must not consult legacy load policy to decide what plugins exist, what should start, or how plugin readiness is explained.
- This slice prefers a soft convergence path:
  - keep legacy code present
  - reduce its authority
  - document and test the new boundary
  - make accidental fallback detectable
- This slice does not remove legacy code by force and does not widen the current cold-start managed-core scope.

## 3. Goals And Non-Goals

### 3.1 Goals

This slice must achieve the following goals:

1. Define a single product startup truth source in code and documentation.
2. Ensure plugin recognition for governed startup and governed activation flows comes only from descriptor artifacts.
3. Make it explicit that `plugin_load_policy.json` is compatibility-only and not a product-authoritative source.
4. Prevent future product code from silently falling back to the legacy CTK policy chain.
5. Align diagnostics, lifecycle reasoning, and recovery language with descriptor-driven truth.
6. Keep the current runtime behavior stable while reducing architecture ambiguity.

### 3.2 Non-Goals

This slice does not do the following:

- It does not hard-delete all legacy CTK policy code.
- It does not migrate every unmanaged plugin into governed scope.
- It does not change the current Phase 1 cold-start plugin set.
- It does not rewrite plugin business implementations.
- It does not redesign `WelcomePage`, `MainInterfaceWidget`, or other UI flows.
- It does not introduce a second startup policy format.

## 4. Product Truth Source Definition

### 4.1 Authoritative Runtime Artifacts

For product runtime behavior, the only authoritative artifacts are:

- `config/platform_runtime.json`
  - defines runtime mode
  - defines descriptor directory
  - defines managed startup scope
- `plugins/descriptors/*.json`
  - define governed plugin identity
  - define runtime symbolic name and bundle facts
  - define service and diagnostics contracts
  - define startup policy such as `managed_core` or `on_demand`
- `PlatformDescriptorLoader`
  - loads and validates descriptors
  - builds the in-memory governed plugin catalog

No product-facing startup or governed activation decision may depend on `plugin_load_policy.json` once equivalent descriptor/runtime facts exist.

### 4.2 Authoritative Execution Chain

The governed execution chain is:

1. `main.cpp` reads `platform_runtime.json`
2. `PlatformDescriptorLoader` loads descriptors from `plugins/descriptors`
3. runtime services build startup or on-demand plans from descriptor facts
4. coordinators execute those plans under the current runtime mode
5. diagnostics and state write-back explain outcomes using the same descriptor-derived truth

This means product startup, governed plugin recognition, service-ready gating, and diagnostics must all share the same artifact root.

### 4.3 What Is No Longer Product Truth

The following items are explicitly not product-authoritative anymore:

- `config/plugin_load_policy.json`
- direct CTK directory scans as a source of product plugin identity
- policy-only plugin lists that are not backed by runtime descriptors
- UI-side logic that infers plugin load order from symbolic-name conventions

These paths may still exist for compatibility, but they may not define product semantics.

## 5. Compatibility-Only Demotion Rules

### 5.1 Compatibility-Only Meaning

For this project, compatibility-only means:

- the code may continue to exist
- the code may support migration or fallback tooling scenarios
- the code is not allowed to define the product mainline
- the code must not be consulted by default product startup assembly
- the code must not be treated as the source of record in diagnostics or docs

### 5.2 Required Demotion Outcomes

This slice must make the following outcomes explicit:

- `plugin_load_policy.json` is documented as legacy compatibility metadata.
- `CTKManager::loadPluginPolicy()` is documented and named as a compatibility entry, not a default bootstrap step.
- `CTKManager::installPluginsFromDirectory()` is documented as a low-level compatibility helper, not a product startup contract.
- If the product mainline does not call these APIs, tests and docs must say so clearly.

### 5.3 Forbidden New Usage

After this slice lands, new product-facing code must not:

- read `plugin_load_policy.json` to discover startup plugins
- call `loadPluginPolicy()` to decide governed startup content
- call `installPluginsFromDirectory()` as the hidden main bootstrap path
- describe legacy policy metadata as equivalent to runtime descriptors

## 6. Code Touchpoints And Consolidation Strategy

### 6.1 Mainline Ownership

`main.cpp` remains the place where product runtime assembly makes the startup truth visible. It must continue to:

- load `platform_runtime.json`
- resolve the descriptor directory from runtime config
- load descriptors through `PlatformDescriptorLoader`
- hand governed truth into startup and on-demand runtime services

This file should not regain hidden policy-driven plugin discovery.

### 6.2 Descriptor Loader Boundary

`PlatformDescriptorLoader` remains the catalog boundary for governed plugins. This slice should tighten that contract so future readers can clearly answer:

- which plugins are known to the governed runtime
- where that knowledge comes from
- whether a plugin is startup-managed, on-demand, or out of scope

### 6.3 CTK Manager Boundary

`CTKManager` still exposes low-level plugin framework operations, but this slice reclassifies part of that surface:

- keep framework control and low-level plugin operations where needed
- demote policy-loading helpers out of product-mainline semantics
- avoid letting `CTKManager` imply that directory scanning plus policy loading is the authoritative startup model

### 6.4 Documentation Boundary

This slice must update project docs so that design docs, implementation plans, and current-status summaries all describe the same truth:

- descriptors define product plugin identity
- runtime config selects mode and descriptor root
- legacy load policy is compatibility-only

## 7. Testing Strategy And Acceptance

### 7.1 Test Goals

This slice needs executable proof for architecture boundaries, not only prose.

Minimum acceptance must prove:

1. product startup assembly uses `platform_runtime.json` and descriptor loading
2. governed plugin recognition comes from descriptors
3. legacy CTK policy helpers are not part of the product mainline call path
4. diagnostics and code comments do not describe `plugin_load_policy.json` as authoritative
5. compatibility-only APIs remain available without regaining product authority

### 7.2 Recommended Acceptance Coverage

Recommended test coverage includes:

- startup-entry contract test that confirms `main.cpp` assembles runtime from runtime config plus descriptors
- descriptor-catalog contract test that confirms governed plugin identity is descriptor-driven
- legacy-boundary contract test that confirms startup mainline does not invoke `loadPluginPolicy()` or `installPluginsFromDirectory()`
- compatibility-surface test that confirms the legacy APIs remain buildable and intentionally separate from the governed path
- documentation or diagnostics string assertions where needed to prevent future language drift

### 7.3 Acceptance Criteria

This slice is accepted only when all of the following are true:

- a reader can identify the single product truth source from the codebase without guessing
- `platform_runtime.json + descriptors + PlatformDescriptorLoader` are the only mainline-governed source of plugin truth
- legacy policy artifacts remain present but clearly demoted
- no new product code path silently depends on legacy load-policy startup
- docs and tests describe the same boundary

## 8. Implementation Batches

This slice should land in four batches:

### Batch 1: truth-source clarification

- add design and implementation docs for truth-source consolidation
- update code comments or naming where the product mainline boundary is currently ambiguous

### Batch 2: mainline boundary hardening

- add or update tests that prove `main.cpp` and governed startup stay descriptor-driven
- add contract coverage that legacy CTK policy helpers are not called by the product mainline

### Batch 3: compatibility demotion

- update legacy config and helper documentation to mark them compatibility-only
- tighten any remaining call sites or comments that still imply policy-driven startup authority

### Batch 4: status write-back

- update current status and decision log after implementation lands
- record the accepted source-of-truth boundary for future slices

## 9. Risks And Rollback Strategy

### 9.1 Risk: architecture language drifts faster than runtime behavior

Risk:
- the code may keep behaving correctly while comments, docs, or helper names still imply two competing truth sources

Mitigation:
- treat this as an executable-boundary slice, not a docs-only slice
- require tests that lock the mainline call path

### 9.2 Risk: accidental regression through legacy helper reuse

Risk:
- future cleanup or startup work may reuse `loadPluginPolicy()` or `installPluginsFromDirectory()` because those APIs appear convenient

Mitigation:
- document them as compatibility-only
- add contract tests that fail if they re-enter the mainline startup path

### 9.3 Risk: demotion is mistaken for deletion

Risk:
- developers may assume the legacy policy path is being removed immediately and fear breaking tooling or migration scenarios

Mitigation:
- keep the slice soft and explicit
- preserve compatibility helpers
- document that the change is about authority, not immediate deletion

### 9.4 Rollback Strategy

If this slice causes confusion or uncovers an unseen compatibility dependency, rollback should be limited to:

- reverting the truth-source hardening tests or comments
- restoring prior documentation language temporarily

Rollback should not require undoing the descriptor-driven startup chain itself, because that chain is already the active product path.

## 10. Final Recommendation

The recommended path for this slice is:

- keep the current descriptor-driven runtime path as the product mainline
- explicitly declare `platform_runtime.json + descriptors + PlatformDescriptorLoader` as the only product truth source
- demote `plugin_load_policy.json` and CTK policy helpers to compatibility-only status
- lock that boundary with tests and documentation before attempting any broad legacy deletion

This is the lowest-risk way to stop the plugin framework, service loading, and startup semantics from drifting back into a multi-truth architecture.
