cmake_minimum_required(VERSION 3.21)
project(TokenMonitorQt VERSION 0.57.0 LANGUAGES CXX)

# The widget carries its own icon resource (resources/app.rc). RC needs rc.exe
# from the MSVC/Windows SDK toolchain, which only exists on Windows, so it is
# enabled here rather than in project() — a configure on another platform must
# not fail just because this Windows-first tree has no resource compiler.
if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT DEFINED CMAKE_PREFIX_PATH AND EXISTS "D:/App/Qt/6.8.3/msvc2022_64")
    set(CMAKE_PREFIX_PATH "D:/App/Qt/6.8.3/msvc2022_64")
endif()

if(MSVC)
    add_compile_options(/utf-8 /W3 /permissive- /Zc:__cplusplus)
    add_definitions(-DUNICODE -D_UNICODE -DNOMINMAX -DWIN32_LEAN_AND_MEAN)
endif()

find_package(Qt6 REQUIRED COMPONENTS
    Core Gui Qml Quick QuickControls2 QuickLayouts Network Concurrent Svg Sql Widgets
)
find_package(Qt6 QUIET COMPONENTS QuickEffects)

set(TMON_CORE_SOURCES
    src/core/io/JsonIo.cpp
    src/core/io/Paths.cpp
    src/core/io/HashKey.cpp
    src/core/io/SettingsStore.cpp
    src/core/io/CredentialStore.cpp
    src/core/io/PidFile.cpp
    src/core/catalog/Catalog.cpp
    src/core/catalog/ClientRoots.cpp
    src/core/usage/JsonUtil.cpp
    src/core/usage/PeriodDelta.cpp
    src/core/usage/UsageNormalize.cpp
    src/core/usage/TokscaleRunner.cpp
    src/core/usage/UsageWatcher.cpp
    src/core/usage/UsageEngine.cpp
    src/core/usage/WslUsage.cpp
    src/core/usage/SelfSync.cpp
    src/core/usage/ArchiveUsage.cpp
    src/core/usage/LocalParsers.cpp
    src/core/process/Subprocess.cpp
    src/core/net/HttpClient.cpp
    src/core/limits/LimitsCore.cpp
    src/core/limits/LimitsBurnRate.cpp
    src/core/limits/LimitsRuntime.cpp
    src/core/limits/Providers.cpp
    src/core/limits/ZcodeDiscovery.cpp
    src/core/limits/SpendStore.cpp
    src/core/hub/HubServer.cpp
    src/core/hub/HubClient.cpp
    src/core/device/DeviceRuntime.cpp
    src/core/export/Exporter.cpp
    src/core/i18n/I18n.cpp
)

