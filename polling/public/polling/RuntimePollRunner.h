#pragma once

#include "IPollRunner.h"
#include "PollableConcept.h"

namespace polling
{
    template <typename... Pollable_Ts>
        requires (CPollableType<Pollable_Ts> && ...)
    class RuntimePollRunner : public IPollRunner
    {
    public:
        template <typename...PollableBuilder_Ts>
            requires (CPollableBuilder<PollableBuilder_Ts> && ...)
        RuntimePollRunner(Pollable_Ts... pollables)
            : pollables(std::make_tuple(std::move(pollables)...)) {}

        void PollOnce() override final {
            std::apply([](auto&... pollables) { (pollables.PollOnce(), ...); }, pollables);
        }

        bool Initialize() override final {
            return std::apply([](auto&... pollables) { return (pollables.Initialize() && ...); }, pollables);
        }

        void StopPolling() override final {
            std::apply([](auto&... pollables) { (pollables.StopPolling(), ...); }, pollables);
        }

    private:
        std::tuple<Pollable_Ts...> pollables;
    };
}