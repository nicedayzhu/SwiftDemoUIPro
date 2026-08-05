cmake_minimum_required(VERSION 3.21)

project(SwiftDemoUIPro VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)

add_library(swift_demo_launcher_core STATIC
    src/Cs2Manager.cpp
    src/Cs2Manager.h
)
target_link_libraries(swift_demo_launcher_core PUBLIC Qt6::Core)
target_include_directories(swift_demo_launcher_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

qt_add_executable(SwiftDemoLauncher WIN32
    src/main.cpp
    src/LauncherWindow.cpp
    src/LauncherWindow.h
)
if(WIN32)
    target_sources(SwiftDemoLauncher PRIVATE assets/windows_app.rc)
endif()
target_link_libraries(SwiftDemoLauncher PRIVATE
    swift_demo_launcher_core
    Qt6::Gui
    Qt6::Widgets
)
set_target_properties(SwiftDemoLauncher PROPERTIES OUTPUT_NAME "SwiftDemoUIPro")

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
        Qt6::Test
    )
    add_test(NAME SwiftDemoLauncherTests COMMAND SwiftDemoLauncherTests)
endif()

install(TARGETS SwiftDemoLauncher RUNTIME DESTINATION .)
if(EXISTS "${MENU_VPK}")
    install(FILES "${MENU_VPK}" DESTINATION .)
endif()
