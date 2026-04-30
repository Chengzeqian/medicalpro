cmake_minimum_required(VERSION 3.16)

# 绂佺敤Qt Creator鐨凪aintenanceTool璀﹀憡
set(QT_CREATOR_SKIP_MAINTENANCE_TOOL_PROVIDER ON CACHE BOOL "Skip Qt Creator maintenance tool provider")

project(medicalpro VERSION 0.1 LANGUAGES CXX)

if(MSVC AND CMAKE_VS_PLATFORM_NAME STREQUAL "arm64")
    message(FATAL_ERROR "medicalpro 当前依赖的 Qt/VTK/ITK 运行环境为 x64 版本，请使用 x64 Kit 或 cmake -A x64 重新配置。")
endif()

# 寮哄埗浣跨敤 Release 鏋勫缓锛堝崟閰嶇疆鐢熸垚鍣級锛涘閰嶇疆鐢熸垚鍣ㄧ敱 IDE/鏋勫缓鍛戒护閫夋嫨 --config Release
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
if(CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Multi-config generator detected; please build with --config Release")
endif()

option(MEDICALPRO_ENABLE_CONSOLE "Enable console window for runtime log debugging" ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set encoding to UTF-8
if(MSVC)
    add_compile_options(/utf-8)
endif()

# ============================================================================
# Include CMake Modules
# ============================================================================
include(cmake/PluginMacros.cmake)

# Set Qt installation path if needed (uncomment and modify if necessary)
# set(CMAKE_PREFIX_PATH "C:/Qt/6.x.x/mingw_64" ${CMAKE_PREFIX_PATH})

set(MEDICALPRO_QT_COMPONENTS
    Core
    Widgets
    Network
    OpenGL
    PrintSupport
    UiTools
    Xml
    Svg
    Sql
    XmlPatterns
    Multimedia
    MultimediaWidgets
    WebEngine
    WebEngineWidgets
    WebChannel
    Qml
    LinguistTools
    Concurrent
)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS ${MEDICALPRO_QT_COMPONENTS})
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS ${MEDICALPRO_QT_COMPONENTS})

# === VTK 閰嶇疆锛堜娇鐢ㄩ」鐩唴/鐜鎻愪緵鐨?VTK锛?==
set(VTK_POSSIBLE_PATHS
    "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/VTK/VTK-install"
    "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/VTK/VTK-install/lib/cmake/vtk-9.4"
    "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/VTK"
    "$ENV{VTK_DIR}"
    "D:/VTK/VTK-install"
    "C:/VTK/VTK-install"
)

foreach(VTK_PATH ${VTK_POSSIBLE_PATHS})
    if(EXISTS "${VTK_PATH}")
        set(VTK_DIR "${VTK_PATH}")
        break()
    endif()
endforeach()

if(VTK_DIR)
    find_package(VTK QUIET PATHS ${VTK_DIR} NO_DEFAULT_PATH)
    if(VTK_FOUND)
        message(STATUS "Found VTK version: ${VTK_VERSION}")
        message(STATUS "VTK_DIR: ${VTK_DIR}")
        add_definitions(-DVTK_FOUND)
    endif()
endif()

# === ITK 閰嶇疆 ===
set(ITK_POSSIBLE_PATHS
    "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ITK/ITK-install"
    "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ITK/ITK-install/lib/cmake/ITK-5.4"
    "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ITK/ITK-build"
    "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ITK"
    "$ENV{ITK_DIR}"
    "D:/ITK/ITK-install"
    "C:/ITK/ITK-install"
)

foreach(ITK_PATH ${ITK_POSSIBLE_PATHS})
    if(EXISTS "${ITK_PATH}")
        set(ITK_DIR "${ITK_PATH}")
        break()
    endif()
endforeach()

if(ITK_DIR)
    find_package(ITK QUIET PATHS ${ITK_DIR} NO_DEFAULT_PATH)
    if(ITK_FOUND)
        message(STATUS "Found ITK version: ${ITK_VERSION}")
        message(STATUS "ITK_DIR: ${ITK_DIR}")
        include(${ITK_USE_FILE})
        add_definitions(-DITK_FOUND)
    else()
        message(WARNING "ITK not found at ${ITK_DIR}")
    endif()
else()
    message(WARNING "ITK directory not found in any search paths")
endif()

# CTK runtime build integration has been removed. Platform modules are the only supported host model.

set(TS_FILES medicalpro_zh_CN.ts)

