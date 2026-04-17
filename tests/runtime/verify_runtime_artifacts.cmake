cmake_minimum_required(VERSION 3.16)

set(required_files
    "${user_management_plugin}"
    "${dicom_viewer_plugin}"
    "${four_view_display_plugin}"
    "${meshgpu_runtime_dll}"
    "${plugin_policy_file}"
)

set(missing_artifacts)
foreach(required_file IN LISTS required_files)
    if(NOT EXISTS "${required_file}")
        list(APPEND missing_artifacts "${required_file}")
    endif()
endforeach()

if(NOT EXISTS "${data_dir}" OR NOT IS_DIRECTORY "${data_dir}")
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

if(missing_artifacts)
    string(JOIN "\n - " missing_report ${missing_artifacts})
    message(FATAL_ERROR "Runtime artifacts are missing:\n - ${missing_report}")
endif()

message(STATUS "Runtime artifacts are available in ${runtime_dir}")