add_library(tmon_core STATIC ${TMON_CORE_SOURCES})
target_include_directories(tmon_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(tmon_core PUBLIC
    Qt6::Core Qt6::Gui Qt6::Network Qt6::Concurrent Qt6::Sql
)

set(TMON_QML_NAMES
    Theme.qml Main.qml TitleBar.qml PeriodTabs.qml Footer.qml HomeView.qml
    UsageRow.qml LimitRow.qml Heatmap.qml TrendChart.qml ToolView.qml ModelView.qml
    LimitsView.qml SessionView.qml SessionDetail.qml ProjectView.qml DeviceView.qml StatusView.qml
    TrendsView.qml SettingsPanel.qml Bubble.qml Dashboard.qml
    TintIcon.qml IconButton.qml TotalPanel.qml HomeListRow.qml SwitchToggle.qml
)
set(TMON_QML)
foreach(f ${TMON_QML_NAMES})
    set(_path qml/TokenMonitor/${f})
    set_source_files_properties(${_path} PROPERTIES QT_RESOURCE_ALIAS ${f})
    list(APPEND TMON_QML ${_path})
endforeach()
set_source_files_properties(qml/TokenMonitor/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

find_program(NODE_EXECUTABLE node)
if(NODE_EXECUTABLE)
    execute_process(
        COMMAND ${NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/extract-i18n.js
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE TMON_I18N_RC
    )
elseif(WIN32)
    execute_process(
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File ${CMAKE_CURRENT_SOURCE_DIR}/scripts/extract-i18n.ps1
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE TMON_I18N_RC
    )
endif()
if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/resources/i18n.json)
    file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/resources/i18n.json "{\"languages\":[\"en\"],\"messages\":{\"en\":{}}}")
endif()

qt_add_resources(tmon_core i18n
    PREFIX /i18n
    BASE ${CMAKE_CURRENT_SOURCE_DIR}/resources
    FILES resources/i18n.json
)

set(TMON_WIDGET_SOURCES
    src/widget/main.cpp
    src/widget/Acrylic.cpp
    src/widget/WidgetWindow.cpp
    src/widget/TrayController.cpp
    src/widget/Hotkey.cpp
    src/widget/AppState.cpp
    src/widget/DashboardWindow.cpp
    src/widget/RowListModel.cpp
    src/widget/MaskIconProvider.cpp
)

# Windowed subsystem: double-clicking the exe must not open a console.
# --scan-once reattaches to the parent console at runtime (main.cpp).
#
# The resource file is generated from resources/app.rc.in so the exe identifies
# itself (0.57 reported version 0.0.0.0 in crash reports). The version comes from
# src/core/tmon.h — the same source scripts/package.ps1 reads for the installer
# name — so the exe and the package cannot drift apart.
set(TMON_RC_SOURCES)
if(WIN32)
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/src/core/tmon.h" TMON_VERSION_LINE REGEX "kAppVersion")
    if(TMON_VERSION_LINE MATCHES "\"([0-9]+\\.[0-9]+\\.[0-9]+)\"")
        set(TMON_VERSION "${CMAKE_MATCH_1}")
    else()
        set(TMON_VERSION "${PROJECT_VERSION}")
        message(WARNING "Could not read kAppVersion from src/core/tmon.h; using ${PROJECT_VERSION} for the version resource.")
    endif()
    string(REPLACE "." ";" TMON_VERSION_PARTS "${TMON_VERSION}")
    list(APPEND TMON_VERSION_PARTS "0")
    list(SUBLIST TMON_VERSION_PARTS 0 4 TMON_VERSION_PARTS)
    list(JOIN TMON_VERSION_PARTS "," TMON_VERSION_COMMA)
    file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.ico" TMON_ICON_NATIVE)
    # rc.exe wants the backslashes escaped inside the quoted ICON path.
    string(REPLACE "\\" "\\\\" TMON_ICON_PATH "${TMON_ICON_NATIVE}")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/app.rc" @ONLY)
    list(APPEND TMON_RC_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/app.rc")
endif()
add_executable(TokenMonitor WIN32 ${TMON_WIDGET_SOURCES} ${TMON_RC_SOURCES})
set_target_properties(TokenMonitor PROPERTIES OUTPUT_NAME TokenMonitorQt)
target_include_directories(TokenMonitor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(TokenMonitor PRIVATE
    tmon_core Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::QuickLayouts Qt6::Svg Qt6::Widgets Qt6::Concurrent
)
if(TARGET Qt6::QuickEffects)
    target_link_libraries(TokenMonitor PRIVATE Qt6::QuickEffects)
endif()

qt_add_qml_module(TokenMonitor
    URI TokenMonitor
    VERSION 1.0
    QML_FILES ${TMON_QML}
    RESOURCE_PREFIX /qt/qml
    DEPENDENCIES
        QtQuick
        QtQuick.Controls
        QtQuick.Effects
)

file(GLOB TMON_ICON_SVGS "${CMAKE_CURRENT_SOURCE_DIR}/../assets/icons/*.svg")
if(TMON_ICON_SVGS)
    qt_add_resources(TokenMonitor icons
        PREFIX /icons
        BASE ${CMAKE_CURRENT_SOURCE_DIR}/../assets
        FILES ${TMON_ICON_SVGS}
    )
endif()

file(GLOB_RECURSE TMON_UI_ICONS "${CMAKE_CURRENT_SOURCE_DIR}/../src/electron/renderer/icons/*.svg")
if(TMON_UI_ICONS)
    qt_add_resources(TokenMonitor uiicons
        PREFIX /ui
        BASE ${CMAKE_CURRENT_SOURCE_DIR}/../src/electron/renderer
        FILES ${TMON_UI_ICONS}
    )
endif()

add_executable(TokenMonitorHub src/hub/main.cpp)
target_include_directories(TokenMonitorHub PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(TokenMonitorHub PRIVATE tmon_core Qt6::Network)

add_executable(TokenMonitorAgent src/agent/main.cpp)
target_include_directories(TokenMonitorAgent PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(TokenMonitorAgent PRIVATE tmon_core Qt6::Network)

add_executable(TokenMonitorDeltaCheck src/tests/period_delta_check.cpp)
target_include_directories(TokenMonitorDeltaCheck PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(TokenMonitorDeltaCheck PRIVATE tmon_core)

add_executable(TokenMonitorHubCheck src/tests/hub_protocol_check.cpp)
target_include_directories(TokenMonitorHubCheck PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(TokenMonitorHubCheck PRIVATE tmon_core Qt6::Network)

add_executable(TokenMonitorBurnCheck src/tests/burn_rate_check.cpp)
target_include_directories(TokenMonitorBurnCheck PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(TokenMonitorBurnCheck PRIVATE tmon_core)
