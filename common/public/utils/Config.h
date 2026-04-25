#pragma once

#include <signal.h>

#include <libconfig.h++>
#include <spdlog/sinks/daily_file_sink.h>
#include "polling/CoreAffinity.h"

namespace common {
    struct CommonComponents {
        const libconfig::Config& config;
        spdlog::logger& logger;
        std::shared_ptr<polling::CoreAffinity> cores;
    };

    class IApplicationService {
    public: 
        virtual void start() = 0;
        virtual void run() = 0;
        virtual void stop() = 0;

    protected:
        std::atomic_bool initialized { false };
        std::atomic_bool running { false };
    };
}