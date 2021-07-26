cmake_minimum_required(VERSION 3.14)

project(ClockifyModifier LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Core Network Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Network Widgets REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)

set(QAPPLICATION_CLASS QApplication CACHE STRING "Inheritance class for SingleApplication")
add_subdirectory(singleapplication)

set(SRC_COMMON
    ClockifyManager.cpp
    ClockifyUser.cpp
)

# make sure that Windows doesn't display a console
if(WIN32)
    set(IS_WIN "WIN32")
else()
    set(IS_WIN "")
endif()

add_executable(ClockifyModifier ${IS_WIN}
    ${SRC_COMMON}

    HeadlessRunner.cpp
)

add_executable(ClockifyTrayIcons ${IS_WIN}
    ${SRC_COMMON}

    ClockifyTrayIcons.cpp
    TrayIcons.cpp

    res.qrc
)

target_link_libraries(ClockifyModifier
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    nlohmann_json::nlohmann_json
)


target_link_libraries(ClockifyTrayIcons
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Widgets
    SingleApplication::SingleApplication
    nlohmann_json::nlohmann_json
)
