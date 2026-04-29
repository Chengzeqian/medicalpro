find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)

add_subdirectory(unit)

if(TARGET medicalpro)
    add_test(
        NAME runtime_artifact_layout_test
        COMMAND ${CMAKE_COMMAND}
            -Druntime_dir=$<TARGET_FILE_DIR:medicalpro>
            -Dmeshgpu_runtime_dll=$<TARGET_FILE_DIR:medicalpro>/MeshGPULib.dll
            -Ddata_dir=$<TARGET_FILE_DIR:medicalpro>/data
            -Drequire_platform_descriptors=ON
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

    add_test(
        NAME plugin_truth_source_runtime_contract_test
        COMMAND ${CMAKE_COMMAND}
            -Druntime_dir=$<TARGET_FILE_DIR:medicalpro>
            -Dverify_plugin_truth_source_runtime_contract=ON
            -P ${CMAKE_CURRENT_SOURCE_DIR}/runtime/verify_runtime_artifacts.cmake
    )

    if(WIN32)
        find_program(POWERSHELL_EXECUTABLE NAMES pwsh powershell REQUIRED)
        add_test(
            NAME cold_start_welcome_shell_smoke_test
            COMMAND ${POWERSHELL_EXECUTABLE}
                -NoProfile
                -ExecutionPolicy Bypass
                -File ${CMAKE_CURRENT_SOURCE_DIR}/runtime/verify_cold_start_welcome_shell.ps1
                -ExePath $<TARGET_FILE:medicalpro>
                -WorkingDirectory $<TARGET_FILE_DIR:medicalpro>
        )
    endif()
endif()

if(TARGET medicalpro AND WIN32)
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
