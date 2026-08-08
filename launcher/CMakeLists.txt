cmake_minimum_required(VERSION 3.21)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../VERSION")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../MENU_VERSION")
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../VERSION" SWIFT_DEMOUI_VERSION LIMIT_COUNT 1)
string(STRIP "${SWIFT_DEMOUI_VERSION}" SWIFT_DEMOUI_VERSION)
if(NOT SWIFT_DEMOUI_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "../VERSION must contain a semantic version such as 1.2.3")
endif()
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../MENU_VERSION" SWIFT_DEMOUI_MENU_VERSION LIMIT_COUNT 1)
string(STRIP "${SWIFT_DEMOUI_MENU_VERSION}" SWIFT_DEMOUI_MENU_VERSION)
if(NOT SWIFT_DEMOUI_MENU_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "../MENU_VERSION must contain a semantic version such as 1.2.3")
endif()

project(SwiftDemoUIPro VERSION "${SWIFT_DEMOUI_VERSION}" LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Network Widgets LinguistTools)
find_package(Git QUIET)

set(SWIFT_DEMOUI_GIT_COMMIT "unknown")
if(Git_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short=12 HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
        RESULT_VARIABLE SWIFT_DEMOUI_GIT_RESULT
        OUTPUT_VARIABLE SWIFT_DEMOUI_GIT_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(SWIFT_DEMOUI_GIT_RESULT EQUAL 0 AND SWIFT_DEMOUI_GIT_OUTPUT)
        set(SWIFT_DEMOUI_GIT_COMMIT "${SWIFT_DEMOUI_GIT_OUTPUT}")
    endif()
endif()

set(SWIFT_DEMOUI_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${SWIFT_DEMOUI_GENERATED_DIR}")
configure_file(
    src/AppVersion.h.in
    "${SWIFT_DEMOUI_GENERATED_DIR}/AppVersion.h"
    @ONLY
)

add_library(miniz STATIC
    third_party/miniz/miniz.c
    third_party/miniz/miniz.h
)
target_include_directories(miniz PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniz")
set_target_properties(miniz PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)

add_library(swift_demo_launcher_core STATIC
    src/Cs2Manager.cpp
    src/Cs2Manager.h
    src/UpdateService.cpp
    src/UpdateService.h
)
target_link_libraries(swift_demo_launcher_core PUBLIC Qt6::Core Qt6::Network PRIVATE miniz)
target_include_directories(swift_demo_launcher_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

qt_add_executable(SwiftDemoLauncher WIN32
    src/main.cpp
    src/LauncherWindow.cpp
    src/LauncherWindow.h
)
if(WIN32)
    configure_file(
        assets/windows_app.rc.in
        "${SWIFT_DEMOUI_GENERATED_DIR}/windows_app.rc"
        @ONLY
    )
    target_sources(SwiftDemoLauncher PRIVATE "${SWIFT_DEMOUI_GENERATED_DIR}/windows_app.rc")
endif()
target_include_directories(SwiftDemoLauncher PRIVATE "${SWIFT_DEMOUI_GENERATED_DIR}")
target_link_libraries(SwiftDemoLauncher PRIVATE
    swift_demo_launcher_core
    Qt6::Gui
    Qt6::Widgets
)
set_target_properties(SwiftDemoLauncher PROPERTIES OUTPUT_NAME "SwiftDemoUIPro")
qt_add_resources(SwiftDemoLauncher "embedded_fonts"
    PREFIX "/fonts"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts"
    FILES assets/fonts/NotoSansSC-VF.ttf
)

set(VOICE_INDEXER_EXE "${CMAKE_CURRENT_SOURCE_DIR}/../tools/voice-indexer/target/release/swift-demo-voice-indexer.exe")
if(EXISTS "${VOICE_INDEXER_EXE}")
    add_custom_command(TARGET SwiftDemoLauncher POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VOICE_INDEXER_EXE}"
            "$<TARGET_FILE_DIR:SwiftDemoLauncher>/swift-demo-voice-indexer.exe"
        COMMENT "Copying the Demo voice indexer next to the launcher"
    )
else()
    message(WARNING "Demo voice indexer is missing. Run cargo build --release in ../tools/voice-indexer before packaging the launcher.")
endif()

file(GLOB APP_TS_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/translations/swift_demoui_pro_*.ts"
)
set_source_files_properties(${APP_TS_FILES}
    PROPERTIES OUTPUT_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/translations"
)
qt_add_translations(SwiftDemoLauncher
    TS_FILES ${APP_TS_FILES}
    SOURCES
        src/LauncherWindow.cpp
        src/LauncherWindow.h
        src/Cs2Manager.cpp
        src/Cs2Manager.h
        src/UpdateService.cpp
        src/UpdateService.h
    QM_FILES_OUTPUT_VARIABLE APP_QM_FILES
    LUPDATE_OPTIONS -source-language en
)
add_custom_command(TARGET SwiftDemoLauncher POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:SwiftDemoLauncher>/translations"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${APP_QM_FILES}
        "$<TARGET_FILE_DIR:SwiftDemoLauncher>/translations"
    COMMENT "Copying application translations next to the executable"
)

if(WIN32)
    target_compile_definitions(swift_demo_launcher_core PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_compile_definitions(SwiftDemoLauncher PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
endif()

if(MSVC)
    target_compile_options(swift_demo_launcher_core PRIVATE /utf-8 /W4 /permissive-)
    target_compile_options(SwiftDemoLauncher PRIVATE /utf-8 /W4 /permissive-)
else()
    target_compile_options(swift_demo_launcher_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(SwiftDemoLauncher PRIVATE -Wall -Wextra -Wpedantic)
endif()

set(MENU_VPK "${CMAKE_CURRENT_SOURCE_DIR}/../dist/swift_demo_menu_override.vpk")
if(EXISTS "${MENU_VPK}")
    add_custom_command(TARGET SwiftDemoLauncher POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MENU_VPK}"
            "$<TARGET_FILE_DIR:SwiftDemoLauncher>/swift_demo_menu_override.vpk"
        COMMENT "Copying the compiled demo menu VPK next to the launcher"
    )
else()
    message(WARNING "Menu VPK is missing. Run ../demo-menu.ps1 before packaging the launcher.")
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
    qt_add_executable(SwiftDemoLauncherTests
        tests/tst_launcher_core.cpp
    )
    target_link_libraries(SwiftDemoLauncherTests PRIVATE
        swift_demo_launcher_core
        miniz
        Qt6::Test
    )
    add_test(NAME SwiftDemoLauncherTests COMMAND SwiftDemoLauncherTests)
endif()

install(TARGETS SwiftDemoLauncher RUNTIME DESTINATION .)
install(FILES ${APP_QM_FILES} DESTINATION translations)
install(FILES assets/fonts/OFL-1.1.txt
    DESTINATION licenses
    RENAME NotoSansSC-OFL-1.1.txt
)
install(FILES licenses/Qt-LGPL-3.0-only.txt
    DESTINATION licenses
    RENAME Qt-LGPL-3.0.txt
)
install(FILES third_party/miniz/LICENSE
    DESTINATION licenses
    RENAME miniz-MIT.txt
)
if(EXISTS "${MENU_VPK}")
    install(FILES "${MENU_VPK}" DESTINATION .)
endif()
if(EXISTS "${VOICE_INDEXER_EXE}")
    install(FILES "${VOICE_INDEXER_EXE}" DESTINATION . RENAME swift-demo-voice-indexer.exe)
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/voice-indexer/SOURCE2_DEMO_LICENSE-MIT.txt"
        DESTINATION licenses
        RENAME source2-demo-MIT.txt
    )
endif()
