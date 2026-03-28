#pragma once

#include <libconfig.h++>
#include <spdlog/sinks/daily_file_sink.h>

namespace common {
    struct CommonComponents {
        const libconfig::Config& config;
        spdlog::logger& logger;
    };

    class IApplicationService {
    public: 
        virtual void run() = 0;
        virtual void stop() = 0;

    protected:
        std::atomic_bool initialized { false };
        std::atomic_bool running { false };
    };
}