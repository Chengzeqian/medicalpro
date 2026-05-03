function(medicalpro_sync_framework_runtime target_name)
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Framework>"
            "$<TARGET_FILE_DIR:${target_name}>/$<TARGET_FILE_NAME:Framework>"
        COMMENT "Syncing Framework runtime for ${target_name}"
    )
endfunction()

function(medicalpro_sync_probe_calibration_runtime target_name)
    if(TARGET ProbeCalibrationDLL)
        add_dependencies(${target_name} ProbeCalibrationDLL)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:ProbeCalibrationDLL>"
                "$<TARGET_FILE_DIR:${target_name}>/ProbeCalibration.dll"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE_DIR:ProbeCalibrationDLL>/fusionTrack64.dll"
                "$<TARGET_FILE_DIR:${target_name}>/fusionTrack64.dll"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE_DIR:ProbeCalibrationDLL>/device64.dll"
                "$<TARGET_FILE_DIR:${target_name}>/device64.dll"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${target_name}>/atracsys_geometry"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${MEDICALPRO_ATRACSYS_SDK_DIR}/data"
                "$<TARGET_FILE_DIR:${target_name}>/atracsys_geometry"
            COMMENT "Syncing ProbeCalibration runtime for ${target_name}"
        )
    endif()
endfunction()

add_executable(three_page_presentation_utils_test
    ThreePagePresentationUtilsTest.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/ThreePagePresentationUtils.cpp
)