# Framework sources
set(FRAMEWORK_SOURCES
    Framework/StartupOrchestrator.h
    Framework/StartupOrchestrator.cpp
    Framework/VTKWidgetPool.h
    Framework/VTKWidgetPool.cpp
    Framework/ErrorHandler.h
    Framework/ErrorHandler.cpp
    Framework/ConsoleLogBridge.h
    Framework/ConsoleLogBridge.cpp
    Framework/DatabaseManager.h
    Framework/DatabaseManager.cpp
    Framework/VTKGlobalInitializer.h
    Framework/VTKGlobalInitializer.cpp
    Framework/VTKContextValidator.h
    Framework/VTKContextValidator.cpp
    Framework/VTKWidgetFactory.h
    Framework/VTKWidgetFactory.cpp
    Framework/ImageDataTransfer.h
    Framework/ImageDataTransfer.cpp
    Framework/Core/MedicalDataStructures.h
    Framework/Core/MedicalDataStructures.cpp
    Framework/ResourceManagement/SingletonManager.h
    Framework/DataTypes/Point3D.h
    Framework/DataTypes/ImageSlice.h
    Framework/Segmentation/SegmentationTypes.h
    Framework/Segmentation/SegmentationService.h
    Framework/Registration/RegistrationTypes.h
    Framework/Registration/RegistrationService.h
    Framework/Navigation/ankle_navigation_types.h
    Framework/Navigation/ankle_case_workspace_repository.h
    Framework/Navigation/ankle_case_workspace_repository.cpp
    Framework/Navigation/ankle_planning_service.h
    Framework/Navigation/ankle_planning_service.cpp
    Framework/Navigation/navigation_confidence_evaluator.h
    Framework/Navigation/navigation_confidence_evaluator.cpp
    Framework/Navigation/navigation_evaluation_service.h
    Framework/Navigation/navigation_evaluation_service.cpp
    Framework/Logger.h
    Framework/Platform/Contracts/PlatformRuntimeTypes.h
    Framework/Platform/Contracts/PlatformPluginDescriptor.h
    Framework/Platform/Contracts/PlatformSnapshots.h
    Framework/Platform/Contracts/StartupShellSnapshot.h
    Framework/Platform/Kernel/PlatformDescriptorLoader.h
    Framework/Platform/Kernel/PlatformDescriptorLoader.cpp
    Framework/Platform/Kernel/PlatformDependencyGraph.h
    Framework/Platform/Kernel/PlatformDependencyGraph.cpp
    Framework/Platform/Kernel/PlatformManagedPluginPlan.h
    Framework/Platform/Kernel/PlatformManagedPluginPlan.cpp
    Framework/Platform/Kernel/PlatformRuntimeConfig.h
    Framework/Platform/Kernel/PlatformRuntimeConfig.cpp
    Framework/Platform/Kernel/PlatformPluginPolicyBridge.h
    Framework/Platform/Kernel/PlatformPluginPolicyBridge.cpp
    Framework/Platform/Kernel/PlatformStartupCoordinator.h
    Framework/Platform/Kernel/PlatformStartupCoordinator.cpp
    Framework/Platform/Kernel/PlatformOnDemandActivationPlan.h
    Framework/Platform/Kernel/PlatformOnDemandActivationPlan.cpp
    Framework/Platform/Kernel/PlatformOnDemandActivationService.h
    Framework/Platform/Kernel/PlatformOnDemandActivationService.cpp
    Framework/Platform/Kernel/PlatformWarmupCoordinator.h
    Framework/Platform/Kernel/PlatformWarmupCoordinator.cpp
    Framework/Platform/Kernel/PlatformStateStore.h
    Framework/Platform/Kernel/PlatformStateStore.cpp
    Framework/Platform/Bootstrap/StartupBootstrapController.h
    Framework/Platform/Bootstrap/StartupBootstrapController.cpp
    Framework/Platform/Bootstrap/startup_ui_coordinator.h
    Framework/Platform/Bootstrap/startup_ui_coordinator.cpp
    Framework/Platform/Bootstrap/startup_phase_registrar.h
    Framework/Platform/Bootstrap/startup_phase_registrar.cpp
    Framework/Platform/Bootstrap/platform_built_in_module_bootstrap.h
    Framework/Platform/Bootstrap/platform_built_in_module_bootstrap.cpp
    Framework/Platform/Bootstrap/PlatformStateStoreHandoff.h
    Framework/Platform/Bootstrap/PlatformStateStoreHandoff.cpp
    Framework/Platform/Diagnostics/PlatformRuntimeSnapshotCollector.h
    Framework/Platform/Diagnostics/PlatformRuntimeSnapshotCollector.cpp
    Framework/Platform/Kernel/platform_runtime_host_adapter.h
    Framework/Platform/Kernel/platform_runtime_host_adapter.cpp
    Framework/Platform/Diagnostics/PlatformDiagnosticsService.h
    Framework/Platform/Diagnostics/PlatformDiagnosticsService.cpp
    Framework/Platform/Diagnostics/PlatformPluginLifecycleAggregator.h
    Framework/Platform/Diagnostics/PlatformPluginLifecycleAggregator.cpp
    Framework/Platform/Diagnostics/PlatformLifecycleTraceRecorder.h
    Framework/Platform/Diagnostics/PlatformLifecycleTraceRecorder.cpp
    Framework/Platform/Contracts/PlatformFacadePorts.h
    Framework/Platform/Contracts/PlatformUiPorts.h
    Framework/Platform/Contracts/platform_runtime_host_ports.h
    Framework/Platform/Contracts/platform_module_ports.h
    Framework/Platform/UiBridge/CoreUiRuntimeStatusProvider.h
    Framework/Platform/UiBridge/CoreUiRuntimeStatusProvider.cpp
    Framework/Platform/UiBridge/NavigationPageServiceAccess.h
    Framework/Platform/UiBridge/NavigationPageServiceAccess.cpp
    Framework/Platform/LegacyAdapters/LegacyUserManagementAdapter.h
    Framework/Platform/LegacyAdapters/LegacyUserManagementAdapter.cpp
    Framework/Platform/LegacyAdapters/LegacyImagingAdapter.h
    Framework/Platform/LegacyAdapters/LegacyImagingAdapter.cpp
    Framework/Platform/LegacyAdapters/LegacyCoreUiRuntimeAdapter.h
    Framework/Platform/LegacyAdapters/LegacyCoreUiRuntimeAdapter.cpp
    Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.h
    Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.cpp
    Framework/Platform/LegacyAdapters/LegacyNavigationPageServiceAdapter.h
    Framework/Platform/LegacyAdapters/LegacyNavigationPageServiceAdapter.cpp
    Framework/Platform/Facades/IdentityAppService.h
    Framework/Platform/Facades/IdentityAppService.cpp
    Framework/Platform/Facades/ImagingAppService.h
    Framework/Platform/Facades/ImagingAppService.cpp
    Framework/Platform/Facades/NavigationAppService.h
    Framework/Platform/Facades/NavigationAppService.cpp
    Framework/Platform/Kernel/platform_service_registry.h
    Framework/Platform/Kernel/platform_service_registry.cpp
    Framework/Platform/Kernel/platform_event_bus.h
    Framework/Platform/Kernel/platform_event_bus.cpp
    Framework/Platform/Kernel/platform_plugin_host.h
    Framework/Platform/Kernel/platform_plugin_host.cpp
)

