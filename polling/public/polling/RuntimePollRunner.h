#pragma once

#include "IPollRunner.h"
#include "PollableConcept.h"

namespace polling
{
    class RuntimePollRunner : public IPollRunner
    {
    public:
        template <typename...PollableBuilder_Ts>
            requires (CPollableBuilder<PollableBuilder_Ts> && ...)
        RuntimePollRunner(PollableBuilder_Ts&&... builders) {
            (pollables.emplace_back(std::invoke(builders)), ... );
        }

        void PollAll() override final {
            for (auto& pollable : pollables) { pollable->PollOnce(); }
        }

        bool StartPolling() override final {
            for (auto& pollable : pollables) { pollable->Initialize(); }
        }

        void StopPolling() override final {
            for (auto& pollable : pollables) { pollable->StopPolling(); }
        }

    private:
        std::vector<std::unique_ptr<IPollable>> pollables;
    };
}