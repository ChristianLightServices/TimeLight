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
#include <QTranslator>

#include <SingleApplication>

#include "JsonHelper.h"
#include "ClockifyManager.h"
#include "User.h"
#include "TrayIcons.h"
#include "SelectDefaultWorkspaceDialog.h"

int main(int argc, char *argv[])
{
	QApplication::setApplicationName("ClockifyTrayIcons");
	QApplication::setOrganizationName("Christian Light");
	QApplication::setOrganizationDomain("org.christianlight");
	QApplication::setQuitOnLastWindowClosed(false);

	SingleApplication a{argc, argv};
	a.setWindowIcon(QIcon{":/icons/greenlight.png"});
	QTranslator translator;
	if (translator.load(QLocale{}, QStringLiteral("ClockifyTrayIcons"), QStringLiteral("_"), QStringLiteral(":/i18n")))
		QApplication::installTranslator(&translator);

	TrayIcons icons;
	if (!icons.valid())
		return 2;
	icons.show();

	return a.exec();
}
