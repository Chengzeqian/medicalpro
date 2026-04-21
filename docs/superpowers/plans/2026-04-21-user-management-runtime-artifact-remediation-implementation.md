# UserManagement Runtime Artifact Remediation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** make `UserManagement` produce a stable, verifiable runtime artifact layout under `build_x64/Release` and eliminate the current startup failures `Plugin handle not found for UserManagement` and `Critical plugin start failed: "UserManagement"`.

**Architecture:** keep the fix local to the `UserManagement` build and runtime-verification path first. Add a dedicated runtime contract test plus a startup smoke test, then normalize `UserManagement` packaging to match the known-good core plugin pattern, and only after that write back docs and acceptance results.

**Tech Stack:** CMake, CTest, PowerShell, Qt 6, CTK plugin framework, existing runtime verification scripts under `tests/runtime`

---

## Files And Responsibilities

- Modify: `tests/CMakeLists.txt`
  - Register a dedicated `user_management_runtime_artifact_contract_test`
  - Register a dedicated Windows `user_management_startup_smoke_test`
- Modify: `tests/runtime/verify_runtime_artifacts.cmake`
  - Add strict `UserManagement` runtime layout and descriptor contract checks
- Create: `tests/runtime/verify_user_management_startup.ps1`
  - Launch `build_x64/Release/medicalpro.exe`, capture output, and fail on the known `UserManagement` startup errors
- Modify: `Plugins/UserManagement/CMakeLists.txt`
  - Normalize `UserManagement` packaging to the same `SOURCES + HEADERS + RESOURCES` pattern used by working core plugins
  - Copy a plugin-specific runtime manifest sidecar for inspection and verification
- Modify: `docs/current_status_and_project_overview.md`
  - Record the new `UserManagement` runtime artifact remediation acceptance
- Modify: `docs/superpowers/tracking/platform-migration-decision-log.md`
  - Record the decision to fix `UserManagement` locally before widening to shared plugin macro governance
- Modify: `docs/superpowers/plans/2026-04-21-user-management-runtime-artifact-remediation-implementation.md`
  - Mark plan status and acceptance once implementation is complete

### Task 1: Add Red Runtime Contract And Startup Smoke Tests

**Files:**
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/runtime/verify_runtime_artifacts.cmake`
- Create: `tests/runtime/verify_user_management_startup.ps1`

- [ ] **Step 1: Add a dedicated runtime contract CTest entry for `UserManagement`**

```cmake
# tests/CMakeLists.txt
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)

add_subdirectory(unit)

if(TARGET medicalpro AND TARGET UserManagement AND TARGET DicomViewer AND TARGET FourViewDisplay)
    add_test(
        NAME runtime_artifact_layout_test
        COMMAND ${CMAKE_COMMAND}
            -Druntime_dir=$<TARGET_FILE_DIR:medicalpro>
            -Duser_management_plugin=$<TARGET_FILE:UserManagement>
            -Ddicom_viewer_plugin=$<TARGET_FILE:DicomViewer>
            -Dfour_view_display_plugin=$<TARGET_FILE:FourViewDisplay>
            -Dmeshgpu_runtime_dll=$<TARGET_FILE_DIR:medicalpro>/MeshGPULib.dll
            -Dplugin_policy_file=$<TARGET_FILE_DIR:medicalpro>/config/plugin_load_policy.json
            -Ddata_dir=$<TARGET_FILE_DIR:medicalpro>/data
            -P ${CMAKE_CURRENT_SOURCE_DIR}/runtime/verify_runtime_artifacts.cmake
    )
endif()

if(TARGET medicalpro)
    add_test(
        NAME platform_descriptor_runtime_layout_test
        COMMAND ${CMAKE_COMMAND}
            -Druntime_dir=$<TARGET_FILE_DIR:medicalpro>
            -Drequire_platform_descriptors=ON
            -P ${CMAKE_CURRENT_SOURCE_DIR}/runtime/verify_runtime_artifacts.cmake
    )
endif()

