#include "polling/CoreAffinity.h"

namespace polling
{
    CoreAffinity::CoreAffinity() {
        get_isolated_cores();
    }

    std::optional<int> CoreAffinity::try_get_isolated_core() {
        std::lock_guard<std::mutex> guard(core_lock);
        if (!isolated_cores.empty()) {
            const auto core = isolated_cores.front();
            isolated_cores.pop();
            return core;
        }

        return std::nullopt;
    }

    void CoreAffinity::get_isolated_cores() {
        // TODO
    }
}