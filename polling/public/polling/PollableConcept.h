#pragma once

#include <type_traits>
#include <concepts>
#include <functional>

namespace polling
{
    template <typename Pollable_T>
    concept CPollableType = requires(Pollable_T pollable)
    {
        { pollable.PollOnce() } -> std::convertible_to<void>;
        { pollable.Initialize() } -> std::convertible_to<bool>;
        { pollable.StopPolling() } -> std::convertible_to<void>;
    };

    class IPollable {
    public:
        virtual ~IPollable() = default;
        virtual void PollOnce() = 0;
        virtual void Initialize() = 0;
        virtual void StopPolling() = 0;
    };

    template <typename PollableBuilder_T>
    concept CPollableBuilder = std::is_invocable_r_v<std::unique_ptr<IPollable>, PollableBuilder_T>;
}