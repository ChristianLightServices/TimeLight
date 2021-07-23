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

const QByteArray WORKSPACE{"5e207622ba05e2483903da0d"};
const QByteArray TIME_301{"5e20d8e0ba05e2483904a186"};

int main(int argc, char *argv[])
{
	QApplication::setApplicationName("ClockifyEasyButtons");
	QApplication::setApplicationName("Christian Light");

	SingleApplication a{argc, argv};

	QString apiKey;
	QString projectId;
	QSettings settings;

	projectId = settings.value("projectId").toString();
	apiKey = settings.value("apiKey").toString();

	while (apiKey == "")
	{
		apiKey = QInputDialog::getText(nullptr, "API key", "Enter your Clockify API key:");
		settings.setValue("apiKey", apiKey);
	}
	while (projectId == "")
	{
		projectId = QInputDialog::getText(nullptr, "Default project", "Enter your default project ID:");
		settings.setValue("projectId", projectId);
	}

	ClockifyManager manager{WORKSPACE, apiKey.toUtf8(), &a};
	if (!manager.isValid())
		return 1;
	QObject::connect(&manager, &ClockifyManager::invalidated, &a, []() {
		QMessageBox::critical(nullptr,
							  "Fatal error", "Could not load information from Clockify. "
							  "Please check your internet connection and run this program again.",
							  QMessageBox::Ok);
		QApplication::exit(1);
	});

	QSharedPointer<ClockifyUser> user{manager.getApiKeyOwner()};

	QObject::connect(&manager, &ClockifyManager::apiKeyChanged, &a, [&]() {
		user = QSharedPointer<ClockifyUser>{manager.getApiKeyOwner()};
	});

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
		if (user->hasRunningTimeEntry())
		{
			clockifyRunning.setToolTip(clockifyOn.first);
			clockifyRunning.setIcon(clockifyOn.second);

			if (user->getRunningTimeEntry()[0]["projectId"].get<QString>() == TIME_301)
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
		if (!user->hasRunningTimeEntry())
		{
			user->startTimeEntry(projectId);
			updateTrayIcons();
		}
	});
	QObject::connect(clockifyRunningMenu.addAction("Stop"), &QAction::triggered, &a, [&]() {
		if (user->hasRunningTimeEntry())
		{
			user->stopCurrentTimeEntry();
			updateTrayIcons();
		}
	});
	QObject::connect(clockifyRunningMenu.addAction("Change default project"), &QAction::triggered, &a, [&]() {
		do
			projectId = QInputDialog::getText(nullptr, "Project ID", "Enter your default project ID:");
		while (projectId == "");

		settings.setValue("projectId", projectId);
	});
	QObject::connect(clockifyRunningMenu.addAction("Change API key"), &QAction::triggered, &a, [&]() {
		do
			apiKey = QInputDialog::getText(nullptr, "API key", "Enter your Clockify API key:");
		while (apiKey == "");

		manager.setApiKey(apiKey);
		settings.setValue("apiKey", apiKey);
	});
	QObject::connect(clockifyRunningMenu.addAction("Quit"), &QAction::triggered, &a, &SingleApplication::quit);

	QObject::connect(runningJobMenu.addAction("Break"), &QAction::triggered, &a, [&]() {
		QDateTime start = QDateTime::currentDateTimeUtc();

		if (user->hasRunningTimeEntry())
			start = user->stopCurrentTimeEntry();
		user->startTimeEntry(TIME_301, start);
		updateTrayIcons();
	});
	QObject::connect(runningJobMenu.addAction("Resume work"), &QAction::triggered, &a, [&]() {
		QDateTime start = QDateTime::currentDateTimeUtc();

		if (user->hasRunningTimeEntry())
			start = user->stopCurrentTimeEntry();
		user->startTimeEntry(projectId, start);
		updateTrayIcons();
	});
	QObject::connect(runningJobMenu.addAction("Change default project"), &QAction::triggered, &a, [&]() {
		do
			projectId = QInputDialog::getText(nullptr, "Project ID", "Enter your default project ID:");
		while (projectId == "");

		settings.setValue("projectId", projectId);
	});
	QObject::connect(runningJobMenu.addAction("Change API key"), &QAction::triggered, &a, [&]() {
		do
			apiKey = QInputDialog::getText(nullptr, "API key", "Enter your Clockify API key:");
		while (apiKey == "");

		manager.setApiKey(apiKey);
		settings.setValue("apiKey", apiKey);
	});
	QObject::connect(runningJobMenu.addAction("Quit"), &QAction::triggered, &a, &SingleApplication::quit);

	QObject::connect(&clockifyRunning, &QSystemTrayIcon::activated, &a, [&]() {
		if (user->hasRunningTimeEntry())
			user->stopCurrentTimeEntry();
		else
			user->startTimeEntry(projectId);
		updateTrayIcons();
	});
	QObject::connect(&runningJob, &QSystemTrayIcon::activated, &a, [&]() {
		if (user->hasRunningTimeEntry())
		{
			if (user->getRunningTimeEntry()[0]["projectId"].get<QString>() == TIME_301)
			{
				auto time = user->stopCurrentTimeEntry();
				user->startTimeEntry(projectId, time);
			}
			else
			{
				auto time = user->stopCurrentTimeEntry();
				user->startTimeEntry(TIME_301, time);
			}
		}
		else
			user->startTimeEntry(projectId);
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
