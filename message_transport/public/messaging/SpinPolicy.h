#pragma once

#if defined(__x86_64__) || defined(__i386__)
    #include <immintrin.h>
#endif

#include <type_traits>
#include <concepts>

namespace message_transport {
    

    struct BusyWaitPolicy {
        static void execute() noexcept {
            #if defined(__x86_64__) || defined(__i386__)
                #include <immintrin.h>
                _mm_pause();
            #elif defined(__aarch64__)
                asm volatile("yield");
            #else
                std::this_thread::yield();
            #endif
        }
    };

    struct YieldPolicy {
        static void execute() noexcept {
            std::this_thread::yield();
        }
    };

    struct SleepPolicy {
        static constexpr auto DEFAULT_TIMEOUT_NS = std::chrono::nanoseconds(50);
        static void execute() noexcept {
            std::this_thread::sleep_for(DEFAULT_TIMEOUT_NS);
        }
    };

    // TODO: this spin count will apply to ALL callers and therefore should be re-evaluated at some point.
    struct HybridPolicy {
        inline static size_t spins = 1;
        static void execute() noexcept {
            if (HybridPolicy::spins++ < 100) {
                BusyWaitPolicy::execute();
            } else if (HybridPolicy::spins++ < 1000) {
                YieldPolicy::execute();
            } else {
                SleepPolicy::execute();
            }
        }
    };

    template <typename Policy_T>
    concept CSpinPolicy = requires(Policy_T&& policy) {
        { Policy_T::execute() } -> std::convertible_to<void>;
    };
}