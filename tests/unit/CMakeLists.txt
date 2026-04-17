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

add_executable(platform_descriptor_loader_test
    PlatformDescriptorLoaderTest.cpp
    ${CMAKE_SOURCE_DIR}/Framework/Platform/Kernel/PlatformDescriptorLoader.cpp
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

add_executable(platform_dependency_graph_test
    PlatformDependencyGraphTest.cpp
    ${CMAKE_SOURCE_DIR}/Framework/Platform/Kernel/PlatformDependencyGraph.cpp
    ${CMAKE_SOURCE_DIR}/Framework/Platform/Kernel/PlatformStateStore.cpp
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
