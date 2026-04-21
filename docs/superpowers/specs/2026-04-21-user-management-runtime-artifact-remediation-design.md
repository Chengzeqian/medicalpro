# UserManagement Runtime Artifact Remediation Design

## Scope Note

- This document defines the next focused remediation slice after plugin-chain remediation Phase 2.
- This slice is limited to `UserManagement` runtime artifact copying, runtime layout truth, plugin discovery input, and install-chain preconditions under `build_x64/Release`.
- This slice does not expand to shared plugin-macro refactoring unless evidence proves that a local `UserManagement` fix is impossible without a minimal shared change.
- This slice does not change `PlatformStartupCoordinator` startup semantics, `ensureReady()` governed activation semantics, or the broader core-plugin set.

## Implementation Links

- Base governance design: `docs/superpowers/specs/2026-04-16-platform-kernel-governance-design.md`
- Plugin-chain remediation Phase 1 design: `docs/superpowers/specs/2026-04-20-plugin-chain-remediation-phase1-design.md`
- Plugin-chain remediation Phase 2 design: `docs/superpowers/specs/2026-04-20-plugin-chain-remediation-phase2-design.md`
- Latest completed implementation plan: `docs/superpowers/plans/2026-04-21-plugin-chain-remediation-phase2-implementation.md`
- Decision log: `docs/superpowers/tracking/platform-migration-decision-log.md`

Date: 2026-04-21  
Scope: `medicalpro` follow-up remediation for `UserManagement` runtime artifact delivery  
Goal: constrain the `UserManagement` path from source plugin to `build_x64/Release` runtime artifacts, descriptor truth, and install preconditions into one stable, verifiable, regression-safe path without expanding into a broader global plugin refactor.

## 1. Design Background

The previous two remediation slices already landed these governance changes:

- Phase 1 moved cold startup onto a descriptor-driven managed startup path.
- Phase 2 moved `RegistrationCore` and `OpticalTracking` onto governed on-demand activation.

However, the current docs and real startup logs still show one clear residual problem:

- `UserManagement` is part of the Phase 1 managed startup scope.
- Real startup still reports `Plugin handle not found for UserManagement`.
- The same run still reports `Critical plugin start failed: "UserManagement"`.

This strongly suggests that the current issue is not the overall governance semantics, but that the `UserManagement` runtime artifact chain is not being constrained by one reliable truth source.

The current codebase already states these facts:

- Descriptor truth comes from `Plugins/UserManagement/platform/plugin.json`.
- The product runtime reads bundles from `appDir/plugins` and descriptors from `appDir/plugins/descriptors`.
- The main project assumes runtime copying is handled by per-plugin CMake rules.

So this slice should not widen governance semantics again. It should first pin down the build and deployment truth for `UserManagement`.

## 2. Decision Summary

The following decisions are already confirmed for this slice:

- Only `UserManagement` is in scope.
- `build_x64/Release` is the only runtime truth directory for acceptance in this slice.
- Hard acceptance is:
  - `UserManagement` enters the installed-plugin list reliably.
  - startup logs no longer show `Plugin handle not found for UserManagement`.
  - startup logs no longer show `Critical plugin start failed: "UserManagement"`.
- This slice prioritizes build and deployment chain repair, not runtime activation semantics changes.
- This slice must add executable automated verification. Manual startup observation is not enough.
- Shared `PluginMacros.cmake` changes are allowed only as evidence-driven minimal changes, not as a proactive global refactor.

## 3. Goals And Non-Goals

### 3.1 Goals

This slice must achieve the following goals:

1. `UserManagement` plugin artifacts reliably land in `build_x64/Release/plugins`.
2. `UserManagement` descriptors reliably land in `build_x64/Release/plugins/descriptors`.
3. The descriptor `id`, descriptor `runtime.ctk_symbolic_name`, and runtime bundle truth no longer drift apart.
4. `medicalpro.exe` started from `build_x64/Release` resolves the correct local bundle file for the CTK install path.
5. Automated acceptance directly checks `UserManagement` runtime artifact and descriptor truth.
6. Documentation clearly records that this slice repairs the `UserManagement` build and deployment chain, not the wider platform startup semantics.

### 3.2 Non-Goals

This slice does not do the following:

- It does not change Phase 1 cold-start semantics in `PlatformStartupCoordinator`.
- It does not change `PlatformOnDemandActivationService` or `ensureReady()`.
- It does not pull `DicomViewer` or `FourViewDisplay` into the same remediation batch.
- It does not proactively refactor all plugin copy macros, descriptor macros, or runtime layout macros.
- It does not elevate this slice's failure classes into new platform diagnostics `reasonCode` contracts.

## 4. Target Chain

The target chain to be constrained in this slice is fixed as:

1. `Plugins/UserManagement` source plugin plus `platform/plugin.json`
2. CMake builds the `UserManagement` plugin bundle
3. The `UserManagement` bundle lands in `build_x64/Release/plugins`
4. `UserManagement.json` lands in `build_x64/Release/plugins/descriptors`
5. `main.cpp` reads runtime config from `build_x64/Release/config/platform_runtime.json` and descriptors from `plugins/descriptors`
6. The managed startup plan resolves the `UserManagement` bundle path from descriptor truth
7. CTK install receives the correct local file path
8. `UserManagement` enters the installed-plugin list and the managed startup path can continue

After this chain is constrained, the system should have only one explanation path left for `UserManagement`:

- Why should it be installed: because it is inside the managed startup scope and the descriptor is valid.
- Why does install fail: because the runtime bundle is missing, the descriptor is missing, the runtime layout is wrong, or the symbolic name truth is inconsistent.
- Why is "there seems to be a copy rule" no longer acceptable: because this slice adds automated checks against the actual runtime layout.

