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
