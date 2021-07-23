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

	QSettings settings;
	QString apiKey = settings.value("apiKey").toString();

	while (apiKey == "")
	{
		apiKey = QInputDialog::getText(nullptr, "API key", "Enter your Clockify API key:");
		settings.setValue("apiKey", apiKey);
	}

	ClockifyManager manager{WORKSPACE, apiKey.toUtf8(), &a};
	if (!manager.isValid())
	{
		QMessageBox::critical(nullptr,
							  "Fatal error", "Could not load information from Clockify. "
							  "Please check your internet connection and run this program again.");
		return 1;
	}
	QObject::connect(&manager, &ClockifyManager::invalidated, &a, []() {
		QMessageBox::critical(nullptr,
							  "Fatal error", "Could not load information from Clockify. "
							  "Please check your internet connection and run this program again.");
		QApplication::exit(1);
	});

	QString projectId = settings.value("projectId").toString();
	while (projectId == "")
	{
		auto projects = manager.projects();
		QStringList projectIds;
		QStringList projectNames;
		for (auto project : projects)
		{
			projectIds.push_back(project.first);
			projectNames.push_back(project.second);
		}

		auto projectName = QInputDialog::getItem(nullptr, "Default project", "Select your default project", projectNames);
		projectId = projectIds[projectNames.indexOf(projectName)];
		settings.setValue("projectId", projectId);
	}

	QSharedPointer<ClockifyUser> user{manager.getApiKeyOwner()};

	QObject::connect(&manager, &ClockifyManager::apiKeyChanged, &a, [&]() {
		auto temp = manager.getApiKeyOwner();
		if (temp != nullptr)
			user = QSharedPointer<ClockifyUser>{temp};
		else
			QMessageBox::warning(nullptr, "Operation failed", "Could not change API key!");
	});

	QPair<QString, QIcon> clockifyOn{"Clockify is running", QIcon{":/greenpower.png"}};
	QPair<QString, QIcon> clockifyOff{"Clockify is not running", QIcon{":/redpower.png"}};
	QPair<QString, QIcon> onBreak{"You are on break", QIcon{":/yellowlight.png"}};
	QPair<QString, QIcon> working{"You are working", QIcon{":/greenlight.png"}};
	QPair<QString, QIcon> notWorking{"You are not working", QIcon{":/redlight.png"}};
	QPair<QString, QIcon> powerNotConnected{"You are offline", QIcon{":/graypower.png"}};
	QPair<QString, QIcon> runningNotConnected{"You are offline", QIcon{":/graylight.png"}};

	QSystemTrayIcon clockifyRunning{clockifyOff.second};
	QSystemTrayIcon runningJob{notWorking.second};

	clockifyRunning.setToolTip(clockifyOff.first);
	runningJob.setToolTip(notWorking.first);

	QMenu clockifyRunningMenu;
	QMenu runningJobMenu;

	// some well-used lambdas
	auto updateTrayIcons = [&](){
		if (!manager.isConnectedToInternet())
		{
			clockifyRunning.setToolTip(powerNotConnected.first);
			runningJob.setToolTip(runningNotConnected.first);

			clockifyRunning.setIcon(powerNotConnected.second);
			runningJob.setIcon(runningNotConnected.second);
		}
		else if (user->hasRunningTimeEntry())
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
	auto getNewProjectId = [&]() {
		auto projects = manager.projects();
		QStringList projectIds;
		QStringList projectNames;
		for (auto project : projects)
		{
			projectIds.push_back(project.first);
			projectNames.push_back(project.second);
		}

		bool ok{true};
		do
		{
			auto projectName = QInputDialog::getItem(nullptr, "Default project", "Select your default project", projectNames, projectIds.indexOf(projectId), false, &ok);
			if (!ok)
				return;
			projectId = projectIds[projectNames.indexOf(projectName)];
		}
		while (projectId == "");

		settings.setValue("projectId", projectId);
	};
	auto getNewApiKey = [&]() {
		bool ok{true};
		do
		{
			apiKey = QInputDialog::getText(nullptr, "API key", "Enter your Clockify API key:", QLineEdit::Normal, apiKey, &ok);
			if (!ok)
				return;
		}
		while (apiKey == "");

		manager.setApiKey(apiKey);
		settings.setValue("apiKey", apiKey);
	};

	updateTrayIcons();

	// set up the menu actions
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
	QObject::connect(clockifyRunningMenu.addAction("Change default project"), &QAction::triggered, &a, getNewProjectId);
	QObject::connect(clockifyRunningMenu.addAction("Change API key"), &QAction::triggered, &a, getNewApiKey);
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
	QObject::connect(runningJobMenu.addAction("Change default project"), &QAction::triggered, &a, getNewProjectId);
	QObject::connect(runningJobMenu.addAction("Change API key"), &QAction::triggered, &a, getNewApiKey);
	QObject::connect(runningJobMenu.addAction("Quit"), &QAction::triggered, &a, &SingleApplication::quit);

	// set up the actions on icon click
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
