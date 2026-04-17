# =====================================================
# NewPages - 简化UI框架 CMake配置
# =====================================================
# 使用方法：
# 1. 在主 CMakeLists.txt 中添加 add_subdirectory(UI/NewPages)
# 2. 链接目标 NewPagesLib
# =====================================================

cmake_minimum_required(VERSION 3.16)

# 设置自动处理（继承父项目设置，但确保开启）
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# UI文件路径
if(NOT DEFINED UI_FORMS_DIR)
    set(UI_FORMS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Forms")
endif()

# 头文件
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
    # Shared workflow implementation lives under Plugins but is used by NewPagesLib
    ${CMAKE_SOURCE_DIR}/Plugins/PointRegistration/RegistrationWorkflow.h
    # Force MOC for plugin service interface used by RegistrationWorkflow
    ${CMAKE_SOURCE_DIR}/Plugins/PointRegistration/PointRegistrationService.h
    # BoneSurfaceMotionSimulator used by NavigationPage
    ${CMAKE_SOURCE_DIR}/Plugins/OpticalTracking/BoneSurfaceMotionSimulator.h
)

# 源文件
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
    ${CMAKE_SOURCE_DIR}/Plugins/PointRegistration/RegistrationWorkflow.cpp
    ${CMAKE_SOURCE_DIR}/Plugins/OpticalTracking/BoneSurfaceMotionSimulator.cpp
)

# UI文件
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

# 创建库
add_library(NewPagesLib STATIC
    ${NEWPAGES_HEADERS}
    ${NEWPAGES_SOURCES}
    ${NEWPAGES_UI_FILES}
)

# Help AUTOUIC locate .ui files that live outside this source dir
set_target_properties(NewPagesLib PROPERTIES
    AUTOUIC_SEARCH_PATHS "${UI_FORMS_DIR}"
)

# 设置目标属性
target_include_directories(NewPagesLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${UI_FORMS_DIR}
)

# 添加项目根目录到包含路径（用于访问插件服务头文件）
target_include_directories(NewPagesLib PRIVATE
    ${CMAKE_SOURCE_DIR}
)

# 链接Qt库（使用父项目已找到的Qt版本）
target_link_libraries(NewPagesLib PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
)

# 链接Framework库（提供CTK/VTK支持）
if(TARGET Framework)
    target_link_libraries(NewPagesLib PUBLIC Framework)
    message(STATUS "NewPagesLib: Framework linked (provides CTK/VTK access)")
endif()

# CTK编译定义（从父项目继承）
if(CTK_FOUND)
    target_compile_definitions(NewPagesLib PRIVATE CTK_PLUGIN_FRAMEWORK)
endif()

# 消息输出
message(STATUS "NewPages UI Framework configured")
message(STATUS "  - UI Forms: ${UI_FORMS_DIR}")
message(STATUS "  - Pages: ${NEWPAGES_HEADERS}")
