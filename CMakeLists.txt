cmake_minimum_required(VERSION 3.16)

# 禁用Qt Creator的MaintenanceTool警告
set(QT_CREATOR_SKIP_MAINTENANCE_TOOL_PROVIDER ON CACHE BOOL "Skip Qt Creator maintenance tool provider")

project(medicalpro VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set encoding to UTF-8
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Set Qt installation path if needed (uncomment and modify if necessary)
# set(CMAKE_PREFIX_PATH "C:/Qt/6.x.x/mingw_64" ${CMAKE_PREFIX_PATH})

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools Sql)

# === VTK 配置 ===
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

# === ITK 配置 ===
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
        # ITK通常需要包含UseitK.cmake
        include(${ITK_USE_FILE})
        add_definitions(-DITK_FOUND)
    else()
        message(WARNING "ITK not found at ${ITK_DIR}")
    endif()
else()
    message(WARNING "ITK directory not found in any search paths")
endif()

# === CTK Plugin Framework 配置（类似ITK/VTK配置） ===
option(ENABLE_CTK_PLUGIN_FRAMEWORK "Enable CTK Plugin Framework support" ON)

if(ENABLE_CTK_PLUGIN_FRAMEWORK)
    # CTK 安装路径配置，指向CTK安装根目录
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
        
        # 保存原始CTK_DIR，防止find_package修改它
        set(CTK_INSTALL_DIR "${CTK_DIR}")
        
        # 使用find_package查找CTK（类似ITK/VTK方式）
        # 注意：避免VTK冲突，优先使用CTK_install目录
        find_package(CTK QUIET PATHS
            ${CTK_INSTALL_DIR}/lib/ctk-0.1
            ${CTK_INSTALL_DIR}
            ${CTK_INSTALL_DIR}/../CTK-build
            NO_DEFAULT_PATH)
        
        if(CTK_FOUND)
            message(STATUS "Found CTK version: ${CTK_VERSION}")
            message(STATUS "CTK_DIR: ${CTK_DIR}")
            message(STATUS "CTK_USE_FILE: ${CTK_USE_FILE}")
            message(STATUS "CTK_CMAKE_DIR: ${CTK_CMAKE_DIR}")
            
            # 包含CTK配置文件（类似VTK的使用方式）
            include(${CTK_USE_FILE})
            
            # 设置编译定义
            add_definitions(-DCTK_PLUGIN_FRAMEWORK)
            add_definitions(-DCTK_PLUGIN_FRAMEWORK_EXPORT)
            
            # 存储CTK库信息（增强版）
            set(CTK_LIBRARIES 
                CTKCore 
                CTKWidgets 
                CTKPluginFramework
                # 新增实用组件
                # CTKDICOMCore        # DICOM支持（如果CTK编译时包含）
                # CTKVisualizationVTKCore  # VTK集成（如果需要）
            )
            
            message(STATUS "CTK Plugin Framework enabled successfully")
            message(STATUS "CTK_LIBRARIES: ${CTK_LIBRARIES}")
            
            # 添加插件目录到子目录
            if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Plugins")
                add_subdirectory(Plugins)
                message(STATUS "Added Plugins subdirectory")
            endif()
        else()
            # 回退到手动查找
            message(STATUS "find_package(CTK) failed, trying manual search...")
            
            # 使用原始的安装目录路径
            set(CTK_LIB_PATH "${CTK_INSTALL_DIR}/lib/ctk-0.1")
            set(CTK_INCLUDE_PATH "${CTK_INSTALL_DIR}/include/ctk-0.1")
            
            message(STATUS "CTK_LIB_PATH: ${CTK_LIB_PATH}")
            message(STATUS "CTK_INCLUDE_PATH: ${CTK_INCLUDE_PATH}")
            
            # 检查关键库是否存在
            find_library(CTK_CORE_LIBRARY
                NAMES CTKCore libCTKCore
                PATHS "${CTK_LIB_PATH}"
                NO_DEFAULT_PATH
            )
            
            find_library(CTK_PLUGIN_FRAMEWORK_LIBRARY
                NAMES CTKPluginFramework libCTKPluginFramework  
                PATHS "${CTK_LIB_PATH}"
                NO_DEFAULT_PATH
            )
            
            find_library(CTK_WIDGETS_LIBRARY
                NAMES CTKWidgets libCTKWidgets
                PATHS "${CTK_LIB_PATH}"
                NO_DEFAULT_PATH
            )
            
            if(CTK_CORE_LIBRARY AND CTK_PLUGIN_FRAMEWORK_LIBRARY AND CTK_WIDGETS_LIBRARY)
                set(CTK_FOUND TRUE)
                
                # 手动设置CTK变量
                set(CTK_LIBRARIES 
                    ${CTK_CORE_LIBRARY}
                    ${CTK_WIDGETS_LIBRARY}
                    ${CTK_PLUGIN_FRAMEWORK_LIBRARY}
                )
                
                set(CTK_INCLUDE_DIRS ${CTK_INCLUDE_PATH})
                
                # 添加包含目录
                include_directories(${CTK_INCLUDE_DIRS})
                
                add_definitions(-DCTK_PLUGIN_FRAMEWORK)
                add_definitions(-DCTK_PLUGIN_FRAMEWORK_EXPORT)
                
                message(STATUS "CTK Plugin Framework enabled (manual configuration)")
                message(STATUS "CTK_CORE_LIBRARY: ${CTK_CORE_LIBRARY}")
                message(STATUS "CTK_PLUGIN_FRAMEWORK_LIBRARY: ${CTK_PLUGIN_FRAMEWORK_LIBRARY}")
                message(STATUS "CTK_WIDGETS_LIBRARY: ${CTK_WIDGETS_LIBRARY}")
                
                # 添加插件目录到子目录
                if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Plugins")
                    add_subdirectory(Plugins)
                    message(STATUS "Added Plugins subdirectory")
                endif()
            else()
                set(CTK_FOUND FALSE)
                message(WARNING "CTK libraries not found at ${CTK_INSTALL_DIR}/lib/ctk-0.1")
                message(WARNING "Required libraries:")
                message(WARNING "  - CTKCore: ${CTK_CORE_LIBRARY}")
                message(WARNING "  - CTKPluginFramework: ${CTK_PLUGIN_FRAMEWORK_LIBRARY}")
                message(WARNING "  - CTKWidgets: ${CTK_WIDGETS_LIBRARY}")
            endif()
        endif()
    else()
        set(CTK_FOUND FALSE)
        message(WARNING "CTK not found in any search paths:")
        foreach(path ${CTK_POSSIBLE_PATHS})
            message(WARNING "  - ${path}")
        endforeach()
        message(WARNING "CTK Plugin Framework disabled")
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
    # 共享数据结构（支持CTK插件间通信）
    Framework/Core/MedicalImageData.h
    Framework/Core/MedicalImageData.cpp
)

