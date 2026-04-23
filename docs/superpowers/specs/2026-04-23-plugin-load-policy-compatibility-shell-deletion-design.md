# Plugin Load Policy Compatibility Shell Deletion Design

## Scope Note

- This document defines the cleanup slice after `plugin load policy compatibility residue minimization`.
- This slice does not reopen product startup truth ownership.
- This slice does not remove the CTK runtime executor itself.
- This slice deletes the final `plugin_load_policy` compatibility shell when no live repository consumer remains.
- This slice also evaluates and removes adjacent legacy helpers that belong to the same compatibility-era loading path.
- This slice does not define a broader `CTK runtime exit` roadmap.

## Implementation Links

- Plugin truth-source governance design: `docs/superpowers/specs/2026-04-21-plugin-truth-source-governance-design.md`
- Plugin legacy consumer governance design: `docs/superpowers/specs/2026-04-21-plugin-legacy-consumer-governance-design.md`
- CTK descriptor policy bridge design: `docs/superpowers/specs/2026-04-22-ctk-descriptor-policy-bridge-design.md`
- Plugin load policy compatibility residue minimization design: `docs/superpowers/specs/2026-04-23-plugin-load-policy-compatibility-residue-minimization-design.md`
- Legacy consumer inventory: `docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md`
- Decision log: `docs/superpowers/tracking/platform-migration-decision-log.md`
- Governance matrix: `docs/superpowers/tracking/platform-plugin-governance-matrix.md`
- Current project overview: `docs/current_status_and_project_overview.md`

Date: 2026-04-23  
Scope: delete the final `plugin_load_policy` compatibility shell and its adjacent dead legacy helpers  
Goal: remove the remaining repository-local `plugin_load_policy` compatibility artifacts, APIs, deployment wiring, and contract surfaces so the repository no longer preserves a second, dead plugin-loading shell beside the descriptor/runtime-governed mainline.

## 1. Design Background

The previous governance slices already established the following facts:

- product startup truth is `config/platform_runtime.json + plugins/descriptors/*.json + PlatformDescriptorLoader`
- `main.cpp` does not use `loadPluginPolicy()` or `installPluginsFromDirectory()` for product startup
- `CTKManager` runtime classification now resolves through `PlatformCtkPolicyBridge`
- `PluginLoadPolicy` has already been reduced to a metadata-only compatibility carrier
- `plugin_load_policy.json` has already been reduced to a minimal compatibility projection

That means the repository already removed runtime authority from the old load-policy chain.

However, one residue still remains in the repository model:

- `PluginLoadPolicy` source files still exist
- `CTKManager::loadPluginPolicy()` still exists
- `config/plugin_load_policy.json` and `config/plugin_load_policy_compatibility.md` still ship
- dedicated runtime and unit contracts still validate those compatibility artifacts
- adjacent directory-scan and manual load-order helpers still preserve the old compatibility-era loading story

At this point the remaining shell no longer behaves like a live compatibility boundary. It behaves like a dead shell that the repository still deploys, documents, and tests into existence.

This slice removes that shell completely instead of shrinking it again.

## 2. Hidden Dependency Assessment

This design is based on repository-local evidence gathered before writing the spec.

### 2.1 Confirmed Live Mainline Truth

Repository context confirms that:

- `main.cpp` loads `platform_runtime.json`
- `main.cpp` loads `plugins/descriptors/*.json` through `PlatformDescriptorLoader`
- `main.cpp` hands runtime config and descriptors into `CTKManager` through `setDescriptorPolicyContext(...)`
- `CTKManager` runtime classification resolves through `PlatformCtkPolicyBridge`

### 2.2 Confirmed Compatibility Shell Residue

Repository context also confirms that the following shell still exists:

- `Framework/PluginLoadPolicy.h`
- `Framework/PluginLoadPolicy.cpp`
- `CTKManager::loadPluginPolicy()`
- `config/plugin_load_policy.json`
- `config/plugin_load_policy_compatibility.md`
- `plugin_load_policy_compatibility_residue_contract_test`
- `plugin_legacy_compatibility_runtime_contract_test`