if(TARGET medicalpro AND TARGET UserManagement)
    add_test(
        NAME user_management_runtime_artifact_contract_test
        COMMAND ${CMAKE_COMMAND}
            -Druntime_dir=$<TARGET_FILE_DIR:medicalpro>
            -Duser_management_plugin=$<TARGET_FILE:UserManagement>
            -Dverify_user_management_runtime_contract=ON
            -P ${CMAKE_CURRENT_SOURCE_DIR}/runtime/verify_runtime_artifacts.cmake
    )

    if(WIN32)
        find_program(POWERSHELL_EXECUTABLE NAMES pwsh powershell REQUIRED)
        add_test(
            NAME user_management_startup_smoke_test
            COMMAND ${POWERSHELL_EXECUTABLE}
                -NoProfile
                -ExecutionPolicy Bypass
                -File ${CMAKE_CURRENT_SOURCE_DIR}/runtime/verify_user_management_startup.ps1
                -ExePath $<TARGET_FILE:medicalpro>
                -WorkingDirectory $<TARGET_FILE_DIR:medicalpro>
        )
    endif()
endif()
```

- [ ] **Step 2: Extend the runtime artifact script with a strict `UserManagement` contract**

```cmake
# tests/runtime/verify_runtime_artifacts.cmake
cmake_minimum_required(VERSION 3.16)

function(append_missing_artifact path_value)
    if(NOT EXISTS "${path_value}")
        list(APPEND missing_artifacts "${path_value}")
        set(missing_artifacts "${missing_artifacts}" PARENT_SCOPE)
    endif()
endfunction()

function(require_json_string json_file expected_value failure_code)
    set(path_segments ${ARGN})
    file(READ "${json_file}" json_text)
    string(JSON actual_value ERROR_VARIABLE json_error GET "${json_text}" ${path_segments})
    if(json_error)
        message(FATAL_ERROR "${failure_code}: failed to read ${path_segments} from ${json_file}: ${json_error}")
    endif()
    if(NOT actual_value STREQUAL expected_value)
        message(FATAL_ERROR "${failure_code}: ${json_file} expected ${path_segments}=${expected_value} but got ${actual_value}")
    endif()
endfunction()

function(require_manifest_symbolic_name manifest_file expected_value failure_code)
    file(STRINGS "${manifest_file}" manifest_lines REGEX "^Plugin-SymbolicName:")
    if(NOT manifest_lines)
        message(FATAL_ERROR "${failure_code}: ${manifest_file} is missing Plugin-SymbolicName")
    endif()

    list(GET manifest_lines 0 manifest_line)
    string(REPLACE "Plugin-SymbolicName:" "" actual_value "${manifest_line}")
    string(STRIP "${actual_value}" actual_value)

    if(NOT actual_value STREQUAL expected_value)
        message(FATAL_ERROR "${failure_code}: ${manifest_file} expected Plugin-SymbolicName=${expected_value} but got ${actual_value}")
    endif()
endfunction()

set(required_files
    "${user_management_plugin}"
    "${dicom_viewer_plugin}"
    "${four_view_display_plugin}"
    "${meshgpu_runtime_dll}"
    "${plugin_policy_file}"
)

set(missing_artifacts)
foreach(required_file IN LISTS required_files)
    if(required_file AND NOT EXISTS "${required_file}")
        list(APPEND missing_artifacts "${required_file}")
    endif()
endforeach()

if(data_dir AND (NOT EXISTS "${data_dir}" OR NOT IS_DIRECTORY "${data_dir}"))
    list(APPEND missing_artifacts "${data_dir}")
endif()

if(require_platform_descriptors)
    set(platform_descriptor_files
        "${runtime_dir}/plugins/descriptors/UserManagement.json"
        "${runtime_dir}/plugins/descriptors/DicomViewer.json"
        "${runtime_dir}/plugins/descriptors/FourViewDisplay.json"
        "${runtime_dir}/plugins/descriptors/RegistrationCore.json"
        "${runtime_dir}/plugins/descriptors/OpticalTracking.json"
    )

    foreach(descriptor_file IN LISTS platform_descriptor_files)
        if(NOT EXISTS "${descriptor_file}")
            list(APPEND missing_artifacts "${descriptor_file}")
        endif()
    endforeach()