# ============================================================================
# Framework Library Configuration (Requirement 1.1, 1.5)
# ============================================================================
# Framework is a SHARED library that provides:
# - VTK initialization and management utilities
# - platform runtime hosting and diagnostics
# - Common data structures and utilities
# - Base classes for UI components
#
# Design Decision (ADR-001): Why SHARED library?
# - Ensures single instance of shared runtime services
# - Reduces total binary size by sharing code across main app and plugins
# - Allows unified updates to Framework without recompiling all plugins
# - Enables proper symbol export/import for cross-module usage
# ============================================================================

add_library(Framework SHARED ${FRAMEWORK_SOURCES})

# Set Framework library properties
set_target_properties(Framework PROPERTIES
    OUTPUT_NAME "Framework"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
    # Enable symbol export for Windows DLL
    WINDOWS_EXPORT_ALL_SYMBOLS OFF  # We use explicit FRAMEWORK_EXPORT macros
)

# Framework library compile definitions for DLL export
# FRAMEWORK_LIBRARY is defined when building Framework.dll itself
# This enables __declspec(dllexport) in FrameworkExport.h
target_compile_definitions(Framework PRIVATE FRAMEWORK_LIBRARY)
if(MSVC AND CMAKE_VS_PLATFORM_NAME STREQUAL "arm64")
    target_compile_options(Framework PRIVATE /FI"${CMAKE_CURRENT_SOURCE_DIR}/Framework/windows_sdk_compat.h")
endif()

# ============================================================================
# Qt Dependencies (PUBLIC - plugins need Qt types in Framework interfaces)
# ============================================================================
target_link_libraries(Framework PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Sql
)

# ============================================================================
# VTK Dependencies (PUBLIC - Requirement 1.3)
# ============================================================================
# Linking Strategy: PUBLIC
# Reason: Plugins need VTK types (vtkSmartPointer, vtkRenderer, etc.) that
#         appear in Framework interfaces and plugin Widget implementations.
#         PUBLIC linking ensures plugins can access VTK headers and libraries
#         through Framework without explicit VTK linking in each plugin.
# ============================================================================
if(VTK_FOUND)
    target_include_directories(Framework PUBLIC ${VTK_INCLUDE_DIRS})
    target_link_libraries(Framework PUBLIC ${VTK_LIBRARIES})
    message(STATUS "Framework: VTK linked as PUBLIC (plugins inherit VTK access)")
endif()

if(ITK_FOUND)
    target_link_libraries(Framework PUBLIC ${ITK_LIBRARIES})
endif()

# ============================================================================
# Framework Public Headers
# ============================================================================
set(_framework_public_includes
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Framework
)
list(REMOVE_DUPLICATES _framework_public_includes)
target_include_directories(Framework PUBLIC ${_framework_public_includes})

