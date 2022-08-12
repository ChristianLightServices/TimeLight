#include "Logger.h"

#include <QStandardPaths>

#include <spdlog/sinks/basic_file_sink.h>

namespace TimeLight::logs
{
    std::shared_ptr<spdlog::logger> _app, _teams, _network;
}

void TimeLight::logs::init(bool debugMode)
{
    auto console = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();
    console->set_level(debugMode ? spdlog::level::trace : spdlog::level::warn);
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
