#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QSettings>
#include <QInputDialog>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QMessageBox>
#include <QDesktopServices>
#include <QMenu>

#include <SingleApplication>

#include "JsonHelper.h"
#include "ClockifyManager.h"
#include "ClockifyUser.h"
#include "TrayIcons.h"

const QByteArray WORKSPACE{"5e207622ba05e2483903da0d"};
const QByteArray TIME_301{"5e20d8e0ba05e2483904a186"};

int main(int argc, char *argv[])
{
	QApplication::setApplicationName("ClockifyTrayIcons");
	QApplication::setOrganizationName("Christian Light");

	SingleApplication a{argc, argv};

	QSettings settings;
	QString apiKey = settings.value("apiKey").toString();

	while (apiKey == "")
	{
		apiKey = QInputDialog::getText(nullptr, "API key", "Enter your Clockify API key:");
		settings.setValue("apiKey", apiKey);
	}

	QSharedPointer<ClockifyManager> manager{new ClockifyManager{WORKSPACE, apiKey.toUtf8(), &a}};
	if (!manager->isValid())
	{
		QMessageBox::critical(nullptr,
							  "Fatal error", "Could not load information from Clockify. "
							  "Please check your internet connection and run this program again.");
		return 1;
	}
	QObject::connect(manager.data(), &ClockifyManager::invalidated, &a, []() {
		QMessageBox::critical(nullptr,
							  "Fatal error", "Could not load information from Clockify. "
							  "Please check your internet connection and run this program again.");
		QApplication::exit(1);
	});

	QSharedPointer<ClockifyUser> user{manager->getApiKeyOwner()};

	QObject::connect(manager.data(), &ClockifyManager::apiKeyChanged, &a, [&]() {
		auto temp = manager->getApiKeyOwner();
		if (temp != nullptr)
			user = QSharedPointer<ClockifyUser>{temp};
		else
			QMessageBox::warning(nullptr, "Operation failed", "Could not change API key!");
	});

	TrayIcons icons{manager, user};
	icons.show();

	return a.exec();
}
