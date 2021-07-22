cmake_minimum_required(VERSION 3.14)

project(ClockifyModifier LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Core Network REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Network REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)

add_executable(ClockifyModifier
    ClockifyManager.cpp
    ClockifyUser.cpp
    main.cpp
)
target_link_libraries(ClockifyModifier Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Network nlohmann_json::nlohmann_json)
