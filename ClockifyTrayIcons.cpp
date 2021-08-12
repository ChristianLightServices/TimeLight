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
	QApplication::setQuitOnLastWindowClosed(false);

	SingleApplication a{argc, argv};

	QSettings settings;

	QString apiKey = settings.value("apiKey").toString();

	while (apiKey == "")
	{
		bool ok{false};
		apiKey = QInputDialog::getText(nullptr, "API key", "Enter your Clockify API key:", QLineEdit::Normal, QString{}, &ok);
		if (!ok)
			QApplication::exit(1);
		settings.setValue("apiKey", apiKey);
	}

	ClockifyManager::init(WORKSPACE, apiKey.toUtf8());
	while (!ClockifyManager::instance()->isValid())
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
		ClockifyManager::init(WORKSPACE, apiKey.toUtf8());
	}
	QObject::connect(ClockifyManager::instance().data(), &ClockifyManager::invalidated, &a, [&]() {
		while (!ClockifyManager::instance()->isValid())
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
			ClockifyManager::init(WORKSPACE, apiKey.toUtf8());
		}
	});

	QSharedPointer<ClockifyUser> user{ClockifyManager::instance()->getApiKeyOwner()};
	if (user.isNull()) [[unlikely]]
	{
		QMessageBox::warning(nullptr, "Fatal error", "Could not load user!");
		return 0;
	}

	QObject::connect(ClockifyManager::instance().data(), &ClockifyManager::apiKeyChanged, &a, [&]() {
		auto temp = ClockifyManager::instance()->getApiKeyOwner();
		if (temp != nullptr) [[likely]]
			*user = *temp;
		else [[unlikely]]
			QMessageBox::warning(nullptr, "Operation failed", "Could not change API key!");
	});

	TrayIcons icons{user};
	icons.show();

	return a.exec();
}