### 2.3 Hidden Consumer Scan Result

Repository-wide scans found:

- no runtime callsites of `CTKManager::loadPluginPolicy(...)` beyond its declaration and implementation
- no runtime consumers of `PluginLoadPolicy::configPath()`
- no runtime consumers of `PluginLoadPolicy::hasValidConfig()`
- no runtime consumers of `PluginLoadPolicy::policyReloaded`

The remaining references are deployment wiring, source contracts, runtime artifact contracts, and governance documentation.

### 2.4 External Invocation Policy

This slice fixes one repository-governance rule explicitly:

- repository-local dead compatibility entry points must not survive merely because an undocumented manual or out-of-repo script might still call them

If an external manual workflow is discovered later, that is treated as a post-deletion migration issue, not as justification to preserve the dead shell in-repo.

## 3. Decision Summary

The following decisions are fixed for this slice:

- The final `plugin_load_policy` compatibility shell must be deleted, not shrunk again.
- Adjacent dead legacy helpers from the same compatibility-era loading path must be deleted in the same slice when repository-local consumers are absent.
- Product truth remains descriptor/runtime-driven and must not gain a replacement legacy policy shell.
- The CTK runtime executor remains in place; this slice is not a CTK runtime exit.
- Tests, deployment, and governance docs must converge on shell deletion in the same slice.

## 4. Goals And Non-Goals

### 4.1 Goals

This slice must achieve the following goals:

1. delete the final `plugin_load_policy` compatibility shell from source, config, deployment, tests, and docs
2. delete adjacent dead legacy helpers that still imply a compatibility loading side path
3. remove runtime artifact deployment for deleted compatibility files
4. replace existence-based compatibility contracts with deletion-based governance contracts
5. keep descriptor/runtime truth as the only repository-recognized plugin loading truth

### 4.2 Non-Goals

This slice does not do the following:

- it does not remove `CTKManager` as the CTK runtime executor
- it does not redesign CTK framework startup or plugin activation ownership
- it does not define a broader de-CTK roadmap
- it does not change `PlatformCtkPolicyBridge`
- it does not change descriptor/runtime startup assembly in `main.cpp`
- it does not touch unrelated worktree files such as `medicalpro_zh_CN.ts`

## 5. Deletion Boundary

### 5.1 Compatibility Shell To Delete

The following items are deletion targets in this slice:

- `Framework/PluginLoadPolicy.h`
- `Framework/PluginLoadPolicy.cpp`
- `CTKManager::loadPluginPolicy()` declaration
- `CTKManager::loadPluginPolicy()` implementation
- `config/plugin_load_policy.json`
- `config/plugin_load_policy_compatibility.md`

### 5.2 Adjacent Legacy Helpers To Delete

The following adjacent helpers belong to the same old compatibility-era loading story and must be deleted in this slice if no repository-local consumers remain:

- `CTKManager::installPluginsFromDirectory()`
- `CTKManager::setPluginLoadOrder()`
- `CTKManager::getRecommendedLoadOrder()`
- `CTKManager::m_pluginLoadOrder`

These helpers preserve the same old mental model:

- scan a directory
- impose a manual legacy order
- treat CTK plugin installation as an alternate startup side path

Keeping them after deleting `PluginLoadPolicy` would still leave a half-dead compatibility shell in place.

### 5.3 Boundaries To Retain

The following surfaces remain explicitly in scope as retained architecture:

- `CTKManager` framework initialization
- `CTKManager` install/start execution helpers that still serve the descriptor-governed runtime
- `PlatformDescriptorLoader`
- `PlatformCtkPolicyBridge`
- `config/platform_runtime.json`
- `plugins/descriptors/*.json`
- `main.cpp` descriptor/runtime handoff chain

This slice deletes only the dead compatibility shell, not the active CTK execution layer.

## 6. Replacement Governance Model

After this slice, the repository must communicate one consistent governance picture:

- there is no longer a `plugin_load_policy` compatibility shell
- there is no shipped runtime projection note for a deleted shell
- there is no repository-recognized legacy load-policy entry point
- there is no repository-recognized directory-scan startup side path
- the only recognized plugin-loading truth remains `platform_runtime.json + plugins/descriptors/*.json + PlatformDescriptorLoader`

This is not merely a file deletion. It is a repository-authority cleanup.

## 7. Test And Contract Strategy

### 7.1 Contracts To Delete

The following tests exist only to keep the deleted shell alive and must be removed:

- `plugin_load_policy_compatibility_residue_contract_test`
- `plugin_legacy_compatibility_runtime_contract_test`
- `verify_plugin_legacy_compatibility_runtime_contract` branch inside `tests/runtime/verify_runtime_artifacts.cmake`

### 7.2 Contracts To Retain

The following tests remain important and must continue to protect the descriptor/runtime mainline:

- `plugin_truth_source_governance_contract_test`
- `plugin_legacy_consumer_governance_contract_test`
- `plugin_truth_source_runtime_contract_test`
- `platform_descriptor_runtime_layout_test`
- `runtime_artifact_layout_test`

### 7.3 Contract Semantic Flip

This slice must flip contract intent from:

- compatibility shell exists and is properly governed

to:

- compatibility shell no longer exists and must not reappear

### 7.4 Required Post-Deletion Contract Coverage

After this slice, governance/source contracts must prove at least the following:

1. `PluginLoadPolicy` no longer exists in source
2. `CTKManager` no longer exposes `loadPluginPolicy()`
3. `CTKManager` no longer exposes `installPluginsFromDirectory()`
4. `CTKManager` no longer exposes `setPluginLoadOrder()` or `getRecommendedLoadOrder()`
5. `CMakeLists.txt` no longer deploys `plugin_load_policy.json` or `plugin_load_policy_compatibility.md`
6. test registration no longer includes compatibility runtime acceptance for the deleted shell
7. repository truth remains descriptor/runtime-driven only

### 7.5 Reintroduction Rule

New contracts must make it hard to reintroduce:

- `PluginLoadPolicy`
- `plugin_load_policy.json`
- `plugin_load_policy_compatibility.md`
- `loadPluginPolicy()`
- `installPluginsFromDirectory()`
- manual legacy load-order surfaces under new names

Any future reintroduction must be treated as new technical debt, not as compatibility restoration.

## 8. Deployment Strategy

### 8.1 Deployment Deletions

This slice must remove explicit deployment of:

- `config/plugin_load_policy.json`
- `config/plugin_load_policy_compatibility.md`

### 8.2 CMake Direction

`CMakeLists.txt` currently separates:

- product config artifacts
- compatibility config artifacts

After this slice:

- `MEDICALPRO_COMPATIBILITY_CONFIG_FILES` must be removed
- `MEDICALPRO_RUNTIME_CONFIG_FILES` must contain product-mainline config artifacts only
- runtime output must no longer contain deleted compatibility files

### 8.3 Runtime Acceptance Meaning

After this slice, runtime artifact acceptance must mean:

- product-mainline runtime artifacts are present
- descriptor/runtime truth artifacts are present
- no deleted compatibility policy artifacts are expected

## 9. Documentation And Governance Write-Back

The following governance documents must be updated in the same slice:

- `docs/current_status_and_project_overview.md`
- `docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md`
- `docs/superpowers/tracking/platform-plugin-governance-matrix.md`
- `docs/superpowers/tracking/platform-migration-decision-log.md`

### 9.1 Required Documentation Shift

Those documents must stop describing:

- `PluginLoadPolicy` as a retained compatibility carrier
- `plugin_load_policy.json` as a retained compatibility projection
- `plugin_legacy_compatibility_runtime_contract_test` as a retained compatibility acceptance owner
- `CTKManager::loadPluginPolicy()` as a retained compatibility entry

They must instead describe:

- the compatibility shell was deleted
- the runtime no longer ships `plugin_load_policy` artifacts
- descriptor/runtime truth remains the only governed source
- reintroducing a second plugin-loading truth source is forbidden

