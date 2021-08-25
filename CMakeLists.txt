cmake_minimum_required(VERSION 3.14)

project(ClockifyTrayIcons VERSION 0.0.0.1 LANGUAGES CXX)

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
	set(OS_HACKS "WIN32")
    set(APP_ICON_RESOURCE_WINDOWS "winres.rc")
elseif(APPLE)
	set(OS_HACKS "MACOSX_BUNDLE")
	set(APP_ICON_RESOURCE_WINDOWS "winres.rc")
else()
	set(OS_HACKS "")
    set(APP_ICON_RESOURCE_WINDOWS "")
endif()

set(SRC
	${SRC_CLOCKIFY}

	ClockifyTrayIcons.cpp
	SelectDefaultProjectDialog.cpp
	TrayIcons.cpp

	res.qrc
	${APP_ICON_RESOURCE_WINDOWS}
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

target_link_libraries(ClockifyTrayIcons PRIVATE ${LIBRARIES})

if(QT_VERSION_MAJOR EQUAL 6)
	qt_finalize_executable(ClockifyTrayIcons)
endif()
