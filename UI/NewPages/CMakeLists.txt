# =====================================================
# NewPages - 绠€鍖朥I妗嗘灦 CMake閰嶇疆
# =====================================================
# 浣跨敤鏂规硶锛?
# 1. 鍦ㄤ富 CMakeLists.txt 涓坊鍔?add_subdirectory(UI/NewPages)
# 2. 閾炬帴鐩爣 NewPagesLib
# =====================================================

cmake_minimum_required(VERSION 3.16)

# 璁剧疆鑷姩澶勭悊锛堢户鎵跨埗椤圭洰璁剧疆锛屼絾纭繚寮€鍚級
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# UI鏂囦欢璺緞
if(NOT DEFINED UI_FORMS_DIR)
    set(UI_FORMS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Forms")
endif()

# 澶存枃浠?
set(NEWPAGES_HEADERS
    BasePage.h
    PageIndex.h
    MainWindow.h
    ThreePagePresentationUtils.h
    WelcomeBrandingUtils.h
    WelcomePage.h
    LoginPage.h
    ModuleSelectionPage.h
    SystemSettingsPage.h
    ManagementPage.h
    DashboardPage.h
    NavigationPage.h
    Navigation/navigation_evaluation_controller.h
    Navigation/navigation_evaluation_summary_formatter.h
    Navigation/navigation_vtk_bridge.h
    Navigation/navigation_workflow_coordinator.h
    Navigation/navigation_service_bundle.h
    Navigation/navigation_workflow_context.h
    Navigation/preparation_planning_controller.h
    Navigation/registration_controller.h
    PlatformDiagnosticsPage.h
    # BoneSurfaceMotionSimulator used by NavigationPage
    ${CMAKE_SOURCE_DIR}/Plugins/OpticalTracking/BoneSurfaceMotionSimulator.h
)

# 婧愭枃浠?
set(NEWPAGES_SOURCES
    MainWindow.cpp
    ThreePagePresentationUtils.cpp
    WelcomeBrandingUtils.cpp
    WelcomePage.cpp
    LoginPage.cpp
    ModuleSelectionPage.cpp
    SystemSettingsPage.cpp
    ManagementPage.cpp
    DashboardPage.cpp
    NavigationPage.cpp
    Navigation/navigation_evaluation_controller.cpp
    Navigation/navigation_evaluation_summary_formatter.cpp
    Navigation/navigation_vtk_bridge.cpp
    Navigation/navigation_workflow_coordinator.cpp
    Navigation/navigation_service_bundle.cpp
    Navigation/navigation_workflow_context.cpp
    Navigation/preparation_planning_controller.cpp
    Navigation/registration_controller.cpp
    PlatformDiagnosticsPage.cpp
    ${CMAKE_SOURCE_DIR}/Plugins/OpticalTracking/BoneSurfaceMotionSimulator.cpp
)

# UI鏂囦欢
if(NOT DEFINED NEWPAGES_UI_FILES)
    set(NEWPAGES_UI_FILES
        ${UI_FORMS_DIR}/WelcomePage.ui
        ${UI_FORMS_DIR}/LoginPage.ui
        ${UI_FORMS_DIR}/ModuleSelectionPage.ui
        ${UI_FORMS_DIR}/SystemSettingsPage.ui
        ${UI_FORMS_DIR}/ManagementPage.ui
        ${UI_FORMS_DIR}/DashboardPage.ui
        ${UI_FORMS_DIR}/NavigationPage.ui
    )
endif()

list(APPEND NEWPAGES_UI_FILES
    ${UI_FORMS_DIR}/PlatformDiagnosticsPage.ui
)

# 鍒涘缓搴?
add_library(NewPagesLib STATIC
    ${NEWPAGES_HEADERS}
    ${NEWPAGES_SOURCES}
    ${NEWPAGES_UI_FILES}
)

# Help AUTOUIC locate .ui files that live outside this source dir
set_target_properties(NewPagesLib PROPERTIES
    AUTOUIC_SEARCH_PATHS "${UI_FORMS_DIR}"
)

# 璁剧疆鐩爣灞炴€?
target_include_directories(NewPagesLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${UI_FORMS_DIR}
)

# 娣诲姞椤圭洰鏍圭洰褰曞埌鍖呭惈璺緞锛堢敤浜庤闂彃浠舵湇鍔″ご鏂囦欢锛?
target_include_directories(NewPagesLib PRIVATE
    ${CMAKE_SOURCE_DIR}
)

# 閾炬帴Qt搴擄紙浣跨敤鐖堕」鐩凡鎵惧埌鐨凲t鐗堟湰锛?
target_link_libraries(NewPagesLib PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
)

# 閾炬帴Framework搴擄紙鎻愪緵CTK/VTK鏀寔锛?
if(TARGET Framework)
    target_link_libraries(NewPagesLib PUBLIC Framework)
    message(STATUS "NewPagesLib: Framework linked (provides shared platform/VTK access)")
endif()

if(TARGET PointRegistrationPlatformModuleLib)
    target_link_libraries(NewPagesLib PUBLIC PointRegistrationPlatformModuleLib)
    message(STATUS "NewPagesLib: PointRegistrationPlatformModuleLib linked")
endif()

if(TARGET OpticalTrackingPlatformModuleLib)
    target_link_libraries(NewPagesLib PUBLIC OpticalTrackingPlatformModuleLib)
    message(STATUS "NewPagesLib: OpticalTrackingPlatformModuleLib linked")
endif()

# CTK缂栬瘧瀹氫箟锛堜粠鐖堕」鐩户鎵匡級

# 娑堟伅杈撳嚭
message(STATUS "NewPages UI Framework configured")
message(STATUS "  - UI Forms: ${UI_FORMS_DIR}")
message(STATUS "  - Pages: ${NEWPAGES_HEADERS}")
