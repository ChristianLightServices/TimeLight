#include "Logger.h"

#include <QStandardPaths>
#include <QDateTime>

#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <backward.hpp>

#include <csignal>

namespace TimeLight::logs
{
    std::shared_ptr<spdlog::logger> _app, _teams, _network, _qt;

    extern "C" void crashHandler(int signal);
}

QString TimeLight::logs::logFileLocation()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
}

void TimeLight::logs::qtMessagesToSpdlog(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    auto formatted = qFormatLogMessage(type, context, msg);

    switch (type)
    {
    case QtDebugMsg:
        _qt->debug(formatted.toStdString());
        break;
    case QtInfoMsg:
        _qt->info(formatted.toStdString());
        break;
    case QtWarningMsg:
        _qt->warn(formatted.toStdString());
        break;
    case QtCriticalMsg:
    case QtFatalMsg:
        _qt->critical(formatted.toStdString());
        break;
    default:
        break;
    }
}

void TimeLight::logs::init(bool debugMode)
{
    auto console = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();
    console->set_level(debugMode ? spdlog::level::debug : spdlog::level::warn);
    auto file = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFileLocation().append("/TimeLight.log").toStdString());
    file->set_level(spdlog::level::trace);
    std::vector<spdlog::sink_ptr> sinks = {console, file};

    _app = std::make_shared<spdlog::logger>("timelight", sinks.begin(), sinks.end());
    _app->set_level(spdlog::level::trace);
    _app->flush_on(spdlog::level::trace);
    _teams = std::make_shared<spdlog::logger>("teams", sinks.begin(), sinks.end());
    _teams->set_level(spdlog::level::trace);
    _teams->flush_on(spdlog::level::trace);
    _network = std::make_shared<spdlog::logger>("network", sinks.begin(), sinks.end());
    _network->set_level(spdlog::level::trace);
    _network->flush_on(spdlog::level::trace);
    _qt = std::make_shared<spdlog::logger>("qt", sinks.begin(), sinks.end());
    _qt->set_level(spdlog::level::trace);
    _qt->flush_on(spdlog::level::trace);

    qInstallMessageHandler(TimeLight::logs::qtMessagesToSpdlog);
    std::signal(SIGILL, crashHandler);
    std::signal(SIGSEGV, crashHandler);
    std::signal(SIGABRT, crashHandler);
    std::signal(SIGFPE, crashHandler);
}

std::shared_ptr<spdlog::logger> TimeLight::logs::app()
{
    return _app;
}

std::shared_ptr<spdlog::logger> TimeLight::logs::teams()
{
    return _teams;
}

std::shared_ptr<spdlog::logger> TimeLight::logs::network()
{
    return _network;
}

void TimeLight::logs::crashHandler(int signal)
{
    _app->critical("Crash signal detected! {}", signal);
    _app->critical("stack trace:");

    using namespace backward;

    StackTrace st;
    st.load_here(100);
    Printer p;
    p.address = true;
    p.object = true;
    p.print(st);

    auto filename{logFileLocation().toStdString() + "/backtrace-" + std::to_string(QDateTime::currentDateTime().toMSecsSinceEpoch()) + ".txt"};
    std::ofstream dump{filename};
    if (dump)
    {
        p.print(st, dump);
        _app->critical("Also dumped stack trace to {}", filename);
    }

    // clear logs
    _app->flush();
    _teams->flush();
    _network->flush();
    _qt->flush();

    // let's get the Genuine Crash Experience(R)
    std::signal(signal, SIG_DFL);
    std::raise(signal);
}
