#pragma once

#include <QtGlobal>

#include <spdlog/spdlog.h>

namespace TimeLight::logs
{
    QString logFileLocation();
    void qtMessagesToSpdlog(QtMsgType type, const QMessageLogContext &context, const QString &msg);
    void init(bool debugMode);

    std::shared_ptr<spdlog::logger> app();
    std::shared_ptr<spdlog::logger> teams();
    std::shared_ptr<spdlog::logger> network();

} // namespace TimeLight::logs
