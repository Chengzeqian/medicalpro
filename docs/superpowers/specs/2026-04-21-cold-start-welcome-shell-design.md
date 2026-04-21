# Cold Start Welcome Shell Design

## Scope Note

- This document defines the next startup-remediation slice after Phase 2 on-demand governance and the `UserManagement` runtime-artifact remediation slice.
- This slice targets perceived cold-start availability, not raw end-to-end platform startup time.
- The user-facing acceptance target for this slice is: `Welcome` appears quickly, while platform bootstrap continues in the background.
- This slice keeps the existing Phase 1 managed startup scope and `platformReady` semantics.
- This slice does not widen the managed startup scope, does not redefine `ready`, and does not turn into a broad UI rewrite.

## Implementation Links

- Base governance design: `docs/superpowers/specs/2026-04-16-platform-kernel-governance-design.md`
- Startup diagnostics design: `docs/superpowers/specs/2026-04-17-startup-performance-and-plugin-lifecycle-diagnostics-design.md`
- Plugin-chain remediation Phase 1 design: `docs/superpowers/specs/2026-04-20-plugin-chain-remediation-phase1-design.md`
- Plugin-chain remediation Phase 2 design: `docs/superpowers/specs/2026-04-20-plugin-chain-remediation-phase2-design.md`
- UserManagement remediation design: `docs/superpowers/specs/2026-04-21-user-management-runtime-artifact-remediation-design.md`
- Decision log: `docs/superpowers/tracking/platform-migration-decision-log.md`

Date: 2026-04-21  
Scope: `medicalpro` cold-start UX optimization through a lightweight startup shell and delayed main-interface creation  
Goal: make `Welcome` visible before CTK and Phase 1 managed-core bootstrap complete, keep `Enter System` disabled until Phase 1 `platformReady`, and surface failure, retry, and diagnostics directly on the startup shell.

## 1. Design Background

The current cold-start architecture still couples first paint to main-interface construction:

- `main.cpp` constructs `MainInterfaceWidget` before background startup begins.
- `WelcomePage` is currently owned by `MainInterfaceWidget`, so showing the welcome experience still requires building the full main UI container.
- The platform governance layer already provides a stable background startup truth:
  - `StartupOrchestrator`
  - `PlatformStartupCoordinator`
  - `PlatformLifecycleTraceRecorder`
  - `PlatformDiagnosticsService`
  - Phase 1 managed startup scope restricted to `UserManagement`, `DicomViewer`, and `FourViewDisplay`
- The recent `UserManagement` remediation proved that Phase 1 managed startup truth is now stabilizing around real descriptor/runtime facts.

That means the next bottleneck is not "make startup semantics valid again", but "stop forcing the user to wait for the heavy main UI before the welcome surface can appear".

This slice therefore optimizes perceived cold-start availability by splitting:

1. lightweight first paint
2. governed background bootstrap
3. deferred main-interface construction

## 2. Decision Summary

The following decisions are fixed for this slice:

- Acceptance is measured by `Welcome / first screen visible quickly`, not by `Startup complete`.
- `Welcome` is allowed to appear before CTK framework init and managed-core readiness finish.
- `Enter System` stays disabled until the current Phase 1 managed startup scope reaches `platformReady`.
- `platformReady` keeps its current meaning: the Phase 1 managed startup scope is ready under the existing governance rules.
- `ModuleSelection`, `Management`, `Dashboard`, and other business pages remain unreachable before `platformReady`.
- If background startup fails, `Welcome` stays visible, `Enter System` remains disabled, and the shell must surface:
  - failure reason
  - recovery hint
  - `Retry Startup`
  - `View Diagnostics`
- The preferred architecture is a new lightweight `StartupShell / WelcomeHost`, not further stretching `MainInterfaceWidget` as the cold-start root.

## 3. Goals And Non-Goals

### 3.1 Goals

This slice must achieve the following goals:

1. `Welcome` can render without waiting for `MainInterfaceWidget` construction.
2. CTK initialization and Phase 1 managed-core bootstrap continue in the background after the startup shell appears.
3. `Enter System` is disabled during bootstrap and becomes enabled only after Phase 1 `platformReady`.
4. The startup shell can explain `booting`, `ready`, and `failed` states in product language rather than raw backend logs.
5. Startup failure can be handled in-place through retry and diagnostics, without dropping the user into half-ready business pages.
6. `MainInterfaceWidget` is created lazily only after readiness and only when the user actually enters the system.
7. Existing governance truth remains the only runtime truth source for readiness and failure explanation.

