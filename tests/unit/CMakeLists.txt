function(medicalpro_sync_framework_runtime target_name)
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Framework>"
            "$<TARGET_FILE_DIR:${target_name}>/$<TARGET_FILE_NAME:Framework>"
        COMMENT "Syncing Framework runtime for ${target_name}"
    )
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
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:navigation_evaluation_service_test>\;$<TARGET_FILE_DIR:Framework>\;$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
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
    ankle_planning_service_test
    navigation_confidence_evaluator_test
    navigation_evaluation_service_test
)

foreach(_medicalpro_framework_unit_test IN LISTS MEDICALPRO_FRAMEWORK_UNIT_TEST_TARGETS)
    medicalpro_sync_framework_runtime(${_medicalpro_framework_unit_test})
endforeach()
