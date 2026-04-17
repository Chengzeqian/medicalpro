cmake_minimum_required(VERSION 3.16)

# 绂佺敤Qt Creator鐨凪aintenanceTool璀﹀憡
set(QT_CREATOR_SKIP_MAINTENANCE_TOOL_PROVIDER ON CACHE BOOL "Skip Qt Creator maintenance tool provider")

project(medicalpro VERSION 0.1 LANGUAGES CXX)

if(MSVC AND CMAKE_VS_PLATFORM_NAME STREQUAL "arm64")
    message(FATAL_ERROR "medicalpro 当前依赖的 Qt/CTK/VTK/ITK 为 x64 版本，请使用 x64 Kit 或 cmake -A x64 重新配置。")
endif()

# 寮哄埗浣跨敤 Release 鏋勫缓锛堝崟閰嶇疆鐢熸垚鍣級锛涘閰嶇疆鐢熸垚鍣ㄧ敱 IDE/鏋勫缓鍛戒护閫夋嫨 --config Release
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
if(CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Multi-config generator detected; please build with --config Release")
endif()

option(MEDICALPRO_ENABLE_CONSOLE "Enable console window for runtime log debugging" ON)

set(MEDICALPRO_CTK_INCLUDE_DIRS "")
macro(medicalpro_collect_ctk_includes)
    set(_medicalpro_ctk_dirs "")
    if(DEFINED CTK_INCLUDE_DIRS)
        list(APPEND _medicalpro_ctk_dirs ${CTK_INCLUDE_DIRS})
    endif()
    if(DEFINED CTK_DIR)
        get_filename_component(_ctk_dir_abs "${CTK_DIR}" ABSOLUTE)
        foreach(_candidate
            "${_ctk_dir_abs}"
            "${_ctk_dir_abs}/CTK"
            "${_ctk_dir_abs}/CTK/Libs"
            "${_ctk_dir_abs}/CTK/Libs/PluginFramework"
            "${_ctk_dir_abs}/CTK/Plugins"
            "${_ctk_dir_abs}/CTK-build"
            "${_ctk_dir_abs}/CTK-build/Libs"
            "${_ctk_dir_abs}/CTK-build/Libs/PluginFramework"
            "${_ctk_dir_abs}/Libs/PluginFramework"
            "${_ctk_dir_abs}/../CTK"
            "${_ctk_dir_abs}/../CTK/Libs"
            "${_ctk_dir_abs}/../CTK/Libs/PluginFramework"
            "${_ctk_dir_abs}/../CTK-build"
            "${_ctk_dir_abs}/../CTK-build/Libs"
            "${_ctk_dir_abs}/../CTK-build/Libs/PluginFramework"
        )
            if(EXISTS "${_candidate}")
                list(APPEND _medicalpro_ctk_dirs "${_candidate}")
            endif()
        endforeach()
    endif()
    list(REMOVE_DUPLICATES _medicalpro_ctk_dirs)
    set(MEDICALPRO_CTK_INCLUDE_DIRS "${_medicalpro_ctk_dirs}")
endmacro()

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

# === CTK Plugin Framework 閰嶇疆 ===
option(ENABLE_CTK_PLUGIN_FRAMEWORK "Enable CTK Plugin Framework support" ON)

if(ENABLE_CTK_PLUGIN_FRAMEWORK)
    # 澶栭儴 CTK 鏌ユ壘
    set(CTK_POSSIBLE_PATHS
        "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/CTK/CTK_install"
        "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/CTK/CTK-build"
        "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/CTK"
        "$ENV{CTK_DIR}"
        "D:/CTK/CTK_install"
        "C:/CTK/CTK_install"
    )

    foreach(CTK_PATH ${CTK_POSSIBLE_PATHS})
        if(EXISTS "${CTK_PATH}")
            set(CTK_DIR "${CTK_PATH}")
            break()
        endif()
    endforeach()

    if(CTK_DIR)
        message(STATUS "Searching for CTK in: ${CTK_DIR}")
        set(CTK_INSTALL_DIR "${CTK_DIR}")

        # Prefer importing CTK targets directly from the install tree to avoid
        # CTKConfig.cmake forcing optional dependencies (PythonQt/QtSOAP/qRestAPI/...).
        set(_ctk_config_dir "${CTK_INSTALL_DIR}/lib/ctk-0.1/CMake")
        set(_ctk_exports_file "${_ctk_config_dir}/CTKExports.cmake")
        if(EXISTS "${_ctk_exports_file}")
            include("${_ctk_exports_file}")
            set(CTK_FOUND TRUE)
            set(CTK_USE_FILE "${_ctk_config_dir}/UseCTK.cmake")
            set(CTK_INCLUDE_DIRS "${CTK_INSTALL_DIR}/include/ctk-0.1")

            add_definitions(-DCTK_PLUGIN_FRAMEWORK -DCTK_PLUGIN_FRAMEWORK_EXPORT)
            set(CTK_LIBRARIES CTKCore CTKWidgets CTKPluginFramework)
            message(STATUS "CTK Plugin Framework enabled successfully (ThirdParty CTK install)")
            medicalpro_collect_ctk_includes()
        else()
            set(CTK_FOUND FALSE)
            message(WARNING "CTK install exports not found at: ${_ctk_exports_file}")
        endif()
    else()
        set(CTK_FOUND FALSE)
        message(WARNING "CTK not found in any search paths")
    endif()

    if(CTK_FOUND)
        set(_ctk_pf_header_found FALSE)
        foreach(_dir ${MEDICALPRO_CTK_INCLUDE_DIRS})
            if(EXISTS "${_dir}/ctkPluginFrameworkFactory.h")
                set(_ctk_pf_header_found TRUE)
                break()
            endif()
        endforeach()
        if(NOT _ctk_pf_header_found)
            message(FATAL_ERROR "CTK PluginFramework headers not found. Ensure your CTK build enables PluginFramework.")
        endif()

        set(_ctk_pf_export_found FALSE)
        foreach(_dir ${MEDICALPRO_CTK_INCLUDE_DIRS})
            if(EXISTS "${_dir}/ctkPluginFrameworkExport.h")
                set(_ctk_pf_export_found TRUE)
                break()
            endif()
        endforeach()
        if(NOT _ctk_pf_export_found)
            message(FATAL_ERROR "ctkPluginFrameworkExport.h not found. CTK PluginFramework appears disabled in the selected CTK build.")
        endif()
    endif()
else()
    set(CTK_FOUND FALSE)
    message(STATUS "CTK Plugin Framework disabled by user option")
endif()

set(TS_FILES medicalpro_zh_CN.ts)

# Framework sources
set(FRAMEWORK_SOURCES
    Framework/CTKManager.h
    Framework/CTKManager.cpp
    Framework/PluginLoadPolicy.h
    Framework/PluginLoadPolicy.cpp
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
    Framework/Logger.h
)

# CTK澧炲己缁勪欢婧愭枃浠讹紙鏉′欢缂栬瘧锛?
if(CTK_FOUND)
    message(STATUS "CTK 鎻掍欢妗嗘灦鍙敤")
endif()

# ============================================================================
# Framework Library Configuration (Requirement 1.1, 1.5)
# ============================================================================
# Framework is a SHARED library that provides:
# - VTK initialization and management utilities
# - CTK plugin framework integration
# - Common data structures and utilities
# - Base classes for UI components
#
# Design Decision (ADR-001): Why SHARED library?
# - Ensures single instance of singletons (e.g., CTKManager)
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
# CTK Dependencies (PUBLIC - Requirement 1.3)
# ============================================================================
# Linking Strategy: PUBLIC
# Reason: Plugins implement CTK service interfaces (ctkPluginActivator,
#         QObject-based services) that require CTK headers. PUBLIC linking
#         allows plugins to access CTK through Framework.
# ============================================================================
if(CTK_FOUND)
    target_link_libraries(Framework PUBLIC ${CTK_LIBRARIES})
    if(DEFINED CTK_INCLUDE_DIRS)
        target_include_directories(Framework PUBLIC ${CTK_INCLUDE_DIRS})
    elseif(DEFINED CTK_INCLUDE_PATH)
        target_include_directories(Framework PUBLIC ${CTK_INCLUDE_PATH})
    endif()
    target_compile_definitions(Framework PUBLIC
        CTK_PLUGIN_FRAMEWORK
        CTK_PLUGIN_FRAMEWORK_EXPORT
    )
    message(STATUS "Framework: CTK linked as PUBLIC (plugins inherit CTK access)")
endif()

# ============================================================================
# Framework Public Headers
# ============================================================================
set(_framework_public_includes
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Framework
)
if(MEDICALPRO_CTK_INCLUDE_DIRS)
    list(APPEND _framework_public_includes ${MEDICALPRO_CTK_INCLUDE_DIRS})
endif()
list(REMOVE_DUPLICATES _framework_public_includes)
target_include_directories(Framework PUBLIC ${_framework_public_includes})

# 鎻掍欢鐩綍鏀惧湪 Framework 瀹氫箟涔嬪悗锛岄伩鍏?PCH 澶嶇敤鏃舵壘涓嶅埌 Framework 鐩爣
if(CTK_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Plugins")
    add_subdirectory(Plugins)
    message(STATUS "Added Plugins subdirectory")
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
if(CTK_FOUND)
    list(APPEND PROJECT_SOURCES
        # 鍖呭惈鏈塓_OBJECT鍜宻ignals鐨勬湇鍔℃帴鍙ｅご鏂囦欢
        # Plugins/ProsthesisPlanning/ProsthesisPlanningService.h  # 宸茬Щ闄?- 鎻掍欢宸插垹闄?
        Plugins/InstrumentManagement/InstrumentManagementService.h
        # 鍏朵粬甯︽湁Q_OBJECT鍜宻ignals鐨勬湇鍔℃帴鍙ｄ篃搴旀坊鍔犲湪姝?
        # Plugins/UserManagement/UserManagementService.h  # 宸茬粡鍙互姝ｅ父宸ヤ綔
    )
    message(STATUS "Added plugin service interface headers for MOC processing")
endif()

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
    Framework  # Link Framework (gets VTK/CTK transitively through PUBLIC)
)

# 閾炬帴鏂癠I妗嗘灦搴?
if(TARGET NewPagesLib)
    target_link_libraries(medicalpro PRIVATE NewPagesLib)
    message(STATUS "Main app: NewPagesLib linked")
endif()

# ========== 涓嶅啀浣跨敤鐩存帴澶嶅埗锛屾彃浠跺壇鏈敱鍚勬彃浠剁洰褰曞唴鐨凜Make閰嶇疆 ==========
# 鎻掍欢鐨凱OST_BUILD浜嬩欢鍦ㄥ悇鑷殑CMakeLists.txt涓厤缃?
if(CTK_FOUND)
    add_plugin_dependency(medicalpro
        UserManagement
        DicomViewer
        FourViewDisplay
        InstrumentManagement
        OpticalTracking
        Registration2D3D
        PointRegistration
        OpticalRegistration
        BoneSegmentation
        RegistrationCore
    )

    # 娉ㄩ噴鎺夋湁闂鐨勫嚱鏁拌皟鐢?
    # configure_plugin_runtime_copy(medicalpro)
    message(STATUS "Plugin runtime handled by individual plugin configurations")
endif()

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
# CTK Linking for Main App (PRIVATE - Requirement 1.3)
# ============================================================================
# Linking Strategy: PRIVATE
# Reason: Main app uses CTK for plugin framework initialization (CTKManager).
#         Framework already links CTK as PUBLIC, so main app gets CTK
#         transitively. Explicit PRIVATE linking here is for direct CTK usage
#         in main.cpp and MainInterfaceWidget.
# ============================================================================
if(CTK_FOUND)
    target_link_libraries(medicalpro PRIVATE ${CTK_LIBRARIES})
    
    if(DEFINED CTK_INCLUDE_DIRS)
        target_include_directories(medicalpro PRIVATE ${CTK_INCLUDE_DIRS})
    elseif(DEFINED CTK_INCLUDE_PATH)
        target_include_directories(medicalpro PRIVATE ${CTK_INCLUDE_PATH})
    endif()
    
    target_compile_definitions(medicalpro PRIVATE
        CTK_PLUGIN_FRAMEWORK
        CTK_PLUGIN_FRAMEWORK_EXPORT
        MEDICALPRO_MAIN_APPLICATION  # 鏍囪瘑涓荤▼搴忥紝鐢ㄤ簬VTK_MODULE_INIT鏉′欢缂栬瘧
    )
    
    # 娣诲姞椤圭洰鏍圭洰褰曞埌鍖呭惈璺緞锛屼娇鎻掍欢澶存枃浠跺彲浠ヨ鎵惧埌
    target_include_directories(medicalpro PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    
    message(STATUS "Main app: CTK linked as PRIVATE (direct usage in main.cpp)")
endif()



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

# 澶嶅埗EventAdmin鎻掍欢鍒拌緭鍑虹洰褰曪紝骞剁‘淇?CTK / VTK 杩愯鏃?DLL 鍦?exe 鐩綍鍙
if(CTK_FOUND)
    # 鍦?CTK 杈撳嚭鐩綍涓煡鎵捐繍琛屾椂 DLL锛岃嫢鏈壘鍒板啀鍥為€€鍒版湰浠撳簱 ThirdParty/CTK
    set(_ctk_runtime_roots)
    list(APPEND _ctk_runtime_roots
        "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/CTK/CTK_install"  # 鍏滃簳
    )
    set(_filtered_ctk_runtime_roots)
    foreach(_root ${_ctk_runtime_roots})
        if(_root AND EXISTS "${_root}")
            list(APPEND _filtered_ctk_runtime_roots "${_root}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES _filtered_ctk_runtime_roots)

    # 鏋勯€犲€欓€夎矾寰勫苟閫夊嚭绗竴涓瓨鍦ㄧ殑 EventAdmin 鎻掍欢
    set(EVENTADMIN_PLUGIN_PATH "")
    set(_eventadmin_candidates)
    foreach(_root ${_filtered_ctk_runtime_roots})
        list(APPEND _eventadmin_candidates
            "${_root}/lib/ctk-0.1/plugins/liborg_commontk_eventadmin.dll"
            "${_root}/bin/plugins/liborg_commontk_eventadmin.dll"
            "${_root}/bin/Release/plugins/liborg_commontk_eventadmin.dll"
            "${_root}/bin/liborg_commontk_eventadmin.dll"
        )
    endforeach()
    foreach(_candidate ${_eventadmin_candidates})
        if(EXISTS "${_candidate}")
            set(EVENTADMIN_PLUGIN_PATH "${_candidate}")
            break()
        endif()
    endforeach()

    if(EVENTADMIN_PLUGIN_PATH)
        message(STATUS "Found EventAdmin plugin at: ${EVENTADMIN_PLUGIN_PATH}")
        add_custom_command(TARGET medicalpro POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${EVENTADMIN_PLUGIN_PATH}"
                "$<TARGET_FILE_DIR:medicalpro>/plugins/liborg_commontk_eventadmin.dll"
            COMMENT "Copying EventAdmin plugin to output directory"
        )
    else()
        message(WARNING "EventAdmin plugin not found in CTK runtime locations. Please ensure CTK PluginFramework plugins are built.")
    endif()

    # 鏌ユ壘 CTKPluginFramework.dll
    set(CTK_PLUGIN_FRAMEWORK_DLL "")
    set(_ctk_fw_candidates)
    foreach(_root ${_filtered_ctk_runtime_roots})
        list(APPEND _ctk_fw_candidates
            "${_root}/lib/ctk-0.1/CTKPluginFramework.dll"
            "${_root}/bin/CTKPluginFramework.dll"
            "${_root}/bin/Release/CTKPluginFramework.dll"
        )
    endforeach()
    foreach(_candidate ${_ctk_fw_candidates})
        if(EXISTS "${_candidate}")
            set(CTK_PLUGIN_FRAMEWORK_DLL "${_candidate}")
            break()
        endif()
    endforeach()

    if(CTK_PLUGIN_FRAMEWORK_DLL)
        add_custom_command(TARGET medicalpro POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CTK_PLUGIN_FRAMEWORK_DLL}"
                "$<TARGET_FILE_DIR:medicalpro>/CTKPluginFramework.dll"
            COMMENT "Copying CTKPluginFramework.dll to output directory"
        )
    else()
        message(WARNING "CTKPluginFramework.dll not found in CTK runtime locations. Please ensure CTK PluginFramework is built.")
    endif()

endif()

# Copy CTK runtime DLLs (beyond PluginFramework) to output directory
if(WIN32 AND ENABLE_CTK_PLUGIN_FRAMEWORK AND DEFINED CTK_INSTALL_DIR)
    set(_ctk_runtime_dirs
        "${CTK_INSTALL_DIR}/lib/ctk-0.1"
        "${CTK_INSTALL_DIR}/bin"
        "${CTK_INSTALL_DIR}/bin/Release"
    )
    foreach(_dir ${_ctk_runtime_dirs})
        if(EXISTS "${_dir}")
            file(GLOB _ctk_dlls "${_dir}/CTK*.dll")
            if(_ctk_dlls)
                add_custom_command(TARGET medicalpro POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        ${_ctk_dlls}
                        "$<TARGET_FILE_DIR:medicalpro>"
                    COMMENT "Copying CTK runtime DLLs from ${_dir} to output directory"
                )
            endif()
        endif()
    endforeach()
endif()

# Qt SQL driver (QSQLITE) - required by CTK plugin database (plugins.db)
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

    # CTK鏀寔
    if(CTK_FOUND)
        target_link_libraries(medicalpro_newui PRIVATE ${CTK_LIBRARIES})
        target_compile_definitions(medicalpro_newui PRIVATE
            CTK_PLUGIN_FRAMEWORK
            CTK_PLUGIN_FRAMEWORK_EXPORT
        )
        if(DEFINED CTK_INCLUDE_DIRS)
            target_include_directories(medicalpro_newui PRIVATE ${CTK_INCLUDE_DIRS})
        endif()
    endif()

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
# 澶嶅埗閰嶇疆鏂囦欢鍒版瀯寤鸿緭鍑虹洰褰曪紙鏀寔澶氶厤缃敓鎴愬櫒濡?MSVC锛?
if(EXISTS "${CMAKE_SOURCE_DIR}/config")
    # 绑定到 medicalpro 的构建后步骤，避免单独构建目标时遗漏 config
    add_custom_command(TARGET medicalpro POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:medicalpro>/config"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/config"
            "$<TARGET_FILE_DIR:medicalpro>/config"
        COMMENT "Copying config files to build directory..."
    )

    # 如果有 medicalpro_newui 目标，也为其复制配置文件
    if(TARGET medicalpro_newui)
        add_custom_command(TARGET medicalpro_newui POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:medicalpro_newui>/config"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/config"
                "$<TARGET_FILE_DIR:medicalpro_newui>/config"
            COMMENT "Copying config files for medicalpro_newui..."
        )
    endif()

    message(STATUS "Config files will be copied to build output directory")
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
if(CTK_FOUND)
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
endif()