# CTK增强组件源文件（条件编译）
if(CTK_FOUND)
    list(APPEND FRAMEWORK_SOURCES
        # CTK增强日志系统（实际使用）
        Framework/CTKEnhancedLogger.h
        Framework/CTKEnhancedLogger.cpp
    )
    
    message(STATUS "CTK增强日志系统已添加到FRAMEWORK_SOURCES")
    
    # 添加CTK增强功能的编译定义
    add_definitions(-DCTK_ENHANCED_COMPONENTS)
    
endif()

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        ${FRAMEWORK_SOURCES}
        ${TS_FILES}
)

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

target_link_libraries(medicalpro PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Sql)

# Link VTK libraries if available (for main app)
if(VTK_FOUND)
    target_include_directories(medicalpro PRIVATE ${VTK_INCLUDE_DIRS})
    target_link_libraries(medicalpro PRIVATE ${VTK_LIBRARIES})
    message(STATUS "Linked VTK libraries to medicalpro")
endif()

# Link ITK libraries if available (for main app)
if(ITK_FOUND)
    target_include_directories(medicalpro PRIVATE ${ITK_INCLUDE_DIRS})
    target_link_libraries(medicalpro PRIVATE ${ITK_LIBRARIES})
    
    # 抑制ITK 5.x的meta data警告和信息输出
    if(MSVC)
        target_compile_options(medicalpro PRIVATE 
            "/wd4996"  # 忽略弃用警告
            "/wd4244"  # 忽略类型转换警告
        )
    endif()
    
    # ITK特定配置，减少冗余输出
    target_compile_definitions(medicalpro PRIVATE
        ITK_LEGACY_SILENT  # 抑制ITK遗留代码警告
        ITK_IO_FACTORY_REGISTER_MANAGER  # 静态注册IO工厂
    )
    
    message(STATUS "Linked ITK libraries to medicalpro:")
    foreach(lib ${ITK_LIBRARIES})
        message(STATUS "  - ${lib}")
    endforeach()
endif()

# Link CTK libraries if available
if(CTK_FOUND)
    # 使用统一的CTK_LIBRARIES变量
    target_link_libraries(medicalpro PRIVATE ${CTK_LIBRARIES})
    
    # 如果有包含目录，添加包含目录
    if(DEFINED CTK_INCLUDE_DIRS)
        target_include_directories(medicalpro PRIVATE ${CTK_INCLUDE_DIRS})
    elseif(DEFINED CTK_INCLUDE_PATH)
        target_include_directories(medicalpro PRIVATE ${CTK_INCLUDE_PATH})
    endif()
    
    message(STATUS "Linked CTK libraries to medicalpro:")
    foreach(lib ${CTK_LIBRARIES})
        message(STATUS "  - ${lib}")
    endforeach()
endif()



# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.medicalpro)
endif()
set_target_properties(medicalpro PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
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

# 插件目录已经在CTK配置部分添加了，这里不需要重复添加

# 复制EventAdmin插件到输出目录
if(CTK_FOUND)
    set(EVENTADMIN_PLUGIN_PATH "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/CTK/CTK_install/lib/ctk-0.1/plugins/liborg_commontk_eventadmin.dll")

    if(EXISTS "${EVENTADMIN_PLUGIN_PATH}")
        message(STATUS "Found EventAdmin plugin at: ${EVENTADMIN_PLUGIN_PATH}")

        # 添加自定义命令来复制EventAdmin插件
        add_custom_command(TARGET medicalpro POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${EVENTADMIN_PLUGIN_PATH}"
                "$<TARGET_FILE_DIR:medicalpro>/plugins/liborg_commontk_eventadmin.dll"
            COMMENT "Copying EventAdmin plugin to output directory"
        )

        message(STATUS "EventAdmin plugin will be copied to output directory")
    else()
        message(WARNING "EventAdmin plugin not found at: ${EVENTADMIN_PLUGIN_PATH}")
    endif()
endif()
