#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDesktopServices>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QTranslator>

#include <SingleApplication>

#include "JsonHelper.h"
#include "Settings.h"
#include "TrayIcons.h"
#include "Utils.h"

int main(int argc, char *argv[])
{
    QApplication::setApplicationName(QStringLiteral("ClockifyTrayIcons"));
    QApplication::setOrganizationName(QStringLiteral("Christian Light"));

    // Migrate old settings to the new organizational format. This must
    // be the third or fourth migration I've written for this app.
    QSettings oldSettings{qApp->organizationName(), QStringLiteral("ClockifyTrayIcons")};
    // we'll assume that apiKey signals everything being non-migrated
    if (oldSettings.contains(QStringLiteral("apiKey")))
    {
        QSettings settings;
        // the following is highly dependent on the config file not being screwed up
        settings.setValue(QStringLiteral("com.clockify/apiKey"), oldSettings.value(QStringLiteral("apiKey")));
        settings.setValue(QStringLiteral("com.clockify/breakTimeId"), oldSettings.value(QStringLiteral("breakTimeId")));
        settings.setValue(QStringLiteral("com.clockify/description"), oldSettings.value(QStringLiteral("description")));
        settings.setValue(QStringLiteral("com.clockify/disableDescription"),
                          oldSettings.value(QStringLiteral("disableDescription"), false));
        settings.setValue(QStringLiteral("com.clockify/projectId"), oldSettings.value(QStringLiteral("projectId")));
        settings.setValue(QStringLiteral("com.clockify/useLastDescription"),
                          oldSettings.value(QStringLiteral("useLastDescription"), true));
        settings.setValue(QStringLiteral("com.clockify/useLastProject"),
                          oldSettings.value(QStringLiteral("useLastProject"), true));
        settings.setValue(QStringLiteral("com.clockify/workspaceId"), oldSettings.value(QStringLiteral("workspaceId")));

        settings.setValue(QStringLiteral("app/eventLoopInterval"),
                          oldSettings.value(QStringLiteral("eventLoopInterval"), 1000));
        settings.setValue(QStringLiteral("app/showDurationNotifications"),
                          oldSettings.value(QStringLiteral("showDurationNotifications"), true));

        // This setting "migration" is a special case: for new users, this setting will default to false,
        // but users who are upgrading from old versions should not have their workflows broken
        settings.setValue(QStringLiteral("com.clockify/useSeparateBreakTime"), true);

        // ...and now we can delete all of the old keys
        const QStringList oldKeys = {QStringLiteral("apiKey"),
                                     QStringLiteral("breakTimeId"),
                                     QStringLiteral("description"),
                                     QStringLiteral("disableDescription"),
                                     QStringLiteral("eventLoopInterval"),
                                     QStringLiteral("projectId"),
                                     QStringLiteral("showDurationNotifications"),
                                     QStringLiteral("useLastDescription"),
                                     QStringLiteral("useLastProject"),
                                     QStringLiteral("workspaceId")};
        for (const auto &key : oldKeys)
            oldSettings.remove(key);
    }
    // end migration

    QApplication::setOrganizationDomain(QStringLiteral("org.christianlight"));
    QApplication::setQuitOnLastWindowClosed(false);

    int retCode{0};
    do
    {
        SingleApplication a{argc, argv};
        a.setWindowIcon(QIcon{QStringLiteral(":/icons/greenlight.png")});

        QTranslator translator;
        if (translator.load(QLocale{}, QStringLiteral("ClockifyTrayIcons"), QStringLiteral("_"), QStringLiteral(":/i18n")))
            QApplication::installTranslator(&translator);

        Settings::init();

        TrayIcons icons;
        if (!icons.valid())
            return 2;
        icons.show();

        retCode = a.exec();
    } while (retCode == ClockifyTrayIcons::appRestartCode);

    return retCode;
}