endif()

if(verify_user_management_runtime_contract)
    set(user_management_runtime_bundle "${runtime_dir}/plugins/UserManagement.dll")
    set(user_management_runtime_descriptor "${runtime_dir}/plugins/descriptors/UserManagement.json")
    set(user_management_runtime_manifest "${runtime_dir}/plugins/UserManagement.manifest")

    append_missing_artifact("${user_management_runtime_bundle}")
    append_missing_artifact("${user_management_runtime_descriptor}")
    append_missing_artifact("${user_management_runtime_manifest}")

    if(missing_artifacts)
        string(JOIN "\n - " missing_report ${missing_artifacts})
        message(FATAL_ERROR "user_management_runtime_layout_mismatch:\n - ${missing_report}")
    endif()

    get_filename_component(runtime_bundle_base "${user_management_runtime_bundle}" NAME_WE)
    if(NOT runtime_bundle_base STREQUAL "UserManagement")
        message(FATAL_ERROR "user_management_runtime_layout_mismatch: expected runtime bundle base name UserManagement but got ${runtime_bundle_base}")
    endif()

    require_json_string(
        "${user_management_runtime_descriptor}"
        "org.medicalpro.user_management"
        "user_management_descriptor_missing"
        id
    )

    require_json_string(
        "${user_management_runtime_descriptor}"
        "UserManagement"
        "user_management_symbolic_name_mismatch"
        runtime ctk_symbolic_name
    )

    require_manifest_symbolic_name(
        "${user_management_runtime_manifest}"
        "UserManagement"
        "user_management_symbolic_name_mismatch"
    )
endif()

if(missing_artifacts)
    string(JOIN "\n - " missing_report ${missing_artifacts})
    message(FATAL_ERROR "Runtime artifacts are missing:\n - ${missing_report}")
endif()

message(STATUS "Runtime artifacts are available in ${runtime_dir}")
```

- [ ] **Step 3: Add a dedicated startup smoke script that captures the current `UserManagement` failure**

```powershell
# tests/runtime/verify_user_management_startup.ps1
param(
    [Parameter(Mandatory = $true)]
    [string]$ExePath,

    [Parameter(Mandatory = $true)]
    [string]$WorkingDirectory
)

$stdoutPath = Join-Path $WorkingDirectory 'user_management_startup_stdout.log'
$stderrPath = Join-Path $WorkingDirectory 'user_management_startup_stderr.log'

Remove-Item $stdoutPath, $stderrPath -Force -ErrorAction SilentlyContinue

$process = Start-Process `
    -FilePath $ExePath `
    -WorkingDirectory $WorkingDirectory `
    -RedirectStandardOutput $stdoutPath `
    -RedirectStandardError $stderrPath `
    -PassThru

Start-Sleep -Seconds 8

if (-not $process.HasExited) {
    Stop-Process -Id $process.Id -Force
    Wait-Process -Id $process.Id -ErrorAction SilentlyContinue
}

$combinedOutput = ''

if (Test-Path $stdoutPath) {
    $combinedOutput += [System.IO.File]::ReadAllText($stdoutPath)
}

if (Test-Path $stderrPath) {
    $combinedOutput += [Environment]::NewLine
    $combinedOutput += [System.IO.File]::ReadAllText($stderrPath)
}

if ($combinedOutput -match 'Plugin handle not found for UserManagement') {
    throw 'user_management_startup_smoke_failed: Plugin handle not found for UserManagement'
}

if ($combinedOutput -match 'Critical plugin start failed:\s+"?UserManagement"?') {
    throw 'user_management_startup_smoke_failed: Critical plugin start failed for UserManagement'
}

Write-Host 'UserManagement startup smoke passed'
```

- [ ] **Step 4: Run the new runtime tests and verify they fail before the local fix**

Run:

