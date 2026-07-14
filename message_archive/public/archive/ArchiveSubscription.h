#pragma once

#include <concepts>

#include "messaging/ipc_queue.h"
#include "messaging/ipc_queue_element_wrapper.h"

namespace archive {

    enum class FragmentAction {
        ABORT,
        CONTINUE
    };

    template <typename Handler_T>
    concept CFragmentHandler = requires(Handler_T&& policy, std::span<const std::byte> fragment) {
        { policy.on_fragment(fragment) } -> std::convertible_to<FragmentAction>;
    };

    /**
     * A subscription to an active message queue. This adds this subscription
     * to the set of readers for a given publication. This is an atomically
     * safe operation that works with the SP-MC queue design.
     */
    class ArchiveSubscription {
    
    public:
        struct ArchiveSubscriptionParams {
            const std::string file_name;
        };
        ArchiveSubscription(ArchiveSubscriptionParams&& params);
        ~ArchiveSubscription();

        // users should poll this to determine queue health
        bool is_ready() const;

        template <CFragmentHandler Handler_T>
        void poll(Handler_T& handler) {
            auto reader = queue->poll_buffer();
            const auto bytes = reader.has_value() ?
                reader->get_as_view<std::span<const std::byte>, std::byte>() : std::span<const std::byte>{};

            const auto code = handler.on_fragment(bytes);
            switch (code) {
                case (FragmentAction::ABORT): {
                    // need to re-mark these bytes as needing-to-read next poll
                    reader->mark_as_read();
                }
                case (FragmentAction::CONTINUE):
                default: {
                    // bytes read successfully or we don't understand, mark as read 
                    reader->release();
                }
            }
        }

        void close();

    private:
        std::unique_ptr<message_transport::IpcQueue> queue;
        std::optional<message_transport::IpcQueueRaiiReaderWrapper> reader;
    };
}