### 9.2 Inventory Outcome

The inventory must move from:

- `allowed_compatibility_surface`

to a model that records:

- deleted shell
- forbidden reintroduction
- retained active runtime surfaces only

## 10. Risks And Mitigations

### 10.1 Risk: undocumented external manual workflow still calls deleted APIs

Mitigation:

- repository-local absence of consumers is the governing criterion
- external/manual callers are not sufficient reason to retain dead shell code
- any later discovery becomes a post-deletion migration task

### 10.2 Risk: half-deletion leaves another legacy side path alive

Mitigation:

- delete adjacent directory-scan and manual load-order helpers in the same slice when no repository-local consumers remain
- do not stop at deleting only `PluginLoadPolicy` files

### 10.3 Risk: docs and tests still describe deleted compatibility surfaces

Mitigation:

- require code deletion, contract deletion, deployment deletion, and documentation rewrite in the same slice
- do not accept phased semantic drift where code is deleted but governance still speaks in retention language

### 10.4 Risk: deletion is misread as CTK runtime exit

Mitigation:

- explicitly retain CTK runtime execution surfaces
- document that this slice removes compatibility governance residue, not CTK runtime itself

## 11. Acceptance Criteria

This slice is accepted only when all of the following are true:

- `PluginLoadPolicy` source files are deleted
- `CTKManager` no longer exposes `loadPluginPolicy()`
- `CTKManager` no longer exposes `installPluginsFromDirectory()`
- `CTKManager` no longer exposes `setPluginLoadOrder()` or `getRecommendedLoadOrder()`
- `config/plugin_load_policy.json` and `config/plugin_load_policy_compatibility.md` are deleted
- runtime deployment no longer copies deleted compatibility config files
- compatibility runtime acceptance for the deleted shell is removed
- repository contracts now protect shell absence rather than shell retention
- governance documents uniformly state that compatibility shell deletion is complete
- product truth remains locked to `platform_runtime.json + plugins/descriptors/*.json + PlatformDescriptorLoader`

## 12. Implementation Batches

This slice should land in four batches:

### Batch 1: contract inversion

- add or rewrite source/governance contracts so they define deleted-shell expectations
- prepare failure signals before large deletions

### Batch 2: source and config deletion

- delete `PluginLoadPolicy.*`
- delete `CTKManager::loadPluginPolicy()`
- delete adjacent legacy helpers:
  - `installPluginsFromDirectory()`
  - `setPluginLoadOrder()`
  - `getRecommendedLoadOrder()`
  - `m_pluginLoadOrder`
- delete `config/plugin_load_policy.json`
- delete `config/plugin_load_policy_compatibility.md`

### Batch 3: deployment and runtime acceptance cleanup

- remove compatibility config deployment from `CMakeLists.txt`
- remove compatibility runtime contract registration
- remove compatibility residue contract registration
- remove compatibility verification logic from runtime artifact scripts

### Batch 4: governance write-back and acceptance

- update current status, decision log, governance matrix, and inventory
- record that compatibility shell deletion is complete
- run descriptor/runtime governance and runtime acceptance suites

## 13. Final Design Summary

The repository has already removed runtime authority from the old load-policy chain. What remains now is not a meaningful compatibility boundary, but a dead shell preserved by documentation, deployment, and tests.

This slice therefore chooses complete deletion rather than further minimization.

The resulting repository model should be:

- no `PluginLoadPolicy`
- no `plugin_load_policy.json`
- no compatibility sidecar note
- no `loadPluginPolicy()` compatibility entry
- no adjacent directory-scan or manual load-order helper from the same legacy shell
- no compatibility runtime acceptance that pretends the deleted shell still matters
- one plugin-loading truth source only: `platform_runtime.json + plugins/descriptors/*.json + PlatformDescriptorLoader`

That leaves the repository in a cleaner state:

- descriptor/runtime truth remains authoritative
- CTK runtime execution remains intact
- the dead compatibility shell is gone
- future CTK cleanup can focus on the real runtime executor, not on policy residue that no longer does anything