# 鎻掍欢鐩綍鏀惧湪 Framework 瀹氫箟涔嬪悗锛岄伩鍏?PCH 澶嶇敤鏃舵壘涓嶅埌 Framework 鐩爣
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Plugins")
    add_subdirectory(Plugins)
    message(STATUS "Added Plugins subdirectory")
endif()

if(TARGET UserManagementPlatformModuleLib)
    target_link_libraries(Framework PRIVATE UserManagementPlatformModuleLib)
    target_compile_definitions(Framework PRIVATE MEDICALPRO_HAS_USER_MANAGEMENT_PLATFORM_MODULE)
endif()

if(TARGET DicomViewerPlatformModuleLib)
    target_link_libraries(Framework PRIVATE DicomViewerPlatformModuleLib)
    target_compile_definitions(Framework PRIVATE MEDICALPRO_HAS_DICOM_VIEWER_PLATFORM_MODULE)
endif()

if(TARGET FourViewDisplayPlatformModuleLib)
    target_link_libraries(Framework PRIVATE FourViewDisplayPlatformModuleLib)
    target_compile_definitions(Framework PRIVATE MEDICALPRO_HAS_FOUR_VIEW_DISPLAY_PLATFORM_MODULE)
endif()

if(TARGET RegistrationCorePlatformModuleLib)
    target_link_libraries(Framework PRIVATE RegistrationCorePlatformModuleLib)
    target_compile_definitions(Framework PRIVATE MEDICALPRO_HAS_REGISTRATION_CORE_PLATFORM_MODULE)
endif()

if(TARGET Registration2D3DPlatformModuleLib)
    target_link_libraries(Framework PRIVATE Registration2D3DPlatformModuleLib)
    target_compile_definitions(Framework PRIVATE MEDICALPRO_HAS_REGISTRATION_2D3D_PLATFORM_MODULE)
endif()

if(TARGET OpticalTrackingPlatformModuleLib)
    target_link_libraries(Framework PRIVATE OpticalTrackingPlatformModuleLib)
    target_compile_definitions(Framework PRIVATE MEDICALPRO_HAS_OPTICAL_TRACKING_PLATFORM_MODULE)
endif()

if(TARGET OpticalRegistrationPlatformModuleLib)
    target_link_libraries(Framework PRIVATE OpticalRegistrationPlatformModuleLib)
    target_compile_definitions(Framework PRIVATE MEDICALPRO_HAS_OPTICAL_REGISTRATION_PLATFORM_MODULE)
endif()

if(TARGET PointRegistrationPlatformModuleLib)
    target_link_libraries(Framework PRIVATE PointRegistrationPlatformModuleLib)
    target_compile_definitions(Framework PRIVATE MEDICALPRO_HAS_POINT_REGISTRATION_PLATFORM_MODULE)
endif()

if(TARGET InstrumentManagementPlatformModuleLib)
    target_link_libraries(Framework PRIVATE InstrumentManagementPlatformModuleLib)
    target_compile_definitions(Framework PRIVATE MEDICALPRO_HAS_INSTRUMENT_MANAGEMENT_PLATFORM_MODULE)
endif()

if(TARGET BoneSegmentationPlatformModuleLib)
    target_link_libraries(Framework PRIVATE BoneSegmentationPlatformModuleLib)
    target_compile_definitions(Framework PRIVATE MEDICALPRO_HAS_BONE_SEGMENTATION_PLATFORM_MODULE)
endif()

