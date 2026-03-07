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

    /**
     * Concept that represents a type that can be built into a pollable. This is useful for the 
     * RuntimePollRunnerBuilder, which needs to be able to construct pollables in place without 
     * knowing their types at compile time.
     */
    template <typename PollableBuilder_T, typename Pollable_T = typename PollableBuilder_T::PollableType>
    concept CPollableBuilder = CPollableType<Pollable_T> && requires(PollableBuilder_T builder, void* at_memory)
    {
        { builder.BuildPollable(at_memory) } -> std::convertible_to<void>;
    };
}