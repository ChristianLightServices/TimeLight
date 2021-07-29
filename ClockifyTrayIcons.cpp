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

const QByteArray WORKSPACE{"redacted"};
const QByteArray BREAKTIME{"redacted"};

int main(int argc, char *argv[])
{
	QApplication::setApplicationName("ClockifyTrayIcons");
	QApplication::setOrganizationName("Christian Light");
	QApplication::setQuitOnLastWindowClosed(false);

	SingleApplication a{argc, argv};

	// migrate old keys
	QSettings migration{QApplication::organizationName(), "ClockifyEasyButtons"};

	QSettings settings;

	// TODO: remove this after everybody has been migrated
	if (migration.allKeys().length() > 0)
		for (const auto &key : migration.allKeys())
			settings.setValue(key, migration.value(key));

	migration.clear();

	QString apiKey = settings.value("apiKey").toString();

	while (apiKey == "")
	{
		bool ok{false};
		apiKey = QInputDialog::getText(nullptr, "API key", "Enter your Clockify API key:", QLineEdit::Normal, QString{}, &ok);
		if (!ok)
			QApplication::exit(1);
		settings.setValue("apiKey", apiKey);
	}

	QSharedPointer<ClockifyManager> manager{new ClockifyManager{WORKSPACE, apiKey.toUtf8(), &a}};
	while (!manager->isValid())
	{
		bool ok{false};
		apiKey = QInputDialog::getText(nullptr,
									   "API key",
									   "The API key is incorrect or invalid. Please enter a valid API key:",
									   QLineEdit::Normal,
									   QString{},
									   &ok);
		if (!ok)
			QApplication::exit(1);
		settings.setValue("apiKey", apiKey);
		manager.reset(new ClockifyManager{WORKSPACE, apiKey.toUtf8(), &a});
	}
	QObject::connect(manager.data(), &ClockifyManager::invalidated, &a, [&]() {
		while (!manager->isValid())
		{
			bool ok{false};
			apiKey = QInputDialog::getText(nullptr,
										   "API key",
										   "The API key is incorrect or invalid. Please enter a valid API key:",
										   QLineEdit::Normal,
										   QString{},
										   &ok);
			if (!ok)
				QApplication::exit(1);
			settings.setValue("apiKey", apiKey);
			manager.reset(new ClockifyManager{WORKSPACE, apiKey.toUtf8(), &a});
		}
	});

	QSharedPointer<ClockifyUser> user{manager->getApiKeyOwner()};
	if (user.isNull())
	{
		QMessageBox::warning(nullptr, "Fatal error", "Could not load user!");
		return 0;
	}

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
