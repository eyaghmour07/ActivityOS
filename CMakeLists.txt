cmake_minimum_required(VERSION 3.24)

project(ActivityOS VERSION 0.1.0 LANGUAGES CXX)
if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

option(ACTIVITYOS_BUILD_TESTS "Build ActivityOS tests" ON)

if(APPLE)
    if(EXISTS "/opt/homebrew/opt/qt")
        list(PREPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/qt")
    elseif(EXISTS "/usr/local/opt/qt")
        list(PREPEND CMAKE_PREFIX_PATH "/usr/local/opt/qt")
    endif()
endif()

find_package(SQLite3 REQUIRED)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)

if(TARGET SQLite3::SQLite3)
    set(ACTIVITYOS_SQLITE_TARGET SQLite3::SQLite3)
elseif(TARGET SQLite::SQLite3)
    set(ACTIVITYOS_SQLITE_TARGET SQLite::SQLite3)
else()
    message(FATAL_ERROR "FindSQLite3 did not provide a supported imported target")
endif()

add_library(activityos_analytics
    src/core/analytics.cpp
)
target_include_directories(activityos_analytics PUBLIC include)

add_library(activityos_storage
    src/storage/storage.cpp
)
target_include_directories(activityos_storage PUBLIC include)
target_link_libraries(activityos_storage PUBLIC ${ACTIVITYOS_SQLITE_TARGET})
target_compile_definitions(activityos_storage PRIVATE
    ACTIVITYOS_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources/migrations"
)

set(ACTIVITYOS_PLATFORM_SOURCE src/platform/activity_source.cpp)
if(APPLE)
    list(APPEND ACTIVITYOS_PLATFORM_SOURCE src/platform/macos_activity_source.mm)
elseif(WIN32)
    list(APPEND ACTIVITYOS_PLATFORM_SOURCE src/platform/windows_activity_source.cpp)
elseif(UNIX)
    list(APPEND ACTIVITYOS_PLATFORM_SOURCE src/platform/linux_activity_source.cpp)
    find_package(X11 REQUIRED)
endif()

add_library(activityos_platform ${ACTIVITYOS_PLATFORM_SOURCE})
target_include_directories(activityos_platform PUBLIC include)
if(APPLE)
    target_link_libraries(activityos_platform PRIVATE
        "-framework AppKit"
        "-framework ApplicationServices"
        "-framework CoreGraphics"
    )
elseif(WIN32)
    target_link_libraries(activityos_platform PRIVATE user32)
elseif(UNIX)
    target_include_directories(activityos_platform PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(activityos_platform PRIVATE ${X11_LIBRARIES} Xss)
endif()

add_library(activityos_services
    src/services/tracker_service.cpp
    src/services/activity_service.cpp
)
target_include_directories(activityos_services PUBLIC include)
target_link_libraries(activityos_services
    PUBLIC
        activityos_analytics
        activityos_platform
        activityos_storage
        Qt6::Core
)

qt_add_executable(ActivityOS
    src/app/main.cpp
    src/app/main_window.cpp
    src/app/main_window.hpp
    resources/activityos.qrc
    resources/icons/ActivityOS.icns
)
set_source_files_properties(resources/icons/ActivityOS.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources"
)
target_link_libraries(ActivityOS
    PRIVATE
        activityos_services
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)
set_target_properties(ActivityOS PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_ICON_FILE ActivityOS.icns
    MACOSX_BUNDLE_BUNDLE_NAME ActivityOS
    MACOSX_BUNDLE_GUI_IDENTIFIER com.activityos.desktop
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
)

if(MSVC)
    foreach(target activityos_analytics activityos_storage activityos_platform activityos_services)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    endforeach()
else()
    foreach(target activityos_analytics activityos_storage activityos_platform activityos_services)
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endforeach()
endif()

include(CTest)
if(BUILD_TESTING AND ACTIVITYOS_BUILD_TESTS)
    add_executable(activityos_analytics_tests tests/analytics_tests.cpp)
    target_link_libraries(activityos_analytics_tests PRIVATE activityos_analytics)
    add_test(NAME analytics COMMAND activityos_analytics_tests)

    add_executable(activityos_storage_tests tests/storage_tests.cpp)
    target_link_libraries(activityos_storage_tests PRIVATE activityos_storage)
    add_test(NAME storage COMMAND activityos_storage_tests)

    add_executable(activityos_activity_source_tests tests/activity_source_tests.cpp)
    target_link_libraries(activityos_activity_source_tests PRIVATE activityos_platform)
    add_test(NAME activity_source COMMAND activityos_activity_source_tests)

    add_executable(activityos_services_tests tests/services_tests.cpp)
    target_link_libraries(activityos_services_tests PRIVATE activityos_services)
    add_test(NAME services COMMAND activityos_services_tests)
endif()

install(TARGETS ActivityOS
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
qt_generate_deploy_app_script(
    TARGET ActivityOS
    OUTPUT_SCRIPT activityos_deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${activityos_deploy_script})

set(CPACK_PACKAGE_NAME "ActivityOS")
set(CPACK_PACKAGE_VENDOR "ActivityOS")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Local-first developer workstyle analytics")
if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
elseif(WIN32)
    set(CPACK_GENERATOR "NSIS")
else()
    set(CPACK_GENERATOR "TGZ")
endif()
include(CPack)
