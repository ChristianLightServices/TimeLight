cmake_minimum_required(VERSION 3.23)

option(GENERATE_INSTALLER "Generate an installer." OFF)

project(TimeLight VERSION 2.3.2 LANGUAGES CXX)

set(TimeLight_VERSION_STR "${TimeLight_VERSION_MAJOR}.${TimeLight_VERSION_MINOR}.${TimeLight_VERSION_PATCH}")

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions("IS_DEBUG_BUILD")
endif()

find_package(Qt6 6.2 COMPONENTS Core Network NetworkAuth Widgets LinguistTools Svg REQUIRED)

# some of the following several lines of CMake shamelessly stolen from https://github.com/ArthurSonzogni/nlohmann_json_cmake_fetchcontent
# (since I'm using the repo, I don't feel too bad about using their example CMake stuff)
find_package(nlohmann_json 3.10.5)

if(NOT nlohmann_json_FOUND)
    message("Fetching nlohmann/json from GitHub...")

    include(FetchContent)

    FetchContent_Declare(json
        GIT_REPOSITORY https://github.com/ArthurSonzogni/nlohmann_json_cmake_fetchcontent
        GIT_TAG v3.10.5
    )

    FetchContent_GetProperties(json)
    if(NOT json_POPULATED)
        FetchContent_Populate(json)
        add_subdirectory(${json_SOURCE_DIR} ${json_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()
endif()

find_package(Qt6Keychain 0.13.2)

# sorta stolen from https://nheko.im/nheko-reborn/nheko/-/blob/master/CMakeLists.txt
if (NOT Qt6Keychain_FOUND)
    message("Fetching qtkeychain from GitHub...")

    set(USED_GITHUB_QTKEYCHAIN ON)
    set(BUILD_TEST_APPLICATION OFF CACHE INTERNAL "")
    set(BUILD_WITH_QT6 ON CACHE INTERNAL "")
    # build qtkeychain statically to avoid Qt deployment issues
    set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")

    include(FetchContent)
    FetchContent_Declare(qt6keychain
        GIT_REPOSITORY https://github.com/frankosterfeld/qtkeychain
        GIT_TAG v0.13.2
    )

    FetchContent_GetProperties(qt6keychain)
    if(NOT qt6keychain_POPULATED)
        FetchContent_Populate(qt6keychain)
        add_subdirectory(${qt6keychain_SOURCE_DIR} ${qt6keychain_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()
endif()

find_package(spdlog 1.10.0)

if(NOT spdlog_FOUND)
    message("Fetching spdlog from GitHub...")

    include(FetchContent)
    FetchContent_Declare(spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog
        GIT_TAG v1.10.0
    )

    FetchContent_GetProperties(spdlog)
    if(NOT spdlog_POPULATED)
        FetchContent_Populate(spdlog)
        add_subdirectory(${spdlog_SOURCE_DIR} ${spdlog_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()

#    FetchContent_MakeAvailable(spdlog)
endif()

find_package(Backward)

if (NOT Backward_FOUND)
    message("Fetching backward-cpp from GitHub...")

    include(FetchContent)
    FetchContent_Declare(Backward
        GIT_REPOSITORY https://github.com/bombela/backward-cpp
        GIT_TAG v1.6
    )
    FetchContent_MakeAvailable(Backward)
endif()

set(QAPPLICATION_CLASS QApplication CACHE STRING "Inheritance class for SingleApplication")
add_subdirectory(singleapplication)

configure_file(src/version.h.in version.h)

set(SRC_CLOCKIFY
    src/ClockifyManager.cpp
    src/ClockifyManager.h
)

set(SRC_TIMECAMP
    src/TimeCampManager.cpp
    src/TimeCampManager.h
)

# put in some platform-specific configuration stuff
if(WIN32)
    set(OS_HACKS "WIN32")
    set(APP_ICON_RESOURCE "winres.rc")
    set(INSTALL_TYPE "RUNTIME")
elseif(APPLE)
    set(OS_HACKS "MACOSX_BUNDLE")
    set(APP_ICON_RESOURCE "icons/macicon.icns")
    set(INSTALL_TYPE "BUNDLE")
    # Apparently this is not always set on macOS: https://github.com/ChristianLightServices/TimeLight/runs/6763058520
    set(CMAKE_INSTALL_BINDIR "/bin")
else()
    set(OS_HACKS "")
    set(APP_ICON_RESOURCE "")
    set(INSTALL_TYPE "RUNTIME")
endif()

set(SRC
    ${SRC_CLOCKIFY}
    ${SRC_TIMECAMP}

    src/AbstractTimeServiceManager.cpp
    src/AbstractTimeServiceManager.h
    src/DailyOverviewDialog.cpp
    src/DailyOverviewDialog.h
    src/DeveloperTools.cpp
    src/DeveloperTools.h
    src/JsonHelper.h
    src/Logger.cpp
    src/Logger.h
    src/main.cpp
    src/ModifyJobDialog.cpp
    src/ModifyJobDialog.h
    src/Project.cpp
    src/Project.h
    src/Settings.cpp
    src/Settings.h
    src/SettingsDialog.cpp
    src/SettingsDialog.h
    src/SetupFlow.cpp
    src/SetupFlow.h
    src/TeamsClient.cpp
    src/TeamsClient.h
    src/TimeEntry.cpp
    src/TimeEntry.h
    src/TimeEntryStore.cpp
    src/TimeEntryStore.h
    src/TrayIcons.cpp
    src/TrayIcons.h
    src/User.cpp
    src/User.h
    src/Utils.h
    src/Utils.cpp
    src/Workspace.cpp
    src/Workspace.h
)

set(TRANSLATIONS
    lang/TimeLight_en_US.ts
)

qt_add_executable(TimeLight ${OS_HACKS}
    ${SRC}
    ${APP_ICON_RESOURCE}
    ${TRANSLATIONS}
    res.qrc
)
qt_add_translations(TimeLight TS_FILES ${TRANSLATIONS} SOURCES ${SRC})

# also stolen from https://nheko.im/nheko-reborn/nheko/-/blob/master/CMakeLists.txt
# supposedly needed to make qtkeychain bundling work
if (USED_GITHUB_QTKEYCHAIN)
    target_include_directories(TimeLight PRIVATE ${qt6keychain_SOURCE_DIR} ${qt6keychain_BINARY_DIR})
endif()

if(APPLE)
    set_target_properties(TimeLight PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_NAME}"
        MACOSX_BUNDLE_BUNDLE_VERSION ${TimeLight_VERSION_STR}
        MACOSX_BUNDLE_COPYRIGHT "Copyright © 2021 Christian Light"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.christianlight.TimeLight"
        MACOSX_BUNDLE_INFO_STRING "${PROJECT_NAME}"
        MACOSX_BUNDLE_LONG_VERSION_STRING ${TimeLight_VERSION_STR}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${TimeLight_VERSION_STR}
        MACOSX_BUNDLE_ICON_FILE "icons/macicon.icns"
    )
endif()

target_link_libraries(TimeLight PRIVATE
    Qt6::Network
    Qt6::NetworkAuth
    Qt6::Widgets
    Qt6::Svg
    qt6keychain
    spdlog::spdlog_header_only
    SingleApplication::SingleApplication
    nlohmann_json::nlohmann_json
)

add_backward(TimeLight)

install(TARGETS TimeLight "${INSTALL_TYPE}" DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT "${PROJECT_NAME}")
set(CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT "${PROJECT_NAME}")
if(WIN32)
    include(InstallRequiredSystemLibraries)
endif()
if(UNIX AND NOT APPLE)
    install(FILES "TimeLight.desktop" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications" COMPONENT "${PROJECT_NAME}")
    install(FILES "icons/TimeLight.svg" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps" COMPONENT "${PROJECT_NAME}")
endif()

if(GENERATE_INSTALLER)
    # Set up the installer
    # https://www.ics.com/blog/revisiting-qt-installer-framework-cmake and https://cmake.org/cmake/help/latest/cpack_gen/ifw.html
    # were very helpful in creating the installer code
    set(CPACK_GENERATOR "IFW")
    set(CPACK_IFW_PACKAGE_NAME "${PROJECT_NAME}")
    set(CPACK_PACKAGE_VENDOR "Christian Light")
    set(CPACK_PACKAGE_VERSION ${TimeLight_VERSION_STR})
    set(CPACK_IFW_PACKAGE_TITLE ${CPACK_IFW_PACKAGE_NAME})
    set(CPACK_IFW_PRODUCT_URL "https://github.com/ChristianLightServices/TimeLight")
    set(CPACK_IFW_PACKAGE_WIZARD_DEFAULT_WIDTH 640)
    set(CPACK_IFW_PACKAGE_WIZARD_DEFAULT_HEIGHT 480)
    set(CPACK_IFW_PACKAGE_WIZARD_STYLE "Modern")
    set(CPACK_IFW_TARGET_DIRECTORY "@ApplicationsDirUser@/Christian Light")
    if(UNIX AND NOT APPLE)
        set(CPACK_IFW_TARGET_DIRECTORY "~/.local/${CPACK_PACKAGE_INSTALL_DIRECTORY}")
    endif()
    if(WIN32)
        set(CPACK_IFW_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/icons/winicon.ico")
    elseif(APPLE)
        set(CPACK_IFW_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/icons/macicon.ico")
    endif()
    set(CPACK_IFW_PACKAGE_WINDOW_ICON "${CMAKE_SOURCE_DIR}/installer/icons/WindowIcon.png")
    set(CPACK_IFW_PACKAGE_LOGO "${CMAKE_SOURCE_DIR}/installer/icons/Logo.png")
    set(CPACK_IFW_PACKAGE_START_MENU_DIRECTORY "Christian Light")
    set(CPACK_IFW_PACKAGE_RUN_PROGRAM "@TargetDir@/bin/TimeLight")
    set(CPACK_IFW_PACKAGE_CONTROL_SCRIPT "${CMAKE_SOURCE_DIR}/installer/controller.qs")

    if(WIN32)
        # set up windeployqt
        find_program(WINDEPLOYQT windeployqt HINTS "${_qt_bin_dir}")
        set(WINDEPLOYQT_COMPONENT "${PROJECT_NAME}")
        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/windeployqt.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/windeployqt.cmake" @ONLY)
        set(CPACK_PRE_BUILD_SCRIPTS "${CMAKE_CURRENT_BINARY_DIR}/windeployqt.cmake")
    endif()

    include(CPack)
    include(CPackIFW)

    cpack_add_component(TimeLight
        DISPLAY_NAME "${PROJECT_NAME}"
        DESCRIPTION "A button for controlling time tracker services from your system tray."
        REQUIRED
    )

    cpack_ifw_configure_component(TimeLight
        FORCED_INSTALLATION
        LICENSES "License" LICENSE
        SCRIPT "${CMAKE_SOURCE_DIR}/installer/components.qs"
        USER_INTERFACES "${CMAKE_SOURCE_DIR}/installer/pages/InstallType.ui"
    )
endif()
