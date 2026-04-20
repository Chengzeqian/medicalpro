# Platform Migration Decision Log

## 2026-04-20

- Decision: switch the product default runtime mode to `facade_mode` for plugin-chain remediation Phase 1.
- Rationale: `observe_only` can explain the current plugin chain but cannot stabilize framework init, managed install, core start, and service-ready gating.
- Impact: the startup truth source is now `platform_runtime.json + descriptor-driven managed startup plan`, while `plugin_load_policy.json` remains compatibility-only.

- Decision: treat `UserManagement`, `DicomViewer`, and `FourViewDisplay` as the only Phase 1 managed startup scope.
- Rationale: these three plugins define the minimum stable core path without forcing deferred or on-demand registration workflows into the first remediation slice.
- Impact: unmanaged plugins no longer block `platform ready`, and diagnostics can report them as excluded rather than startup failures.

- Decision: keep warmup outside the blocking Phase 1 ready path.
- Rationale: the old main-thread hard-coded warmup path mixed readiness with plugin-specific tail work and made startup slowness harder to explain.
- Impact: `ready` now stops at dependency satisfaction, service registration, and lightweight health checks, while warmup is routed through `PlatformWarmupCoordinator`.

## 2026-04-16

- Decision: keep CTK for phase 1 and add a platform-governance layer around it.
- Rationale: the immediate problems are startup-chain disorder, missing diagnostics, and UI direct CTK coupling.
- Impact: rollout proceeds in three modes: `observe_only -> facade_mode -> orchestrate_core`.

## 2026-04-17

- Decision: close the startup-governance phase without hiding the remaining UI direct-CTK calls.
- Rationale: startup routing and runtime descriptor governance are now verified, but UI decoupling is a separate cleanup track and should stay explicit.
- Impact: the next implementation slice should target `UI/MainInterfaceWidget.cpp`, `UI/NewPages/LoginPage.cpp`, `UI/NewPages/NavigationPage.cpp`, and `UI/NewPages/MainWindow.cpp` for facade/provider migration.

## 2026-04-17

- Decision: keep Task 10 Step 3 open after rerunning build, unit, runtime-layout, and CTK-scan acceptance in the worktree.
- Rationale: identity-flow direct user-service access has been removed from `UI/MainInterfaceWidget.cpp`, `UI/NewPages/LoginPage.cpp`, and `UI/NewPages/MainWindow.cpp`, but the scan still reported `CTKManager::instance()` runtime-status access in `UI/MainInterfaceWidget.cpp` and direct service lookups in `UI/NewPages/NavigationPage.cpp`.
- Impact: Task 10 stays open until MainInterface runtime-status provider extraction and NavigationPage service-access migration are complete.

## 2026-04-17

- Decision: close Task 10 technical decoupling after extracting `CoreUiRuntimeStatusProvider` and `NavigationPageServiceAccess`.
- Rationale: `UI/MainInterfaceWidget.cpp` no longer reads runtime state from `CTKManager` directly, `UI/NewPages/NavigationPage.cpp` no longer performs direct `getService<...>()` lookups, and the worktree now passes build, unit, runtime-layout, bridge, and CTK-scan acceptance.
- Impact: Task 10 is complete in this worktree, so the next step can move to commit/merge handling or the next platform-governance slice instead of more UI CTK cleanup.

## 2026-04-17

- Decision: accept the startup-performance and plugin-lifecycle diagnostics foundation plus the current implementation-plan subset as the Task 5 baseline for this worktree.
- Rationale: the accepted scope is the lifecycle-event-based diagnostics foundation, `StartupTrace` timeline semantics, the `ready-path` / `warmup-tail` split, and the currently implemented diagnostics page `summary + problems + timeline + plugin lifecycle` subset. The full field matrix in `docs/superpowers/specs/2026-04-17-startup-performance-and-plugin-lifecycle-diagnostics-design.md` is still a forward target, not the already-landed Task 5 contract.
- Impact: follow-up work should treat the current rollout as a stable subset baseline, and any expansion toward the full 2026-04-17 diagnostics page matrix must be tracked as a later implementation slice instead of being implied as already accepted.

## 2026-04-20

- Decision: accept the full diagnostics page matrix as landed on top of the previously accepted lifecycle-event-based diagnostics foundation.
- Rationale: the page now surfaces all required summary, plugin lifecycle, timeline, and problem-list fields from the 2026-04-17 diagnostics design without reintroducing UI direct-CTK access.
- Impact: the previous `implementation-plan subset only` wording is now historical rollout context instead of the current functional limitation in this worktree.

## 2026-04-20

- Decision: accept the startup lifecycle diagnostics infrastructure itself as landed, not only the diagnostics page presentation layer.
- Rationale: `PlatformLifecycleTraceRecorder` now records session, phase, and plugin-step facts; `PlatformPluginLifecycleAggregator` now derives slowest plugin, blocking point, failure point, ready-path versus warmup-tail, and recovery hints; `PlatformDiagnosticsService` now reports `ctk_platform_state_mismatch` as a first-class governed problem instead of leaving it implicit.
- Impact: startup slowness and degradation should now be explainable through the governance layer in `observe_only`, `facade_mode`, and `orchestrate_core`, so future work can extend the diagnostics experience without redefining the lifecycle model again.
