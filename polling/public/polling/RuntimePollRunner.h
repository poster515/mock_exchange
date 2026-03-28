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

        void PollAll() override final {
            for (auto& pollable : pollables) { pollable->PollOnce(); }
        }

        bool StartPolling() override final {
            for (auto& pollable : pollables) { pollable->Initialize(); }
            return true;
        }

        void StopPolling() override final {
            for (auto& pollable : pollables) { pollable->StopPolling(); }
        }

    private:
        std::vector<std::unique_ptr<IPollable>> pollables;
    };
}