```powershell
cmake --build build_x64 --config Release --target UserManagement medicalpro
ctest --test-dir build_x64 -C Release -R "user_management_runtime_artifact_contract_test|user_management_startup_smoke_test" --output-on-failure
```

Expected:

- `user_management_runtime_artifact_contract_test` FAILS with `user_management_runtime_layout_mismatch` because `build_x64/Release/plugins/UserManagement.manifest` does not exist yet
- `user_management_startup_smoke_test` FAILS with either:
  - `user_management_startup_smoke_failed: Plugin handle not found for UserManagement`
  - or `user_management_startup_smoke_failed: Critical plugin start failed for UserManagement`

### Task 2: Normalize Local `UserManagement` Packaging And Runtime Inspection Artifacts

**Files:**
- Modify: `Plugins/UserManagement/CMakeLists.txt`

- [ ] **Step 1: Normalize `UserManagement` to the same packaging shape as the working core plugins**

```cmake
# Plugins/UserManagement/CMakeLists.txt
# ============================================================================
# UserManagement Plugin Configuration (Requirement 1.2, 1.5)
# ============================================================================
cmake_minimum_required(VERSION 3.16)

project(UserManagement VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set encoding to UTF-8
if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets Sql)

if(CTK_FOUND)
    set(PLUGIN_SOURCES
        UserManagementActivator.cpp
        UserManagementServiceImpl.cpp
        UserDataStructures.cpp
    )

    set(PLUGIN_HEADERS
        UserManagementActivator.h
        UserManagementService.h
        UserManagementServiceImpl.h
        UserDataStructures.h
    )

    set(PLUGIN_RESOURCES
        UserManagement.qrc
    )

    add_medical_plugin(UserManagement
        SOURCES ${PLUGIN_SOURCES}
        HEADERS ${PLUGIN_HEADERS}
        RESOURCES ${PLUGIN_RESOURCES}
        DEPENDENCIES
            Qt${QT_VERSION_MAJOR}::Sql
    )

    add_custom_command(TARGET UserManagement POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/MANIFEST.MF"
            "$<TARGET_FILE_DIR:UserManagement>/UserManagement.manifest"
        COMMENT "Copying UserManagement manifest sidecar for runtime contract verification"
        VERBATIM
    )

    message(STATUS "UserManagement plugin configured as MODULE library")
else()
    message(WARNING "CTK Plugin Framework not available - UserManagement will not be built")
endif()
```

- [ ] **Step 2: Rebuild `UserManagement` and `medicalpro` with the local packaging fix**

Run:

```powershell
cmake --build build_x64 --config Release --target UserManagement medicalpro
```

Expected:

- `UserManagement` builds successfully
- `build_x64/Release/plugins/UserManagement.dll` exists
- `build_x64/Release/plugins/UserManagement.manifest` exists

- [ ] **Step 3: Rerun the runtime contract and startup smoke tests**

Run:

```powershell
ctest --test-dir build_x64 -C Release -R "user_management_runtime_artifact_contract_test|user_management_startup_smoke_test|platform_descriptor_runtime_layout_test" --output-on-failure
```

Expected:

- `user_management_runtime_artifact_contract_test` PASS
- `user_management_startup_smoke_test` PASS
- `platform_descriptor_runtime_layout_test` PASS

- [ ] **Step 4: Commit the local `UserManagement` packaging repair**

```powershell
git add tests/CMakeLists.txt tests/runtime/verify_runtime_artifacts.cmake tests/runtime/verify_user_management_startup.ps1 Plugins/UserManagement/CMakeLists.txt
git commit -m "fix: stabilize user management runtime artifacts"
```

### Task 3: Write Back Docs And Rerun Full Acceptance

**Files:**
- Modify: `docs/current_status_and_project_overview.md`
- Modify: `docs/superpowers/tracking/platform-migration-decision-log.md`
- Modify: `docs/superpowers/plans/2026-04-21-user-management-runtime-artifact-remediation-implementation.md`

- [ ] **Step 1: Write the current-status acceptance entry**

