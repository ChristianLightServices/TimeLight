#include "Logger.h"

#include <QStandardPaths>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/ansicolor_sink.h>

namespace TimeLight::logs
{
    std::shared_ptr<spdlog::logger> _app, _teams, _network, _qt;
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
    auto file = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation).append("/log.txt").toStdString());
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
