#pragma once

#include <concepts>

#include "messaging/ipc_queue.h"
#include "messaging/ipc_queue_element_wrapper.h"

namespace archive {

    enum class FragmentAction : uint8_t {
        CONTINUE = 0,
        ABORT = 1
    };

    /**
     * A subscription to an active message queue. This adds this subscription
     * to the set of readers for a given publication. This is an atomically
     * safe operation that works with the SP-MC queue design.
     */
    class ArchiveSubscription {
    
    public:
        using ConsumerCallback = std::function<uint8_t(const uint8_t*, size_t)>;
        struct ArchiveSubscriptionParams {
            message_transport::IpcQueue::IpcQueueParameters queue_params;
        };
        ArchiveSubscription(ArchiveSubscriptionParams&& params);
        ~ArchiveSubscription();

        // users should poll this to determine queue health
        bool is_ready() const;

        // due to the C-wrapper ABI, we cannot accept C++ functions but rather C-style function pointers.
        // I would have preferred something that can use std::spans here but alas.
        void poll(ConsumerCallback handler) {
            auto reader = queue->poll_buffer();
            if (!reader.has_value()) {
                spdlog::warn("Did not receive any data for agent {}", queue->name());
                return;
            }
            const auto bytes = reader->get_as_view<std::span<const std::byte>, std::byte>();
            const uint8_t* casted = reinterpret_cast<const uint8_t*>(bytes.data());
            const size_t len = bytes.size_bytes();
            const auto code = static_cast<FragmentAction>(handler(casted, len));

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