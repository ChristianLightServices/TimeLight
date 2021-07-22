#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QSettings>
#include <QInputDialog>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QMessageBox>
#include <QMenu>

#include <SingleApplication>

#include "JsonHelper.h"
#include "ClockifyManager.h"
#include "ClockifyUser.h"
#include "Common.h"

int main(int argc, char *argv[])
{
	QApplication::setApplicationName("ClockifyEasyButtons");
	QApplication::setApplicationName("Christian Light");

	SingleApplication a{argc, argv};

	ClockifyManager manager{WORKSPACE, APIKEY, &a};
	if (!manager.isValid())
		return 1;
	QObject::connect(&manager, &ClockifyManager::invalidated, &a, []() {
		QMessageBox::critical(nullptr,
							  "Fatal error", "Could not load information from Clockify. "
							  "Please check your internet connection and run this program again.",
							  QMessageBox::Ok);
		QApplication::exit(1);
	});

	QString userId;
	QString projectId;
	QSettings settings;

	userId = settings.value("userId").toString();
	projectId = settings.value("projectId").toString();

	if (userId == "")
	{
		userId = QInputDialog::getText(nullptr, "User ID", "Enter your Clockify user ID:");
		settings.setValue("userId", userId);
	}
	if (projectId == "")
	{
		projectId = QInputDialog::getText(nullptr, "Default project", "Enter your default project ID:");
		settings.setValue("projectId", projectId);
	}

	if (userId.isEmpty() || projectId.isEmpty())
		return 2;

	ClockifyUser me{userId, &manager};

	QPair<QString, QIcon> clockifyOn{"Clockify is running", QIcon{":/greenpower.png"}};
	QPair<QString, QIcon> clockifyOff{"Clockify is not running", QIcon{":/redpower.png"}};
	QPair<QString, QIcon> onBreak{"You are on break", QIcon{":/yellowlight.png"}};
	QPair<QString, QIcon> working{"You are working", QIcon{":/greenlight.png"}};
	QPair<QString, QIcon> notWorking{"You are not working", QIcon{":/redlight.png"}};

	QSystemTrayIcon clockifyRunning{clockifyOff.second};
	QSystemTrayIcon runningJob{notWorking.second};

	clockifyRunning.setToolTip(clockifyOff.first);
	runningJob.setToolTip(notWorking.first);

	QMenu clockifyRunningMenu;
	QMenu runningJobMenu;

	auto updateTrayIcons = [&](){
		if (me.hasRunningTimeEntry())
		{
			clockifyRunning.setToolTip(clockifyOn.first);
			clockifyRunning.setIcon(clockifyOn.second);

			if (me.getRunningTimeEntry()[0]["projectId"].get<QString>() == BREAKTIME)
			{
				runningJob.setToolTip(onBreak.first);
				runningJob.setIcon(onBreak.second);
			}
			else
			{
				runningJob.setToolTip(working.first);
				runningJob.setIcon(working.second);
			}
		}
		else
		{
			clockifyRunning.setToolTip(clockifyOff.first);
			runningJob.setToolTip(notWorking.first);

			clockifyRunning.setIcon(clockifyOff.second);
			runningJob.setIcon(notWorking.second);
		}
	};

	QObject::connect(clockifyRunningMenu.addAction("Start"), &QAction::triggered, &a, [&]() {
		if (!me.hasRunningTimeEntry())
		{
			me.startTimeEntry(projectId);
			updateTrayIcons();
		}
	});
	QObject::connect(clockifyRunningMenu.addAction("Stop"), &QAction::triggered, &a, [&]() {
		if (me.hasRunningTimeEntry())
		{
			me.stopCurrentTimeEntry();
			updateTrayIcons();
		}
	});
	QObject::connect(clockifyRunningMenu.addAction("Quit"), &QAction::triggered, &a, &SingleApplication::quit);

	QObject::connect(runningJobMenu.addAction("Break"), &QAction::triggered, &a, [&]() {
		if (me.hasRunningTimeEntry())
			me.stopCurrentTimeEntry();
		me.startTimeEntry(BREAKTIME);
		updateTrayIcons();
	});
	QObject::connect(runningJobMenu.addAction("Resume work"), &QAction::triggered, &a, [&]() {
		if (me.hasRunningTimeEntry())
			me.stopCurrentTimeEntry();
		me.startTimeEntry(projectId);
		updateTrayIcons();
	});
	QObject::connect(runningJobMenu.addAction("Quit"), &QAction::triggered, &a, &SingleApplication::quit);

	QObject::connect(&clockifyRunning, &QSystemTrayIcon::activated, &a, [&]() {
		if (me.hasRunningTimeEntry())
			me.stopCurrentTimeEntry();
		else
			me.startTimeEntry(projectId);
		updateTrayIcons();
	});
	QObject::connect(&runningJob, &QSystemTrayIcon::activated, &a, [&]() {
		if (me.hasRunningTimeEntry())
		{
			if (me.getRunningTimeEntry()[0]["projectId"].get<QString>() == BREAKTIME)
			{
				me.stopCurrentTimeEntry();
				me.startTimeEntry(projectId);
			}
			else
			{
				me.stopCurrentTimeEntry();
				me.startTimeEntry(BREAKTIME);
			}
		}
		else
			me.startTimeEntry(projectId);
		updateTrayIcons();
	});

	clockifyRunning.setContextMenu(&clockifyRunningMenu);
	runningJob.setContextMenu(&runningJobMenu);

	clockifyRunning.show();
	runningJob.show();

	QTimer eventLoop;
	eventLoop.setInterval(500);
	eventLoop.setSingleShot(false);
	eventLoop.callOnTimeout(updateTrayIcons);
	eventLoop.start();

	return a.exec();
}
