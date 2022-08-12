#pragma once

#include <spdlog/spdlog.h>

namespace TimeLight::logs
{
    void init(bool debugMode);

    std::shared_ptr<spdlog::logger> app();
    std::shared_ptr<spdlog::logger> teams();
    std::shared_ptr<spdlog::logger> network();
}
