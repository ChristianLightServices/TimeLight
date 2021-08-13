cmake_minimum_required(VERSION 3.14)

project(ClockifyModifier LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Core Network Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Network Widgets REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)

set(QAPPLICATION_CLASS QApplication CACHE STRING "Inheritance class for SingleApplication")
add_subdirectory(singleapplication)

set(SRC_CLOCKIFY
    ClockifyProject.cpp
    ClockifyManager.cpp
    ClockifyUser.cpp
    TimeEntry.cpp
)

# make sure that Windows doesn't display a console and add other Windows-specific stuff
if(WIN32)
    set(IS_WIN "WIN32")
    set(APP_ICON_RESOURCE_WINDOWS "winres.rc")
else()
    set(IS_WIN "")
    set(APP_ICON_RESOURCE_WINDOWS "")
endif()

add_executable(ClockifyTrayIcons ${IS_WIN}
    ${SRC_CLOCKIFY}

    ClockifyTrayIcons.cpp
    SelectDefaultProjectDialog.cpp
    TrayIcons.cpp

    res.qrc
    ${APP_ICON_RESOURCE_WINDOWS}
)

target_link_libraries(ClockifyTrayIcons
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Widgets
    SingleApplication::SingleApplication
    nlohmann_json::nlohmann_json
)
