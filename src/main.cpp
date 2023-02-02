#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QTranslator>

#include <SingleApplication>

#include <spdlog/spdlog.h>

#include "Logger.h"
#include "Settings.h"
#include "TrayIcons.h"
#include "Utils.h"
#include "version.h"

int main(int argc, char *argv[])
{
    QApplication::setApplicationName(QStringLiteral("TimeLight"));
    QApplication::setOrganizationName(QStringLiteral("Christian Light"));
    QApplication::setOrganizationDomain(QStringLiteral("org.christianlight"));
    QApplication::setQuitOnLastWindowClosed(false);

#if defined(IS_DEBUG_BUILD)
    // debug mode will always be on in debug builds
    TimeLight::logs::init(true);
#else
    bool debugMode{false};
    for (int i = 0; i < argc; ++i)
        if (std::strcmp(argv[i], "--debug") == 0)
        {
            debugMode = true;
            break;
        }
    TimeLight::logs::init(debugMode);
#endif

    int retCode{0};
    do
    {
        // Make sure argc and argv survive restarts. This has to be outside the try so the pointer can be freed in the catch
        // block if necessary.
        int newArgc = argc;
        auto newArgv = new char *[argc];
        for (int i = 0; i < argc; ++i)
            newArgv[i] = qstrdup(argv[i]);

        try
        {
            SingleApplication a{newArgc, newArgv};
            a.setWindowIcon(QIcon{QStringLiteral(":/icons/greenlight.png")});
            a.setApplicationVersion(QStringLiteral(VERSION_STR));

            QCommandLineParser parser;
            QCommandLineOption debug{"debug", QObject::tr("Print debug information to the command line.")};
            parser.addOption(debug);
            parser.addHelpOption();
            parser.addVersionOption();
            parser.process(a);

            TimeLight::logs::app()->trace("Starting TimeLight {}...", VERSION_STR);

            if (QTranslator translator;
                translator.load(QLocale{}, QStringLiteral("TimeLight"), QStringLiteral("_"), QStringLiteral(":/i18n")))
                QApplication::installTranslator(&translator);

            Settings::init();

            TrayIcons icons;
            if (!icons.valid())
                return 2;
            icons.show();

            retCode = a.exec();

            if (retCode == TimeLight::appRestartCode)
                TimeLight::logs::app()->debug("App restart requested");

            delete[] newArgv;
            newArgv = nullptr;
        }
        catch (const nlohmann::json::exception &e)
        {
            // hmm... Unhandled error. Let's just reboot the app and let the chips fall where they may.
            TimeLight::logs::app()->critical("Unhandled exception: {}", e.what());
            TimeLight::logs::app()->critical("Restarting the app...");
            retCode = TimeLight::appRestartCode;

            if (newArgv)
                delete[] newArgv;
        }
    } while (retCode == TimeLight::appRestartCode);

    TimeLight::logs::app()->trace("quitting");

    return retCode;
}