### 3.2 Non-Goals

This slice does not do the following:

- It does not redefine `platformReady`.
- It does not change the Phase 1 managed startup scope.
- It does not widen startup governance to all deferred or on-demand plugins.
- It does not rewrite `PlatformStartupCoordinator`, `PlatformOnDemandActivationService`, or `PlatformDiagnosticsService` semantics.
- It does not turn `WelcomePage` into a general-purpose diagnostics console.
- It does not refactor `ModuleSelection`, `Management`, `Dashboard`, or other business pages as part of the cold-start shell work.

## 4. Target Architecture

### 4.1 High-Level Structure

The target runtime flow becomes:

1. `main.cpp` creates the application and loads minimal startup configuration.
2. `main.cpp` creates a lightweight `StartupShell`.
3. `StartupShell` hosts `WelcomePage` and shows immediately.
4. `StartupBootstrapController` starts governed background bootstrap:
   - CTK framework initialization
   - managed plugin installation
   - managed core activation
   - Phase 1 service-ready gating
5. `StartupBootstrapController` emits shell snapshot updates.
6. `WelcomePage` reacts to shell state:
   - `booting`
   - `ready`
   - `failed`
7. Only after readiness, and only when the user clicks `Enter System`, the app constructs `MainInterfaceWidget`.
8. After successful handoff, `StartupShell` closes or hides, and `MainInterfaceWidget` becomes the main runtime window.

### 4.2 New Runtime Units

#### `StartupShell`

A lightweight root window responsible only for:

- hosting `WelcomePage`
- presenting startup state
- exposing `Enter System`, `Retry Startup`, and `View Diagnostics`
- staying alive until either:
  - the user enters the system successfully
  - the user exits the app

It must not own heavy business-page trees or broad runtime adapters.

#### `StartupBootstrapController`

A dedicated controller responsible for:

- owning the current bootstrap session state
- starting governed background bootstrap
- publishing shell-facing startup snapshots
- deciding when Phase 1 `platformReady` has been reached
- surfacing failure reasons and recovery hints
- handling controlled retry
- handing off the prepared runtime context to main-UI creation

This controller becomes the bridge between governance truth and welcome-shell UX.

#### `MainInterfaceFactory` or equivalent assembly function

A narrow assembly boundary responsible for:

- constructing `MainInterfaceWidget`
- wiring the existing runtime context into it
- preserving the existing state-store, diagnostics, lifecycle-recorder, and navigation service contracts

It must not own cold-start orchestration logic.

## 5. Startup Shell State Model

The startup shell uses a minimal shell state machine:

- `booting`
- `ready`
- `failed`

### 5.1 `booting`

While in `booting`:

- `Welcome` is visible
- `Enter System` is disabled
- the page shows a current stage summary such as:
  - `CTK framework initialization`
  - `Plugin installation`
  - `Critical plugin activation`
- `Exit` remains available
- `View Diagnostics` may be available as a secondary action

### 5.2 `ready`

`ready` means:

- the current Phase 1 managed startup scope satisfies the existing `platformReady` contract
- the shell can safely let the user enter the business UI

While in `ready`:

- `Enter System` becomes enabled
- the app does not auto-navigate
- the user still explicitly chooses to enter
- the first `Enter System` click triggers lazy main-interface construction

### 5.3 `failed`

While in `failed`:

- `Enter System` remains disabled
- `Welcome` stays visible
- the page shows:
  - a user-facing failure reason
  - recovery hints
  - `Retry Startup`
  - `View Diagnostics`

The system must not bypass this state by allowing the user into pages that depend on unfinished managed-core readiness.

## 6. Welcome Page Contract

`WelcomePage` remains the product's first-screen UI, but its runtime-status contract changes.

Instead of relying on `MainInterfaceWidget`-owned runtime providers, the startup shell feeds it a dedicated shell snapshot that answers only:

1. what startup state the product is in
2. what stage is currently blocking readiness
3. whether entering the system is allowed
4. if startup failed, why and what to do next

The welcome surface should present three product-facing status cards:

- platform framework
  - `not started / booting / ready / failed`
- Phase 1 managed scope
  - `waiting / activating / ready / failed`
- runtime resources
  - runtime directories, descriptors, and other lightweight prerequisites

The welcome surface should explain "why can't I enter yet?" rather than dumping raw plugin-framework details.

## 7. Handoff And Runtime-Context Ownership

The ownership rule for this slice is:

- `StartupBootstrapController` owns bootstrap-time governance context
- `MainInterfaceWidget` consumes that context after readiness

