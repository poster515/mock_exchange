#pragma once

#include <type_traits>
#include <concepts>
#include <functional>
#include <memory>

namespace polling
{
    template <typename Pollable_T>
    concept CPollableType = requires(Pollable_T pollable)
    {
        { pollable.Initialize() } -> std::convertible_to<bool>;
        { pollable.PollOnce() } -> std::convertible_to<size_t>;
        { pollable.StopPolling() } -> std::convertible_to<void>;
    };

    class IPollable {
    public:
        virtual ~IPollable() = default;

        virtual bool Initialize() = 0;
        virtual size_t PollOnce() = 0;
        virtual void StopPolling() = 0;
    };

    template <typename PollableBuilder_T>
    concept CPollableBuilder = std::is_invocable_r_v<std::unique_ptr<IPollable>, PollableBuilder_T>;
}