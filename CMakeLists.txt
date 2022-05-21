cmake_minimum_required(VERSION 3.14)

option(GENERATE_INSTALLER "Generate an installer." OFF)

project(TimeLight VERSION 1.0.1 LANGUAGES CXX)

set(TimeLight_VERSION_STR "${TimeLight_VERSION_MAJOR}.${TimeLight_VERSION_MINOR}.${TimeLight_VERSION_PATCH}")

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Installer data
set(CPACK_GENERATOR "IFW")
set(CPACK_IFW_PACKAGE_NAME "TimeLight")
set(CPACK_PACKAGE_VERSION ${TimeLight_VERSION_STR})
set(CPACK_IFW_PACKAGE_TITLE ${CPACK_IFW_PACKAGE_NAME})
set(CPACK_IFW_PRODUCT_URL "https://github.com/ChristianLightServices/TimeLight")
set(CPACK_IFW_PACKAGE_WIZARD_DEFAULT_WIDTH 640)
set(CPACK_IFW_PACKAGE_WIZARD_DEFAULT_HEIGHT 480)
set(CPACK_IFW_PACKAGE_WIZARD_STYLE "Aero")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Christian Light")
if(UNIX AND NOT APPLE)
    set(CPACK_IFW_TARGET_DIRECTORY "~/.local/${CPACK_PACKAGE_INSTALL_DIRECTORY}")
endif()
if(WIN32)
    set(CPACK_IFW_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/icons/winicon.ico")
elseif(APPLE)
    set(CPACK_IFW_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/icons/macicon.ico")
endif()
set(CPACK_IFW_PACKAGE_WINDOW_ICON "${CMAKE_SOURCE_DIR}/icons/greenlight.png")
set(CPACK_IFW_PACKAGE_LOGO "${CMAKE_SOURCE_DIR}/icons/greenlight.png")
set(CPACK_IFW_PACKAGE_START_MENU_DIRECTORY "Christian Light")

include(CPack)
include(CPackIFW)

find_package(Qt6 6.2 COMPONENTS Core Network Widgets LinguistTools REQUIRED)

# some of the following several lines of CMake shamelessly stolen from https://github.com/ArthurSonzogni/nlohmann_json_cmake_fetchcontent
# (since I'm using the repo, I don't feel too bad about using their example CMake stuff)
find_package(nlohmann_json 3.2.0)

if(NOT nlohmann_json_FOUND)
    include(FetchContent)

    FetchContent_Declare(json
      GIT_REPOSITORY https://github.com/ArthurSonzogni/nlohmann_json_cmake_fetchcontent
      GIT_TAG v3.10.0
    )

    FetchContent_GetProperties(json)
    if(NOT json_POPULATED)
      FetchContent_Populate(json)
      add_subdirectory(${json_SOURCE_DIR} ${json_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()
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
elseif(APPLE)
    set(OS_HACKS "MACOSX_BUNDLE")
    set(APP_ICON_RESOURCE "icons/macicon.icns")
else()
    set(OS_HACKS "")
    set(APP_ICON_RESOURCE "")
endif()

set(SRC
    ${SRC_CLOCKIFY}
    ${SRC_TIMECAMP}

    src/AbstractTimeServiceManager.cpp
    src/AbstractTimeServiceManager.h
    src/JsonHelper.h
    src/main.cpp
    src/Project.cpp
    src/Project.h
    src/Settings.cpp
    src/Settings.h
    src/SettingsDialog.cpp
    src/SettingsDialog.h
    src/TimeEntry.cpp
    src/TimeEntry.h
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
    TimeLight_en_US.ts
)

set(LIBRARIES
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Widgets
    SingleApplication::SingleApplication
    nlohmann_json::nlohmann_json
)

qt_add_executable(TimeLight ${OS_HACKS}
    ${SRC}
    ${APP_ICON_RESOURCE}
    ${TRANSLATIONS}
    res.qrc
)
qt_add_translations(TimeLight TS_FILES ${TRANSLATIONS} SOURCES ${SRC})

if(APPLE)
    set_target_properties(TimeLight PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "TimeLight"
        MACOSX_BUNDLE_BUNDLE_VERSION ${TimeLight_VERSION_STR}
        MACOSX_BUNDLE_COPYRIGHT "Copyright © 2021 Christian Light"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.christianlight.TimeLight"
        MACOSX_BUNDLE_INFO_STRING "TimeLight"
        MACOSX_BUNDLE_LONG_VERSION_STRING ${TimeLight_VERSION_STR}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${TimeLight_VERSION_STR}
        MACOSX_BUNDLE_ICON_FILE "icons/macicon.icns"
    )
endif()

target_link_libraries(TimeLight PRIVATE ${LIBRARIES})

install(TARGETS TimeLight RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT "TimeLight")
if(UNIX AND NOT APPLE)
    install(FILES "TimeLight.desktop" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")
    install(FILES "icons/TimeLight.svg" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps")
endif()

cpack_add_component(TimeLight
    DISPLAY_NAME "TimeLight"
    DESCRIPTION "A button for controlling time tracker services from your system tray."
    REQUIRED
)

cpack_ifw_configure_component(TimeLight
    FORCED_INSTALLATION
    LICENSES "License" LICENSE
)
