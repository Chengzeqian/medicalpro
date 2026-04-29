cmake_minimum_required(VERSION 3.16)

function(append_missing_artifact path_value)
    if(NOT EXISTS "${path_value}")
        list(APPEND missing_artifacts "${path_value}")
        set(missing_artifacts "${missing_artifacts}" PARENT_SCOPE)
    endif()
endfunction()

function(require_json_field_value json_file field_name expected_value failure_code)
    file(READ "${json_file}" json_text)
    string(REGEX MATCH "\"${field_name}\"[ \t\r\n]*:[ \t\r\n]*\"([^\"]+)\"" field_match "${json_text}")
    if(NOT field_match)
        message(FATAL_ERROR "${failure_code}: ${json_file} is missing field ${field_name}")
    endif()

    set(actual_value "${CMAKE_MATCH_1}")
    if(NOT actual_value STREQUAL expected_value)
        message(FATAL_ERROR "${failure_code}: ${json_file} expected ${field_name}=${expected_value} but got ${actual_value}")
    endif()
endfunction()

function(require_nested_json_field_value json_file parent_name field_name expected_value failure_code)
    file(READ "${json_file}" json_text)
    string(REGEX MATCH "\"${parent_name}\"[ \t\r\n]*:[ \t\r\n]*\\{([^\\}]*)\\}" parent_match "${json_text}")
    if(NOT parent_match)
        message(FATAL_ERROR "${failure_code}: ${json_file} is missing object ${parent_name}")
    endif()

    set(parent_body "${CMAKE_MATCH_1}")
    string(REGEX MATCH "\"${field_name}\"[ \t\r\n]*:[ \t\r\n]*\"([^\"]+)\"" child_match "${parent_body}")
    if(NOT child_match)
        message(FATAL_ERROR "${failure_code}: ${json_file} is missing ${parent_name}.${field_name}")
    endif()

    set(actual_value "${CMAKE_MATCH_1}")
    if(NOT actual_value STREQUAL expected_value)
        message(FATAL_ERROR "${failure_code}: ${json_file} expected ${parent_name}.${field_name}=${expected_value} but got ${actual_value}")
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

set(required_files)
if(meshgpu_runtime_dll)
    list(APPEND required_files "${meshgpu_runtime_dll}")
endif()

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

file(GLOB stale_runtime_host_artifacts
    "${runtime_dir}/CTK*.dll"
    "${runtime_dir}/plugins/liborg_commontk_eventadmin.dll"
)
if(stale_runtime_host_artifacts)
    string(JOIN "\n - " stale_runtime_host_report ${stale_runtime_host_artifacts})
    message(FATAL_ERROR "legacy runtime host artifacts still exist in runtime layout:\n - ${stale_runtime_host_report}")
endif()

file(GLOB stale_legacy_plugin_manifests
    "${runtime_dir}/plugins/*.manifest"
)
if(stale_legacy_plugin_manifests)
    string(JOIN "\n - " stale_manifest_report ${stale_legacy_plugin_manifests})
    message(FATAL_ERROR "legacy plugin manifests still exist in runtime layout:\n - ${stale_manifest_report}")
endif()

if(verify_plugin_truth_source_runtime_contract)
    set(platform_runtime_file "${runtime_dir}/config/platform_runtime.json")

    append_missing_artifact("${platform_runtime_file}")

    set(platform_descriptor_files
        "${runtime_dir}/plugins/descriptors/UserManagement.json"
        "${runtime_dir}/plugins/descriptors/DicomViewer.json"
        "${runtime_dir}/plugins/descriptors/FourViewDisplay.json"
        "${runtime_dir}/plugins/descriptors/RegistrationCore.json"
        "${runtime_dir}/plugins/descriptors/OpticalTracking.json"
    )

    foreach(descriptor_file IN LISTS platform_descriptor_files)
        append_missing_artifact("${descriptor_file}")
    endforeach()

    if(missing_artifacts)
        string(JOIN "\n - " missing_report ${missing_artifacts})
        message(FATAL_ERROR "plugin_truth_source_runtime_layout_mismatch:\n - ${missing_report}")
    endif()

    require_json_field_value(
        "${platform_runtime_file}"
        "descriptor_directory"
        "plugins/descriptors"
        "platform_descriptor_directory_mismatch"
    )
endif()

if(missing_artifacts)
    string(JOIN "\n - " missing_report ${missing_artifacts})
    message(FATAL_ERROR "Runtime artifacts are missing:\n - ${missing_report}")
endif()

message(STATUS "Runtime artifacts are available in ${runtime_dir}")
