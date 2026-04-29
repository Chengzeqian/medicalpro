# Platform Migration Decision Log

## 2026-04-27

- Decision: switch the default product mainline to `ENABLE_CTK_PLUGIN_FRAMEWORK=OFF`.
- Rationale: CTK runtime exit is accepted only when the default Release build no longer depends on CTK runtime linkage, deployment, or startup ownership.
- Impact: the product mainline now boots through platform-owned runtime/service ports by default, while CTK runtime behavior is confined to bridge-only source paths.

- Decision: make `MEDICALPRO_DEPLOY_CTK_RUNTIME` and `MEDICALPRO_BUILD_LEGACY_CTK_PLUGINS` fatal if explicitly enabled.
- Rationale: deprecated toggles must not silently revive deleted runtime/deployment paths after acceptance.
- Impact: configuration now fails fast instead of rebuilding CTK runtime shipping or legacy plugin deployment by accident.

- Decision: delete the remaining migrated plugin activator source files instead of keeping bridge-shell stubs.
- Rationale: keeping empty CTK activator shells would preserve a false second host model and weaken the migration boundary.
- Impact: `UserManagement`, `DicomViewer`, `FourViewDisplay`, `RegistrationCore`, `Registration2D3D`, `OpticalTracking`, `InstrumentManagement`, `OpticalRegistration`, `PointRegistration`, and `BoneSegmentation` are now governed as platform-host modules only.

- Decision: harden Framework-linked unit tests by synchronizing the current `Framework.dll` into each test output directory.
- Rationale: the last false-negative acceptance failure came from a stale `tests/unit/Release/Framework.dll` shadowing the real no-CTK runtime binary and reintroducing CTK-linked behavior inside one test process.
- Impact: the acceptance suite now validates the current product/runtime binary state instead of stale local DLL residue.

## 2026-04-23

- Decision: delete the final `plugin_load_policy` compatibility shell.
- Rationale: repository-local scans show that the remaining shell is kept alive only by deployment, contracts, and documentation rather than live runtime consumers.
- Impact: `PluginLoadPolicy`, `plugin_load_policy.json`, `plugin_load_policy_compatibility.md`, and `CTKManager::loadPluginPolicy()` are removed from the repository.

- Decision: delete adjacent dead legacy helpers from the same compatibility-era loading path.
- Rationale: keeping directory-scan and manual load-order helpers would preserve a half-dead legacy loading side path after shell deletion.
- Impact: `CTKManager::installPluginsFromDirectory()`, `CTKManager::setPluginLoadOrder()`, and `CTKManager::getRecommendedLoadOrder()` are removed alongside the shell.

- Decision: remove compatibility runtime acceptance for the deleted shell.
- Rationale: repository contracts must protect shell absence rather than continue validating deleted compatibility artifacts.
- Impact: compatibility runtime and residue contracts are deleted, while descriptor/runtime truth contracts remain in place.

## 2026-04-22

- Decision: switch `CTKManager` runtime bucket classification to `PlatformPluginPolicyBridge` with explicit descriptor policy context handoff.
- Rationale: runtime classification must follow the same governed descriptor/runtime facts as product startup truth rather than internal legacy policy lookup.
- Impact: `main.cpp` now hands runtime descriptors into `CTKManager` through `setDescriptorPolicyContext(...)`, and fallback diagnostics are explicit when context is missing.

- Decision: safe mode criticality follows platform_runtime.json.core_plugin_ids.
- Rationale: safe mode should evaluate plugin criticality from the same runtime truth source that governs managed core startup.
- Impact: safe mode skip behavior now tracks descriptor/runtime governance and no longer depends on `PluginLoadPolicy` runtime criticality metadata.

## 2026-04-21

- Decision: split default product runtime artifact acceptance from compatibility runtime artifact acceptance.
- Rationale: `plugin_load_policy.json` is still a shipped compatibility artifact, but default product runtime verification must not keep implying that it is a universal runtime requirement.
- Impact: `runtime_artifact_layout_test` now verifies product-mainline runtime artifacts only, while `plugin_legacy_compatibility_runtime_contract_test` owns legacy policy artifact shipping.

- Decision: classify `CTKManager::policyForPlugin()` and `CTKManager::applyPolicyForPlugin()` as temporary internal compatibility debt.
- Rationale: those helpers still preserve current internal deferred/on-demand behavior, but they are not product-mainline truth and should not be treated as approved long-term architecture.
- Impact: the remaining legacy consumer set is now documented and test-protected without changing runtime behavior in this slice.