```md
<!-- docs/current_status_and_project_overview.md -->
### 2026-04-21 UserManagement Runtime Artifact Remediation Acceptance

- `UserManagement` now ships a stable runtime bundle layout under `build_x64/Release/plugins`.
- `UserManagement` descriptor truth remains available under `build_x64/Release/plugins/descriptors/UserManagement.json`.
- `UserManagement.manifest` is now copied next to the runtime bundle as an inspection artifact for contract verification.
- Runtime acceptance now includes a dedicated `user_management_runtime_artifact_contract_test`.
- Startup acceptance now includes a dedicated `user_management_startup_smoke_test`.
- Real startup no longer reports `Plugin handle not found for UserManagement`.
- Real startup no longer reports `Critical plugin start failed: "UserManagement"`.
```

- [ ] **Step 2: Write the decision-log entry**

```md
<!-- docs/superpowers/tracking/platform-migration-decision-log.md -->
## 2026-04-21

- Decision: repair `UserManagement` locally before widening to shared plugin-macro governance.
- Rationale: the current failure is still a concrete `UserManagement` runtime artifact and startup problem, so widening the scope before restoring one reliable local truth would add risk without removing the active blocker.
- Impact: `UserManagement` now has dedicated runtime artifact verification and startup smoke acceptance under `build_x64/Release`.
```

- [ ] **Step 3: Mark the implementation plan status at the end of this plan file**

```md
<!-- docs/superpowers/plans/2026-04-21-user-management-runtime-artifact-remediation-implementation.md -->
Status update 2026-04-21:

- Completed. `UserManagement` runtime artifact delivery is now stable under `build_x64/Release`.
- Acceptance rerun passed for runtime artifact contract verification, startup smoke, and descriptor runtime layout.
- The remediation remained local to `UserManagement` and did not widen into shared plugin-macro refactoring.
```

- [ ] **Step 4: Run the full acceptance commands**

Run:

```powershell
cmake --build build_x64 --config Release --target UserManagement medicalpro
ctest --test-dir build_x64 -C Release -R "runtime_artifact_layout_test|platform_descriptor_runtime_layout_test|user_management_runtime_artifact_contract_test|user_management_startup_smoke_test" --output-on-failure
$exe = Resolve-Path 'build_x64/Release/medicalpro.exe'
$p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru
Start-Sleep -Seconds 8
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
```

Expected:

- `runtime_artifact_layout_test` PASS
- `platform_descriptor_runtime_layout_test` PASS
- `user_management_runtime_artifact_contract_test` PASS
- `user_management_startup_smoke_test` PASS
- manual startup output contains neither:
  - `Plugin handle not found for UserManagement`
  - nor `Critical plugin start failed: "UserManagement"`

- [ ] **Step 5: Commit the doc write-back and acceptance state**

```powershell
git add docs/current_status_and_project_overview.md docs/superpowers/tracking/platform-migration-decision-log.md docs/superpowers/plans/2026-04-21-user-management-runtime-artifact-remediation-implementation.md
git commit -m "docs: record user management runtime artifact remediation"
```

## Self-Review

- Spec coverage:
  - runtime artifact delivery under `build_x64/Release`: Task 1 and Task 2
  - descriptor and symbolic-name truth checks: Task 1
  - local `UserManagement`-only repair scope: Task 2 and Task 3
  - real startup acceptance with known-failure strings removed: Task 1 and Task 3
  - docs and decision-log write-back: Task 3
- Placeholder scan:
  - no `TODO`, `TBD`, `later`, or open placeholders remain
  - every code-changing step includes concrete code
  - every verification step includes exact commands and expected outcomes
- Type consistency:
  - test names are consistent: `user_management_runtime_artifact_contract_test`, `user_management_startup_smoke_test`
  - runtime inspection sidecar name is consistent: `UserManagement.manifest`
  - failure-class names are consistent: `user_management_runtime_layout_mismatch`, `user_management_descriptor_missing`, `user_management_symbolic_name_mismatch`, `user_management_startup_smoke_failed`
