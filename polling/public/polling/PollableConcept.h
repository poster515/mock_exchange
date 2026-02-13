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
}