target_include_directories(three_page_presentation_utils_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(three_page_presentation_utils_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME three_page_presentation_utils_test
    COMMAND three_page_presentation_utils_test
)

add_executable(branding_and_console_utils_test
    BrandingAndConsoleUtilsTest.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/WelcomeBrandingUtils.cpp
    ${CMAKE_SOURCE_DIR}/Framework/ConsoleLogBridge.cpp
    ${CMAKE_SOURCE_DIR}/resources.qrc
)

target_include_directories(branding_and_console_utils_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(branding_and_console_utils_test PRIVATE
    FRAMEWORK_LIBRARY
)

target_link_libraries(branding_and_console_utils_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME branding_and_console_utils_test
    COMMAND branding_and_console_utils_test
)

add_executable(startup_console_log_policy_test
    StartupConsoleLogPolicyTest.cpp
)

target_include_directories(startup_console_log_policy_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(startup_console_log_policy_test PRIVATE
    MEDICALPRO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(startup_console_log_policy_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME startup_console_log_policy_test
    COMMAND startup_console_log_policy_test
)

add_executable(platform_runtime_host_ports_contract_test
    PlatformRuntimeHostPortsContractTest.cpp
)

target_include_directories(platform_runtime_host_ports_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(platform_runtime_host_ports_contract_test PRIVATE
    MEDICALPRO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(platform_runtime_host_ports_contract_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME platform_runtime_host_ports_contract_test
    COMMAND platform_runtime_host_ports_contract_test
)

add_executable(platform_naming_governance_contract_test
    PlatformNamingGovernanceContractTest.cpp
)

target_include_directories(platform_naming_governance_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(platform_naming_governance_contract_test PRIVATE
    MEDICALPRO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(platform_naming_governance_contract_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME platform_naming_governance_contract_test
    COMMAND platform_naming_governance_contract_test
)

add_executable(source_encoding_governance_test
    SourceEncodingGovernanceTest.cpp
)

target_include_directories(source_encoding_governance_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(source_encoding_governance_test PRIVATE
    MEDICALPRO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(source_encoding_governance_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME source_encoding_governance_test
    COMMAND source_encoding_governance_test
)

add_executable(runtime_host_detachment_contract_test
    RuntimeHostDetachmentContractTest.cpp
)

target_include_directories(runtime_host_detachment_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(runtime_host_detachment_contract_test PRIVATE
    MEDICALPRO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(runtime_host_detachment_contract_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME runtime_host_detachment_contract_test
    COMMAND runtime_host_detachment_contract_test
)

add_executable(runtime_build_deployment_contract_test
    RuntimeBuildDeploymentContractTest.cpp
)

target_include_directories(runtime_build_deployment_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(runtime_build_deployment_contract_test PRIVATE
    MEDICALPRO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(runtime_build_deployment_contract_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME runtime_build_deployment_contract_test
    COMMAND runtime_build_deployment_contract_test
)

add_executable(platform_plugin_build_governance_contract_test
    PlatformPluginBuildGovernanceContractTest.cpp
)

target_include_directories(platform_plugin_build_governance_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(platform_plugin_build_governance_contract_test PRIVATE
    MEDICALPRO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(platform_plugin_build_governance_contract_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME platform_plugin_build_governance_contract_test
    COMMAND platform_plugin_build_governance_contract_test
)

add_executable(platform_plugin_host_primitives_contract_test
    PlatformPluginHostPrimitivesContractTest.cpp
)

target_include_directories(platform_plugin_host_primitives_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(platform_plugin_host_primitives_contract_test PRIVATE
    MEDICALPRO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(platform_plugin_host_primitives_contract_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME platform_plugin_host_primitives_contract_test
    COMMAND platform_plugin_host_primitives_contract_test
)

add_executable(platform_plugin_host_lifecycle_behavior_test
    PlatformPluginHostLifecycleBehaviorTest.cpp
)

target_include_directories(platform_plugin_host_lifecycle_behavior_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(platform_plugin_host_lifecycle_behavior_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME platform_plugin_host_lifecycle_behavior_test
    COMMAND ${CMAKE_COMMAND} -E env
        "PATH=$<TARGET_FILE_DIR:platform_plugin_host_lifecycle_behavior_test>;$<TARGET_FILE_DIR:medicalpro>;$<TARGET_FILE_DIR:Framework>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Gui>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Widgets>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Sql>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Network>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::OpenGL>;$ENV{PATH}"
        $<TARGET_FILE:platform_plugin_host_lifecycle_behavior_test>
)

set_tests_properties(platform_plugin_host_lifecycle_behavior_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:platform_plugin_host_lifecycle_behavior_test>\;$<TARGET_FILE_DIR:medicalpro>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Gui>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Widgets>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Sql>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Network>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::OpenGL>\;$ENV{PATH}"
)

add_executable(platform_built_in_module_bootstrap_behavior_test
    PlatformBuiltInModuleBootstrapBehaviorTest.cpp
)

target_include_directories(platform_built_in_module_bootstrap_behavior_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(platform_built_in_module_bootstrap_behavior_test PRIVATE
    MEDICALPRO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(platform_built_in_module_bootstrap_behavior_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME platform_built_in_module_bootstrap_behavior_test
    COMMAND ${CMAKE_COMMAND} -E env
        "PATH=$<TARGET_FILE_DIR:platform_built_in_module_bootstrap_behavior_test>;$<TARGET_FILE_DIR:medicalpro>;$<TARGET_FILE_DIR:Framework>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Gui>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Widgets>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Sql>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Network>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::OpenGL>;$ENV{PATH}"
        $<TARGET_FILE:platform_built_in_module_bootstrap_behavior_test>
)

set_tests_properties(platform_built_in_module_bootstrap_behavior_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:platform_built_in_module_bootstrap_behavior_test>\;$<TARGET_FILE_DIR:medicalpro>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Gui>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Widgets>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Sql>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Network>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::OpenGL>\;$ENV{PATH}"
)

add_executable(platform_runtime_host_adapter_platform_fallback_test
    PlatformRuntimeHostAdapterPlatformFallbackTest.cpp
)

target_include_directories(platform_runtime_host_adapter_platform_fallback_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(platform_runtime_host_adapter_platform_fallback_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
    RegistrationCorePlatformModuleLib
)

add_test(
    NAME platform_runtime_host_adapter_platform_fallback_test
    COMMAND ${CMAKE_COMMAND} -E env
        "PATH=$<TARGET_FILE_DIR:platform_runtime_host_adapter_platform_fallback_test>;$<TARGET_FILE_DIR:medicalpro>;$<TARGET_FILE_DIR:Framework>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Gui>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Widgets>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Sql>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Network>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::OpenGL>;$ENV{PATH}"
        $<TARGET_FILE:platform_runtime_host_adapter_platform_fallback_test>
)

set_tests_properties(platform_runtime_host_adapter_platform_fallback_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:platform_runtime_host_adapter_platform_fallback_test>\;$<TARGET_FILE_DIR:medicalpro>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Gui>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Widgets>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Sql>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Network>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::OpenGL>\;$ENV{PATH}"
)

add_executable(platform_plugin_host_core_migration_contract_test
    PlatformPluginHostCoreMigrationContractTest.cpp
)

target_include_directories(platform_plugin_host_core_migration_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(platform_plugin_host_core_migration_contract_test PRIVATE
    MEDICALPRO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(platform_plugin_host_core_migration_contract_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME platform_plugin_host_core_migration_contract_test
    COMMAND platform_plugin_host_core_migration_contract_test
)

add_executable(platform_plugin_host_ui_migration_contract_test
    PlatformPluginHostUiMigrationContractTest.cpp
)

target_include_directories(platform_plugin_host_ui_migration_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(platform_plugin_host_ui_migration_contract_test PRIVATE
    MEDICALPRO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(platform_plugin_host_ui_migration_contract_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME platform_plugin_host_ui_migration_contract_test
    COMMAND platform_plugin_host_ui_migration_contract_test
)

add_executable(runtime_console_log_policy_test
    RuntimeConsoleLogPolicyTest.cpp
)

target_include_directories(runtime_console_log_policy_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(runtime_console_log_policy_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME runtime_console_log_policy_test
    COMMAND runtime_console_log_policy_test
)

add_executable(welcome_page_runtime_refresh_test
    WelcomePageRuntimeRefreshTest.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/BasePage.h
    ${CMAKE_SOURCE_DIR}/UI/NewPages/WelcomePage.h
    ${CMAKE_SOURCE_DIR}/UI/NewPages/WelcomePage.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/ThreePagePresentationUtils.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/WelcomeBrandingUtils.cpp
    ${CMAKE_SOURCE_DIR}/UI/Forms/WelcomePage.ui
)

set_target_properties(welcome_page_runtime_refresh_test PROPERTIES
    AUTOUIC_SEARCH_PATHS "${CMAKE_SOURCE_DIR}/UI/Forms"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/MinSizeRel"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/RelWithDebInfo"
)

target_include_directories(welcome_page_runtime_refresh_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/UI/NewPages
    ${CMAKE_SOURCE_DIR}/UI/Forms
)

target_link_libraries(welcome_page_runtime_refresh_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

if(MSVC)
    target_compile_options(welcome_page_runtime_refresh_test PRIVATE /utf-8)
endif()

add_test(
    NAME welcome_page_runtime_refresh_test
    COMMAND welcome_page_runtime_refresh_test
)

add_executable(welcome_page_bootstrap_state_test
    WelcomePageBootstrapStateTest.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/BasePage.h
    ${CMAKE_SOURCE_DIR}/UI/NewPages/WelcomePage.h
    ${CMAKE_SOURCE_DIR}/UI/NewPages/WelcomePage.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/ThreePagePresentationUtils.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/WelcomeBrandingUtils.cpp
    ${CMAKE_SOURCE_DIR}/UI/Forms/WelcomePage.ui
)

set_target_properties(welcome_page_bootstrap_state_test PROPERTIES
    AUTOUIC_SEARCH_PATHS "${CMAKE_SOURCE_DIR}/UI/Forms"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/MinSizeRel"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/RelWithDebInfo"
)

target_include_directories(welcome_page_bootstrap_state_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/UI/NewPages
    ${CMAKE_SOURCE_DIR}/UI/Forms
)

target_link_libraries(welcome_page_bootstrap_state_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

if(MSVC)
    target_compile_options(welcome_page_bootstrap_state_test PRIVATE /utf-8)
endif()

add_test(
    NAME welcome_page_bootstrap_state_test
    COMMAND welcome_page_bootstrap_state_test
)

add_executable(startup_bootstrap_controller_test
    StartupBootstrapControllerTest.cpp
)

target_include_directories(startup_bootstrap_controller_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(startup_bootstrap_controller_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

if(MSVC)
    target_compile_options(startup_bootstrap_controller_test PRIVATE /utf-8)
endif()

add_test(
    NAME startup_bootstrap_controller_test
    COMMAND startup_bootstrap_controller_test
)

set_tests_properties(startup_bootstrap_controller_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:startup_bootstrap_controller_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Widgets>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(startup_ui_coordinator_test
    StartupUiCoordinatorTest.cpp
)

target_include_directories(startup_ui_coordinator_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(startup_ui_coordinator_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

if(MSVC)
    target_compile_options(startup_ui_coordinator_test PRIVATE /utf-8)
endif()

add_test(
    NAME startup_ui_coordinator_test
    COMMAND startup_ui_coordinator_test
)

set_tests_properties(startup_ui_coordinator_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:startup_ui_coordinator_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Widgets>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(platform_state_store_handoff_test
    PlatformStateStoreHandoffTest.cpp
)

target_include_directories(platform_state_store_handoff_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(platform_state_store_handoff_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

if(MSVC)
    target_compile_options(platform_state_store_handoff_test PRIVATE /utf-8)
endif()

add_test(
    NAME platform_state_store_handoff_test
    COMMAND platform_state_store_handoff_test
)

set_tests_properties(platform_state_store_handoff_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:platform_state_store_handoff_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(startup_shell_surface_contract_test
    StartupShellSurfaceContractTest.cpp
    ${CMAKE_SOURCE_DIR}/UI/StartupShell.h
    ${CMAKE_SOURCE_DIR}/UI/StartupShell.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/BasePage.h
    ${CMAKE_SOURCE_DIR}/UI/NewPages/WelcomePage.h
    ${CMAKE_SOURCE_DIR}/UI/NewPages/WelcomePage.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/ThreePagePresentationUtils.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/WelcomeBrandingUtils.cpp
    ${CMAKE_SOURCE_DIR}/UI/Forms/WelcomePage.ui
)

set_target_properties(startup_shell_surface_contract_test PROPERTIES
    AUTOUIC_SEARCH_PATHS "${CMAKE_SOURCE_DIR}/UI/Forms"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/MinSizeRel"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/RelWithDebInfo"
)

target_include_directories(startup_shell_surface_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/UI/NewPages
    ${CMAKE_SOURCE_DIR}/UI/Forms
)

target_link_libraries(startup_shell_surface_contract_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

if(MSVC)
    target_compile_options(startup_shell_surface_contract_test PRIVATE /utf-8)
endif()

add_test(
    NAME startup_shell_surface_contract_test
    COMMAND startup_shell_surface_contract_test
)

set_tests_properties(startup_shell_surface_contract_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:startup_shell_surface_contract_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Widgets>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(platform_descriptor_loader_test
    PlatformDescriptorLoaderTest.cpp
)

target_include_directories(platform_descriptor_loader_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(platform_descriptor_loader_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME platform_descriptor_loader_test
    COMMAND platform_descriptor_loader_test
)

set_tests_properties(platform_descriptor_loader_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:platform_descriptor_loader_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(platform_managed_plugin_plan_test
    PlatformManagedPluginPlanTest.cpp
)

target_include_directories(platform_managed_plugin_plan_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(platform_managed_plugin_plan_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME platform_managed_plugin_plan_test
    COMMAND platform_managed_plugin_plan_test
)

set_tests_properties(platform_managed_plugin_plan_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:platform_managed_plugin_plan_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(platform_on_demand_activation_plan_test
    PlatformOnDemandActivationPlanTest.cpp
)

target_include_directories(platform_on_demand_activation_plan_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(platform_on_demand_activation_plan_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME platform_on_demand_activation_plan_test
    COMMAND platform_on_demand_activation_plan_test
)

set_tests_properties(platform_on_demand_activation_plan_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:platform_on_demand_activation_plan_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(platform_plugin_policy_bridge_test
    PlatformPluginPolicyBridgeTest.cpp
)

target_include_directories(platform_plugin_policy_bridge_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(platform_plugin_policy_bridge_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME platform_plugin_policy_bridge_test
    COMMAND platform_plugin_policy_bridge_test
)

set_tests_properties(platform_plugin_policy_bridge_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:platform_plugin_policy_bridge_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(platform_startup_trace_recorder_test
    PlatformStartupTraceRecorderTest.cpp
)

target_include_directories(platform_startup_trace_recorder_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(platform_startup_trace_recorder_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME platform_startup_trace_recorder_test
    COMMAND platform_startup_trace_recorder_test
)

set_tests_properties(platform_startup_trace_recorder_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:platform_startup_trace_recorder_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(startup_orchestrator_lifecycle_test
    StartupOrchestratorLifecycleTest.cpp
)

target_include_directories(startup_orchestrator_lifecycle_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(startup_orchestrator_lifecycle_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME startup_orchestrator_lifecycle_test
    COMMAND startup_orchestrator_lifecycle_test
)

set_tests_properties(startup_orchestrator_lifecycle_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:startup_orchestrator_lifecycle_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Widgets>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(startup_phase_registrar_contract_test
    StartupPhaseRegistrarContractTest.cpp
)

target_include_directories(startup_phase_registrar_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(startup_phase_registrar_contract_test PRIVATE
    MEDICALPRO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(startup_phase_registrar_contract_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME startup_phase_registrar_contract_test
    COMMAND startup_phase_registrar_contract_test
)

set_tests_properties(startup_phase_registrar_contract_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:startup_phase_registrar_contract_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Widgets>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(platform_dependency_graph_test
    PlatformDependencyGraphTest.cpp
    ${CMAKE_SOURCE_DIR}/Framework/Platform/Kernel/PlatformDependencyGraph.cpp
)

target_include_directories(platform_dependency_graph_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(platform_dependency_graph_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME platform_dependency_graph_test
    COMMAND platform_dependency_graph_test
)

set_tests_properties(platform_dependency_graph_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:platform_dependency_graph_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(platform_diagnostics_service_test
    PlatformDiagnosticsServiceTest.cpp
)

set_target_properties(platform_diagnostics_service_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/MinSizeRel"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/RelWithDebInfo"
)

target_include_directories(platform_diagnostics_service_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(platform_diagnostics_service_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME platform_diagnostics_service_test
    COMMAND platform_diagnostics_service_test
)

set_tests_properties(platform_diagnostics_service_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:platform_diagnostics_service_test>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(platform_plugin_lifecycle_aggregator_test
    PlatformPluginLifecycleAggregatorTest.cpp
)

set_target_properties(platform_plugin_lifecycle_aggregator_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/MinSizeRel"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/RelWithDebInfo"
)

target_include_directories(platform_plugin_lifecycle_aggregator_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(platform_plugin_lifecycle_aggregator_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME platform_plugin_lifecycle_aggregator_test
    COMMAND platform_plugin_lifecycle_aggregator_test
)

set_tests_properties(platform_plugin_lifecycle_aggregator_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:platform_plugin_lifecycle_aggregator_test>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(platform_diagnostics_page_test
    PlatformDiagnosticsPageTest.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/BasePage.h
    ${CMAKE_SOURCE_DIR}/UI/NewPages/PlatformDiagnosticsPage.h
    ${CMAKE_SOURCE_DIR}/UI/NewPages/PlatformDiagnosticsPage.cpp
    ${CMAKE_SOURCE_DIR}/UI/Forms/PlatformDiagnosticsPage.ui
)

set_target_properties(platform_diagnostics_page_test PROPERTIES
    AUTOUIC_SEARCH_PATHS "${CMAKE_SOURCE_DIR}/UI/Forms"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/MinSizeRel"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/RelWithDebInfo"
)

target_include_directories(platform_diagnostics_page_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/UI/NewPages
    ${CMAKE_SOURCE_DIR}/UI/Forms
)

target_link_libraries(platform_diagnostics_page_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME platform_diagnostics_page_test
    COMMAND platform_diagnostics_page_test
)

set_tests_properties(platform_diagnostics_page_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:platform_diagnostics_page_test>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(platform_facades_test
    PlatformFacadesTest.cpp
)

target_include_directories(platform_facades_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(platform_facades_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME platform_facades_test
    COMMAND platform_facades_test
)

set_tests_properties(platform_facades_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:platform_facades_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(platform_startup_coordinator_test
    PlatformStartupCoordinatorTest.cpp
)

target_include_directories(platform_startup_coordinator_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(platform_startup_coordinator_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME platform_startup_coordinator_test
    COMMAND platform_startup_coordinator_test
)

set_tests_properties(platform_startup_coordinator_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:platform_startup_coordinator_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(platform_warmup_coordinator_test
    PlatformWarmupCoordinatorTest.cpp
)

target_include_directories(platform_warmup_coordinator_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(platform_warmup_coordinator_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME platform_warmup_coordinator_test
    COMMAND platform_warmup_coordinator_test
)

set_tests_properties(platform_warmup_coordinator_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:platform_warmup_coordinator_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(core_pages_platform_providers_test
    CorePagesPlatformProvidersTest.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/BasePage.h
    ${CMAKE_SOURCE_DIR}/UI/NewPages/ThreePagePresentationUtils.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/ManagementPage.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/DashboardPage.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/ModuleSelectionPage.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/SystemSettingsPage.cpp
    ${CMAKE_SOURCE_DIR}/UI/Forms/ManagementPage.ui
    ${CMAKE_SOURCE_DIR}/UI/Forms/DashboardPage.ui
    ${CMAKE_SOURCE_DIR}/UI/Forms/ModuleSelectionPage.ui
    ${CMAKE_SOURCE_DIR}/UI/Forms/SystemSettingsPage.ui
)

set_target_properties(core_pages_platform_providers_test PROPERTIES
    AUTOUIC_SEARCH_PATHS "${CMAKE_SOURCE_DIR}/UI/Forms"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/MinSizeRel"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/RelWithDebInfo"
)

target_include_directories(core_pages_platform_providers_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/UI/NewPages
    ${CMAKE_SOURCE_DIR}/UI/Forms
)

target_link_libraries(core_pages_platform_providers_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME core_pages_platform_providers_test
    COMMAND core_pages_platform_providers_test
)

set_tests_properties(core_pages_platform_providers_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:core_pages_platform_providers_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(login_page_platform_providers_test
    LoginPagePlatformProvidersTest.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/BasePage.h
    ${CMAKE_SOURCE_DIR}/UI/NewPages/LoginPage.h
    ${CMAKE_SOURCE_DIR}/UI/NewPages/LoginPage.cpp
    ${CMAKE_SOURCE_DIR}/UI/Forms/LoginPage.ui
)

set_target_properties(login_page_platform_providers_test PROPERTIES
    AUTOUIC_SEARCH_PATHS "${CMAKE_SOURCE_DIR}/UI/Forms"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/MinSizeRel"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/RelWithDebInfo"
)

target_include_directories(login_page_platform_providers_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/UI/NewPages
    ${CMAKE_SOURCE_DIR}/UI/Forms
)

target_link_libraries(login_page_platform_providers_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME login_page_platform_providers_test
    COMMAND login_page_platform_providers_test
)

set_tests_properties(login_page_platform_providers_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:login_page_platform_providers_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(ui_platform_decoupling_acceptance_test
    UiPlatformDecouplingAcceptanceTest.cpp
)

target_include_directories(ui_platform_decoupling_acceptance_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(ui_platform_decoupling_acceptance_test PRIVATE
    MEDICALPRO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(ui_platform_decoupling_acceptance_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME ui_platform_decoupling_acceptance_test
    COMMAND ui_platform_decoupling_acceptance_test
)

add_executable(main_interface_entry_api_contract_test
    MainInterfaceEntryApiContractTest.cpp
)

target_include_directories(main_interface_entry_api_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(main_interface_entry_api_contract_test PRIVATE
    MEDICALPRO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(main_interface_entry_api_contract_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME main_interface_entry_api_contract_test
    COMMAND main_interface_entry_api_contract_test
)

add_executable(startup_welcome_entry_source_contract_test
    StartupWelcomeEntrySourceContractTest.cpp
)

target_include_directories(startup_welcome_entry_source_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(startup_welcome_entry_source_contract_test PRIVATE
    MEDICALPRO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(startup_welcome_entry_source_contract_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME startup_welcome_entry_source_contract_test
    COMMAND startup_welcome_entry_source_contract_test
)

add_executable(plugin_truth_source_governance_contract_test
    PluginTruthSourceGovernanceContractTest.cpp
)

target_include_directories(plugin_truth_source_governance_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(plugin_truth_source_governance_contract_test PRIVATE
    MEDICALPRO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(plugin_truth_source_governance_contract_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME plugin_truth_source_governance_contract_test
    COMMAND plugin_truth_source_governance_contract_test
)

add_executable(plugin_legacy_consumer_governance_contract_test
    PluginLegacyConsumerGovernanceContractTest.cpp
)

target_include_directories(plugin_legacy_consumer_governance_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(plugin_legacy_consumer_governance_contract_test PRIVATE
    MEDICALPRO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(plugin_legacy_consumer_governance_contract_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME plugin_legacy_consumer_governance_contract_test
    COMMAND plugin_legacy_consumer_governance_contract_test
)

add_executable(platform_ui_bridge_test
    PlatformUiBridgeTest.cpp
)

target_include_directories(platform_ui_bridge_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(platform_ui_bridge_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME platform_ui_bridge_test
    COMMAND platform_ui_bridge_test
)

set_tests_properties(platform_ui_bridge_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:platform_ui_bridge_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(navigation_workflow_context_test
    NavigationWorkflowContextTest.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/Navigation/navigation_workflow_context.cpp
)

target_include_directories(navigation_workflow_context_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(navigation_workflow_context_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Test
)

if(MSVC)
    target_compile_options(navigation_workflow_context_test PRIVATE /utf-8)
endif()

add_test(
    NAME navigation_workflow_context_test
    COMMAND navigation_workflow_context_test
)

add_executable(navigation_runtime_state_test
    NavigationRuntimeStateTest.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/Navigation/navigation_runtime_state.cpp
)

target_include_directories(navigation_runtime_state_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(navigation_runtime_state_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Test
)

if(MSVC)
    target_compile_options(navigation_runtime_state_test PRIVATE /utf-8)
endif()

add_test(
    NAME navigation_runtime_state_test
    COMMAND navigation_runtime_state_test
)

set_tests_properties(navigation_runtime_state_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:navigation_runtime_state_test>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Gui>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>\;$ENV{PATH}"
)

add_executable(navigation_runtime_coordinator_contract_test
    NavigationRuntimeCoordinatorContractTest.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/Navigation/navigation_runtime_coordinator.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/Navigation/navigation_runtime_state.cpp
)

target_include_directories(navigation_runtime_coordinator_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(navigation_runtime_coordinator_contract_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

if(MSVC)
    target_compile_options(navigation_runtime_coordinator_contract_test PRIVATE /utf-8)
endif()

add_test(
    NAME navigation_runtime_coordinator_contract_test
    COMMAND navigation_runtime_coordinator_contract_test
)

set_tests_properties(navigation_runtime_coordinator_contract_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:navigation_runtime_coordinator_contract_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Gui>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>\;$ENV{PATH}"
)

add_executable(navigation_workflow_coordinator_test
    NavigationWorkflowCoordinatorTest.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/Navigation/navigation_evaluation_controller.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/Navigation/navigation_runtime_coordinator.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/Navigation/navigation_runtime_state.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/Navigation/navigation_workflow_coordinator.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/Navigation/navigation_workflow_context.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/Navigation/preparation_planning_controller.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/Navigation/registration_controller.cpp
)

target_include_directories(navigation_workflow_coordinator_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(navigation_workflow_coordinator_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

if(MSVC)
    target_compile_options(navigation_workflow_coordinator_test PRIVATE /utf-8)
endif()

add_test(
    NAME navigation_workflow_coordinator_test
    COMMAND navigation_workflow_coordinator_test
)

set_tests_properties(navigation_workflow_coordinator_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:navigation_workflow_coordinator_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Widgets>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Gui>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>\;$ENV{PATH}"
)

add_executable(embedded_vtk_view_host_test
    EmbeddedVtkViewHostTest.cpp
    ${CMAKE_SOURCE_DIR}/Framework/VTK/embedded_vtk_view_host.cpp
)

target_include_directories(embedded_vtk_view_host_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(embedded_vtk_view_host_test PRIVATE
    FRAMEWORK_LIBRARY
)

target_link_libraries(embedded_vtk_view_host_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Test
)

if(MSVC)
    target_compile_options(embedded_vtk_view_host_test PRIVATE /utf-8)
endif()

add_test(
    NAME embedded_vtk_view_host_test
    COMMAND embedded_vtk_view_host_test
)

set_tests_properties(embedded_vtk_view_host_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:embedded_vtk_view_host_test>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Widgets>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(navigation_vtk_bridge_test
    NavigationVtkBridgeTest.cpp
    ${CMAKE_SOURCE_DIR}/Framework/VTK/embedded_vtk_view_host.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/Navigation/navigation_vtk_bridge.cpp
)

target_include_directories(navigation_vtk_bridge_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

if(VTK_FOUND)
    target_include_directories(navigation_vtk_bridge_test PRIVATE ${VTK_INCLUDE_DIRS})
endif()

target_compile_definitions(navigation_vtk_bridge_test PRIVATE
    FRAMEWORK_LIBRARY
)

target_link_libraries(navigation_vtk_bridge_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Test
)

if(VTK_FOUND)
    target_link_libraries(navigation_vtk_bridge_test PRIVATE ${VTK_LIBRARIES})
endif()

if(MSVC)
    target_compile_options(navigation_vtk_bridge_test PRIVATE /utf-8)
endif()

add_test(
    NAME navigation_vtk_bridge_test
    COMMAND navigation_vtk_bridge_test
)

set_tests_properties(navigation_vtk_bridge_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:navigation_vtk_bridge_test>\;$<TARGET_FILE_DIR:medicalpro>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Widgets>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(ankle_case_workspace_repository_test
    AnkleCaseWorkspaceRepositoryTest.cpp
)

target_include_directories(ankle_case_workspace_repository_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(ankle_case_workspace_repository_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME ankle_case_workspace_repository_test
    COMMAND ankle_case_workspace_repository_test
)

set_tests_properties(ankle_case_workspace_repository_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:ankle_case_workspace_repository_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(innovation_experiment_repository_test
    InnovationExperimentRepositoryTest.cpp
)

target_include_directories(innovation_experiment_repository_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(innovation_experiment_repository_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME innovation_experiment_repository_test
    COMMAND innovation_experiment_repository_test
)

set_tests_properties(innovation_experiment_repository_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:innovation_experiment_repository_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(ankle_planning_service_test
    AnklePlanningServiceTest.cpp
)

target_include_directories(ankle_planning_service_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(ankle_planning_service_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME ankle_planning_service_test
    COMMAND ankle_planning_service_test
)

set_tests_properties(ankle_planning_service_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:ankle_planning_service_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(real_case_asset_bootstrap_test
    RealCaseAssetBootstrapTest.cpp
)

target_include_directories(real_case_asset_bootstrap_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(real_case_asset_bootstrap_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME real_case_asset_bootstrap_test
    COMMAND real_case_asset_bootstrap_test
)

set_tests_properties(real_case_asset_bootstrap_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:real_case_asset_bootstrap_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(real_case_asset_bootstrapper_test
    RealCaseAssetBootstrapperTest.cpp
)

target_include_directories(real_case_asset_bootstrapper_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(real_case_asset_bootstrapper_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME real_case_asset_bootstrapper_test
    COMMAND real_case_asset_bootstrapper_test
)

set_tests_properties(real_case_asset_bootstrapper_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:real_case_asset_bootstrapper_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(real_case_workspace_seed_coordinator_test
    RealCaseWorkspaceSeedCoordinatorTest.cpp
)

target_include_directories(real_case_workspace_seed_coordinator_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(real_case_workspace_seed_coordinator_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME real_case_workspace_seed_coordinator_test
    COMMAND real_case_workspace_seed_coordinator_test
)

set_tests_properties(real_case_workspace_seed_coordinator_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:real_case_workspace_seed_coordinator_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(ankle_navigation_workflow_contract_test
    AnkleNavigationWorkflowContractTest.cpp
)

target_include_directories(ankle_navigation_workflow_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(ankle_navigation_workflow_contract_test PRIVATE
    MEDICALPRO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(ankle_navigation_workflow_contract_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME ankle_navigation_workflow_contract_test
    COMMAND ankle_navigation_workflow_contract_test
)

add_executable(algorithm_subsystems_repository_contract_test
    AlgorithmSubsystemsRepositoryContractTest.cpp
)

target_include_directories(algorithm_subsystems_repository_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(algorithm_subsystems_repository_contract_test PRIVATE
    MEDICALPRO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(algorithm_subsystems_repository_contract_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME algorithm_subsystems_repository_contract_test
    COMMAND algorithm_subsystems_repository_contract_test
)

add_executable(algorithm_build_integration_contract_test
    AlgorithmBuildIntegrationContractTest.cpp
)

target_include_directories(algorithm_build_integration_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(algorithm_build_integration_contract_test PRIVATE
    MEDICALPRO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(algorithm_build_integration_contract_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME algorithm_build_integration_contract_test
    COMMAND algorithm_build_integration_contract_test
)

add_executable(algorithm_runtime_load_path_contract_test
    AlgorithmRuntimeLoadPathContractTest.cpp
)

target_include_directories(algorithm_runtime_load_path_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(algorithm_runtime_load_path_contract_test PRIVATE
    MEDICALPRO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(algorithm_runtime_load_path_contract_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME algorithm_runtime_load_path_contract_test
    COMMAND algorithm_runtime_load_path_contract_test
)

add_executable(meshgpu_subproject_contract_test
    MeshGpuSubprojectContractTest.cpp
)

target_include_directories(meshgpu_subproject_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(meshgpu_subproject_contract_test PRIVATE
    MEDICALPRO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(meshgpu_subproject_contract_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME meshgpu_subproject_contract_test
    COMMAND meshgpu_subproject_contract_test
)

add_executable(probe_calibration_subproject_contract_test
    ProbeCalibrationSubprojectContractTest.cpp
)

target_include_directories(probe_calibration_subproject_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(probe_calibration_subproject_contract_test PRIVATE
    MEDICALPRO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(probe_calibration_subproject_contract_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME probe_calibration_subproject_contract_test
    COMMAND probe_calibration_subproject_contract_test
)

add_executable(target_sensitive_point_selection_test
    TargetSensitivePointSelectionTest.cpp
)

target_include_directories(target_sensitive_point_selection_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(target_sensitive_point_selection_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Test
    PointRegistrationPlatformModuleLib
)

add_test(
    NAME target_sensitive_point_selection_test
    COMMAND target_sensitive_point_selection_test
)

add_executable(point_selection_baseline_strategy_test
    PointSelectionBaselineStrategyTest.cpp
)

target_include_directories(point_selection_baseline_strategy_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(point_selection_baseline_strategy_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Test
    PointRegistrationPlatformModuleLib
)

add_test(
    NAME point_selection_baseline_strategy_test
    COMMAND point_selection_baseline_strategy_test
)

add_executable(point_registration_registration_core_integration_test
    PointRegistrationRegistrationCoreIntegrationTest.cpp
)

target_include_directories(point_registration_registration_core_integration_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(point_registration_registration_core_integration_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Test
    Framework
    PointRegistrationPlatformModuleLib
)

add_test(
    NAME point_registration_registration_core_integration_test
    COMMAND point_registration_registration_core_integration_test
)

set_tests_properties(point_registration_registration_core_integration_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:point_registration_registration_core_integration_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Widgets>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Gui>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>\;$<TARGET_FILE_DIR:VTK::CommonCore>\;$ENV{PATH}"
)

add_executable(registration_core_meshgpu_smoke_test
    RegistrationCoreMeshGpuSmokeTest.cpp
)

set_target_properties(registration_core_meshgpu_smoke_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/MinSizeRel"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/RelWithDebInfo"
)

target_include_directories(registration_core_meshgpu_smoke_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(registration_core_meshgpu_smoke_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Test
    Framework
    RegistrationCorePlatformModuleLib
)

if(TARGET MeshGPULib)
    add_dependencies(registration_core_meshgpu_smoke_test MeshGPULib)
endif()

add_test(
    NAME registration_core_meshgpu_smoke_test
    COMMAND ${CMAKE_COMMAND} -E env
        "PATH=$<TARGET_FILE_DIR:registration_core_meshgpu_smoke_test>;$<TARGET_FILE_DIR:medicalpro>;$<TARGET_FILE_DIR:Framework>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Widgets>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Gui>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>;$<TARGET_FILE_DIR:VTK::CommonCore>;$ENV{PATH}"
        $<TARGET_FILE:registration_core_meshgpu_smoke_test>
)

set_tests_properties(registration_core_meshgpu_smoke_test PROPERTIES
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:registration_core_meshgpu_smoke_test>"
)

add_executable(probe_calibration_runtime_smoke_test
    ProbeCalibrationRuntimeSmokeTest.cpp
)

set_target_properties(probe_calibration_runtime_smoke_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/MinSizeRel"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/RelWithDebInfo"
)

target_include_directories(probe_calibration_runtime_smoke_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(probe_calibration_runtime_smoke_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

if(TARGET ProbeCalibrationDLL)
    add_dependencies(probe_calibration_runtime_smoke_test ProbeCalibrationDLL)
endif()

medicalpro_sync_probe_calibration_runtime(probe_calibration_runtime_smoke_test)

add_test(
    NAME probe_calibration_runtime_smoke_test
    COMMAND ${CMAKE_COMMAND} -E env
        "PATH=$<TARGET_FILE_DIR:probe_calibration_runtime_smoke_test>;$<TARGET_FILE_DIR:medicalpro>;$<TARGET_FILE_DIR:Framework>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>;$ENV{PATH}"
        $<TARGET_FILE:probe_calibration_runtime_smoke_test>
)

set_tests_properties(probe_calibration_runtime_smoke_test PROPERTIES
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:probe_calibration_runtime_smoke_test>"
)

add_executable(optical_tracking_probe_calibration_load_smoke_test
    OpticalTrackingProbeCalibrationLoadSmokeTest.cpp
)

set_target_properties(optical_tracking_probe_calibration_load_smoke_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/MinSizeRel"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/RelWithDebInfo"
)

target_include_directories(optical_tracking_probe_calibration_load_smoke_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(optical_tracking_probe_calibration_load_smoke_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Test
    Framework
    OpticalTrackingPlatformModuleLib
)

if(TARGET ProbeCalibrationDLL)
    add_dependencies(optical_tracking_probe_calibration_load_smoke_test ProbeCalibrationDLL)
endif()

medicalpro_sync_probe_calibration_runtime(optical_tracking_probe_calibration_load_smoke_test)

add_test(
    NAME optical_tracking_probe_calibration_load_smoke_test
    COMMAND ${CMAKE_COMMAND} -E env
        "PATH=$<TARGET_FILE_DIR:optical_tracking_probe_calibration_load_smoke_test>;$<TARGET_FILE_DIR:medicalpro>;$<TARGET_FILE_DIR:Framework>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Network>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Widgets>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Gui>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>;$ENV{PATH}"
        $<TARGET_FILE:optical_tracking_probe_calibration_load_smoke_test>
)

set_tests_properties(optical_tracking_probe_calibration_load_smoke_test PROPERTIES
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:optical_tracking_probe_calibration_load_smoke_test>"
)

add_executable(optical_tracking_probe_calibration_initialization_test
    OpticalTrackingProbeCalibrationInitializationTest.cpp
)

set_target_properties(optical_tracking_probe_calibration_initialization_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/MinSizeRel"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/RelWithDebInfo"
)

target_include_directories(optical_tracking_probe_calibration_initialization_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(optical_tracking_probe_calibration_initialization_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Test
    OpticalTrackingPlatformModuleLib
)

if(TARGET ProbeCalibrationDLL)
    add_dependencies(optical_tracking_probe_calibration_initialization_test ProbeCalibrationDLL)
endif()

medicalpro_sync_probe_calibration_runtime(optical_tracking_probe_calibration_initialization_test)

add_test(
    NAME optical_tracking_probe_calibration_initialization_test
    COMMAND ${CMAKE_COMMAND} -E env
        "PATH=$<TARGET_FILE_DIR:optical_tracking_probe_calibration_initialization_test>;$<TARGET_FILE_DIR:medicalpro>;$<TARGET_FILE_DIR:Framework>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Network>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Widgets>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Gui>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>;$ENV{PATH}"
        $<TARGET_FILE:optical_tracking_probe_calibration_initialization_test>
)

set_tests_properties(optical_tracking_probe_calibration_initialization_test PROPERTIES
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:optical_tracking_probe_calibration_initialization_test>"
)

add_executable(optical_tracking_probe_calibration_tool_geometry_selection_test
    OpticalTrackingProbeCalibrationToolGeometrySelectionTest.cpp
)

set_target_properties(optical_tracking_probe_calibration_tool_geometry_selection_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/MinSizeRel"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/RelWithDebInfo"
)

target_include_directories(optical_tracking_probe_calibration_tool_geometry_selection_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(optical_tracking_probe_calibration_tool_geometry_selection_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Test
    OpticalTrackingPlatformModuleLib
)

medicalpro_sync_probe_calibration_runtime(optical_tracking_probe_calibration_tool_geometry_selection_test)

add_test(
    NAME optical_tracking_probe_calibration_tool_geometry_selection_test
    COMMAND ${CMAKE_COMMAND} -E env
        "PATH=$<TARGET_FILE_DIR:optical_tracking_probe_calibration_tool_geometry_selection_test>;$<TARGET_FILE_DIR:medicalpro>;$<TARGET_FILE_DIR:Framework>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Network>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Widgets>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Gui>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>;$ENV{PATH}"
        $<TARGET_FILE:optical_tracking_probe_calibration_tool_geometry_selection_test>
)

set_tests_properties(optical_tracking_probe_calibration_tool_geometry_selection_test PROPERTIES
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:optical_tracking_probe_calibration_tool_geometry_selection_test>"
)

add_executable(innovation_1_point_selection_experiment_test
    Innovation1PointSelectionExperimentTest.cpp
)

target_include_directories(innovation_1_point_selection_experiment_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(innovation_1_point_selection_experiment_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME innovation_1_point_selection_experiment_test
    COMMAND innovation_1_point_selection_experiment_test
)

set_tests_properties(innovation_1_point_selection_experiment_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:innovation_1_point_selection_experiment_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(innovation_2_registration_experiment_test
    Innovation2RegistrationExperimentTest.cpp
)

target_include_directories(innovation_2_registration_experiment_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(innovation_2_registration_experiment_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME innovation_2_registration_experiment_test
    COMMAND innovation_2_registration_experiment_test
)

set_tests_properties(innovation_2_registration_experiment_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:innovation_2_registration_experiment_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(navigation_gate_strategy_test
    NavigationGateStrategyTest.cpp
)

target_include_directories(navigation_gate_strategy_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(navigation_gate_strategy_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    PointRegistrationPlatformModuleLib
)

add_test(
    NAME navigation_gate_strategy_test
    COMMAND navigation_gate_strategy_test
)

add_executable(innovation_3_gate_experiment_test
    Innovation3GateExperimentTest.cpp
)

target_include_directories(innovation_3_gate_experiment_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(innovation_3_gate_experiment_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME innovation_3_gate_experiment_test
    COMMAND innovation_3_gate_experiment_test
)

set_tests_properties(innovation_3_gate_experiment_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:innovation_3_gate_experiment_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(innovation_experiment_batch_runner_test
    InnovationExperimentBatchRunnerTest.cpp
)

target_include_directories(innovation_experiment_batch_runner_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(innovation_experiment_batch_runner_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME innovation_experiment_batch_runner_test
    COMMAND innovation_experiment_batch_runner_test
)

set_tests_properties(innovation_experiment_batch_runner_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:innovation_experiment_batch_runner_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(optical_tracking_quality_snapshot_test
    OpticalTrackingQualitySnapshotTest.cpp
)

set_target_properties(optical_tracking_quality_snapshot_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/MinSizeRel"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/RelWithDebInfo"
)

target_include_directories(optical_tracking_quality_snapshot_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(optical_tracking_quality_snapshot_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Test
    OpticalTrackingPlatformModuleLib
)

medicalpro_sync_probe_calibration_runtime(optical_tracking_quality_snapshot_test)

add_test(
    NAME optical_tracking_quality_snapshot_test
    COMMAND ${CMAKE_COMMAND} -E env
        "PATH=$<TARGET_FILE_DIR:optical_tracking_quality_snapshot_test>;$<TARGET_FILE_DIR:medicalpro>;$<TARGET_FILE_DIR:Framework>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Network>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Widgets>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Gui>;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>;$ENV{PATH}"
        $<TARGET_FILE:optical_tracking_quality_snapshot_test>
)

set_tests_properties(optical_tracking_quality_snapshot_test PROPERTIES
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:optical_tracking_quality_snapshot_test>"
)

add_executable(ankle_registration_utils_test
    AnkleRegistrationUtilsTest.cpp
)

target_include_directories(ankle_registration_utils_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(ankle_registration_utils_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Test
    RegistrationCorePlatformModuleLib
)

add_test(
    NAME ankle_registration_utils_test
    COMMAND ankle_registration_utils_test
)

set_tests_properties(ankle_registration_utils_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Widgets>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Gui>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>\;$<TARGET_FILE_DIR:VTK::CommonCore>\;$ENV{PATH}"
)

add_executable(ankle_registration_baseline_test
    AnkleRegistrationBaselineTest.cpp
)

target_include_directories(ankle_registration_baseline_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(ankle_registration_baseline_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Test
    RegistrationCorePlatformModuleLib
)

add_test(
    NAME ankle_registration_baseline_test
    COMMAND ankle_registration_baseline_test
)

set_tests_properties(ankle_registration_baseline_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Widgets>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Gui>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>\;$<TARGET_FILE_DIR:VTK::CommonCore>\;$ENV{PATH}"
)

add_executable(navigation_confidence_evaluator_test
    NavigationConfidenceEvaluatorTest.cpp
)

target_include_directories(navigation_confidence_evaluator_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(navigation_confidence_evaluator_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME navigation_confidence_evaluator_test
    COMMAND navigation_confidence_evaluator_test
)

set_tests_properties(navigation_confidence_evaluator_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:navigation_confidence_evaluator_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

add_executable(navigation_evaluation_service_test
    NavigationEvaluationServiceTest.cpp
)

target_include_directories(navigation_evaluation_service_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(navigation_evaluation_service_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME navigation_evaluation_service_test
    COMMAND navigation_evaluation_service_test
)

set_tests_properties(navigation_evaluation_service_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:navigation_evaluation_service_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Widgets>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Gui>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>\;$ENV{PATH}"
)

add_executable(navigation_evaluation_summary_formatter_test
    NavigationEvaluationSummaryFormatterTest.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/Navigation/navigation_evaluation_summary_formatter.cpp
)

target_include_directories(navigation_evaluation_summary_formatter_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(navigation_evaluation_summary_formatter_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Framework
)

add_test(
    NAME navigation_evaluation_summary_formatter_test
    COMMAND navigation_evaluation_summary_formatter_test
)

set_tests_properties(navigation_evaluation_summary_formatter_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:navigation_evaluation_summary_formatter_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>\;$ENV{PATH}"
)

set(MEDICALPRO_FRAMEWORK_UNIT_TEST_TARGETS
    platform_plugin_host_lifecycle_behavior_test
    platform_built_in_module_bootstrap_behavior_test
    platform_runtime_host_adapter_platform_fallback_test
    startup_bootstrap_controller_test
    startup_ui_coordinator_test
    platform_state_store_handoff_test
    platform_descriptor_loader_test
    platform_managed_plugin_plan_test
    platform_on_demand_activation_plan_test
    platform_plugin_policy_bridge_test
    platform_startup_trace_recorder_test
    startup_orchestrator_lifecycle_test
    startup_phase_registrar_contract_test
    platform_dependency_graph_test
    platform_facades_test
    platform_startup_coordinator_test
    platform_warmup_coordinator_test
    platform_ui_bridge_test
    ankle_case_workspace_repository_test
    innovation_experiment_repository_test
    ankle_planning_service_test
    navigation_gate_strategy_test
    navigation_confidence_evaluator_test
    navigation_evaluation_service_test
    navigation_evaluation_summary_formatter_test
    innovation_1_point_selection_experiment_test
    innovation_2_registration_experiment_test
    innovation_3_gate_experiment_test
    innovation_experiment_batch_runner_test
    registration_core_meshgpu_smoke_test
    real_case_asset_bootstrapper_test
    real_case_workspace_seed_coordinator_test
)

foreach(_medicalpro_framework_unit_test IN LISTS MEDICALPRO_FRAMEWORK_UNIT_TEST_TARGETS)
    medicalpro_sync_framework_runtime(${_medicalpro_framework_unit_test})
endforeach()
