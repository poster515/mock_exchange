#pragma once

#include <queue>
#include <optional>
#include <memory>

namespace polling
{
    class CoreAffinity {
    public:
        CoreAffinity();

        std::optional<int> try_get_isolated_core();

    private:
        void get_isolated_cores();
        std::queue<int> isolated_cores;
        std::mutex core_lock;
    };
}