- Decision: treat `plugin_load_policy.json` and CTK load-policy helpers as compatibility-only metadata rather than product startup truth.
- Rationale: the product mainline already boots from `platform_runtime.json` plus descriptors, so leaving the legacy policy chain unnamed creates a false second truth source.
- Impact: future product-mainline code must not call `loadPluginPolicy()` or `installPluginsFromDirectory()` to decide startup content, while the legacy helpers remain available for compatibility scenarios and are documented by the runtime sidecar note.

- Decision: withdraw `StartupShell` from the visible product entry path and restore the in-app `MainInterfaceWidget` welcome page as the only user-facing welcome surface.
- Rationale: runtime feedback showed the visible shell-first path degraded the actual experience in two ways: it rendered shell snapshot state instead of the `CoreUiRuntimeStatusProvider` truth the user preferred, and it moved `MainInterfaceWidget` construction onto the `Enter System` click path, introducing a noticeable transition hitch.
- Impact: `main.cpp` now pre-creates `MainInterfaceWidget` directly, logout stays inside the in-app welcome flow, `MainInterfaceWidget/MainInterfaceFactory` no longer expose the retired external-shell switch in their public API, and `StartupShell` remains non-visible groundwork until a future cold-start path can preserve the original welcome experience without stale state or handoff lag.

- Decision: land cold-start UX remediation through a shell-first startup host plus `PlatformStateStore` handoff, not by pushing external state-store ownership directly into `MainInterfaceWidget`.
- Rationale: the old startup path coupled first paint to `MainInterfaceWidget` construction, but forcing pointer-owned runtime state into that legacy widget would enlarge the refactor surface and risk destabilizing existing page wiring.
- Impact: `Welcome` can appear before CTK and managed-core completion, `Enter System` remains gated by the existing Phase 1 `platformReady` truth, and the runtime state bridge now swaps from bootstrap store to main-interface store only when the user actually enters the system.

- Decision: keep the original themed `WelcomePageNew` as the only user-facing welcome surface after the shell-first rollout.
- Rationale: the cold-start shell already reuses the original welcome page, so showing the embedded main-interface welcome again after `Enter System` created duplicate welcome perception and diluted the intended startup improvement.
- Impact: startup still paints early through `StartupShell`, but shell entry now lands on `ModuleSelectionPage`, and logout returns to the shell-hosted welcome surface instead of re-showing an in-app welcome step.

- Decision: collapse shell failure recovery back into the original welcome-page CTA instead of rendering separate shell controls outside the page.
- Rationale: the extra retry/diagnostics controls made the shell-hosted welcome look like a second gray startup page even though the product requirement is to preserve the original themed welcome as the only visible welcome surface.
- Impact: `StartupShell` is now a thin host around `WelcomePageNew`, booting/ready/failure states stay on the same themed page, and failure recovery routes through the welcome primary action (`重试启动`).

- Decision: move `ensureReady()` onto the governed on-demand activation path for `RegistrationCore` and `OpticalTracking`.
- Rationale: the old `plugin id -> CTK symbolic name -> direct start` path bypassed descriptor validation, service-ready gating, and diagnostics.
- Impact: on-demand activation is now descriptor-driven and diagnosable, but Phase 1 startup readiness remains scoped to the cold-start core set.

- Decision: keep kernel startup semantics and shared plugin macros unchanged for the `UserManagement` remediation slice, but correct Phase 1 managed-core descriptor service truth where runtime evidence proves drift.
- Rationale: red-test and startup-log evidence showed `UserManagement` was already installable and startable; the real blocker was `Service ready timeout: "org.medicalpro.user_management"` because descriptor `diagnostics.required_services` did not match the actual CTK registered service class names. The same mismatch existed in `DicomViewer` and `FourViewDisplay`.
- Impact: `UserManagement` packaging remains a local fix, while `UserManagement`, `DicomViewer`, and `FourViewDisplay` now align their `required_services` and `provides.services` contracts to runtime truth so `service_ready` gating is reliable instead of guess-based.

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
- Rationale: `PlatformLifecycleTraceRecorder` now records session, phase, and plugin-step facts; `PlatformPluginLifecycleAggregator` now derives slowest plugin, blocking point, failure point, ready-path versus warmup-tail, and recovery hints; `PlatformDiagnosticsService` now reports `runtime_platform_state_mismatch` as a first-class governed problem instead of leaving it implicit.
- Impact: startup slowness and degradation should now be explainable through the governance layer in `observe_only`, `facade_mode`, and `orchestrate_core`, so future work can extend the diagnostics experience without redefining the lifecycle model again.
