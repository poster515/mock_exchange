#pragma once

#include "IPollRunner.h"
#include "PollableConcept.h"

namespace polling
{
    using PollableFactory = std::function<std::unique_ptr<IPollable>()>;
    
    class RuntimePollRunner : public IPollRunner
    {
    public:
        RuntimePollRunner(const std::vector<PollableFactory>& builders) {
            for (auto builder : builders) { pollables.emplace_back(std::invoke(builder)); }
        }

        size_t PollAll() override final {
            size_t work_done = 0;
            for (auto& pollable : pollables) work_done += pollable->PollOnce();
            return work_done;
        }

        bool StartPolling() override final {
            bool all_success = true;
            for (auto& pollable : pollables) all_success &= pollable->Initialize();
            return all_success;
        }

        void StopPolling() override final {
            for (auto& pollable : pollables) pollable->StopPolling();
        }

    private:
        std::vector<std::unique_ptr<IPollable>> pollables;
    };
}