This avoids the current coupling where `MainInterfaceWidget` both hosts `Welcome` and acts as a prerequisite for startup-state consumption.

The handoff contract must preserve:

- `PlatformStateStore`
- `PlatformLifecycleTraceRecorder`
- `PlatformDiagnosticsService` input truth
- navigation/on-demand activation ports
- the managed-scope descriptor set

No second truth source may be introduced during handoff.

## 8. Retry And Diagnostics Semantics

### 8.1 Retry

This slice includes a real retry entry, not a cosmetic shell action.

The controlled retry model is:

- abort or finish the previous bootstrap session
- stop and clean framework/plugin state through existing CTK stop facilities when needed
- create a fresh bootstrap session boundary
- clear shell-facing transient state
- rerun the governed startup pipeline

The design assumes retry remains inside the same process, but it must use a fresh bootstrap session instead of calling "start again" on stale state.

### 8.2 Diagnostics

The startup shell does not construct the full in-app diagnostics page before readiness.

Instead, it exposes a lightweight diagnostics surface backed by the existing diagnostics snapshot:

- summary
- current blocking point
- failure point
- problems
- recovery hints

After `MainInterfaceWidget` is created, the full diagnostics page remains available through the normal in-app path.

## 9. Verification Strategy

This slice must add executable acceptance that verifies shell-first cold start behavior, not only traditional `Startup complete`.

Minimum acceptance targets:

1. `Welcome` becomes visible before `Startup complete`.
2. `Enter System` is disabled before Phase 1 `platformReady`.
3. `Enter System` becomes enabled after Phase 1 `platformReady`.
4. `MainInterfaceWidget` is not created during the initial cold-start shell paint path.
5. Startup failure keeps the shell visible and exposes:
   - reason
   - retry
   - diagnostics
6. Retry can start a fresh shell bootstrap session without leaving stale ready/failure state behind.

Recommended evidence paths:

- startup-shell unit tests for shell state transitions
- startup-bootstrap controller tests for:
  - `booting -> ready`
  - `booting -> failed`
  - retry from `failed`
- runtime smoke test proving:
  - shell visible before `Startup complete`
  - enter button locked until readiness
- documentation write-back in current status and decision log

## 10. Risks And Mitigations

### 10.1 Runtime-context ownership drift

Risk:
- splitting cold-start shell from main UI can create duplicate or drifting state ownership

Mitigation:
- bootstrap controller owns startup truth
- main UI only receives handed-off state after readiness

### 10.2 Heavy delayed main-interface creation

Risk:
- the user may gain a fast welcome screen but still hit a large pause after clicking `Enter System`

Mitigation:
- treat first-time main-interface construction as part of this slice’s observed cold-start UX
- do not declare success if the wait merely moves from “app launch” to “first enter click”

### 10.3 Dirty retry state

Risk:
- retrying on partially initialized CTK/plugin state can yield false greens or compound failures

Mitigation:
- retry must create a fresh bootstrap session boundary and reuse existing framework stop/cleanup facilities when required

### 10.4 User confusion during booting

Risk:
- a fast shell with a disabled enter button can feel worse if the UI does not explain why it is locked

Mitigation:
- always show current stage summary and recovery-oriented language on the welcome shell

## 11. Rollout Strategy

This slice should land in four batches:

### Batch 1: shell-first startup architecture

- add `StartupShell`
- add `StartupBootstrapController`
- move cold-start root-window responsibility out of `MainInterfaceWidget`

### Batch 2: welcome-shell state contract

- feed `WelcomePage` from shell bootstrap snapshot
- implement `booting / ready / failed`
- lock `Enter System` until readiness

### Batch 3: lazy main-interface handoff

- lazily create `MainInterfaceWidget` after readiness
- preserve state-store and diagnostics handoff
- recover to shell failure state if main-interface creation fails

### Batch 4: acceptance and governance write-back

- add cold-start shell tests and smoke coverage
- write back current status, decision log, design links, and implementation plan

## 12. Final Recommendation

The recommended implementation path for this slice is:

- introduce a lightweight `StartupShell / WelcomeHost`
- move governed bootstrap behind that shell
- keep Phase 1 managed-scope readiness as the only enter gate
- keep failure and retry on the shell
- delay `MainInterfaceWidget` creation until the user enters after readiness

This is the smallest architecture change that directly matches the accepted product goal:

> let the user see `Welcome` quickly, without loosening platform readiness truth or letting unfinished startup leak into the business flow.