# ============================================================================
# UI forms registration (for Qt Creator visibility and reuse)
# ============================================================================
set(UI_FORMS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/UI/Forms")
set(NEWPAGES_UI_FILES
    ${UI_FORMS_DIR}/WelcomePage.ui
    ${UI_FORMS_DIR}/SystemSettingsPage.ui
    ${UI_FORMS_DIR}/NavigationPage.ui
    ${UI_FORMS_DIR}/ModuleSelectionPage.ui
    ${UI_FORMS_DIR}/ManagementPage.ui
    ${UI_FORMS_DIR}/LoginPage.ui
    ${UI_FORMS_DIR}/DashboardPage.ui
)

# Expose UI forms to Qt Creator even though they are compiled in NewPagesLib
add_custom_target(ui_forms SOURCES ${NEWPAGES_UI_FILES})

# ============================================================================
# 鏂癠I妗嗘灦 - NewPages (绠€鍖栫殑UI灞?
# ============================================================================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/UI/NewPages")
    add_subdirectory(UI/NewPages)
    message(STATUS "Added NewPages UI Framework subdirectory")
endif()

set(PROJECT_SOURCES
        main.cpp

        # === 鏃I鍏ュ彛锛堜繚鐣欑敤浜庢笎杩涜縼绉伙紝鍚庣画鍙垹闄わ級 ===
        UI/MainInterfaceWidget.h
        UI/MainInterfaceWidget.cpp
        UI/MainInterfaceFactory.h
        UI/MainInterfaceFactory.cpp
        UI/StartupShell.h
        UI/StartupShell.cpp

        # === 瀵硅瘽妗嗘ā鍧?===
        UI/Dialogs/InstrumentPreviewDialog.h
        UI/Dialogs/InstrumentPreviewDialog.cpp
        UI/Dialogs/VTKTestDialog.h
        UI/Dialogs/VTKTestDialog.cpp

        # === 淇濈暀鐨刉idget锛堥潪鎻掍欢鐩稿叧鎴栨湁鐗规畩鐢ㄩ€旓級===
        UI/Widgets/Instrument3DPreviewWidget.h
        UI/Widgets/Instrument3DPreviewWidget.cpp
        UI/Widgets/Navigation3DViewWidget.h
        UI/Widgets/Navigation3DViewWidget.cpp

        # === 鏂癠I妗嗘灦閫氳繃NewPagesLib闈欐€佸簱閾炬帴 ===
        # UI/NewPages/* 鏂囦欢鐢?add_subdirectory(UI/NewPages) 澶勭悊

        # Framework sources now in separate library
        ${TS_FILES}
        resources.qrc
)

# 娣诲姞鎻掍欢鎺ュ彛澶存枃浠讹紝浣縌t MOC鑳藉涓哄畠浠敓鎴愬厓瀵硅薄浠ｇ爜
# 杩欏浜庡湪涓荤▼搴忎腑杩炴帴鎻掍欢淇″彿鏄繀闇€鐨?
list(APPEND PROJECT_SOURCES
    Plugins/InstrumentManagement/InstrumentManagementService.h
)
message(STATUS "Added plugin service interface headers for MOC processing")

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(medicalpro
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET medicalpro APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation

    qt_create_translation(QM_FILES ${PROJECT_SOURCES} ${TS_FILES})
else()
    if(ANDROID)
        add_library(medicalpro SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(medicalpro
            ${PROJECT_SOURCES}
        )
    endif()

    qt5_create_translation(QM_FILES ${PROJECT_SOURCES} ${TS_FILES})
endif()

# ============================================================================
# Main Application Linking (Requirement 1.3)
# ============================================================================
target_link_libraries(medicalpro PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Concurrent
    Qt${QT_VERSION_MAJOR}::PrintSupport
    Framework
)

# 閾炬帴鏂癠I妗嗘灦搴?
if(TARGET NewPagesLib)
    target_link_libraries(medicalpro PRIVATE NewPagesLib)
    message(STATUS "Main app: NewPagesLib linked")
endif()

configure_platform_descriptor_runtime_copy(medicalpro)

# ============================================================================
# VTK Linking for Main App (PRIVATE - Requirement 1.3)
# ============================================================================
# Linking Strategy: PRIVATE
# Reason: Main app accesses VTK only through Framework's public interfaces.
#         Framework already links VTK as PUBLIC, so main app gets VTK
#         transitively. Explicit PRIVATE linking here is for direct VTK usage
#         in main.cpp (e.g., VTKGlobalInitializer).
# ============================================================================
if(VTK_FOUND)
    target_include_directories(medicalpro PRIVATE ${VTK_INCLUDE_DIRS})
    target_link_libraries(medicalpro PRIVATE ${VTK_LIBRARIES})
    message(STATUS "Main app: VTK linked as PRIVATE (direct usage in main.cpp)")
endif()

# ============================================================================
# ITK Linking for Main App (NOT LINKED - Requirement 1.3)
# ============================================================================
# Linking Strategy: NOT LINKED to main app
# Reason: ITK is only used in specific plugins (DicomViewer)
#         for image processing algorithms. Main app has no direct ITK usage.
#         Each plugin that needs ITK links it PRIVATE.
# Note: Keeping this section for reference, but ITK should NOT be linked here.
# ============================================================================
if(ITK_FOUND)
    # ITK is NOT linked to main app - only to plugins that need it
    message(STATUS "Main app: ITK NOT linked (only used in specific plugins)")
    
    # If main app ever needs ITK directly, uncomment below:
    # target_include_directories(medicalpro PRIVATE ${ITK_INCLUDE_DIRS})
    # target_link_libraries(medicalpro PRIVATE ${ITK_LIBRARIES})
    # target_compile_definitions(medicalpro PRIVATE
    #     ITK_LEGACY_SILENT
    #     ITK_IO_FACTORY_REGISTER_MANAGER
    # )
endif()

# ============================================================================
# Main App Compile Definitions
# ============================================================================
target_compile_definitions(medicalpro PRIVATE
    MEDICALPRO_MAIN_APPLICATION
)

# 娣诲姞椤圭洰鏍圭洰褰曞埌鍖呭惈璺緞锛屼娇鎻掍欢澶存枃浠跺彲浠ヨ鎵惧埌
target_include_directories(medicalpro PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

message(STATUS "Main app: platform runtime host only")



# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.medicalpro)
endif()
set(MEDICALPRO_WIN32_EXECUTABLE TRUE)
if(WIN32 AND MEDICALPRO_ENABLE_CONSOLE)
    set(MEDICALPRO_WIN32_EXECUTABLE FALSE)
endif()
set_target_properties(medicalpro PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE ${MEDICALPRO_WIN32_EXECUTABLE}
    ENABLE_EXPORTS TRUE  # Export symbols for plugins to use
)

include(GNUInstallDirs)
install(TARGETS medicalpro
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(medicalpro)
endif()

# 鎻掍欢鐩綍宸茬粡鍦–TK閰嶇疆閮ㄥ垎娣诲姞浜嗭紝杩欓噷涓嶉渶瑕侀噸澶嶆坊鍔?

# === 娴嬭瘯閰嶇疆 ===
option(BUILD_TESTING "Build tests" OFF)
if(BUILD_TESTING)
    enable_testing()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
        add_subdirectory(tests)
        message(STATUS "Testing enabled")
    else()
        message(WARNING "BUILD_TESTING=ON but tests directory not found, skipping add_subdirectory(tests)")
    endif()
endif()

add_custom_command(TARGET medicalpro POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DOUTPUT_DIR="$<TARGET_FILE_DIR:medicalpro>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/remove_stale_runtime_host_artifacts.cmake"
    COMMENT "Removing stale legacy runtime host artifacts"
)

add_custom_command(TARGET medicalpro POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DOUTPUT_DIR="$<TARGET_FILE_DIR:medicalpro>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/remove_stale_legacy_plugin_artifacts.cmake"
    COMMENT "Removing stale legacy plugin artifacts"
)

# Qt SQL driver (QSQLITE)
if(WIN32)
    set(_qt_qmake_executable "")
    get_target_property(_qt_qmake_executable Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
    if(NOT _qt_qmake_executable)
        get_target_property(_qt_qmake_executable Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION_RELEASE)
    endif()

    if(_qt_qmake_executable)
        get_filename_component(_qt_bin_dir "${_qt_qmake_executable}" DIRECTORY)
        get_filename_component(_qt_prefix_dir "${_qt_bin_dir}" DIRECTORY)
        set(_qt_plugins_dir "${_qt_prefix_dir}/plugins")
        set(_qt_qsqlite_plugin "${_qt_plugins_dir}/sqldrivers/qsqlite.dll")
        set(_qt_sql_dll "${_qt_bin_dir}/Qt${QT_VERSION_MAJOR}Sql.dll")

        if(EXISTS "${_qt_qsqlite_plugin}")
            add_custom_command(TARGET medicalpro POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_FILE_DIR:medicalpro>/sqldrivers"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_qt_qsqlite_plugin}"
                    "$<TARGET_FILE_DIR:medicalpro>/sqldrivers/qsqlite.dll"
                COMMENT "Copying Qt SQL driver plugin (qsqlite.dll) to output directory"
            )
        else()
            message(WARNING "Qt SQL driver plugin not found at: ${_qt_qsqlite_plugin} (QSQLITE may fail to load)")
        endif()

        if(EXISTS "${_qt_sql_dll}")
            add_custom_command(TARGET medicalpro POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_qt_sql_dll}"
                    "$<TARGET_FILE_DIR:medicalpro>"
                COMMENT "Copying Qt Sql runtime DLL to output directory"
            )
        else()
            message(WARNING "Qt Sql runtime DLL not found at: ${_qt_sql_dll} (QSQLITE plugin may fail to load)")
        endif()
    else()
        message(WARNING "Qt qmake target not found; cannot auto-deploy QSQLITE driver plugin.")
    endif()
endif()

if(WIN32 AND VTK_FOUND)
    set(LOCAL_VTK_BIN_DIR "")

    # ???VTK ??/???????
    # 1) VTK_DIR ??????? bin/<config> ? ../../bin/<config>
    # 2) ?????? ThirdParty/VTK/VTK-install/bin
    set(_vtk_bin_candidates)
    if(DEFINED VTK_DIR)
        list(APPEND _vtk_bin_candidates
            "${VTK_DIR}/bin/Release"
            "${VTK_DIR}/bin/RelWithDebInfo"
            "${VTK_DIR}/bin/Debug"
            "${VTK_DIR}/bin"
            "${VTK_DIR}/../bin/Release"
            "${VTK_DIR}/../bin/RelWithDebInfo"
            "${VTK_DIR}/../bin/Debug"
            "${VTK_DIR}/../bin"
            "${VTK_DIR}/../../bin/Release"
            "${VTK_DIR}/../../bin/RelWithDebInfo"
            "${VTK_DIR}/../../bin/Debug"
            "${VTK_DIR}/../../bin"
        )
    endif()
    list(APPEND _vtk_bin_candidates
        "${CMAKE_SOURCE_DIR}/ThirdParty/VTK/VTK-install/bin"
    )
    set(_copied_vtk_runtime FALSE)
    foreach(_cand ${_vtk_bin_candidates})
        if(EXISTS "${_cand}")
            file(GLOB _vtk_runtime_dlls
                "${_cand}/vtk*.dll"
                "${_cand}/tbb*.dll"
            )
            if(_vtk_runtime_dlls)
                add_custom_command(TARGET medicalpro POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        ${_vtk_runtime_dlls}
                        "$<TARGET_FILE_DIR:medicalpro>"
                    COMMENT "Copying VTK runtime DLLs from ${_cand} to output directory"
                )
                set(_copied_vtk_runtime TRUE)
            endif()
        endif()
    endforeach()

    if(NOT _copied_vtk_runtime)
        message(WARNING "Could not find any vtk*.dll in expected VTK bin directories (VTK_DIR=${VTK_DIR})")
    endif()
endif()

# ITK runtime DLLs (needed by FourViewDisplay and other plugins)
if(WIN32 AND ITK_FOUND)
    set(_itk_bin_candidates)
    if(DEFINED ITK_DIR)
        get_filename_component(_itk_prefix "${ITK_DIR}/../../.." ABSOLUTE)
        list(APPEND _itk_bin_candidates
            "${_itk_prefix}/bin"
            "${_itk_prefix}/bin/Release"
            "${_itk_prefix}/bin/RelWithDebInfo"
            "${_itk_prefix}/bin/Debug"
            "${ITK_DIR}/bin"
            "${ITK_DIR}/../bin"
            "${ITK_DIR}/../../bin"
            "${ITK_DIR}/../../../bin"
            "${ITK_DIR}/bin/Release"
            "${ITK_DIR}/bin/RelWithDebInfo"
            "${ITK_DIR}/bin/Debug"
            "${ITK_DIR}/../bin/Release"
            "${ITK_DIR}/../bin/RelWithDebInfo"
            "${ITK_DIR}/../bin/Debug"
            "${ITK_DIR}/../../bin/Release"
            "${ITK_DIR}/../../bin/RelWithDebInfo"
            "${ITK_DIR}/../../bin/Debug"
        )
    endif()
    # Project-local ITK install (manual build)
    list(APPEND _itk_bin_candidates
        "${CMAKE_SOURCE_DIR}/ThirdParty/ITK/ITK-install/bin"
    )
    set(_itk_runtime_bin "")
    foreach(_cand ${_itk_bin_candidates})
        if(EXISTS "${_cand}")
            file(GLOB _itk_probe "${_cand}/*.dll")
            if(_itk_probe)
                set(_itk_runtime_bin "${_cand}")
                break()
            endif()
        endif()
    endforeach()
    if(_itk_runtime_bin)
        add_custom_command(TARGET medicalpro POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                -Dsrc="${_itk_runtime_bin}"
                -Ddst="$<TARGET_FILE_DIR:medicalpro>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/scripts/copy_all_dlls.cmake"
            COMMENT "Copying ITK runtime DLLs from ${_itk_runtime_bin} to output directory"
        )
    else()
        message(WARNING "ITK runtime bin directory not found (ITK_DIR=${ITK_DIR}); ITK DLLs may be missing at runtime.")
    endif()
endif()

# MSVC CRT runtime (vcruntime/msvcp/vccorlib/concrt)
if(MSVC)
    set(_vc_redist_candidates)
    if(DEFINED ENV{VCToolsRedistDir})
        list(APPEND _vc_redist_candidates
            "$ENV{VCToolsRedistDir}/x64/Microsoft.VC142.CRT"
            "$ENV{VCToolsRedistDir}/onecore/x64/Microsoft.VC142.CRT"
            "$ENV{VCToolsRedistDir}/x64/Microsoft.VC143.CRT"
            "$ENV{VCToolsRedistDir}/onecore/x64/Microsoft.VC143.CRT"
        )
    endif()
    if(DEFINED ENV{VSINSTALLDIR})
        list(APPEND _vc_redist_candidates
            "$ENV{VSINSTALLDIR}/VC/Redist/MSVC/*/x64/Microsoft.VC142.CRT"
            "$ENV{VSINSTALLDIR}/VC/Redist/MSVC/*/onecore/x64/Microsoft.VC142.CRT"
            "$ENV{VSINSTALLDIR}/VC/Redist/MSVC/*/x64/Microsoft.VC143.CRT"
            "$ENV{VSINSTALLDIR}/VC/Redist/MSVC/*/onecore/x64/Microsoft.VC143.CRT"
        )
    endif()
    list(APPEND _vc_redist_candidates
        "C:/Program Files (x86)/Microsoft Visual Studio/2019/BuildTools/VC/Redist/MSVC/*/x64/Microsoft.VC142.CRT"
        "C:/Program Files (x86)/Microsoft Visual Studio/2019/BuildTools/VC/Redist/MSVC/*/onecore/x64/Microsoft.VC142.CRT"
        "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Redist/MSVC/*/x64/Microsoft.VC143.CRT"
        "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Redist/MSVC/*/onecore/x64/Microsoft.VC143.CRT"
    )

    set(_crt_found FALSE)
    foreach(_cand ${_vc_redist_candidates})
        file(GLOB _crt_dlls
            "${_cand}/vcruntime140*.dll"
            "${_cand}/msvcp140*.dll"
            "${_cand}/vccorlib140*.dll"
            "${_cand}/concrt140*.dll"
        )
        if(_crt_dlls)
            add_custom_command(TARGET medicalpro POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${_crt_dlls}
                    "$<TARGET_FILE_DIR:medicalpro>"
                COMMENT "Copying MSVC CRT runtime DLLs from ${_cand} to output directory"
            )
            set(_crt_found TRUE)
            break()
        endif()
    endforeach()

    if(NOT _crt_found)
        message(WARNING "MSVC CRT DLLs not copied; install VC++ 2015-2022 Redistributable (x64) or set VCToolsRedistDir.")
    endif()
endif()

# ============================================================================
# 鏂癠I妗嗘灦鐙珛娴嬭瘯绋嬪簭 (鍙€?
# ============================================================================
option(BUILD_NEWUI_TEST "Build standalone test for new UI framework" OFF)

if(BUILD_NEWUI_TEST AND TARGET NewPagesLib)
    if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
        qt_add_executable(medicalpro_newui
            MANUAL_FINALIZATION
            UI/NewPages/main_new.cpp
        )
    else()
        add_executable(medicalpro_newui
            UI/NewPages/main_new.cpp
        )
    endif()

    target_link_libraries(medicalpro_newui PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        NewPagesLib
        Framework
    )

    # VTK鏀寔
    if(VTK_FOUND)
        target_link_libraries(medicalpro_newui PRIVATE ${VTK_LIBRARIES})
        target_include_directories(medicalpro_newui PRIVATE ${VTK_INCLUDE_DIRS})
    endif()

    set_target_properties(medicalpro_newui PROPERTIES
        WIN32_EXECUTABLE ${MEDICALPRO_WIN32_EXECUTABLE}
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
    )

    if(QT_VERSION_MAJOR EQUAL 6)
        qt_finalize_executable(medicalpro_newui)
    endif()

    message(STATUS "New UI test executable 'medicalpro_newui' configured")
endif()

# ============================================================================
# Copy config files to build output directory
# ============================================================================
set(MEDICALPRO_PRODUCT_CONFIG_FILES
    platform_runtime.json
    segmentation_config.json
)

set(MEDICALPRO_RUNTIME_CONFIG_FILES
    ${MEDICALPRO_PRODUCT_CONFIG_FILES}
)

function(copy_runtime_config_file target_name config_file)
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>/config"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/config/${config_file}"
            "$<TARGET_FILE_DIR:${target_name}>/config/${config_file}"
        COMMENT "Copying config/${config_file} to ${target_name} runtime directory"
    )
endfunction()

if(EXISTS "${CMAKE_SOURCE_DIR}/config")
    foreach(_config_file ${MEDICALPRO_RUNTIME_CONFIG_FILES})
        if(EXISTS "${CMAKE_SOURCE_DIR}/config/${_config_file}")
            copy_runtime_config_file(medicalpro "${_config_file}")

            if(TARGET medicalpro_newui)
                copy_runtime_config_file(medicalpro_newui "${_config_file}")
            endif()
        endif()
    endforeach()

    message(STATUS "Runtime config files will be copied to build output directory")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/data")
    add_custom_command(TARGET medicalpro POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:medicalpro>/data"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/data"
            "$<TARGET_FILE_DIR:medicalpro>/data"
        COMMENT "Copying data files to build directory..."
    )

    if(TARGET medicalpro_newui)
        add_custom_command(TARGET medicalpro_newui POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:medicalpro_newui>/data"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/data"
                "$<TARGET_FILE_DIR:medicalpro_newui>/data"
            COMMENT "Copying data files for medicalpro_newui..."
        )
    endif()

    message(STATUS "Data files will be copied to build output directory")
endif()

# Clean legacy plugin artifacts that can break plugin discovery order in runtime output
set(MEDICALPRO_LEGACY_PLUGIN_FILES
    SlicerBridge.dll
    SlicerBridge.lib
    SlicerBridge.exp
    SlicerBridge.pdb
    Segmentation.dll
    Segmentation.lib
    Segmentation.exp
    Segmentation.pdb
    SegmentationCore.dll
    SegmentationCore.lib
    SegmentationCore.exp
    SegmentationCore.pdb
)

foreach(_legacy_plugin_file ${MEDICALPRO_LEGACY_PLUGIN_FILES})
    add_custom_command(TARGET medicalpro POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -f "$<TARGET_FILE_DIR:medicalpro>/plugins/${_legacy_plugin_file}"
        COMMENT "Removing legacy plugin artifact: ${_legacy_plugin_file}"
    )
endforeach()


