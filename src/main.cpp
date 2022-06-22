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
    QApplication::setApplicationName(QStringLiteral("TimeLight"));
    QApplication::setOrganizationName(QStringLiteral("Christian Light"));
    QApplication::setOrganizationDomain(QStringLiteral("org.christianlight"));
    QApplication::setQuitOnLastWindowClosed(false);

    int retCode{0};
    do
    {
        try
        {
            SingleApplication a{argc, argv};
            a.setWindowIcon(QIcon{QStringLiteral(":/icons/greenlight.png")});

            if (QTranslator translator;
                translator.load(QLocale{}, QStringLiteral("TimeLight"), QStringLiteral("_"), QStringLiteral(":/i18n")))
                QApplication::installTranslator(&translator);

            Settings::init();

            TrayIcons icons;
            if (!icons.valid())
                return 2;
            icons.show();

            retCode = a.exec();
        }
        catch (const nlohmann::json::exception &e)
        {
            // hmm... Unhandled error. Let's just reboot the app and let the chips fall where they may.
            std::cerr << "Unhandled exception: " << e.what() << "\n";
            std::cerr << "Restarting the app..." << std::endl;
            retCode = TimeLight::appRestartCode;
        }
    } while (retCode == TimeLight::appRestartCode);

    return retCode;
}
