cmake_minimum_required(VERSION 3.14)

project(ClockifyTrayIcons VERSION 1.0.0 LANGUAGES CXX)

set(ClockifyTrayIcons_VERSION_STR "${ClockifyTrayIcons_VERSION_MAJOR}.${ClockifyTrayIcons_VERSION_MINOR}.${ClockifyTrayIcons_VERSION_PATCH}")

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# using 5.15 here doesn't mean that we can't use Qt 6, y'know
find_package(QT 5.15 NAMES Qt6 Qt5 COMPONENTS Core Network Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Network Widgets REQUIRED)

# some of the following several lines of CMake shamelessly stolen from https://github.com/ArthurSonzogni/nlohmann_json_cmake_fetchcontent
# (since I'm using the repo, I don't feel too bad about using their example CMake stuff)
# this is quiet because it just might not be found the first time around
find_package(nlohmann_json 3.2.0 QUIET)

if(NOT nlohmann_json_FOUND)
	include(FetchContent)

	FetchContent_Declare(json
	  GIT_REPOSITORY https://github.com/ArthurSonzogni/nlohmann_json_cmake_fetchcontent
	  GIT_TAG v3.10.0)

    FetchContent_GetProperties(json)
	if(NOT json_POPULATED)
	  FetchContent_Populate(json)
	  add_subdirectory(${json_SOURCE_DIR} ${json_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()
endif()

set(QAPPLICATION_CLASS QApplication CACHE STRING "Inheritance class for SingleApplication")
add_subdirectory(singleapplication)

configure_file(version.h.in version.h)

set(SRC_CLOCKIFY
    ClockifyProject.cpp
    ClockifyManager.cpp
    ClockifyUser.cpp
	ClockifyWorkspace.cpp
    TimeEntry.cpp
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

	main.cpp
	SelectDefaultProjectDialog.cpp
	SelectDefaultWorkspaceDialog.cpp
	TrayIcons.cpp

	res.qrc
	${APP_ICON_RESOURCE}
)

set(LIBRARIES
	Qt${QT_VERSION_MAJOR}::Network
	Qt${QT_VERSION_MAJOR}::Widgets
	SingleApplication::SingleApplication
	nlohmann_json::nlohmann_json
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
	qt_add_executable(ClockifyTrayIcons ${OS_HACKS} MANUAL_FINALIZATION ${SRC})
else()
	set(LIBRARIES
		Qt${QT_VERSION_MAJOR}::Core
		${LIBRARIES}
	)
    add_executable(ClockifyTrayIcons ${OS_HACKS} ${SRC})
endif()

if(APPLE)
	set_target_properties(ClockifyTrayIcons PROPERTIES
		MACOSX_BUNDLE_BUNDLE_NAME "ClockifyTrayIcons"
		MACOSX_BUNDLE_BUNDLE_VERSION ${ClockifyTrayIcons_VERSION_STR}
		MACOSX_BUNDLE_COPYRIGHT "Copyright © 2021 Christian Light"
		MACOSX_BUNDLE_GUI_IDENTIFIER "org.christianlight.ClockifyTrayIcons"
		MACOSX_BUNDLE_INFO_STRING "ClockifyTrayIcons"
		MACOSX_BUNDLE_LONG_VERSION_STRING ${ClockifyTrayIcons_VERSION_STR}
		MACOSX_BUNDLE_SHORT_VERSION_STRING ${ClockifyTrayIcons_VERSION_STR}
		MACOSX_BUNDLE_ICON_FILE "icons/macicon.icns"
	)
endif()

target_link_libraries(ClockifyTrayIcons PRIVATE ${LIBRARIES})

if(QT_VERSION_MAJOR EQUAL 6)
	qt_finalize_executable(ClockifyTrayIcons)
endif()