## 5. Root-Cause Hypotheses

Based on the current CMake rules and runtime paths, this slice should prioritize these root-cause classes:

### 5.1 Bundle Missing

- `build_x64/Release/plugins/UserManagement.dll` does not actually land in the runtime directory.
- Or the build only produces a bundle in a different location such as `build_x64/plugins`, while the product actually runs from `build_x64/Release`.

### 5.2 Descriptor Missing

- `build_x64/Release/plugins/descriptors/UserManagement.json` is missing.
- Or the descriptor is not copied together with the main runtime directory.

### 5.3 Runtime Layout Mismatch

- The plugin bundle and descriptor both exist, but not under the directory that `medicalpro.exe` actually reads from.
- In other words, "a build artifact exists" and "the runtime truth exists" are not the same fact.

### 5.4 Symbolic Name Mismatch

Any mismatch between the following can explain why descriptor truth exists while CTK still cannot resolve a handle:

- descriptor `runtime.ctk_symbolic_name`
- `Plugins/UserManagement/MANIFEST.MF`
- runtime bundle file name
- the name CTK uses for install and handle lookup

## 6. Design Approach

### 6.1 Change Boundary

This slice should prefer changes only in these files:

- `Plugins/UserManagement/CMakeLists.txt`
- `tests/runtime/verify_runtime_artifacts.cmake`
- `tests/CMakeLists.txt`
- required documentation files

These areas stay out of scope by default:

- `main.cpp`
- `Framework/Platform/Kernel/*`
- `Framework/Platform/Diagnostics/*`
- `cmake/PluginMacros.cmake`

Only if the evidence shows that a local `UserManagement` fix cannot work should `cmake/PluginMacros.cmake` receive a minimal shared fix.

### 6.2 Preferred Fix Order

The repair order for this slice is fixed as:

1. make automated verification fail accurately
2. inspect the actual `UserManagement` bundle and descriptor layout
3. if the gap is local, fix only `Plugins/UserManagement/CMakeLists.txt`
4. only if shared macros are proven to be the root cause, make the smallest possible shared change in `cmake/PluginMacros.cmake`

### 6.3 Runtime Truth Contract

This slice treats the following paths as the runtime truth for `UserManagement`:

- `build_x64/Release/plugins/UserManagement.dll`
- `build_x64/Release/plugins/descriptors/UserManagement.json`

The following fields are hard constraints:

- descriptor `id == org.medicalpro.user_management`
- descriptor `runtime.ctk_symbolic_name == UserManagement`
- `Plugins/UserManagement/MANIFEST.MF` contains `Plugin-SymbolicName: UserManagement`

If these truths do not hold, this slice should fail explicitly instead of leaving startup code to guess.

## 7. Automated Verification Strategy

### 7.1 Build Verification

The minimum build command for this slice is:

```powershell
cmake --build build_x64 --config Release --target UserManagement medicalpro
```

Purpose:

- ensure both `UserManagement` and the main executable participate in runtime artifact generation for the same Release layout

### 7.2 Runtime Artifact Verification

This slice must keep an executable runtime-artifact verification path that at minimum checks:

- `build_x64/Release/plugins/UserManagement.dll` exists
- `build_x64/Release/plugins/descriptors/UserManagement.json` exists
- descriptor `id` is correct
- descriptor `runtime.ctk_symbolic_name` is correct

The preferred implementation is to extend `tests/runtime/verify_runtime_artifacts.cmake` and keep using `runtime_artifacts_test`, but with `UserManagement` elevated from a passive existence check to an explicit primary acceptance target.

### 7.3 Real Startup Verification

Real startup verification for this slice is fixed as:

```powershell
$exe = Resolve-Path 'build_x64/Release/medicalpro.exe'
$p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru
Start-Sleep -Seconds 8
```

Acceptance:

- startup logs no longer show `Plugin handle not found for UserManagement`
- startup logs no longer show `Critical plugin start failed: "UserManagement"`

## 8. Failure Classification

This slice does not introduce new platform diagnostics contracts, but tests and docs should at least align on these failure classes:

- `user_management_bundle_missing`
  - meaning: `build_x64/Release/plugins/UserManagement.dll` does not exist
- `user_management_descriptor_missing`
  - meaning: `build_x64/Release/plugins/descriptors/UserManagement.json` does not exist
- `user_management_runtime_layout_mismatch`
  - meaning: artifacts exist, but not under the directory the product actually reads
- `user_management_symbolic_name_mismatch`
  - meaning: descriptor, manifest, bundle name, or CTK expected symbolic name do not match

These names do not need to become official platform `reasonCode` values yet, but test failures, acceptance notes, and documentation should use the same vocabulary.

## 9. Documentation Write-Back

Implementation of this slice should update:

- `docs/current_status_and_project_overview.md`
- `docs/superpowers/tracking/platform-migration-decision-log.md`
- the corresponding implementation plan

Write-back must at least record:

- this slice only fixes `UserManagement`
- `build_x64/Release` is the only runtime acceptance directory
- automated acceptance covers `UserManagement` bundle and descriptor truth
- this slice does not expand into shared plugin-macro governance or other core plugins

## 10. Acceptance Summary

The completion standard for this slice is fixed as one sentence:

> `UserManagement` has a stable, verifiable runtime artifact layout under `build_x64/Release`, and real startup no longer reports `Plugin handle not found` or `Critical plugin start failed: "UserManagement"`.

If this slice must choose between these two priorities, the order is fixed:

1. verifiable structural truth
2. broader plugin-chain unification later

That keeps the project from drifting back into the same failure mode: expanding architecture work first, then later rediscovering that a core plugin never reliably entered the runtime directory in the first place.
